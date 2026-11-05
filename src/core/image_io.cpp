#include "ticket_ocr/image_io.hpp"

#include <Windows.h>
#include <objidl.h>
#include <algorithm>
using std::max;
using std::min;
#include <gdiplus.h>

#include <cstring>
#include <mutex>
#include <vector>

namespace ticket_ocr {

namespace {

void ensure_global_gdiplus_started() {
    static std::once_flag flag;
    std::call_once(flag, []() {
        Gdiplus::GdiplusStartupInput input;
        ULONG_PTR token = 0;
        Gdiplus::GdiplusStartup(&token, &input, nullptr);
        // Intentionally kept alive for process lifetime so worker threads never hit Shutdown races
    });
}

bool find_encoder_clsid(const wchar_t* mime_type, CLSID* out_clsid) {
    UINT num = 0;
    UINT size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0 || num == 0) return false;

    std::vector<uint8_t> buffer(size);
    auto* encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
    if (Gdiplus::GetImageEncoders(num, size, encoders) != Gdiplus::Ok) return false;

    for (UINT i = 0; i < num; ++i) {
        if (encoders[i].MimeType && std::wcscmp(encoders[i].MimeType, mime_type) == 0) {
            *out_clsid = encoders[i].Clsid;
            return true;
        }
    }
    return false;
}

} // namespace

GdiPlusSession::GdiPlusSession() {
    ensure_global_gdiplus_started();
    initialized_ = true;
}

GdiPlusSession::~GdiPlusSession() = default;

std::optional<ImageBuffer> ImageIO::load_from_file(const std::filesystem::path& path, std::string* error_out) {
    ensure_global_gdiplus_started();

    if (!std::filesystem::exists(path)) {
        if (error_out) *error_out = "El archivo de imagen no existe: " + wide_to_utf8(path.wstring());
        return std::nullopt;
    }

    Gdiplus::Bitmap bmp(path.wstring().c_str());
    if (bmp.GetLastStatus() != Gdiplus::Ok) {
        if (error_out) *error_out = "No se pudo decodificar la imagen: " + wide_to_utf8(path.wstring());
        return std::nullopt;
    }

    const int w = static_cast<int>(bmp.GetWidth());
    const int h = static_cast<int>(bmp.GetHeight());
    if (w <= 0 || h <= 0) {
        if (error_out) *error_out = "Dimensiones de imagen inválidas.";
        return std::nullopt;
    }

    Gdiplus::Rect rect(0, 0, w, h);
    Gdiplus::BitmapData data{};
    if (bmp.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) != Gdiplus::Ok) {
        if (error_out) *error_out = "Error al bloquear bits de la imagen.";
        return std::nullopt;
    }

    ImageBuffer out = ImageBuffer::create(w, h);
    const auto* src_base = static_cast<const uint8_t*>(data.Scan0);
    for (int y = 0; y < h; ++y) {
        const uint8_t* src_row = src_base + static_cast<ptrdiff_t>(y) * data.Stride;
        std::memcpy(out.row_ptr(y), src_row, static_cast<size_t>(out.stride));
    }

    bmp.UnlockBits(&data);
    return out;
}

bool ImageIO::save_to_file(const ImageBuffer& image, const std::filesystem::path& path, int jpeg_quality, std::string* error_out) {
    ensure_global_gdiplus_started();

    if (image.empty()) {
        if (error_out) *error_out = "No se puede guardar un buffer de imagen vacío.";
        return false;
    }

    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    Gdiplus::Bitmap bmp(
        image.width,
        image.height,
        image.stride,
        PixelFormat32bppARGB,
        const_cast<BYTE*>(image.pixels.data())
    );
    if (bmp.GetLastStatus() != Gdiplus::Ok) {
        if (error_out) *error_out = "Fallo al construir Gdiplus::Bitmap desde buffer.";
        return false;
    }

    std::string ext = to_lower_ascii(wide_to_utf8(path.extension().wstring()));
    const wchar_t* mime = L"image/png";
    bool is_jpeg = false;
    if (ext == ".jpg" || ext == ".jpeg") {
        mime = L"image/jpeg";
        is_jpeg = true;
    } else if (ext == ".bmp") {
        mime = L"image/bmp";
    } else if (ext == ".tif" || ext == ".tiff") {
        mime = L"image/tiff";
    }

    CLSID clsid{};
    if (!find_encoder_clsid(mime, &clsid)) {
        if (error_out) *error_out = "Codificador de imagen no disponible.";
        return false;
    }

    Gdiplus::Status st = Gdiplus::GenericError;
    if (is_jpeg) {
        ULONG quality = static_cast<ULONG>(std::clamp(jpeg_quality, 10, 100));
        Gdiplus::EncoderParameters params{};
        params.Count = 1;
        params.Parameter[0].Guid = Gdiplus::EncoderQuality;
        params.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
        params.Parameter[0].NumberOfValues = 1;
        params.Parameter[0].Value = &quality;
        st = bmp.Save(path.wstring().c_str(), &clsid, &params);
    } else {
        st = bmp.Save(path.wstring().c_str(), &clsid, nullptr);
    }

    if (st != Gdiplus::Ok) {
        if (error_out) *error_out = "Fallo al escribir imagen en disco: " + wide_to_utf8(path.wstring());
        return false;
    }
    return true;
}

} // namespace ticket_ocr

#include "ticket_ocr/ocr_engine.hpp"
#include "ticket_ocr/image_preprocessor.hpp"

#include <unknwn.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Storage.Streams.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <regex>
#include <sstream>
#include <thread>
#include <utility>

namespace ticket_ocr {

namespace {

using winrt::Windows::Globalization::Language;
using winrt::Windows::Graphics::Imaging::BitmapAlphaMode;
using winrt::Windows::Graphics::Imaging::BitmapPixelFormat;
using winrt::Windows::Graphics::Imaging::SoftwareBitmap;
using winrt::Windows::Media::Ocr::OcrEngine;
using winrt::Windows::Storage::Streams::DataWriter;

bool is_tight_connector_suffix(std::string_view w) {
    if (w.empty()) return false;
    const char last = w.back();
    if (last == '/' || last == '.' || last == '-' || last == '_' ||
        last == '?' || last == '=' || last == '&' || last == '@') {
        return true;
    }
    const std::string low = to_lower_ascii(w);
    if (low == "http:" || low == "https:") return true;
    return false;
}

bool is_tight_connector_prefix(std::string_view w) {
    if (w.empty()) return false;
    const char first = w.front();
    return (first == '/' || first == '.' || first == '?' || first == '=' ||
            first == '&' || first == '@' || first == ']');
}

class WinRtOcrDispatcher {
public:
    static WinRtOcrDispatcher& instance() {
        static WinRtOcrDispatcher inst;
        return inst;
    }

    OcrResult run_ocr(const ImageBuffer& img) {
        if (img.empty()) return {};
        std::packaged_task<OcrResult()> task([this, &img]() {
            return execute_on_worker(img);
        });
        std::future<OcrResult> fut = task.get_future();
        {
            std::lock_guard<std::mutex> lk(mu_);
            tasks_.push(std::move(task));
        }
        cv_.notify_one();
        return fut.get();
    }

private:
    std::mutex mu_;
    std::condition_variable cv_;
    std::queue<std::packaged_task<OcrResult()>> tasks_;
    bool stop_ = false;
    OcrEngine engine_{nullptr};
    std::string engine_lang_;
    std::thread worker_;

    WinRtOcrDispatcher() {
        worker_ = std::thread([this]() { worker_loop(); });
    }

    ~WinRtOcrDispatcher() {
        {
            std::lock_guard<std::mutex> lk(mu_);
            stop_ = true;
        }
        cv_.notify_all();
        if (worker_.joinable()) worker_.join();
    }

    void init_engine_once() {
        if (engine_) return;
        const wchar_t* preferred_tags[] = {L"es-MX", L"es-ES", L"es-419", L"en-US"};
        for (const wchar_t* tag : preferred_tags) {
            try {
                Language lang(tag);
                if (OcrEngine::IsLanguageSupported(lang)) {
                    engine_ = OcrEngine::TryCreateFromLanguage(lang);
                    if (engine_) {
                        engine_lang_ = wide_to_utf8(tag);
                        return;
                    }
                }
            } catch (...) {
            }
        }
        engine_ = OcrEngine::TryCreateFromUserProfileLanguages();
        if (engine_) {
            engine_lang_ = wide_to_utf8(engine_.RecognizerLanguage().LanguageTag().c_str());
        }
    }

    OcrResult execute_on_worker(const ImageBuffer& img) {
        OcrResult out;
        try {
            init_engine_once();
            if (!engine_) return out;
            out.engine_language = engine_lang_;

            DataWriter writer;
            writer.WriteBytes(winrt::array_view<const uint8_t>(
                img.pixels.data(),
                img.pixels.data() + img.pixels.size()
            ));
            auto ibuf = writer.DetachBuffer();

            SoftwareBitmap sbmp = SoftwareBitmap::CreateCopyFromBuffer(
                ibuf,
                BitmapPixelFormat::Bgra8,
                img.width,
                img.height,
                BitmapAlphaMode::Ignore
            );

            auto winrt_res = engine_.RecognizeAsync(sbmp).get();
            auto lines = winrt_res.Lines();

            std::vector<OcrLineBox> extracted;
            extracted.reserve(lines.Size());

            for (const auto& line : lines) {
                OcrLineBox box;
                auto words = line.Words();
                if (words.Size() > 0) {
                    float min_x = 1e9f, min_y = 1e9f, max_x = 0.0f, max_y = 0.0f;
                    float sum_h = 0.0f;
                    for (const auto& w : words) {
                        const auto r = w.BoundingRect();
                        min_x = std::min(min_x, r.X);
                        min_y = std::min(min_y, r.Y);
                        max_x = std::max(max_x, r.X + r.Width);
                        max_y = std::max(max_y, r.Y + r.Height);
                        sum_h += r.Height;
                    }
                    const float avg_h = sum_h / static_cast<float>(words.Size());

                    // Reconstruct line geometrically using word bounding-box gaps!
                    // Prevents WinRT word-breaker from inserting spaces inside URLs & dotted module names.
                    std::string line_str;
                    std::string prev_w_str;
                    float prev_right = 0.0f;
                    bool first_word = true;

                    for (const auto& w : words) {
                        const std::string w_str = wide_to_utf8(w.Text().c_str());
                        const auto r = w.BoundingRect();
                        if (!first_word) {
                            const float gap = r.X - prev_right;
                            const bool after_label_colon = (!prev_w_str.empty() && prev_w_str.back() == ':' &&
                                to_lower_ascii(prev_w_str) != "http:" && to_lower_ascii(prev_w_str) != "https:");

                            if (after_label_colon) {
                                line_str.push_back(' ');
                            } else if (gap <= avg_h * 0.14f ||
                                       (gap <= avg_h * 0.34f && (is_tight_connector_suffix(prev_w_str) || is_tight_connector_prefix(w_str)))) {
                                // Join directly without artificial space
                            } else {
                                line_str.push_back(' ');
                            }
                        }
                        line_str += w_str;
                        prev_w_str = w_str;
                        prev_right = r.X + r.Width;
                        first_word = false;
                    }

                    box.text = std::move(line_str);
                    box.x = min_x;
                    box.y = min_y;
                    box.width = std::max(0.0f, max_x - min_x);
                    box.height = std::max(1.0f, max_y - min_y);
                } else {
                    box.text = wide_to_utf8(line.Text().c_str());
                }

                if (!trim_copy(box.text).empty()) {
                    extracted.push_back(std::move(box));
                }
            }

            std::sort(extracted.begin(), extracted.end(), [](const OcrLineBox& a, const OcrLineBox& b) {
                const float cy_a = a.y + a.height * 0.5f;
                const float cy_b = b.y + b.height * 0.5f;
                const float row_tol = std::max(a.height, b.height) * 0.52f;
                if (std::abs(cy_a - cy_b) <= row_tol) {
                    return a.x < b.x;
                }
                return cy_a < cy_b;
            });

            std::ostringstream oss;
            for (size_t i = 0; i < extracted.size(); ++i) {
                if (i > 0) {
                    const float prev_cy = extracted[i - 1].y + extracted[i - 1].height * 0.5f;
                    const float curr_cy = extracted[i].y + extracted[i].height * 0.5f;
                    const float row_tol = std::max(extracted[i - 1].height, extracted[i].height) * 0.52f;
                    if (std::abs(curr_cy - prev_cy) <= row_tol) {
                        const float hgap = extracted[i].x - (extracted[i - 1].x + extracted[i - 1].width);
                        if (hgap <= std::max(extracted[i - 1].height, extracted[i].height) * 0.30f &&
                            (is_tight_connector_suffix(extracted[i - 1].text) || is_tight_connector_prefix(extracted[i].text))) {
                            // Contiguous URL/module split into adjacent line boxes on same baseline
                        } else {
                            oss << ' ';
                        }
                    } else {
                        oss << '\n';
                    }
                }
                oss << extracted[i].text;
            }

            out.lines = std::move(extracted);
            out.raw_text = WindowsNativeOcrEngine::post_correct_technical_ocr(oss.str());
        } catch (...) {
        }
        return out;
    }

    void worker_loop() {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        while (true) {
            std::packaged_task<OcrResult()> task;
            {
                std::unique_lock<std::mutex> lk(mu_);
                cv_.wait(lk, [this]() { return stop_ || !tasks_.empty(); });
                if (stop_ && tasks_.empty()) break;
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
        engine_ = nullptr;
        winrt::uninit_apartment();
    }
};

void replace_all_inplace(std::string& str, std::string_view from, std::string_view to) {
    if (from.empty()) return;
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.size(), to);
        pos += to.size();
    }
}

bool looks_like_spanish_text(std::string_view text) {
    const std::string low = to_lower_ascii(fold_spanish_accents_to_ascii(text));
    int hits = 0;
    const char* markers[] = {
        "modulo:", "codigo:", "mensaje:", "no se ", "fallo ", "conexion", "autenticacion",
        "facturacion", "configuracion", "sesion", "usuario", "servidor", "expiro",
        "base de datos", "solicitud", "error de ", "tiempo de espera", "cargo rechazado",
        "acceso denegado", "memoria insuficiente", "certificado", "parametro", "version obsoleta"
    };
    for (const char* m : markers) {
        if (low.find(m) != std::string::npos) ++hits;
    }
    return hits >= 2;
}

} // namespace

std::string WindowsNativeOcrEngine::post_correct_technical_ocr(std::string_view raw_text) {
    std::string text(raw_text);
    if (text.empty()) return text;

    // 1. Replace UTF-8 'ø'/'Ø' (never valid in ES/EN error codes or versions) with '0'
    replace_all_inplace(text, "ø", "0");
    replace_all_inplace(text, "Ø", "0");

    // 2. Repair URL scheme OCR glitches: "https;//", "https : / [", "https:/ "
    static const std::regex re_https_glitch(R"(\b(https?)\s*[;:]\s*/\s*[/\[\|l1I]\s*)", std::regex::icase);
    text = std::regex_replace(text, re_https_glitch, "$1://");

    static const std::regex re_https_single_slash(R"(\b(https?):/([A-Za-z0-9]))", std::regex::icase);
    text = std::regex_replace(text, re_https_single_slash, "$1://$2");

    // Collapse accidental spaces inside URL lines
    {
        std::istringstream iss(text);
        std::ostringstream oss;
        std::string line;
        bool first = true;
        while (std::getline(iss, line)) {
            if (!first) oss << '\n';
            first = false;
            const auto pos = line.find("://");
            if (pos != std::string::npos && pos >= 4) {
                size_t ustart = pos - 4;
                if (pos >= 5 && (line[pos - 5] == 'h' || line[pos - 5] == 'H')) ustart = pos - 5;
                std::string prefix = line.substr(0, ustart);
                std::string url_rest = line.substr(ustart);
                replace_all_inplace(url_rest, ":// ", "://");
                replace_all_inplace(url_rest, " /", "/");
                replace_all_inplace(url_rest, "/ ", "/");
                replace_all_inplace(url_rest, " .", ".");
                replace_all_inplace(url_rest, ". ", ".");
                replace_all_inplace(url_rest, " - ", "-");
                replace_all_inplace(url_rest, "- ", "-");
                replace_all_inplace(url_rest, " -", "-");
                replace_all_inplace(url_rest, " ? ", "?");
                replace_all_inplace(url_rest, " ?", "?");
                replace_all_inplace(url_rest, "? ", "?");
                replace_all_inplace(url_rest, " = ", "=");
                replace_all_inplace(url_rest, " =", "=");
                replace_all_inplace(url_rest, "= ", "=");
                replace_all_inplace(url_rest, " •", "-");
                replace_all_inplace(url_rest, "• ", "-");
                replace_all_inplace(url_rest, "•", "-");
                replace_all_inplace(url_rest, "://s so.", "://sso.");
                replace_all_inplace(url_rest, ".Io/", ".io/");
                // Repair missing dot turned into space inside URL hostname (e.g. "https://edge internal-mesh.net")
                static const std::regex re_host_space(R"(^(https?://[A-Za-z0-9-]+)\s+([A-Za-z0-9-]+\.[A-Za-z]{2,}))");
                url_rest = std::regex_replace(url_rest, re_host_space, "$1.$2");
                line = prefix + url_rest;
            }
            oss << line;
        }
        text = oss.str();
    }

    // 3. Repair hex error prefix "Ox800...", "ox800...", "0\"800...", "OXCOOOOOOS" -> "0x..."
    static const std::regex re_hex_ox(R"(\b[Oo0][xX\"\*]([0-9A-Fa-fOolISs]{6,10})\b)");
    {
        std::string fixed;
        fixed.reserve(text.size());
        size_t last = 0;
        for (std::sregex_iterator it(text.begin(), text.end(), re_hex_ox), end; it != end; ++it) {
            fixed.append(text, last, static_cast<size_t>(it->position()) - last);
            std::string body = (*it)[1].str();
            for (char& c : body) {
                if (c == 'O' || c == 'o') c = '0';
                else if (c == 'l' || c == 'I') c = '1';
                else if (c == 'S' || c == 's') c = '5';
                else c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            fixed += "0x" + body;
            last = static_cast<size_t>(it->position() + it->length());
        }
        fixed.append(text, last, std::string::npos);
        text = std::move(fixed);
    }

    // 4. Repair common OCR digit confusions inside structured error codes & semantic versions
    static const std::regex re_prefixed_code(R"(\b(ERR-[A-Z]{2,8}-|ORA-|SQL-|HTTP\s+)([0-9OolISs]{3,6})\b)");
    {
        std::string fixed;
        fixed.reserve(text.size());
        size_t last = 0;
        for (std::sregex_iterator it(text.begin(), text.end(), re_prefixed_code), end; it != end; ++it) {
            fixed.append(text, last, static_cast<size_t>(it->position()) - last);
            std::string pfx = (*it)[1].str();
            std::string num = (*it)[2].str();
            for (char& c : num) {
                if (c == 'O' || c == 'o') c = '0';
                else if (c == 'l' || c == 'I') c = '1';
                else if (c == 'S' || c == 's') c = '5';
            }
            fixed += pfx + num;
            last = static_cast<size_t>(it->position() + it->length());
        }
        fixed.append(text, last, std::string::npos);
        text = std::move(fixed);
    }

    // Repair labeled Version OCR confusions (e.g. "Version: va.12.3", "Version: vd.9.5", "Version: v5.1.e", "Version: \/9.0.1")
    static const std::regex re_labeled_ver_fix(
        R"(((?:Versi(?:ó|o|ö)n|Version)\s*:\s*)(?:[vV]|\\/)\s*([0-9aAdDeEsSgGoOlI]+(?:\s*\.\s*[0-9aAdDeEsSgGoOlI]+){1,2}))",
        std::regex::icase
    );
    {
        std::string fixed;
        fixed.reserve(text.size());
        size_t last = 0;
        for (std::sregex_iterator it(text.begin(), text.end(), re_labeled_ver_fix), end; it != end; ++it) {
            fixed.append(text, last, static_cast<size_t>(it->position()) - last);
            std::string label = (*it)[1].str();
            std::string ver_body = (*it)[2].str();
            std::string clean_ver;
            for (char c : ver_body) {
                if (c == ' ' || c == '\t') continue;
                if (c == 'a' || c == 'A' || c == 'd' || c == 'D') clean_ver.push_back('4');
                else if (c == 's' || c == 'S') clean_ver.push_back('5');
                else if (c == 'g') clean_ver.push_back('9');
                else if (c == 'o' || c == 'O' || c == 'e' || c == 'E') clean_ver.push_back('0');
                else if (c == 'l' || c == 'I') clean_ver.push_back('1');
                else clean_ver.push_back(c);
            }
            fixed += label + "v" + clean_ver;
            last = static_cast<size_t>(it->position() + it->length());
        }
        fixed.append(text, last, std::string::npos);
        text = std::move(fixed);
    }

    // 5. Repair '1'/'I' misread inside lowercase words (e.g. "TokenVa1idator", "vaIidator" -> "validator")
    static const std::regex re_inner_one(R"(([a-z]{2})[1I]([a-z]{2}))");
    text = std::regex_replace(text, re_inner_one, "$1l$2");

    // 6. Contextual Spanish diacritic normalization
    if (looks_like_spanish_text(text)) {
        static const std::regex re_spanish_6n(R"(([a-zA-Z])6n\b)");
        text = std::regex_replace(text, re_spanish_6n, "$1ón");
        replace_all_inplace(text, "ä", "á");
        replace_all_inplace(text, "ë", "é");
        replace_all_inplace(text, "ï", "í");
        replace_all_inplace(text, "ö", "ó");
        replace_all_inplace(text, "â", "á");
        replace_all_inplace(text, "ê", "é");
        replace_all_inplace(text, "î", "í");
        replace_all_inplace(text, "ô", "ó");

        static const std::pair<const char*, const char*> kSpanishRepairs[] = {
            {"Modulo:", "Módulo:"},
            {"Modulo :", "Módulo:"},
            {"Version:", "Versión:"},
            {"Version :", "Versión:"},
            {"Codigo:", "Código:"},
            {"Codigo :", "Código:"},
            {"Codigo de error", "Código de error"},
            {"Autenticacion", "Autenticación"},
            {"autenticacion", "autenticación"},
            {"Conexion", "Conexión"},
            {"conexion", "conexión"},
            {"Facturacion", "Facturación"},
            {"facturacion", "facturación"},
            {"Configuracion", "Configuración"},
            {"configuracion", "configuración"},
            {"Transaccion", "Transacción"},
            {"transaccion", "transacción"},
            {"Operacion", "Operación"},
            {"operacion", "operación"},
            {"Excepcion", "Excepción"},
            {"excepcion", "excepción"},
            {"Comunicacion", "Comunicación"},
            {"comunicacion", "comunicación"},
            {" expiracion", " expiración"},
            {" expiro ", " expiró "},
            {" sesion", " sesión"},
            {"Sesion ", "Sesión "},
            {" invalido", " inválido"},
            {" invalida", " inválida"},
            {"Parametro ", "Parámetro "},
            {" parametro", " parámetro"},
            {" parametros", " parámetros"},
            {"Version obsoleta", "Versión obsoleta"},
            {" version obsoleta", " versión obsoleta"},
            {" numero ", " número "},
            {" pagina ", " página "}
        };
        for (const auto& [src_tok, dst_tok] : kSpanishRepairs) {
            replace_all_inplace(text, src_tok, dst_tok);
        }
    }

    return text;
}

double WindowsNativeOcrEngine::score_ocr_text_quality(std::string_view text) {
    if (text.empty()) return 0.0;

    size_t alnum_count = 0;
    size_t weird_symbol_count = 0;
    for (unsigned char c : text) {
        if (std::isalnum(c) || c >= 0x80) {
            ++alnum_count;
        } else if (c != ' ' && c != '\n' && c != '\r' && c != '.' && c != ',' &&
                   c != ':' && c != '-' && c != '_' && c != '/' && c != '?' &&
                   c != '=' && c != '&' && c != '@' && c != '(' && c != ')' && c != '[' && c != ']') {
            ++weird_symbol_count;
        }
    }

    double score = static_cast<double>(alnum_count) - static_cast<double>(weird_symbol_count) * 4.0;

    const std::string low = to_lower_ascii(fold_spanish_accents_to_ascii(text));
    if (low.find("http://") != std::string::npos || low.find("https://") != std::string::npos) score += 40.0;
    if (low.find("version") != std::string::npos) score += 28.0;
    if (low.find("modulo") != std::string::npos || low.find("module") != std::string::npos) score += 28.0;
    if (low.find("err-") != std::string::npos || low.find("0x") != std::string::npos ||
        low.find("ora-") != std::string::npos || low.find("sqlstate") != std::string::npos ||
        low.find("econnrefused") != std::string::npos) score += 40.0;

    return score;
}

OcrResult WindowsNativeOcrEngine::recognize_buffer(const ImageBuffer& buffer) const {
    const auto t0 = std::chrono::steady_clock::now();
    OcrResult res = WinRtOcrDispatcher::instance().run_ocr(buffer);
    const auto t1 = std::chrono::steady_clock::now();
    res.ocr_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return res;
}

OcrResult WindowsNativeOcrEngine::recognize(const PreprocessOutput& preprocessed) const {
    const auto t0 = std::chrono::steady_clock::now();

    OcrResult primary = WinRtOcrDispatcher::instance().run_ocr(preprocessed.primary_image);
    double primary_score = score_ocr_text_quality(primary.raw_text);

    if (!preprocessed.anti_moire_image.empty()) {
        OcrResult anti_moire = WinRtOcrDispatcher::instance().run_ocr(preprocessed.anti_moire_image);
        const double moire_score = score_ocr_text_quality(anti_moire.raw_text);
        if (moire_score > primary_score || primary.raw_text.size() < 30) {
            anti_moire.used_anti_moire_pass = true;
            const auto t1 = std::chrono::steady_clock::now();
            anti_moire.ocr_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            return anti_moire;
        }
    }

    const auto t1 = std::chrono::steady_clock::now();
    primary.ocr_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return primary;
}

} // namespace ticket_ocr

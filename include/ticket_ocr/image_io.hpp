#pragma once

#include "ticket_ocr/types.hpp"
#include <filesystem>
#include <optional>
#include <string>

namespace ticket_ocr {

// RAII session guard for GDI+ subsystem
class GdiPlusSession {
public:
    GdiPlusSession();
    ~GdiPlusSession();
    GdiPlusSession(const GdiPlusSession&) = delete;
    GdiPlusSession& operator=(const GdiPlusSession&) = delete;
    bool ok() const noexcept { return initialized_; }

private:
    uintptr_t token_ = 0;
    bool initialized_ = false;
};

class ImageIO {
public:
    // Load PNG, JPEG, BMP, TIFF into 32-bit BGRA contiguous buffer
    static std::optional<ImageBuffer> load_from_file(const std::filesystem::path& path, std::string* error_out = nullptr);

    // Save 32-bit BGRA buffer to PNG, JPEG (with configurable quality), or BMP based on extension
    static bool save_to_file(const ImageBuffer& image, const std::filesystem::path& path, int jpeg_quality = 92, std::string* error_out = nullptr);
};

} // namespace ticket_ocr

#pragma once

#include "ticket_ocr/types.hpp"
#include <string>
#include <string_view>

namespace ticket_ocr {

class IOcrEngine {
public:
    virtual ~IOcrEngine() = default;
    virtual OcrResult recognize(const PreprocessOutput& preprocessed) const = 0;
    virtual OcrResult recognize_buffer(const ImageBuffer& buffer) const = 0;
};

// Native Windows.Media.Ocr (WinRT) implementation running safely on MTA worker context
class WindowsNativeOcrEngine final : public IOcrEngine {
public:
    WindowsNativeOcrEngine() = default;

    OcrResult recognize(const PreprocessOutput& preprocessed) const override;
    OcrResult recognize_buffer(const ImageBuffer& buffer) const override;

    // Post-OCR technical & bilingual lexical normalization (repairs common OCR glyph confusions)
    static std::string post_correct_technical_ocr(std::string_view raw_text);

    // Quality heuristic used to choose the cleanest multi-pass OCR result
    static double score_ocr_text_quality(std::string_view text);
};

} // namespace ticket_ocr

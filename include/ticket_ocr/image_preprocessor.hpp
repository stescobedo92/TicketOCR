#pragma once

#include "ticket_ocr/types.hpp"

namespace ticket_ocr {

struct PreprocessorOptions {
    bool auto_invert_dark_mode = true;
    bool auto_mitigate_moire = true;
    bool force_anti_moire = false;
    int min_target_width = 1760;
    int max_target_width = 2560;
};

class ImagePreprocessor {
public:
    explicit ImagePreprocessor(PreprocessorOptions opts = {});

    PreprocessOutput process(const ImageBuffer& input) const;

    // Individual building blocks exposed for unit/integration inspection
    static bool detect_dark_mode(const ImageBuffer& img, double* mean_lum_out = nullptr);
    static double compute_moire_score(const ImageBuffer& img);
    static ImageBuffer resize_bicubic(const ImageBuffer& src, int dst_w, int dst_h);
    static ImageBuffer invert_luminance_preserving_contrast(const ImageBuffer& src);
    static ImageBuffer restore_moire_and_uneven_lighting(const ImageBuffer& src);
    static ImageBuffer sharpen_glyphs(const ImageBuffer& src, float amount = 0.42f);

private:
    PreprocessorOptions opts_;
};

} // namespace ticket_ocr

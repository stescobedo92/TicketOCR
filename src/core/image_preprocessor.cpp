#include "ticket_ocr/image_preprocessor.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ticket_ocr {

namespace {

inline uint8_t clamp_u8(int v) noexcept {
    return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
}

inline uint8_t luma_from_bgr(uint8_t b, uint8_t g, uint8_t r) noexcept {
    return static_cast<uint8_t>((29u * b + 150u * g + 77u * r + 128u) >> 8);
}

inline float cubic_hermite(float A, float B, float C, float D, float t) noexcept {
    const float a = -A * 0.5f + (3.0f * B) * 0.5f - (3.0f * C) * 0.5f + D * 0.5f;
    const float b = A - (5.0f * B) * 0.5f + 2.0f * C - D * 0.5f;
    const float c = -A * 0.5f + C * 0.5f;
    const float d = B;
    return ((a * t + b) * t + c) * t + d;
}

} // namespace

ImagePreprocessor::ImagePreprocessor(PreprocessorOptions opts) : opts_(opts) {}

bool ImagePreprocessor::detect_dark_mode(const ImageBuffer& img, double* mean_lum_out) {
    if (img.empty()) {
        if (mean_lum_out) *mean_lum_out = 255.0;
        return false;
    }

    const int x0 = img.width * 8 / 100;
    const int x1 = std::max(x0 + 1, img.width * 92 / 100);
    const int y0 = img.height * 8 / 100;
    const int y1 = std::max(y0 + 1, img.height * 92 / 100);

    std::array<uint64_t, 256> hist{};
    uint64_t sum_lum = 0;
    uint64_t count = 0;

    for (int y = y0; y < y1; ++y) {
        const uint8_t* row = img.row_ptr(y);
        for (int x = x0; x < x1; ++x) {
            const size_t idx = static_cast<size_t>(x) * 4;
            const uint8_t lum = luma_from_bgr(row[idx], row[idx + 1], row[idx + 2]);
            hist[lum]++;
            sum_lum += lum;
            ++count;
        }
    }

    const double mean_lum = count > 0 ? static_cast<double>(sum_lum) / static_cast<double>(count) : 255.0;
    if (mean_lum_out) *mean_lum_out = mean_lum;

    uint64_t dark_pixels = 0;
    uint64_t acc = 0;
    int median_lum = 128;
    bool median_found = false;
    for (int i = 0; i < 256; ++i) {
        if (i < 105) dark_pixels += hist[static_cast<size_t>(i)];
        acc += hist[static_cast<size_t>(i)];
        if (!median_found && acc * 2 >= count) {
            median_lum = i;
            median_found = true;
        }
    }

    const double dark_ratio = count > 0 ? static_cast<double>(dark_pixels) / static_cast<double>(count) : 0.0;
    return (median_lum < 112 && dark_ratio > 0.52) || (mean_lum < 108.0);
}

double ImagePreprocessor::compute_moire_score(const ImageBuffer& img) {
    if (img.width < 32 || img.height < 32) return 0.0;

    uint64_t oscillating_bg_pixels = 0;
    uint64_t chromatic_ripple_energy = 0;
    uint64_t total_sampled = 0;

    const int step_y = std::max(1, img.height / 240);
    for (int y = 2; y < img.height - 2; y += step_y) {
        const uint8_t* row = img.row_ptr(y);
        for (int x = 2; x < img.width - 2; ++x) {
            const size_t i0 = static_cast<size_t>(x - 2) * 4;
            const size_t i1 = static_cast<size_t>(x - 1) * 4;
            const size_t i2 = static_cast<size_t>(x) * 4;
            const size_t i3 = static_cast<size_t>(x + 1) * 4;
            const size_t i4 = static_cast<size_t>(x + 2) * 4;

            const int y_m2 = luma_from_bgr(row[i0], row[i0 + 1], row[i0 + 2]);
            const int y_m1 = luma_from_bgr(row[i1], row[i1 + 1], row[i1 + 2]);
            const int y_0  = luma_from_bgr(row[i2], row[i2 + 1], row[i2 + 2]);
            const int y_p1 = luma_from_bgr(row[i3], row[i3 + 1], row[i3 + 2]);
            const int y_p2 = luma_from_bgr(row[i4], row[i4 + 1], row[i4 + 2]);

            const int local_span = std::max({y_m2, y_m1, y_0, y_p1, y_p2}) - std::min({y_m2, y_m1, y_0, y_p1, y_p2});
            if (local_span >= 6 && local_span <= 85) {
                const int d1 = y_0 - y_m1;
                const int d2 = y_p1 - y_0;
                const int d3 = y_p2 - y_p1;
                if ((d1 * d2 < -4) || (d2 * d3 < -4) || std::abs(2 * y_0 - y_m2 - y_p2) >= 12) {
                    ++oscillating_bg_pixels;
                }
            }

            const int rg_curr = static_cast<int>(row[i2 + 2]) - static_cast<int>(row[i2 + 1]);
            const int rg_next = static_cast<int>(row[i3 + 2]) - static_cast<int>(row[i3 + 1]);
            const int diff_rg = std::abs(rg_curr - rg_next);
            if (diff_rg >= 6 && diff_rg <= 75) {
                chromatic_ripple_energy += static_cast<uint64_t>(diff_rg);
            }

            ++total_sampled;
        }
    }

    if (total_sampled == 0) return 0.0;
    const double osc_ratio = static_cast<double>(oscillating_bg_pixels) / static_cast<double>(total_sampled);
    const double chroma_norm = std::min(1.0, (static_cast<double>(chromatic_ripple_energy) / static_cast<double>(total_sampled)) / 8.0);
    return std::clamp(osc_ratio * 1.45 + chroma_norm * 0.45, 0.0, 1.0);
}

ImageBuffer ImagePreprocessor::resize_bicubic(const ImageBuffer& src, int dst_w, int dst_h) {
    if (src.empty() || dst_w <= 0 || dst_h <= 0) return src;
    if (src.width == dst_w && src.height == dst_h) return src;

    ImageBuffer dst = ImageBuffer::create(dst_w, dst_h);
    const float scale_x = static_cast<float>(src.width) / static_cast<float>(dst_w);
    const float scale_y = static_cast<float>(src.height) / static_cast<float>(dst_h);

    for (int y = 0; y < dst_h; ++y) {
        const float fy = (static_cast<float>(y) + 0.5f) * scale_y - 0.5f;
        const int iy = static_cast<int>(std::floor(fy));
        const float ty = fy - static_cast<float>(iy);

        const int y_idx[4] = {
            std::clamp(iy - 1, 0, src.height - 1),
            std::clamp(iy + 0, 0, src.height - 1),
            std::clamp(iy + 1, 0, src.height - 1),
            std::clamp(iy + 2, 0, src.height - 1)
        };

        const uint8_t* r0 = src.row_ptr(y_idx[0]);
        const uint8_t* r1 = src.row_ptr(y_idx[1]);
        const uint8_t* r2 = src.row_ptr(y_idx[2]);
        const uint8_t* r3 = src.row_ptr(y_idx[3]);
        uint8_t* dst_row = dst.row_ptr(y);

        for (int x = 0; x < dst_w; ++x) {
            const float fx = (static_cast<float>(x) + 0.5f) * scale_x - 0.5f;
            const int ix = static_cast<int>(std::floor(fx));
            const float tx = fx - static_cast<float>(ix);

            const size_t x_off[4] = {
                static_cast<size_t>(std::clamp(ix - 1, 0, src.width - 1)) * 4,
                static_cast<size_t>(std::clamp(ix + 0, 0, src.width - 1)) * 4,
                static_cast<size_t>(std::clamp(ix + 1, 0, src.width - 1)) * 4,
                static_cast<size_t>(std::clamp(ix + 2, 0, src.width - 1)) * 4
            };

            const size_t out_off = static_cast<size_t>(x) * 4;
            for (size_t c = 0; c < 3; ++c) {
                const float col0 = cubic_hermite(r0[x_off[0] + c], r0[x_off[1] + c], r0[x_off[2] + c], r0[x_off[3] + c], tx);
                const float col1 = cubic_hermite(r1[x_off[0] + c], r1[x_off[1] + c], r1[x_off[2] + c], r1[x_off[3] + c], tx);
                const float col2 = cubic_hermite(r2[x_off[0] + c], r2[x_off[1] + c], r2[x_off[2] + c], r2[x_off[3] + c], tx);
                const float col3 = cubic_hermite(r3[x_off[0] + c], r3[x_off[1] + c], r3[x_off[2] + c], r3[x_off[3] + c], tx);
                const float val = cubic_hermite(col0, col1, col2, col3, ty);
                dst_row[out_off + c] = clamp_u8(static_cast<int>(std::lround(val)));
            }
            dst_row[out_off + 3] = 255;
        }
    }
    return dst;
}

ImageBuffer ImagePreprocessor::invert_luminance_preserving_contrast(const ImageBuffer& src) {
    if (src.empty()) return src;
    ImageBuffer dst = ImageBuffer::create(src.width, src.height);

    // Pure perceptual luminance inversion (avoids ClearType RGB subpixel fringe distortion!)
    std::array<uint64_t, 256> hist{};
    const size_t total_px = static_cast<size_t>(src.width) * static_cast<size_t>(src.height);

    for (int y = 0; y < src.height; ++y) {
        const uint8_t* srow = src.row_ptr(y);
        for (int x = 0; x < src.width; ++x) {
            const size_t i = static_cast<size_t>(x) * 4;
            const uint8_t inv_y = static_cast<uint8_t>(255u - luma_from_bgr(srow[i], srow[i + 1], srow[i + 2]));
            hist[inv_y]++;
        }
    }

    int lo = 0;
    int hi = 255;
    uint64_t acc = 0;
    for (int v = 0; v < 256; ++v) {
        acc += hist[static_cast<size_t>(v)];
        if (acc * 1000 >= total_px * 5) { lo = v; break; } // 0.5th percentile
    }
    acc = 0;
    for (int v = 255; v >= 0; --v) {
        acc += hist[static_cast<size_t>(v)];
        if (acc * 100 >= total_px * 2) { hi = v; break; } // 98th percentile
    }
    const int span = std::max(40, hi - lo);

    for (int y = 0; y < src.height; ++y) {
        const uint8_t* srow = src.row_ptr(y);
        uint8_t* drow = dst.row_ptr(y);
        for (int x = 0; x < src.width; ++x) {
            const size_t i = static_cast<size_t>(x) * 4;
            const int inv = 255 - static_cast<int>(luma_from_bgr(srow[i], srow[i + 1], srow[i + 2]));
            const uint8_t stretched = clamp_u8((inv - lo) * 255 / span);
            drow[i + 0] = stretched;
            drow[i + 1] = stretched;
            drow[i + 2] = stretched;
            drow[i + 3] = 255;
        }
    }
    return dst;
}

ImageBuffer ImagePreprocessor::restore_moire_and_uneven_lighting(const ImageBuffer& src) {
    if (src.empty()) return src;

    const bool dark = detect_dark_mode(src);
    const int w = src.width;
    const int h = src.height;
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);

    // Step 1: Perceptual grayscale (inverted cleanly if dark mode)
    std::vector<uint8_t> gray(n);
    for (int y = 0; y < h; ++y) {
        const uint8_t* srow = src.row_ptr(y);
        uint8_t* grow = gray.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        for (int x = 0; x < w; ++x) {
            const size_t i = static_cast<size_t>(x) * 4;
            const uint8_t lum = luma_from_bgr(srow[i], srow[i + 1], srow[i + 2]);
            grow[x] = dark ? static_cast<uint8_t>(255u - lum) : lum;
        }
    }

    // Step 2: Separable 3-tap binomial low-pass [1, 2, 1]/4 at native resolution
    // Attenuates high-frequency CMOS/LCD moire interference (~75% notch at Nyquist/2)
    // WITHOUT erasing thin 1.5px glyph strokes!
    std::vector<uint8_t> tmp(gray);
    std::vector<uint8_t> smooth(gray);
    for (int y = 0; y < h; ++y) {
        const uint8_t* row = gray.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        uint8_t* out = tmp.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        for (int x = 1; x < w - 1; ++x) {
            out[x] = static_cast<uint8_t>((row[x - 1] + 2 * row[x] + row[x + 1] + 2) >> 2);
        }
    }
    for (int y = 1; y < h - 1; ++y) {
        const uint8_t* r0 = tmp.data() + static_cast<size_t>(y - 1) * static_cast<size_t>(w);
        const uint8_t* r1 = tmp.data() + static_cast<size_t>(y + 0) * static_cast<size_t>(w);
        const uint8_t* r2 = tmp.data() + static_cast<size_t>(y + 1) * static_cast<size_t>(w);
        uint8_t* out = smooth.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        for (int x = 0; x < w; ++x) {
            out[x] = static_cast<uint8_t>((r0[x] + 2 * r1[x] + r2[x] + 2) >> 2);
        }
    }

    // Step 3: O(1) Integral-Image Local Illumination Flattening (removes camera vignette & glare)
    const size_t iw = static_cast<size_t>(w + 1);
    std::vector<uint64_t> integral(iw * static_cast<size_t>(h + 1), 0);
    for (int y = 0; y < h; ++y) {
        uint64_t row_sum = 0;
        const uint8_t* srow = smooth.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        const uint64_t* prev_irow = integral.data() + static_cast<size_t>(y) * iw;
        uint64_t* curr_irow = integral.data() + static_cast<size_t>(y + 1) * iw;
        for (int x = 0; x < w; ++x) {
            row_sum += srow[x];
            curr_irow[x + 1] = prev_irow[x + 1] + row_sum;
        }
    }

    const int radius = std::max(40, std::min(w, h) / 10);
    ImageBuffer restored = ImageBuffer::create(w, h);

    for (int y = 0; y < h; ++y) {
        const int y0 = std::max(0, y - radius);
        const int y1 = std::min(h, y + radius + 1);
        const uint64_t* ir0 = integral.data() + static_cast<size_t>(y0) * iw;
        const uint64_t* ir1 = integral.data() + static_cast<size_t>(y1) * iw;
        const uint8_t* srow = smooth.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        uint8_t* drow = restored.row_ptr(y);

        for (int x = 0; x < w; ++x) {
            const int x0 = std::max(0, x - radius);
            const int x1 = std::min(w, x + radius + 1);
            const uint64_t area = static_cast<uint64_t>(x1 - x0) * static_cast<uint64_t>(y1 - y0);
            const uint64_t sum = ir1[x1] - ir0[x1] - ir1[x0] + ir0[x0];
            const int local_bg = std::max(55, static_cast<int>(sum / area) + 6);
            // Smooth linear gain preserving anti-aliased gray glyph ramps!
            int norm = (static_cast<int>(srow[x]) * 244) / local_bg;
            if (norm < 175) {
                norm = std::max(0, ((norm - 175) * 128) / 100 + 175);
            }
            const uint8_t v = clamp_u8(norm);
            const size_t i = static_cast<size_t>(x) * 4;
            drow[i + 0] = v;
            drow[i + 1] = v;
            drow[i + 2] = v;
            drow[i + 3] = 255;
        }
    }

    return sharpen_glyphs(restored, 0.42f);
}

ImageBuffer ImagePreprocessor::sharpen_glyphs(const ImageBuffer& src, float amount) {
    if (src.width < 4 || src.height < 4) return src;
    ImageBuffer dst = src;

    const int gain_q8 = static_cast<int>(std::lround(amount * 256.0f));
    for (int y = 1; y < src.height - 1; ++y) {
        const uint8_t* r0 = src.row_ptr(y - 1);
        const uint8_t* r1 = src.row_ptr(y);
        const uint8_t* r2 = src.row_ptr(y + 1);
        uint8_t* out = dst.row_ptr(y);

        for (int x = 1; x < src.width - 1; ++x) {
            const size_t i = static_cast<size_t>(x) * 4;
            for (size_t c = 0; c < 3; ++c) {
                const int center = r1[i + c];
                const int blur = (r0[i + c] + r2[i + c] + r1[i - 4 + c] + r1[i + 4 + c] + 4 * center) >> 3;
                const int high_pass = center - blur;
                out[i + c] = clamp_u8(center + ((high_pass * gain_q8) >> 8));
            }
        }
    }
    return dst;
}

PreprocessOutput ImagePreprocessor::process(const ImageBuffer& input) const {
    const auto t0 = std::chrono::steady_clock::now();
    PreprocessOutput out;
    out.diagnostics.original_width = input.width;
    out.diagnostics.original_height = input.height;

    if (input.empty()) return out;

    out.diagnostics.is_dark_mode = detect_dark_mode(input, &out.diagnostics.mean_luminance);
    out.diagnostics.moire_score = compute_moire_score(input);
    out.diagnostics.moire_detected = opts_.force_anti_moire || (opts_.auto_mitigate_moire && out.diagnostics.moire_score >= 0.16);

    auto scale_if_needed = [&](const ImageBuffer& buf) -> ImageBuffer {
        if (buf.width < 1850) {
            const double factor = (buf.width < 1000) ? 2.0 : 1.5;
            const int nw = static_cast<int>(std::lround(buf.width * factor));
            const int nh = static_cast<int>(std::lround(buf.height * factor));
            out.diagnostics.upscaled = true;
            return resize_bicubic(buf, nw, nh);
        }
        if (buf.width > opts_.max_target_width) {
            const double factor = static_cast<double>(opts_.max_target_width) / static_cast<double>(buf.width);
            const int nw = opts_.max_target_width;
            const int nh = std::max(1, static_cast<int>(std::lround(buf.height * factor)));
            return resize_bicubic(buf, nw, nh);
        }
        return buf;
    };

    // Suppress high-frequency moire & flatten local dark-mode/camera backgrounds at native sensor resolution!
    if (out.diagnostics.moire_detected || out.diagnostics.is_dark_mode) {
        ImageBuffer restored_native = restore_moire_and_uneven_lighting(input);
        out.anti_moire_image = sharpen_glyphs(scale_if_needed(restored_native), 0.35f);
    }

    ImageBuffer scaled = scale_if_needed(input);
    out.diagnostics.processed_width = scaled.width;
    out.diagnostics.processed_height = scaled.height;

    if (opts_.auto_invert_dark_mode && out.diagnostics.is_dark_mode) {
        out.primary_image = sharpen_glyphs(invert_luminance_preserving_contrast(scaled), 0.32f);
    } else {
        out.primary_image = sharpen_glyphs(scaled, 0.25f);
    }

    const auto t1 = std::chrono::steady_clock::now();
    out.diagnostics.preprocess_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return out;
}

} // namespace ticket_ocr

#pragma once

#include "ticket_ocr/types.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace ticket_ocr {

class CorpusGenerator {
public:
    // Builds the canonical 18-sample anonymized multi-OS, multi-language, dark-mode & mobile-moire corpus definition
    static std::vector<CorpusSample> build_default_corpus_spec();

    // Renders a single CorpusSample into a realistic screenshot (with optional mobile camera moire + vignette + tilt)
    static ImageBuffer render_sample_image(const CorpusSample& sample);

    // Generates all images onto disk + writes manifest.json with ground truth & support team labels
    static bool generate_corpus_on_disk(
        const std::filesystem::path& output_dir,
        std::vector<CorpusSample>* out_samples = nullptr,
        std::string* error_out = nullptr
    );

    // Loads corpus specification from manifest.json on disk
    static std::vector<CorpusSample> load_manifest(const std::filesystem::path& manifest_path);
};

} // namespace ticket_ocr

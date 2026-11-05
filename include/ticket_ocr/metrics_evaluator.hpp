#pragma once

#include "ticket_ocr/json_utils.hpp"
#include "ticket_ocr/ticket_pipeline.hpp"
#include "ticket_ocr/types.hpp"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace ticket_ocr {

class MetricsEvaluator {
public:
    // Exact UTF-8 codepoint Levenshtein Character Error Rate: (S + D + I) / max(1, |ref|)
    static double compute_cer(std::string_view reference, std::string_view hypothesis);

    // Normalized comparison CER (case/accent-normalized) for cross-locale OCR tolerance
    static double compute_normalized_cer(std::string_view reference, std::string_view hypothesis);

    // Evaluate entire corpus end-to-end with optional progress callback for GUI/CLI
    static BenchmarkReport evaluate_corpus(
        const TicketPipeline& pipeline,
        const std::filesystem::path& corpus_root_dir,
        const std::vector<CorpusSample>& samples,
        std::function<void(size_t current, size_t total, const std::string& sample_id)> on_progress = nullptr
    );

    static JsonValue report_to_json(const BenchmarkReport& report);
    static std::string report_to_markdown(const BenchmarkReport& report);
};

} // namespace ticket_ocr

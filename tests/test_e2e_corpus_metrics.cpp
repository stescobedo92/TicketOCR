#include "tests/test_framework.hpp"
#include "ticket_ocr/corpus_generator.hpp"
#include "ticket_ocr/json_utils.hpp"
#include "ticket_ocr/metrics_evaluator.hpp"
#include "ticket_ocr/ticket_pipeline.hpp"

#include <iomanip>
#include <iostream>

using namespace ticket_ocr;

TEST_CASE(FullCorpusEndToEndMetricsAndZeroLeakageGate) {
    const std::filesystem::path corpus_dir = std::filesystem::path(TICKET_OCR_SOURCE_DIR) / "assets" / "corpus";
    std::vector<CorpusSample> samples;
    std::string err;
    REQUIRE_TRUE(CorpusGenerator::generate_corpus_on_disk(corpus_dir, &samples, &err));
    REQUIRE_EQ(samples.size(), static_cast<size_t>(18));

    TicketPipeline pipeline;
    const BenchmarkReport rep = MetricsEvaluator::evaluate_corpus(pipeline, corpus_dir, samples);

    std::cout << "         -> CER Global Campos Clave : " << std::fixed << std::setprecision(4) << rep.overall_key_fields_cer << "\n"
              << "         -> CER Efecto Moiré        : " << rep.moire_subset_cer << "\n"
              << "         -> CER Modo Oscuro         : " << rep.dark_mode_subset_cer << "\n"
              << "         -> Macro-F1 Categoría      : " << rep.macro_f1 << "\n"
              << "         -> Minutos / Ticket        : " << std::setprecision(2) << rep.assisted_minutes_per_ticket << " min\n"
              << "         -> % Sin Pedir Más Info    : " << rep.pct_tickets_without_requesting_more_info << "%\n"
              << "         -> Fugas Sensibles (Meta 0): " << rep.total_sensitive_leaks << "\n";

    // Persist artifacts for human inspection as well
    write_text_file(std::filesystem::path(TICKET_OCR_SOURCE_DIR) / "reports" / "benchmark_report.md",
                    MetricsEvaluator::report_to_markdown(rep));
    write_text_file(std::filesystem::path(TICKET_OCR_SOURCE_DIR) / "reports" / "benchmark_metrics.json",
                    serialize_json(MetricsEvaluator::report_to_json(rep), 2));

    // Strict Quality Gates
    REQUIRE_EQ(rep.total_sensitive_leaks, static_cast<size_t>(0));
    REQUIRE_GE(rep.macro_f1, 0.85);
    REQUIRE_LE(rep.overall_key_fields_cer, 0.15);
    REQUIRE_GE(rep.pct_tickets_without_requesting_more_info, 80.0);
    REQUIRE_LE(rep.assisted_minutes_per_ticket, 1.0);
    REQUIRE_TRUE(rep.all_quality_gates_passed);
}

int main() {
    return test_fw::run_all_tests("Pipeline End-to-End sobre Corpus Anonimizado Completo y Métricas");
}

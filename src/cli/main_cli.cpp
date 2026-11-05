#include "ticket_ocr/corpus_generator.hpp"
#include "ticket_ocr/json_utils.hpp"
#include "ticket_ocr/metrics_evaluator.hpp"
#include "ticket_ocr/ticket_pipeline.hpp"

#include <Windows.h>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

void print_usage() {
    std::cout <<
        "TicketOCR CLI & Automated Evaluation Harness (C++20)\n"
        "Uso:\n"
        "  ticket_ocr_cli --generate-corpus [carpeta_destino]\n"
        "  ticket_ocr_cli --export-kb [ruta_json]\n"
        "  ticket_ocr_cli --analyze <ruta_captura> [--json salida.json] [--md salida.md] [--force-anti-moire]\n"
        "  ticket_ocr_cli --benchmark [carpeta_corpus] [--report reporte.md] [--json metricas.json]\n"
        "  ticket_ocr_cli --interactive-qa\n";
}

} // namespace

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);

    if (args.empty() || args[0] == "--help" || args[0] == "-h") {
        print_usage();
        return 0;
    }

    if (args[0] == "--export-kb") {
        std::filesystem::path out_kb = (args.size() >= 2)
            ? std::filesystem::path(ticket_ocr::utf8_to_wide(args[1]))
            : std::filesystem::path("data/knowledge_base.json");
        ticket_ocr::KbRecommender kb;
        if (!kb.save_to_json_file(out_kb)) {
            std::cerr << "Error al exportar catálogo KB.\n";
            return 1;
        }
        std::cout << "[OK] Base de Conocimiento exportada a: " << ticket_ocr::wide_to_utf8(out_kb.wstring())
                  << " (" << kb.articles().size() << " artículos).\n";
        return 0;
    }

    if (args[0] == "--generate-corpus") {
        std::filesystem::path out_dir = (args.size() >= 2)
            ? std::filesystem::path(ticket_ocr::utf8_to_wide(args[1]))
            : std::filesystem::path("assets/corpus");
        std::vector<ticket_ocr::CorpusSample> samples;
        std::string err;
        if (!ticket_ocr::CorpusGenerator::generate_corpus_on_disk(out_dir, &samples, &err)) {
            std::cerr << "[ERROR] Fallo generando corpus: " << err << "\n";
            return 1;
        }
        std::cout << "[OK] Corpus anonimizado generado en '" << ticket_ocr::wide_to_utf8(out_dir.wstring())
                  << "' con " << samples.size() << " capturas (SO variados, ES/EN, Modo Oscuro, Efecto Moiré y PII plantada).\n";
        return 0;
    }

    if (args[0] == "--analyze" && args.size() >= 2) {
        const std::filesystem::path img_path = ticket_ocr::utf8_to_wide(args[1]);
        std::filesystem::path out_json;
        std::filesystem::path out_md;
        ticket_ocr::PreprocessorOptions opts;

        for (size_t i = 2; i < args.size(); ++i) {
            if (args[i] == "--json" && i + 1 < args.size()) {
                out_json = ticket_ocr::utf8_to_wide(args[++i]);
            } else if (args[i] == "--md" && i + 1 < args.size()) {
                out_md = ticket_ocr::utf8_to_wide(args[++i]);
            } else if (args[i] == "--force-anti-moire") {
                opts.force_anti_moire = true;
            }
        }

        ticket_ocr::TicketPipeline pipeline(opts);
        std::string err;
        auto res = pipeline.process_file(img_path, &err);
        if (!res) {
            std::cerr << "[ERROR] " << err << "\n";
            return 1;
        }

        std::cout << ticket_ocr::TicketPipeline::ticket_to_markdown(res->ticket) << "\n";
        if (!out_json.empty()) {
            ticket_ocr::write_text_file(out_json, ticket_ocr::TicketPipeline::ticket_to_json_string(res->ticket));
            std::cout << "[OK] JSON guardado en: " << ticket_ocr::wide_to_utf8(out_json.wstring()) << "\n";
        }
        if (!out_md.empty()) {
            ticket_ocr::write_text_file(out_md, ticket_ocr::TicketPipeline::ticket_to_markdown(res->ticket));
            std::cout << "[OK] Markdown guardado en: " << ticket_ocr::wide_to_utf8(out_md.wstring()) << "\n";
        }
        return 0;
    }

    if (args[0] == "--benchmark") {
        std::filesystem::path corpus_dir = "assets/corpus";
        std::filesystem::path out_md = "reports/benchmark_report.md";
        std::filesystem::path out_json = "reports/benchmark_metrics.json";

        size_t i = 1;
        if (i < args.size() && !args[i].starts_with("--")) {
            corpus_dir = ticket_ocr::utf8_to_wide(args[i++]);
        }
        for (; i < args.size(); ++i) {
            if (args[i] == "--report" && i + 1 < args.size()) {
                out_md = ticket_ocr::utf8_to_wide(args[++i]);
            } else if (args[i] == "--json" && i + 1 < args.size()) {
                out_json = ticket_ocr::utf8_to_wide(args[++i]);
            }
        }

        if (!std::filesystem::exists(corpus_dir / "manifest.json")) {
            std::cout << "[INFO] Generando corpus inicial en " << ticket_ocr::wide_to_utf8(corpus_dir.wstring()) << "...\n";
            ticket_ocr::CorpusGenerator::generate_corpus_on_disk(corpus_dir);
        }

        const auto samples = ticket_ocr::CorpusGenerator::load_manifest(corpus_dir / "manifest.json");
        if (samples.empty()) {
            std::cerr << "[ERROR] No se encontraron muestras en manifest.json.\n";
            return 1;
        }

        ticket_ocr::TicketPipeline pipeline;
        const auto report = ticket_ocr::MetricsEvaluator::evaluate_corpus(
            pipeline, corpus_dir, samples,
            [](size_t cur, size_t tot, const std::string& id) {
                std::cout << "  [" << std::setw(2) << cur << "/" << tot << "] Procesando " << id << "...\n";
            }
        );

        ticket_ocr::write_text_file(out_md, ticket_ocr::MetricsEvaluator::report_to_markdown(report));
        ticket_ocr::write_text_file(out_json, ticket_ocr::serialize_json(ticket_ocr::MetricsEvaluator::report_to_json(report), 2));

        std::cout << "\n=============================================================\n";
        std::cout << "  RESULTADOS CUANTITATIVOS DEL BENCHMARK (" << report.total_samples << " CAPTURAS)\n";
        std::cout << "=============================================================\n";
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "  • CER Promedio Campos Clave      : " << report.overall_key_fields_cer
                  << " (" << std::setprecision(2) << (report.overall_key_fields_cer * 100.0) << "%)\n";
        std::cout << "  • CER Subconjunto Efecto Moiré   : " << (report.moire_subset_cer * 100.0) << "%\n";
        std::cout << "  • CER Subconjunto Modo Oscuro    : " << (report.dark_mode_subset_cer * 100.0) << "%\n";
        std::cout << std::setprecision(4);
        std::cout << "  • Macro-F1 Categoría (Soporte)   : " << report.macro_f1
                  << " (Accuracy: " << std::setprecision(1) << (report.accuracy * 100.0) << "%)\n";
        std::cout << std::setprecision(2);
        std::cout << "  • Minutos por Ticket (Asistido)  : " << report.assisted_minutes_per_ticket
                  << " min (vs " << report.manual_baseline_minutes_per_ticket << " min manual | -"
                  << report.time_saved_percent << "%)\n";
        std::cout << "  • % Tickets sin Pedir Más Info   : " << report.pct_tickets_without_requesting_more_info << "%\n";
        std::cout << "  • Fugas de Datos Sensibles (DLP) : " << report.total_sensitive_leaks
                  << " / " << report.total_planted_secrets << " (Meta: 0)\n";
        std::cout << "  • Estado Compuertas de Calidad   : "
                  << (report.all_quality_gates_passed ? "PASS" : "FAIL") << "\n";
        std::cout << "=============================================================\n";
        std::cout << "Reporte detallado: " << ticket_ocr::wide_to_utf8(out_md.wstring()) << "\n";

        return report.all_quality_gates_passed ? 0 : 2;
    }

    if (args[0] == "--interactive-qa") {
        std::filesystem::path corpus_dir = "assets/corpus";
        if (!std::filesystem::exists(corpus_dir / "manifest.json")) {
            ticket_ocr::CorpusGenerator::generate_corpus_on_disk(corpus_dir);
        }
        const auto samples = ticket_ocr::CorpusGenerator::load_manifest(corpus_dir / "manifest.json");
        ticket_ocr::TicketPipeline pipeline;

        while (true) {
            std::cout << "\n--- MODO DE PRUEBA INTERACTIVA HUMANA (QA) ---\n";
            for (size_t i = 0; i < samples.size(); ++i) {
                std::cout << "  [" << (i + 1) << "] " << samples[i].sample_id
                          << " (" << samples[i].os_style << ", " << samples[i].language
                          << (samples[i].dark_mode ? ", Dark" : ", Light")
                          << (samples[i].mobile_moire_photo ? ", Moiré Celular" : "") << ")\n";
            }
            std::cout << "Ingrese número de muestra (1-" << samples.size() << "), ruta de imagen, o 'q' para salir: ";
            std::string line;
            if (!std::getline(std::cin, line) || line == "q" || line == "Q") break;

            std::filesystem::path target_path;
            try {
                size_t idx = std::stoul(line);
                if (idx >= 1 && idx <= samples.size()) {
                    target_path = corpus_dir / ticket_ocr::utf8_to_wide(samples[idx - 1].relative_image_path);
                }
            } catch (...) {
                target_path = ticket_ocr::utf8_to_wide(line);
            }

            if (target_path.empty() || !std::filesystem::exists(target_path)) {
                std::cout << "Ruta o selección inválida.\n";
                continue;
            }

            auto res = pipeline.process_file(target_path);
            if (res) {
                std::cout << "\n" << ticket_ocr::TicketPipeline::ticket_to_markdown(res->ticket) << "\n";
            }
        }
        return 0;
    }

    print_usage();
    return 1;
}

#include "ticket_ocr/metrics_evaluator.hpp"
#include "ticket_ocr/dlp_redactor.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <vector>

namespace ticket_ocr {

namespace {

std::u32string utf8_to_u32(std::string_view s) {
    std::u32string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            out.push_back(static_cast<char32_t>(c));
            ++i;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
            char32_t cp = (static_cast<char32_t>(c & 0x1F) << 6) |
                          (static_cast<char32_t>(static_cast<unsigned char>(s[i + 1]) & 0x3F));
            out.push_back(cp);
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
            char32_t cp = (static_cast<char32_t>(c & 0x0F) << 12) |
                          (static_cast<char32_t>(static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6) |
                          (static_cast<char32_t>(static_cast<unsigned char>(s[i + 2]) & 0x3F));
            out.push_back(cp);
            i += 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < s.size()) {
            char32_t cp = (static_cast<char32_t>(c & 0x07) << 18) |
                          (static_cast<char32_t>(static_cast<unsigned char>(s[i + 1]) & 0x3F) << 12) |
                          (static_cast<char32_t>(static_cast<unsigned char>(s[i + 2]) & 0x3F) << 6) |
                          (static_cast<char32_t>(static_cast<unsigned char>(s[i + 3]) & 0x3F));
            out.push_back(cp);
            i += 4;
        } else {
            out.push_back(static_cast<char32_t>(c));
            ++i;
        }
    }
    return out;
}

size_t levenshtein_u32(const std::u32string& a, const std::u32string& b) {
    if (a.empty()) return b.size();
    if (b.empty()) return a.size();

    std::vector<size_t> prev(b.size() + 1);
    std::vector<size_t> curr(b.size() + 1);
    for (size_t j = 0; j <= b.size(); ++j) prev[j] = j;

    for (size_t i = 1; i <= a.size(); ++i) {
        curr[0] = i;
        for (size_t j = 1; j <= b.size(); ++j) {
            const size_t cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            curr[j] = std::min({
                prev[j] + 1,         // deletion
                curr[j - 1] + 1,     // insertion
                prev[j - 1] + cost   // substitution
            });
        }
        std::swap(prev, curr);
    }
    return prev[b.size()];
}

} // namespace

double MetricsEvaluator::compute_cer(std::string_view reference, std::string_view hypothesis) {
    const std::string ref_trim = trim_copy(reference);
    const std::string hyp_trim = trim_copy(hypothesis);
    if (ref_trim.empty()) {
        return hyp_trim.empty() ? 0.0 : 1.0;
    }
    const std::u32string u_ref = utf8_to_u32(ref_trim);
    const std::u32string u_hyp = utf8_to_u32(hyp_trim);
    const size_t dist = levenshtein_u32(u_ref, u_hyp);
    return std::min(1.0, static_cast<double>(dist) / static_cast<double>(u_ref.size()));
}

double MetricsEvaluator::compute_normalized_cer(std::string_view reference, std::string_view hypothesis) {
    const std::string n_ref = to_lower_ascii(fold_spanish_accents_to_ascii(trim_copy(reference)));
    const std::string n_hyp = to_lower_ascii(fold_spanish_accents_to_ascii(trim_copy(hypothesis)));
    return compute_cer(n_ref, n_hyp);
}

BenchmarkReport MetricsEvaluator::evaluate_corpus(
    const TicketPipeline& pipeline,
    const std::filesystem::path& corpus_root_dir,
    const std::vector<CorpusSample>& samples,
    std::function<void(size_t, size_t, const std::string&)> on_progress) {
    BenchmarkReport rep;
    rep.total_samples = samples.size();
    if (samples.empty()) return rep;

    size_t code_count = 0, mod_count = 0, ver_count = 0, url_count = 0, msg_count = 0;
    double sum_cer_code = 0.0, sum_cer_mod = 0.0, sum_cer_ver = 0.0, sum_cer_url = 0.0, sum_cer_msg = 0.0;
    double sum_moire_cer = 0.0;
    size_t moire_count = 0;
    double sum_dark_cer = 0.0;
    size_t dark_count = 0;

    size_t correct_cats = 0;
    size_t actionable_count = 0;
    double sum_ms = 0.0;
    double sum_assisted_minutes = 0.0;

    rep.sample_results.reserve(samples.size());

    for (size_t idx = 0; idx < samples.size(); ++idx) {
        const auto& s = samples[idx];
        if (on_progress) {
            on_progress(idx + 1, samples.size(), s.sample_id);
        }

        const std::filesystem::path img_path = corpus_root_dir / utf8_to_wide(s.relative_image_path);
        std::string err;
        auto run_opt = pipeline.process_file(img_path, &err);

        SampleEvaluation ev;
        ev.sample_id = s.sample_id;
        ev.dark_mode = s.dark_mode;
        ev.mobile_moire = s.mobile_moire_photo;
        ev.language = s.language;
        ev.os_style = s.os_style;
        ev.expected_category = s.gt_category;
        rep.total_planted_secrets += s.planted_secrets.size();

        if (!run_opt) {
            ev.cer_key_fields_mean = 1.0;
            rep.sample_results.push_back(std::move(ev));
            continue;
        }

        const TicketDraft& t = run_opt->ticket;
        ev.generated_ticket = t;
        ev.processing_ms = t.total_processing_ms;
        sum_ms += t.total_processing_ms;
        sum_assisted_minutes += t.estimated_agent_minutes;

        // Compute per-field CER on non-empty ground-truth key fields
        double sample_cer_sum = 0.0;
        int sample_cer_fields = 0;

        if (!s.gt_error_code.empty()) {
            ev.cer_error_code = compute_cer(s.gt_error_code, t.fields.error_code);
            sum_cer_code += ev.cer_error_code;
            ++code_count;
            sample_cer_sum += ev.cer_error_code;
            ++sample_cer_fields;
        }
        if (!s.gt_module.empty()) {
            ev.cer_module = compute_cer(s.gt_module, t.fields.module_name);
            sum_cer_mod += ev.cer_module;
            ++mod_count;
            sample_cer_sum += ev.cer_module;
            ++sample_cer_fields;
        }
        if (!s.gt_version.empty()) {
            ev.cer_version = compute_cer(s.gt_version, t.fields.software_version);
            sum_cer_ver += ev.cer_version;
            ++ver_count;
            sample_cer_sum += ev.cer_version;
            ++sample_cer_fields;
        }
        if (!s.gt_sanitized_url.empty()) {
            ev.cer_url = compute_cer(s.gt_sanitized_url, t.fields.sanitized_url);
            sum_cer_url += ev.cer_url;
            ++url_count;
            sample_cer_sum += ev.cer_url;
            ++sample_cer_fields;
        }
        if (!s.gt_error_message.empty()) {
            ev.cer_message = compute_cer(s.gt_error_message, t.fields.error_message);
            sum_cer_msg += ev.cer_message;
            ++msg_count;
            sample_cer_sum += ev.cer_message;
            ++sample_cer_fields;
        }

        ev.cer_key_fields_mean = sample_cer_fields > 0 ? (sample_cer_sum / sample_cer_fields) : 0.0;
        if (s.mobile_moire_photo) {
            sum_moire_cer += ev.cer_key_fields_mean;
            ++moire_count;
        }
        if (s.dark_mode) {
            sum_dark_cer += ev.cer_key_fields_mean;
            ++dark_count;
        }

        // Classification evaluation
        ev.predicted_category = t.category;
        ev.category_correct = (ev.predicted_category == ev.expected_category);
        if (ev.category_correct) ++correct_cats;

        const int r_idx = static_cast<int>(ev.expected_category);
        const int c_idx = static_cast<int>(ev.predicted_category);
        if (r_idx >= 0 && r_idx < 6 && c_idx >= 0 && c_idx < 6) {
            rep.confusion_matrix[r_idx][c_idx]++;
        }

        // Completeness without asking user for more info
        ev.actionable_without_followup = t.fields.actionable_without_followup;
        if (ev.actionable_without_followup) ++actionable_count;

        // Strict DLP Audit over full serialized JSON + Markdown outputs!
        ev.redactions_performed = t.dlp_summary.total_redactions;
        rep.total_redactions_applied += t.dlp_summary.total_redactions;

        const std::string full_serialized =
            TicketPipeline::ticket_to_json_string(t) + "\n" + TicketPipeline::ticket_to_markdown(t);
        ev.leaked_items = DlpRedactor::audit_for_leaks(full_serialized, s.planted_secrets);
        ev.sensitive_leaks_detected = ev.leaked_items.size();
        rep.total_sensitive_leaks += ev.sensitive_leaks_detected;

        rep.sample_results.push_back(std::move(ev));
    }

    rep.mean_cer_error_code = code_count > 0 ? sum_cer_code / static_cast<double>(code_count) : 0.0;
    rep.mean_cer_module     = mod_count > 0  ? sum_cer_mod  / static_cast<double>(mod_count)  : 0.0;
    rep.mean_cer_version    = ver_count > 0  ? sum_cer_ver  / static_cast<double>(ver_count)  : 0.0;
    rep.mean_cer_url        = url_count > 0  ? sum_cer_url  / static_cast<double>(url_count)  : 0.0;
    rep.mean_cer_message    = msg_count > 0  ? sum_cer_msg  / static_cast<double>(msg_count)  : 0.0;

    rep.overall_key_fields_cer = (
        rep.mean_cer_error_code +
        rep.mean_cer_module +
        rep.mean_cer_version +
        rep.mean_cer_url +
        rep.mean_cer_message
    ) / 5.0;

    rep.moire_subset_cer     = moire_count > 0 ? sum_moire_cer / static_cast<double>(moire_count) : 0.0;
    rep.dark_mode_subset_cer = dark_count > 0  ? sum_dark_cer  / static_cast<double>(dark_count)  : 0.0;

    // Classification Macro-F1
    rep.accuracy = static_cast<double>(correct_cats) / static_cast<double>(samples.size());
    double sum_f1 = 0.0;
    int active_classes = 0;

    for (int c = 0; c < 6; ++c) {
        ClassMetrics cm;
        cm.category = static_cast<IncidentCategory>(c);
        cm.true_positives = rep.confusion_matrix[c][c];
        for (int k = 0; k < 6; ++k) {
            if (k != c) {
                cm.false_positives += rep.confusion_matrix[k][c];
                cm.false_negatives += rep.confusion_matrix[c][k];
            }
        }
        const int denom_p = cm.true_positives + cm.false_positives;
        const int denom_r = cm.true_positives + cm.false_negatives;
        cm.precision = denom_p > 0 ? static_cast<double>(cm.true_positives) / denom_p : 0.0;
        cm.recall    = denom_r > 0 ? static_cast<double>(cm.true_positives) / denom_r : 0.0;
        cm.f1 = (cm.precision + cm.recall) > 0.0
            ? (2.0 * cm.precision * cm.recall) / (cm.precision + cm.recall)
            : 0.0;

        if (denom_r > 0 || denom_p > 0) {
            sum_f1 += cm.f1;
            ++active_classes;
        }
        rep.per_class_metrics.push_back(cm);
    }
    rep.macro_f1 = active_classes > 0 ? sum_f1 / static_cast<double>(active_classes) : 0.0;

    // Operational time & first-contact completeness
    rep.avg_processing_ms = sum_ms / static_cast<double>(samples.size());
    rep.assisted_minutes_per_ticket = sum_assisted_minutes / static_cast<double>(samples.size());
    rep.time_saved_percent =
        ((rep.manual_baseline_minutes_per_ticket - rep.assisted_minutes_per_ticket) /
         rep.manual_baseline_minutes_per_ticket) * 100.0;
    rep.pct_tickets_without_requesting_more_info =
        (static_cast<double>(actionable_count) / static_cast<double>(samples.size())) * 100.0;

    // Quality gates
    rep.all_quality_gates_passed =
        (rep.total_sensitive_leaks == 0) &&
        (rep.macro_f1 >= 0.85) &&
        (rep.overall_key_fields_cer <= 0.15) &&
        (rep.pct_tickets_without_requesting_more_info >= 80.0);

    return rep;
}

JsonValue MetricsEvaluator::report_to_json(const BenchmarkReport& report) {
    JsonObject root;
    root["total_samples"] = report.total_samples;
    root["all_quality_gates_passed"] = report.all_quality_gates_passed;

    JsonObject cer;
    cer["error_code"] = report.mean_cer_error_code;
    cer["module"] = report.mean_cer_module;
    cer["version"] = report.mean_cer_version;
    cer["url"] = report.mean_cer_url;
    cer["error_message"] = report.mean_cer_message;
    cer["overall_key_fields_cer"] = report.overall_key_fields_cer;
    cer["moire_subset_cer"] = report.moire_subset_cer;
    cer["dark_mode_subset_cer"] = report.dark_mode_subset_cer;
    root["cer_metrics"] = std::move(cer);

    JsonObject cls;
    cls["accuracy"] = report.accuracy;
    cls["macro_f1"] = report.macro_f1;
    JsonArray per_cls;
    for (const auto& cm : report.per_class_metrics) {
        JsonObject co;
        co["category"] = category_to_code(cm.category);
        co["tp"] = cm.true_positives;
        co["fp"] = cm.false_positives;
        co["fn"] = cm.false_negatives;
        co["precision"] = cm.precision;
        co["recall"] = cm.recall;
        co["f1"] = cm.f1;
        per_cls.emplace_back(std::move(co));
    }
    cls["per_class"] = std::move(per_cls);
    root["classification_metrics"] = std::move(cls);

    JsonObject ops;
    ops["avg_processing_ms"] = report.avg_processing_ms;
    ops["manual_baseline_minutes_per_ticket"] = report.manual_baseline_minutes_per_ticket;
    ops["assisted_minutes_per_ticket"] = report.assisted_minutes_per_ticket;
    ops["time_saved_percent"] = report.time_saved_percent;
    ops["pct_tickets_without_requesting_more_info"] = report.pct_tickets_without_requesting_more_info;
    root["operational_metrics"] = std::move(ops);

    JsonObject dlp;
    dlp["total_planted_secrets"] = report.total_planted_secrets;
    dlp["total_redactions_applied"] = report.total_redactions_applied;
    dlp["total_sensitive_leaks"] = report.total_sensitive_leaks;
    dlp["zero_leak_target_met"] = (report.total_sensitive_leaks == 0);
    root["dlp_security_metrics"] = std::move(dlp);

    JsonArray samples_arr;
    for (const auto& sr : report.sample_results) {
        JsonObject so;
        so["sample_id"] = sr.sample_id;
        so["os_style"] = sr.os_style;
        so["language"] = sr.language;
        so["dark_mode"] = sr.dark_mode;
        so["mobile_moire"] = sr.mobile_moire;
        so["cer_key_fields_mean"] = sr.cer_key_fields_mean;
        so["expected_category"] = category_to_code(sr.expected_category);
        so["predicted_category"] = category_to_code(sr.predicted_category);
        so["category_correct"] = sr.category_correct;
        so["actionable_without_followup"] = sr.actionable_without_followup;
        so["redactions_performed"] = sr.redactions_performed;
        so["sensitive_leaks_detected"] = sr.sensitive_leaks_detected;
        so["processing_ms"] = sr.processing_ms;
        samples_arr.emplace_back(std::move(so));
    }
    root["samples"] = std::move(samples_arr);

    return root;
}

std::string MetricsEvaluator::report_to_markdown(const BenchmarkReport& report) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);
    oss << "# Reporte de Evaluación y Métricas del Corpus (End-to-End)\n\n";
    oss << "- **Total de Capturas Evaluadas:** " << report.total_samples << "\n";
    oss << "- **Estado Global de Compuertas de Calidad:** "
        << (report.all_quality_gates_passed ? "APROBADO (PASS)" : "REQUIERE REVISIÓN (FAIL)") << "\n\n";

    oss << "## 1. Resumen Ejecutivo de KPIs\n\n";
    oss << "| Métrica Clave | Resultado Obtenido | Meta / Referencia |\n";
    oss << "|---|---|---|\n";
    oss << "| **CER Promedio en Campos Clave** | `" << (report.overall_key_fields_cer * 100.0) << "%` (" << report.overall_key_fields_cer << ") | `<= 15.0%` |\n";
    oss << "| **CER Subconjunto Efecto Moiré (Foto Celular)** | `" << (report.moire_subset_cer * 100.0) << "%` | Mitigación activa |\n";
    oss << "| **CER Subconjunto Modo Oscuro** | `" << (report.dark_mode_subset_cer * 100.0) << "%` | Inversión adaptativa |\n";
    oss << "| **Macro-F1 de Categoría (vs Soporte)** | `" << report.macro_f1 << "` (`" << std::setprecision(1) << (report.accuracy * 100.0) << "%` acc) | `>= 0.85` |\n";
    oss << std::setprecision(2);
    oss << "| **Minutos por Ticket (Asistido vs Manual)** | `" << report.assisted_minutes_per_ticket << " min` (ahorro `" << report.time_saved_percent << "%`) | Baseline manual: `5.20 min` |\n";
    oss << "| **Latencia de Cómputo C++ por Captura** | `" << report.avg_processing_ms << " ms` | `< 500 ms` |\n";
    oss << "| **% Tickets sin Pedir Más Información** | `" << report.pct_tickets_without_requesting_more_info << "%` | `>= 80.0%` |\n";
    oss << "| **Fuga de Datos Sensibles (PII / Tokens)** | **`" << report.total_sensitive_leaks << "` fugas** (de " << report.total_planted_secrets << " secretos plantados) | **Meta estricta: `0`** |\n\n";

    oss << std::setprecision(4);
    oss << "## 2. Desglose de CER (Character Error Rate) por Campo Clave\n\n";
    oss << "| Campo Extraído | CER Promedio |\n";
    oss << "|---|---|\n";
    oss << "| `error_code` (Código) | " << report.mean_cer_error_code << " (`" << std::setprecision(2) << (report.mean_cer_error_code * 100.0) << "%`) |\n" << std::setprecision(4);
    oss << "| `module` (Módulo) | " << report.mean_cer_module << " (`" << std::setprecision(2) << (report.mean_cer_module * 100.0) << "%`) |\n" << std::setprecision(4);
    oss << "| `version` (Versión) | " << report.mean_cer_version << " (`" << std::setprecision(2) << (report.mean_cer_version * 100.0) << "%`) |\n" << std::setprecision(4);
    oss << "| `url` (URL Sanitizada) | " << report.mean_cer_url << " (`" << std::setprecision(2) << (report.mean_cer_url * 100.0) << "%`) |\n" << std::setprecision(4);
    oss << "| `error_message` (Mensaje) | " << report.mean_cer_message << " (`" << std::setprecision(2) << (report.mean_cer_message * 100.0) << "%`) |\n\n";

    oss << std::setprecision(3);
    oss << "## 3. Desempeño de Clasificación por Categoría (Etiquetado Soporte)\n\n";
    oss << "| Categoría | TP | FP | FN | Precisión | Recall | F1-Score |\n";
    oss << "|---|---:|---:|---:|---:|---:|---:|\n";
    for (const auto& cm : report.per_class_metrics) {
        oss << "| `" << category_to_code(cm.category) << "` | "
            << cm.true_positives << " | " << cm.false_positives << " | " << cm.false_negatives << " | "
            << cm.precision << " | " << cm.recall << " | " << cm.f1 << " |\n";
    }
    oss << "\n## 4. Detalle por Muestra del Corpus\n\n";
    oss << "| ID | SO / Estilo | Idioma | Dark | Moiré | CER Medio | Categoría Predicha | Sin +Info | Redacciones DLP | Fugas | ms |\n";
    oss << "|---|---|---|---|---|---:|---|---|---:|---:|---:|\n";
    for (const auto& sr : report.sample_results) {
        oss << "| `" << sr.sample_id << "` | " << sr.os_style << " | " << sr.language
            << " | " << (sr.dark_mode ? "Sí" : "No")
            << " | " << (sr.mobile_moire ? "Sí" : "No")
            << " | " << std::setprecision(3) << sr.cer_key_fields_mean
            << " | `" << category_to_code(sr.predicted_category) << "` " << (sr.category_correct ? "✓" : "✗")
            << " | " << (sr.actionable_without_followup ? "Sí" : "No")
            << " | " << sr.redactions_performed
            << " | " << sr.sensitive_leaks_detected
            << " | " << std::setprecision(1) << sr.processing_ms << " |\n";
    }

    return oss.str();
}

} // namespace ticket_ocr

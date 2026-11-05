#include "ticket_ocr/ticket_pipeline.hpp"
#include "ticket_ocr/image_io.hpp"

#include <atomic>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace ticket_ocr {

namespace {

std::string next_ticket_id() {
    static std::atomic<uint32_t> counter{1000};
    const uint32_t val = ++counter;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "INC-2026-%05u", val);
    return std::string(buf);
}

std::string build_ticket_title(const ExtractedFields& f, IncidentCategory cat) {
    std::ostringstream oss;
    if (!f.error_code.empty()) {
        oss << "[" << f.error_code << "] ";
    } else {
        oss << "[" << category_to_code(cat) << "] ";
    }

    if (!f.module_name.empty()) {
        oss << f.module_name << " - ";
    }

    std::string summary = f.error_message;
    if (summary.empty()) {
        summary = "Incidente reportado mediante captura de pantalla";
    }
    if (summary.size() > 78) {
        summary = summary.substr(0, 75) + "...";
    }
    oss << summary;
    return oss.str();
}

std::string build_formatted_description(const TicketDraft& t) {
    std::ostringstream oss;
    oss << "=== RESUMEN AUTOMATIZADO DEL INCIDENTE ===\n";
    oss << "Código de Error : " << (t.fields.error_code.empty() ? "(No detectado)" : t.fields.error_code) << "\n";
    oss << "Módulo / Comp.  : " << (t.fields.module_name.empty() ? "(No detectado)" : t.fields.module_name) << "\n";
    oss << "Versión         : " << (t.fields.software_version.empty() ? "(No detectada)" : t.fields.software_version) << "\n";
    oss << "URL / Endpoint  : " << (t.fields.sanitized_url.empty() ? "(No aplica / No visible)" : t.fields.sanitized_url) << "\n";
    oss << "Entorno / Idioma: " << t.fields.detected_os_hint << " (" << t.fields.detected_language << ")\n";
    oss << "Primer Contacto : " << (t.fields.actionable_without_followup
        ? "COMPLETO - Accionable sin solicitar más información al usuario"
        : "PARCIAL - Se sugiere confirmar campos faltantes") << "\n\n";

    oss << "=== MENSAJE EXTRAÍDO (SANITIZADO DLP) ===\n";
    oss << (t.fields.error_message.empty() ? t.dlp_summary.redacted_text : t.fields.error_message) << "\n\n";

    oss << "=== PROTECCIÓN DE DATOS SENSIBLES (DLP) ===\n";
    if (t.dlp_summary.total_redactions == 0) {
        oss << "0 datos sensibles expuestos detectados (Verificado).\n\n";
    } else {
        oss << t.dlp_summary.total_redactions << " elemento(s) sensible(s) redactado(s) automáticamente: ";
        for (size_t i = 0; i < t.dlp_summary.events.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << t.dlp_summary.events[i].rule_type << " (" << t.dlp_summary.events[i].count << ")";
        }
        oss << ".\n\n";
    }

    if (!t.suggested_articles.empty()) {
        oss << "=== ARTÍCULOS SUGERIDOS DE BASE DE CONOCIMIENTO (KB) ===\n";
        for (const auto& sug : t.suggested_articles) {
            oss << "• [" << sug.article.id << "] " << sug.article.title
                << " (Relevancia: " << static_cast<int>(sug.relevance_score * 100.0) << "%)\n";
            if (!sug.article.resolution_steps.empty()) {
                oss << "  Acción inicial: " << sug.article.resolution_steps.front() << "\n";
            }
        }
    }

    return oss.str();
}

} // namespace

TicketPipeline::TicketPipeline(PreprocessorOptions preproc_opts, std::shared_ptr<IOcrEngine> custom_ocr)
    : preproc_opts_(preproc_opts),
      ocr_engine_(custom_ocr ? std::move(custom_ocr) : std::make_shared<WindowsNativeOcrEngine>()) {}

TicketDraft TicketPipeline::process_raw_text(std::string_view raw_ocr_text, std::string_view source_label) const {
    const auto t0 = std::chrono::steady_clock::now();
    TicketDraft draft;
    draft.ticket_id = next_ticket_id();
    draft.source_image_path = std::string(source_label);

    const std::string normalized_ocr = WindowsNativeOcrEngine::post_correct_technical_ocr(raw_ocr_text);
    draft.ocr_info.raw_text = normalized_ocr;

    // Step 1: Mandatory DLP Zero-Leakage Redaction BEFORE any field extraction or classification
    draft.dlp_summary = dlp_.redact(normalized_ocr);

    // Step 2: Structured Entity Extraction on Redacted Text
    draft.fields = extractor_.extract(draft.dlp_summary.redacted_text);

    // Step 3: Multi-class Incident Classification
    const ClassificationResult cls = classifier_.classify(draft.fields, draft.dlp_summary.redacted_text);
    draft.category = cls.predicted_category;
    draft.category_confidence = cls.confidence;
    draft.priority = cls.suggested_priority;
    draft.support_queue = cls.assigned_queue;

    // Step 4: Knowledge Base Article Recommendations
    draft.suggested_articles = kb_.recommend(draft.fields, cls, draft.dlp_summary.redacted_text, 3);

    // Step 5: Pre-fill Ticket Title & Formatted Description
    draft.title = build_ticket_title(draft.fields, draft.category);
    draft.formatted_description = build_formatted_description(draft);

    const auto t1 = std::chrono::steady_clock::now();
    draft.total_processing_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    draft.estimated_agent_minutes =
        (draft.total_processing_ms / 60000.0) + (draft.fields.actionable_without_followup ? 0.32 : 0.85);
    return draft;
}

PipelineRunResult TicketPipeline::process_image(const ImageBuffer& image, std::string_view source_label) const {
    const auto t0 = std::chrono::steady_clock::now();

    PipelineRunResult result;
    result.original_image = image;

    ImagePreprocessor preprocessor(preproc_opts_);
    result.preprocessed = preprocessor.process(image);

    OcrResult ocr = ocr_engine_->recognize(result.preprocessed);
    result.ticket = process_raw_text(ocr.raw_text, source_label);
    result.ticket.image_diagnostics = result.preprocessed.diagnostics;
    result.ticket.ocr_info = std::move(ocr);
    // Do NOT store unredacted raw_text inside exported ocr_info to enforce strict Zero-Leakage!
    result.ticket.ocr_info.raw_text = result.ticket.dlp_summary.redacted_text;
    for (auto& line : result.ticket.ocr_info.lines) {
        line.text = dlp_.redact(line.text).redacted_text;
    }

    const auto t1 = std::chrono::steady_clock::now();
    result.ticket.total_processing_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    result.ticket.estimated_agent_minutes =
        (result.ticket.total_processing_ms / 60000.0) +
        (result.ticket.fields.actionable_without_followup ? 0.32 : 0.85);

    return result;
}

std::optional<PipelineRunResult> TicketPipeline::process_file(const std::filesystem::path& image_path, std::string* error_out) const {
    auto img = ImageIO::load_from_file(image_path, error_out);
    if (!img) return std::nullopt;
    return process_image(*img, wide_to_utf8(image_path.filename().wstring()));
}

JsonValue TicketPipeline::ticket_to_json(const TicketDraft& ticket) {
    JsonObject root;
    root["ticket_id"] = ticket.ticket_id;
    root["source_image"] = ticket.source_image_path;
    root["title"] = ticket.title;
    root["category"] = category_to_code(ticket.category);
    root["category_display"] = category_to_display_name(ticket.category);
    root["category_confidence"] = ticket.category_confidence;
    root["priority"] = priority_to_string(ticket.priority);
    root["support_queue"] = ticket.support_queue;

    JsonObject fields;
    fields["error_code"] = ticket.fields.error_code;
    fields["module"] = ticket.fields.module_name;
    fields["version"] = ticket.fields.software_version;
    fields["url"] = ticket.fields.sanitized_url;
    fields["error_message"] = ticket.fields.error_message;
    fields["os_hint"] = ticket.fields.detected_os_hint;
    fields["language"] = ticket.fields.detected_language;
    fields["actionable_without_followup"] = ticket.fields.actionable_without_followup;

    JsonArray missing;
    for (const auto& m : ticket.fields.missing_recommended_fields) missing.emplace_back(m);
    fields["missing_recommended_fields"] = std::move(missing);
    root["extracted_fields"] = std::move(fields);

    JsonObject dlp;
    dlp["total_redactions"] = ticket.dlp_summary.total_redactions;
    dlp["redacted_ocr_text"] = ticket.dlp_summary.redacted_text;
    JsonArray events;
    for (const auto& ev : ticket.dlp_summary.events) {
        JsonObject eo;
        eo["rule_type"] = ev.rule_type;
        eo["placeholder"] = ev.placeholder;
        eo["count"] = ev.count;
        events.emplace_back(std::move(eo));
    }
    dlp["events"] = std::move(events);
    root["dlp_protection"] = std::move(dlp);

    JsonObject diag;
    diag["original_width"] = ticket.image_diagnostics.original_width;
    diag["original_height"] = ticket.image_diagnostics.original_height;
    diag["is_dark_mode"] = ticket.image_diagnostics.is_dark_mode;
    diag["moire_detected"] = ticket.image_diagnostics.moire_detected;
    diag["moire_score"] = ticket.image_diagnostics.moire_score;
    diag["used_anti_moire_ocr_pass"] = ticket.ocr_info.used_anti_moire_pass;
    diag["total_processing_ms"] = ticket.total_processing_ms;
    diag["estimated_agent_minutes"] = ticket.estimated_agent_minutes;
    diag["manual_baseline_minutes"] = ticket.manual_baseline_minutes;
    root["diagnostics"] = std::move(diag);

    JsonArray kb_arr;
    for (const auto& sug : ticket.suggested_articles) {
        JsonObject ko;
        ko["id"] = sug.article.id;
        ko["title"] = sug.article.title;
        ko["relevance_score"] = sug.relevance_score;
        ko["match_reason"] = sug.match_reason;
        ko["root_cause"] = sug.article.root_cause;
        JsonArray steps;
        for (const auto& st : sug.article.resolution_steps) steps.emplace_back(st);
        ko["resolution_steps"] = std::move(steps);
        ko["reference_doc_url"] = sug.article.reference_doc_url;
        kb_arr.emplace_back(std::move(ko));
    }
    root["kb_suggestions"] = std::move(kb_arr);
    root["formatted_description"] = ticket.formatted_description;

    return root;
}

std::string TicketPipeline::ticket_to_json_string(const TicketDraft& ticket) {
    return serialize_json(ticket_to_json(ticket), 2);
}

std::string TicketPipeline::ticket_to_markdown(const TicketDraft& ticket) {
    std::ostringstream oss;
    oss << "# Ticket Pre-llenado: " << ticket.ticket_id << "\n\n";
    oss << "- **Título:** " << ticket.title << "\n";
    oss << "- **Categoría:** `" << category_to_code(ticket.category) << "` — "
        << category_to_display_name(ticket.category)
        << " (" << std::fixed << std::setprecision(1) << (ticket.category_confidence * 100.0) << "% confianza)\n";
    oss << "- **Prioridad Sugerida:** " << priority_to_string(ticket.priority) << "\n";
    oss << "- **Cola de Asignación:** `" << ticket.support_queue << "`\n";
    oss << "- **Resolución en Primer Contacto:** "
        << (ticket.fields.actionable_without_followup ? "Sí (Sin pedir más información)" : "Requiere información adicional") << "\n";
    oss << "- **Tiempo Estimado por Ticket:** " << std::setprecision(2) << ticket.estimated_agent_minutes
        << " min (vs " << ticket.manual_baseline_minutes << " min manual)\n\n";

    oss << "## Campos Técnicos Extraídos\n\n";
    oss << "| Campo | Valor Extraído |\n";
    oss << "|---|---|\n";
    oss << "| **Código de Error** | `" << (ticket.fields.error_code.empty() ? "N/A" : ticket.fields.error_code) << "` |\n";
    oss << "| **Módulo / Servicio** | `" << (ticket.fields.module_name.empty() ? "N/A" : ticket.fields.module_name) << "` |\n";
    oss << "| **Versión** | `" << (ticket.fields.software_version.empty() ? "N/A" : ticket.fields.software_version) << "` |\n";
    oss << "| **URL Sanitizada** | `" << (ticket.fields.sanitized_url.empty() ? "N/A" : ticket.fields.sanitized_url) << "` |\n";
    oss << "| **Mensaje del Error** | " << (ticket.fields.error_message.empty() ? "N/A" : ticket.fields.error_message) << " |\n\n";

    oss << "## Auditoría DLP (Protección de Datos Sensibles)\n\n";
    oss << "- **Total de Redacciones Aplicadas:** " << ticket.dlp_summary.total_redactions << "\n";
    for (const auto& ev : ticket.dlp_summary.events) {
        oss << "  - `" << ev.rule_type << "` -> `" << ev.placeholder << "` (" << ev.count << ")\n";
    }
    oss << "\n## Artículos Recomendados de Base de Conocimiento (KB)\n\n";
    for (const auto& sug : ticket.suggested_articles) {
        oss << "### [" << sug.article.id << "] " << sug.article.title
            << " (" << static_cast<int>(sug.relevance_score * 100.0) << "%)\n";
        oss << "- **Motivo de coincidencia:** " << sug.match_reason << "\n";
        oss << "- **Causa raíz probable:** " << sug.article.root_cause << "\n";
        oss << "- **Pasos sugeridos:**\n";
        for (size_t i = 0; i < sug.article.resolution_steps.size(); ++i) {
            oss << "  " << (i + 1) << ". " << sug.article.resolution_steps[i] << "\n";
        }
        oss << "\n";
    }
    return oss.str();
}

} // namespace ticket_ocr

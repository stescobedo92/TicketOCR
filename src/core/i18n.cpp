#include "ticket_ocr/i18n.hpp"
#include "ticket_ocr/json_utils.hpp"

#include <sstream>

namespace ticket_ocr {

I18n::I18n(Locale default_locale) : current_(default_locale) {
    init_embedded_defaults();
    const std::filesystem::path src_locales = std::filesystem::path(TICKET_OCR_SOURCE_DIR) / "locales";
    if (std::filesystem::exists(src_locales)) {
        load_locales_dir(src_locales);
    }
}

void I18n::init_embedded_defaults() {
    en_catalog_ = {
        {"app.window_title", "TicketOCR Studio C++20 — Intelligent Error Screenshot Reader & ITSM Ticket Generator"},
        {"menu.file", "&File"},
        {"menu.file.open", "&Open Screenshot..."},
        {"menu.file.export_json", "Export Ticket &JSON..."},
        {"menu.file.export_md", "Export Ticket &Markdown..."},
        {"menu.file.exit", "E&xit"},
        {"menu.view", "&View"},
        {"menu.view.toggle_ocr", "&Toggle Original / Preprocessed OCR View"},
        {"menu.view.theme_light", "&Light Theme"},
        {"menu.view.theme_dark", "&Dark Theme"},
        {"menu.language", "&Language"},
        {"menu.language.en", "&English (Default)"},
        {"menu.language.es", "E&spañol"},
        {"menu.tools", "&Tools"},
        {"menu.tools.copy", "&Copy Ticket to Clipboard"},
        {"menu.tools.benchmark", "Run Full &Corpus Benchmark..."},
        {"btn.open_image", "Load Screenshot..."},
        {"lbl.corpus", "Test Corpus:"},
        {"combo.corpus_placeholder", "-- Select anonymized corpus screenshot --"},
        {"btn.view_original", "View: Original Capture"},
        {"btn.view_preprocessed", "View: Preprocessed OCR"},
        {"chk.force_moire", "Force Anti-Moiré"},
        {"btn.reanalyze", "Re-analyze"},
        {"btn.benchmark", "Evaluate Corpus (Metrics)"},
        {"diag.initial", "Visual Diagnostics: Drag & drop a PNG/JPG screenshot here or pick a corpus sample above."},
        {"diag.summary", "Res: {0}x{1} | Dark Mode: {2} | Moiré: {3} ({4}) | Time: {5} ms"},
        {"diag.dark_yes", "Yes (Inverted)"},
        {"diag.dark_no", "No"},
        {"diag.moire_yes", "Detected/Filtered"},
        {"diag.moire_no", "Clean"},
        {"sec.ticket_header", "Pre-filled & DLP-Sanitized ITSM Ticket (Editable)"},
        {"lbl.title", "Ticket Title:"},
        {"lbl.category", "Incident Category:"},
        {"lbl.priority", "Priority:"},
        {"lbl.queue", "L2/L3 Queue:"},
        {"lbl.code", "Error Code:"},
        {"lbl.module", "Module / Service:"},
        {"lbl.version", "Version:"},
        {"lbl.url", "Sanitized URL:"},
        {"lbl.complete_pending", "First Contact: Awaiting image"},
        {"lbl.complete_yes", "First Contact: COMPLETE (No follow-up needed)"},
        {"lbl.complete_no", "First Contact: PARTIAL (Review missing fields)"},
        {"lbl.dlp_initial", "DLP Audit: Zero-leakage guard active"},
        {"lbl.dlp_status", "DLP Safe: 0 leaks ({0} redacted)"},
        {"lbl.description", "Structured Description & Redacted OCR Text:"},
        {"lbl.kb_header", "Recommended Knowledge Base (KB) Articles:"},
        {"col.kb_id", "KB ID"},
        {"col.kb_rel", "Relevance"},
        {"col.kb_title", "Article Title"},
        {"col.kb_reason", "Match Signal"},
        {"kb.placeholder", "Select a suggested KB article above to inspect root cause and resolution steps..."},
        {"kb.none", "No specific KB articles matched."},
        {"kb.root_cause", "[{0}] Root Cause: {1}"},
        {"btn.copy", "Copy Ticket (Clipboard)"},
        {"btn.export_json", "Export ITSM JSON..."},
        {"btn.export_md", "Export Markdown..."},
        {"canvas.empty_title", "Drag & drop an error screenshot here"},
        {"canvas.empty_sub", "Supports PNG, JPEG, BMP, Dark Mode, and Mobile Camera Photos with Moiré Effect.\nYou can also select any of the 18 Corpus scenarios from the top bar."},
        {"status.ready", "Ready — Load an error screenshot (PNG/JPG/BMP) or drag it into the window."},
        {"status.copied", "Markdown ticket copied to clipboard (DLP redaction verified)."},
        {"status.ticket_done", "Ticket {0} generated in {1} ms | Category: {2} ({3}%) | Est. agent time: {4} min (~93% saved)."},
        {"status.benchmark_prog", "Evaluating end-to-end corpus ({0}/{1}): {2}..."},
        {"status.error_load", "Failed to process image: {0}"},
        {"msg.export_ok_title", "Export Successful"},
        {"msg.export_json_ok", "Ticket exported successfully as JSON (verified 0 DLP leaks)."},
        {"msg.export_md_ok", "Ticket exported successfully as Markdown."},
        {"cat.AUTENTICACION_ACCESO", "Authentication & Access Control (IAM/SSO)"},
        {"cat.RED_CONECTIVIDAD", "Network, Connectivity & Upstream Services"},
        {"cat.BASE_DE_DATOS", "Database & Transactional Persistence"},
        {"cat.FACTURACION_PAGOS", "Electronic Invoicing & Payment Gateway"},
        {"cat.SISTEMA_MEMORIA_CRASH", "OS, Memory & Application Crash"},
        {"cat.CONFIGURACION_COMPATIBILIDAD", "Configuration, Parameters & Compatibility"},
        {"prio.P1", "P1 - Critical"},
        {"prio.P2", "P2 - High"},
        {"prio.P3", "P3 - Medium"},
        {"prio.P4", "P4 - Low"}
    };

    es_catalog_ = {
        {"app.window_title", "TicketOCR Studio C++20 — Lector Inteligente de Capturas de Error y Generador ITSM"},
        {"menu.file", "&Archivo"},
        {"menu.file.open", "&Abrir Captura..."},
        {"menu.file.export_json", "Exportar Ticket &JSON..."},
        {"menu.file.export_md", "Exportar Ticket &Markdown..."},
        {"menu.file.exit", "&Salir"},
        {"menu.view", "&Ver"},
        {"menu.view.toggle_ocr", "&Alternar Vista Original / Preprocesada OCR"},
        {"menu.view.theme_light", "Tema &Claro (Light)"},
        {"menu.view.theme_dark", "Tema &Oscuro (Dark)"},
        {"menu.language", "&Idioma"},
        {"menu.language.en", "&English (Default)"},
        {"menu.language.es", "E&spañol"},
        {"menu.tools", "&Herramientas"},
        {"menu.tools.copy", "&Copiar Ticket al Portapapeles"},
        {"menu.tools.benchmark", "Evaluar &Corpus Completo (Métricas)..."},
        {"btn.open_image", "Cargar Captura..."},
        {"lbl.corpus", "Corpus de Prueba:"},
        {"combo.corpus_placeholder", "-- Seleccionar captura del corpus anonimizado --"},
        {"btn.view_original", "Vista: Captura Original"},
        {"btn.view_preprocessed", "Vista: Preprocesada OCR"},
        {"chk.force_moire", "Forzar Anti-Moiré"},
        {"btn.reanalyze", "Re-analizar"},
        {"btn.benchmark", "Evaluar Corpus (Métricas)"},
        {"diag.initial", "Diagnóstico Visual: Arrastre una captura PNG/JPG aquí o seleccione un caso del corpus superior."},
        {"diag.summary", "Res: {0}x{1} | Modo Oscuro: {2} | Moiré: {3} ({4}) | Tiempo: {5} ms"},
        {"diag.dark_yes", "Sí (Invertido)"},
        {"diag.dark_no", "No"},
        {"diag.moire_yes", "Detectado/Filtrado"},
        {"diag.moire_no", "Limpio"},
        {"sec.ticket_header", "Ticket ITSM Pre-llenado y Sanitizado (Editable)"},
        {"lbl.title", "Título del Ticket:"},
        {"lbl.category", "Categoría Incidente:"},
        {"lbl.priority", "Prioridad:"},
        {"lbl.queue", "Cola N2/N3:"},
        {"lbl.code", "Código de Error:"},
        {"lbl.module", "Módulo / Servicio:"},
        {"lbl.version", "Versión:"},
        {"lbl.url", "URL Sanitizada:"},
        {"lbl.complete_pending", "Primer Contacto: Pendiente de carga"},
        {"lbl.complete_yes", "Primer Contacto: COMPLETO (Sin pedir +info)"},
        {"lbl.complete_no", "Primer Contacto: PARCIAL (Revisar campos)"},
        {"lbl.dlp_initial", "Auditoría DLP: Meta 0 fugas activa"},
        {"lbl.dlp_status", "DLP Seguro: 0 fugas ({0} redactados)"},
        {"lbl.description", "Descripción Estructurada y Texto Redactado:"},
        {"lbl.kb_header", "Artículos Sugeridos de Base de Conocimiento (KB):"},
        {"col.kb_id", "ID KB"},
        {"col.kb_rel", "Relevancia"},
        {"col.kb_title", "Título del Artículo"},
        {"col.kb_reason", "Motivo Coincidencia"},
        {"kb.placeholder", "Seleccione un artículo KB sugerido para inspeccionar causa raíz y pasos de resolución..."},
        {"kb.none", "No se encontraron artículos KB específicos."},
        {"kb.root_cause", "[{0}] Causa Raíz: {1}"},
        {"btn.copy", "Copiar Ticket (Portapapeles)"},
        {"btn.export_json", "Exportar JSON ITSM..."},
        {"btn.export_md", "Exportar Markdown..."},
        {"canvas.empty_title", "Arrastre y suelte aquí una captura de pantalla de error"},
        {"canvas.empty_sub", "Soporta PNG, JPEG, BMP, Modo Oscuro y Fotos de Celular con Efecto Moiré.\nTambién puede elegir cualquiera de los 18 escenarios del Corpus arriba."},
        {"status.ready", "Listo — Cargue una captura de error (PNG/JPG/BMP) o arrástrela a la ventana."},
        {"status.copied", "Ticket Markdown copiado al portapapeles (con redacción DLP verificada)."},
        {"status.ticket_done", "Ticket {0} generado en {1} ms | Categoría: {2} ({3}%) | Tiempo estimado: {4} min (ahorro ~93%)."},
        {"status.benchmark_prog", "Evaluando corpus end-to-end ({0}/{1}): {2}..."},
        {"status.error_load", "Error al procesar imagen: {0}"},
        {"msg.export_ok_title", "Exportación Exitosa"},
        {"msg.export_json_ok", "Ticket exportado correctamente en formato JSON (sanitizado con 0 fugas DLP)."},
        {"msg.export_md_ok", "Ticket exportado correctamente en formato Markdown."},
        {"cat.AUTENTICACION_ACCESO", "Autenticación y Control de Acceso (IAM/SSO)"},
        {"cat.RED_CONECTIVIDAD", "Red, Conectividad y Servicios Upstream"},
        {"cat.BASE_DE_DATOS", "Base de Datos y Persistencia Transaccional"},
        {"cat.FACTURACION_PAGOS", "Facturación Electrónica y Pasarela de Pagos"},
        {"cat.SISTEMA_MEMORIA_CRASH", "Sistema Operativo, Memoria y Crash de Aplicación"},
        {"cat.CONFIGURACION_COMPATIBILIDAD", "Configuración, Parámetros y Compatibilidad"},
        {"prio.P1", "P1 - Crítica"},
        {"prio.P2", "P2 - Alta"},
        {"prio.P3", "P3 - Media"},
        {"prio.P4", "P4 - Baja"}
    };
}

bool I18n::load_single_json(const std::filesystem::path& file_path, std::map<std::string, std::string>& target) {
    const std::string raw = read_text_file(file_path);
    if (raw.empty()) return false;
    const JsonValue root = parse_json(raw);
    const JsonValue* strings_node = root.find("strings");
    if (!strings_node || !strings_node->is_object()) return false;

    for (const auto& [k, v] : strings_node->as_object()) {
        if (v.is_string()) {
            target[k] = v.as_string();
        }
    }
    return true;
}

bool I18n::load_locales_dir(const std::filesystem::path& locales_dir) {
    bool ok_en = load_single_json(locales_dir / "en.json", en_catalog_);
    bool ok_es = load_single_json(locales_dir / "es.json", es_catalog_);
    return ok_en || ok_es;
}

std::string I18n::tr(std::string_view key) const {
    const std::string k(key);
    const auto& active = (current_ == Locale::Spanish) ? es_catalog_ : en_catalog_;
    if (auto it = active.find(k); it != active.end()) {
        return it->second;
    }
    if (auto it_en = en_catalog_.find(k); it_en != en_catalog_.end()) {
        return it_en->second;
    }
    return k;
}

std::string I18n::trf(std::string_view key, std::initializer_list<std::string_view> args) const {
    std::string out = tr(key);
    size_t idx = 0;
    for (std::string_view arg : args) {
        const std::string token = "{" + std::to_string(idx) + "}";
        size_t pos = 0;
        while ((pos = out.find(token, pos)) != std::string::npos) {
            out.replace(pos, token.size(), arg);
            pos += arg.size();
        }
        ++idx;
    }
    return out;
}

std::string I18n::category_name(IncidentCategory cat) const {
    return tr("cat." + category_to_code(cat));
}

std::string I18n::priority_name(TicketPriority prio) const {
    switch (prio) {
        case TicketPriority::P1_Critica: return tr("prio.P1");
        case TicketPriority::P2_Alta:    return tr("prio.P2");
        case TicketPriority::P3_Media:   return tr("prio.P3");
        case TicketPriority::P4_Baja:    return tr("prio.P4");
        default:                         return tr("prio.P3");
    }
}

std::string I18n::format_ticket_description(const TicketDraft& t, Locale loc) {
    std::ostringstream oss;
    if (loc == Locale::English) {
        oss << "=== AUTOMATED INCIDENT SUMMARY ===\n";
        oss << "Error Code      : " << (t.fields.error_code.empty() ? "(Not detected)" : t.fields.error_code) << "\n";
        oss << "Module / Service: " << (t.fields.module_name.empty() ? "(Not detected)" : t.fields.module_name) << "\n";
        oss << "Version         : " << (t.fields.software_version.empty() ? "(Not detected)" : t.fields.software_version) << "\n";
        oss << "Sanitized URL   : " << (t.fields.sanitized_url.empty() ? "(N/A or Not visible)" : t.fields.sanitized_url) << "\n";
        oss << "OS / Language   : " << t.fields.detected_os_hint << " (" << t.fields.detected_language << ")\n";
        oss << "First Contact   : " << (t.fields.actionable_without_followup
            ? "COMPLETE - Actionable without requesting extra user info"
            : "PARTIAL - Follow-up recommended for missing fields") << "\n\n";

        oss << "=== EXTRACTED MESSAGE (DLP SANITIZED) ===\n";
        oss << (t.fields.error_message.empty() ? t.dlp_summary.redacted_text : t.fields.error_message) << "\n\n";

        oss << "=== SENSITIVE DATA PROTECTION (DLP) ===\n";
        if (t.dlp_summary.total_redactions == 0) {
            oss << "0 exposed sensitive items detected (Verified).\n\n";
        } else {
            oss << t.dlp_summary.total_redactions << " sensitive item(s) automatically redacted: ";
            for (size_t i = 0; i < t.dlp_summary.events.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << t.dlp_summary.events[i].rule_type << " (" << t.dlp_summary.events[i].count << ")";
            }
            oss << ".\n\n";
        }

        if (!t.suggested_articles.empty()) {
            oss << "=== SUGGESTED KNOWLEDGE BASE (KB) ARTICLES ===\n";
            for (const auto& sug : t.suggested_articles) {
                oss << "• [" << sug.article.id << "] " << sug.article.title
                    << " (Relevance: " << static_cast<int>(sug.relevance_score * 100.0) << "%)\n";
                if (!sug.article.resolution_steps.empty()) {
                    oss << "  Initial action: " << sug.article.resolution_steps.front() << "\n";
                }
            }
        }
    } else {
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
    }
    return oss.str();
}

} // namespace ticket_ocr

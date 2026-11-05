#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ticket_ocr {

enum class IncidentCategory : int {
    AutenticacionAcceso = 0,
    RedConectividad = 1,
    BaseDeDatos = 2,
    FacturacionPagos = 3,
    SistemaMemoriaCrash = 4,
    ConfiguracionCompatibilidad = 5,
    Count = 6
};

std::string category_to_code(IncidentCategory cat);
std::string category_to_display_name(IncidentCategory cat);
std::string category_to_support_queue(IncidentCategory cat);
std::optional<IncidentCategory> category_from_code(std::string_view code);

enum class TicketPriority {
    P1_Critica,
    P2_Alta,
    P3_Media,
    P4_Baja
};

std::string priority_to_string(TicketPriority p);

// Contiguous 32-bit BGRA image buffer in row-major order (cache-friendly)
struct ImageBuffer {
    int width = 0;
    int height = 0;
    int stride = 0; // width * 4
    std::vector<uint8_t> pixels; // B, G, R, A per pixel

    bool empty() const noexcept { return width <= 0 || height <= 0 || pixels.empty(); }

    static ImageBuffer create(int w, int h, uint8_t b = 255, uint8_t g = 255, uint8_t r = 255, uint8_t a = 255) {
        ImageBuffer img;
        img.width = w;
        img.height = h;
        img.stride = w * 4;
        img.pixels.resize(static_cast<size_t>(img.stride) * static_cast<size_t>(h));
        for (size_t i = 0; i < img.pixels.size(); i += 4) {
            img.pixels[i + 0] = b;
            img.pixels[i + 1] = g;
            img.pixels[i + 2] = r;
            img.pixels[i + 3] = a;
        }
        return img;
    }

    uint8_t* row_ptr(int y) noexcept {
        return pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(stride);
    }

    const uint8_t* row_ptr(int y) const noexcept {
        return pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(stride);
    }
};

struct PreprocessDiagnostics {
    int original_width = 0;
    int original_height = 0;
    int processed_width = 0;
    int processed_height = 0;
    bool is_dark_mode = false;
    bool moire_detected = false;
    bool upscaled = false;
    double mean_luminance = 0.0;
    double moire_score = 0.0;
    double preprocess_ms = 0.0;
};

struct PreprocessOutput {
    ImageBuffer primary_image;      // Contrast-enhanced / dark-mode-inverted / upscaled
    ImageBuffer anti_moire_image;   // Integral-image illumination flattened + anti-moire filtered
    PreprocessDiagnostics diagnostics;
};

struct OcrLineBox {
    std::string text;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct OcrResult {
    std::string raw_text;
    std::vector<OcrLineBox> lines;
    std::string engine_language;
    bool used_anti_moire_pass = false;
    double ocr_ms = 0.0;
};

struct RedactionEvent {
    std::string rule_type;        // EMAIL, JWT_TOKEN, SECRET_TOKEN, URL_SECRET, CARD_PAN, etc.
    std::string placeholder;      // e.g. [REDACTED:EMAIL]
    size_t count = 0;
};

struct RedactionResult {
    std::string redacted_text;
    size_t total_redactions = 0;
    std::vector<RedactionEvent> events;
};

struct ExtractedFields {
    std::string error_code;
    std::string module_name;
    std::string software_version;
    std::string sanitized_url;
    std::string error_message;
    std::string detected_os_hint;
    std::string detected_language; // "ES", "EN", "MIXED"
    bool actionable_without_followup = false;
    std::vector<std::string> missing_recommended_fields;
};

struct CategoryScore {
    IncidentCategory category = IncidentCategory::ConfiguracionCompatibilidad;
    double probability = 0.0;
};

struct ClassificationResult {
    IncidentCategory predicted_category = IncidentCategory::ConfiguracionCompatibilidad;
    double confidence = 0.0;
    TicketPriority suggested_priority = TicketPriority::P3_Media;
    std::string assigned_queue;
    std::vector<CategoryScore> ranking;
    std::vector<std::string> matched_signals;
};

struct KbArticle {
    std::string id;               // e.g. "KB-1001"
    std::string title;
    IncidentCategory category = IncidentCategory::ConfiguracionCompatibilidad;
    std::vector<std::string> error_codes;
    std::vector<std::string> keywords;
    std::string root_cause;
    std::vector<std::string> resolution_steps;
    std::string reference_doc_url;
};

struct KbSuggestion {
    KbArticle article;
    double relevance_score = 0.0; // 0.0 .. 1.0
    std::string match_reason;
};

struct TicketDraft {
    std::string ticket_id;
    std::string source_image_path;
    std::string title;
    IncidentCategory category = IncidentCategory::ConfiguracionCompatibilidad;
    double category_confidence = 0.0;
    TicketPriority priority = TicketPriority::P3_Media;
    std::string support_queue;

    ExtractedFields fields;
    RedactionResult dlp_summary;
    PreprocessDiagnostics image_diagnostics;
    OcrResult ocr_info;
    std::vector<KbSuggestion> suggested_articles;

    std::string formatted_description;
    double total_processing_ms = 0.0;
    double estimated_agent_minutes = 0.35; // Processing + quick human review
    double manual_baseline_minutes = 5.20; // Industry/support desk baseline
};

// Ground truth sample for corpus generation and automated/human benchmark evaluation
struct CorpusSample {
    std::string sample_id;
    std::string relative_image_path;
    std::string os_style;         // "Windows11", "macOS", "Linux", "WebSaaS"
    std::string language;         // "ES", "EN"
    int width = 1280;
    int height = 720;
    bool dark_mode = false;
    bool mobile_moire_photo = false;

    // Ground truth key fields (expected post-redaction values)
    std::string gt_error_code;
    std::string gt_module;
    std::string gt_version;
    std::string gt_sanitized_url;
    std::string gt_error_message;
    IncidentCategory gt_category = IncidentCategory::ConfiguracionCompatibilidad;
    bool gt_has_complete_info = true;

    // Raw text rendered into the screenshot (may include sensitive tokens/emails to test DLP!)
    std::string rendered_window_title;
    std::string rendered_headline;
    std::string rendered_body_line1;
    std::string rendered_body_line2;
    std::string rendered_footer;
    std::string rendered_url_bar;

    // Explicit secrets planted in the screenshot to verify strict 0-leakage!
    std::vector<std::string> planted_secrets;
};

struct SampleEvaluation {
    std::string sample_id;
    bool dark_mode = false;
    bool mobile_moire = false;
    std::string language;
    std::string os_style;

    double cer_error_code = 0.0;
    double cer_module = 0.0;
    double cer_version = 0.0;
    double cer_url = 0.0;
    double cer_message = 0.0;
    double cer_key_fields_mean = 0.0;

    IncidentCategory expected_category = IncidentCategory::ConfiguracionCompatibilidad;
    IncidentCategory predicted_category = IncidentCategory::ConfiguracionCompatibilidad;
    bool category_correct = false;

    bool actionable_without_followup = false;
    size_t redactions_performed = 0;
    size_t sensitive_leaks_detected = 0;
    std::vector<std::string> leaked_items;
    double processing_ms = 0.0;

    TicketDraft generated_ticket;
};

struct ClassMetrics {
    IncidentCategory category = IncidentCategory::ConfiguracionCompatibilidad;
    int true_positives = 0;
    int false_positives = 0;
    int false_negatives = 0;
    double precision = 0.0;
    double recall = 0.0;
    double f1 = 0.0;
};

struct BenchmarkReport {
    size_t total_samples = 0;

    // CER metrics (0.0 = perfect)
    double mean_cer_error_code = 0.0;
    double mean_cer_module = 0.0;
    double mean_cer_version = 0.0;
    double mean_cer_url = 0.0;
    double mean_cer_message = 0.0;
    double overall_key_fields_cer = 0.0;
    double moire_subset_cer = 0.0;
    double dark_mode_subset_cer = 0.0;

    // Classification metrics against support team labels
    double accuracy = 0.0;
    double macro_f1 = 0.0;
    std::vector<ClassMetrics> per_class_metrics;
    int confusion_matrix[6][6]{};

    // Operational metrics
    double avg_processing_ms = 0.0;
    double manual_baseline_minutes_per_ticket = 5.20;
    double assisted_minutes_per_ticket = 0.0;
    double time_saved_percent = 0.0;
    double pct_tickets_without_requesting_more_info = 0.0;

    // Security / DLP metric (Target: 0)
    size_t total_planted_secrets = 0;
    size_t total_redactions_applied = 0;
    size_t total_sensitive_leaks = 0;

    bool all_quality_gates_passed = false;
    std::vector<SampleEvaluation> sample_results;
};

// UTF-8 <-> Wide string helpers (Windows native)
std::wstring utf8_to_wide(std::string_view utf8_str);
std::string wide_to_utf8(std::wstring_view wide_str);
std::string trim_copy(std::string_view sv);
std::string to_lower_ascii(std::string_view sv);
std::string fold_spanish_accents_to_ascii(std::string_view utf8_str);

} // namespace ticket_ocr

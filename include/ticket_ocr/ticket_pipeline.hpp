#pragma once

#include "ticket_ocr/dlp_redactor.hpp"
#include "ticket_ocr/entity_extractor.hpp"
#include "ticket_ocr/image_preprocessor.hpp"
#include "ticket_ocr/incident_classifier.hpp"
#include "ticket_ocr/json_utils.hpp"
#include "ticket_ocr/kb_recommender.hpp"
#include "ticket_ocr/ocr_engine.hpp"
#include "ticket_ocr/types.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace ticket_ocr {

struct PipelineRunResult {
    TicketDraft ticket;
    ImageBuffer original_image;
    PreprocessOutput preprocessed;
};

class TicketPipeline {
public:
    explicit TicketPipeline(PreprocessorOptions preproc_opts = {}, std::shared_ptr<IOcrEngine> custom_ocr = nullptr);

    // End-to-end execution from an image file on disk
    std::optional<PipelineRunResult> process_file(const std::filesystem::path& image_path, std::string* error_out = nullptr) const;

    // End-to-end execution from an in-memory BGRA image buffer
    PipelineRunResult process_image(const ImageBuffer& image, std::string_view source_label = "memory_buffer") const;

    // End-to-end execution directly from raw text (useful for isolated NLP/DLP pipeline testing)
    TicketDraft process_raw_text(std::string_view raw_ocr_text, std::string_view source_label = "text_input") const;

    // Serialization for ITSM systems (Jira / ServiceNow / Zendesk JSON & human-friendly Markdown)
    static JsonValue ticket_to_json(const TicketDraft& ticket);
    static std::string ticket_to_json_string(const TicketDraft& ticket);
    static std::string ticket_to_markdown(const TicketDraft& ticket);

    KbRecommender& kb_recommender() noexcept { return kb_; }
    const KbRecommender& kb_recommender() const noexcept { return kb_; }

private:
    PreprocessorOptions preproc_opts_;
    std::shared_ptr<IOcrEngine> ocr_engine_;
    DlpRedactor dlp_;
    EntityExtractor extractor_;
    IncidentClassifier classifier_;
    KbRecommender kb_;
};

} // namespace ticket_ocr

#pragma once

#include "ticket_ocr/types.hpp"
#include <filesystem>
#include <string_view>
#include <vector>

namespace ticket_ocr {

class KbRecommender {
public:
    KbRecommender();

    // Load additional or replacement KB articles from JSON file
    bool load_from_json_file(const std::filesystem::path& json_path);

    // Export built-in catalog to JSON file
    bool save_to_json_file(const std::filesystem::path& json_path) const;

    // Recommend Top-K KB articles ranked by code match, category alignment, and lexical BM25-like overlap
    std::vector<KbSuggestion> recommend(
        const ExtractedFields& fields,
        const ClassificationResult& classification,
        std::string_view redacted_text,
        size_t top_k = 3
    ) const;

    const std::vector<KbArticle>& articles() const noexcept { return articles_; }

private:
    std::vector<KbArticle> articles_;
    void load_builtin_catalog();
};

} // namespace ticket_ocr

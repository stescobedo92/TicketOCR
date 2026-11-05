#pragma once

#include "ticket_ocr/types.hpp"
#include <string_view>

namespace ticket_ocr {

class EntityExtractor {
public:
    EntityExtractor() = default;

    // Extracts error_code, module_name, software_version, sanitized_url, error_message, OS/language hints
    ExtractedFields extract(std::string_view redacted_ocr_text) const;
};

} // namespace ticket_ocr

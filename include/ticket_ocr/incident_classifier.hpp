#pragma once

#include "ticket_ocr/types.hpp"
#include <string_view>

namespace ticket_ocr {

class IncidentClassifier {
public:
    IncidentClassifier() = default;

    // Classifies incident into one of the 6 support categories using weighted lexical + structural signals
    ClassificationResult classify(const ExtractedFields& fields, std::string_view full_redacted_text) const;
};

} // namespace ticket_ocr

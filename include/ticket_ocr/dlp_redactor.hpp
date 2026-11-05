#pragma once

#include "ticket_ocr/types.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace ticket_ocr {

class DlpRedactor {
public:
    DlpRedactor() = default;

    // Redacts all sensitive PII, credentials, JWTs, API keys, URL query tokens, and payment cards
    RedactionResult redact(std::string_view input) const;

    // Audits any text/JSON/Markdown output to verify 0 sensitive items survive
    // Checks both known planted secrets (including partial secret cores) and pattern-based PII detectors
    static std::vector<std::string> audit_for_leaks(
        std::string_view output_payload,
        const std::vector<std::string>& known_secrets = {}
    );

    static bool passes_luhn_check(std::string_view digits_only) noexcept;
    static double compute_shannon_entropy(std::string_view token) noexcept;
};

} // namespace ticket_ocr

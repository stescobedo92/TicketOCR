#include "ticket_ocr/dlp_redactor.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <functional>
#include <map>
#include <regex>
#include <sstream>

namespace ticket_ocr {

bool DlpRedactor::passes_luhn_check(std::string_view digits_only) noexcept {
    if (digits_only.size() < 13 || digits_only.size() > 19) return false;
    int sum = 0;
    bool alternate = false;
    bool all_same = true;
    for (size_t i = digits_only.size(); i > 0; --i) {
        const char c = digits_only[i - 1];
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        if (c != digits_only[0]) all_same = false;
        int n = c - '0';
        if (alternate) {
            n *= 2;
            if (n > 9) n -= 9;
        }
        sum += n;
        alternate = !alternate;
    }
    return !all_same && (sum % 10 == 0);
}

double DlpRedactor::compute_shannon_entropy(std::string_view token) noexcept {
    if (token.empty()) return 0.0;
    std::array<uint32_t, 256> freq{};
    for (unsigned char c : token) {
        freq[c]++;
    }
    const double inv_len = 1.0 / static_cast<double>(token.size());
    double entropy = 0.0;
    for (uint32_t count : freq) {
        if (count > 0) {
            const double p = static_cast<double>(count) * inv_len;
            entropy -= p * std::log2(p);
        }
    }
    return entropy;
}

namespace {

void record_event(std::map<std::string, std::pair<std::string, size_t>>& ledger,
                  const std::string& rule_type,
                  const std::string& placeholder,
                  size_t count) {
    if (count == 0) return;
    auto& entry = ledger[rule_type];
    entry.first = placeholder;
    entry.second += count;
}

std::string regex_replace_counted(
    const std::string& input,
    const std::regex& pattern,
    const std::function<std::string(const std::smatch&)>& replacer,
    size_t* match_count_out) {
    std::string out;
    out.reserve(input.size());
    size_t last = 0;
    size_t count = 0;
    for (std::sregex_iterator it(input.begin(), input.end(), pattern), end; it != end; ++it) {
        const auto& m = *it;
        out.append(input, last, static_cast<size_t>(m.position()) - last);
        out += replacer(m);
        last = static_cast<size_t>(m.position() + m.length());
        ++count;
    }
    out.append(input, last, std::string::npos);
    if (match_count_out) *match_count_out = count;
    return out;
}

bool is_safe_technical_identifier(std::string_view tok) {
    if (tok.find("[REDACTED:") != std::string::npos) return true;
    const std::string low = to_lower_ascii(tok);
    if (low.starts_with("http://") || low.starts_with("https://") || low.starts_with("/")) return true;
    if (low.starts_with("err_") || low.starts_with("err-") || low.starts_with("sqlstate")) return true;
    // Check if PascalCase/dot-separated namespace without random digit interleaving
    bool has_dot = tok.find('.') != std::string::npos;
    size_t digit_count = 0;
    for (unsigned char c : tok) {
        if (std::isdigit(c)) ++digit_count;
    }
    if (has_dot && digit_count <= 2) return true;
    return false;
}

} // namespace

RedactionResult DlpRedactor::redact(std::string_view input) const {
    RedactionResult res;
    std::string text(input);
    if (text.empty()) return res;

    std::map<std::string, std::pair<std::string, size_t>> ledger;

    // 1. URL embedded Basic-Auth credentials: https://user:password@host
    {
        static const std::regex re_url_creds(R"(\b(https?://)([^/\s:@]+:[^/\s@]+)@)");
        size_t cnt = 0;
        text = regex_replace_counted(text, re_url_creds, [](const std::smatch& m) {
            return m[1].str() + "[REDACTED:URL_CREDENTIALS]@";
        }, &cnt);
        record_event(ledger, "URL_CREDENTIALS", "[REDACTED:URL_CREDENTIALS]", cnt);
    }

    // 2. URL query/hash secret parameters (?api_key=..., &token=..., &access_token=..., &client_secret=...)
    {
        static const std::regex re_url_param(
            R"(([?&;#])\s*(token|access_token|refresh_token|id_token|api_key|apikey|secret|client_secret|password|passwd|pwd|auth|auth_token|key|session|session_id|sid|jwt|signature|sig)\s*=\s*([^&#\s]+))",
            std::regex::icase
        );
        size_t cnt = 0;
        text = regex_replace_counted(text, re_url_param, [](const std::smatch& m) {
            return m[1].str() + m[2].str() + "=[REDACTED:URL_SECRET]";
        }, &cnt);
        record_event(ledger, "URL_SECRET", "[REDACTED:URL_SECRET]", cnt);
    }

    // 3. JWT Tokens (eyJ... with tolerance for minor OCR whitespace or non-labeled line wraps)
    {
        static const std::regex re_jwt(
            R"(\beyJ[A-Za-z0-9_-]{6,}(?:[ \t]*\.[ \t]*[A-Za-z0-9_-]{4,})*(?:\r?\n(?![A-Za-zÁÉÍÓÚáéíóúñÑ]{3,15}\s*:)[A-Za-z0-9_.-]{8,})?)"
        );
        size_t cnt = 0;
        text = regex_replace_counted(text, re_jwt, [](const std::smatch&) {
            return "[REDACTED:JWT_TOKEN]";
        }, &cnt);
        record_event(ledger, "JWT_TOKEN", "[REDACTED:JWT_TOKEN]", cnt);
    }

    // 4. Prefixed API Keys & Cloud/SaaS Tokens (OpenAI/Stripe sk-, GitHub ghp_, AWS AKIA, Google AIza, Slack xoxb-, tok_prod_, sec_)
    {
        static const std::regex re_api_keys(
            R"(\b(?:sk-(?:live-|test-|proj-)?[A-Za-z0-9_-]{10,}|(?:sk|pk|rk)_(?:live|test)_[A-Za-z0-9]{10,}|(?:ghp|gho|ghu|ghs|ghr)_[A-Za-z0-9]{10,}|github_pat_[A-Za-z0-9_]{12,}|[A4]K[I1l]A[A-Z0-9]{10,}|AIza[A-Za-z0-9_-]{16,}|xox[baprs]-[A-Za-z0-9-]{10,}|glpat-[A-Za-z0-9_-]{10,}|(?:tok|sec|key)_(?:prod|live|test|dev)?_?[A-Za-z0-9_-]{8,})\b)"
        );
        size_t cnt = 0;
        text = regex_replace_counted(text, re_api_keys, [](const std::smatch&) {
            return "[REDACTED:API_KEY]";
        }, &cnt);
        record_event(ledger, "API_KEY", "[REDACTED:API_KEY]", cnt);
    }

    // 5. Header / Labeled Secrets (Bearer ..., Authorization: ..., Token: ..., API-Key: ..., Password: ..., SessionID: ...)
    {
        static const std::regex re_labeled_secret(
            R"(\b(Bearer|Authorization\s*:?\s*Bearer|Access[-_ ]?Token\s*:|Refresh[-_ ]?Token\s*:|API[-_ ]?Key\s*:|x-api-key\s*:|Client[-_ ]?Secret\s*:|Secret\s*:|Password\s*:|Contrase(?:ñ|n)a\s*:|Session[-_ ]?ID\s*:)\s*(?!\[REDACTED:)([A-Za-z0-9._~+/=-]{6,}))",
            std::regex::icase
        );
        size_t cnt = 0;
        text = regex_replace_counted(text, re_labeled_secret, [](const std::smatch& m) {
            return m[1].str() + " [REDACTED:SECRET_TOKEN]";
        }, &cnt);
        record_event(ledger, "SECRET_TOKEN", "[REDACTED:SECRET_TOKEN]", cnt);
    }

    // 6. Email Addresses (including OCR-induced spaces adjacent to '@')
    {
        static const std::regex re_email(
            R"([A-Za-z0-9._%+-]+\s*@\s*[A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)*\.[A-Za-z]{2,})"
        );
        size_t cnt = 0;
        text = regex_replace_counted(text, re_email, [](const std::smatch&) {
            return "[REDACTED:EMAIL]";
        }, &cnt);
        record_event(ledger, "EMAIL", "[REDACTED:EMAIL]", cnt);
    }

    // 7. Payment Card PANs (13-19 digits with spaces/hyphens validated with Luhn)
    {
        static const std::regex re_pan_candidate(R"(\b(?:\d[ -]*?){13,19}\b)");
        std::string out;
        out.reserve(text.size());
        size_t last = 0;
        size_t cnt = 0;
        for (std::sregex_iterator it(text.begin(), text.end(), re_pan_candidate), end; it != end; ++it) {
            const auto& m = *it;
            std::string raw = m.str();
            std::string digits;
            for (char c : raw) {
                if (std::isdigit(static_cast<unsigned char>(c))) digits.push_back(c);
            }
            out.append(text, last, static_cast<size_t>(m.position()) - last);
            if (passes_luhn_check(digits)) {
                out += "[REDACTED:CARD_PAN]";
                ++cnt;
            } else {
                out += raw;
            }
            last = static_cast<size_t>(m.position() + m.length());
        }
        out.append(text, last, std::string::npos);
        text = std::move(out);
        record_event(ledger, "CARD_PAN", "[REDACTED:CARD_PAN]", cnt);
    }

    // 8. High-entropy standalone tokens (>= 26 chars, mixed upper/lower/digits, entropy >= 3.65)
    {
        static const std::regex re_long_tok(R"(\b[A-Za-z0-9_+/=-]{26,}\b)");
        std::string out;
        out.reserve(text.size());
        size_t last = 0;
        size_t cnt = 0;
        for (std::sregex_iterator it(text.begin(), text.end(), re_long_tok), end; it != end; ++it) {
            const auto& m = *it;
            const size_t pos = static_cast<size_t>(m.position());
            // Skip if inside [REDACTED:...] or part of a URL path
            const bool preceded_by_url_or_tag =
                (pos > 0 && (text[pos - 1] == ':' || text[pos - 1] == '/')) ||
                (pos >= 10 && text.substr(pos - 10, 10).find("[REDACTED") != std::string::npos);

            const std::string tok = m.str();
            bool has_upper = false, has_lower = false, has_digit = false;
            for (unsigned char c : tok) {
                if (std::isupper(c)) has_upper = true;
                else if (std::islower(c)) has_lower = true;
                else if (std::isdigit(c)) has_digit = true;
            }
            const int classes = (has_upper ? 1 : 0) + (has_lower ? 1 : 0) + (has_digit ? 1 : 0);

            out.append(text, last, pos - last);
            if (!preceded_by_url_or_tag && !is_safe_technical_identifier(tok) &&
                classes >= 3 && compute_shannon_entropy(tok) >= 3.65) {
                out += "[REDACTED:HIGH_ENTROPY_SECRET]";
                ++cnt;
            } else {
                out += tok;
            }
            last = pos + static_cast<size_t>(m.length());
        }
        out.append(text, last, std::string::npos);
        text = std::move(out);
        record_event(ledger, "HIGH_ENTROPY_SECRET", "[REDACTED:HIGH_ENTROPY_SECRET]", cnt);
    }

    res.redacted_text = std::move(text);
    for (const auto& [rule, pair_val] : ledger) {
        res.events.push_back(RedactionEvent{rule, pair_val.first, pair_val.second});
        res.total_redactions += pair_val.second;
    }
    return res;
}

std::vector<std::string> DlpRedactor::audit_for_leaks(
    std::string_view output_payload,
    const std::vector<std::string>& known_secrets) {
    std::vector<std::string> leaks;
    if (output_payload.empty()) return leaks;

    const std::string low_payload = to_lower_ascii(output_payload);

    // Layer A: Check against ground-truth planted secrets (and distinctive core fragments)
    for (const std::string& secret : known_secrets) {
        if (secret.empty()) continue;
        const std::string low_sec = to_lower_ascii(secret);
        if (low_payload.find(low_sec) != std::string::npos) {
            leaks.push_back("EXACT_SECRET_LEAK:" + secret);
            continue;
        }
        // Check email local-part + '@'
        const auto at_pos = low_sec.find('@');
        if (at_pos != std::string::npos && at_pos >= 4) {
            const std::string local_with_at = low_sec.substr(0, at_pos + 1);
            if (low_payload.find(local_with_at) != std::string::npos) {
                leaks.push_back("PARTIAL_EMAIL_LEAK:" + secret);
                continue;
            }
        }
        // Check core 12-char middle fragment of long tokens/keys (handles minor OCR edge char drops)
        if (at_pos == std::string::npos && low_sec.size() >= 18) {
            const std::string mid_core = low_sec.substr(4, 12);
            if (low_payload.find(mid_core) != std::string::npos) {
                leaks.push_back("PARTIAL_TOKEN_LEAK:" + secret);
                continue;
            }
        }
    }

    // Layer B: Independent pattern scan on output (stripping [REDACTED:...] markers first)
    std::string stripped(output_payload);
    static const std::regex re_placeholder(R"(\[REDACTED:[A-Z0-9_]+\])");
    stripped = std::regex_replace(stripped, re_placeholder, " ");

    static const std::regex re_email_leak(R"([A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)*\.[A-Za-z]{2,})");
    for (std::sregex_iterator it(stripped.begin(), stripped.end(), re_email_leak), end; it != end; ++it) {
        leaks.push_back("PATTERN_EMAIL_LEAK:" + it->str());
    }

    static const std::regex re_jwt_leak(R"(\beyJ[A-Za-z0-9_-]{8,}\.[A-Za-z0-9_-]{6,})");
    for (std::sregex_iterator it(stripped.begin(), stripped.end(), re_jwt_leak), end; it != end; ++it) {
        leaks.push_back("PATTERN_JWT_LEAK:" + it->str());
    }

    static const std::regex re_key_leak(
        R"(\b(?:sk-(?:live-|test-|proj-)?[A-Za-z0-9_-]{10,}|(?:ghp|gho)_[A-Za-z0-9]{10,}|AKIA[A-Z0-9]{10,}|AIza[A-Za-z0-9_-]{16,})\b)"
    );
    for (std::sregex_iterator it(stripped.begin(), stripped.end(), re_key_leak), end; it != end; ++it) {
        leaks.push_back("PATTERN_KEY_LEAK:" + it->str());
    }

    return leaks;
}

} // namespace ticket_ocr

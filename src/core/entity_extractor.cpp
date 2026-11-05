#include "ticket_ocr/entity_extractor.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <vector>

namespace ticket_ocr {

namespace {

std::string strip_trailing_punct(std::string s) {
    while (!s.empty() && (s.back() == '.' || s.back() == ',' || s.back() == ';' ||
                          s.back() == ')' || s.back() == '\'' || s.back() == '"')) {
        // Keep closing ']' for SQLSTATE[23505] or [REDACTED:...]
        s.pop_back();
    }
    return trim_copy(s);
}

std::string collapse_spaces(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    bool prev_space = false;
    for (unsigned char c : sv) {
        if (c == ' ' || c == '\t' || c == '\r') {
            if (!prev_space) {
                out.push_back(' ');
                prev_space = true;
            }
        } else {
            out.push_back(static_cast<char>(c));
            prev_space = false;
        }
    }
    return trim_copy(out);
}

bool is_button_or_chrome_line(std::string_view line) {
    const std::string clean = to_lower_ascii(fold_spanish_accents_to_ascii(collapse_spaces(line)));
    if (clean.empty()) return true;

    // Pure button bars
    static const std::regex re_buttons(
        R"(^(?:\[?\s*(?:aceptar|cancelar|cerrar|reintentar|detalles|ayuda|copiar|ok|cancel|close|retry|details|help|ignore)\s*\]?\s*)+$)"
    );
    if (std::regex_match(clean, re_buttons)) return true;

    // Pure OS / shell chrome headers
    if (clean.starts_with("ubuntu@") || clean.starts_with("root@") || clean.starts_with("archivo editar ver")) {
        return true;
    }
    if (clean == "error del sistema" || clean == "system error" ||
        clean == "alerta de aplicacion" || clean == "application alert" ||
        clean == "microsoft windows" || clean == "macos alert") {
        return true;
    }
    return false;
}

bool is_pure_metadata_line(std::string_view line) {
    const std::string clean = to_lower_ascii(fold_spanish_accents_to_ascii(collapse_spaces(line)));
    if (clean.starts_with("url:") || clean.starts_with("http://") || clean.starts_with("https://")) {
        return true;
    }
    if (clean.starts_with("modulo:") || clean.starts_with("module:") ||
        clean.starts_with("component:") || clean.starts_with("componente:") ||
        clean.starts_with("service:") || clean.starts_with("servicio:") ||
        clean.starts_with("version:") || clean.starts_with("build:")) {
        return true;
    }
    if ((clean.starts_with("codigo:") || clean.starts_with("codigo de error:") ||
         clean.starts_with("error code:") || clean.starts_with("code:")) &&
        clean.size() < 48) {
        return true;
    }
    return false;
}

std::string clean_button_suffix(std::string line) {
    static const std::regex re_trailing_btns(
        R"(\s+(?:\[\s*)?(?:Aceptar|Cancelar|Cerrar|Reintentar|Detalles|OK|Cancel|Close|Retry|Details)(?:\s*\])?(?:\s+(?:\[\s*)?(?:Aceptar|Cancelar|Cerrar|Reintentar|Detalles|OK|Cancel|Close|Retry|Details)(?:\s*\])?)*\s*$)",
        std::regex::icase
    );
    return trim_copy(std::regex_replace(line, re_trailing_btns, ""));
}

} // namespace

ExtractedFields EntityExtractor::extract(std::string_view redacted_ocr_text) const {
    ExtractedFields out;
    if (redacted_ocr_text.empty()) return out;

    const std::string text(redacted_ocr_text);
    const std::string ascii_folded = fold_spanish_accents_to_ascii(text);

    // 1. Detect Language ("ES", "EN", "MIXED")
    {
        const std::string low = to_lower_ascii(ascii_folded);
        int es_score = 0;
        int en_score = 0;
        for (const char* w : {"modulo", "codigo", "sesion", "conexion", "fallo", "facturacion", "usuario", "no se pudo", "tiempo de espera", "solicitud"}) {
            if (low.find(w) != std::string::npos) ++es_score;
        }
        for (const char* w : {"module", "error code", "failed to", "unable to", "connection", "timeout", "request", "invalid", "memory", "database"}) {
            if (low.find(w) != std::string::npos) ++en_score;
        }
        if (es_score > 0 && en_score > 1 && std::abs(es_score - en_score) <= 1) out.detected_language = "MIXED";
        else if (es_score >= en_score && es_score > 0) out.detected_language = "ES";
        else out.detected_language = "EN";
    }

    // 2. Detect OS / Environment Hint
    {
        const std::string low = to_lower_ascii(ascii_folded);
        if (low.find(".dll") != std::string::npos || low.find("0x800") != std::string::npos ||
            low.find("0xc00") != std::string::npos || low.find("windows") != std::string::npos) {
            out.detected_os_hint = "Windows";
        } else if (low.find("macos") != std::string::npos || low.find(".dylib") != std::string::npos ||
                   low.find("nsurl") != std::string::npos || low.find("corefoundation") != std::string::npos) {
            out.detected_os_hint = "macOS";
        } else if (low.find("ubuntu@") != std::string::npos || low.find("systemd") != std::string::npos ||
                   low.find(".so") != std::string::npos || low.find("linux") != std::string::npos ||
                   low.find("/var/") != std::string::npos || low.find("epipe") != std::string::npos) {
            out.detected_os_hint = "Linux";
        } else if (low.find("https://") != std::string::npos || low.find("http ") != std::string::npos) {
            out.detected_os_hint = "Web / Cloud SaaS";
        } else {
            out.detected_os_hint = "Desktop / Multiplataforma";
        }
    }

    // 3. Extract Sanitized URL
    {
        static const std::regex re_url(R"(\bhttps?://[^\s<>"']+)");
        std::smatch m;
        if (std::regex_search(text, m, re_url)) {
            out.sanitized_url = strip_trailing_punct(m.str());
        }
    }

    // 4. Extract Error Code (check structured canonical patterns first, then labeled field)
    {
        static const std::regex re_canonical_code(
            R"(\b(ERR-[A-Z0-9]+-[0-9]{3,6}|ERR_[A-Z0-9_]{4,30}|ORA-\d{4,6}|SQLSTATE\[[A-Z0-9]{4,6}\]|0x[0-9A-Fa-f]{6,10}|HTTP\s+[45]\d{2}|ECONNREFUSED|ETIMEDOUT|EACCES)\b)"
        );
        std::smatch m;
        if (std::regex_search(ascii_folded, m, re_canonical_code)) {
            out.error_code = strip_trailing_punct(m[1].str());
        } else {
            static const std::regex re_labeled_code(
                R"((?:Codigo(?:\s+de\s+error)?|Error\s*Code|Code)[ \t]*[:=#][ \t]*([A-Za-z0-9_\-\[\]]{3,28}))",
                std::regex::icase
            );
            if (std::regex_search(ascii_folded, m, re_labeled_code)) {
                out.error_code = strip_trailing_punct(m[1].str());
            }
        }
    }

    // 5. Extract Software Version
    {
        static const std::regex re_labeled_ver(
            R"((?:Version|Ver|Release)[ \t]*[:=][ \t]*(v?\d+\.\d+(?:\.\d+)?(?:-[A-Za-z0-9._-]+)?))",
            std::regex::icase
        );
        std::smatch m;
        if (std::regex_search(ascii_folded, m, re_labeled_ver)) {
            out.software_version = strip_trailing_punct(m[1].str());
        } else {
            static const std::regex re_semver(R"(\b(v\d+\.\d+\.\d+(?:-[A-Za-z0-9._-]+)?|\d{4}\.\d+\.\d+(?:-[A-Za-z0-9._-]+)?)\b)");
            if (std::regex_search(ascii_folded, m, re_semver)) {
                out.software_version = strip_trailing_punct(m[1].str());
            }
        }
    }

    // 6. Extract Module / Component / Service Name
    {
        static const std::regex re_labeled_mod(
            R"((?:Modulo|Module|Componente|Component|Servicio|Service)[ \t]*[:=][ \t]*([A-Za-z0-9_/\\-]+(?:[ \t]*\.[ \t]*[A-Za-z0-9_/\\-]+)*))",
            std::regex::icase
        );
        std::smatch m;
        if (std::regex_search(ascii_folded, m, re_labeled_mod)) {
            std::string mod_str = strip_trailing_punct(m[1].str());
            // Remove any OCR spaces around '.' inside module identifier
            mod_str = std::regex_replace(mod_str, std::regex(R"(\s*\.\s*)"), ".");
            out.module_name = mod_str;
        } else {
            static const std::regex re_namespace_or_bin(
                R"(\b([A-Z][A-Za-z0-9]+(?:\.[A-Z][A-Za-z0-9]+){1,3}|[A-Za-z0-9_-]+\.(?:dll|exe|so|sys))\b)"
            );
            for (std::sregex_iterator it(ascii_folded.begin(), ascii_folded.end(), re_namespace_or_bin), end; it != end; ++it) {
                const std::string cand = (*it)[1].str();
                if (!out.sanitized_url.empty() && out.sanitized_url.find(cand) != std::string::npos) continue;
                out.module_name = strip_trailing_punct(cand);
                break;
            }
        }
    }

    // 7. Extract Core Error Message
    {
        // First check if an explicit "Mensaje:" / "Message:" / "Detalle:" line exists
        static const std::regex re_explicit_msg(
            R"((?:Mensaje|Message|Descripcion|Description)\s*:\s*(.+))",
            std::regex::icase
        );

        std::istringstream iss(text);
        std::string raw_line;
        std::vector<std::string> candidate_lines;

        while (std::getline(iss, raw_line)) {
            std::string line = clean_button_suffix(collapse_spaces(raw_line));
            if (line.empty()) continue;

            // Remove inline footer segments if OCR merged metadata onto same line via triple spaces
            const std::string line_folded = fold_spanish_accents_to_ascii(line);
            std::smatch em;
            if (std::regex_search(line_folded, em, re_explicit_msg)) {
                // Find ':' in original UTF-8 line to preserve accents!
                const auto colon = line.find(':');
                if (colon != std::string::npos && colon + 1 < line.size()) {
                    out.error_message = trim_copy(line.substr(colon + 1));
                    break;
                }
            }

            if (is_button_or_chrome_line(line) || is_pure_metadata_line(line)) continue;

            // Skip short window title banners when there are fuller lines below
            candidate_lines.push_back(std::move(line));
        }

        if (out.error_message.empty() && !candidate_lines.empty()) {
            // If first line is a short window title (< 34 chars) and second line is longer, skip title banner
            size_t start_idx = 0;
            if (candidate_lines.size() >= 2 && candidate_lines[0].size() < 34 &&
                candidate_lines[1].size() > candidate_lines[0].size()) {
                start_idx = 1;
            }

            std::ostringstream msg_oss;
            size_t added = 0;
            for (size_t i = start_idx; i < candidate_lines.size() && added < 2; ++i) {
                const std::string& cl = candidate_lines[i];
                const std::string low_cl = to_lower_ascii(fold_spanish_accents_to_ascii(cl));
                // Stop before metadata lines containing multiple labels
                if ((low_cl.find("modulo:") != std::string::npos || low_cl.find("module:") != std::string::npos) &&
                    (low_cl.find("version:") != std::string::npos || low_cl.find("codigo:") != std::string::npos)) {
                    continue;
                }
                // Skip standalone headline that merely repeats the error code
                if (!out.error_code.empty() && cl == out.error_code) continue;
                if (added > 0) msg_oss << ' ';
                msg_oss << cl;
                ++added;
            }
            out.error_message = collapse_spaces(msg_oss.str());
            if (out.error_message.empty() && !candidate_lines.empty()) {
                out.error_message = candidate_lines.front();
            }
        }
    }

    // 8. Evaluate First-Contact Actionability (% de tickets sin pedir más información)
    if (out.error_code.empty())       out.missing_recommended_fields.push_back("Código de error");
    if (out.module_name.empty())      out.missing_recommended_fields.push_back("Módulo / Servicio");
    if (out.software_version.empty()) out.missing_recommended_fields.push_back("Versión del software");
    if (out.sanitized_url.empty())    out.missing_recommended_fields.push_back("URL / Endpoint");

    int context_anchors = 0;
    if (!out.module_name.empty())      ++context_anchors;
    if (!out.software_version.empty()) ++context_anchors;
    if (!out.sanitized_url.empty())    ++context_anchors;

    out.actionable_without_followup =
        (out.error_message.size() >= 14) &&
        (!out.error_code.empty()) &&
        (context_anchors >= 2);

    return out;
}

} // namespace ticket_ocr

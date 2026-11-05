#include "ticket_ocr/json_utils.hpp"

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace ticket_ocr {

std::string category_to_code(IncidentCategory cat) {
    switch (cat) {
        case IncidentCategory::AutenticacionAcceso:         return "AUTENTICACION_ACCESO";
        case IncidentCategory::RedConectividad:             return "RED_CONECTIVIDAD";
        case IncidentCategory::BaseDeDatos:                 return "BASE_DE_DATOS";
        case IncidentCategory::FacturacionPagos:            return "FACTURACION_PAGOS";
        case IncidentCategory::SistemaMemoriaCrash:         return "SISTEMA_MEMORIA_CRASH";
        case IncidentCategory::ConfiguracionCompatibilidad: return "CONFIGURACION_COMPATIBILIDAD";
        default:                                            return "CONFIGURACION_COMPATIBILIDAD";
    }
}

std::string category_to_display_name(IncidentCategory cat) {
    switch (cat) {
        case IncidentCategory::AutenticacionAcceso:         return "Autenticación y Control de Acceso (IAM/SSO)";
        case IncidentCategory::RedConectividad:             return "Red, Conectividad y Servicios Upstream";
        case IncidentCategory::BaseDeDatos:                 return "Base de Datos y Persistencia Transaccional";
        case IncidentCategory::FacturacionPagos:            return "Facturación Electrónica y Pasarela de Pagos";
        case IncidentCategory::SistemaMemoriaCrash:         return "Sistema Operativo, Memoria y Crash de Aplicación";
        case IncidentCategory::ConfiguracionCompatibilidad: return "Configuración, Parámetros y Compatibilidad";
        default:                                            return "Configuración, Parámetros y Compatibilidad";
    }
}

std::string category_to_support_queue(IncidentCategory cat) {
    switch (cat) {
        case IncidentCategory::AutenticacionAcceso:         return "N2-Seguridad-IAM";
        case IncidentCategory::RedConectividad:             return "N2-Infraestructura-Redes";
        case IncidentCategory::BaseDeDatos:                 return "N2-DBA-PlataformaDatos";
        case IncidentCategory::FacturacionPagos:            return "N2-FinOps-Facturacion";
        case IncidentCategory::SistemaMemoriaCrash:         return "N3-Ingenieria-CoreRuntime";
        case IncidentCategory::ConfiguracionCompatibilidad: return "N1-SoporteAplicaciones";
        default:                                            return "N1-SoporteAplicaciones";
    }
}

std::optional<IncidentCategory> category_from_code(std::string_view code) {
    const std::string upper = [&]() {
        std::string s(code);
        for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return s;
    }();
    if (upper == "AUTENTICACION_ACCESO")         return IncidentCategory::AutenticacionAcceso;
    if (upper == "RED_CONECTIVIDAD")             return IncidentCategory::RedConectividad;
    if (upper == "BASE_DE_DATOS")                 return IncidentCategory::BaseDeDatos;
    if (upper == "FACTURACION_PAGOS")            return IncidentCategory::FacturacionPagos;
    if (upper == "SISTEMA_MEMORIA_CRASH")         return IncidentCategory::SistemaMemoriaCrash;
    if (upper == "CONFIGURACION_COMPATIBILIDAD") return IncidentCategory::ConfiguracionCompatibilidad;
    return std::nullopt;
}

std::string priority_to_string(TicketPriority p) {
    switch (p) {
        case TicketPriority::P1_Critica: return "P1 - Crítica";
        case TicketPriority::P2_Alta:    return "P2 - Alta";
        case TicketPriority::P3_Media:   return "P3 - Media";
        case TicketPriority::P4_Baja:    return "P4 - Baja";
        default:                         return "P3 - Media";
    }
}

std::wstring utf8_to_wide(std::string_view utf8_str) {
    if (utf8_str.empty()) return {};
    const int needed = MultiByteToWideChar(
        CP_UTF8, 0, utf8_str.data(), static_cast<int>(utf8_str.size()), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring result(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, utf8_str.data(), static_cast<int>(utf8_str.size()), result.data(), needed);
    return result;
}

std::string wide_to_utf8(std::wstring_view wide_str) {
    if (wide_str.empty()) return {};
    const int needed = WideCharToMultiByte(
        CP_UTF8, 0, wide_str.data(), static_cast<int>(wide_str.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string result(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, wide_str.data(), static_cast<int>(wide_str.size()), result.data(), needed, nullptr, nullptr);
    return result;
}

std::string trim_copy(std::string_view sv) {
    size_t start = 0;
    while (start < sv.size() && std::isspace(static_cast<unsigned char>(sv[start]))) {
        ++start;
    }
    size_t end = sv.size();
    while (end > start && std::isspace(static_cast<unsigned char>(sv[end - 1]))) {
        --end;
    }
    return std::string(sv.substr(start, end - start));
}

std::string to_lower_ascii(std::string_view sv) {
    std::string out(sv);
    for (char& c : out) {
        if (static_cast<unsigned char>(c) < 128) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    return out;
}

std::string fold_spanish_accents_to_ascii(std::string_view utf8_str) {
    std::string out;
    out.reserve(utf8_str.size());
    for (size_t i = 0; i < utf8_str.size(); ++i) {
        const unsigned char b0 = static_cast<unsigned char>(utf8_str[i]);
        if (b0 == 0xC3 && (i + 1) < utf8_str.size()) {
            const unsigned char b1 = static_cast<unsigned char>(utf8_str[i + 1]);
            ++i;
            switch (b1) {
                case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: out.push_back('A'); break;
                case 0x88: case 0x89: case 0x8A: case 0x8B:            out.push_back('E'); break;
                case 0x8C: case 0x8D: case 0x8E: case 0x8F:            out.push_back('I'); break;
                case 0x91:                                             out.push_back('N'); break;
                case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: out.push_back('O'); break;
                case 0x99: case 0x9A: case 0x9B: case 0x9C:            out.push_back('U'); break;
                case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: out.push_back('a'); break;
                case 0xA8: case 0xA9: case 0xAA: case 0xAB:            out.push_back('e'); break;
                case 0xAC: case 0xAD: case 0xAE: case 0xAF:            out.push_back('i'); break;
                case 0xB1:                                             out.push_back('n'); break;
                case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: out.push_back('o'); break;
                case 0xB9: case 0xBA: case 0xBB: case 0xBC:            out.push_back('u'); break;
                default:
                    out.push_back(static_cast<char>(b0));
                    out.push_back(static_cast<char>(b1));
                    break;
            }
        } else if (b0 == 0xC2 && (i + 1) < utf8_str.size()) {
            const unsigned char b1 = static_cast<unsigned char>(utf8_str[i + 1]);
            if (b1 == 0xBF || b1 == 0xA1) { // ¿ or ¡
                ++i;
            } else {
                out.push_back(static_cast<char>(b0));
            }
        } else {
            out.push_back(static_cast<char>(b0));
        }
    }
    return out;
}

// --- JsonValue accessors ---

bool JsonValue::as_bool(bool fallback) const noexcept {
    if (const auto* p = std::get_if<bool>(&data)) return *p;
    return fallback;
}

double JsonValue::as_number(double fallback) const noexcept {
    if (const auto* p = std::get_if<double>(&data)) return *p;
    return fallback;
}

int JsonValue::as_int(int fallback) const noexcept {
    if (const auto* p = std::get_if<double>(&data)) return static_cast<int>(std::llround(*p));
    return fallback;
}

std::string JsonValue::as_string(std::string_view fallback) const {
    if (const auto* p = std::get_if<std::string>(&data)) return *p;
    return std::string(fallback);
}

const JsonArray& JsonValue::as_array() const {
    static const JsonArray kEmpty;
    if (const auto* p = std::get_if<JsonArray>(&data)) return *p;
    return kEmpty;
}

const JsonObject& JsonValue::as_object() const {
    static const JsonObject kEmpty;
    if (const auto* p = std::get_if<JsonObject>(&data)) return *p;
    return kEmpty;
}

const JsonValue* JsonValue::find(std::string_view key) const {
    if (const auto* obj = std::get_if<JsonObject>(&data)) {
        auto it = obj->find(std::string(key));
        if (it != obj->end()) return &it->second;
    }
    return nullptr;
}

std::string json_escape(std::string_view sv) {
    std::string out;
    out.reserve(sv.size() + 8);
    for (unsigned char c : sv) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(c));
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    return out;
}

namespace {

void serialize_impl(const JsonValue& val, std::ostringstream& oss, int indent, int depth) {
    const std::string pad(indent > 0 ? static_cast<size_t>(indent * depth) : 0, ' ');
    const std::string child_pad(indent > 0 ? static_cast<size_t>(indent * (depth + 1)) : 0, ' ');
    const std::string nl = indent > 0 ? "\n" : "";
    const std::string sp = indent > 0 ? " " : "";

    if (val.is_null()) {
        oss << "null";
    } else if (val.is_bool()) {
        oss << (val.as_bool() ? "true" : "false");
    } else if (val.is_number()) {
        const double d = val.as_number();
        if (!std::isfinite(d)) {
            oss << "0";
        } else if (std::floor(d) == d && std::fabs(d) < 1e15) {
            oss << static_cast<long long>(d);
        } else {
            oss << std::setprecision(6) << d;
        }
    } else if (val.is_string()) {
        oss << '"' << json_escape(val.as_string()) << '"';
    } else if (val.is_array()) {
        const auto& arr = val.as_array();
        if (arr.empty()) {
            oss << "[]";
            return;
        }
        oss << '[' << nl;
        for (size_t i = 0; i < arr.size(); ++i) {
            oss << child_pad;
            serialize_impl(arr[i], oss, indent, depth + 1);
            if (i + 1 < arr.size()) oss << ',';
            oss << nl;
        }
        oss << pad << ']';
    } else if (val.is_object()) {
        const auto& obj = val.as_object();
        if (obj.empty()) {
            oss << "{}";
            return;
        }
        oss << '{' << nl;
        size_t idx = 0;
        for (const auto& [k, v] : obj) {
            oss << child_pad << '"' << json_escape(k) << "\":" << sp;
            serialize_impl(v, oss, indent, depth + 1);
            if (idx + 1 < obj.size()) oss << ',';
            oss << nl;
            ++idx;
        }
        oss << pad << '}';
    }
}

class JsonParser {
public:
    explicit JsonParser(std::string_view src) : src_(src) {}

    JsonValue parse() {
        skip_ws();
        if (pos_ >= src_.size()) return JsonValue(nullptr);
        return parse_value();
    }

private:
    std::string_view src_;
    size_t pos_ = 0;

    void skip_ws() {
        // Handle optional UTF-8 BOM at beginning
        if (pos_ == 0 && src_.size() >= 3 &&
            static_cast<unsigned char>(src_[0]) == 0xEF &&
            static_cast<unsigned char>(src_[1]) == 0xBB &&
            static_cast<unsigned char>(src_[2]) == 0xBF) {
            pos_ = 3;
        }
        while (pos_ < src_.size() && std::isspace(static_cast<unsigned char>(src_[pos_]))) {
            ++pos_;
        }
    }

    JsonValue parse_value() {
        skip_ws();
        if (pos_ >= src_.size()) return JsonValue(nullptr);
        const char c = src_[pos_];
        if (c == '"') return JsonValue(parse_string());
        if (c == '{') return JsonValue(parse_object());
        if (c == '[') return JsonValue(parse_array());
        if (c == 't') {
            if (src_.substr(pos_, 4) == "true") { pos_ += 4; return JsonValue(true); }
        }
        if (c == 'f') {
            if (src_.substr(pos_, 5) == "false") { pos_ += 5; return JsonValue(false); }
        }
        if (c == 'n') {
            if (src_.substr(pos_, 4) == "null") { pos_ += 4; return JsonValue(nullptr); }
        }
        return JsonValue(parse_number());
    }

    std::string parse_string() {
        if (src_[pos_] == '"') ++pos_;
        std::string out;
        while (pos_ < src_.size()) {
            const char c = src_[pos_++];
            if (c == '"') break;
            if (c == '\\' && pos_ < src_.size()) {
                const char esc = src_[pos_++];
                switch (esc) {
                    case '"':  out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/'); break;
                    case 'b':  out.push_back('\b'); break;
                    case 'f':  out.push_back('\f'); break;
                    case 'n':  out.push_back('\n'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 't':  out.push_back('\t'); break;
                    case 'u': {
                        if (pos_ + 4 <= src_.size()) {
                            std::string hex(src_.substr(pos_, 4));
                            pos_ += 4;
                            unsigned int cp = static_cast<unsigned int>(std::strtoul(hex.c_str(), nullptr, 16));
                            if (cp < 0x80) {
                                out.push_back(static_cast<char>(cp));
                            } else if (cp < 0x800) {
                                out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                            } else {
                                out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                            }
                        }
                        break;
                    }
                    default: out.push_back(esc); break;
                }
            } else {
                out.push_back(c);
            }
        }
        return out;
    }

    double parse_number() {
        const size_t start = pos_;
        if (pos_ < src_.size() && (src_[pos_] == '-' || src_[pos_] == '+')) ++pos_;
        while (pos_ < src_.size() &&
               (std::isdigit(static_cast<unsigned char>(src_[pos_])) ||
                src_[pos_] == '.' || src_[pos_] == 'e' || src_[pos_] == 'E' ||
                src_[pos_] == '+' || src_[pos_] == '-')) {
            ++pos_;
        }
        const std::string token(src_.substr(start, pos_ - start));
        return std::strtod(token.c_str(), nullptr);
    }

    JsonArray parse_array() {
        ++pos_; // skip '['
        JsonArray arr;
        skip_ws();
        if (pos_ < src_.size() && src_[pos_] == ']') {
            ++pos_;
            return arr;
        }
        while (pos_ < src_.size()) {
            arr.push_back(parse_value());
            skip_ws();
            if (pos_ < src_.size() && src_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (pos_ < src_.size() && src_[pos_] == ']') {
                ++pos_;
                break;
            }
            break;
        }
        return arr;
    }

    JsonObject parse_object() {
        ++pos_; // skip '{'
        JsonObject obj;
        skip_ws();
        if (pos_ < src_.size() && src_[pos_] == '}') {
            ++pos_;
            return obj;
        }
        while (pos_ < src_.size()) {
            skip_ws();
            if (pos_ >= src_.size() || src_[pos_] != '"') break;
            std::string key = parse_string();
            skip_ws();
            if (pos_ < src_.size() && src_[pos_] == ':') ++pos_;
            obj[std::move(key)] = parse_value();
            skip_ws();
            if (pos_ < src_.size() && src_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (pos_ < src_.size() && src_[pos_] == '}') {
                ++pos_;
                break;
            }
            break;
        }
        return obj;
    }
};

} // namespace

std::string serialize_json(const JsonValue& val, int indent) {
    std::ostringstream oss;
    serialize_impl(val, oss, indent, 0);
    return oss.str();
}

JsonValue parse_json(std::string_view text) {
    JsonParser parser(text);
    return parser.parse();
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return {};
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

bool write_text_file(const std::filesystem::path& path, std::string_view content) {
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs) return false;
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    return ofs.good();
}

} // namespace ticket_ocr

#include "ticket_ocr/incident_classifier.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace ticket_ocr {

namespace {

struct WeightedSignal {
    IncidentCategory category;
    const char* token;
    double weight;
};

constexpr WeightedSignal kSignals[] = {
    // 1. AUTENTICACION_ACCESO
    {IncidentCategory::AutenticacionAcceso, "err-auth", 6.5},
    {IncidentCategory::AutenticacionAcceso, "err_auth", 6.5},
    {IncidentCategory::AutenticacionAcceso, "http 401", 5.5},
    {IncidentCategory::AutenticacionAcceso, "http 403", 5.2},
    {IncidentCategory::AutenticacionAcceso, "0x80070005", 5.8},
    {IncidentCategory::AutenticacionAcceso, "autenticacion", 4.2},
    {IncidentCategory::AutenticacionAcceso, "authentication", 4.2},
    {IncidentCategory::AutenticacionAcceso, "unauthorized", 4.5},
    {IncidentCategory::AutenticacionAcceso, "forbidden", 3.8},
    {IncidentCategory::AutenticacionAcceso, "acceso denegado", 4.8},
    {IncidentCategory::AutenticacionAcceso, "access denied", 4.8},
    {IncidentCategory::AutenticacionAcceso, "credenciales", 4.0},
    {IncidentCategory::AutenticacionAcceso, "credentials", 4.0},
    {IncidentCategory::AutenticacionAcceso, "saml", 4.5},
    {IncidentCategory::AutenticacionAcceso, "oauth", 4.5},
    {IncidentCategory::AutenticacionAcceso, "sso", 4.2},
    {IncidentCategory::AutenticacionAcceso, "mfa", 4.0},
    {IncidentCategory::AutenticacionAcceso, "sesion expirada", 4.4},
    {IncidentCategory::AutenticacionAcceso, "session expired", 4.4},
    {IncidentCategory::AutenticacionAcceso, "permisos insuficientes", 4.5},
    {IncidentCategory::AutenticacionAcceso, "authgateway", 4.6},
    {IncidentCategory::AutenticacionAcceso, "identityprovider", 4.6},
    {IncidentCategory::AutenticacionAcceso, "[redacted:jwt_token]", 3.2},

    // 2. RED_CONECTIVIDAD
    {IncidentCategory::RedConectividad, "err-net", 6.5},
    {IncidentCategory::RedConectividad, "err_connection", 6.5},
    {IncidentCategory::RedConectividad, "econnrefused", 6.0},
    {IncidentCategory::RedConectividad, "etimedout", 6.0},
    {IncidentCategory::RedConectividad, "http 502", 5.5},
    {IncidentCategory::RedConectividad, "http 503", 5.5},
    {IncidentCategory::RedConectividad, "http 504", 5.8},
    {IncidentCategory::RedConectividad, "gateway timeout", 5.2},
    {IncidentCategory::RedConectividad, "bad gateway", 5.0},
    {IncidentCategory::RedConectividad, "tiempo de espera", 4.6},
    {IncidentCategory::RedConectividad, "timed out", 4.5},
    {IncidentCategory::RedConectividad, "dns", 4.3},
    {IncidentCategory::RedConectividad, "tls", 4.2},
    {IncidentCategory::RedConectividad, "ssl", 4.2},
    {IncidentCategory::RedConectividad, "handshake", 4.4},
    {IncidentCategory::RedConectividad, "certificado", 3.6},
    {IncidentCategory::RedConectividad, "proxy", 4.0},
    {IncidentCategory::RedConectividad, "vpn", 4.0},
    {IncidentCategory::RedConectividad, "upstream", 4.2},
    {IncidentCategory::RedConectividad, "netproxy", 4.6},
    {IncidentCategory::RedConectividad, "apigateway", 3.8},
    {IncidentCategory::RedConectividad, "sin conexion", 4.4},
    {IncidentCategory::RedConectividad, "unreachable", 4.5},

    // 3. BASE_DE_DATOS
    {IncidentCategory::BaseDeDatos, "ora-", 6.8},
    {IncidentCategory::BaseDeDatos, "sqlstate", 6.8},
    {IncidentCategory::BaseDeDatos, "err-db", 6.5},
    {IncidentCategory::BaseDeDatos, "deadlock", 5.8},
    {IncidentCategory::BaseDeDatos, "interbloqueo", 5.8},
    {IncidentCategory::BaseDeDatos, "base de datos", 4.8},
    {IncidentCategory::BaseDeDatos, "database", 4.6},
    {IncidentCategory::BaseDeDatos, "postgres", 4.8},
    {IncidentCategory::BaseDeDatos, "mysql", 4.8},
    {IncidentCategory::BaseDeDatos, "sqlserver", 4.8},
    {IncidentCategory::BaseDeDatos, "connection pool", 5.0},
    {IncidentCategory::BaseDeDatos, "pool de conexiones", 5.2},
    {IncidentCategory::BaseDeDatos, "foreign key", 5.0},
    {IncidentCategory::BaseDeDatos, "llave foranea", 5.0},
    {IncidentCategory::BaseDeDatos, "unique constraint", 5.0},
    {IncidentCategory::BaseDeDatos, "replica", 3.8},
    {IncidentCategory::BaseDeDatos, "query", 3.5},
    {IncidentCategory::BaseDeDatos, "dbcluster", 4.8},
    {IncidentCategory::BaseDeDatos, "repository", 3.4},

    // 4. FACTURACION_PAGOS
    {IncidentCategory::FacturacionPagos, "err-pay", 6.8},
    {IncidentCategory::FacturacionPagos, "err-bill", 6.8},
    {IncidentCategory::FacturacionPagos, "err-cfdi", 6.8},
    {IncidentCategory::FacturacionPagos, "facturacion", 5.4},
    {IncidentCategory::FacturacionPagos, "factura", 4.8},
    {IncidentCategory::FacturacionPagos, "invoice", 5.2},
    {IncidentCategory::FacturacionPagos, "billing", 5.2},
    {IncidentCategory::FacturacionPagos, "cfdi", 5.8},
    {IncidentCategory::FacturacionPagos, "timbrado", 5.8},
    {IncidentCategory::FacturacionPagos, "pasarela", 5.2},
    {IncidentCategory::FacturacionPagos, "payment", 5.2},
    {IncidentCategory::FacturacionPagos, "cobro", 4.8},
    {IncidentCategory::FacturacionPagos, "cargo rechazado", 5.4},
    {IncidentCategory::FacturacionPagos, "card declined", 5.4},
    {IncidentCategory::FacturacionPagos, "stripe", 4.6},
    {IncidentCategory::FacturacionPagos, "impuesto", 4.2},
    {IncidentCategory::FacturacionPagos, "[redacted:card_pan]", 4.5},

    // 5. SISTEMA_MEMORIA_CRASH
    {IncidentCategory::SistemaMemoriaCrash, "0xc0000005", 7.0},
    {IncidentCategory::SistemaMemoriaCrash, "0xe0434352", 6.8},
    {IncidentCategory::SistemaMemoriaCrash, "err-mem", 6.5},
    {IncidentCategory::SistemaMemoriaCrash, "err-sys", 6.5},
    {IncidentCategory::SistemaMemoriaCrash, "out of memory", 6.0},
    {IncidentCategory::SistemaMemoriaCrash, "memoria insuficiente", 6.0},
    {IncidentCategory::SistemaMemoriaCrash, "access violation", 6.2},
    {IncidentCategory::SistemaMemoriaCrash, "violacion de acceso", 6.2},
    {IncidentCategory::SistemaMemoriaCrash, "segmentation fault", 6.2},
    {IncidentCategory::SistemaMemoriaCrash, "sigsegv", 6.2},
    {IncidentCategory::SistemaMemoriaCrash, "oom", 5.0},
    {IncidentCategory::SistemaMemoriaCrash, "heap", 4.6},
    {IncidentCategory::SistemaMemoriaCrash, "stack overflow", 5.5},
    {IncidentCategory::SistemaMemoriaCrash, "disco lleno", 5.4},
    {IncidentCategory::SistemaMemoriaCrash, "no space left on device", 5.8},
    {IncidentCategory::SistemaMemoriaCrash, "crash", 4.6},
    {IncidentCategory::SistemaMemoriaCrash, "coreruntime", 4.8},

    // 6. CONFIGURACION_COMPATIBILIDAD
    {IncidentCategory::ConfiguracionCompatibilidad, "err-cfg", 6.8},
    {IncidentCategory::ConfiguracionCompatibilidad, "err-comp", 6.8},
    {IncidentCategory::ConfiguracionCompatibilidad, "0x8007007e", 6.2},
    {IncidentCategory::ConfiguracionCompatibilidad, "configuracion", 4.6},
    {IncidentCategory::ConfiguracionCompatibilidad, "configuration", 4.6},
    {IncidentCategory::ConfiguracionCompatibilidad, "incompatible", 5.4},
    {IncidentCategory::ConfiguracionCompatibilidad, "version obsoleta", 5.8},
    {IncidentCategory::ConfiguracionCompatibilidad, "unsupported version", 5.8},
    {IncidentCategory::ConfiguracionCompatibilidad, "deprecated", 4.8},
    {IncidentCategory::ConfiguracionCompatibilidad, "parametro invalido", 5.0},
    {IncidentCategory::ConfiguracionCompatibilidad, "invalid parameter", 5.0},
    {IncidentCategory::ConfiguracionCompatibilidad, "schema mismatch", 5.2},
    {IncidentCategory::ConfiguracionCompatibilidad, "variable de entorno", 5.0},
    {IncidentCategory::ConfiguracionCompatibilidad, "missing dependency", 5.4},
    {IncidentCategory::ConfiguracionCompatibilidad, "dependencia faltante", 5.4},
    {IncidentCategory::ConfiguracionCompatibilidad, "configloader", 5.0},
    {IncidentCategory::ConfiguracionCompatibilidad, "pluginmanager", 4.6}
};

} // namespace

ClassificationResult IncidentClassifier::classify(const ExtractedFields& fields, std::string_view full_redacted_text) const {
    ClassificationResult out;

    // Combine structured extracted fields (with extra emphasis) + full text
    const std::string structured_blob =
        fields.error_code + " " + fields.module_name + " " + fields.sanitized_url + " " + fields.error_message;
    const std::string norm_structured = to_lower_ascii(fold_spanish_accents_to_ascii(structured_blob));
    const std::string norm_full = to_lower_ascii(fold_spanish_accents_to_ascii(full_redacted_text));

    std::array<double, 6> raw_scores{0.15, 0.15, 0.15, 0.15, 0.15, 0.15};

    for (const auto& sig : kSignals) {
        const size_t cat_idx = static_cast<size_t>(sig.category);
        if (norm_structured.find(sig.token) != std::string::npos) {
            raw_scores[cat_idx] += sig.weight * 1.35;
            if (out.matched_signals.size() < 6) {
                out.matched_signals.emplace_back(sig.token);
            }
        } else if (norm_full.find(sig.token) != std::string::npos) {
            raw_scores[cat_idx] += sig.weight;
            if (out.matched_signals.size() < 6) {
                out.matched_signals.emplace_back(sig.token);
            }
        }
    }

    // Calibrated softmax conversion into probabilities
    const double max_s = *std::max_element(raw_scores.begin(), raw_scores.end());
    double exp_sum = 0.0;
    std::array<double, 6> probs{};
    for (size_t i = 0; i < 6; ++i) {
        probs[i] = std::exp((raw_scores[i] - max_s) * 0.55);
        exp_sum += probs[i];
    }

    out.ranking.reserve(6);
    for (size_t i = 0; i < 6; ++i) {
        probs[i] /= std::max(1e-9, exp_sum);
        out.ranking.push_back(CategoryScore{static_cast<IncidentCategory>(i), probs[i]});
    }

    std::sort(out.ranking.begin(), out.ranking.end(), [](const CategoryScore& a, const CategoryScore& b) {
        return a.probability > b.probability;
    });

    out.predicted_category = out.ranking.front().category;
    out.confidence = out.ranking.front().probability;
    out.assigned_queue = category_to_support_queue(out.predicted_category);

    // Priority heuristics based on category & critical failure indicators
    if (out.predicted_category == IncidentCategory::SistemaMemoriaCrash ||
        norm_full.find("deadlock") != std::string::npos ||
        norm_full.find("interbloqueo") != std::string::npos ||
        norm_full.find("ora-12170") != std::string::npos ||
        norm_full.find("pool de conexiones") != std::string::npos ||
        norm_full.find("connection pool") != std::string::npos) {
        out.suggested_priority = TicketPriority::P1_Critica;
    } else if (out.predicted_category == IncidentCategory::FacturacionPagos ||
               out.predicted_category == IncidentCategory::BaseDeDatos ||
               out.predicted_category == IncidentCategory::RedConectividad) {
        out.suggested_priority = TicketPriority::P2_Alta;
    } else if (out.predicted_category == IncidentCategory::AutenticacionAcceso) {
        out.suggested_priority = TicketPriority::P2_Alta;
    } else {
        out.suggested_priority = TicketPriority::P3_Media;
    }

    return out;
}

} // namespace ticket_ocr

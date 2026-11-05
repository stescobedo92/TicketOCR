#include "ticket_ocr/kb_recommender.hpp"
#include "ticket_ocr/json_utils.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace ticket_ocr {

KbRecommender::KbRecommender() {
    load_builtin_catalog();
    // Attempt automatic load from portable or workspace data/knowledge_base.json if present
    const std::filesystem::path cwd_kb = std::filesystem::current_path() / "data" / "knowledge_base.json";
    const std::filesystem::path default_kb = std::filesystem::path(TICKET_OCR_SOURCE_DIR) / "data" / "knowledge_base.json";
    if (std::filesystem::exists(cwd_kb)) {
        load_from_json_file(cwd_kb);
    } else if (std::filesystem::exists(default_kb)) {
        load_from_json_file(default_kb);
    }
}

void KbRecommender::load_builtin_catalog() {
    articles_ = {
        // --- AUTENTICACION_ACCESO ---
        {
            "KB-1001",
            "Resolución de expiración o firma inválida de tokens SSO/JWT (ERR-AUTH-4019 / HTTP 401)",
            IncidentCategory::AutenticacionAcceso,
            {"ERR-AUTH-4019", "HTTP 401"},
            {"autenticacion", "jwt", "token", "sso", "oauth", "sesion expirada", "authgateway", "unauthorized"},
            "Desfase de reloj NTP o rotación de clave pública JWKS en el proveedor de identidad (IdP).",
            {
                "Verificar sincronización NTP del nodo cliente/servidor (desviación máxima tolerada: 60s).",
                "Forzar actualización de caché JWKS en AuthGateway.TokenValidator.",
                "Solicitar al usuario cerrar sesión completamente y renovar flujo OAuth2/OIDC."
            },
            "https://kb.soporte.internal/iam/KB-1001"
        },
        {
            "KB-1002",
            "Permisos insuficientes o denegación RBAC en recursos corporativos (0x80070005 / HTTP 403)",
            IncidentCategory::AutenticacionAcceso,
            {"0x80070005", "ERR-AUTH-4030", "HTTP 403"},
            {"acceso denegado", "access denied", "permisos", "forbidden", "rbac", "rol", "identityprovider"},
            "El perfil del usuario carece del rol operativo requerido en Active Directory / IAM para el módulo invocado.",
            {
                "Validar membresía de grupos IAM del usuario afectado en consola de seguridad.",
                "Confirmar política de acceso condicional MFA para el entorno solicitado.",
                "Reemitir credenciales de sesión tras asignar el rol correspondiente."
            },
            "https://kb.soporte.internal/iam/KB-1002"
        },
        {
            "KB-1003",
            "Fallo de federación SAML 2.0 y bloqueo preventivo por intentos MFA (ERR-AUTH-4092)",
            IncidentCategory::AutenticacionAcceso,
            {"ERR-AUTH-4092"},
            {"saml", "mfa", "credenciales", "credentials", "bloqueo", "federation", "login"},
            "Certificado SAML vencido o límite de reintentos de segundo factor alcanzado.",
            {
                "Desbloquear contador temporal MFA en el panel IdentityProvider.SamlBroker.",
                "Comprobar vigencia del certificado X.509 de federación empresarial.",
                "Verificar que el navegador no bloquee cookies SameSite=None del IdP."
            },
            "https://kb.soporte.internal/iam/KB-1003"
        },

        // --- RED_CONECTIVIDAD ---
        {
            "KB-2001",
            "Timeout de enlace con servicios Upstream / API Gateway (HTTP 504 / ERR-NET-5040)",
            IncidentCategory::RedConectividad,
            {"ERR-NET-5040", "HTTP 504", "ETIMEDOUT"},
            {"tiempo de espera", "timed out", "gateway timeout", "upstream", "apigateway", "netproxy"},
            "Saturación de workers en el microservicio destino o bloqueo de ruta en túnel VPN/Firewall.",
            {
                "Ejecutar health-check del upstream desde ApiGateway.UpstreamRouter.",
                "Comprobar latencia p99 y estado del túnel VPN corporativo.",
                "Ajustar timeout de proxy reverso si la operación procesa lotes extensos."
            },
            "https://kb.soporte.internal/network/KB-2001"
        },
        {
            "KB-2002",
            "Fallo de negociación TLS 1.3 / Certificado SSL no confiable en proxy corporativo (ERR-NET-5260)",
            IncidentCategory::RedConectividad,
            {"ERR-NET-5260", "HTTP 502"},
            {"tls", "ssl", "handshake", "certificado", "proxy", "bad gateway", "netproxy"},
            "Inspección HTTPS intermedia sin certificado raíz corporativo instalado en el almacén local.",
            {
                "Instalar la cadena CA corporativa actualizada en el almacén de confianza del SO.",
                "Confirmar soporte de cifrados TLS 1.2/1.3 en el endpoint remoto.",
                "Excluir el dominio de API interna de la inspección SSL profunda del proxy."
            },
            "https://kb.soporte.internal/network/KB-2002"
        },
        {
            "KB-2003",
            "Conexión rechazada o servicio no disponible en clúster regional (ECONNREFUSED / HTTP 503)",
            IncidentCategory::RedConectividad,
            {"ECONNREFUSED", "HTTP 503", "ERR_CONNECTION_TIMED_OUT"},
            {"conexion", "unreachable", "sin conexion", "dns", "503", "puerto"},
            "Instancia backend fuera de rotación en el balanceador o fallo de resolución DNS interna.",
            {
                "Verificar resolución DNS interna y estado de endpoints en el balanceador L7.",
                "Confirmar que el servicio escucha en el puerto esperado y no esté en ventana de mantenimiento."
            },
            "https://kb.soporte.internal/network/KB-2003"
        },

        // --- BASE_DE_DATOS ---
        {
            "KB-3001",
            "Timeout y agotamiento del pool de conexiones a Base de Datos (ORA-12170 / ERR-DB-3010)",
            IncidentCategory::BaseDeDatos,
            {"ORA-12170", "ERR-DB-3010"},
            {"pool de conexiones", "connection pool", "ora-", "database", "base de datos", "dbcluster", "postgres"},
            "Fuga de conexiones abiertas sin cerrar o bloqueo de firewall hacia el listener de BD.",
            {
                "Inspeccionar conexiones activas e inactivas en DbCluster.ConnectionPool.",
                "Verificar alcanzabilidad de red hacia el listener primario (puerto 1521/5432).",
                "Reciclar ordenadamente los pods con conexiones huérfanas y habilitar statement_timeout."
            },
            "https://kb.soporte.internal/dba/KB-3001"
        },
        {
            "KB-3002",
            "Interbloqueo transaccional (Deadlock) o violación de restricción única (SQLSTATE[40001] / SQLSTATE[23505])",
            IncidentCategory::BaseDeDatos,
            {"SQLSTATE[40001]", "SQLSTATE[23505]", "ERR-DB-4001"},
            {"deadlock", "interbloqueo", "sqlstate", "unique constraint", "llave", "transaccion", "repository"},
            "Transacciones concurrentes adquiriendo bloqueos de fila en orden inverso o reintento no idempotente.",
            {
                "Revisar el grafo de bloqueo en pg_stat_activity / performance_schema.",
                "Verificar clave de idempotencia antes de reinsertar el registro.",
                "Aplicar política de reintento exponencial para errores serializables 40001."
            },
            "https://kb.soporte.internal/dba/KB-3002"
        },
        {
            "KB-3003",
            "Fallo de réplica de lectura y latencia de sincronización WAL (ERR-DB-3088)",
            IncidentCategory::BaseDeDatos,
            {"ERR-DB-3088"},
            {"replica", "query", "sqlserver", "mysql", "sincronizacion"},
            "Retraso de replicación superior al umbral de consistencia configurado en el ORM.",
            {
                "Redirigir temporalmente lecturas críticas al nodo primario del clúster.",
                "Revisar IOPS de almacenamiento en la réplica afectada."
            },
            "https://kb.soporte.internal/dba/KB-3003"
        },

        // --- FACTURACION_PAGOS ---
        {
            "KB-4001",
            "Error de timbrado fiscal CFDI / Rechazo de sello digital en motor de facturación (ERR-CFDI-4402)",
            IncidentCategory::FacturacionPagos,
            {"ERR-CFDI-4402", "ERR-BILL-4402"},
            {"facturacion", "factura", "cfdi", "timbrado", "impuesto", "billing", "invoice"},
            "Certificado de Sello Digital (CSD) vencido o discrepancia de redondeo en tasas de impuestos.",
            {
                "Validar vigencia del archivo .cer/.key del emisor en BillingEngine.CfdiSigner.",
                "Comprobar catálogo fiscal de régimen y código postal del receptor.",
                "Reprocesar comprobante desde la cola de documentos pendientes sin duplicar folio."
            },
            "https://kb.soporte.internal/finops/KB-4001"
        },
        {
            "KB-4002",
            "Rechazo en pasarela de pagos / Tokenización fallida de tarjeta (ERR-PAY-4021)",
            IncidentCategory::FacturacionPagos,
            {"ERR-PAY-4021"},
            {"pasarela", "payment", "cobro", "cargo rechazado", "card declined", "stripe"},
            "Rechazo del banco emisor por validación 3D-Secure incompleta o límite transaccional.",
            {
                "Confirmar estado del intento de cobro en PaymentGateway.ChargeProcessor usando ID de referencia (nunca solicitar PAN completo).",
                "Solicitar al cliente completar autenticación 3DS2 o emplear método de pago alterno."
            },
            "https://kb.soporte.internal/finops/KB-4002"
        },
        {
            "KB-4003",
            "Desajuste de moneda o webhook duplicado en conciliación de suscripción (ERR-BILL-4109)",
            IncidentCategory::FacturacionPagos,
            {"ERR-BILL-4109"},
            {"billing", "invoice", "suscripcion", "moneda", "conciliacion"},
            "Tipo de cambio no sincronizado para el periodo contable activo.",
            {
                "Actualizar tabla de tipos de cambio diarios en el servicio financiero.",
                "Ejecutar job de reconciliación idempotente para el cliente afectado."
            },
            "https://kb.soporte.internal/finops/KB-4003"
        },

        // --- SISTEMA_MEMORIA_CRASH ---
        {
            "KB-5001",
            "Violación de acceso a memoria / Crash nativo en módulo de escritorio o worker (0xC0000005 / SIGSEGV)",
            IncidentCategory::SistemaMemoriaCrash,
            {"0xC0000005", "ERR-SYS-5005"},
            {"access violation", "violacion de acceso", "segmentation fault", "sigsegv", "crash", "coreruntime"},
            "Desreferencia de puntero nulo en caché gráfica o librería nativa desactualizada.",
            {
                "Recolectar minidump (.dmp) generado en %LOCALAPPDATA%\\CrashDumps.",
                "Desactivar aceleración por hardware GPU si ocurre al renderizar reportes pesados.",
                "Actualizar el cliente a la revisión parcheada más reciente de CoreRuntime."
            },
            "https://kb.soporte.internal/runtime/KB-5001"
        },
        {
            "KB-5002",
            "Agotamiento de memoria Heap / Out Of Memory al exportar volúmenes masivos (ERR-MEM-5012)",
            IncidentCategory::SistemaMemoriaCrash,
            {"ERR-MEM-5012", "0xE0434352"},
            {"out of memory", "memoria insuficiente", "oom", "heap", "stack overflow"},
            "Carga en memoria completa de dataset sin paginación por streaming.",
            {
                "Habilitar modo de exportación paginada por lotes (chunk_size=5000).",
                "Incrementar límite de memoria del contenedor/proceso de 2 GB a 4 GB si está justificado."
            },
            "https://kb.soporte.internal/runtime/KB-5002"
        },
        {
            "KB-5003",
            "Almacenamiento lleno en volumen temporal o partición de logs (ERR-SYS-5028)",
            IncidentCategory::SistemaMemoriaCrash,
            {"ERR-SYS-5028"},
            {"disco lleno", "no space left on device", "almacenamiento", "temp"},
            "Rotación de logs inactiva en /var/log o carpeta temporal saturada.",
            {
                "Ejecutar limpieza inmediata de artefactos temporales y rotar logs comprimidos.",
                "Verificar cuota de volumen persistente y alertas de capacidad al 80%."
            },
            "https://kb.soporte.internal/runtime/KB-5003"
        },

        // --- CONFIGURACION_COMPATIBILIDAD ---
        {
            "KB-6001",
            "Versión de cliente incompatible con el contrato de API actual (ERR-CFG-6014)",
            IncidentCategory::ConfiguracionCompatibilidad,
            {"ERR-CFG-6014", "ERR-COMP-6014"},
            {"incompatible", "version obsoleta", "unsupported version", "deprecated", "schema mismatch", "configloader"},
            "El cliente ejecuta una versión anterior al corte mínimo obligatorio del servidor.",
            {
                "Ejecutar el actualizador automático empresarial o desplegar paquete MSI/DMG vigente.",
                "Limpiar caché local de esquemas en ConfigLoader.SchemaValidator tras actualizar."
            },
            "https://kb.soporte.internal/config/KB-6001"
        },
        {
            "KB-6002",
            "Dependencia dinámica o librería del sistema faltante al iniciar módulo (0x8007007E / ERR-CFG-6078)",
            IncidentCategory::ConfiguracionCompatibilidad,
            {"0x8007007E", "ERR-CFG-6078"},
            {"missing dependency", "dependencia faltante", "pluginmanager", "dll", "modulo no encontrado"},
            "Paquete redistribuible de runtime C++ ausente o ruta PATH corrupta.",
            {
                "Reparar instalación del paquete Microsoft Visual C++ Redistributable x64.",
                "Verificar integridad de plugins firmados en el directorio de la aplicación."
            },
            "https://kb.soporte.internal/config/KB-6002"
        },
        {
            "KB-6003",
            "Parámetro de configuración regional o variable de entorno inválida (ERR-CFG-6033)",
            IncidentCategory::ConfiguracionCompatibilidad,
            {"ERR-CFG-6033"},
            {"parametro invalido", "invalid parameter", "configuracion", "configuration", "variable de entorno"},
            "Formato numérico/fecha incompatible o clave obligatoria ausente en archivo de configuración.",
            {
                "Validar archivo settings.json contra la plantilla oficial del entorno.",
                "Restablecer valores predeterminados de fábrica desde el menú Diagnóstico."
            },
            "https://kb.soporte.internal/config/KB-6003"
        }
    };
}

bool KbRecommender::load_from_json_file(const std::filesystem::path& json_path) {
    const std::string content = read_text_file(json_path);
    if (content.empty()) return false;

    const JsonValue root = parse_json(content);
    const JsonValue* arr_node = root.is_array() ? &root : root.find("articles");
    if (!arr_node || !arr_node->is_array()) return false;

    std::vector<KbArticle> loaded;
    for (const auto& item : arr_node->as_array()) {
        if (!item.is_object()) continue;
        KbArticle art;
        if (const auto* p = item.find("id")) art.id = p->as_string();
        if (const auto* p = item.find("title")) art.title = p->as_string();
        if (const auto* p = item.find("category")) {
            if (auto cat = category_from_code(p->as_string())) art.category = *cat;
        }
        if (const auto* p = item.find("error_codes")) {
            for (const auto& c : p->as_array()) art.error_codes.push_back(c.as_string());
        }
        if (const auto* p = item.find("keywords")) {
            for (const auto& kw : p->as_array()) art.keywords.push_back(kw.as_string());
        }
        if (const auto* p = item.find("root_cause")) art.root_cause = p->as_string();
        if (const auto* p = item.find("resolution_steps")) {
            for (const auto& st : p->as_array()) art.resolution_steps.push_back(st.as_string());
        }
        if (const auto* p = item.find("reference_doc_url")) art.reference_doc_url = p->as_string();
        if (!art.id.empty() && !art.title.empty()) {
            loaded.push_back(std::move(art));
        }
    }

    if (loaded.empty()) return false;
    articles_ = std::move(loaded);
    return true;
}

bool KbRecommender::save_to_json_file(const std::filesystem::path& json_path) const {
    JsonArray arr;
    for (const auto& art : articles_) {
        JsonObject obj;
        obj["id"] = art.id;
        obj["title"] = art.title;
        obj["category"] = category_to_code(art.category);

        JsonArray codes;
        for (const auto& c : art.error_codes) codes.emplace_back(c);
        obj["error_codes"] = std::move(codes);

        JsonArray kws;
        for (const auto& k : art.keywords) kws.emplace_back(k);
        obj["keywords"] = std::move(kws);

        obj["root_cause"] = art.root_cause;

        JsonArray steps;
        for (const auto& s : art.resolution_steps) steps.emplace_back(s);
        obj["resolution_steps"] = std::move(steps);

        obj["reference_doc_url"] = art.reference_doc_url;
        arr.emplace_back(std::move(obj));
    }

    JsonObject root;
    root["version"] = "1.0";
    root["articles"] = std::move(arr);
    return write_text_file(json_path, serialize_json(root, 2));
}

std::vector<KbSuggestion> KbRecommender::recommend(
    const ExtractedFields& fields,
    const ClassificationResult& classification,
    std::string_view redacted_text,
    size_t top_k) const {
    const std::string norm_code = to_lower_ascii(fields.error_code);
    const std::string norm_haystack = to_lower_ascii(fold_spanish_accents_to_ascii(
        fields.error_code + " " + fields.module_name + " " + fields.error_message + " " + std::string(redacted_text)
    ));

    std::vector<KbSuggestion> scored;
    scored.reserve(articles_.size());

    for (const auto& art : articles_) {
        double raw = 0.0;
        std::vector<std::string> reasons;

        // Exact or prefix error code match is highest-precision signal
        for (const auto& c : art.error_codes) {
            const std::string nc = to_lower_ascii(c);
            if (!norm_code.empty() && norm_code == nc) {
                raw += 6.5;
                reasons.push_back("Código exacto (" + c + ")");
                break;
            }
            if (!nc.empty() && norm_haystack.find(nc) != std::string::npos) {
                raw += 4.8;
                reasons.push_back("Referencia a código (" + c + ")");
                break;
            }
        }

        // Category alignment
        if (art.category == classification.predicted_category) {
            raw += 3.0 * std::max(0.5, classification.confidence);
            reasons.push_back("Categoría " + category_to_code(art.category));
        }

        // Keyword BM25-style hits
        int kw_hits = 0;
        for (const auto& kw : art.keywords) {
            const std::string nkw = to_lower_ascii(fold_spanish_accents_to_ascii(kw));
            if (!nkw.empty() && norm_haystack.find(nkw) != std::string::npos) {
                ++kw_hits;
                raw += 1.15;
            }
        }
        if (kw_hits > 0) {
            reasons.push_back(std::to_string(kw_hits) + " términos clave coincidentes");
        }

        if (raw > 0.4) {
            KbSuggestion sug;
            sug.article = art;
            sug.relevance_score = std::clamp(1.0 - std::exp(-0.28 * raw), 0.10, 0.99);
            std::ostringstream ross;
            for (size_t i = 0; i < reasons.size(); ++i) {
                if (i > 0) ross << " + ";
                ross << reasons[i];
            }
            sug.match_reason = ross.str();
            scored.push_back(std::move(sug));
        }
    }

    std::sort(scored.begin(), scored.end(), [](const KbSuggestion& a, const KbSuggestion& b) {
        return a.relevance_score > b.relevance_score;
    });

    if (scored.size() > top_k) {
        scored.resize(top_k);
    }
    return scored;
}

} // namespace ticket_ocr

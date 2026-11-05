#include "tests/test_framework.hpp"
#include "ticket_ocr/entity_extractor.hpp"
#include "ticket_ocr/incident_classifier.hpp"
#include "ticket_ocr/kb_recommender.hpp"
#include "ticket_ocr/ticket_pipeline.hpp"

using namespace ticket_ocr;

TEST_CASE(MultilingualEntityExtractionAndFirstContactCompleteness) {
    EntityExtractor extractor;

    const std::string es_complete =
        "Seguridad Corporativa\n"
        "https://sso.corporativo.mx/oauth2/authorize?token=[REDACTED:URL_SECRET]\n"
        "Código: ERR-AUTH-4019\n"
        "Mensaje: La sesión expiró para [REDACTED:EMAIL] al validar firma OAuth\n"
        "Módulo: AuthGateway.TokenValidator\n"
        "Versión: v4.12.3\n"
        "Aceptar";

    const ExtractedFields f_es = extractor.extract(es_complete);
    REQUIRE_EQ(f_es.error_code, std::string("ERR-AUTH-4019"));
    REQUIRE_EQ(f_es.module_name, std::string("AuthGateway.TokenValidator"));
    REQUIRE_EQ(f_es.software_version, std::string("v4.12.3"));
    REQUIRE_EQ(f_es.sanitized_url, std::string("https://sso.corporativo.mx/oauth2/authorize?token=[REDACTED:URL_SECRET]"));
    REQUIRE_EQ(f_es.error_message, std::string("La sesión expiró para [REDACTED:EMAIL] al validar firma OAuth"));
    REQUIRE_TRUE(f_es.actionable_without_followup);

    // Partial user crop missing URL & Version -> must flag as requiring follow-up
    const std::string partial_crop =
        "Código: ERR-CFG-6033\n"
        "Mensaje: Parámetro inválido en variable de entorno regional\n"
        "Módulo: ConfigLoader.EnvParser";
    const ExtractedFields f_partial = extractor.extract(partial_crop);
    REQUIRE_TRUE(!f_partial.actionable_without_followup);
    REQUIRE_GE(f_partial.missing_recommended_fields.size(), static_cast<size_t>(2));
}

TEST_CASE(AllSixSupportCategoriesClassificationAndKbMatching) {
    TicketPipeline pipeline;

    struct Case {
        const char* raw;
        IncidentCategory expected_cat;
        const char* expected_kb_id;
    };

    const Case cases[] = {
        {
            "Codigo: ERR-AUTH-4019\nMensaje: Sesion expirada SSO OAuth\nModulo: AuthGateway.TokenValidator\nVersion: v4.1.0",
            IncidentCategory::AutenticacionAcceso, "KB-1001"
        },
        {
            "Error Code: ERR-NET-5040\nMessage: Gateway timeout connecting to upstream\nModule: ApiGateway.UpstreamRouter\nVersion: v2.1.0",
            IncidentCategory::RedConectividad, "KB-2001"
        },
        {
            "Codigo: ORA-12170\nMensaje: Agotamiento del pool de conexiones en base de datos\nModulo: DbCluster.ConnectionPool\nVersion: v7.4.0",
            IncidentCategory::BaseDeDatos, "KB-3001"
        },
        {
            "Codigo: ERR-CFDI-4402\nMensaje: Error de timbrado CFDI en factura electronica\nModulo: BillingEngine.CfdiSigner\nVersion: v8.2.1",
            IncidentCategory::FacturacionPagos, "KB-4001"
        },
        {
            "Error Code: 0xC0000005\nMessage: Unhandled access violation crash\nModule: CoreRuntime.RenderWorker\nVersion: v9.0.1",
            IncidentCategory::SistemaMemoriaCrash, "KB-5001"
        },
        {
            "Codigo: ERR-CFG-6014\nMensaje: Version obsoleta e incompatible con el esquema\nModulo: ConfigLoader.SchemaValidator\nVersion: v1.9.2",
            IncidentCategory::ConfiguracionCompatibilidad, "KB-6001"
        }
    };

    for (const auto& c : cases) {
        const TicketDraft draft = pipeline.process_raw_text(c.raw);
        REQUIRE_EQ(category_to_code(draft.category), category_to_code(c.expected_cat));
        REQUIRE_TRUE(!draft.suggested_articles.empty());
        REQUIRE_EQ(draft.suggested_articles.front().article.id, std::string(c.expected_kb_id));
    }
}

TEST_CASE(KnowledgeBaseJsonSerializationRoundtrip) {
    KbRecommender kb1;
    const std::filesystem::path tmp_kb = std::filesystem::temp_directory_path() / "ticket_ocr_kb_test.json";
    REQUIRE_TRUE(kb1.save_to_json_file(tmp_kb));

    KbRecommender kb2;
    REQUIRE_TRUE(kb2.load_from_json_file(tmp_kb));
    REQUIRE_EQ(kb1.articles().size(), kb2.articles().size());

    std::error_code ec;
    std::filesystem::remove(tmp_kb, ec);
}

int main() {
    return test_fw::run_all_tests("Extracción Estructurada, Clasificador Multiclase y Base de Conocimiento (KB)");
}

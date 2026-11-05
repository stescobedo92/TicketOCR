#include "tests/test_framework.hpp"
#include "ticket_ocr/dlp_redactor.hpp"
#include "ticket_ocr/ticket_pipeline.hpp"

using namespace ticket_ocr;

TEST_CASE(ZeroLeakageAcrossAllSensitiveSecretTypes) {
    const std::vector<std::string> planted = {
        "carlos.mendez@finanzas-latam.mx",
        "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiJ1c3JfOTg0MSJ9.Sig89KpL2xQ9mZ",
        "sk-live-94a8f72c10b3e6d549f8a1",
        "ghp_7Km9Pq2Rs5Tu8Vw1Xy4Za6Bc3De9Fg0HiJk",
        "AKIAIOSFODNN7EXAMPLE99",
        "4532-0151-1283-0366",
        "sec_live_445566778899aabbcc",
        "dba_prod:Sup3rS3cretDB99"
    };

    const std::string raw_incident =
        "https://dba_prod:Sup3rS3cretDB99@api.pagos.mx/v2/charge?api_key=sec_live_445566778899aabbcc\n"
        "Codigo: ERR-PAY-4021\n"
        "Mensaje: Fallo al cobrar tarjeta 4532-0151-1283-0366 de carlos.mendez@finanzas-latam.mx "
        "con JWT eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiJ1c3JfOTg0MSJ9.Sig89KpL2xQ9mZ "
        "y llaves sk-live-94a8f72c10b3e6d549f8a1 ghp_7Km9Pq2Rs5Tu8Vw1Xy4Za6Bc3De9Fg0HiJk AKIAIOSFODNN7EXAMPLE99\n"
        "Modulo: PaymentGateway.ChargeProcessor\n"
        "Version: v4.9.5";

    TicketPipeline pipeline;
    const TicketDraft ticket = pipeline.process_raw_text(raw_incident, "dlp_test_case.png");

    // Technical diagnostic fields MUST be preserved intact!
    REQUIRE_EQ(ticket.fields.error_code, std::string("ERR-PAY-4021"));
    REQUIRE_EQ(ticket.fields.module_name, std::string("PaymentGateway.ChargeProcessor"));
    REQUIRE_EQ(ticket.fields.software_version, std::string("v4.9.5"));
    REQUIRE_TRUE(ticket.fields.sanitized_url.find("api.pagos.mx/v2/charge") != std::string::npos);

    // Full JSON + Markdown export MUST have strictly 0 leaks!
    const std::string exported =
        TicketPipeline::ticket_to_json_string(ticket) + "\n" +
        TicketPipeline::ticket_to_markdown(ticket);

    const auto leaks = DlpRedactor::audit_for_leaks(exported, planted);
    REQUIRE_EQ(leaks.size(), static_cast<size_t>(0));
    REQUIRE_GE(ticket.dlp_summary.total_redactions, static_cast<size_t>(8));
}

TEST_CASE(OcrPerturbedEmailAndJwtTolerance) {
    DlpRedactor dlp;
    // Simulate OCR inserting spaces around '@' in email and '.' in JWT
    const std::string noisy =
        "Usuario afectado: maria.gomez @ retailcorp.es con token "
        "eyJhbGciOiJIUzI1NiJ9 . eyJzdWIiOiJhZG1pbiJ9 . K9mP4xR2vL8n";

    const RedactionResult r = dlp.redact(noisy);
    REQUIRE_TRUE(r.redacted_text.find("maria.gomez") == std::string::npos);
    REQUIRE_TRUE(r.redacted_text.find("eyJhbG") == std::string::npos);
    REQUIRE_TRUE(r.redacted_text.find("[REDACTED:EMAIL]") != std::string::npos);
    REQUIRE_TRUE(r.redacted_text.find("[REDACTED:JWT_TOKEN]") != std::string::npos);
}

int main() {
    return test_fw::run_all_tests("Motor DLP Zero-Leakage y Preservación de Diagnóstico Técnico");
}

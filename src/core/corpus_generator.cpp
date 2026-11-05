#include "ticket_ocr/corpus_generator.hpp"
#include "ticket_ocr/image_io.hpp"
#include "ticket_ocr/json_utils.hpp"

#include <Windows.h>
#include <objidl.h>
#include <algorithm>
using std::max;
using std::min;
#include <gdiplus.h>

#include <cmath>
#include <cstdint>
#include <random>

namespace ticket_ocr {

std::vector<CorpusSample> CorpusGenerator::build_default_corpus_spec() {
    std::vector<CorpusSample> spec;
    spec.reserve(18);

    // 01: Windows 11 | ES | Light | 1280x720 | Auth + JWT + Email
    spec.push_back(CorpusSample{
        "S01_WIN_ES_AUTH_LIGHT",
        "S01_WIN_ES_AUTH_LIGHT.png",
        "Windows11", "ES", 1280, 720, false, false,
        "ERR-AUTH-4019",
        "AuthGateway.TokenValidator",
        "v4.12.3",
        "https://sso.corporativo-latam.mx/oauth2/authorize?token=[REDACTED:URL_SECRET]",
        "La sesión expiró para [REDACTED:EMAIL] al validar firma OAuth [REDACTED:JWT_TOKEN]",
        IncidentCategory::AutenticacionAcceso, true,
        "Seguridad Corporativa - Windows 11",
        "Codigo: ERR-AUTH-4019",
        "Mensaje: La sesión expiró para carlos.mendez@finanzas-latam.mx al validar firma OAuth eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiJ1c3JfOTg0MSJ9.Sig89KpL2xQ9mZ",
        "Modulo: AuthGateway.TokenValidator",
        "Version: v4.12.3",
        "https://sso.corporativo-latam.mx/oauth2/authorize?token=tok_prod_99887766aabbcc",
        {
            "carlos.mendez@finanzas-latam.mx",
            "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiJ1c3JfOTg0MSJ9.Sig89KpL2xQ9mZ",
            "tok_prod_99887766aabbcc"
        }
    });

    // 02: WebSaaS | EN | Dark Mode | 1600x900 | Auth + API Key
    spec.push_back(CorpusSample{
        "S02_WEB_EN_AUTH_DARK",
        "S02_WEB_EN_AUTH_DARK.png",
        "WebSaaS", "EN", 1600, 900, true, false,
        "0x80070005",
        "IdentityProvider.RbacGuard",
        "v5.1.0",
        "https://console.cloudops-global.io/iam/roles",
        "Access denied for account [REDACTED:EMAIL] using key [REDACTED:API_KEY]",
        IncidentCategory::AutenticacionAcceso, true,
        "CloudOps Console - Access Alert",
        "Error Code: 0x80070005",
        "Message: Access denied for account sec.ops@cloudops-global.io using key ghp_7Km9Pq2Rs5Tu8Vw1Xy4Za6Bc3De9Fg0HiJk",
        "Module: IdentityProvider.RbacGuard",
        "Version: v5.1.0",
        "https://console.cloudops-global.io/iam/roles",
        {
            "sec.ops@cloudops-global.io",
            "ghp_7Km9Pq2Rs5Tu8Vw1Xy4Za6Bc3De9Fg0HiJk"
        }
    });

    // 03: Mobile Moire Photo | macOS | ES | Light | 1280x720 | Auth SAML
    spec.push_back(CorpusSample{
        "S03_MAC_ES_AUTH_MOIRE",
        "S03_MAC_ES_AUTH_MOIRE.jpg",
        "macOS", "ES", 1280, 720, false, true,
        "ERR-AUTH-4092",
        "IdentityProvider.SamlBroker",
        "v3.8.4",
        "https://idp.grupo-industrial.es/saml2/sso",
        "Bloqueo preventivo MFA en cuenta [REDACTED:EMAIL] por certificado SAML vencido",
        IncidentCategory::AutenticacionAcceso, true,
        "Alerta de Identidad macOS",
        "Codigo: ERR-AUTH-4092",
        "Mensaje: Bloqueo preventivo MFA en cuenta elena.rojas@grupo-industrial.es por certificado SAML vencido",
        "Modulo: IdentityProvider.SamlBroker",
        "Version: v3.8.4",
        "https://idp.grupo-industrial.es/saml2/sso",
        {"elena.rojas@grupo-industrial.es"}
    });

    // 04: WebSaaS | ES | Light | 1920x1080 | Network Gateway Timeout + URL api_key
    spec.push_back(CorpusSample{
        "S04_WEB_ES_NET_LIGHT",
        "S04_WEB_ES_NET_LIGHT.png",
        "WebSaaS", "ES", 1920, 1080, false, false,
        "ERR-NET-5040",
        "ApiGateway.UpstreamRouter",
        "v2.19.0",
        "https://api.logistica-express.mx/v2/shipments?api_key=[REDACTED:URL_SECRET]",
        "Tiempo de espera agotado al conectar con el servicio upstream en el puerto 8443",
        IncidentCategory::RedConectividad, true,
        "Portal Logístico - HTTP 504",
        "Codigo: ERR-NET-5040",
        "Mensaje: Tiempo de espera agotado al conectar con el servicio upstream en el puerto 8443",
        "Modulo: ApiGateway.UpstreamRouter",
        "Version: v2.19.0",
        "https://api.logistica-express.mx/v2/shipments?api_key=sec_live_445566778899aabbcc",
        {"sec_live_445566778899aabbcc"}
    });

    // 05: Linux | EN | Dark Mode + Mobile Moire | 1280x720 | Network TLS Handshake
    spec.push_back(CorpusSample{
        "S05_LIN_EN_NET_DARK_MOIRE",
        "S05_LIN_EN_NET_DARK_MOIRE.jpg",
        "Linux", "EN", 1280, 720, true, true,
        "ERR-NET-5260",
        "NetProxy.TlsConnector",
        "v6.0.2",
        "https://edge.internal-mesh.net/healthz",
        "TLS handshake failed with corporate proxy for node [REDACTED:EMAIL]",
        IncidentCategory::RedConectividad, true,
        "ubuntu@prod-gateway-04",
        "Error Code: ERR-NET-5260",
        "Message: TLS handshake failed with corporate proxy for node netadmin@internal-mesh.net",
        "Module: NetProxy.TlsConnector",
        "Version: v6.0.2",
        "https://edge.internal-mesh.net/healthz",
        {"netadmin@internal-mesh.net"}
    });

    // 06: Windows 11 | EN | Compact Low-Res 860x520 | Network ECONNREFUSED
    spec.push_back(CorpusSample{
        "S06_WIN_EN_NET_LOWRES",
        "S06_WIN_EN_NET_LOWRES.png",
        "Windows11", "EN", 860, 520, false, false,
        "ECONNREFUSED",
        "NetProxy.DnsResolver",
        "v4.3.1",
        "https://sync.erp-enterprise.com/v1/ping",
        "Upstream cluster unreachable and connection refused on regional endpoint",
        IncidentCategory::RedConectividad, true,
        "ERP Sync Connector",
        "Error Code: ECONNREFUSED",
        "Message: Upstream cluster unreachable and connection refused on regional endpoint",
        "Module: NetProxy.DnsResolver",
        "Version: v4.3.1",
        "https://sync.erp-enterprise.com/v1/ping",
        {}
    });

    // 07: Linux | ES | Dark Mode | 1600x900 | Database ORA-12170 + Basic Auth URL
    spec.push_back(CorpusSample{
        "S07_LIN_ES_DB_DARK",
        "S07_LIN_ES_DB_DARK.png",
        "Linux", "ES", 1600, 900, true, false,
        "ORA-12170",
        "DbCluster.ConnectionPool",
        "v7.4.0",
        "https://[REDACTED:URL_CREDENTIALS]@dbmonitor.banca-central.mx/status",
        "Agotamiento del pool de conexiones hacia la base de datos primaria",
        IncidentCategory::BaseDeDatos, true,
        "Monitor Transaccional Linux",
        "Codigo: ORA-12170",
        "Mensaje: Agotamiento del pool de conexiones hacia la base de datos primaria",
        "Modulo: DbCluster.ConnectionPool",
        "Version: v7.4.0",
        "https://dba_prod:Sup3rS3cretDB99@dbmonitor.banca-central.mx/status",
        {"dba_prod:Sup3rS3cretDB99"}
    });

    // 08: WebSaaS | EN | Light + Mobile Moire | 1280x720 | Database Deadlock SQLSTATE
    spec.push_back(CorpusSample{
        "S08_WEB_EN_DB_MOIRE",
        "S08_WEB_EN_DB_MOIRE.jpg",
        "WebSaaS", "EN", 1280, 720, false, true,
        "SQLSTATE[40001]",
        "OrderRepository.TxManager",
        "v3.11.8",
        "https://orders.commerce-hub.io/api/v3/commit",
        "Database transaction deadlock detected while updating inventory ledger",
        IncidentCategory::BaseDeDatos, true,
        "CommerceHub Backoffice",
        "Error Code: SQLSTATE[40001]",
        "Message: Database transaction deadlock detected while updating inventory ledger",
        "Module: OrderRepository.TxManager",
        "Version: v3.11.8",
        "https://orders.commerce-hub.io/api/v3/commit",
        {}
    });

    // 09: Windows 11 | ES | Dark Mode | 1280x720 | Database Replica + AWS Key
    spec.push_back(CorpusSample{
        "S09_WIN_ES_DB_DARK",
        "S09_WIN_ES_DB_DARK.png",
        "Windows11", "ES", 1280, 720, true, false,
        "ERR-DB-3088",
        "DbCluster.ReplicaReader",
        "v5.6.2",
        "https://datos.salud-integral.org/v1/replicas",
        "Fallo de lectura en replica SQL con credencial [REDACTED:API_KEY]",
        IncidentCategory::BaseDeDatos, true,
        "Consola de Datos Clínicos",
        "Codigo: ERR-DB-3088",
        "Mensaje: Fallo de lectura en replica SQL con credencial AKIAIOSFODNN7EXAMPLE99",
        "Modulo: DbCluster.ReplicaReader",
        "Version: v5.6.2",
        "https://datos.salud-integral.org/v1/replicas",
        {"AKIAIOSFODNN7EXAMPLE99"}
    });

    // 10: Windows 11 | ES | Light | 1600x900 | Billing CFDI + Email
    spec.push_back(CorpusSample{
        "S10_WIN_ES_BILL_LIGHT",
        "S10_WIN_ES_BILL_LIGHT.png",
        "Windows11", "ES", 1600, 900, false, false,
        "ERR-CFDI-4402",
        "BillingEngine.CfdiSigner",
        "v8.2.1",
        "https://facturacion.erp-fiscal.mx/v4/timbrado",
        "Error de timbrado CFDI por sello vencido del receptor [REDACTED:EMAIL]",
        IncidentCategory::FacturacionPagos, true,
        "Módulo Fiscal ERP",
        "Codigo: ERR-CFDI-4402",
        "Mensaje: Error de timbrado CFDI por sello vencido del receptor contabilidad@aceros-norte.mx",
        "Modulo: BillingEngine.CfdiSigner",
        "Version: v8.2.1",
        "https://facturacion.erp-fiscal.mx/v4/timbrado",
        {"contabilidad@aceros-norte.mx"}
    });

    // 11: WebSaaS | ES | Dark Mode + Mobile Moire | 1280x720 | Payment Card Luhn PAN + sk-live key!
    spec.push_back(CorpusSample{
        "S11_WEB_ES_PAY_DARK_MOIRE",
        "S11_WEB_ES_PAY_DARK_MOIRE.jpg",
        "WebSaaS", "ES", 1280, 720, true, true,
        "ERR-PAY-4021",
        "PaymentGateway.ChargeProcessor",
        "v4.9.5",
        "https://checkout.pagos-seguros.com/v2/charge",
        "Cargo rechazado en pasarela para tarjeta [REDACTED:CARD_PAN] con llave [REDACTED:API_KEY]",
        IncidentCategory::FacturacionPagos, true,
        "Pasarela Checkout - Error de Cobro",
        "Codigo: ERR-PAY-4021",
        "Mensaje: Cargo rechazado en pasarela para tarjeta 4532-0151-1283-0366 con llave sk-live-94a8f72c10b3e6d549f8a1",
        "Modulo: PaymentGateway.ChargeProcessor",
        "Version: v4.9.5",
        "https://checkout.pagos-seguros.com/v2/charge",
        {"4532-0151-1283-0366", "sk-live-94a8f72c10b3e6d549f8a1"}
    });

    // 12: macOS | EN | Light | 1280x720 | Billing Invoice Reconciliation
    spec.push_back(CorpusSample{
        "S12_MAC_EN_BILL_LIGHT",
        "S12_MAC_EN_BILL_LIGHT.png",
        "macOS", "EN", 1280, 720, false, false,
        "ERR-BILL-4109",
        "BillingEngine.InvoiceLedger",
        "v3.4.7",
        "https://billing.saas-suite.io/v1/invoices/reconcile",
        "Invoice currency reconciliation failed for customer [REDACTED:EMAIL]",
        IncidentCategory::FacturacionPagos, true,
        "SaaS Billing Desktop",
        "Error Code: ERR-BILL-4109",
        "Message: Invoice currency reconciliation failed for customer ap.team@global-retail.com",
        "Module: BillingEngine.InvoiceLedger",
        "Version: v3.4.7",
        "https://billing.saas-suite.io/v1/invoices/reconcile",
        {"ap.team@global-retail.com"}
    });

    // 13: Windows 11 | EN | Dark Mode | 1280x720 | Crash 0xC0000005 Access Violation
    spec.push_back(CorpusSample{
        "S13_WIN_EN_CRASH_DARK",
        "S13_WIN_EN_CRASH_DARK.png",
        "Windows11", "EN", 1280, 720, true, false,
        "0xC0000005",
        "CoreRuntime.RenderWorker",
        "v9.0.1",
        "https://telemetry.workstation-app.io/crash-reports",
        "Unhandled access violation crash while rendering analytics dashboard",
        IncidentCategory::SistemaMemoriaCrash, true,
        "Workstation Pro Fatal Exception",
        "Error Code: 0xC0000005",
        "Message: Unhandled access violation crash while rendering analytics dashboard",
        "Module: CoreRuntime.RenderWorker",
        "Version: v9.0.1",
        "https://telemetry.workstation-app.io/crash-reports",
        {}
    });

    // 14: Linux | ES | Light + Mobile Moire | 1280x720 | Out of Memory Heap
    spec.push_back(CorpusSample{
        "S14_LIN_ES_MEM_MOIRE",
        "S14_LIN_ES_MEM_MOIRE.jpg",
        "Linux", "ES", 1280, 720, false, true,
        "ERR-MEM-5012",
        "CoreRuntime.ExportBatch",
        "v2.7.9",
        "https://reportes.bi-corporativo.cl/export/xlsx",
        "Memoria insuficiente en heap al exportar lote contable de [REDACTED:EMAIL]",
        IncidentCategory::SistemaMemoriaCrash, true,
        "Nodo de Exportación BI",
        "Codigo: ERR-MEM-5012",
        "Mensaje: Memoria insuficiente en heap al exportar lote contable de ana.torres@bi-corporativo.cl",
        "Modulo: CoreRuntime.ExportBatch",
        "Version: v2.7.9",
        "https://reportes.bi-corporativo.cl/export/xlsx",
        {"ana.torres@bi-corporativo.cl"}
    });

    // 15: macOS | EN | Dark Mode | 1600x900 | System Disk Full
    spec.push_back(CorpusSample{
        "S15_MAC_EN_SYS_DARK",
        "S15_MAC_EN_SYS_DARK.png",
        "macOS", "EN", 1600, 900, true, false,
        "ERR-SYS-5028",
        "CoreRuntime.StorageSpooler",
        "v4.5.0",
        "https://agent.media-studio.io/spool/status",
        "No space left on device while writing temporary render cache",
        IncidentCategory::SistemaMemoriaCrash, true,
        "Media Studio Agent Alert",
        "Error Code: ERR-SYS-5028",
        "Message: No space left on device while writing temporary render cache",
        "Module: CoreRuntime.StorageSpooler",
        "Version: v4.5.0",
        "https://agent.media-studio.io/spool/status",
        {}
    });

    // 16: WebSaaS | ES | Light | 1280x720 | Config Incompatible Client Version
    spec.push_back(CorpusSample{
        "S16_WEB_ES_CFG_LIGHT",
        "S16_WEB_ES_CFG_LIGHT.png",
        "WebSaaS", "ES", 1280, 720, false, false,
        "ERR-CFG-6014",
        "ConfigLoader.SchemaValidator",
        "v1.9.2",
        "https://clientes.plataforma-rrhh.pe/api/schema?secret=[REDACTED:URL_SECRET]",
        "Versión obsoleta e incompatible con el esquema actual del servidor",
        IncidentCategory::ConfiguracionCompatibilidad, true,
        "Plataforma RRHH - Compatibilidad",
        "Codigo: ERR-CFG-6014",
        "Mensaje: Versión obsoleta e incompatible con el esquema actual del servidor",
        "Modulo: ConfigLoader.SchemaValidator",
        "Version: v1.9.2",
        "https://clientes.plataforma-rrhh.pe/api/schema?secret=key_prod_77331199zzxxcc",
        {"key_prod_77331199zzxxcc"}
    });

    // 17: Windows 11 | EN | Compact Low-Res 880x540 | Missing Dependency DLL
    spec.push_back(CorpusSample{
        "S17_WIN_EN_CFG_LOWRES",
        "S17_WIN_EN_CFG_LOWRES.png",
        "Windows11", "EN", 880, 540, false, false,
        "0x8007007E",
        "PluginManager.NativeLoader",
        "v6.2.0",
        "https://updates.cad-suite.com/runtime/vc143",
        "Missing dependency library while initializing native CAD plugin",
        IncidentCategory::ConfiguracionCompatibilidad, true,
        "CAD Suite Loader",
        "Error Code: 0x8007007E",
        "Message: Missing dependency library while initializing native CAD plugin",
        "Module: PluginManager.NativeLoader",
        "Version: v6.2.0",
        "https://updates.cad-suite.com/runtime/vc143",
        {}
    });

    // 18: Linux | ES | Dark Mode | 1280x720 | Partial User Crop (no URL & no version) -> tests % sin pedir mas info!
    spec.push_back(CorpusSample{
        "S18_LIN_ES_CFG_PARTIAL_DARK",
        "S18_LIN_ES_CFG_PARTIAL_DARK.png",
        "Linux", "ES", 1280, 720, true, false,
        "ERR-CFG-6033",
        "ConfigLoader.EnvParser",
        "",
        "",
        "Parámetro inválido en variable de entorno regional para [REDACTED:EMAIL]",
        IncidentCategory::ConfiguracionCompatibilidad, false,
        "Recorte de Terminal",
        "Codigo: ERR-CFG-6033",
        "Mensaje: Parámetro inválido en variable de entorno regional para soporte.ti@planta-sur.ar",
        "Modulo: ConfigLoader.EnvParser",
        "",
        "",
        {"soporte.ti@planta-sur.ar"}
    });

    return spec;
}

namespace {

void apply_mobile_camera_moire_simulation(ImageBuffer& img, uint32_t seed) {
    const int w = img.width;
    const int h = img.height;
    ImageBuffer src_copy = img;

    std::mt19937 rng(seed);
    std::normal_distribution<float> sensor_noise(0.0f, 3.2f);

    // Slight camera roll angle (~0.65 degrees) + 2D RGB sinusoidal LCD interference moire + radial vignette + hotspot glare
    const float angle = 0.0113f; // ~0.65 deg
    const float cos_a = std::cos(angle);
    const float sin_a = std::sin(angle);
    const float cx = w * 0.5f;
    const float cy = h * 0.5f;
    const float glare_x = w * 0.34f;
    const float glare_y = h * 0.28f;
    const float inv_max_r2 = 1.0f / (cx * cx + cy * cy);

    for (int y = 0; y < h; ++y) {
        uint8_t* drow = img.row_ptr(y);
        const float dy = static_cast<float>(y) - cy;

        for (int x = 0; x < w; ++x) {
            const float dx = static_cast<float>(x) - cx;

            // Bilinear sample from slightly tilted monitor plane
            const float sx = cx + dx * cos_a + dy * sin_a;
            const float sy = cy - dx * sin_a + dy * cos_a;
            const int ix = std::clamp(static_cast<int>(std::lround(sx)), 0, w - 1);
            const int iy = std::clamp(static_cast<int>(std::lround(sy)), 0, h - 1);
            const uint8_t* spx = src_copy.row_ptr(iy) + static_cast<size_t>(ix) * 4;

            // High-frequency LCD pixel-grid beating against CMOS grid (phase-shifted per R,G,B subpixel!)
            const float wave1 = std::sin(static_cast<float>(x) * 1.48f + static_cast<float>(y) * 0.37f);
            const float wave2 = std::cos(static_cast<float>(x) * 0.29f - static_cast<float>(y) * 1.41f);
            const float moire_b = 14.0f * wave1 + 8.0f * wave2;
            const float moire_g = 14.0f * std::sin(static_cast<float>(x) * 1.48f + static_cast<float>(y) * 0.37f + 1.05f) + 8.0f * wave2;
            const float moire_r = 14.0f * std::sin(static_cast<float>(x) * 1.48f + static_cast<float>(y) * 0.37f + 2.10f) - 8.0f * wave2;

            // Vignette darkening toward corners + soft ambient glare reflection
            const float r2 = (dx * dx + dy * dy) * inv_max_r2;
            const float vignette = 1.0f - 0.24f * r2;

            const float gx = static_cast<float>(x) - glare_x;
            const float gy = static_cast<float>(y) - glare_y;
            const float glare = 16.0f * std::exp(-(gx * gx + gy * gy) / (2.0f * (w * 0.19f) * (w * 0.19f)));

            const float n = sensor_noise(rng);
            const size_t out_i = static_cast<size_t>(x) * 4;

            auto clamp8 = [](float v) -> uint8_t {
                int iv = static_cast<int>(std::lround(v));
                return static_cast<uint8_t>(iv < 0 ? 0 : (iv > 255 ? 255 : iv));
            };

            drow[out_i + 0] = clamp8(spx[0] * vignette + moire_b + glare + n);
            drow[out_i + 1] = clamp8(spx[1] * vignette + moire_g + glare + n);
            drow[out_i + 2] = clamp8(spx[2] * vignette + moire_r + glare + n);
            drow[out_i + 3] = 255;
        }
    }
}

} // namespace

ImageBuffer CorpusGenerator::render_sample_image(const CorpusSample& sample) {
    GdiPlusSession session;
    const int w = std::max(640, sample.width);
    const int h = std::max(420, sample.height);

    ImageBuffer out = ImageBuffer::create(w, h);
    {
        Gdiplus::Bitmap bmp(w, h, out.stride, PixelFormat32bppARGB, out.pixels.data());
        Gdiplus::Graphics g(&bmp);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

        const Gdiplus::Color desktop_bg = sample.dark_mode
            ? Gdiplus::Color(255, 18, 21, 30)
            : Gdiplus::Color(255, 228, 234, 242);
        const Gdiplus::Color card_bg = sample.dark_mode
            ? Gdiplus::Color(255, 28, 32, 46)
            : Gdiplus::Color(255, 252, 253, 255);
        const Gdiplus::Color header_bg = sample.dark_mode
            ? Gdiplus::Color(255, 38, 44, 64)
            : Gdiplus::Color(255, 238, 242, 249);
        const Gdiplus::Color border_col = sample.dark_mode
            ? Gdiplus::Color(255, 72, 82, 112)
            : Gdiplus::Color(255, 182, 192, 208);
        const Gdiplus::Color primary_fg = sample.dark_mode
            ? Gdiplus::Color(255, 242, 245, 252)
            : Gdiplus::Color(255, 22, 26, 36);
        const Gdiplus::Color code_fg = sample.dark_mode
            ? Gdiplus::Color(255, 255, 195, 195)
            : Gdiplus::Color(255, 175, 24, 24);
        const Gdiplus::Color meta_fg = sample.dark_mode
            ? Gdiplus::Color(255, 224, 232, 248)
            : Gdiplus::Color(255, 36, 44, 60);

        g.Clear(desktop_bg);

        const float pad_x = w * 0.06f;
        const float pad_y = h * 0.08f;
        const float card_w = w - 2.0f * pad_x;
        const float card_h = h - 2.0f * pad_y;
        const float header_h = std::max(42.0f, h * 0.075f);

        Gdiplus::SolidBrush card_brush(card_bg);
        Gdiplus::SolidBrush header_brush(header_bg);
        Gdiplus::Pen border_pen(border_col, 2.0f);

        g.FillRectangle(&card_brush, pad_x, pad_y, card_w, card_h);
        g.FillRectangle(&header_brush, pad_x, pad_y, card_w, header_h);
        g.DrawRectangle(&border_pen, pad_x, pad_y, card_w, card_h);

        // OS-specific window decorations
        if (sample.os_style == "macOS") {
            Gdiplus::SolidBrush red_b(Gdiplus::Color(255, 255, 95, 86));
            Gdiplus::SolidBrush yel_b(Gdiplus::Color(255, 255, 189, 46));
            Gdiplus::SolidBrush grn_b(Gdiplus::Color(255, 39, 201, 63));
            const float cy = pad_y + header_h * 0.34f;
            g.FillEllipse(&red_b, pad_x + 18.0f, cy, 14.0f, 14.0f);
            g.FillEllipse(&yel_b, pad_x + 40.0f, cy, 14.0f, 14.0f);
            g.FillEllipse(&grn_b, pad_x + 62.0f, cy, 14.0f, 14.0f);
        }

        const float scale = std::clamp(w / 1280.0f, 0.76f, 1.35f);
        Gdiplus::FontFamily font_ui(L"Segoe UI");
        Gdiplus::FontFamily font_mono(L"Consolas");
        const Gdiplus::FontFamily* body_family = (sample.os_style == "Linux") ? &font_mono : &font_ui;

        Gdiplus::Font f_title(&font_ui, 13.5f * scale, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
        Gdiplus::Font f_url(&font_mono, 12.5f * scale, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
        Gdiplus::Font f_code(body_family, 16.5f * scale, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
        Gdiplus::Font f_body(body_family, 14.0f * scale, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
        Gdiplus::Font f_meta(&font_mono, 13.5f * scale, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);

        Gdiplus::SolidBrush fg_brush(primary_fg);
        Gdiplus::SolidBrush code_brush(code_fg);
        Gdiplus::SolidBrush meta_brush(meta_fg);

        // Window Title
        const float title_x = (sample.os_style == "macOS") ? (pad_x + 96.0f) : (pad_x + 22.0f);
        const std::wstring w_title = utf8_to_wide(sample.rendered_window_title);
        g.DrawString(w_title.c_str(), -1, &f_title, Gdiplus::PointF(title_x, pad_y + header_h * 0.22f), &fg_brush);

        float cursor_y = pad_y + header_h + 22.0f * scale;
        const float content_x = pad_x + 32.0f * scale;
        const float content_w = card_w - 64.0f * scale;

        // Optional Browser / Endpoint URL Bar
        if (!sample.rendered_url_bar.empty()) {
            const float bar_h = 38.0f * scale;
            Gdiplus::SolidBrush url_bg(sample.dark_mode
                ? Gdiplus::Color(255, 20, 24, 35)
                : Gdiplus::Color(255, 241, 245, 250));
            g.FillRectangle(&url_bg, content_x, cursor_y, content_w, bar_h);
            g.DrawRectangle(&border_pen, content_x, cursor_y, content_w, bar_h);

            const std::wstring w_url = utf8_to_wide(sample.rendered_url_bar);
            g.DrawString(w_url.c_str(), -1, &f_url, Gdiplus::PointF(content_x + 12.0f, cursor_y + 7.0f * scale), &meta_brush);
            cursor_y += bar_h + 26.0f * scale;
        } else {
            cursor_y += 14.0f * scale;
        }

        // Error Code Headline
        {
            const std::wstring w_head = utf8_to_wide(sample.rendered_headline);
            g.DrawString(w_head.c_str(), -1, &f_code, Gdiplus::PointF(content_x, cursor_y), &code_brush);
            cursor_y += 46.0f * scale;
        }

        // Main Diagnostic Message (wrapped cleanly inside card)
        {
            const std::wstring w_msg = utf8_to_wide(sample.rendered_body_line1);
            Gdiplus::RectF msg_rect(content_x, cursor_y, content_w, 125.0f * scale);
            g.DrawString(w_msg.c_str(), -1, &f_body, msg_rect, nullptr, &fg_brush);
            cursor_y += 118.0f * scale;
        }

        // Module Line
        if (!sample.rendered_body_line2.empty()) {
            const std::wstring w_mod = utf8_to_wide(sample.rendered_body_line2);
            g.DrawString(w_mod.c_str(), -1, &f_meta, Gdiplus::PointF(content_x, cursor_y), &meta_brush);
            cursor_y += 38.0f * scale;
        }

        // Version Line
        if (!sample.rendered_footer.empty()) {
            const std::wstring w_ver = utf8_to_wide(sample.rendered_footer);
            g.DrawString(w_ver.c_str(), -1, &f_meta, Gdiplus::PointF(content_x, cursor_y), &meta_brush);
            cursor_y += 42.0f * scale;
        }

        // Action Button Bar at Bottom-Right (authentic UI chrome that EntityExtractor must ignore!)
        {
            const float btn_w = 118.0f * scale;
            const float btn_h = 36.0f * scale;
            const float btn_x = pad_x + card_w - btn_w - 32.0f * scale;
            const float btn_y = pad_y + card_h - btn_h - 24.0f * scale;
            Gdiplus::SolidBrush btn_brush(Gdiplus::Color(255, 37, 99, 235));
            Gdiplus::SolidBrush white_brush(Gdiplus::Color(255, 255, 255, 255));
            g.FillRectangle(&btn_brush, btn_x, btn_y, btn_w, btn_h);
            const wchar_t* btn_txt = (sample.language == "ES") ? L"Aceptar" : L"OK";
            g.DrawString(btn_txt, -1, &f_title, Gdiplus::PointF(btn_x + 28.0f * scale, btn_y + 6.0f * scale), &white_brush);
        }
    }

    if (sample.mobile_moire_photo) {
        uint32_t seed = 20261105u;
        for (unsigned char c : sample.sample_id) seed = seed * 131u + c;
        apply_mobile_camera_moire_simulation(out, seed);
    }

    return out;
}

bool CorpusGenerator::generate_corpus_on_disk(
    const std::filesystem::path& output_dir,
    std::vector<CorpusSample>* out_samples,
    std::string* error_out) {
    std::error_code ec;
    std::filesystem::create_directories(output_dir, ec);

    const std::vector<CorpusSample> samples = build_default_corpus_spec();
    JsonArray manifest_arr;

    for (const auto& s : samples) {
        const ImageBuffer img = render_sample_image(s);
        const std::filesystem::path img_path = output_dir / utf8_to_wide(s.relative_image_path);
        const int jpeg_q = s.mobile_moire_photo ? 84 : 95;
        if (!ImageIO::save_to_file(img, img_path, jpeg_q, error_out)) {
            return false;
        }

        JsonObject obj;
        obj["sample_id"] = s.sample_id;
        obj["relative_image_path"] = s.relative_image_path;
        obj["os_style"] = s.os_style;
        obj["language"] = s.language;
        obj["width"] = s.width;
        obj["height"] = s.height;
        obj["dark_mode"] = s.dark_mode;
        obj["mobile_moire_photo"] = s.mobile_moire_photo;
        obj["gt_error_code"] = s.gt_error_code;
        obj["gt_module"] = s.gt_module;
        obj["gt_version"] = s.gt_version;
        obj["gt_sanitized_url"] = s.gt_sanitized_url;
        obj["gt_error_message"] = s.gt_error_message;
        obj["support_team_label"] = category_to_code(s.gt_category);
        obj["gt_has_complete_info"] = s.gt_has_complete_info;

        JsonArray sec_arr;
        for (const auto& sec : s.planted_secrets) sec_arr.emplace_back(sec);
        obj["planted_secrets"] = std::move(sec_arr);

        manifest_arr.emplace_back(std::move(obj));
    }

    JsonObject root;
    root["corpus_name"] = "TicketOCR Anonymized Multi-Platform Support Benchmark Corpus v1.0";
    root["total_samples"] = samples.size();
    root["samples"] = std::move(manifest_arr);

    if (!write_text_file(output_dir / "manifest.json", serialize_json(root, 2))) {
        if (error_out) *error_out = "No se pudo escribir manifest.json en el directorio del corpus.";
        return false;
    }

    if (out_samples) *out_samples = samples;
    return true;
}

std::vector<CorpusSample> CorpusGenerator::load_manifest(const std::filesystem::path& manifest_path) {
    const std::string content = read_text_file(manifest_path);
    if (content.empty()) return {};

    const JsonValue root = parse_json(content);
    const JsonValue* arr_node = root.find("samples");
    if (!arr_node || !arr_node->is_array()) return {};

    std::vector<CorpusSample> out;
    for (const auto& item : arr_node->as_array()) {
        if (!item.is_object()) continue;
        CorpusSample s;
        if (const auto* p = item.find("sample_id")) s.sample_id = p->as_string();
        if (const auto* p = item.find("relative_image_path")) s.relative_image_path = p->as_string();
        if (const auto* p = item.find("os_style")) s.os_style = p->as_string();
        if (const auto* p = item.find("language")) s.language = p->as_string();
        if (const auto* p = item.find("width")) s.width = p->as_int(1280);
        if (const auto* p = item.find("height")) s.height = p->as_int(720);
        if (const auto* p = item.find("dark_mode")) s.dark_mode = p->as_bool(false);
        if (const auto* p = item.find("mobile_moire_photo")) s.mobile_moire_photo = p->as_bool(false);
        if (const auto* p = item.find("gt_error_code")) s.gt_error_code = p->as_string();
        if (const auto* p = item.find("gt_module")) s.gt_module = p->as_string();
        if (const auto* p = item.find("gt_version")) s.gt_version = p->as_string();
        if (const auto* p = item.find("gt_sanitized_url")) s.gt_sanitized_url = p->as_string();
        if (const auto* p = item.find("gt_error_message")) s.gt_error_message = p->as_string();
        if (const auto* p = item.find("support_team_label")) {
            if (auto cat = category_from_code(p->as_string())) s.gt_category = *cat;
        }
        if (const auto* p = item.find("gt_has_complete_info")) s.gt_has_complete_info = p->as_bool(true);
        if (const auto* p = item.find("planted_secrets")) {
            for (const auto& sec : p->as_array()) s.planted_secrets.push_back(sec.as_string());
        }
        if (!s.sample_id.empty()) out.push_back(std::move(s));
    }
    return out;
}

} // namespace ticket_ocr

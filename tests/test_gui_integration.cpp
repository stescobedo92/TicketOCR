#include "tests/test_framework.hpp"
#include "ticket_ocr/corpus_generator.hpp"
#include "ticket_ocr/dlp_redactor.hpp"
#include "ticket_ocr/gui_app.hpp"
#include "ticket_ocr/json_utils.hpp"

#include <Windows.h>
#include <objbase.h>
#include <thread>

using namespace ticket_ocr;

namespace {

bool g_visual_mode = false;

void pump_pending_messages() {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) break;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

} // namespace

TEST_CASE(GuiDefaultEnglishI18nThemeSwitchingAndZeroLeakExport) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    const std::filesystem::path root(TICKET_OCR_SOURCE_DIR);
    const std::filesystem::path corpus_dir = root / "assets" / "corpus";
    if (!std::filesystem::exists(corpus_dir / "manifest.json")) {
        REQUIRE_TRUE(CorpusGenerator::generate_corpus_on_disk(corpus_dir));
    }

    // 1. Verify English is the default language and Light is the default theme
    GuiMainWindow gui(root);
    REQUIRE_TRUE(gui.create(GetModuleHandleW(nullptr), g_visual_mode ? SW_SHOWDEFAULT : SW_HIDE));
    pump_pending_messages();

    const GuiControlSnapshot snap_init = gui.capture_control_snapshot();
    REQUIRE_TRUE(snap_init.active_locale == Locale::English);
    REQUIRE_TRUE(snap_init.active_theme == UiTheme::Light);
    REQUIRE_EQ(snap_init.open_button_text, std::string("Load Screenshot..."));

    // 2. Load hardest combined scenario: WebSaaS + ES + Dark Mode + Mobile Moire + Card PAN + sk-live API Key
    const std::filesystem::path img_path = corpus_dir / "S11_WEB_ES_PAY_DARK_MOIRE.jpg";
    REQUIRE_TRUE(gui.load_and_analyze_file(img_path));
    pump_pending_messages();
    if (g_visual_mode) std::this_thread::sleep_for(std::chrono::milliseconds(600));

    const GuiControlSnapshot snap1 = gui.capture_control_snapshot();
    REQUIRE_EQ(snap1.error_code, std::string("ERR-PAY-4021"));
    REQUIRE_EQ(snap1.module_name, std::string("PaymentGateway.ChargeProcessor"));
    REQUIRE_EQ(snap1.version, std::string("v4.9.5"));
    REQUIRE_TRUE(snap1.url.find("checkout.pagos-seguros.com") != std::string::npos);
    REQUIRE_GE(snap1.kb_row_count, static_cast<size_t>(1));
    REQUIRE_TRUE(!snap1.showing_preprocessed);
    REQUIRE_TRUE(snap1.description.find("=== AUTOMATED INCIDENT SUMMARY ===") != std::string::npos);

    // 3. Test runtime i18n switch to Spanish and Dark Theme via GUI API
    gui.set_language(Locale::Spanish);
    gui.set_theme(UiTheme::Dark);
    pump_pending_messages();
    if (g_visual_mode) std::this_thread::sleep_for(std::chrono::milliseconds(600));

    const GuiControlSnapshot snap_es_dark = gui.capture_control_snapshot();
    REQUIRE_TRUE(snap_es_dark.active_locale == Locale::Spanish);
    REQUIRE_TRUE(snap_es_dark.active_theme == UiTheme::Dark);
    REQUIRE_EQ(snap_es_dark.open_button_text, std::string("Cargar Captura..."));
    REQUIRE_EQ(snap_es_dark.error_code, std::string("ERR-PAY-4021"));
    REQUIRE_TRUE(snap_es_dark.description.find("=== RESUMEN AUTOMATIZADO DEL INCIDENTE ===") != std::string::npos);

    // Switch back to English (Default) while keeping Dark Theme
    gui.set_language(Locale::English);
    pump_pending_messages();
    const GuiControlSnapshot snap_en_dark = gui.capture_control_snapshot();
    REQUIRE_TRUE(snap_en_dark.active_locale == Locale::English);
    REQUIRE_TRUE(snap_en_dark.active_theme == UiTheme::Dark);
    REQUIRE_EQ(snap_en_dark.open_button_text, std::string("Load Screenshot..."));

    // 4. Toggle to Preprocessed Anti-Moire View and verify GUI state updates
    gui.toggle_preprocessed_view();
    pump_pending_messages();
    if (g_visual_mode) std::this_thread::sleep_for(std::chrono::milliseconds(600));

    const GuiControlSnapshot snap2 = gui.capture_control_snapshot();
    REQUIRE_TRUE(snap2.showing_preprocessed);

    // 5. Export JSON and Markdown from GUI controls and verify 0 sensitive leaks
    const std::filesystem::path tmp_json = std::filesystem::temp_directory_path() / "gui_exported_ticket.json";
    const std::filesystem::path tmp_md = std::filesystem::temp_directory_path() / "gui_exported_ticket.md";
    REQUIRE_TRUE(gui.export_ticket_json_to(tmp_json));
    REQUIRE_TRUE(gui.export_ticket_markdown_to(tmp_md));

    const std::string exported_json = read_text_file(tmp_json);
    const std::string exported_md = read_text_file(tmp_md);
    REQUIRE_TRUE(!exported_json.empty());
    REQUIRE_TRUE(!exported_md.empty());

    const auto leaks = DlpRedactor::audit_for_leaks(
        exported_json + "\n" + exported_md,
        {"4532-0151-1283-0366", "sk-live-94a8f72c10b3e6d549f8a1"}
    );
    REQUIRE_EQ(leaks.size(), static_cast<size_t>(0));

    std::error_code ec;
    std::filesystem::remove(tmp_json, ec);
    std::filesystem::remove(tmp_md, ec);

    gui.close_window();
    pump_pending_messages();
    CoUninitialize();
}

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--visual") {
            g_visual_mode = true;
        }
    }
    return test_fw::run_all_tests("GUI Integration (Default English i18n, Dark/Light Theme, Anti-Moire & Zero-Leak Export)");
}

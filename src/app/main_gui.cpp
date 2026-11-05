#include "ticket_ocr/gui_app.hpp"
#include "ticket_ocr/json_utils.hpp"

#include <Windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

namespace {

std::filesystem::path resolve_workspace_root() {
    wchar_t exe_buf[MAX_PATH] = L"";
    std::filesystem::path exe_dir;
    if (GetModuleFileNameW(nullptr, exe_buf, MAX_PATH) > 0) {
        exe_dir = std::filesystem::path(exe_buf).parent_path();
    }

    const std::filesystem::path cwd = std::filesystem::current_path();
    if (!exe_dir.empty() &&
        (std::filesystem::exists(exe_dir / "locales") || std::filesystem::exists(exe_dir / "data" / "knowledge_base.json"))) {
        return exe_dir;
    }
    if (std::filesystem::exists(cwd / "locales") || std::filesystem::exists(cwd / "CMakeLists.txt")) {
        return cwd;
    }
    if (std::filesystem::exists(std::filesystem::path(TICKET_OCR_SOURCE_DIR))) {
        return std::filesystem::path(TICKET_OCR_SOURCE_DIR);
    }
    return exe_dir.empty() ? cwd : exe_dir;
}

void pump_messages_for_ms(HWND hwnd, int duration_ms) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return;
            if (!IsDialogMessageW(hwnd, &msg)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    const std::filesystem::path root = resolve_workspace_root();

    ticket_ocr::Locale init_locale = ticket_ocr::Locale::English; // English by default
    ticket_ocr::UiTheme init_theme = ticket_ocr::UiTheme::Light;
    std::filesystem::path preload_image;
    std::filesystem::path screenshot_out;

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            const std::wstring warg = argv[i];
            if (warg == L"--dark") {
                init_theme = ticket_ocr::UiTheme::Dark;
            } else if (warg == L"--light") {
                init_theme = ticket_ocr::UiTheme::Light;
            } else if (warg == L"--lang" && i + 1 < argc) {
                const std::wstring wlang = argv[++i];
                if (wlang == L"es" || wlang == L"ES") {
                    init_locale = ticket_ocr::Locale::Spanish;
                } else {
                    init_locale = ticket_ocr::Locale::English;
                }
            } else if (warg == L"--screenshot" && i + 1 < argc) {
                screenshot_out = argv[++i];
            } else if (!warg.empty() && warg[0] != L'-' && std::filesystem::exists(warg)) {
                preload_image = warg;
            }
        }
        LocalFree(argv);
    }

    ticket_ocr::GuiMainWindow app(root, init_locale, init_theme);
    const int show_mode = !screenshot_out.empty() ? SW_SHOW : nCmdShow;
    if (!app.create(hInstance, show_mode)) {
        CoUninitialize();
        return 1;
    }

    if (!preload_image.empty()) {
        app.load_and_analyze_file(preload_image);
    }

    if (!screenshot_out.empty()) {
        SetForegroundWindow(app.hwnd());
        RedrawWindow(app.hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
        UpdateWindow(app.hwnd());
        pump_messages_for_ms(app.hwnd(), 450);
        RedrawWindow(app.hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
        pump_messages_for_ms(app.hwnd(), 150);

        const bool ok = app.capture_window_screenshot(screenshot_out);
        app.close_window();
        pump_messages_for_ms(nullptr, 50);
        CoUninitialize();
        return ok ? 0 : 2;
    }

    const int code = app.run_message_loop();
    CoUninitialize();
    return code;
}

#include "ticket_ocr/gui_app.hpp"
#include "ticket_ocr/metrics_evaluator.hpp"
#include "ticket_ocr/resource.h"

#include <Windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <objidl.h>
#include <algorithm>
using std::max;
using std::min;
#include <gdiplus.h>

#include <cstring>
#include <cwchar>
#include <iomanip>
#include <sstream>

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

namespace ticket_ocr {

namespace {

constexpr int IDC_BTN_OPEN_IMAGE     = 1001;
constexpr int IDC_COMBO_CORPUS       = 1002;
constexpr int IDC_BTN_TOGGLE_VIEW    = 1003;
constexpr int IDC_CHK_FORCE_MOIRE    = 1004;
constexpr int IDC_BTN_REANALYZE      = 1005;
constexpr int IDC_BTN_RUN_BENCHMARK  = 1006;

constexpr int IDC_EDIT_TITLE         = 1101;
constexpr int IDC_COMBO_CATEGORY     = 1102;
constexpr int IDC_COMBO_PRIORITY     = 1103;
constexpr int IDC_EDIT_QUEUE         = 1104;
constexpr int IDC_EDIT_CODE          = 1105;
constexpr int IDC_EDIT_MODULE        = 1106;
constexpr int IDC_EDIT_VERSION       = 1107;
constexpr int IDC_EDIT_URL           = 1108;
constexpr int IDC_LBL_COMPLETENESS   = 1109;
constexpr int IDC_LBL_DLP_STATUS     = 1110;
constexpr int IDC_EDIT_DESCRIPTION   = 1111;
constexpr int IDC_LIST_KB            = 1112;
constexpr int IDC_EDIT_KB_STEPS      = 1113;

constexpr int IDC_BTN_COPY_CLIP      = 1201;
constexpr int IDC_BTN_EXPORT_JSON    = 1202;
constexpr int IDC_BTN_EXPORT_MD      = 1203;
constexpr int IDC_STATUS_BAR         = 1301;

// Menu Bar Command IDs
constexpr int IDM_FILE_OPEN          = 2001;
constexpr int IDM_FILE_EXPORT_JSON   = 2002;
constexpr int IDM_FILE_EXPORT_MD     = 2003;
constexpr int IDM_FILE_EXIT          = 2004;
constexpr int IDM_VIEW_TOGGLE_OCR    = 2101;
constexpr int IDM_VIEW_THEME_LIGHT   = 2102;
constexpr int IDM_VIEW_THEME_DARK    = 2103;
constexpr int IDM_LANG_ENGLISH       = 2201;
constexpr int IDM_LANG_SPANISH       = 2202;
constexpr int IDM_TOOLS_COPY         = 2301;
constexpr int IDM_TOOLS_BENCHMARK    = 2302;

std::string lf_to_crlf(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 32);
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\n' && (i == 0 || s[i - 1] != '\r')) {
            out += "\r\n";
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

std::string crlf_to_lf(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c != '\r') out.push_back(c);
    }
    return out;
}

std::string get_hwnd_text_utf8(HWND h) {
    if (!h) return {};
    const int len = GetWindowTextLengthW(h);
    if (len <= 0) return {};
    std::wstring buf(static_cast<size_t>(len) + 1, L'\0');
    GetWindowTextW(h, buf.data(), len + 1);
    buf.resize(static_cast<size_t>(len));
    return wide_to_utf8(buf);
}

void set_hwnd_text_utf8(HWND h, std::string_view text) {
    if (!h) return;
    const std::wstring w = utf8_to_wide(text);
    SetWindowTextW(h, w.c_str());
}

} // namespace

struct GuiMainWindow::Impl {
    HFONT font_regular = nullptr;
    HFONT font_bold = nullptr;
    HFONT font_header = nullptr;
    HFONT font_mono = nullptr;

    HBRUSH light_bg_brush = nullptr;
    HBRUSH light_input_brush = nullptr;
    HBRUSH dark_bg_brush = nullptr;
    HBRUSH dark_input_brush = nullptr;
    HICON app_icon_big = nullptr;
    HICON app_icon_small = nullptr;

    HMENU menu_bar = nullptr;
    RECT canvas_rect{20, 100, 620, 740};
    std::filesystem::path current_file_path;

    HWND btn_open = nullptr;
    HWND lbl_corpus = nullptr;
    HWND combo_corpus = nullptr;
    HWND btn_toggle_view = nullptr;
    HWND chk_force_moire = nullptr;
    HWND btn_reanalyze = nullptr;
    HWND btn_benchmark = nullptr;
    HWND lbl_diag_strip = nullptr;

    HWND lbl_sec_ticket = nullptr;
    HWND lbl_title = nullptr;      HWND edit_title = nullptr;
    HWND lbl_cat = nullptr;        HWND combo_cat = nullptr;
    HWND lbl_prio = nullptr;       HWND combo_prio = nullptr;
    HWND lbl_queue = nullptr;      HWND edit_queue = nullptr;
    HWND lbl_code = nullptr;       HWND edit_code = nullptr;
    HWND lbl_mod = nullptr;        HWND edit_mod = nullptr;
    HWND lbl_ver = nullptr;        HWND edit_ver = nullptr;
    HWND lbl_url = nullptr;        HWND edit_url = nullptr;
    HWND lbl_complete = nullptr;
    HWND lbl_dlp = nullptr;
    HWND lbl_desc = nullptr;       HWND edit_desc = nullptr;
    HWND lbl_kb = nullptr;         HWND list_kb = nullptr;
    HWND edit_kb_steps = nullptr;

    HWND btn_copy = nullptr;
    HWND btn_exp_json = nullptr;
    HWND btn_exp_md = nullptr;
    HWND status_bar = nullptr;

    ~Impl() {
        if (font_regular) DeleteObject(font_regular);
        if (font_bold) DeleteObject(font_bold);
        if (font_header) DeleteObject(font_header);
        if (font_mono) DeleteObject(font_mono);
        if (light_bg_brush) DeleteObject(light_bg_brush);
        if (light_input_brush) DeleteObject(light_input_brush);
        if (dark_bg_brush) DeleteObject(dark_bg_brush);
        if (dark_input_brush) DeleteObject(dark_input_brush);
        if (app_icon_big) DestroyIcon(app_icon_big);
        if (app_icon_small) DestroyIcon(app_icon_small);
    }
};

GuiMainWindow::GuiMainWindow(
    std::filesystem::path workspace_root,
    Locale initial_locale,
    UiTheme initial_theme)
    : gdi_session_(std::make_unique<GdiPlusSession>()),
      workspace_root_(std::move(workspace_root)),
      corpus_dir_(workspace_root_ / "assets" / "corpus"),
      i18n_(initial_locale),
      theme_(initial_theme),
      impl_(std::make_unique<Impl>()) {
    const std::filesystem::path loc_dir = workspace_root_ / "locales";
    if (std::filesystem::exists(loc_dir)) {
        i18n_.load_locales_dir(loc_dir);
    }
}

GuiMainWindow::~GuiMainWindow() {
    if (hwnd_ && IsWindow(hwnd_)) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void GuiMainWindow::ensure_corpus_ready() {
    const std::filesystem::path manifest = corpus_dir_ / "manifest.json";
    if (!std::filesystem::exists(manifest)) {
        CorpusGenerator::generate_corpus_on_disk(corpus_dir_, &corpus_samples_);
    } else if (corpus_samples_.empty()) {
        corpus_samples_ = CorpusGenerator::load_manifest(manifest);
    }
}

bool GuiMainWindow::create(HINSTANCE hInstance, int nCmdShow) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    const int big_cx = GetSystemMetrics(SM_CXICON);
    const int big_cy = GetSystemMetrics(SM_CYICON);
    const int sm_cx  = GetSystemMetrics(SM_CXSMICON);
    const int sm_cy  = GetSystemMetrics(SM_CYSMICON);

    impl_->app_icon_big = static_cast<HICON>(LoadImageW(
        hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, big_cx, big_cy, LR_DEFAULTCOLOR));
    impl_->app_icon_small = static_cast<HICON>(LoadImageW(
        hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, sm_cx, sm_cy, LR_DEFAULTCOLOR));

    const std::filesystem::path ico_file = workspace_root_ / "assets" / "icons" / "ticket_ocr.ico";
    if (!impl_->app_icon_big && std::filesystem::exists(ico_file)) {
        impl_->app_icon_big = static_cast<HICON>(LoadImageW(
            nullptr, ico_file.c_str(), IMAGE_ICON, big_cx, big_cy, LR_LOADFROMFILE));
    }
    if (!impl_->app_icon_small && std::filesystem::exists(ico_file)) {
        impl_->app_icon_small = static_cast<HICON>(LoadImageW(
            nullptr, ico_file.c_str(), IMAGE_ICON, sm_cx, sm_cy, LR_LOADFROMFILE));
    }

    const wchar_t* kClassName = L"TicketOcrMainWindowClass";
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &GuiMainWindow::WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = impl_->app_icon_big ? impl_->app_icon_big : LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = impl_->app_icon_small ? impl_->app_icon_small : LoadIcon(nullptr, IDI_APPLICATION);
    wc.hbrBackground = nullptr; // Custom handled in WM_ERASEBKGND for smooth Light/Dark switching
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    const std::wstring wtitle = utf8_to_wide(i18n_.tr("app.window_title"));
    hwnd_ = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        kClassName,
        wtitle.c_str(),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1480, 930,
        nullptr, nullptr, hInstance, this
    );

    if (!hwnd_) return false;

    if (impl_->app_icon_big) {
        SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(impl_->app_icon_big));
    }
    if (impl_->app_icon_small) {
        SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(impl_->app_icon_small));
    }

    rebuild_menu_bar();
    apply_i18n_texts();
    apply_theme_styles();

    ShowWindow(hwnd_, nCmdShow);
    UpdateWindow(hwnd_);
    return true;
}

int GuiMainWindow::run_message_loop() {
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd_, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return static_cast<int>(msg.wParam);
}

void GuiMainWindow::close_window() {
    if (hwnd_ && IsWindow(hwnd_)) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void GuiMainWindow::rebuild_menu_bar() {
    if (!hwnd_) return;

    HMENU bar = CreateMenu();
    HMENU m_file = CreatePopupMenu();
    HMENU m_view = CreatePopupMenu();
    HMENU m_lang = CreatePopupMenu();
    HMENU m_tools = CreatePopupMenu();

    AppendMenuW(m_file, MF_STRING, IDM_FILE_OPEN, utf8_to_wide(i18n_.tr("menu.file.open")).c_str());
    AppendMenuW(m_file, MF_STRING, IDM_FILE_EXPORT_JSON, utf8_to_wide(i18n_.tr("menu.file.export_json")).c_str());
    AppendMenuW(m_file, MF_STRING, IDM_FILE_EXPORT_MD, utf8_to_wide(i18n_.tr("menu.file.export_md")).c_str());
    AppendMenuW(m_file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_file, MF_STRING, IDM_FILE_EXIT, utf8_to_wide(i18n_.tr("menu.file.exit")).c_str());

    AppendMenuW(m_view, MF_STRING, IDM_VIEW_TOGGLE_OCR, utf8_to_wide(i18n_.tr("menu.view.toggle_ocr")).c_str());
    AppendMenuW(m_view, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_view, MF_STRING, IDM_VIEW_THEME_LIGHT, utf8_to_wide(i18n_.tr("menu.view.theme_light")).c_str());
    AppendMenuW(m_view, MF_STRING, IDM_VIEW_THEME_DARK, utf8_to_wide(i18n_.tr("menu.view.theme_dark")).c_str());
    CheckMenuRadioItem(
        m_view,
        IDM_VIEW_THEME_LIGHT,
        IDM_VIEW_THEME_DARK,
        (theme_ == UiTheme::Dark) ? IDM_VIEW_THEME_DARK : IDM_VIEW_THEME_LIGHT,
        MF_BYCOMMAND
    );

    AppendMenuW(m_lang, MF_STRING, IDM_LANG_ENGLISH, utf8_to_wide(i18n_.tr("menu.language.en")).c_str());
    AppendMenuW(m_lang, MF_STRING, IDM_LANG_SPANISH, utf8_to_wide(i18n_.tr("menu.language.es")).c_str());
    CheckMenuRadioItem(
        m_lang,
        IDM_LANG_ENGLISH,
        IDM_LANG_SPANISH,
        (i18n_.locale() == Locale::Spanish) ? IDM_LANG_SPANISH : IDM_LANG_ENGLISH,
        MF_BYCOMMAND
    );

    AppendMenuW(m_tools, MF_STRING, IDM_TOOLS_COPY, utf8_to_wide(i18n_.tr("menu.tools.copy")).c_str());
    AppendMenuW(m_tools, MF_STRING, IDM_TOOLS_BENCHMARK, utf8_to_wide(i18n_.tr("menu.tools.benchmark")).c_str());

    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(m_file), utf8_to_wide(i18n_.tr("menu.file")).c_str());
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(m_view), utf8_to_wide(i18n_.tr("menu.view")).c_str());
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(m_lang), utf8_to_wide(i18n_.tr("menu.language")).c_str());
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(m_tools), utf8_to_wide(i18n_.tr("menu.tools")).c_str());

    HMENU prev = GetMenu(hwnd_);
    SetMenu(hwnd_, bar);
    if (prev) DestroyMenu(prev);
    impl_->menu_bar = bar;
    DrawMenuBar(hwnd_);
}

void GuiMainWindow::set_language(Locale loc) {
    i18n_.set_locale(loc);
    if (!hwnd_) return;
    rebuild_menu_bar();
    apply_i18n_texts();
    if (current_result_) {
        populate_controls_from_result(*current_result_);
    }
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void GuiMainWindow::set_theme(UiTheme theme) {
    theme_ = theme;
    if (!hwnd_) return;
    rebuild_menu_bar();
    apply_theme_styles();
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void GuiMainWindow::apply_theme_styles() {
    if (!hwnd_) return;

    const BOOL dark_mode = (theme_ == UiTheme::Dark) ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark_mode, sizeof(dark_mode));

    const wchar_t* ctrl_theme = dark_mode ? L"DarkMode_Explorer" : L"Explorer";
    const wchar_t* cfd_theme = dark_mode ? L"DarkMode_CFD" : L"CFD";

    HWND themed_buttons[] = {
        impl_->btn_open, impl_->btn_toggle_view, impl_->btn_reanalyze,
        impl_->btn_benchmark, impl_->btn_copy, impl_->btn_exp_json, impl_->btn_exp_md
    };
    for (HWND b : themed_buttons) {
        if (b) SetWindowTheme(b, ctrl_theme, nullptr);
    }

    HWND themed_combos[] = {impl_->combo_corpus, impl_->combo_cat, impl_->combo_prio};
    for (HWND c : themed_combos) {
        if (c) SetWindowTheme(c, cfd_theme, nullptr);
    }

    HWND themed_edits[] = {
        impl_->edit_title, impl_->edit_queue, impl_->edit_code, impl_->edit_mod,
        impl_->edit_ver, impl_->edit_url, impl_->edit_desc, impl_->edit_kb_steps
    };
    for (HWND e : themed_edits) {
        if (e) SetWindowTheme(e, ctrl_theme, nullptr);
    }

    if (impl_->chk_force_moire) {
        SetWindowTheme(impl_->chk_force_moire, dark_mode ? L"" : L"Explorer", dark_mode ? L"" : nullptr);
    }

    if (impl_->list_kb) {
        SetWindowTheme(impl_->list_kb, ctrl_theme, nullptr);
        const COLORREF lv_bg = dark_mode ? RGB(30, 37, 54) : RGB(255, 255, 255);
        const COLORREF lv_fg = dark_mode ? RGB(241, 245, 249) : RGB(30, 41, 59);
        ListView_SetBkColor(impl_->list_kb, lv_bg);
        ListView_SetTextBkColor(impl_->list_kb, lv_bg);
        ListView_SetTextColor(impl_->list_kb, lv_fg);
    }
}

void GuiMainWindow::apply_i18n_texts() {
    if (!hwnd_) return;

    set_hwnd_text_utf8(hwnd_, i18n_.tr("app.window_title"));
    set_hwnd_text_utf8(impl_->btn_open, i18n_.tr("btn.open_image"));
    set_hwnd_text_utf8(impl_->lbl_corpus, i18n_.tr("lbl.corpus"));
    set_hwnd_text_utf8(impl_->btn_toggle_view,
        i18n_.tr(show_preprocessed_ ? "btn.view_preprocessed" : "btn.view_original"));
    set_hwnd_text_utf8(impl_->chk_force_moire, i18n_.tr("chk.force_moire"));
    set_hwnd_text_utf8(impl_->btn_reanalyze, i18n_.tr("btn.reanalyze"));
    set_hwnd_text_utf8(impl_->btn_benchmark, i18n_.tr("btn.benchmark"));

    // Preserve corpus ComboBox selection while updating placeholder text
    {
        const int prev_sel = static_cast<int>(SendMessageW(impl_->combo_corpus, CB_GETCURSEL, 0, 0));
        SendMessageW(impl_->combo_corpus, CB_RESETCONTENT, 0, 0);
        const std::wstring wph = utf8_to_wide(i18n_.tr("combo.corpus_placeholder"));
        SendMessageW(impl_->combo_corpus, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wph.c_str()));
        for (const auto& s : corpus_samples_) {
            std::ostringstream label;
            label << s.sample_id << " [" << s.os_style << "/" << s.language
                  << (s.dark_mode ? "/Dark" : "/Light")
                  << (s.mobile_moire_photo ? "/Moiré" : "") << "]";
            const std::wstring wl = utf8_to_wide(label.str());
            SendMessageW(impl_->combo_corpus, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wl.c_str()));
        }
        SendMessageW(impl_->combo_corpus, CB_SETCURSEL, prev_sel >= 0 ? prev_sel : 0, 0);
    }

    set_hwnd_text_utf8(impl_->lbl_sec_ticket, i18n_.tr("sec.ticket_header"));
    set_hwnd_text_utf8(impl_->lbl_title, i18n_.tr("lbl.title"));
    set_hwnd_text_utf8(impl_->lbl_cat, i18n_.tr("lbl.category"));
    set_hwnd_text_utf8(impl_->lbl_prio, i18n_.tr("lbl.priority"));
    set_hwnd_text_utf8(impl_->lbl_queue, i18n_.tr("lbl.queue"));
    set_hwnd_text_utf8(impl_->lbl_code, i18n_.tr("lbl.code"));
    set_hwnd_text_utf8(impl_->lbl_mod, i18n_.tr("lbl.module"));
    set_hwnd_text_utf8(impl_->lbl_ver, i18n_.tr("lbl.version"));
    set_hwnd_text_utf8(impl_->lbl_url, i18n_.tr("lbl.url"));
    set_hwnd_text_utf8(impl_->lbl_desc, i18n_.tr("lbl.description"));
    set_hwnd_text_utf8(impl_->lbl_kb, i18n_.tr("lbl.kb_header"));

    // Repopulate Category ComboBox in active locale
    {
        const int prev_cat = static_cast<int>(SendMessageW(impl_->combo_cat, CB_GETCURSEL, 0, 0));
        SendMessageW(impl_->combo_cat, CB_RESETCONTENT, 0, 0);
        for (int i = 0; i < static_cast<int>(IncidentCategory::Count); ++i) {
            const auto cat = static_cast<IncidentCategory>(i);
            const std::wstring wcat = utf8_to_wide(category_to_code(cat) + " — " + i18n_.category_name(cat));
            SendMessageW(impl_->combo_cat, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wcat.c_str()));
        }
        SendMessageW(impl_->combo_cat, CB_SETCURSEL, prev_cat >= 0 ? prev_cat : 0, 0);
    }

    // Repopulate Priority ComboBox in active locale
    {
        const int prev_prio = static_cast<int>(SendMessageW(impl_->combo_prio, CB_GETCURSEL, 0, 0));
        SendMessageW(impl_->combo_prio, CB_RESETCONTENT, 0, 0);
        for (auto p : {TicketPriority::P1_Critica, TicketPriority::P2_Alta, TicketPriority::P3_Media, TicketPriority::P4_Baja}) {
            const std::wstring wp = utf8_to_wide(i18n_.priority_name(p));
            SendMessageW(impl_->combo_prio, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wp.c_str()));
        }
        SendMessageW(impl_->combo_prio, CB_SETCURSEL, prev_prio >= 0 ? prev_prio : 2, 0);
    }

    // Update ListView column titles
    auto set_col_title = [&](int idx, std::string_view key) {
        const std::wstring wt = utf8_to_wide(i18n_.tr(key));
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT;
        col.pszText = const_cast<wchar_t*>(wt.c_str());
        ListView_SetColumn(impl_->list_kb, idx, &col);
    };
    set_col_title(0, "col.kb_id");
    set_col_title(1, "col.kb_rel");
    set_col_title(2, "col.kb_title");
    set_col_title(3, "col.kb_reason");

    set_hwnd_text_utf8(impl_->btn_copy, i18n_.tr("btn.copy"));
    set_hwnd_text_utf8(impl_->btn_exp_json, i18n_.tr("btn.export_json"));
    set_hwnd_text_utf8(impl_->btn_exp_md, i18n_.tr("btn.export_md"));

    if (!current_result_) {
        set_hwnd_text_utf8(impl_->lbl_diag_strip, i18n_.tr("diag.initial"));
        set_hwnd_text_utf8(impl_->lbl_complete, i18n_.tr("lbl.complete_pending"));
        set_hwnd_text_utf8(impl_->lbl_dlp, i18n_.tr("lbl.dlp_initial"));
        set_hwnd_text_utf8(impl_->edit_kb_steps, i18n_.tr("kb.placeholder"));
        const std::wstring wready = utf8_to_wide(i18n_.tr("status.ready"));
        SendMessageW(impl_->status_bar, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(wready.c_str()));
    }
}

LRESULT CALLBACK GuiMainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    GuiMainWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        const auto* cs = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        self = static_cast<GuiMainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<GuiMainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->handle_message(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void GuiMainWindow::create_child_controls() {
    const UINT dpi = GetDpiForWindow(hwnd_);
    auto scale_font = [dpi](int pt) { return -MulDiv(pt, static_cast<int>(dpi), 72); };

    impl_->font_regular = CreateFontW(scale_font(9), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    impl_->font_bold = CreateFontW(scale_font(9), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    impl_->font_header = CreateFontW(scale_font(11), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    impl_->font_mono = CreateFontW(scale_font(9), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");

    impl_->light_bg_brush    = CreateSolidBrush(RGB(244, 246, 250));
    impl_->light_input_brush = CreateSolidBrush(RGB(255, 255, 255));
    impl_->dark_bg_brush     = CreateSolidBrush(RGB(19, 23, 34));
    impl_->dark_input_brush  = CreateSolidBrush(RGB(30, 37, 54));

    HINSTANCE hInst = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd_, GWLP_HINSTANCE));

    auto mk_ctrl = [&](const wchar_t* cls, const wchar_t* txt, DWORD style, DWORD ex_style, int id, HFONT f) -> HWND {
        if (std::wcscmp(cls, L"STATIC") == 0) {
            style |= SS_NOPREFIX;
        }
        HWND h = CreateWindowExW(ex_style, cls, txt, WS_CHILD | WS_VISIBLE | style,
                                 0, 0, 10, 10, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), hInst, nullptr);
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(f), TRUE);
        return h;
    };

    ensure_corpus_ready();

    impl_->btn_open        = mk_ctrl(L"BUTTON", L"", BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_OPEN_IMAGE, impl_->font_bold);
    impl_->lbl_corpus      = mk_ctrl(L"STATIC", L"", SS_CENTERIMAGE, 0, 0, impl_->font_bold);
    impl_->combo_corpus    = mk_ctrl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, IDC_COMBO_CORPUS, impl_->font_regular);
    impl_->btn_toggle_view = mk_ctrl(L"BUTTON", L"", BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_TOGGLE_VIEW, impl_->font_regular);
    impl_->chk_force_moire = mk_ctrl(L"BUTTON", L"", BS_AUTOCHECKBOX | WS_TABSTOP, 0, IDC_CHK_FORCE_MOIRE, impl_->font_regular);
    impl_->btn_reanalyze   = mk_ctrl(L"BUTTON", L"", BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_REANALYZE, impl_->font_regular);
    impl_->btn_benchmark   = mk_ctrl(L"BUTTON", L"", BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_RUN_BENCHMARK, impl_->font_bold);
    impl_->lbl_diag_strip  = mk_ctrl(L"STATIC", L"", SS_LEFTNOWORDWRAP | SS_PATHELLIPSIS, 0, 0, impl_->font_regular);

    impl_->lbl_sec_ticket = mk_ctrl(L"STATIC", L"", 0, 0, 0, impl_->font_header);
    impl_->lbl_title = mk_ctrl(L"STATIC", L"", 0, 0, 0, impl_->font_bold);
    impl_->edit_title = mk_ctrl(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_EDIT_TITLE, impl_->font_regular);

    impl_->lbl_cat = mk_ctrl(L"STATIC", L"", 0, 0, 0, impl_->font_bold);
    impl_->combo_cat = mk_ctrl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, IDC_COMBO_CATEGORY, impl_->font_regular);

    impl_->lbl_prio = mk_ctrl(L"STATIC", L"", 0, 0, 0, impl_->font_bold);
    impl_->combo_prio = mk_ctrl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 0, IDC_COMBO_PRIORITY, impl_->font_regular);

    impl_->lbl_queue = mk_ctrl(L"STATIC", L"", 0, 0, 0, impl_->font_bold);
    impl_->edit_queue = mk_ctrl(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_EDIT_QUEUE, impl_->font_mono);

    impl_->lbl_code = mk_ctrl(L"STATIC", L"", 0, 0, 0, impl_->font_bold);
    impl_->edit_code = mk_ctrl(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_EDIT_CODE, impl_->font_mono);

    impl_->lbl_mod = mk_ctrl(L"STATIC", L"", 0, 0, 0, impl_->font_bold);
    impl_->edit_mod = mk_ctrl(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_EDIT_MODULE, impl_->font_mono);

    impl_->lbl_ver = mk_ctrl(L"STATIC", L"", 0, 0, 0, impl_->font_bold);
    impl_->edit_ver = mk_ctrl(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_EDIT_VERSION, impl_->font_mono);

    impl_->lbl_url = mk_ctrl(L"STATIC", L"", 0, 0, 0, impl_->font_bold);
    impl_->edit_url = mk_ctrl(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_EDIT_URL, impl_->font_mono);

    impl_->lbl_complete = mk_ctrl(L"STATIC", L"", 0, 0, IDC_LBL_COMPLETENESS, impl_->font_bold);
    impl_->lbl_dlp = mk_ctrl(L"STATIC", L"", 0, 0, IDC_LBL_DLP_STATUS, impl_->font_bold);

    impl_->lbl_desc = mk_ctrl(L"STATIC", L"", 0, 0, 0, impl_->font_bold);
    impl_->edit_desc = mk_ctrl(L"EDIT", L"",
        ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_WANTRETURN | WS_TABSTOP,
        WS_EX_CLIENTEDGE, IDC_EDIT_DESCRIPTION, impl_->font_mono);

    impl_->lbl_kb = mk_ctrl(L"STATIC", L"", 0, 0, 0, impl_->font_bold);
    impl_->list_kb = mk_ctrl(WC_LISTVIEWW, L"",
        LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP,
        WS_EX_CLIENTEDGE, IDC_LIST_KB, impl_->font_regular);
    ListView_SetExtendedListViewStyle(impl_->list_kb, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    auto add_col = [&](int idx, const wchar_t* title, int width) {
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        col.pszText = const_cast<wchar_t*>(title);
        col.cx = width;
        col.iSubItem = idx;
        ListView_InsertColumn(impl_->list_kb, idx, &col);
    };
    add_col(0, L"KB ID", 85);
    add_col(1, L"Relevance", 85);
    add_col(2, L"Article Title", 330);
    add_col(3, L"Match Signal", 190);

    impl_->edit_kb_steps = mk_ctrl(L"EDIT", L"",
        ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
        WS_EX_CLIENTEDGE, IDC_EDIT_KB_STEPS, impl_->font_regular);

    impl_->btn_copy     = mk_ctrl(L"BUTTON", L"", BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_COPY_CLIP, impl_->font_bold);
    impl_->btn_exp_json = mk_ctrl(L"BUTTON", L"", BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_EXPORT_JSON, impl_->font_regular);
    impl_->btn_exp_md   = mk_ctrl(L"BUTTON", L"", BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_EXPORT_MD, impl_->font_regular);

    impl_->status_bar = CreateWindowExW(0, STATUSCLASSNAMEW, L"",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0,
        hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_STATUS_BAR)), hInst, nullptr);
}

void GuiMainWindow::layout_controls(int client_w, int client_h) {
    if (!impl_->btn_open || client_w <= 100 || client_h <= 100) return;

    SendMessageW(impl_->status_bar, WM_SIZE, 0, 0);
    RECT sb_rc{};
    GetWindowRect(impl_->status_bar, &sb_rc);
    const int sb_h = sb_rc.bottom - sb_rc.top;

    const int margin = 14;
    const int top_y = 12;
    const int bar_h = 30;

    int x = margin;
    MoveWindow(impl_->btn_open, x, top_y, 150, bar_h, TRUE); x += 158;
    MoveWindow(impl_->lbl_corpus, x, top_y + 4, 118, 22, TRUE); x += 120;
    MoveWindow(impl_->combo_corpus, x, top_y + 2, 330, 320, TRUE); x += 338;
    MoveWindow(impl_->btn_toggle_view, x, top_y, 195, bar_h, TRUE); x += 203;
    MoveWindow(impl_->chk_force_moire, x, top_y + 4, 145, 22, TRUE); x += 150;
    MoveWindow(impl_->btn_reanalyze, x, top_y, 105, bar_h, TRUE); x += 113;
    MoveWindow(impl_->btn_benchmark, x, top_y, std::max(175, client_w - x - margin), bar_h, TRUE);

    const int content_top = top_y + bar_h + 12;
    const int usable_h = client_h - content_top - sb_h - margin;

    const int left_w = std::max(400, (client_w - 3 * margin) * 45 / 100);
    const int right_x = margin + left_w + margin;
    const int right_w = std::max(440, client_w - right_x - margin);

    MoveWindow(impl_->lbl_diag_strip, margin, content_top, left_w, 22, TRUE);
    impl_->canvas_rect = RECT{margin, content_top + 26, margin + left_w, content_top + usable_h};

    int ry = content_top;
    MoveWindow(impl_->lbl_sec_ticket, right_x, ry, right_w, 24, TRUE); ry += 28;

    MoveWindow(impl_->lbl_title, right_x, ry + 3, 130, 20, TRUE);
    MoveWindow(impl_->edit_title, right_x + 135, ry, right_w - 135, 25, TRUE); ry += 31;

    MoveWindow(impl_->lbl_cat, right_x, ry + 3, 130, 20, TRUE);
    MoveWindow(impl_->combo_cat, right_x + 135, ry, right_w - 135, 240, TRUE); ry += 31;

    const int half_w = (right_w - 16) / 2;
    MoveWindow(impl_->lbl_prio, right_x, ry + 3, 130, 20, TRUE);
    MoveWindow(impl_->combo_prio, right_x + 135, ry, half_w - 135, 180, TRUE);
    MoveWindow(impl_->lbl_queue, right_x + half_w + 16, ry + 3, 88, 20, TRUE);
    MoveWindow(impl_->edit_queue, right_x + half_w + 106, ry, right_w - (half_w + 106), 25, TRUE); ry += 31;

    MoveWindow(impl_->lbl_code, right_x, ry + 3, 130, 20, TRUE);
    MoveWindow(impl_->edit_code, right_x + 135, ry, half_w - 135, 25, TRUE);
    MoveWindow(impl_->lbl_ver, right_x + half_w + 16, ry + 3, 88, 20, TRUE);
    MoveWindow(impl_->edit_ver, right_x + half_w + 106, ry, right_w - (half_w + 106), 25, TRUE); ry += 31;

    MoveWindow(impl_->lbl_mod, right_x, ry + 3, 130, 20, TRUE);
    MoveWindow(impl_->edit_mod, right_x + 135, ry, right_w - 135, 25, TRUE); ry += 31;

    MoveWindow(impl_->lbl_url, right_x, ry + 3, 130, 20, TRUE);
    MoveWindow(impl_->edit_url, right_x + 135, ry, right_w - 135, 25, TRUE); ry += 32;

    MoveWindow(impl_->lbl_complete, right_x, ry, half_w, 20, TRUE);
    MoveWindow(impl_->lbl_dlp, right_x + half_w + 12, ry, right_w - half_w - 12, 20, TRUE); ry += 25;

    const int bottom_btns_h = 32;
    const int remaining_h = std::max(220, (content_top + usable_h) - ry - bottom_btns_h - 10);
    const int desc_h = remaining_h * 44 / 100;
    const int kb_list_h = remaining_h * 30 / 100;
    const int kb_steps_h = std::max(54, remaining_h - desc_h - kb_list_h - 46);

    MoveWindow(impl_->lbl_desc, right_x, ry, right_w, 20, TRUE); ry += 22;
    MoveWindow(impl_->edit_desc, right_x, ry, right_w, desc_h, TRUE); ry += desc_h + 8;

    MoveWindow(impl_->lbl_kb, right_x, ry, right_w, 20, TRUE); ry += 22;
    MoveWindow(impl_->list_kb, right_x, ry, right_w, kb_list_h, TRUE); ry += kb_list_h + 6;
    MoveWindow(impl_->edit_kb_steps, right_x, ry, right_w, kb_steps_h, TRUE); ry += kb_steps_h + 8;

    const int btn_w = (right_w - 20) / 3;
    MoveWindow(impl_->btn_copy, right_x, ry, btn_w, bottom_btns_h, TRUE);
    MoveWindow(impl_->btn_exp_json, right_x + btn_w + 10, ry, btn_w, bottom_btns_h, TRUE);
    MoveWindow(impl_->btn_exp_md, right_x + 2 * (btn_w + 10), ry, right_w - 2 * (btn_w + 10), bottom_btns_h, TRUE);
}

bool GuiMainWindow::load_and_analyze_file(const std::filesystem::path& image_path) {
    PreprocessorOptions opts;
    opts.force_anti_moire = (SendMessageW(impl_->chk_force_moire, BM_GETCHECK, 0, 0) == BST_CHECKED);

    TicketPipeline pipeline(opts);
    std::string err;
    auto res_opt = pipeline.process_file(image_path, &err);
    if (!res_opt) {
        const std::wstring werr = utf8_to_wide(i18n_.trf("status.error_load", {err}));
        SendMessageW(impl_->status_bar, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(werr.c_str()));
        return false;
    }

    impl_->current_file_path = image_path;
    current_result_ = std::move(res_opt);
    show_preprocessed_ = false;
    set_hwnd_text_utf8(impl_->btn_toggle_view, i18n_.tr("btn.view_original"));
    populate_controls_from_result(*current_result_);
    InvalidateRect(hwnd_, &impl_->canvas_rect, FALSE);
    return true;
}

void GuiMainWindow::toggle_preprocessed_view() {
    if (!current_result_) return;
    show_preprocessed_ = !show_preprocessed_;
    set_hwnd_text_utf8(impl_->btn_toggle_view,
        i18n_.tr(show_preprocessed_ ? "btn.view_preprocessed" : "btn.view_original"));
    InvalidateRect(hwnd_, &impl_->canvas_rect, FALSE);
}

void GuiMainWindow::populate_controls_from_result(const PipelineRunResult& res) {
    const TicketDraft& t = res.ticket;

    set_hwnd_text_utf8(impl_->edit_title, t.title);
    SendMessageW(impl_->combo_cat, CB_SETCURSEL, static_cast<WPARAM>(t.category), 0);
    SendMessageW(impl_->combo_prio, CB_SETCURSEL, static_cast<WPARAM>(t.priority), 0);
    set_hwnd_text_utf8(impl_->edit_queue, t.support_queue);
    set_hwnd_text_utf8(impl_->edit_code, t.fields.error_code);
    set_hwnd_text_utf8(impl_->edit_mod, t.fields.module_name);
    set_hwnd_text_utf8(impl_->edit_ver, t.fields.software_version);
    set_hwnd_text_utf8(impl_->edit_url, t.fields.sanitized_url);

    set_hwnd_text_utf8(impl_->lbl_complete,
        i18n_.tr(t.fields.actionable_without_followup ? "lbl.complete_yes" : "lbl.complete_no"));

    const std::string red_cnt = std::to_string(t.dlp_summary.total_redactions);
    set_hwnd_text_utf8(impl_->lbl_dlp, i18n_.trf("lbl.dlp_status", {red_cnt}));

    const std::string localized_desc = I18n::format_ticket_description(t, i18n_.locale());
    set_hwnd_text_utf8(impl_->edit_desc, lf_to_crlf(localized_desc));

    {
        const std::string w_str = std::to_string(t.image_diagnostics.original_width);
        const std::string h_str = std::to_string(t.image_diagnostics.original_height);
        const std::string dark_s = i18n_.tr(t.image_diagnostics.is_dark_mode ? "diag.dark_yes" : "diag.dark_no");
        const std::string moire_s = i18n_.tr(t.image_diagnostics.moire_detected ? "diag.moire_yes" : "diag.moire_no");
        char mscore_buf[16];
        std::snprintf(mscore_buf, sizeof(mscore_buf), "%.2f", t.image_diagnostics.moire_score);
        char ms_buf[16];
        std::snprintf(ms_buf, sizeof(ms_buf), "%.1f", t.total_processing_ms);

        set_hwnd_text_utf8(impl_->lbl_diag_strip,
            i18n_.trf("diag.summary", {w_str, h_str, dark_s, moire_s, mscore_buf, ms_buf}));
    }

    ListView_DeleteAllItems(impl_->list_kb);
    for (size_t i = 0; i < t.suggested_articles.size(); ++i) {
        const auto& sug = t.suggested_articles[i];
        const std::wstring wid = utf8_to_wide(sug.article.id);
        const std::wstring wrel = utf8_to_wide(std::to_string(static_cast<int>(sug.relevance_score * 100.0)) + "%");
        const std::wstring wtitle = utf8_to_wide(sug.article.title);
        const std::wstring wreason = utf8_to_wide(sug.match_reason);

        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(i);
        item.iSubItem = 0;
        item.pszText = const_cast<wchar_t*>(wid.c_str());
        ListView_InsertItem(impl_->list_kb, &item);
        ListView_SetItemText(impl_->list_kb, static_cast<int>(i), 1, const_cast<wchar_t*>(wrel.c_str()));
        ListView_SetItemText(impl_->list_kb, static_cast<int>(i), 2, const_cast<wchar_t*>(wtitle.c_str()));
        ListView_SetItemText(impl_->list_kb, static_cast<int>(i), 3, const_cast<wchar_t*>(wreason.c_str()));
    }

    if (!t.suggested_articles.empty()) {
        ListView_SetItemState(impl_->list_kb, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        on_kb_selection_changed(0);
    } else {
        set_hwnd_text_utf8(impl_->edit_kb_steps, i18n_.tr("kb.none"));
    }

    char ms_buf[16], min_buf[16];
    std::snprintf(ms_buf, sizeof(ms_buf), "%.1f", t.total_processing_ms);
    std::snprintf(min_buf, sizeof(min_buf), "%.2f", t.estimated_agent_minutes);
    const std::string cat_code = category_to_code(t.category);
    const std::string conf_str = std::to_string(static_cast<int>(t.category_confidence * 100.0));

    const std::wstring wsb = utf8_to_wide(
        i18n_.trf("status.ticket_done", {t.ticket_id, ms_buf, cat_code, conf_str, min_buf}));
    SendMessageW(impl_->status_bar, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(wsb.c_str()));
}

void GuiMainWindow::on_kb_selection_changed(int index) {
    if (!current_result_ || index < 0 ||
        static_cast<size_t>(index) >= current_result_->ticket.suggested_articles.size()) {
        return;
    }
    const auto& sug = current_result_->ticket.suggested_articles[static_cast<size_t>(index)];
    std::ostringstream oss;
    oss << i18n_.trf("kb.root_cause", {sug.article.id, sug.article.root_cause}) << "\n";
    for (size_t i = 0; i < sug.article.resolution_steps.size(); ++i) {
        oss << "  " << (i + 1) << ") " << sug.article.resolution_steps[i] << "\n";
    }
    set_hwnd_text_utf8(impl_->edit_kb_steps, lf_to_crlf(oss.str()));
}

TicketDraft GuiMainWindow::build_ticket_from_edited_controls() const {
    TicketDraft t = current_result_ ? current_result_->ticket : TicketDraft{};
    if (t.ticket_id.empty()) t.ticket_id = "INC-2026-MANUAL";

    DlpRedactor dlp;
    t.title = dlp.redact(get_hwnd_text_utf8(impl_->edit_title)).redacted_text;

    const int cat_sel = static_cast<int>(SendMessageW(impl_->combo_cat, CB_GETCURSEL, 0, 0));
    if (cat_sel >= 0 && cat_sel < static_cast<int>(IncidentCategory::Count)) {
        t.category = static_cast<IncidentCategory>(cat_sel);
    }

    const int prio_sel = static_cast<int>(SendMessageW(impl_->combo_prio, CB_GETCURSEL, 0, 0));
    if (prio_sel >= 0 && prio_sel <= 3) {
        t.priority = static_cast<TicketPriority>(prio_sel);
    }

    t.support_queue = get_hwnd_text_utf8(impl_->edit_queue);
    t.fields.error_code = dlp.redact(get_hwnd_text_utf8(impl_->edit_code)).redacted_text;
    t.fields.module_name = dlp.redact(get_hwnd_text_utf8(impl_->edit_mod)).redacted_text;
    t.fields.software_version = dlp.redact(get_hwnd_text_utf8(impl_->edit_ver)).redacted_text;
    t.fields.sanitized_url = dlp.redact(get_hwnd_text_utf8(impl_->edit_url)).redacted_text;
    t.formatted_description = dlp.redact(crlf_to_lf(get_hwnd_text_utf8(impl_->edit_desc))).redacted_text;
    return t;
}

bool GuiMainWindow::export_ticket_json_to(const std::filesystem::path& json_path) const {
    const TicketDraft t = build_ticket_from_edited_controls();
    return write_text_file(json_path, TicketPipeline::ticket_to_json_string(t));
}

bool GuiMainWindow::export_ticket_markdown_to(const std::filesystem::path& md_path) const {
    const TicketDraft t = build_ticket_from_edited_controls();
    return write_text_file(md_path, TicketPipeline::ticket_to_markdown(t));
}

bool GuiMainWindow::capture_window_screenshot(const std::filesystem::path& output_png_path) const {
    if (!hwnd_ || !IsWindow(hwnd_)) return false;

    RECT wrc{};
    if (!GetWindowRect(hwnd_, &wrc)) return false;
    const int w = wrc.right - wrc.left;
    const int h = wrc.bottom - wrc.top;
    if (w <= 0 || h <= 0) return false;

    HDC screen_dc = GetDC(nullptr);
    HDC mem_dc = CreateCompatibleDC(screen_dc);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* dib_bits = nullptr;
    HBITMAP dib = CreateDIBSection(screen_dc, &bmi, DIB_RGB_COLORS, &dib_bits, nullptr, 0);
    if (!dib || !dib_bits) {
        DeleteDC(mem_dc);
        ReleaseDC(nullptr, screen_dc);
        return false;
    }

    HGDIOBJ old_obj = SelectObject(mem_dc, dib);
    BOOL printed = PrintWindow(hwnd_, mem_dc, PW_RENDERFULLCONTENT);
    if (!printed) {
        HDC win_dc = GetWindowDC(hwnd_);
        BitBlt(mem_dc, 0, 0, w, h, win_dc, 0, 0, SRCCOPY);
        ReleaseDC(hwnd_, win_dc);
    }

    ImageBuffer out = ImageBuffer::create(w, h);
    std::memcpy(out.pixels.data(), dib_bits, out.pixels.size());
    // Ensure opaque alpha across captured Win32 controls
    for (size_t i = 3; i < out.pixels.size(); i += 4) {
        out.pixels[i] = 255;
    }

    SelectObject(mem_dc, old_obj);
    DeleteObject(dib);
    DeleteDC(mem_dc);
    ReleaseDC(nullptr, screen_dc);

    return ImageIO::save_to_file(out, output_png_path, 95);
}

GuiControlSnapshot GuiMainWindow::capture_control_snapshot() const {
    GuiControlSnapshot snap;
    snap.window_title = get_hwnd_text_utf8(hwnd_);
    snap.open_button_text = get_hwnd_text_utf8(impl_->btn_open);
    snap.title = get_hwnd_text_utf8(impl_->edit_title);
    snap.category_text = get_hwnd_text_utf8(impl_->combo_cat);
    snap.priority_text = get_hwnd_text_utf8(impl_->combo_prio);
    snap.error_code = get_hwnd_text_utf8(impl_->edit_code);
    snap.module_name = get_hwnd_text_utf8(impl_->edit_mod);
    snap.version = get_hwnd_text_utf8(impl_->edit_ver);
    snap.url = get_hwnd_text_utf8(impl_->edit_url);
    snap.completeness_badge = get_hwnd_text_utf8(impl_->lbl_complete);
    snap.dlp_badge = get_hwnd_text_utf8(impl_->lbl_dlp);
    snap.description = crlf_to_lf(get_hwnd_text_utf8(impl_->edit_desc));
    snap.kb_row_count = static_cast<size_t>(ListView_GetItemCount(impl_->list_kb));
    snap.showing_preprocessed = show_preprocessed_;
    snap.active_locale = i18n_.locale();
    snap.active_theme = theme_;
    return snap;
}

void GuiMainWindow::paint_image_canvas(HDC hdc, const RECT& rc) {
    const int cw = rc.right - rc.left;
    const int ch = rc.bottom - rc.top;
    if (cw <= 10 || ch <= 10) return;

    HDC mem_dc = CreateCompatibleDC(hdc);
    HBITMAP mem_bmp = CreateCompatibleBitmap(hdc, cw, ch);
    HGDIOBJ old_bmp = SelectObject(mem_dc, mem_bmp);

    Gdiplus::Graphics g(mem_dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

    const bool dark_ui = (theme_ == UiTheme::Dark);
    Gdiplus::SolidBrush canvas_bg(dark_ui
        ? Gdiplus::Color(255, 13, 17, 26)
        : Gdiplus::Color(255, 28, 34, 48));
    Gdiplus::Pen frame_pen(dark_ui
        ? Gdiplus::Color(255, 56, 70, 98)
        : Gdiplus::Color(255, 165, 178, 198), 2.0f);
    g.FillRectangle(&canvas_bg, 0, 0, cw, ch);
    g.DrawRectangle(&frame_pen, 1, 1, cw - 2, ch - 2);

    const ImageBuffer* img_to_draw = nullptr;
    if (current_result_) {
        if (show_preprocessed_) {
            img_to_draw = !current_result_->preprocessed.anti_moire_image.empty()
                ? &current_result_->preprocessed.anti_moire_image
                : &current_result_->preprocessed.primary_image;
        } else {
            img_to_draw = &current_result_->original_image;
        }
    }

    if (img_to_draw && !img_to_draw->empty()) {
        Gdiplus::Bitmap bmp(
            img_to_draw->width,
            img_to_draw->height,
            img_to_draw->stride,
            PixelFormat32bppARGB,
            const_cast<BYTE*>(img_to_draw->pixels.data())
        );

        const float avail_w = static_cast<float>(cw - 24);
        const float avail_h = static_cast<float>(ch - 24);
        const float scale = std::min(avail_w / img_to_draw->width, avail_h / img_to_draw->height);
        const float dw = img_to_draw->width * scale;
        const float dh = img_to_draw->height * scale;
        const float dx = (cw - dw) * 0.5f;
        const float dy = (ch - dh) * 0.5f;

        g.DrawImage(&bmp, dx, dy, dw, dh);
    } else {
        Gdiplus::FontFamily ff(L"Segoe UI");
        Gdiplus::Font f_big(&ff, 13.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
        Gdiplus::Font f_sub(&ff, 10.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
        Gdiplus::SolidBrush txt_b(Gdiplus::Color(255, 212, 222, 238));
        Gdiplus::SolidBrush sub_b(Gdiplus::Color(255, 148, 163, 184));

        Gdiplus::StringFormat sf;
        sf.SetAlignment(Gdiplus::StringAlignmentCenter);
        sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);

        const std::wstring w_empty_t = utf8_to_wide(i18n_.tr("canvas.empty_title"));
        const std::wstring w_empty_s = utf8_to_wide(i18n_.tr("canvas.empty_sub"));

        Gdiplus::RectF r1(20.0f, ch * 0.38f, static_cast<float>(cw - 40), 40.0f);
        Gdiplus::RectF r2(20.0f, ch * 0.46f, static_cast<float>(cw - 40), 75.0f);
        g.DrawString(w_empty_t.c_str(), -1, &f_big, r1, &sf, &txt_b);
        g.DrawString(w_empty_s.c_str(), -1, &f_sub, r2, &sf, &sub_b);
    }

    BitBlt(hdc, rc.left, rc.top, cw, ch, mem_dc, 0, 0, SRCCOPY);
    SelectObject(mem_dc, old_bmp);
    DeleteObject(mem_bmp);
    DeleteDC(mem_dc);
}

void GuiMainWindow::on_open_file_dialog() {
    wchar_t file_buf[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFilter = L"Screenshots (*.png;*.jpg;*.jpeg;*.bmp;*.tif)\0*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = file_buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn)) {
        load_and_analyze_file(file_buf);
    }
}

void GuiMainWindow::on_export_json_dialog() {
    wchar_t file_buf[MAX_PATH] = L"generated_ticket.json";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFilter = L"JSON ITSM Ticket (*.json)\0*.json\0";
    ofn.lpstrFile = file_buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"json";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (GetSaveFileNameW(&ofn)) {
        if (export_ticket_json_to(file_buf)) {
            const std::wstring wmsg = utf8_to_wide(i18n_.tr("msg.export_json_ok"));
            const std::wstring wtit = utf8_to_wide(i18n_.tr("msg.export_ok_title"));
            MessageBoxW(hwnd_, wmsg.c_str(), wtit.c_str(), MB_OK | MB_ICONINFORMATION);
        }
    }
}

void GuiMainWindow::on_export_md_dialog() {
    wchar_t file_buf[MAX_PATH] = L"generated_ticket.md";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFilter = L"Markdown Ticket (*.md)\0*.md\0";
    ofn.lpstrFile = file_buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"md";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (GetSaveFileNameW(&ofn)) {
        if (export_ticket_markdown_to(file_buf)) {
            const std::wstring wmsg = utf8_to_wide(i18n_.tr("msg.export_md_ok"));
            const std::wstring wtit = utf8_to_wide(i18n_.tr("msg.export_ok_title"));
            MessageBoxW(hwnd_, wmsg.c_str(), wtit.c_str(), MB_OK | MB_ICONINFORMATION);
        }
    }
}

void GuiMainWindow::on_copy_clipboard() {
    const TicketDraft t = build_ticket_from_edited_controls();
    const std::wstring wmd = utf8_to_wide(TicketPipeline::ticket_to_markdown(t));
    if (!OpenClipboard(hwnd_)) return;
    EmptyClipboard();
    const size_t bytes = (wmd.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hMem) {
        void* ptr = GlobalLock(hMem);
        if (ptr) {
            std::memcpy(ptr, wmd.c_str(), bytes);
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
    }
    CloseClipboard();
    const std::wstring wsb = utf8_to_wide(i18n_.tr("status.copied"));
    SendMessageW(impl_->status_bar, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(wsb.c_str()));
}

void GuiMainWindow::on_run_corpus_benchmark() {
    SetCursor(LoadCursor(nullptr, IDC_WAIT));
    ensure_corpus_ready();

    TicketPipeline pipeline;
    const BenchmarkReport rep = MetricsEvaluator::evaluate_corpus(
        pipeline, corpus_dir_, corpus_samples_,
        [this](size_t curr, size_t tot, const std::string& sid) {
            const std::string sc = std::to_string(curr);
            const std::string st = std::to_string(tot);
            const std::wstring ws = utf8_to_wide(i18n_.trf("status.benchmark_prog", {sc, st, sid}));
            SendMessageW(impl_->status_bar, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(ws.c_str()));
            UpdateWindow(impl_->status_bar);
        }
    );

    const std::filesystem::path rep_md = workspace_root_ / "reports" / "benchmark_report.md";
    const std::filesystem::path rep_json = workspace_root_ / "reports" / "benchmark_metrics.json";
    write_text_file(rep_md, MetricsEvaluator::report_to_markdown(rep));
    write_text_file(rep_json, serialize_json(MetricsEvaluator::report_to_json(rep), 2));

    SetCursor(LoadCursor(nullptr, IDC_ARROW));

    std::ostringstream summary;
    summary << std::fixed << std::setprecision(2)
            << "Corpus Evaluation (" << rep.total_samples << " screenshots):\n\n"
            << "• Overall Key-Field CER: " << (rep.overall_key_fields_cer * 100.0) << "%\n"
            << "• Mobile Moiré Subset CER: " << (rep.moire_subset_cer * 100.0) << "%\n"
            << "• Dark Mode Subset CER: " << (rep.dark_mode_subset_cer * 100.0) << "%\n"
            << "• Support Category Macro-F1: " << std::setprecision(3) << rep.macro_f1
            << " (Accuracy: " << std::setprecision(1) << (rep.accuracy * 100.0) << "%)\n"
            << "• Minutes per Ticket: " << std::setprecision(2) << rep.assisted_minutes_per_ticket
            << " min (vs " << rep.manual_baseline_minutes_per_ticket << " min manual | -"
            << std::setprecision(1) << rep.time_saved_percent << "%)\n"
            << "• % Without Follow-up Info: " << rep.pct_tickets_without_requesting_more_info << "%\n"
            << "• Sensitive Data Leaks: " << rep.total_sensitive_leaks << " / " << rep.total_planted_secrets
            << " planted secrets (Target: 0)\n\n"
            << "Reports saved to:\n  reports\\benchmark_report.md\n  reports\\benchmark_metrics.json";

    const std::wstring wsum = utf8_to_wide(summary.str());
    MessageBoxW(hwnd_, wsum.c_str(),
        rep.all_quality_gates_passed ? L"Benchmark Passed (PASS)" : L"Benchmark Results",
        MB_OK | MB_ICONINFORMATION);
}

LRESULT GuiMainWindow::handle_message(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            create_child_controls();
            return 0;

        case WM_ERASEBKGND: {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            RECT rc{};
            GetClientRect(hwnd_, &rc);
            FillRect(hdc, &rc, (theme_ == UiTheme::Dark) ? impl_->dark_bg_brush : impl_->light_bg_brush);
            return 1;
        }

        case WM_SIZE:
            layout_controls(LOWORD(lParam), HIWORD(lParam));
            InvalidateRect(hwnd_, &impl_->canvas_rect, FALSE);
            return 0;

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetBkMode(hdc, TRANSPARENT);
            const bool dark = (theme_ == UiTheme::Dark);
            HWND hCtrl = reinterpret_cast<HWND>(lParam);

            if (hCtrl == impl_->lbl_dlp) {
                SetTextColor(hdc, dark ? RGB(52, 211, 153) : RGB(16, 124, 65));
            } else if (hCtrl == impl_->lbl_complete) {
                const bool ok = current_result_ && current_result_->ticket.fields.actionable_without_followup;
                SetTextColor(hdc, ok
                    ? (dark ? RGB(52, 211, 153) : RGB(16, 124, 65))
                    : (dark ? RGB(251, 191, 36) : RGB(180, 83, 9)));
            } else if (hCtrl == impl_->lbl_sec_ticket) {
                SetTextColor(hdc, dark ? RGB(147, 197, 253) : RGB(15, 35, 75));
            } else {
                SetTextColor(hdc, dark ? RGB(226, 232, 240) : RGB(30, 41, 59));
            }
            return reinterpret_cast<LRESULT>(dark ? impl_->dark_bg_brush : impl_->light_bg_brush);
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            const bool dark = (theme_ == UiTheme::Dark);
            SetBkColor(hdc, dark ? RGB(30, 37, 54) : RGB(255, 255, 255));
            SetTextColor(hdc, dark ? RGB(241, 245, 249) : RGB(15, 23, 42));
            return reinterpret_cast<LRESULT>(dark ? impl_->dark_input_brush : impl_->light_input_brush);
        }

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd_, &ps);
            paint_image_canvas(hdc, impl_->canvas_rect);
            EndPaint(hwnd_, &ps);
            return 0;
        }

        case WM_DROPFILES: {
            HDROP hDrop = reinterpret_cast<HDROP>(wParam);
            wchar_t path_buf[MAX_PATH] = L"";
            if (DragQueryFileW(hDrop, 0, path_buf, MAX_PATH) > 0) {
                load_and_analyze_file(path_buf);
            }
            DragFinish(hDrop);
            return 0;
        }

        case WM_NOTIFY: {
            const auto* hdr = reinterpret_cast<const NMHDR*>(lParam);
            if (hdr && hdr->idFrom == IDC_LIST_KB && hdr->code == LVN_ITEMCHANGED) {
                const auto* nmlv = reinterpret_cast<const NMLISTVIEW*>(lParam);
                if ((nmlv->uNewState & LVIS_SELECTED) != 0) {
                    on_kb_selection_changed(nmlv->iItem);
                }
            }
            return 0;
        }

        case WM_COMMAND: {
            const int id = LOWORD(wParam);
            const int code = HIWORD(wParam);
            switch (id) {
                case IDC_BTN_OPEN_IMAGE:
                case IDM_FILE_OPEN:
                    on_open_file_dialog();
                    break;
                case IDM_FILE_EXIT:
                    close_window();
                    break;
                case IDC_COMBO_CORPUS:
                    if (code == CBN_SELCHANGE) {
                        const int sel = static_cast<int>(SendMessageW(impl_->combo_corpus, CB_GETCURSEL, 0, 0));
                        if (sel > 0 && static_cast<size_t>(sel - 1) < corpus_samples_.size()) {
                            const auto& s = corpus_samples_[static_cast<size_t>(sel - 1)];
                            load_and_analyze_file(corpus_dir_ / utf8_to_wide(s.relative_image_path));
                        }
                    }
                    break;
                case IDC_BTN_TOGGLE_VIEW:
                case IDM_VIEW_TOGGLE_OCR:
                    toggle_preprocessed_view();
                    break;
                case IDM_VIEW_THEME_LIGHT:
                    set_theme(UiTheme::Light);
                    break;
                case IDM_VIEW_THEME_DARK:
                    set_theme(UiTheme::Dark);
                    break;
                case IDM_LANG_ENGLISH:
                    set_language(Locale::English);
                    break;
                case IDM_LANG_SPANISH:
                    set_language(Locale::Spanish);
                    break;
                case IDC_BTN_REANALYZE:
                    if (!impl_->current_file_path.empty()) {
                        load_and_analyze_file(impl_->current_file_path);
                    }
                    break;
                case IDC_BTN_RUN_BENCHMARK:
                case IDM_TOOLS_BENCHMARK:
                    on_run_corpus_benchmark();
                    break;
                case IDC_BTN_COPY_CLIP:
                case IDM_TOOLS_COPY:
                    on_copy_clipboard();
                    break;
                case IDC_BTN_EXPORT_JSON:
                case IDM_FILE_EXPORT_JSON:
                    on_export_json_dialog();
                    break;
                case IDC_BTN_EXPORT_MD:
                case IDM_FILE_EXPORT_MD:
                    on_export_md_dialog();
                    break;
                default:
                    break;
            }
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

} // namespace ticket_ocr

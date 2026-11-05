#pragma once

#include "ticket_ocr/corpus_generator.hpp"
#include "ticket_ocr/i18n.hpp"
#include "ticket_ocr/image_io.hpp"
#include "ticket_ocr/ticket_pipeline.hpp"
#include "ticket_ocr/types.hpp"

#include <Windows.h>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ticket_ocr {

struct GuiControlSnapshot {
    std::string window_title;
    std::string open_button_text;
    std::string title;
    std::string category_text;
    std::string priority_text;
    std::string error_code;
    std::string module_name;
    std::string version;
    std::string url;
    std::string completeness_badge;
    std::string dlp_badge;
    std::string description;
    size_t kb_row_count = 0;
    bool showing_preprocessed = false;
    Locale active_locale = Locale::English;
    UiTheme active_theme = UiTheme::Light;
};

class GuiMainWindow {
public:
    explicit GuiMainWindow(
        std::filesystem::path workspace_root = std::filesystem::current_path(),
        Locale initial_locale = Locale::English,
        UiTheme initial_theme = UiTheme::Light
    );
    ~GuiMainWindow();

    GuiMainWindow(const GuiMainWindow&) = delete;
    GuiMainWindow& operator=(const GuiMainWindow&) = delete;

    // Creates native High-DPI Win32 window (English by default, with Language & Theme menu bar)
    bool create(HINSTANCE hInstance, int nCmdShow = SW_SHOWDEFAULT);

    // Standard Win32 message loop for interactive human usage
    int run_message_loop();

    // Runtime i18n & Theme switching (accessible from Menu Bar and automated tests)
    void set_language(Locale loc);
    Locale language() const noexcept { return i18n_.locale(); }

    void set_theme(UiTheme theme);
    UiTheme theme() const noexcept { return theme_; }

    // Programmatic & human-triggered actions
    bool load_and_analyze_file(const std::filesystem::path& image_path);
    void toggle_preprocessed_view();
    bool export_ticket_json_to(const std::filesystem::path& json_path) const;
    bool export_ticket_markdown_to(const std::filesystem::path& md_path) const;
    bool capture_window_screenshot(const std::filesystem::path& output_png_path) const;

    GuiControlSnapshot capture_control_snapshot() const;
    const std::optional<PipelineRunResult>& last_result() const noexcept { return current_result_; }
    HWND hwnd() const noexcept { return hwnd_; }
    void close_window();

private:
    struct Impl;
    std::unique_ptr<GdiPlusSession> gdi_session_;
    std::filesystem::path workspace_root_;
    std::filesystem::path corpus_dir_;
    I18n i18n_;
    UiTheme theme_ = UiTheme::Light;
    std::vector<CorpusSample> corpus_samples_;
    std::optional<PipelineRunResult> current_result_;
    bool show_preprocessed_ = false;

    HWND hwnd_ = nullptr;
    std::unique_ptr<Impl> impl_;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handle_message(UINT msg, WPARAM wParam, LPARAM lParam);

    void rebuild_menu_bar();
    void create_child_controls();
    void apply_i18n_texts();
    void apply_theme_styles();
    void layout_controls(int client_w, int client_h);
    void populate_controls_from_result(const PipelineRunResult& res);
    TicketDraft build_ticket_from_edited_controls() const;
    void paint_image_canvas(HDC hdc, const RECT& rc);
    void ensure_corpus_ready();
    void on_open_file_dialog();
    void on_export_json_dialog();
    void on_export_md_dialog();
    void on_copy_clipboard();
    void on_run_corpus_benchmark();
    void on_kb_selection_changed(int index);
};

} // namespace ticket_ocr

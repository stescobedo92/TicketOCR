#pragma once

#include "ticket_ocr/types.hpp"
#include <filesystem>
#include <initializer_list>
#include <map>
#include <string>
#include <string_view>

namespace ticket_ocr {

enum class Locale {
    English = 0, // Default language
    Spanish = 1
};

enum class UiTheme {
    Light = 0,
    Dark = 1
};

class I18n {
public:
    explicit I18n(Locale default_locale = Locale::English);

    void set_locale(Locale loc) noexcept { current_ = loc; }
    Locale locale() const noexcept { return current_; }

    // Load external locale JSON catalogs from a directory (e.g. "locales/")
    bool load_locales_dir(const std::filesystem::path& locales_dir);

    // Translate key in active locale (falls back to English, then key name)
    std::string tr(std::string_view key) const;

    // Positional placeholder formatting: replaces {0}, {1}, {2}...
    std::string trf(std::string_view key, std::initializer_list<std::string_view> args) const;

    std::string category_name(IncidentCategory cat) const;
    std::string priority_name(TicketPriority prio) const;

    // Localized structured ticket body for ITSM pre-fill
    static std::string format_ticket_description(const TicketDraft& t, Locale loc);

private:
    Locale current_ = Locale::English;
    std::map<std::string, std::string> en_catalog_;
    std::map<std::string, std::string> es_catalog_;

    void init_embedded_defaults();
    bool load_single_json(const std::filesystem::path& file_path, std::map<std::string, std::string>& target);
};

} // namespace ticket_ocr

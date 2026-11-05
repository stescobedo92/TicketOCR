#pragma once

#include "ticket_ocr/types.hpp"
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ticket_ocr {

struct JsonValue;
using JsonObject = std::map<std::string, JsonValue>;
using JsonArray = std::vector<JsonValue>;

struct JsonValue {
    using Storage = std::variant<std::nullptr_t, bool, double, std::string, JsonArray, JsonObject>;
    Storage data = nullptr;

    JsonValue() = default;
    JsonValue(std::nullptr_t) : data(nullptr) {}
    JsonValue(bool b) : data(b) {}
    JsonValue(int i) : data(static_cast<double>(i)) {}
    JsonValue(size_t n) : data(static_cast<double>(n)) {}
    JsonValue(double d) : data(d) {}
    JsonValue(const char* s) : data(std::string(s)) {}
    JsonValue(std::string s) : data(std::move(s)) {}
    JsonValue(JsonArray arr) : data(std::move(arr)) {}
    JsonValue(JsonObject obj) : data(std::move(obj)) {}

    bool is_null() const noexcept { return std::holds_alternative<std::nullptr_t>(data); }
    bool is_bool() const noexcept { return std::holds_alternative<bool>(data); }
    bool is_number() const noexcept { return std::holds_alternative<double>(data); }
    bool is_string() const noexcept { return std::holds_alternative<std::string>(data); }
    bool is_array() const noexcept { return std::holds_alternative<JsonArray>(data); }
    bool is_object() const noexcept { return std::holds_alternative<JsonObject>(data); }

    bool as_bool(bool fallback = false) const noexcept;
    double as_number(double fallback = 0.0) const noexcept;
    int as_int(int fallback = 0) const noexcept;
    std::string as_string(std::string_view fallback = "") const;
    const JsonArray& as_array() const;
    const JsonObject& as_object() const;

    const JsonValue* find(std::string_view key) const;
};

std::string json_escape(std::string_view sv);
std::string serialize_json(const JsonValue& val, int indent = 2);
JsonValue parse_json(std::string_view text);

std::string read_text_file(const std::filesystem::path& path);
bool write_text_file(const std::filesystem::path& path, std::string_view content);

} // namespace ticket_ocr

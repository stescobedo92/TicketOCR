#pragma once

#include <Windows.h>
#include <chrono>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace test_fw {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        registry().push_back(TestCase{name, std::move(fn)});
    }
};

#define TEST_CASE(name) \
    static void test_fn_##name(); \
    static ::test_fw::Registrar reg_##name(#name, &test_fn_##name); \
    static void test_fn_##name()

#define REQUIRE_TRUE(expr) \
    do { \
        if (!(expr)) { \
            std::ostringstream _oss; \
            _oss << "REQUIRE_TRUE falló en " << __FILE__ << ":" << __LINE__ << " -> " << #expr; \
            throw std::runtime_error(_oss.str()); \
        } \
    } while (0)

#define REQUIRE_EQ(actual, expected) \
    do { \
        const auto _a = (actual); \
        const auto _e = (expected); \
        if (!(_a == _e)) { \
            std::ostringstream _oss; \
            _oss << "REQUIRE_EQ falló en " << __FILE__ << ":" << __LINE__ \
                 << "\n  Esperado: [" << _e << "]\n  Obtenido: [" << _a << "]"; \
            throw std::runtime_error(_oss.str()); \
        } \
    } while (0)

#define REQUIRE_LE(actual, upper_bound) \
    do { \
        const auto _a = (actual); \
        const auto _u = (upper_bound); \
        if (!(_a <= _u)) { \
            std::ostringstream _oss; \
            _oss << "REQUIRE_LE falló en " << __FILE__ << ":" << __LINE__ \
                 << " (" << _a << " > " << _u << ")"; \
            throw std::runtime_error(_oss.str()); \
        } \
    } while (0)

#define REQUIRE_GE(actual, lower_bound) \
    do { \
        const auto _a = (actual); \
        const auto _l = (lower_bound); \
        if (!(_a >= _l)) { \
            std::ostringstream _oss; \
            _oss << "REQUIRE_GE falló en " << __FILE__ << ":" << __LINE__ \
                 << " (" << _a << " < " << _l << ")"; \
            throw std::runtime_error(_oss.str()); \
        } \
    } while (0)

inline int run_all_tests(const char* suite_title) {
    SetConsoleOutputCP(CP_UTF8);
    std::cout << "====================================================================\n";
    std::cout << "  SUITE DE INTEGRACIÓN: " << suite_title << "\n";
    std::cout << "====================================================================\n";

    int passed = 0;
    int failed = 0;

    for (const auto& tc : registry()) {
        const auto t0 = std::chrono::steady_clock::now();
        try {
            tc.fn();
            const auto t1 = std::chrono::steady_clock::now();
            const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            std::cout << "  [PASS] " << std::left << std::setw(46) << tc.name
                      << " (" << std::fixed << std::setprecision(1) << ms << " ms)\n";
            ++passed;
        } catch (const std::exception& ex) {
            std::cout << "  [FAIL] " << tc.name << "\n         " << ex.what() << "\n";
            ++failed;
        }
    }

    std::cout << "--------------------------------------------------------------------\n";
    std::cout << "  Resumen: " << passed << " aprobadas, " << failed << " fallidas.\n";
    std::cout << "====================================================================\n";
    return failed == 0 ? 0 : 1;
}

} // namespace test_fw

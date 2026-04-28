#pragma once

#include <iostream>
#include <stdexcept>
#include <string>

namespace ptcgp_test
{

inline int g_failures = 0;

inline void require(bool passed, const char* expression, const char* file, int line)
{
    if (!passed)
    {
        throw std::runtime_error(
            std::string(file) + ":" + std::to_string(line) +
            " — REQUIRE failed: " + expression);
    }
}

template <typename Func>
void run_test(Func&& func, const char* name)
{
    try
    {
        func();
    }
    catch (const std::exception& e)
    {
        std::cerr << "  [FAIL] " << name << "\n"
                  << "         " << e.what() << "\n";
        ++g_failures;
    }
}

template <typename Func>
void run_test_with_pass(Func&& func, const char* name)
{
    try
    {
        func();
        std::cout << "  [PASS] " << name << "\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "  [FAIL] " << name << "\n"
                  << "         " << e.what() << "\n";
        ++g_failures;
    }
}

inline int print_summary()
{
    std::cout << "\n";
    if (g_failures == 0)
        std::cout << "All tests passed.\n";
    else
        std::cerr << g_failures << " test(s) FAILED.\n";

    return g_failures > 0 ? 1 : 0;
}

} // namespace ptcgp_test

#define REQUIRE(expr) ::ptcgp_test::require(static_cast<bool>(expr), #expr, __FILE__, __LINE__)
#define RUN_TEST(func) ::ptcgp_test::run_test((func), #func)
#define RUN_TEST_WITH_PASS(func) ::ptcgp_test::run_test_with_pass((func), #func)
#define g_failures (::ptcgp_test::g_failures)

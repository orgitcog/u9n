/**
 * @file test_main.cpp
 * @brief Simple test framework and main entry point
 */

#include <iostream>
#include <functional>
#include <string>
#include <vector>
#include <chrono>

namespace test {

struct TestCase {
    std::string name;
    std::function<bool()> func;
};

std::vector<TestCase>& get_tests() {
    static std::vector<TestCase> tests;
    return tests;
}

bool register_test(const std::string& name, std::function<bool()> func) {
    get_tests().push_back({name, func});
    return true;
}

#define TEST(name) \
    bool test_##name(); \
    static bool _registered_##name = test::register_test(#name, test_##name); \
    bool test_##name()

#define ASSERT(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "  FAILED: " << #expr << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
            return false; \
        } \
    } while(0)

#define ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            std::cerr << "  FAILED: " << #a << " == " << #b << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
            std::cerr << "    got: " << (a) << " vs " << (b) << "\n"; \
            return false; \
        } \
    } while(0)

#define ASSERT_NE(a, b) \
    do { \
        if ((a) == (b)) { \
            std::cerr << "  FAILED: " << #a << " != " << #b << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
            return false; \
        } \
    } while(0)

#define ASSERT_NEAR(a, b, eps) \
    do { \
        if (std::abs((a) - (b)) > (eps)) { \
            std::cerr << "  FAILED: " << #a << " ~= " << #b << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
            std::cerr << "    got: " << (a) << " vs " << (b) << " (diff=" << std::abs((a)-(b)) << ")\n"; \
            return false; \
        } \
    } while(0)

int run_all_tests() {
    std::cout << std::unitbuf;  // flush after each insertion (diagnose hangs reliably)
    std::cerr << std::unitbuf;
    auto& tests = get_tests();
    int passed = 0;
    int failed = 0;

    std::cout << "Running " << tests.size() << " tests...\n\n";

    auto start = std::chrono::high_resolution_clock::now();

    for (const auto& test : tests) {
        std::cout << "[ RUN      ] " << test.name << "\n";

        auto test_start = std::chrono::high_resolution_clock::now();
        bool success = false;

        try {
            success = test.func();
        } catch (const std::exception& e) {
            std::cerr << "  EXCEPTION: " << e.what() << "\n";
            success = false;
        } catch (...) {
            std::cerr << "  EXCEPTION: unknown\n";
            success = false;
        }

        auto test_end = std::chrono::high_resolution_clock::now();
        auto test_duration = std::chrono::duration_cast<std::chrono::microseconds>(test_end - test_start);

        if (success) {
            std::cout << "[       OK ] " << test.name << " (" << test_duration.count() << " us)\n";
            ++passed;
        } else {
            std::cout << "[  FAILED  ] " << test.name << "\n";
            ++failed;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "\n";
    std::cout << "==========================================================\n";
    std::cout << "Passed: " << passed << ", Failed: " << failed;
    std::cout << ", Total: " << tests.size() << "\n";
    std::cout << "Time: " << total_duration.count() << " ms\n";
    std::cout << "==========================================================\n";

    return failed > 0 ? 1 : 0;
}

} // namespace test

// Global-namespace adapter used by test files that declare
// `extern bool register_test(const char*, bool(*)())` (e.g. test_atomese.cpp)
bool register_test(const char* name, bool (*fn)()) {
    return test::register_test(std::string(name), std::function<bool()>(fn));
}

int main() {
    return test::run_all_tests();
}

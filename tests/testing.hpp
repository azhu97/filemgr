#pragma once
// Minimal self-registering test framework (no external dependencies).
//
//   TEST(parses_flags) { CHECK(x); CHECK_EQ(a, b); }
//
// Each test runs in its own fresh temporary directory, available as tmp().

#include <filesystem>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace testing {

struct Failure {
    std::string message;
};

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

inline std::filesystem::path& currentTmp() {
    static std::filesystem::path p;
    return p;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

template <typename A, typename B>
void checkEq(const A& a, const B& b, const char* ea, const char* eb, const char* file, int line) {
    if (!(a == b)) {
        std::ostringstream out;
        out << file << ":" << line << ": CHECK_EQ(" << ea << ", " << eb << ")\n      got: " << a
            << "\n expected: " << b;
        throw Failure{out.str()};
    }
}

} // namespace testing

inline const std::filesystem::path& tmp() { return testing::currentTmp(); }

// Writes `content` to `p`, creating parent folders.
void writeFile(const std::filesystem::path& p, const std::string& content = "x");
std::string readFile(const std::filesystem::path& p);

#define TEST_CONCAT2(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT2(a, b)
#define TEST(name)                                                              \
    static void test_##name();                                                  \
    static testing::Registrar TEST_CONCAT(registrar_, name)(#name, test_##name); \
    static void test_##name()

#define CHECK(cond)                                                                             \
    do {                                                                                        \
        if (!(cond))                                                                            \
            throw testing::Failure{std::string(__FILE__) + ":" + std::to_string(__LINE__) +     \
                                   ": CHECK(" #cond ") failed"};                                \
    } while (0)

#define CHECK_EQ(a, b) testing::checkEq((a), (b), #a, #b, __FILE__, __LINE__)

#define CHECK_THROWS(expr, type)                                                                \
    do {                                                                                        \
        bool thrown = false;                                                                    \
        try { expr; } catch (const type&) { thrown = true; }                                    \
        if (!thrown)                                                                            \
            throw testing::Failure{std::string(__FILE__) + ":" + std::to_string(__LINE__) +     \
                                   ": expected " #type " from " #expr};                         \
    } while (0)

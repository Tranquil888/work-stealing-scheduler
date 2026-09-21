#pragma once

#include <cstdio>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace wss_test {

struct Case {
    std::string suite;
    std::string name;
    std::function<void()> func;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

inline int failures = 0;
inline int passes = 0;

struct Register {
    Register(std::string suite, std::string name, std::function<void()> func) {
        registry().push_back({std::move(suite), std::move(name), std::move(func)});
    }
};

inline void check(bool cond, const char* expr, const char* file, int line) {
    if (cond) {
        ++passes;
    } else {
        ++failures;
        std::printf("FAIL %s:%d: %s\n", file, line, expr);
    }
}

inline int run_all() {
    for (auto& c : registry()) {
        try {
            c.func();
            std::printf("ok %s.%s\n", c.suite.c_str(), c.name.c_str());
        } catch (const std::exception& e) {
            ++failures;
            std::printf("FAIL %s.%s threw: %s\n", c.suite.c_str(), c.name.c_str(),
                        e.what());
        } catch (...) {
            ++failures;
            std::printf("FAIL %s.%s threw unknown\n", c.suite.c_str(),
                        c.name.c_str());
        }
    }
    std::printf("%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}

}  // namespace wss_test

#define WSS_TEST(suite, name)                                                  \
    static void wss_test_fn_##suite##_##name();                                \
    static ::wss_test::Register wss_test_reg_##suite##_##name(                 \
        #suite, #name, wss_test_fn_##suite##_##name);                           \
    static void wss_test_fn_##suite##_##name()

#define EXPECT_TRUE(x) ::wss_test::check((x), "EXPECT_TRUE(" #x ")", __FILE__, __LINE__)
#define EXPECT_FALSE(x) ::wss_test::check(!(x), "EXPECT_FALSE(" #x ")", __FILE__, __LINE__)
#define EXPECT_EQ(a, b) \
    ::wss_test::check((a) == (b), "EXPECT_EQ(" #a ", " #b ")", __FILE__, __LINE__)
#define EXPECT_NE(a, b) \
    ::wss_test::check((a) != (b), "EXPECT_NE(" #a ", " #b ")", __FILE__, __LINE__)

#define WSS_TEST_MAIN() \
    int main() { return ::wss_test::run_all(); }

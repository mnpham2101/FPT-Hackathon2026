#ifndef GTEST_COMPAT_H
#define GTEST_COMPAT_H

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <functional>

namespace testing {
    struct TestInfo {
        std::string test_case_name;
        std::string test_name;
        std::function<void()> func;
    };

    inline std::vector<TestInfo>& get_tests() {
        static std::vector<TestInfo> tests;
        return tests;
    }

    inline int register_test(const char* test_case_name, const char* test_name, std::function<void()> func) {
        get_tests().push_back({test_case_name, test_name, func});
        return 0;
    }

    inline int run_all_tests() {
        int passed = 0;
        int failed = 0;
        std::cout << "[==========] Running " << get_tests().size() << " tests." << std::endl;
        for (const auto& t : get_tests()) {
            std::cout << "[ RUN      ] " << t.test_case_name << "." << t.test_name << std::endl;
            try {
                t.func();
                std::cout << "[       OK ] " << t.test_case_name << "." << t.test_name << std::endl;
                passed++;
            } catch (const std::exception& e) {
                std::cout << "[  FAILED  ] " << t.test_case_name << "." << t.test_name << ": " << e.what() << std::endl;
                failed++;
            } catch (...) {
                std::cout << "[  FAILED  ] " << t.test_case_name << "." << t.test_name << std::endl;
                failed++;
            }
        }
        std::cout << "[==========] " << passed << " passed, " << failed << " failed." << std::endl;
        return failed == 0 ? 0 : 1;
    }
}

#define TEST(test_case_name, test_name) \
    void test_case_name##_##test_name(); \
    static int dummy_##test_case_name##_##test_name = \
        testing::register_test(#test_case_name, #test_name, test_case_name##_##test_name); \
    void test_case_name##_##test_name()

#define EXPECT_TRUE(cond) if (!(cond)) throw std::runtime_error("EXPECT_TRUE failed: " #cond);
#define EXPECT_FALSE(cond) if (cond) throw std::runtime_error("EXPECT_FALSE failed: " #cond);
#define EXPECT_EQ(val1, val2) if ((val1) != (val2)) throw std::runtime_error("EXPECT_EQ failed: " #val1 " != " #val2);
#define EXPECT_NE(val1, val2) if ((val1) == (val2)) throw std::runtime_error("EXPECT_NE failed: " #val1 " == " #val2);
#define EXPECT_DOUBLE_EQ(val1, val2) if (std::abs(static_cast<double>(val1) - static_cast<double>(val2)) > 1e-5) throw std::runtime_error("EXPECT_DOUBLE_EQ failed");
#define EXPECT_NEAR(val1, val2, abs_error) if (std::abs(static_cast<double>(val1) - static_cast<double>(val2)) > (abs_error)) throw std::runtime_error("EXPECT_NEAR failed");
#define ASSERT_TRUE(cond) EXPECT_TRUE(cond)
#define ASSERT_FALSE(cond) EXPECT_FALSE(cond)
#define ASSERT_EQ(val1, val2) EXPECT_EQ(val1, val2)

int main(int argc, char** argv) {
    return testing::run_all_tests();
}

#endif // GTEST_COMPAT_H

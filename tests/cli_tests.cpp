#include "common/cli_parser.h"
#include "common/validation.h"

#include <cstdlib>
#include <iostream>
#include <vector>

#define TEST_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "[FAIL] Assertion failed: (" #expr ") at " << __FILE__ << ":" << __LINE__ << "\n"; \
            std::abort(); \
        } \
    } while (false)

namespace perf_lab::test {

void test_default_config() {
    std::vector<std::string> args = {};
    const auto res = CliParser::parse(args);

    TEST_ASSERT(res.success);
    TEST_ASSERT(!res.should_exit);
    TEST_ASSERT(res.config.mode == WorkloadMode::Compare);
    TEST_ASSERT(res.config.threads == 0); // 0 indicates auto-detection
    TEST_ASSERT(res.config.operations_per_worker == 1'000'000);
    TEST_ASSERT(res.config.runs == 5);
    TEST_ASSERT(res.config.output_dir == "results");
    std::cout << "[PASS] test_default_config\n";
}

void test_valid_explicit_arguments() {
    std::vector<std::string> args = {
        "--mode", "baseline",
        "--threads", "8",
        "--operations", "250000",
        "--runs", "3",
        "--output-dir", "custom_dir"
    };
    const auto res = CliParser::parse(args);

    TEST_ASSERT(res.success);
    TEST_ASSERT(!res.should_exit);
    TEST_ASSERT(res.config.mode == WorkloadMode::Baseline);
    TEST_ASSERT(res.config.threads == 8);
    TEST_ASSERT(res.config.operations_per_worker == 250'000);
    TEST_ASSERT(res.config.runs == 3);
    TEST_ASSERT(res.config.output_dir == "custom_dir");
    std::cout << "[PASS] test_valid_explicit_arguments\n";
}

void test_inline_equal_arguments() {
    std::vector<std::string> args = {
        "--mode=optimized",
        "--threads=16",
        "--operations=500000",
        "--runs=10",
        "--output-dir=test_out"
    };
    const auto res = CliParser::parse(args);

    TEST_ASSERT(res.success);
    TEST_ASSERT(!res.should_exit);
    TEST_ASSERT(res.config.mode == WorkloadMode::Optimized);
    TEST_ASSERT(res.config.threads == 16);
    TEST_ASSERT(res.config.operations_per_worker == 500'000);
    TEST_ASSERT(res.config.runs == 10);
    TEST_ASSERT(res.config.output_dir == "test_out");
    std::cout << "[PASS] test_inline_equal_arguments\n";
}

void test_help_and_version() {
    {
        std::vector<std::string> args = {"--help"};
        const auto res = CliParser::parse(args);
        TEST_ASSERT(res.success);
        TEST_ASSERT(res.should_exit);
        TEST_ASSERT(res.exit_code == 0);
    }
    {
        std::vector<std::string> args = {"-h"};
        const auto res = CliParser::parse(args);
        TEST_ASSERT(res.success);
        TEST_ASSERT(res.should_exit);
        TEST_ASSERT(res.exit_code == 0);
    }
    {
        std::vector<std::string> args = {"--version"};
        const auto res = CliParser::parse(args);
        TEST_ASSERT(res.success);
        TEST_ASSERT(res.should_exit);
        TEST_ASSERT(res.exit_code == 0);
    }
    std::cout << "[PASS] test_help_and_version\n";
}

void test_reject_invalid_mode() {
    std::vector<std::string> args = {"--mode", "invalid_mode"};
    const auto res = CliParser::parse(args);
    TEST_ASSERT(!res.success);
    TEST_ASSERT(!res.error_message.empty());
    std::cout << "[PASS] test_reject_invalid_mode\n";
}

void test_reject_zero_threads() {
    std::vector<std::string> args = {"--threads", "0"};
    const auto res = CliParser::parse(args);
    TEST_ASSERT(!res.success);
    TEST_ASSERT(!res.error_message.empty());
    std::cout << "[PASS] test_reject_zero_threads\n";
}

void test_reject_excessive_threads() {
    std::vector<std::string> args = {"--threads", "2048"};
    const auto res = CliParser::parse(args);
    TEST_ASSERT(!res.success);
    TEST_ASSERT(!res.error_message.empty());
    std::cout << "[PASS] test_reject_excessive_threads\n";
}

void test_reject_zero_operations() {
    std::vector<std::string> args = {"--operations", "0"};
    const auto res = CliParser::parse(args);
    TEST_ASSERT(!res.success);
    TEST_ASSERT(!res.error_message.empty());
    std::cout << "[PASS] test_reject_zero_operations\n";
}

void test_reject_excessive_operations() {
    std::vector<std::string> args = {"--operations", "5000000000"};
    const auto res = CliParser::parse(args);
    TEST_ASSERT(!res.success);
    TEST_ASSERT(!res.error_message.empty());
    std::cout << "[PASS] test_reject_excessive_operations\n";
}

void test_reject_zero_runs() {
    std::vector<std::string> args = {"--runs", "0"};
    const auto res = CliParser::parse(args);
    TEST_ASSERT(!res.success);
    TEST_ASSERT(!res.error_message.empty());
    std::cout << "[PASS] test_reject_zero_runs\n";
}

void test_reject_non_numeric_threads() {
    std::vector<std::string> args = {"--threads", "four"};
    const auto res = CliParser::parse(args);
    TEST_ASSERT(!res.success);
    TEST_ASSERT(!res.error_message.empty());
    std::cout << "[PASS] test_reject_non_numeric_threads\n";
}

void test_reject_negative_threads() {
    std::vector<std::string> args = {"--threads", "-8"};
    const auto res = CliParser::parse(args);
    TEST_ASSERT(!res.success);
    TEST_ASSERT(!res.error_message.empty());
    std::cout << "[PASS] test_reject_negative_threads\n";
}

void test_reject_missing_value() {
    std::vector<std::string> args = {"--threads"};
    const auto res = CliParser::parse(args);
    TEST_ASSERT(!res.success);
    TEST_ASSERT(!res.error_message.empty());
    std::cout << "[PASS] test_reject_missing_value\n";
}

void test_reject_invalid_output_dir() {
    std::vector<std::string> args = {"--output-dir", "invalid*dir"};
    const auto res = CliParser::parse(args);
    TEST_ASSERT(!res.success);
    TEST_ASSERT(!res.error_message.empty());
    std::cout << "[PASS] test_reject_invalid_output_dir\n";
}

} // namespace perf_lab::test

int main() {
    using namespace perf_lab::test;
    test_default_config();
    test_valid_explicit_arguments();
    test_inline_equal_arguments();
    test_help_and_version();
    test_reject_invalid_mode();
    test_reject_zero_threads();
    test_reject_excessive_threads();
    test_reject_zero_operations();
    test_reject_excessive_operations();
    test_reject_zero_runs();
    test_reject_non_numeric_threads();
    test_reject_negative_threads();
    test_reject_missing_value();
    test_reject_invalid_output_dir();

    std::cout << "All CLI tests passed successfully.\n";
    return 0;
}

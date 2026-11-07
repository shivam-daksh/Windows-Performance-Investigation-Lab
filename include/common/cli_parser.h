#pragma once

#include "benchmark/benchmark_config.h"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace perf_lab {

struct ParseResult {
    bool should_exit{false};
    int exit_code{0};
    bool success{false};
    std::string error_message;
    BenchmarkConfig config;
};

class CliParser {
public:
    static constexpr std::string_view kVersion = "1.0.0";

    static ParseResult parse(int argc, const char* const* argv);
    static ParseResult parse(const std::vector<std::string>& args);

    static void print_help(std::ostream& os = std::cout);
    static void print_version(std::ostream& os = std::cout);
};

} // namespace perf_lab

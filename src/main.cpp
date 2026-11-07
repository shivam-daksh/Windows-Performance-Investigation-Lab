#include "benchmark/benchmark_runner.h"
#include "common/cli_parser.h"
#include "diagnostics/etw_provider.h"

#include <iostream>

int main(int argc, char* argv[]) {
    using namespace perf_lab;

    const auto parse_res = CliParser::parse(argc, argv);
    if (parse_res.should_exit) {
        return parse_res.exit_code;
    }

    if (!parse_res.success) {
        std::cerr << "Error: " << parse_res.error_message << "\n";
        std::cerr << "Run 'perf_lab.exe --help' for usage instructions.\n";
        return 1;
    }

    std::cout << "================================================================================\n";
    std::cout << "                   WINDOWS PERFORMANCE INVESTIGATION LAB\n";
    std::cout << "================================================================================\n";

    // Initialize the RAII ETW provider
    EtwProvider etw_provider;
    if (etw_provider.is_registered()) {
        std::cout << "[ETW] TraceLogging provider registered: Shivam.PerformanceInvestigationLab\n";
    } else {
        std::cout << "[ETW] Tracing inactive (unsupported platform or registration failed).\n"
                  << "      The benchmark will run normally without ETW event emission.\n";
    }

    BenchmarkRunner runner(etw_provider);
    const auto result = runner.execute(parse_res.config);

    if (!result.overall_success) {
        std::cerr << "\n[ERROR] Benchmark execution encountered errors.\n";
        return 1;
    }

    return 0;
}

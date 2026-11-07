#include "common/cli_parser.h"
#include "common/validation.h"

#include <cctype>
#include <charconv>
#include <iostream>

namespace perf_lab {

namespace {

bool parse_uint64(std::string_view str, uint64_t& out_val) {
    if (str.empty()) {
        return false;
    }
    for (char c : str) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    const auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), out_val);
    return ec == std::errc{} && ptr == str.data() + str.size();
}

bool parse_uint32(std::string_view str, uint32_t& out_val) {
    uint64_t val64 = 0;
    if (!parse_uint64(str, val64) || val64 > 0xFFFFFFFFULL) {
        return false;
    }
    out_val = static_cast<uint32_t>(val64);
    return true;
}

} // namespace

void CliParser::print_help(std::ostream& os) {
    os << "Windows Performance Investigation Lab - CLI\n"
       << "Usage: perf_lab.exe [options]\n\n"
       << "Options:\n"
       << "  --mode <mode>         Workload execution mode:\n"
       << "                          baseline  : Contended mutex counter\n"
       << "                          optimized : Thread-local counter reduction\n"
       << "                          compare   : Run both and compare (default)\n"
       << "  --threads <N>         Number of worker threads (default: hardware concurrency)\n"
       << "  --operations <N>      Number of operations per worker thread (default: 1,000,000)\n"
       << "  --runs <N>            Number of benchmark iterations to measure (default: 5)\n"
       << "  --output-dir <path>   Directory where CSV and summary are saved (default: results)\n"
       << "  -h, --help            Display this help message and exit\n"
       << "  -v, --version         Display application version and exit\n\n"
       << "Example:\n"
       << "  perf_lab.exe --mode compare --threads 8 --operations 2000000 --runs 5\n";
}

void CliParser::print_version(std::ostream& os) {
    os << "Windows Performance Investigation Lab v" << kVersion << "\n"
       << "Target Architecture: Native Windows x64 (C++20)\n"
       << "ETW Provider: Shivam.PerformanceInvestigationLab\n";
}

ParseResult CliParser::parse(int argc, const char* const* argv) {
    std::vector<std::string> args;
    args.reserve(argc > 0 ? argc - 1 : 0);
    for (int i = 1; i < argc; ++i) {
        if (argv[i] != nullptr) {
            args.emplace_back(argv[i]);
        }
    }
    return parse(args);
}

ParseResult CliParser::parse(const std::vector<std::string>& args) {
    ParseResult result{};
    result.success = false;

    for (size_t i = 0; i < args.size(); ++i) {
        std::string_view arg = args[i];
        std::string_view opt_name = arg;
        std::string_view opt_value;
        bool has_inline_val = false;

        const size_t eq_pos = arg.find('=');
        if (eq_pos != std::string_view::npos) {
            opt_name = arg.substr(0, eq_pos);
            opt_value = arg.substr(eq_pos + 1);
            has_inline_val = true;
        }

        if (opt_name == "-h" || opt_name == "--help") {
            result.should_exit = true;
            result.exit_code = 0;
            result.success = true;
            print_help();
            return result;
        }

        if (opt_name == "-v" || opt_name == "--version") {
            result.should_exit = true;
            result.exit_code = 0;
            result.success = true;
            print_version();
            return result;
        }

        auto get_value = [&]() -> std::pair<bool, std::string_view> {
            if (has_inline_val) {
                return {true, opt_value};
            }
            if (i + 1 < args.size()) {
                ++i;
                return {true, args[i]};
            }
            return {false, {}};
        };

        if (opt_name == "--mode") {
            const auto [ok, val] = get_value();
            if (!ok) {
                result.error_message = "Missing value for --mode argument.";
                return result;
            }
            const auto mode_res = Validator::validate_mode(val);
            if (!mode_res) {
                result.error_message = mode_res.error_message;
                return result;
            }
            if (val == "baseline") {
                result.config.mode = WorkloadMode::Baseline;
            } else if (val == "optimized") {
                result.config.mode = WorkloadMode::Optimized;
            } else if (val == "compare") {
                result.config.mode = WorkloadMode::Compare;
            }
        } else if (opt_name == "--threads") {
            const auto [ok, val] = get_value();
            if (!ok) {
                result.error_message = "Missing value for --threads argument.";
                return result;
            }
            uint32_t threads_val = 0;
            if (!parse_uint32(val, threads_val)) {
                result.error_message = "Invalid numeric value for --threads: '" + std::string(val) + "'.";
                return result;
            }
            const auto val_res = Validator::validate_threads(threads_val);
            if (!val_res) {
                result.error_message = val_res.error_message;
                return result;
            }
            result.config.threads = threads_val;
        } else if (opt_name == "--operations") {
            const auto [ok, val] = get_value();
            if (!ok) {
                result.error_message = "Missing value for --operations argument.";
                return result;
            }
            uint64_t ops_val = 0;
            if (!parse_uint64(val, ops_val)) {
                result.error_message = "Invalid numeric value for --operations: '" + std::string(val) + "'.";
                return result;
            }
            const auto val_res = Validator::validate_operations(ops_val);
            if (!val_res) {
                result.error_message = val_res.error_message;
                return result;
            }
            result.config.operations_per_worker = ops_val;
        } else if (opt_name == "--runs") {
            const auto [ok, val] = get_value();
            if (!ok) {
                result.error_message = "Missing value for --runs argument.";
                return result;
            }
            uint32_t runs_val = 0;
            if (!parse_uint32(val, runs_val)) {
                result.error_message = "Invalid numeric value for --runs: '" + std::string(val) + "'.";
                return result;
            }
            const auto val_res = Validator::validate_runs(runs_val);
            if (!val_res) {
                result.error_message = val_res.error_message;
                return result;
            }
            result.config.runs = runs_val;
        } else if (opt_name == "--output-dir") {
            const auto [ok, val] = get_value();
            if (!ok) {
                result.error_message = "Missing value for --output-dir argument.";
                return result;
            }
            const std::filesystem::path path_val(val);
            const auto val_res = Validator::validate_output_dir(path_val);
            if (!val_res) {
                result.error_message = val_res.error_message;
                return result;
            }
            result.config.output_dir = path_val;
        } else {
            result.error_message = "Unrecognized option: '" + std::string(opt_name) + "'. Use --help for usage.";
            return result;
        }
    }

    result.success = true;
    return result;
}

} // namespace perf_lab

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace perf_lab {

struct ValidationResult {
    bool is_valid{true};
    std::string error_message;

    explicit operator bool() const noexcept {
        return is_valid;
    }

    static ValidationResult success() noexcept {
        return {true, {}};
    }

    static ValidationResult failure(std::string message) {
        return {false, std::move(message)};
    }
};

class Validator {
public:
    static constexpr uint32_t kMinThreads = 1;
    static constexpr uint32_t kMaxThreads = 1024;
    static constexpr uint64_t kMinOperations = 1;
    static constexpr uint64_t kMaxOperations = 1'000'000'000ULL;
    static constexpr uint32_t kMinRuns = 1;
    static constexpr uint32_t kMaxRuns = 1000;

    static ValidationResult validate_threads(uint32_t threads) noexcept {
        if (threads < kMinThreads) {
            return ValidationResult::failure("Thread count must be at least 1.");
        }
        if (threads > kMaxThreads) {
            return ValidationResult::failure(
                "Thread count exceeds maximum allowed limit of " + std::to_string(kMaxThreads) + ".");
        }
        return ValidationResult::success();
    }

    static ValidationResult validate_operations(uint64_t operations) noexcept {
        if (operations < kMinOperations) {
            return ValidationResult::failure("Operations per worker must be at least 1.");
        }
        if (operations > kMaxOperations) {
            return ValidationResult::failure(
                "Operations count exceeds maximum allowed limit of " + std::to_string(kMaxOperations) + ".");
        }
        return ValidationResult::success();
    }

    static ValidationResult validate_runs(uint32_t runs) noexcept {
        if (runs < kMinRuns) {
            return ValidationResult::failure("Number of runs must be at least 1.");
        }
        if (runs > kMaxRuns) {
            return ValidationResult::failure(
                "Number of runs exceeds maximum allowed limit of " + std::to_string(kMaxRuns) + ".");
        }
        return ValidationResult::success();
    }

    static ValidationResult validate_mode(std::string_view mode) noexcept {
        if (mode == "baseline" || mode == "optimized" || mode == "compare") {
            return ValidationResult::success();
        }
        return ValidationResult::failure(
            "Invalid mode '" + std::string(mode) + "'. Supported modes: baseline, optimized, compare.");
    }

    static ValidationResult validate_output_dir(const std::filesystem::path& path) {
        if (path.empty()) {
            return ValidationResult::failure("Output directory path cannot be empty.");
        }

        const std::string path_str = path.string();
        // Check for disallowed characters on Windows/POSIX paths
        if (path_str.find_first_of("<>\"|?*") != std::string::npos) {
            return ValidationResult::failure(
                "Output directory path contains invalid filesystem characters (<>\"|?*).");
        }

        return ValidationResult::success();
    }
};

} // namespace perf_lab

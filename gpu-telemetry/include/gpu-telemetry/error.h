/**
 * @brief Error codes for GPU telemetry
 *
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <system_error>

namespace gpu::telemetry {

/**
 * @brief Error codes for public API
 */
enum class Error {
    /** Operation successful */
    Success = 0,
    
    /** Invalid argument provided */
    InvalidArgument = -1,
    
    /** Failed to connect to server */
    ConnectionFailed = -2,
    
    /** Failed to send message */
    SendFailed = -3,
    
    /** Failed to receive message */
    ReceiveFailed = -4,
    
    /** Operation timed out */
    Timeout = -5,
    
    /** Server error */
    ServerError = -6,
    
    /** NSM protocol error */
    NsmError = -7
};

/**
 * @brief Error category for GPU telemetry
 */
class ErrorCategory : public std::error_category {
public:
    const char* name() const noexcept override {
        return "gpu_telemetry";
    }
    
    std::string message(int ev) const override {
        switch (static_cast<Error>(ev)) {
            case Error::Success:
                return "Success";
            case Error::InvalidArgument:
                return "Invalid argument";
            case Error::ConnectionFailed:
                return "Connection failed";
            case Error::SendFailed:
                return "Send failed";
            case Error::ReceiveFailed:
                return "Receive failed";
            case Error::Timeout:
                return "Operation timed out";
            case Error::ServerError:
                return "Server error";
            case Error::NsmError:
                return "NSM protocol error";
            default:
                return "Unknown error";
        }
    }
};

/**
 * @brief Get the error category singleton
 */
const ErrorCategory& error_category() noexcept {
    static ErrorCategory category;
    return category;
}

/**
 * @brief Convert Error to error_code
 */
std::error_code make_error_code(Error e) noexcept {
    return {static_cast<int>(e), error_category()};
}

} // namespace gpu::telemetry

namespace std {
    template<>
    struct is_error_code_enum<gpu::telemetry::Error> : true_type {};
}

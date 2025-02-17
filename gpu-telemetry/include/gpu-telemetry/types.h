/**
 * @brief Common type definitions for GPU telemetry
 *
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <string>

namespace gpu::telemetry {

/**
 * @brief Server configuration options
 */
struct ServerConfig {
    /** Maximum number of concurrent clients */
    std::size_t maxClients{10};
    
    /** Path to Unix domain socket */
    std::string socketPath{"/tmp/gpu-telemetry.sock"};
    
    /** Socket permissions (octal) */
    uint32_t socketPerms{0666};
};

/**
 * @brief Message types for internal communication
 */
enum class MessageType : uint8_t {
    /** NSM temperature request */
    TemperatureRequest = 0x01,
    
    /** NSM temperature response */
    TemperatureResponse = 0x02,
    
    /** Error response */
    Error = 0xFF
};

/**
 * @brief Error codes for internal communication
 */
enum class ErrorCode : int32_t {
    /** Operation successful */
    Success = 0,
    
    /** Invalid arguments */
    InvalidArgument = -1,
    
    /** Connection failed */
    ConnectionFailed = -2,
    
    /** Send failed */
    SendFailed = -3,
    
    /** Receive failed */
    ReceiveFailed = -4,
    
    /** Operation timed out */
    Timeout = -5
};

} // namespace gpu::telemetry

/**
 * @brief Transport abstraction for GPU telemetry
 *
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdexec/execution.hpp>
#include <span>
#include <functional>
#include <cstdint>

namespace gpu::telemetry {

/**
 * @brief Callback type for transport responses
 * @param response Response data to send back to client
 */
using ResponseCallback = std::function<void(std::span<const std::uint8_t>)>;

/**
 * @brief Abstract transport interface for handling NSM messages
 */
class Transport {
public:
    virtual ~Transport() = default;

    /**
     * @brief Send NSM message to device
     * @param message NSM message data
     * @param callback Callback for response data
     * @return Sender that completes when message is handled
     */
    virtual stdexec::sender auto sendMessage(
        std::span<const std::uint8_t> message,
        ResponseCallback callback) = 0;

protected:
    Transport() = default;
    Transport(const Transport&) = default;
    Transport& operator=(const Transport&) = default;
};

} // namespace gpu::telemetry

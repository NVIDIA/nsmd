/**
 * @brief Mock device for testing GPU telemetry
 *
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "transport.hpp"
#include <chrono>
#include <vector>
#include <optional>

namespace gpu::telemetry {

/**
 * @brief Configuration for mock device behavior
 */
struct MockDeviceConfig {
    /** Fixed response to return (empty for echo) */
    std::optional<std::vector<uint8_t>> fixedResponse;

    /** Delay before sending response */
    std::chrono::milliseconds responseDelay{0};

    /** Whether to simulate errors */
    bool simulateErrors{false};

    /** Error rate (0.0 - 1.0) when simulating errors */
    float errorRate{0.1f};

    /** Temperature range for mock readings */
    struct {
        float min{30.0f};
        float max{80.0f};
    } temperature;
};

/**
 * @brief Mock device implementation for testing
 */
class MockDevice : public Transport {
public:
    /**
     * @brief Construct mock device with default config
     */
    MockDevice() = default;

    /**
     * @brief Construct mock device with config
     * @param config Initial configuration
     */
    explicit MockDevice(MockDeviceConfig config);

    /**
     * @brief Update mock device configuration
     * @param config New configuration
     */
    void updateConfig(const MockDeviceConfig& config);

    /**
     * @brief Send message to mock device
     * @param message NSM message data
     * @param callback Response callback
     * @return Sender that completes when response is ready
     */
    stdexec::sender auto sendMessage(
        std::span<const std::uint8_t> message,
        ResponseCallback callback) override;

private:
    /**
     * @brief Generate mock temperature reading
     * @return NSM temperature response
     */
    std::vector<uint8_t> generateTemperatureResponse();

    /**
     * @brief Check if should simulate error
     * @return True if should error
     */
    bool shouldError() const;

    MockDeviceConfig config_;
};

} // namespace gpu::telemetry

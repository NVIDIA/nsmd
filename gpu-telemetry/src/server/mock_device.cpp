/**
 * @brief Mock device implementation
 *
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mock_device.hpp"
#include "libnsm/base.h"
#include <random>
#include <chrono>
#include <thread>
#include <stdexcept>

namespace gpu::telemetry {

MockDevice::MockDevice(MockDeviceConfig config)
    : config_(std::move(config)) {
}

void MockDevice::updateConfig(const MockDeviceConfig& config) {
    config_ = config;
}

stdexec::sender auto MockDevice::sendMessage(
    std::span<const std::uint8_t> message,
    ResponseCallback callback) {
    
    return stdexec::just() |
           stdexec::then([this, message, callback]() {
               // Simulate processing delay
               if (config_.responseDelay.count() > 0) {
                   std::this_thread::sleep_for(config_.responseDelay);
               }

               // Check for simulated errors
               if (config_.simulateErrors && shouldError()) {
                   throw std::runtime_error("Simulated device error");
               }

               // Parse NSM message
               const nsm_msg* request = 
                   reinterpret_cast<const nsm_msg*>(message.data());

               // Generate response
               std::vector<uint8_t> response;

               if (config_.fixedResponse) {
                   // Return fixed response if configured
                   response = *config_.fixedResponse;
               }
               else if (request->header[1] == NSM_TYPE_TEMPERATURE &&
                        request->header[2] == NSM_GET_TEMPERATURE_READING) {
                   // Generate temperature response
                   response = generateTemperatureResponse();
               }
               else {
                   // Echo back request for unknown messages
                   response.assign(message.begin(), message.end());
               }

               // Send response
               callback(response);
           });
}

std::vector<uint8_t> MockDevice::generateTemperatureResponse() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    // Generate random temperature in configured range
    float range = config_.temperature.max - config_.temperature.min;
    float temp = config_.temperature.min + (dist(gen) * range);

    // Create NSM response message
    std::vector<uint8_t> response(sizeof(nsm_msg));
    nsm_msg* msg = reinterpret_cast<nsm_msg*>(response.data());

    // Set response header
    msg->header[0] = NSM_SUCCESS;  // Completion code
    *reinterpret_cast<uint16_t*>(&msg->header[1]) = sizeof(float);  // Data size
    *reinterpret_cast<uint16_t*>(&msg->header[3]) = 0;  // Reason code

    // Set temperature data
    *reinterpret_cast<float*>(msg->payload) = temp;

    return response;
}

bool MockDevice::shouldError() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    return dist(gen) < config_.errorRate;
}

} // namespace gpu::telemetry

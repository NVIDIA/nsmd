/**
 * @brief Unit tests for mock device
 *
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "server/mock_device.hpp"
#include "libnsm/base.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>

using namespace testing;
using namespace gpu::telemetry;
using namespace std::chrono_literals;

class MockDeviceTest : public Test {
protected:
    void SetUp() override {
        config_.temperature.min = 30.0f;
        config_.temperature.max = 80.0f;
        config_.responseDelay = 0ms;
        config_.simulateErrors = false;
        device_ = std::make_unique<MockDevice>(config_);
    }

    // Helper to create temperature request
    std::vector<uint8_t> createTempRequest() {
        nsm_msg request = {};
        encode_common_req(0x01, NSM_TYPE_TEMPERATURE,
                         NSM_GET_TEMPERATURE_READING, &request);
        return std::vector<uint8_t>(
            reinterpret_cast<uint8_t*>(&request),
            reinterpret_cast<uint8_t*>(&request) + sizeof(request));
    }

    // Helper to verify temperature response
    void verifyTempResponse(std::span<const std::uint8_t> response) {
        ASSERT_GE(response.size(), sizeof(nsm_msg));
        const nsm_msg* msg = reinterpret_cast<const nsm_msg*>(response.data());

        // Check header
        EXPECT_EQ(msg->header[0], NSM_SUCCESS);
        uint16_t dataSize = *reinterpret_cast<const uint16_t*>(&msg->header[1]);
        EXPECT_EQ(dataSize, sizeof(float));
        uint16_t reason = *reinterpret_cast<const uint16_t*>(&msg->header[3]);
        EXPECT_EQ(reason, 0);

        // Check temperature value
        float temp = *reinterpret_cast<const float*>(msg->payload);
        EXPECT_GE(temp, config_.temperature.min);
        EXPECT_LE(temp, config_.temperature.max);
    }

    MockDeviceConfig config_;
    std::unique_ptr<MockDevice> device_;
    std::vector<uint8_t> lastResponse_;
    bool responseReceived_{false};
};

TEST_F(MockDeviceTest, TemperatureRequest) {
    auto request = createTempRequest();
    
    stdexec::sync_wait(device_->sendMessage(
        request,
        [this](auto response) {
            lastResponse_.assign(response.begin(), response.end());
            responseReceived_ = true;
        }));

    ASSERT_TRUE(responseReceived_);
    verifyTempResponse(lastResponse_);
}

TEST_F(MockDeviceTest, TemperatureRange) {
    // Test multiple temperature readings
    std::vector<float> temperatures;
    for (int i = 0; i < 100; i++) {
        auto request = createTempRequest();
        
        stdexec::sync_wait(device_->sendMessage(
            request,
            [&temperatures](auto response) {
                const nsm_msg* msg = 
                    reinterpret_cast<const nsm_msg*>(response.data());
                float temp = *reinterpret_cast<const float*>(msg->payload);
                temperatures.push_back(temp);
            }));
    }

    // Verify temperature distribution
    ASSERT_EQ(temperatures.size(), 100);
    for (float temp : temperatures) {
        EXPECT_GE(temp, config_.temperature.min);
        EXPECT_LE(temp, config_.temperature.max);
    }

    // Check that we got some variation
    auto [min, max] = std::minmax_element(
        temperatures.begin(), temperatures.end());
    EXPECT_GT(*max - *min, 1.0f);
}

TEST_F(MockDeviceTest, ResponseDelay) {
    // Configure delay
    config_.responseDelay = 100ms;
    device_->updateConfig(config_);

    auto request = createTempRequest();
    auto start = std::chrono::steady_clock::now();
    
    stdexec::sync_wait(device_->sendMessage(
        request,
        [this](auto response) {
            lastResponse_ = std::vector<uint8_t>(
                response.begin(), response.end());
            responseReceived_ = true;
        }));

    auto duration = std::chrono::steady_clock::now() - start;
    EXPECT_GE(duration, config_.responseDelay);
}

TEST_F(MockDeviceTest, SimulatedErrors) {
    // Configure high error rate
    config_.simulateErrors = true;
    config_.errorRate = 1.0f;
    device_->updateConfig(config_);

    auto request = createTempRequest();
    EXPECT_THROW(
        stdexec::sync_wait(device_->sendMessage(
            request,
            [](auto) {})),
        std::runtime_error);
}

TEST_F(MockDeviceTest, FixedResponse) {
    // Configure fixed response
    std::vector<uint8_t> fixedResponse = {0x01, 0x02, 0x03};
    config_.fixedResponse = fixedResponse;
    device_->updateConfig(config_);

    auto request = createTempRequest();
    stdexec::sync_wait(device_->sendMessage(
        request,
        [this](auto response) {
            lastResponse_ = std::vector<uint8_t>(
                response.begin(), response.end());
            responseReceived_ = true;
        }));

    ASSERT_TRUE(responseReceived_);
    EXPECT_EQ(lastResponse_, fixedResponse);
}

TEST_F(MockDeviceTest, UnknownMessage) {
    // Send unknown message type
    std::vector<uint8_t> request = {0xFF, 0xFF, 0xFF};
    
    stdexec::sync_wait(device_->sendMessage(
        request,
        [this](auto response) {
            lastResponse_ = std::vector<uint8_t>(
                response.begin(), response.end());
            responseReceived_ = true;
        }));

    ASSERT_TRUE(responseReceived_);
    EXPECT_EQ(lastResponse_, request);  // Should echo unknown messages
}

TEST_F(MockDeviceTest, ConfigUpdate) {
    // Update configuration
    config_.temperature.min = 90.0f;
    config_.temperature.max = 100.0f;
    device_->updateConfig(config_);

    auto request = createTempRequest();
    stdexec::sync_wait(device_->sendMessage(
        request,
        [this](auto response) {
            lastResponse_ = std::vector<uint8_t>(
                response.begin(), response.end());
            responseReceived_ = true;
        }));

    ASSERT_TRUE(responseReceived_);
    verifyTempResponse(lastResponse_);
}

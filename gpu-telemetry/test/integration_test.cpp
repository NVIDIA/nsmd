/**
 * @brief Integration tests for GPU telemetry
 *
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gpu-telemetry/client.h"
#include "server/server.hpp"
#include "server/mock_device.hpp"
#include "libnsm/base.h"
#include <gtest/gtest.h>
#include <thread>
#include <future>
#include <chrono>

using namespace testing;
using namespace gpu::telemetry;
using namespace std::chrono_literals;

class IntegrationTest : public Test {
protected:
    void SetUp() override {
        // Configure mock device
        MockDeviceConfig deviceConfig;
        deviceConfig.temperature.min = 30.0f;
        deviceConfig.temperature.max = 80.0f;
        deviceConfig.responseDelay = 10ms;

        // Configure server
        ServerConfig serverConfig;
        serverConfig.socketPath = "/tmp/gpu-telemetry-test.sock";
        serverConfig.maxClients = 5;
        serverConfig.socketPerms = 0666;

        // Create and start server
        server_ = std::make_unique<Server>(
            std::make_unique<MockDevice>(deviceConfig),
            serverConfig);

        serverFuture_ = std::async(std::launch::async, [this]() {
            try {
                stdexec::sync_wait(server_->start());
                while (running_) {
                    std::this_thread::sleep_for(10ms);
                }
                stdexec::sync_wait(server_->stop());
            }
            catch (const std::exception& e) {
                ADD_FAILURE() << "Server error: " << e.what();
            }
        });

        // Wait for server to start
        std::this_thread::sleep_for(100ms);
    }

    void TearDown() override {
        running_ = false;
        if (serverFuture_.valid()) {
            serverFuture_.wait();
        }
        server_.reset();
        unlink("/tmp/gpu-telemetry-test.sock");
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

    std::unique_ptr<Server> server_;
    std::future<void> serverFuture_;
    std::atomic<bool> running_{true};
    std::vector<uint8_t> lastResponse_;
    bool responseReceived_{false};
};

// Callback for client responses
void response_callback(void* user_data, 
                      const uint8_t* response, 
                      size_t response_len) {
    auto test = static_cast<IntegrationTest*>(user_data);
    test->lastResponse_.assign(response, response + response_len);
    test->responseReceived_ = true;
}

TEST_F(IntegrationTest, SingleClient) {
    // Initialize client
    gpu_telemetry_ctx* ctx;
    ASSERT_EQ(gpu_telemetry_init(&ctx), 0);

    // Create and send temperature request
    auto request = createTempRequest();
    ASSERT_EQ(gpu_telemetry_send_message(ctx,
                                        request.data(),
                                        request.size(),
                                        response_callback,
                                        this), 0);

    // Wait for response
    for (int i = 0; i < 100 && !responseReceived_; i++) {
        std::this_thread::sleep_for(10ms);
    }
    ASSERT_TRUE(responseReceived_);

    // Verify response
    ASSERT_GE(lastResponse_.size(), sizeof(nsm_msg));
    const nsm_msg* response = 
        reinterpret_cast<const nsm_msg*>(lastResponse_.data());
    ASSERT_EQ(response->header[0], NSM_SUCCESS);

    uint16_t dataSize = *reinterpret_cast<const uint16_t*>(&response->header[1]);
    ASSERT_EQ(dataSize, sizeof(float));

    float temp = *reinterpret_cast<const float*>(response->payload);
    ASSERT_GE(temp, 30.0f);
    ASSERT_LE(temp, 80.0f);

    gpu_telemetry_free(ctx);
}

TEST_F(IntegrationTest, MultipleClients) {
    constexpr int NUM_CLIENTS = 3;
    std::vector<gpu_telemetry_ctx*> clients(NUM_CLIENTS);
    std::vector<bool> responses(NUM_CLIENTS, false);

    // Initialize clients
    for (int i = 0; i < NUM_CLIENTS; i++) {
        ASSERT_EQ(gpu_telemetry_init(&clients[i]), 0);
    }

    // Send requests from all clients
    auto request = createTempRequest();
    for (int i = 0; i < NUM_CLIENTS; i++) {
        ASSERT_EQ(gpu_telemetry_send_message(
            clients[i],
            request.data(),
            request.size(),
            [](void* user_data, const uint8_t*, size_t) {
                auto received = static_cast<bool*>(user_data);
                *received = true;
            },
            &responses[i]), 0);
    }

    // Wait for all responses
    auto deadline = std::chrono::steady_clock::now() + 1s;
    while (std::chrono::steady_clock::now() < deadline) {
        if (std::all_of(responses.begin(), responses.end(),
                        [](bool r) { return r; })) {
            break;
        }
        std::this_thread::sleep_for(10ms);
    }

    // Verify all responses received
    for (int i = 0; i < NUM_CLIENTS; i++) {
        EXPECT_TRUE(responses[i]) << "Client " << i << " didn't get response";
        gpu_telemetry_free(clients[i]);
    }
}

TEST_F(IntegrationTest, RapidRequests) {
    gpu_telemetry_ctx* ctx;
    ASSERT_EQ(gpu_telemetry_init(&ctx), 0);

    // Send multiple rapid requests
    constexpr int NUM_REQUESTS = 50;
    std::vector<bool> responses(NUM_REQUESTS, false);
    auto request = createTempRequest();

    for (int i = 0; i < NUM_REQUESTS; i++) {
        ASSERT_EQ(gpu_telemetry_send_message(
            ctx,
            request.data(),
            request.size(),
            [](void* user_data, const uint8_t*, size_t) {
                auto received = static_cast<bool*>(user_data);
                *received = true;
            },
            &responses[i]), 0);
    }

    // Wait for all responses
    auto deadline = std::chrono::steady_clock::now() + 2s;
    while (std::chrono::steady_clock::now() < deadline) {
        if (std::all_of(responses.begin(), responses.end(),
                        [](bool r) { return r; })) {
            break;
        }
        std::this_thread::sleep_for(10ms);
    }

    // Verify all responses received
    EXPECT_TRUE(std::all_of(responses.begin(), responses.end(),
                           [](bool r) { return r; }));

    gpu_telemetry_free(ctx);
}

TEST_F(IntegrationTest, ServerRestart) {
    gpu_telemetry_ctx* ctx;
    ASSERT_EQ(gpu_telemetry_init(&ctx), 0);

    // Stop server
    running_ = false;
    serverFuture_.wait();

    // Restart server
    running_ = true;
    serverFuture_ = std::async(std::launch::async, [this]() {
        try {
            stdexec::sync_wait(server_->start());
            while (running_) {
                std::this_thread::sleep_for(10ms);
            }
            stdexec::sync_wait(server_->stop());
        }
        catch (const std::exception& e) {
            ADD_FAILURE() << "Server error: " << e.what();
        }
    });

    std::this_thread::sleep_for(100ms);

    // Send request after restart
    auto request = createTempRequest();
    ASSERT_EQ(gpu_telemetry_send_message(ctx,
                                        request.data(),
                                        request.size(),
                                        response_callback,
                                        this), 0);

    // Wait for response
    for (int i = 0; i < 100 && !responseReceived_; i++) {
        std::this_thread::sleep_for(10ms);
    }
    ASSERT_TRUE(responseReceived_);

    gpu_telemetry_free(ctx);
}

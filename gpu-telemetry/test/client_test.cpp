/**
 * @brief Unit tests for GPU telemetry client
 *
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gpu-telemetry/client.h"
#include "libnsm/base.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

using namespace testing;

class ClientTest : public Test {
protected:
    void SetUp() override {
        // Create test socket
        serverFd_ = socket(AF_UNIX, SOCK_STREAM, 0);
        ASSERT_GE(serverFd_, 0);

        sockaddr_un addr = {};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, socketPath_, sizeof(addr.sun_path) - 1);

        unlink(socketPath_);
        ASSERT_EQ(bind(serverFd_, (struct sockaddr*)&addr, sizeof(addr)), 0);
        ASSERT_EQ(listen(serverFd_, 1), 0);

        // Start server thread
        serverThread_ = std::thread([this]() { serverLoop(); });
    }

    void TearDown() override {
        // Stop server
        running_ = false;
        if (serverThread_.joinable()) {
            serverThread_.join();
        }

        // Cleanup
        if (serverFd_ >= 0) {
            close(serverFd_);
        }
        if (clientFd_ >= 0) {
            close(clientFd_);
        }
        unlink(socketPath_);
    }

    // Mock server loop
    void serverLoop() {
        while (running_) {
            struct sockaddr_un addr;
            socklen_t addrLen = sizeof(addr);
            
            clientFd_ = accept(serverFd_, (struct sockaddr*)&addr, &addrLen);
            if (clientFd_ < 0) {
                continue;
            }

            while (running_) {
                // Read message size
                uint32_t size;
                if (read(clientFd_, &size, sizeof(size)) <= 0) {
                    break;
                }

                // Read message
                std::vector<uint8_t> message(size);
                if (read(clientFd_, message.data(), size) <= 0) {
                    break;
                }

                // Handle message
                handleMessage(message);
            }

            close(clientFd_);
            clientFd_ = -1;
        }
    }

    // Mock message handler
    void handleMessage(const std::vector<uint8_t>& message) {
        const nsm_msg* request = 
            reinterpret_cast<const nsm_msg*>(message.data());

        // Check for temperature request
        if (request->header[1] == NSM_TYPE_TEMPERATURE &&
            request->header[2] == NSM_GET_TEMPERATURE_READING) {
            
            // Send mock temperature response
            nsm_msg response = {};
            response.header[0] = NSM_SUCCESS;
            *reinterpret_cast<uint16_t*>(&response.header[1]) = sizeof(float);
            *reinterpret_cast<uint16_t*>(&response.header[3]) = 0;
            
            float temp = 45.5f;
            memcpy(response.payload, &temp, sizeof(temp));

            uint32_t size = sizeof(response);
            write(clientFd_, &size, sizeof(size));
            write(clientFd_, &response, sizeof(response));
        }
    }

    static void responseCallback(void* user_data, 
                               const uint8_t* response, 
                               size_t response_len) {
        auto test = static_cast<ClientTest*>(user_data);
        test->lastResponse_.assign(response, response + response_len);
        test->responseReceived_ = true;
    }

    const char* socketPath_ = "/tmp/gpu-telemetry-test.sock";
    int serverFd_{-1};
    int clientFd_{-1};
    std::atomic<bool> running_{true};
    std::thread serverThread_;
    
    std::vector<uint8_t> lastResponse_;
    bool responseReceived_{false};
};

TEST_F(ClientTest, InitializeClient) {
    gpu_telemetry_ctx* ctx;
    ASSERT_EQ(gpu_telemetry_init(&ctx), 0);
    ASSERT_NE(ctx, nullptr);
    gpu_telemetry_free(ctx);
}

TEST_F(ClientTest, InitializeNullContext) {
    ASSERT_LT(gpu_telemetry_init(nullptr), 0);
}

TEST_F(ClientTest, SendTemperatureRequest) {
    // Initialize client
    gpu_telemetry_ctx* ctx;
    ASSERT_EQ(gpu_telemetry_init(&ctx), 0);

    // Create temperature request
    nsm_msg request = {};
    ASSERT_EQ(encode_common_req(0x01, NSM_TYPE_TEMPERATURE,
                               NSM_GET_TEMPERATURE_READING, &request), 0);

    // Send request
    ASSERT_EQ(gpu_telemetry_send_message(ctx, 
                                        reinterpret_cast<uint8_t*>(&request),
                                        sizeof(request),
                                        responseCallback, 
                                        this), 0);

    // Wait for response
    for (int i = 0; i < 100 && !responseReceived_; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
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
    ASSERT_FLOAT_EQ(temp, 45.5f);

    gpu_telemetry_free(ctx);
}

TEST_F(ClientTest, SendInvalidMessage) {
    gpu_telemetry_ctx* ctx;
    ASSERT_EQ(gpu_telemetry_init(&ctx), 0);

    // Test null message
    ASSERT_LT(gpu_telemetry_send_message(ctx, nullptr, 10,
                                        responseCallback, this), 0);

    // Test zero length
    uint8_t data[1] = {0};
    ASSERT_LT(gpu_telemetry_send_message(ctx, data, 0,
                                        responseCallback, this), 0);

    // Test null callback
    ASSERT_LT(gpu_telemetry_send_message(ctx, data, 1,
                                        nullptr, this), 0);

    gpu_telemetry_free(ctx);
}

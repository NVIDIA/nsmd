/**
 * @brief Unit tests for GPU telemetry server
 *
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "server/server.hpp"
#include "server/mock_device.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

using namespace testing;
using namespace gpu::telemetry;

// Mock transport for testing
class MockTransport : public Transport {
public:
    MOCK_METHOD(stdexec::sender auto, sendMessage, 
                (std::span<const std::uint8_t>, ResponseCallback),
                (override));
};

class ServerTest : public Test {
protected:
    void SetUp() override {
        // Create mock transport
        mockTransport_ = new MockTransport();
        
        // Configure server
        config_.socketPath = "/tmp/gpu-telemetry-test.sock";
        config_.maxClients = 2;
        config_.socketPerms = 0666;

        // Create server
        server_ = std::make_unique<Server>(
            std::unique_ptr<Transport>(mockTransport_),
            config_);
    }

    void TearDown() override {
        server_.reset();
        unlink(config_.socketPath.c_str());
    }

    // Helper to create client connection
    int connectClient() {
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        EXPECT_GE(fd, 0);

        struct sockaddr_un addr = {};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, config_.socketPath.c_str(),
                sizeof(addr.sun_path) - 1);

        if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(fd);
            return -1;
        }

        return fd;
    }

    // Helper to send message
    bool sendMessage(int fd, const std::vector<uint8_t>& message) {
        uint32_t size = message.size();
        if (write(fd, &size, sizeof(size)) != sizeof(size)) {
            return false;
        }
        if (write(fd, message.data(), size) != size) {
            return false;
        }
        return true;
    }

    // Helper to read response
    std::vector<uint8_t> readResponse(int fd) {
        uint32_t size;
        if (read(fd, &size, sizeof(size)) != sizeof(size)) {
            return {};
        }

        std::vector<uint8_t> response(size);
        if (read(fd, response.data(), size) != size) {
            return {};
        }

        return response;
    }

    ServerConfig config_;
    MockTransport* mockTransport_;  // Owned by server_
    std::unique_ptr<Server> server_;
};

TEST_F(ServerTest, StartServer) {
    EXPECT_NO_THROW(stdexec::sync_wait(server_->start()));
}

TEST_F(ServerTest, StopServer) {
    stdexec::sync_wait(server_->start());
    EXPECT_NO_THROW(stdexec::sync_wait(server_->stop()));
}

TEST_F(ServerTest, ClientConnection) {
    stdexec::sync_wait(server_->start());
    
    int fd = connectClient();
    ASSERT_GE(fd, 0);
    
    close(fd);
}

TEST_F(ServerTest, MaxClients) {
    stdexec::sync_wait(server_->start());

    // Connect max clients
    std::vector<int> clients;
    for (size_t i = 0; i < config_.maxClients; i++) {
        int fd = connectClient();
        ASSERT_GE(fd, 0);
        clients.push_back(fd);
    }

    // Additional connection should fail
    EXPECT_LT(connectClient(), 0);

    // Cleanup
    for (int fd : clients) {
        close(fd);
    }
}

TEST_F(ServerTest, MessageHandling) {
    stdexec::sync_wait(server_->start());

    // Connect client
    int fd = connectClient();
    ASSERT_GE(fd, 0);

    // Prepare test message
    std::vector<uint8_t> testMessage = {0x01, 0x02, 0x03};
    std::vector<uint8_t> testResponse = {0x04, 0x05, 0x06};

    // Set up mock expectation
    EXPECT_CALL(*mockTransport_, sendMessage(_, _))
        .WillOnce([&testResponse](auto message, auto callback) {
            callback(testResponse);
            return stdexec::just();
        });

    // Send message
    ASSERT_TRUE(sendMessage(fd, testMessage));

    // Read response
    auto response = readResponse(fd);
    EXPECT_EQ(response, testResponse);

    close(fd);
}

TEST_F(ServerTest, ClientDisconnect) {
    stdexec::sync_wait(server_->start());

    // Connect and immediately disconnect
    int fd = connectClient();
    ASSERT_GE(fd, 0);
    close(fd);

    // Server should handle disconnect gracefully
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_NO_THROW(stdexec::sync_wait(server_->stop()));
}

TEST_F(ServerTest, InvalidMessages) {
    stdexec::sync_wait(server_->start());

    int fd = connectClient();
    ASSERT_GE(fd, 0);

    // Send invalid size
    uint32_t invalidSize = 0xFFFFFFFF;
    write(fd, &invalidSize, sizeof(invalidSize));

    // Server should handle invalid message gracefully
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_NO_THROW(stdexec::sync_wait(server_->stop()));

    close(fd);
}

TEST_F(ServerTest, TransportError) {
    stdexec::sync_wait(server_->start());

    int fd = connectClient();
    ASSERT_GE(fd, 0);

    // Set up mock to simulate error
    EXPECT_CALL(*mockTransport_, sendMessage(_, _))
        .WillOnce([](auto, auto) {
            throw std::runtime_error("Transport error");
            return stdexec::just();
        });

    // Send message
    std::vector<uint8_t> testMessage = {0x01, 0x02, 0x03};
    ASSERT_TRUE(sendMessage(fd, testMessage));

    // Server should handle transport error gracefully
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_NO_THROW(stdexec::sync_wait(server_->stop()));

    close(fd);
}

/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

// NSMD_SOCKET_REDIRECT must NOT be defined here — the macros would expand
// MOCK_METHOD-generated method definitions that share syscall names.
#include "nsmd/socketIo.hpp"

#include <libnsm/base.h>

#include <cstdint>
#include <queue>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

/**
 * @brief GMock implementation of SocketIoInterface.
 *
 * A single NiceMock<MockSocketIo> instance is shared across all tests in a
 * binary (see mockSocketIo.cpp).  Access it via getMockSocketIo().
 */
class MockSocketIo : public SocketIoInterface
{
  public:
    MOCK_METHOD(int, socket, (int, int, int), (override));
    MOCK_METHOD(int, bind, (int, const struct sockaddr*, socklen_t),
                (override));
    MOCK_METHOD(int, connect, (int, const struct sockaddr*, socklen_t),
                (override));
    MOCK_METHOD(int, close, (int), (override));

    MOCK_METHOD(ssize_t, send, (int, const void*, size_t, int), (override));
    MOCK_METHOD(ssize_t, sendto,
                (int, const void*, size_t, int, const struct sockaddr*,
                 socklen_t),
                (override));
    MOCK_METHOD(ssize_t, sendmsg, (int, const struct msghdr*, int), (override));
    MOCK_METHOD(ssize_t, write, (int, const void*, size_t), (override));

    MOCK_METHOD(ssize_t, recv, (int, void*, size_t, int), (override));
    MOCK_METHOD(ssize_t, recvfrom,
                (int, void*, size_t, int, struct sockaddr*, socklen_t*),
                (override));
    MOCK_METHOD(ssize_t, recvmsg, (int, struct msghdr*, int), (override));

    MOCK_METHOD(int, poll, (struct pollfd*, nfds_t, int), (override));

    MOCK_METHOD(int, getsockopt, (int, int, int, void*, socklen_t*),
                (override));
    MOCK_METHOD(int, setsockopt, (int, int, int, const void*, socklen_t),
                (override));
};

/** Returns the MockSocketIo singleton used by getSocketIo() in test binaries.
 */
MockSocketIo& getMockSocketIo();

/**
 * @brief Captures sent MCTP messages and provides canned responses.
 */
class MctpMessageCapture
{
  public:
    struct SentMessage
    {
        uint8_t tag;
        uint8_t eid;
        uint8_t msgType;
        std::vector<uint8_t> payload;
    };

    struct Response
    {
        uint8_t eid;
        uint8_t tag;
        std::vector<uint8_t> mctpPayload; // 3-byte header + NSM payload
    };

    void enqueueMctpResponse(uint8_t eid, uint8_t tag,
                             const std::vector<uint8_t>& nsmPayload)
    {
        Response resp;
        resp.eid = eid;
        resp.tag = tag;
        resp.mctpPayload.push_back(tag);
        resp.mctpPayload.push_back(eid);
        resp.mctpPayload.push_back(0x7E); // VDM type
        resp.mctpPayload.insert(resp.mctpPayload.end(), nsmPayload.begin(),
                                nsmPayload.end());
        responses_.push(resp);
    }

    void recordSentMessage(uint8_t tag, uint8_t eid, uint8_t msgType,
                           const std::vector<uint8_t>& payload)
    {
        sent_.push_back({tag, eid, msgType, payload});
    }

    const std::vector<SentMessage>& getSentMessages() const
    {
        return sent_;
    }

    bool getNextResponse(Response& resp)
    {
        if (responses_.empty())
        {
            return false;
        }
        resp = responses_.front();
        responses_.pop();
        return true;
    }

    void clearMessages()
    {
        sent_.clear();
        while (!responses_.empty())
        {
            responses_.pop();
        }
    }

  private:
    std::queue<Response> responses_;
    std::vector<SentMessage> sent_;
};

/**
 * @brief Helper for building NSM test messages.
 */
class NsmMessageBuilder
{
  public:
    static std::vector<uint8_t> successResponse(uint8_t cmdId,
                                                uint8_t instanceId)
    {
        std::vector<uint8_t> resp;
        resp.push_back(0xDE);
        resp.push_back(0x10);
        resp.push_back((instanceId & 0x1F));
        resp.push_back(0x00);
        resp.push_back(cmdId);
        resp.push_back(0x00);
        resp.push_back(0x00);
        resp.push_back(0x00);
        resp.push_back(0x00);
        return resp;
    }

    static std::vector<uint8_t> errorResponse(uint8_t cmdId, uint8_t instanceId,
                                              uint8_t errorCode)
    {
        std::vector<uint8_t> resp;
        resp.push_back(0xDE);
        resp.push_back(0x10);
        resp.push_back((instanceId & 0x1F));
        resp.push_back(0x00);
        resp.push_back(cmdId);
        resp.push_back(errorCode);
        resp.push_back(0x00);
        resp.push_back(0x00);
        resp.push_back(0x00);
        return resp;
    }

    static std::vector<uint8_t>
        request(uint8_t cmdId, uint8_t instanceId,
                const std::vector<uint8_t>& payload = {})
    {
        std::vector<uint8_t> req;
        req.push_back(0xDE);
        req.push_back(0x10);
        req.push_back(0x80 | (instanceId & 0x1F));
        req.push_back(0x00);
        req.push_back(cmdId);
        req.insert(req.end(), payload.begin(), payload.end());
        return req;
    }
};

/**
 * @brief Base fixture for MCTP socket tests.
 *
 * Provides access to the shared MockSocketIo singleton and resets it between
 * tests.  No injection/restore needed — the mock is always active in test
 * binaries.
 */
class MctpTestFixture : public ::testing::Test
{
  protected:
    MockSocketIo* mockIo_ = &getMockSocketIo();
    std::unique_ptr<MctpMessageCapture> capture_;

    void SetUp() override
    {
        capture_ = std::make_unique<MctpMessageCapture>();
    }

    void TearDown() override
    {
        ::testing::Mock::VerifyAndClearExpectations(mockIo_);
        capture_->clearMessages();
    }

    void expectSocketCreation(int fakeFd = 42, int domain = AF_MCTP)
    {
        using ::testing::_;
        using ::testing::Return;
        EXPECT_CALL(*mockIo_, socket(domain, _, _)).WillOnce(Return(fakeFd));
    }

    void expectMctpSendMsg(int fd, uint8_t expectedEid, uint8_t expectedTag)
    {
        using ::testing::_;
        EXPECT_CALL(*mockIo_, sendmsg(fd, _, 0))
            .WillOnce([=, this](int, const struct msghdr* msg, int) {
            EXPECT_EQ(msg->msg_iovlen, 2);
            struct iovec* iov = msg->msg_iov;
            EXPECT_EQ(iov[0].iov_len, 3);
            const uint8_t* hdr = static_cast<const uint8_t*>(iov[0].iov_base);
            EXPECT_EQ(hdr[0], expectedTag);
            EXPECT_EQ(hdr[1], expectedEid);
            EXPECT_EQ(hdr[2], 0x7E);
            const uint8_t* payload =
                static_cast<const uint8_t*>(iov[1].iov_base);
            std::vector<uint8_t> payloadVec(payload, payload + iov[1].iov_len);
            capture_->recordSentMessage(expectedTag, expectedEid, 0x7E,
                                        payloadVec);
            return iov[0].iov_len + iov[1].iov_len;
        });
    }

    void expectMctpRecvWithPeek(int fd, const std::vector<uint8_t>& response)
    {
        using ::testing::_;
        using ::testing::Return;
        EXPECT_CALL(*mockIo_, recv(fd, nullptr, 0, MSG_PEEK | MSG_TRUNC))
            .WillOnce(Return(response.size()));
        EXPECT_CALL(*mockIo_, recvmsg(fd, _, 0))
            .WillOnce([=](int, struct msghdr* msg, int) {
            size_t offset = 0;
            for (size_t i = 0; i < msg->msg_iovlen && offset < response.size();
                 i++)
            {
                size_t toCopy = std::min(msg->msg_iov[i].iov_len,
                                         response.size() - offset);
                memcpy(msg->msg_iov[i].iov_base, response.data() + offset,
                       toCopy);
                offset += toCopy;
            }
            return static_cast<ssize_t>(response.size());
        });
    }

    void expectPoll(int fd, int timeout, int result = 1)
    {
        using ::testing::_;
        using ::testing::Return;
        EXPECT_CALL(*mockIo_, poll(_, 1, timeout))
            .WillOnce([=](struct pollfd* fds, nfds_t, int) {
            EXPECT_EQ(fds[0].fd, fd);
            EXPECT_EQ(fds[0].events, POLLIN);
            if (result > 0)
            {
                fds[0].revents = POLLIN;
            }
            return result;
        });
    }
};

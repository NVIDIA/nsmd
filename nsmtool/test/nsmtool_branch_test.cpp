/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
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

#include "base.h"

#include "cmd_helper.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ordered_json = nlohmann::ordered_json;

// =============================================================================
// parseBitfieldVar branch coverage tests
// =============================================================================

TEST(ParseBitfieldVarBranch, AllBitsZero)
{
    ordered_json result;
    bitfield8_t val[1] = {};
    val[0].byte = 0x00;
    nsmtool::helper::parseBitfieldVar(result, "key", val, 1);
    // No bits set - key should not exist or be empty
    EXPECT_TRUE(!result.contains("key") || result["key"].empty());
}

TEST(ParseBitfieldVarBranch, AllBitsSet)
{
    ordered_json result;
    bitfield8_t val[1] = {};
    val[0].byte = 0xFF;
    nsmtool::helper::parseBitfieldVar(result, "key", val, 1);
    EXPECT_EQ(result["key"].size(), 8u);
    EXPECT_THAT(result["key"], ElementsAre(0, 1, 2, 3, 4, 5, 6, 7));
}

TEST(ParseBitfieldVarBranch, Bit0OnlySet)
{
    ordered_json result;
    bitfield8_t val[1] = {};
    val[0].byte = 0x01;
    nsmtool::helper::parseBitfieldVar(result, "key", val, 1);
    EXPECT_EQ(result["key"].size(), 1u);
    EXPECT_THAT(result["key"], ElementsAre(0));
}

TEST(ParseBitfieldVarBranch, Bit1OnlySet)
{
    ordered_json result;
    bitfield8_t val[1] = {};
    val[0].byte = 0x02;
    nsmtool::helper::parseBitfieldVar(result, "key", val, 1);
    EXPECT_EQ(result["key"].size(), 1u);
    EXPECT_THAT(result["key"], ElementsAre(1));
}

TEST(ParseBitfieldVarBranch, Bit2OnlySet)
{
    ordered_json result;
    bitfield8_t val[1] = {};
    val[0].byte = 0x04;
    nsmtool::helper::parseBitfieldVar(result, "key", val, 1);
    EXPECT_EQ(result["key"].size(), 1u);
    EXPECT_THAT(result["key"], ElementsAre(2));
}

TEST(ParseBitfieldVarBranch, Bit3OnlySet)
{
    ordered_json result;
    bitfield8_t val[1] = {};
    val[0].byte = 0x08;
    nsmtool::helper::parseBitfieldVar(result, "key", val, 1);
    EXPECT_EQ(result["key"].size(), 1u);
    EXPECT_THAT(result["key"], ElementsAre(3));
}

TEST(ParseBitfieldVarBranch, Bit4OnlySet)
{
    ordered_json result;
    bitfield8_t val[1] = {};
    val[0].byte = 0x10;
    nsmtool::helper::parseBitfieldVar(result, "key", val, 1);
    EXPECT_EQ(result["key"].size(), 1u);
    EXPECT_THAT(result["key"], ElementsAre(4));
}

TEST(ParseBitfieldVarBranch, Bit5OnlySet)
{
    ordered_json result;
    bitfield8_t val[1] = {};
    val[0].byte = 0x20;
    nsmtool::helper::parseBitfieldVar(result, "key", val, 1);
    EXPECT_EQ(result["key"].size(), 1u);
    EXPECT_THAT(result["key"], ElementsAre(5));
}

TEST(ParseBitfieldVarBranch, Bit6OnlySet)
{
    ordered_json result;
    bitfield8_t val[1] = {};
    val[0].byte = 0x40;
    nsmtool::helper::parseBitfieldVar(result, "key", val, 1);
    EXPECT_EQ(result["key"].size(), 1u);
    EXPECT_THAT(result["key"], ElementsAre(6));
}

TEST(ParseBitfieldVarBranch, Bit7OnlySet)
{
    ordered_json result;
    bitfield8_t val[1] = {};
    val[0].byte = 0x80;
    nsmtool::helper::parseBitfieldVar(result, "key", val, 1);
    EXPECT_EQ(result["key"].size(), 1u);
    EXPECT_THAT(result["key"], ElementsAre(7));
}

TEST(ParseBitfieldVarBranch, MultipleBytesWithOffset)
{
    ordered_json result;
    bitfield8_t val[3] = {};
    val[0].byte = 0x00;
    val[1].byte = 0x01; // bit 8
    val[2].byte = 0x80; // bit 23
    nsmtool::helper::parseBitfieldVar(result, "key", val, 3);
    EXPECT_EQ(result["key"].size(), 2u);
    EXPECT_THAT(result["key"], ElementsAre(8, 23));
}

TEST(ParseBitfieldVarBranch, SizeZero)
{
    ordered_json result;
    bitfield8_t val[1] = {};
    val[0].byte = 0xFF;
    nsmtool::helper::parseBitfieldVar(result, "key", val, 0);
    EXPECT_TRUE(!result.contains("key") || result["key"].empty());
}

TEST(ParseBitfieldVarBranch, AlternatingBits)
{
    ordered_json result;
    bitfield8_t val[1] = {};
    val[0].byte = 0xAA; // bits 1,3,5,7
    nsmtool::helper::parseBitfieldVar(result, "key", val, 1);
    EXPECT_EQ(result["key"].size(), 4u);
    EXPECT_THAT(result["key"], ElementsAre(1, 3, 5, 7));
}

TEST(ParseBitfieldVarBranch, EvenBitsOnly)
{
    ordered_json result;
    bitfield8_t val[1] = {};
    val[0].byte = 0x55; // bits 0,2,4,6
    nsmtool::helper::parseBitfieldVar(result, "key", val, 1);
    EXPECT_EQ(result["key"].size(), 4u);
    EXPECT_THAT(result["key"], ElementsAre(0, 2, 4, 6));
}

// =============================================================================
// Logger branch coverage tests
// =============================================================================

TEST(LoggerBranch, VerboseTrue)
{
    // Capture stdout
    testing::internal::CaptureStdout();
    nsmtool::helper::Logger(true, "Test: ", 42);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("Test: 42"), std::string::npos);
}

TEST(LoggerBranch, VerboseFalse)
{
    testing::internal::CaptureStdout();
    nsmtool::helper::Logger(false, "Test: ", 42);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.empty());
}

TEST(LoggerBranch, VerboseWithString)
{
    testing::internal::CaptureStdout();
    nsmtool::helper::Logger(true, "Message: ", std::string("hello"));
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("Message: hello"), std::string::npos);
}

// =============================================================================
// bytesToHexString branch coverage tests
// =============================================================================

TEST(BytesToHexStringBranch, EmptyInput)
{
    auto result = nsmtool::helper::bytesToHexString(nullptr, 0);
    EXPECT_TRUE(result.empty());
}

TEST(BytesToHexStringBranch, SingleByte)
{
    uint8_t data[] = {0xAB};
    auto result = nsmtool::helper::bytesToHexString(data, 1);
    EXPECT_EQ(result, "ab");
}

TEST(BytesToHexStringBranch, MultipleBytes)
{
    uint8_t data[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    auto result = nsmtool::helper::bytesToHexString(data, 8);
    EXPECT_EQ(result, "0123456789abcdef");
}

TEST(BytesToHexStringBranch, AllZeros)
{
    uint8_t data[] = {0x00, 0x00, 0x00};
    auto result = nsmtool::helper::bytesToHexString(data, 3);
    EXPECT_EQ(result, "000000");
}

TEST(BytesToHexStringBranch, AllFF)
{
    uint8_t data[] = {0xFF, 0xFF};
    auto result = nsmtool::helper::bytesToHexString(data, 2);
    EXPECT_EQ(result, "ffff");
}

// =============================================================================
// DisplayInJson branch coverage tests
// =============================================================================

TEST(DisplayInJsonBranch, SimpleObject)
{
    ordered_json data;
    data["key"] = "value";
    data["num"] = 42;
    testing::internal::CaptureStdout();
    nsmtool::helper::DisplayInJson(data);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("\"key\": \"value\""), std::string::npos);
    EXPECT_NE(output.find("\"num\": 42"), std::string::npos);
}

TEST(DisplayInJsonBranch, EmptyObject)
{
    ordered_json data = ordered_json::object();
    testing::internal::CaptureStdout();
    nsmtool::helper::DisplayInJson(data);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("{}"), std::string::npos);
}

TEST(DisplayInJsonBranch, ArrayData)
{
    ordered_json data = ordered_json::array({1, 2, 3});
    testing::internal::CaptureStdout();
    nsmtool::helper::DisplayInJson(data);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("1"), std::string::npos);
    EXPECT_NE(output.find("2"), std::string::npos);
    EXPECT_NE(output.find("3"), std::string::npos);
}

// =============================================================================
// CommandInterface branch coverage tests
// =============================================================================

// Concrete subclass for testing
class TestCommandInterface : public nsmtool::helper::CommandInterface
{
  public:
    explicit TestCommandInterface(CLI::App* app) :
        CommandInterface("test", "testCmd", app)
    {}

    int createRequestMsgRc = NSM_SW_SUCCESS;
    std::vector<uint8_t> createRequestMsgData;
    bool parseResponseCalled = false;

    std::pair<int, std::vector<uint8_t>> createRequestMsg() override
    {
        return {createRequestMsgRc, createRequestMsgData};
    }

    void parseResponseMsg(struct nsm_msg* /*responsePtr*/,
                          size_t /*payloadLength*/) override
    {
        parseResponseCalled = true;
    }

    // Expose protected members for testing
    void testEnqueueRequest(const std::vector<uint8_t>& msg)
    {
        enqueueRequest(msg);
    }
    void testEnqueueRequests(const std::vector<std::vector<uint8_t>>& msgs)
    {
        enqueueRequests(msgs);
    }
    size_t testQueueSize() const
    {
        return requestQueue.size();
    }
};

TEST(CommandInterfaceBranch, CreateRequestMsgsDefaultCallsSingle)
{
    CLI::App app;
    TestCommandInterface cmd(&app);

    // Set up a valid request message
    cmd.createRequestMsgData.resize(sizeof(nsm_msg_hdr) +
                                    sizeof(nsm_common_req));
    auto msg = reinterpret_cast<nsm_msg*>(cmd.createRequestMsgData.data());
    [[maybe_unused]] auto rc = encode_ping_req(0, msg);

    auto [retRc, msgs] = cmd.createRequestMsgs();
    EXPECT_EQ(retRc, NSM_SW_SUCCESS);
    EXPECT_EQ(msgs.size(), 1u);
}

TEST(CommandInterfaceBranch, CreateRequestMsgsFailureReturnsEmpty)
{
    CLI::App app;
    TestCommandInterface cmd(&app);
    cmd.createRequestMsgRc = -1;

    auto [retRc, msgs] = cmd.createRequestMsgs();
    EXPECT_NE(retRc, NSM_SW_SUCCESS);
    EXPECT_TRUE(msgs.empty());
}

TEST(CommandInterfaceBranch, GetMCTPEID)
{
    CLI::App app;
    TestCommandInterface cmd(&app);
    EXPECT_EQ(cmd.getMCTPEID(), nsmtool::helper::NSM_ENTITY_ID);
}

TEST(CommandInterfaceBranch, IsVerboseDefault)
{
    CLI::App app;
    TestCommandInterface cmd(&app);
    EXPECT_FALSE(cmd.isVerbose());
}

TEST(CommandInterfaceBranch, EnqueueRequestEmpty)
{
    CLI::App app;
    TestCommandInterface cmd(&app);
    std::vector<uint8_t> emptyMsg;
    cmd.testEnqueueRequest(emptyMsg);
    EXPECT_EQ(cmd.testQueueSize(), 0u);
}

TEST(CommandInterfaceBranch, EnqueueRequestNonEmpty)
{
    CLI::App app;
    TestCommandInterface cmd(&app);
    std::vector<uint8_t> msg = {0x01, 0x02, 0x03};
    cmd.testEnqueueRequest(msg);
    EXPECT_EQ(cmd.testQueueSize(), 1u);
}

TEST(CommandInterfaceBranch, EnqueueRequestsMultiple)
{
    CLI::App app;
    TestCommandInterface cmd(&app);
    std::vector<std::vector<uint8_t>> msgs = {
        {0x01, 0x02}, {0x03, 0x04}, {0x05, 0x06}};
    cmd.testEnqueueRequests(msgs);
    EXPECT_EQ(cmd.testQueueSize(), 3u);
}

TEST(CommandInterfaceBranch, EnqueueRequestsWithEmpty)
{
    CLI::App app;
    TestCommandInterface cmd(&app);
    std::vector<std::vector<uint8_t>> msgs = {{0x01, 0x02}, {}, {0x05, 0x06}};
    cmd.testEnqueueRequests(msgs);
    // Empty message should be skipped
    EXPECT_EQ(cmd.testQueueSize(), 2u);
}

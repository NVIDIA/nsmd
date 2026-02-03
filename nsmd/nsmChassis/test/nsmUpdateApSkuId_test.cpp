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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "nsmUpdateApSkuId.hpp"

namespace nsm
{
bool validateApSkuIdFormat(const std::string& skuId, std::string& errorMsg);
} // namespace nsm

using namespace nsm;

struct NsmUpdateApSkuIdTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;

    NsmUpdateApSkuIdTest() : SensorManagerTest(devices) {}
};

TEST_F(NsmUpdateApSkuIdTest, goodTestFormatApSkuId)
{
    uint32_t skuId = 0x12345678;
    std::string formatted = formatApSkuId(skuId);
    EXPECT_EQ(formatted, "0x12345678");

    skuId = 0x00000000;
    formatted = formatApSkuId(skuId);
    EXPECT_EQ(formatted, "0x00000000");

    skuId = 0xFFFFFFFF;
    formatted = formatApSkuId(skuId);
    EXPECT_EQ(formatted, "0xFFFFFFFF");

    skuId = 0xABCDEF12;
    formatted = formatApSkuId(skuId);
    EXPECT_EQ(formatted, "0xABCDEF12");
}

TEST_F(NsmUpdateApSkuIdTest, goodTestFormatMultipleSkuIds)
{
    std::string sku1 = formatApSkuId(0x11111111);
    std::string sku2 = formatApSkuId(0x22222222);
    std::string sku3 = formatApSkuId(0x33333333);

    EXPECT_EQ(sku1, "0x11111111");
    EXPECT_EQ(sku2, "0x22222222");
    EXPECT_EQ(sku3, "0x33333333");
}

TEST_F(NsmUpdateApSkuIdTest, goodTestFormatLeadingZeros)
{
    uint32_t skuId = 0x00000001;
    std::string formatted = formatApSkuId(skuId);
    EXPECT_EQ(formatted, "0x00000001");

    skuId = 0x00001234;
    formatted = formatApSkuId(skuId);
    EXPECT_EQ(formatted, "0x00001234");
}

TEST_F(NsmUpdateApSkuIdTest, testValidateApSkuIdFormatValidInput)
{
    std::string errorMsg;

    EXPECT_TRUE(validateApSkuIdFormat("0x12345678", errorMsg));
    EXPECT_TRUE(validateApSkuIdFormat("0xABCDEF12", errorMsg));
    EXPECT_TRUE(validateApSkuIdFormat("0x00000000", errorMsg));
    EXPECT_TRUE(validateApSkuIdFormat("0xFFFFFFFF", errorMsg));
    EXPECT_TRUE(validateApSkuIdFormat("0x12abcdef", errorMsg));
}

TEST_F(NsmUpdateApSkuIdTest, testValidateApSkuIdFormatUppercasePrefix)
{
    std::string errorMsg;

    EXPECT_TRUE(validateApSkuIdFormat("0X12345678", errorMsg));
    EXPECT_TRUE(validateApSkuIdFormat("0XABCDEF12", errorMsg));
}

TEST_F(NsmUpdateApSkuIdTest, testValidateApSkuIdFormatInvalidLength)
{
    std::string errorMsg;

    EXPECT_FALSE(validateApSkuIdFormat("0x123", errorMsg));
    EXPECT_NE(errorMsg.find("length"), std::string::npos);

    EXPECT_FALSE(validateApSkuIdFormat("0x12345678901", errorMsg));
    EXPECT_NE(errorMsg.find("length"), std::string::npos);

    EXPECT_FALSE(validateApSkuIdFormat("", errorMsg));
    EXPECT_FALSE(validateApSkuIdFormat("0x", errorMsg));
}

TEST_F(NsmUpdateApSkuIdTest, testValidateApSkuIdFormatInvalidPrefix)
{
    std::string errorMsg;

    // These will fail on length check first (8 != 10)
    EXPECT_FALSE(validateApSkuIdFormat("12345678", errorMsg));
    EXPECT_NE(errorMsg.find("length"), std::string::npos);

    EXPECT_FALSE(validateApSkuIdFormat("x12345678", errorMsg));
    EXPECT_NE(errorMsg.find("length"), std::string::npos);

    // This has correct length (10) but wrong prefix
    EXPECT_FALSE(validateApSkuIdFormat("0y12345678", errorMsg));
    EXPECT_NE(errorMsg.find("prefix"), std::string::npos);
}

TEST_F(NsmUpdateApSkuIdTest, testValidateApSkuIdFormatInvalidHexCharacters)
{
    std::string errorMsg;

    EXPECT_FALSE(validateApSkuIdFormat("0x1234567G", errorMsg));
    EXPECT_NE(errorMsg.find("hexadecimal"), std::string::npos);

    EXPECT_FALSE(validateApSkuIdFormat("0x1234567!", errorMsg));
    EXPECT_NE(errorMsg.find("hexadecimal"), std::string::npos);

    EXPECT_FALSE(validateApSkuIdFormat("0x1234 678", errorMsg));
    EXPECT_NE(errorMsg.find("hexadecimal"), std::string::npos);

    EXPECT_FALSE(validateApSkuIdFormat("0x1234567Z", errorMsg));
    EXPECT_NE(errorMsg.find("hexadecimal"), std::string::npos);
}

TEST_F(NsmUpdateApSkuIdTest, testValidateApSkuIdFormatBoundaryValues)
{
    std::string errorMsg;

    EXPECT_TRUE(validateApSkuIdFormat("0x00000000", errorMsg));
    EXPECT_TRUE(validateApSkuIdFormat("0xFFFFFFFF", errorMsg));
    EXPECT_TRUE(validateApSkuIdFormat("0x80000000", errorMsg));
    EXPECT_TRUE(validateApSkuIdFormat("0x7FFFFFFF", errorMsg));
}

TEST_F(NsmUpdateApSkuIdTest, testValidateApSkuIdFormatMixedCase)
{
    std::string errorMsg;

    EXPECT_TRUE(validateApSkuIdFormat("0xAbCdEf12", errorMsg));
    EXPECT_TRUE(validateApSkuIdFormat("0XaBcDeF12", errorMsg));
    EXPECT_TRUE(validateApSkuIdFormat("0x1a2B3c4D", errorMsg));
}

TEST_F(NsmUpdateApSkuIdTest, testFormatAndValidateRoundtrip)
{
    std::string errorMsg;

    uint32_t originalSkuId = 0xABCD1234;
    std::string formatted = formatApSkuId(originalSkuId);

    EXPECT_TRUE(validateApSkuIdFormat(formatted, errorMsg));
}

TEST_F(NsmUpdateApSkuIdTest, testValidateApSkuIdFormatErrorMessages)
{
    std::string errorMsg;

    validateApSkuIdFormat("0x123", errorMsg);
    EXPECT_FALSE(errorMsg.empty());

    errorMsg.clear();
    validateApSkuIdFormat("12345678", errorMsg);
    EXPECT_FALSE(errorMsg.empty());

    errorMsg.clear();
    validateApSkuIdFormat("0x1234567G", errorMsg);
    EXPECT_FALSE(errorMsg.empty());
}

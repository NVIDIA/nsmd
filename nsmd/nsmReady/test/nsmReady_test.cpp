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

#include "common/types.hpp"
#include "utils.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

// Test utility function for device type and role combination
TEST(NsmReadyUtilsTest, CombineDeviceTypeAndRole)
{
    // Test combining device type and role
    uint8_t deviceType = NSM_DEV_ID_GPU;
    uint8_t deviceRole = 0;

    uint16_t combined = utils::combineDeviceTypeAndRole(deviceType, deviceRole);

    // Role is in upper byte, type in lower byte
    EXPECT_EQ(combined & 0xFF, deviceType);
    EXPECT_EQ((combined >> 8) & 0xFF, deviceRole);
}

TEST(NsmReadyUtilsTest, CombineDeviceTypeAndRole_Switch)
{
    uint8_t deviceType = NSM_DEV_ID_SWITCH;
    uint8_t deviceRole = 0;

    uint16_t combined = utils::combineDeviceTypeAndRole(deviceType, deviceRole);

    EXPECT_EQ(combined & 0xFF, deviceType);
    EXPECT_EQ((combined >> 8) & 0xFF, deviceRole);
}

TEST(NsmReadyUtilsTest, CombineDeviceTypeAndRole_WithRole)
{
    uint8_t deviceType = NSM_DEV_ID_MCTP_BRIDGE;
    uint8_t deviceRole = NSM_MCTP_BRIDGE_DEV_ROLE_SXM_SMA;

    uint16_t combined = utils::combineDeviceTypeAndRole(deviceType, deviceRole);

    EXPECT_EQ(combined & 0xFF, deviceType);
    EXPECT_EQ((combined >> 8) & 0xFF, deviceRole);
}

TEST(NsmReadyUtilsTest, CombineDeviceTypeAndRole_PCIeBridge_CX7)
{
    uint8_t deviceType = NSM_DEV_ID_PCIE_BRIDGE;
    uint8_t deviceRole = NSM_PCIE_BRIDGE_DEV_ROLE_CX7;

    uint16_t combined = utils::combineDeviceTypeAndRole(deviceType, deviceRole);

    EXPECT_EQ(combined & 0xFF, deviceType);
    EXPECT_EQ((combined >> 8) & 0xFF, deviceRole);
}

TEST(NsmReadyUtilsTest, CombineDeviceTypeAndRole_PCIeBridge_CX8)
{
    uint8_t deviceType = NSM_DEV_ID_PCIE_BRIDGE;
    uint8_t deviceRole = NSM_PCIE_BRIDGE_DEV_ROLE_CX8;

    uint16_t combined = utils::combineDeviceTypeAndRole(deviceType, deviceRole);

    EXPECT_EQ(combined & 0xFF, deviceType);
    EXPECT_EQ((combined >> 8) & 0xFF, deviceRole);
}

TEST(NsmReadyUtilsTest, CombineDeviceTypeAndRole_PCIeBridge_CX9)
{
    uint8_t deviceType = NSM_DEV_ID_PCIE_BRIDGE;
    uint8_t deviceRole = NSM_PCIE_BRIDGE_DEV_ROLE_CX9;

    uint16_t combined = utils::combineDeviceTypeAndRole(deviceType, deviceRole);

    EXPECT_EQ(combined & 0xFF, deviceType);
    EXPECT_EQ((combined >> 8) & 0xFF, deviceRole);
}

TEST(NsmReadyUtilsTest, CombineDeviceTypeAndRole_AllDeviceTypes)
{
    // Test all device types mentioned in nsmRemapInstanceNumber.cpp
    std::vector<uint8_t> deviceTypes = {
        NSM_DEV_ID_GPU,       NSM_DEV_ID_SWITCH, NSM_DEV_ID_PCIE_BRIDGE,
        NSM_DEV_ID_BASEBOARD, NSM_DEV_ID_EROT,   NSM_DEV_ID_MCTP_BRIDGE};

    for (auto deviceType : deviceTypes)
    {
        uint16_t combined = utils::combineDeviceTypeAndRole(deviceType, 0);
        EXPECT_EQ(combined & 0xFF, deviceType);
        EXPECT_EQ((combined >> 8) & 0xFF, 0);
    }
}

TEST(NsmReadyUtilsTest, CombineDeviceTypeAndRole_MaxValues)
{
    // Test with maximum values
    uint8_t deviceType = 0xFF;
    uint8_t deviceRole = 0xFF;

    uint16_t combined = utils::combineDeviceTypeAndRole(deviceType, deviceRole);

    EXPECT_EQ(combined & 0xFF, deviceType);
    EXPECT_EQ((combined >> 8) & 0xFF, deviceRole);
    EXPECT_EQ(combined, 0xFFFF);
}

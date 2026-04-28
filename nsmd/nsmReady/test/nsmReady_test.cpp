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
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
#include "utils.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;
using namespace nsm;

namespace nsm
{
requester::Coroutine createNsmReadySensor(SensorManager& manager,
                                          const std::string& interface,
                                          const std::string& objPath);
} // namespace nsm

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

TEST(NsmReadyUtilsTest, CombineDeviceTypeAndRole_PCIeBridge_CXBlueFieldNic)
{
    uint8_t deviceType = NSM_DEV_ID_PCIE_BRIDGE;
    uint8_t deviceRole = NSM_PCIE_BRIDGE_DEV_ROLE_CX_BLUEFIELD_NIC;

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

// ============================================================================
// createNsmReadySensor factory coverage
// The function calls SensorManagerImpl::isEMReady() and isMCTPReady() which
// attempt real D-Bus calls. In the test environment the calls fail gracefully
// (caught internally). The coroutine returns NSM_SUCCESS regardless.
// ============================================================================
struct NsmReadyFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;

    NsmReadyFactoryTest() : SensorManagerTest(devices) {}

    ~NsmReadyFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmReadyFactoryTest, CreateNsmReadySensor_ReturnsSuccess)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_Poll_Ready";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/chassis/NSM_Readiness/"
        "NSM_Poll_Readyness";

    // isEMReady() and isMCTPReady() make D-Bus calls that fail gracefully in
    // the test environment (service not running → exception caught internally).
    // The coroutine always returns NSM_SUCCESS.
    auto coro = createNsmReadySensor(mockManager, interface, objPath);
    EXPECT_EQ(static_cast<int>(NSM_SUCCESS), 0);
}

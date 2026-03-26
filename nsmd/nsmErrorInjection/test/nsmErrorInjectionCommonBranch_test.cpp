/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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

/*
 * Branch coverage for nsmd/nsmErrorInjection/nsmErrorInjectionCommon.hpp
 *
 * Covers:
 * - getErrorInjectionTypeAndSubtype: nullptr guard TRUE branch (L111)
 * - getErrorInjectionTypeAndSubtype: each switch case (all 9 types + Unknown)
 * - createErrorInjectionSensorsForType: unsupported type path (Unknown type)
 * - createNsmMCUErrorInjectionSensors: role-based branching
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "device-configuration.h"

#include "nsmErrorInjection/nsmErrorInjectionCommon.hpp"

using namespace nsm;

using Type = ErrorInjectionCapabilityIntf::Type;

// ============================================================================
// getErrorInjectionTypeAndSubtype – nullptr guard
// ============================================================================

TEST(GetErrorInjectionTypeAndSubtype, NullptrType_ReturnsFalse)
{
    uint16_t subtype = 99;
    bool result = getErrorInjectionTypeAndSubtype(Type::MemoryErrors, nullptr,
                                                  &subtype);
    EXPECT_FALSE(result);
}

TEST(GetErrorInjectionTypeAndSubtype, NullptrSubtype_ReturnsFalse)
{
    uint16_t eiType = 99;
    bool result = getErrorInjectionTypeAndSubtype(Type::MemoryErrors, &eiType,
                                                  nullptr);
    EXPECT_FALSE(result);
}

TEST(GetErrorInjectionTypeAndSubtype, BothNull_ReturnsFalse)
{
    bool result = getErrorInjectionTypeAndSubtype(Type::MemoryErrors, nullptr,
                                                  nullptr);
    EXPECT_FALSE(result);
}

// ============================================================================
// Switch cases
// ============================================================================

TEST(GetErrorInjectionTypeAndSubtype, MemoryErrors_SetsCorrectValues)
{
    uint16_t eiType = 0, subtype = 0;
    bool result = getErrorInjectionTypeAndSubtype(Type::MemoryErrors, &eiType,
                                                  &subtype);
    EXPECT_TRUE(result);
    EXPECT_EQ(eiType, static_cast<uint16_t>(EI_MEMORY_ERRORS));
    EXPECT_EQ(subtype, 0u);
}

TEST(GetErrorInjectionTypeAndSubtype, PCIeErrors_SetsCorrectValues)
{
    uint16_t eiType = 0, subtype = 0;
    bool result = getErrorInjectionTypeAndSubtype(Type::PCIeErrors, &eiType,
                                                  &subtype);
    EXPECT_TRUE(result);
    EXPECT_EQ(eiType, static_cast<uint16_t>(EI_PCI_ERRORS));
    EXPECT_EQ(subtype, 0u);
}

TEST(GetErrorInjectionTypeAndSubtype, NVLinkErrors_SetsCorrectValues)
{
    uint16_t eiType = 0, subtype = 0;
    bool result = getErrorInjectionTypeAndSubtype(Type::NVLinkErrors, &eiType,
                                                  &subtype);
    EXPECT_TRUE(result);
    EXPECT_EQ(eiType, static_cast<uint16_t>(EI_NVLINK_ERRORS));
    EXPECT_EQ(subtype, 0u);
}

TEST(GetErrorInjectionTypeAndSubtype, ThermalErrors_SetsCorrectValues)
{
    uint16_t eiType = 0, subtype = 0;
    bool result = getErrorInjectionTypeAndSubtype(Type::ThermalErrors, &eiType,
                                                  &subtype);
    EXPECT_TRUE(result);
    EXPECT_EQ(eiType, static_cast<uint16_t>(EI_THERMAL_ERRORS));
    EXPECT_EQ(subtype, 0u);
}

TEST(GetErrorInjectionTypeAndSubtype, FatalErrors_SetsCorrectValues)
{
    uint16_t eiType = 0, subtype = 0;
    bool result = getErrorInjectionTypeAndSubtype(Type::FatalErrors, &eiType,
                                                  &subtype);
    EXPECT_TRUE(result);
    EXPECT_EQ(eiType, static_cast<uint16_t>(EI_DEVICE_ERRORS));
    EXPECT_EQ(subtype, static_cast<uint16_t>(EI_DEVICE_ERRORS_SUBTYPE_FATAL));
}

TEST(GetErrorInjectionTypeAndSubtype, PortRecoveryErrors_SetsCorrectValues)
{
    uint16_t eiType = 0, subtype = 0;
    bool result = getErrorInjectionTypeAndSubtype(Type::PortRecoveryErrors,
                                                  &eiType, &subtype);
    EXPECT_TRUE(result);
    EXPECT_EQ(eiType, static_cast<uint16_t>(EI_DEVICE_ERRORS));
    EXPECT_EQ(subtype,
              static_cast<uint16_t>(EI_DEVICE_ERRORS_SUBTYPE_PORT_RECOVERY));
}

TEST(GetErrorInjectionTypeAndSubtype,
     USBBridgeEmulationErrors_SetsCorrectValues)
{
    uint16_t eiType = 0, subtype = 0;
    bool result = getErrorInjectionTypeAndSubtype(
        Type::USBBridgeEmulationErrors, &eiType, &subtype);
    EXPECT_TRUE(result);
    EXPECT_EQ(eiType, static_cast<uint16_t>(EI_DEVICE_ERRORS));
    EXPECT_EQ(subtype,
              static_cast<uint16_t>(EI_DEVICE_ERRORS_SUBTYPE_USB_EMULATION));
}

TEST(GetErrorInjectionTypeAndSubtype, LeakDetectionErrors_SetsCorrectValues)
{
    uint16_t eiType = 0, subtype = 0;
    bool result = getErrorInjectionTypeAndSubtype(Type::LeakDetectionErrors,
                                                  &eiType, &subtype);
    EXPECT_TRUE(result);
    EXPECT_EQ(eiType, static_cast<uint16_t>(EI_DEVICE_ERRORS));
    EXPECT_EQ(subtype,
              static_cast<uint16_t>(EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT));
}

TEST(GetErrorInjectionTypeAndSubtype, GPIOSpoofingErrors_SetsCorrectValues)
{
    uint16_t eiType = 0, subtype = 0;
    bool result = getErrorInjectionTypeAndSubtype(Type::GPIOSpoofingErrors,
                                                  &eiType, &subtype);
    EXPECT_TRUE(result);
    EXPECT_EQ(eiType, static_cast<uint16_t>(EI_GPIO_SPOOFING));
    EXPECT_EQ(subtype, 0u);
}

TEST(GetErrorInjectionTypeAndSubtype, UnknownType_ReturnsFalse)
{
    uint16_t eiType = 99, subtype = 99;
    bool result = getErrorInjectionTypeAndSubtype(Type::Unknown, &eiType,
                                                  &subtype);
    EXPECT_FALSE(result);
}

// ============================================================================
// createErrorInjectionSensorsForType: Unknown type → early return via
// getErrorInjectionTypeAndSubtype returning false
// ============================================================================

struct NsmErrorInjectionCommonTest :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> device;

    NsmErrorInjectionCommonTest() : SensorManagerTest(devices)
    {
        device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(
                "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0"));
    }

    ~NsmErrorInjectionCommonTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmErrorInjectionCommonTest,
       CreateErrorInjectionSensorsForType_Unknown_EarlyReturn)
{
    // Unknown type → getErrorInjectionTypeAndSubtype returns false →
    // early return path; no sensors added, no crash
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createErrorInjectionSensorsForType(
        mockManager, device, "/xyz/test/ei_unk0", Type::Unknown));
    // No sensors should have been added
    EXPECT_EQ(device->deviceSensors.size(), before);
}

// ============================================================================
// createNsmMCUErrorInjectionSensors: role-dependent branches
// Use unique objPaths to avoid D-Bus registration conflicts between tests.
// ============================================================================

TEST_F(NsmErrorInjectionCommonTest,
       CreateNsmMCUErrorInjectionSensors_RoleHpmSma_AllBranchesCovered)
{
    // device role = NSM_MCTP_BRIDGE_DEV_ROLE_HPM_SMA (=3)
    // → both if-branches TRUE (LeakDetection + USBBridgeEmulation added)
    auto hpmDevice = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(
            "STATIC:768:10:NSM_DEVICE_INSTANCE_NUMBER:10"));
    ASSERT_NE(hpmDevice, nullptr);
    // NSM_MCTP_BRIDGE_DEV_ROLE_HPM_SMA = 3, deviceType=0 →
    // combined=(3<<8)|0=768
    // deviceRole is set from UUID parsing; verify/override it:
    hpmDevice->deviceRole = NSM_MCTP_BRIDGE_DEV_ROLE_HPM_SMA;

    EXPECT_NO_THROW(createNsmMCUErrorInjectionSensors(mockManager, hpmDevice,
                                                      "/xyz/test/mcu_hpm10"));
    cleanupDeviceSensors(devices);
}

TEST_F(NsmErrorInjectionCommonTest,
       CreateNsmMCUErrorInjectionSensors_RoleCxSma_LeakDetectionOnly)
{
    // device role = NSM_MCTP_BRIDGE_DEV_ROLE_CX_SMA (=2)
    // → first if TRUE (LeakDetection added), second if FALSE (no USB)
    auto cxDevice = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(
            "STATIC:512:11:NSM_DEVICE_INSTANCE_NUMBER:11"));
    ASSERT_NE(cxDevice, nullptr);
    cxDevice->deviceRole = NSM_MCTP_BRIDGE_DEV_ROLE_CX_SMA;

    EXPECT_NO_THROW(createNsmMCUErrorInjectionSensors(mockManager, cxDevice,
                                                      "/xyz/test/mcu_cx11"));
    cleanupDeviceSensors(devices);
}

TEST_F(NsmErrorInjectionCommonTest,
       CreateNsmMCUErrorInjectionSensors_OtherRole_NoLeakOrUsb)
{
    // device role = 0 (neither HPM_SMA nor CX_SMA)
    // → both if-branches FALSE (no LeakDetection, no USB)
    auto otherDevice = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(
            "STATIC:0:12:NSM_DEVICE_INSTANCE_NUMBER:12"));
    ASSERT_NE(otherDevice, nullptr);
    otherDevice->deviceRole = 0;

    EXPECT_NO_THROW(createNsmMCUErrorInjectionSensors(mockManager, otherDevice,
                                                      "/xyz/test/mcu_other12"));
    cleanupDeviceSensors(devices);
}

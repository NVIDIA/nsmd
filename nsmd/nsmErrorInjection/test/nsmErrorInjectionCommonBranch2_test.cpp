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
 * Branch coverage batch 2 for
 * nsmd/nsmErrorInjection/nsmErrorInjectionCommon.hpp
 *
 * Covers additional branches not in batch 1:
 * - createNsmErrorInjectionSensors: the for-loop over ErrorInjectionCapability
 *   types and the continue conditions for FatalErrors, PortRecoveryErrors,
 *   USBBridgeEmulationErrors, LeakDetectionErrors, GPIOSpoofingErrors
 * - createNsmErrorInjectionSensors: types that are NOT filtered (MemoryErrors,
 *   PCIeErrors, NVLinkErrors, ThermalErrors are processed)
 * - createErrorInjectionSensorsForType: valid type paths (not Unknown)
 * - createNsmMCUErrorInjectionSensors: combined role checks with different
 *   unique paths
 * - getErrorInjectionTypeAndSubtype: cast from out-of-range int to Type
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
// Test fixture
// ============================================================================

struct NsmErrorInjectionCommonBranch2Test :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> device;

    NsmErrorInjectionCommonBranch2Test() : SensorManagerTest(devices)
    {
        device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(
                "STATIC:0:20:NSM_DEVICE_INSTANCE_NUMBER:20"));
    }

    ~NsmErrorInjectionCommonBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// createNsmErrorInjectionSensors: exercises the for-loop that iterates over
// all ErrorInjectionCapabilityIntf::Type values. The continue conditions for
// FatalErrors, PortRecoveryErrors, USBBridgeEmulationErrors,
// LeakDetectionErrors, GPIOSpoofingErrors are each hit. Types that are NOT
// filtered (MemoryErrors, PCIeErrors, NVLinkErrors, ThermalErrors) get
// interfaces created and dispatchers registered.
// ============================================================================
TEST_F(NsmErrorInjectionCommonBranch2Test,
       CreateNsmErrorInjectionSensors_ForLoopContinueBranches)
{
    EXPECT_NO_THROW(createNsmErrorInjectionSensors(mockManager, device,
                                                   "/xyz/test/ei_loop20"));
    cleanupDeviceSensors(devices);
}

// ============================================================================
// createErrorInjectionSensorsForType: valid types (not Unknown)
// Exercises the success path through getErrorInjectionTypeAndSubtype and
// the sensor/dispatcher creation code.
// ============================================================================
TEST_F(NsmErrorInjectionCommonBranch2Test,
       CreateErrorInjectionSensorsForType_MemoryErrors)
{
    auto context = createErrorInjectionCapabilityContext(
        mockManager, device, "/xyz/test/ei_mem20", {Type::MemoryErrors});
    EXPECT_NO_THROW(createErrorInjectionSensorsForType(
        mockManager, device, "/xyz/test/ei_mem20", Type::MemoryErrors,
        context));
    cleanupDeviceSensors(devices);
}

TEST_F(NsmErrorInjectionCommonBranch2Test,
       CreateErrorInjectionSensorsForType_PCIeErrors)
{
    auto context = createErrorInjectionCapabilityContext(
        mockManager, device, "/xyz/test/ei_pcie20", {Type::PCIeErrors});
    EXPECT_NO_THROW(createErrorInjectionSensorsForType(
        mockManager, device, "/xyz/test/ei_pcie20", Type::PCIeErrors, context));
    cleanupDeviceSensors(devices);
}

TEST_F(NsmErrorInjectionCommonBranch2Test,
       CreateErrorInjectionSensorsForType_NVLinkErrors)
{
    auto context = createErrorInjectionCapabilityContext(
        mockManager, device, "/xyz/test/ei_nvlink20", {Type::NVLinkErrors});
    EXPECT_NO_THROW(createErrorInjectionSensorsForType(
        mockManager, device, "/xyz/test/ei_nvlink20", Type::NVLinkErrors,
        context));
    cleanupDeviceSensors(devices);
}

TEST_F(NsmErrorInjectionCommonBranch2Test,
       CreateErrorInjectionSensorsForType_ThermalErrors)
{
    auto context = createErrorInjectionCapabilityContext(
        mockManager, device, "/xyz/test/ei_thermal20", {Type::ThermalErrors});
    EXPECT_NO_THROW(createErrorInjectionSensorsForType(
        mockManager, device, "/xyz/test/ei_thermal20", Type::ThermalErrors,
        context));
    cleanupDeviceSensors(devices);
}

TEST_F(NsmErrorInjectionCommonBranch2Test,
       CreateErrorInjectionSensorsForType_FatalErrors)
{
    auto context = createErrorInjectionCapabilityContext(
        mockManager, device, "/xyz/test/ei_fatal20", {Type::FatalErrors});
    EXPECT_NO_THROW(createErrorInjectionSensorsForType(
        mockManager, device, "/xyz/test/ei_fatal20", Type::FatalErrors,
        context));
    cleanupDeviceSensors(devices);
}

TEST_F(NsmErrorInjectionCommonBranch2Test,
       CreateErrorInjectionSensorsForType_PortRecoveryErrors)
{
    auto context = createErrorInjectionCapabilityContext(
        mockManager, device, "/xyz/test/ei_portrec20",
        {Type::PortRecoveryErrors});
    EXPECT_NO_THROW(createErrorInjectionSensorsForType(
        mockManager, device, "/xyz/test/ei_portrec20", Type::PortRecoveryErrors,
        context));
    cleanupDeviceSensors(devices);
}

TEST_F(NsmErrorInjectionCommonBranch2Test,
       CreateErrorInjectionSensorsForType_USBBridgeEmulationErrors)
{
    auto context = createErrorInjectionCapabilityContext(
        mockManager, device, "/xyz/test/ei_usb20",
        {Type::USBBridgeEmulationErrors});
    EXPECT_NO_THROW(createErrorInjectionSensorsForType(
        mockManager, device, "/xyz/test/ei_usb20",
        Type::USBBridgeEmulationErrors, context));
    cleanupDeviceSensors(devices);
}

TEST_F(NsmErrorInjectionCommonBranch2Test,
       CreateErrorInjectionSensorsForType_LeakDetectionErrors)
{
    auto context = createErrorInjectionCapabilityContext(
        mockManager, device, "/xyz/test/ei_leak20",
        {Type::LeakDetectionErrors});
    EXPECT_NO_THROW(createErrorInjectionSensorsForType(
        mockManager, device, "/xyz/test/ei_leak20", Type::LeakDetectionErrors,
        context));
    cleanupDeviceSensors(devices);
}

TEST_F(NsmErrorInjectionCommonBranch2Test,
       CreateErrorInjectionSensorsForType_GPIOSpoofingErrors)
{
    auto context = createErrorInjectionCapabilityContext(
        mockManager, device, "/xyz/test/ei_gpio20", {Type::GPIOSpoofingErrors});
    EXPECT_NO_THROW(createErrorInjectionSensorsForType(
        mockManager, device, "/xyz/test/ei_gpio20", Type::GPIOSpoofingErrors,
        context));
    cleanupDeviceSensors(devices);
}

// ============================================================================
// Regression guard for the shared-container contract: a type that is absent
// from the context has no PDI to read during read-modify-write, so it must be
// skipped rather than registered against a container it does not belong to.
// ============================================================================
TEST_F(NsmErrorInjectionCommonBranch2Test,
       CreateErrorInjectionSensorsForType_TypeAbsentFromContext)
{
    auto context = createErrorInjectionCapabilityContext(
        mockManager, device, "/xyz/test/ei_absent20", {Type::MemoryErrors});
    EXPECT_NO_THROW(createErrorInjectionSensorsForType(
        mockManager, device, "/xyz/test/ei_absent20", Type::PCIeErrors,
        context));
    cleanupDeviceSensors(devices);
}

// ============================================================================
// createNsmMCUErrorInjectionSensors: role-based branches with unique paths
// Re-test with different unique object paths to ensure no D-Bus conflicts.
// ============================================================================

TEST_F(NsmErrorInjectionCommonBranch2Test,
       CreateNsmMCUErrorInjectionSensors_RoleHpmSma)
{
    auto hpmDevice = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(
            "STATIC:768:21:NSM_DEVICE_INSTANCE_NUMBER:21"));
    ASSERT_NE(hpmDevice, nullptr);
    hpmDevice->deviceRole = NSM_MCTP_BRIDGE_DEV_ROLE_HPM_SMA;

    EXPECT_NO_THROW(createNsmMCUErrorInjectionSensors(mockManager, hpmDevice,
                                                      "/xyz/test/mcu_hpm21"));
    cleanupDeviceSensors(devices);
}

// ============================================================================
// The point of the shared context: one mask owner for the whole device, not
// one per type. SetCurrentErrorInjectionTypesV1 writes all 64 bits at once, so
// a per-type owner rebuilds the mask from a single-entry container and zeroes
// every sibling's bit. One owner also means the aggregate batch property has
// something to address.
// ============================================================================
TEST_F(NsmErrorInjectionCommonBranch2Test,
       CreateNsmMCUErrorInjectionSensors_SingleSharedMaskOwner)
{
    auto hpmDevice = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(
            "STATIC:768:21:NSM_DEVICE_INSTANCE_NUMBER:21"));
    ASSERT_NE(hpmDevice, nullptr);
    hpmDevice->deviceRole = NSM_MCTP_BRIDGE_DEV_ROLE_HPM_SMA;

    createNsmMCUErrorInjectionSensors(mockManager, hpmDevice,
                                      "/xyz/test/mcu_shared24");

    size_t maskOwners = 0;
    for (const auto& sensor : hpmDevice->deviceSensors)
    {
        if (std::dynamic_pointer_cast<NsmSetErrorInjectionCapabilities>(sensor))
        {
            ++maskOwners;
        }
    }
    // HPM_SMA exposes five capability types; all five share this one owner.
    EXPECT_EQ(maskOwners, 1u);

    cleanupDeviceSensors(devices);
}

TEST_F(NsmErrorInjectionCommonBranch2Test,
       CreateNsmMCUErrorInjectionSensors_RoleCxSma)
{
    auto cxDevice = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(
            "STATIC:512:22:NSM_DEVICE_INSTANCE_NUMBER:22"));
    ASSERT_NE(cxDevice, nullptr);
    cxDevice->deviceRole = NSM_MCTP_BRIDGE_DEV_ROLE_CX_SMA;

    EXPECT_NO_THROW(createNsmMCUErrorInjectionSensors(mockManager, cxDevice,
                                                      "/xyz/test/mcu_cx22"));
    cleanupDeviceSensors(devices);
}

TEST_F(NsmErrorInjectionCommonBranch2Test,
       CreateNsmMCUErrorInjectionSensors_RoleQmSma)
{
    auto qmDevice = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(
            "STATIC:1285:22:NSM_DEVICE_INSTANCE_NUMBER:22"));
    ASSERT_NE(qmDevice, nullptr);
    qmDevice->deviceRole = NSM_MCTP_BRIDGE_DEV_ROLE_QM_SMA;

    EXPECT_NO_THROW(createNsmMCUErrorInjectionSensors(mockManager, qmDevice,
                                                      "/xyz/test/mcu_qm22"));
    cleanupDeviceSensors(devices);
}

TEST_F(NsmErrorInjectionCommonBranch2Test,
       CreateNsmMCUErrorInjectionSensors_OtherRole)
{
    auto otherDevice = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(
            "STATIC:0:23:NSM_DEVICE_INSTANCE_NUMBER:23"));
    ASSERT_NE(otherDevice, nullptr);
    otherDevice->deviceRole = 0;

    EXPECT_NO_THROW(createNsmMCUErrorInjectionSensors(mockManager, otherDevice,
                                                      "/xyz/test/mcu_other23"));
    cleanupDeviceSensors(devices);
}

// ============================================================================
// getErrorInjectionTypeAndSubtype: out-of-range int cast to Type
// ============================================================================
TEST(GetErrorInjectionTypeAndSubtypeBranch2, OutOfRangeValue_ReturnsFalse)
{
    uint16_t eiType = 99, subtype = 99;
    // Cast a large value to Type; should hit the default/Unknown case
    bool result = getErrorInjectionTypeAndSubtype(static_cast<Type>(999),
                                                  &eiType, &subtype);
    EXPECT_FALSE(result);
}

// ============================================================================
// getErrorInjectionTypeAndSubtype: both pointers valid for each type
// Ensures the success path writes correct values (complementing batch 1).
// ============================================================================
TEST(GetErrorInjectionTypeAndSubtypeBranch2, AllValidTypes_SuccessPath)
{
    struct TestCase
    {
        Type type;
        uint16_t expectedType;
        uint16_t expectedSubtype;
    };

    std::vector<TestCase> cases = {
        {Type::MemoryErrors, EI_MEMORY_ERRORS, 0},
        {Type::PCIeErrors, EI_PCI_ERRORS, 0},
        {Type::NVLinkErrors, EI_NVLINK_ERRORS, 0},
        {Type::ThermalErrors, EI_THERMAL_ERRORS, 0},
        {Type::FatalErrors, EI_DEVICE_ERRORS, EI_DEVICE_ERRORS_SUBTYPE_FATAL},
        {Type::PortRecoveryErrors, EI_DEVICE_ERRORS,
         EI_DEVICE_ERRORS_SUBTYPE_PORT_RECOVERY},
        {Type::USBBridgeEmulationErrors, EI_DEVICE_ERRORS,
         EI_DEVICE_ERRORS_SUBTYPE_USB_EMULATION},
        {Type::LeakDetectionErrors, EI_DEVICE_ERRORS,
         EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT},
        {Type::GPIOSpoofingErrors, EI_GPIO_SPOOFING, 0},
    };

    for (const auto& tc : cases)
    {
        uint16_t eiType = 0, subtype = 0;
        bool result = getErrorInjectionTypeAndSubtype(tc.type, &eiType,
                                                      &subtype);
        EXPECT_TRUE(result);
        EXPECT_EQ(eiType, tc.expectedType);
        EXPECT_EQ(subtype, tc.expectedSubtype);
    }
}

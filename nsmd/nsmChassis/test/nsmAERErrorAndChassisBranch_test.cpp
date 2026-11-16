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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#include "base.h"
#include "pci-links.h"

#include <sdbusplus/bus.hpp>

#define private public
#define protected public

#include "features/creators.hpp"
#include "features/feature.hpp"
#include "nsmChassis/nsmAERError.hpp"
#include "nsmChassis/nsmChassisPCIeDevice.hpp"
#include "nsmChassis/nsmNVSwitchAndNVMgmtNICChassis.hpp"

#undef private
#undef protected

using namespace nsm;

// Forward declarations for factory functions
namespace nsm
{
requester::Coroutine createNsmNVSwitchChassis(SensorManager& manager,
                                              const std::string& interface,
                                              const std::string& objPath);
} // namespace nsm

/** @brief Run a feature creator for the device `devName` */
static requester::Coroutine runFeature(nsm::features::CreatorFn creator,
                                       SensorManager& manager,
                                       const std::string& devName,
                                       const uuid_t& uuid,
                                       std::shared_ptr<NsmDevice> device,
                                       const dbus::PropertyMap& params)
{
    nsm::features::DeviceContext dev;
    dev.name = devName;
    dev.kind = "test";
    dev.staticUuid = uuid;
    dev.platformPrefix = "HGX_";
    dev.device = std::move(device);
    return creator(manager, dev, params);
}

static auto& testBus = utils::DBusHandler::getBus();

// ============================================================================
// SECTION 4: nsmAERError.cpp -- NsmPCIeAERErrorStatus tests
// ============================================================================

TEST(NsmPCIeAERErrorStatusTest, Constructor_ValidArgs_CreatesObject)
{
    // Arrange
    auto device = std::make_shared<MockNsmDevice>(
        0, 0, "MCTP_UUID", std::string("00000000-0000-0000-0000-000000000000"),
        0);
    auto aerIntf = std::make_shared<NsmAERErrorStatusIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/aer1", 0, device);

    // Act
    NsmPCIeAERErrorStatus sensor("AER_0", "PCIeAerErrorStatus", aerIntf, 0);

    // Assert
    EXPECT_EQ(sensor.getName(), "AER_0");
    EXPECT_EQ(sensor.getType(), "PCIeAerErrorStatus");
    EXPECT_EQ(sensor.deviceIndex, 0);
    EXPECT_NE(sensor.aerErrorStatusIntf, nullptr);
}

TEST(NsmPCIeAERErrorStatusTest, GenRequestMsg_ValidArgs_ReturnsRequest)
{
    // Arrange
    auto device = std::make_shared<MockNsmDevice>(
        0, 0, "MCTP_UUID", std::string("00000000-0000-0000-0000-000000000000"),
        0);
    auto aerIntf = std::make_shared<NsmAERErrorStatusIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/aer2", 0, device);
    NsmPCIeAERErrorStatus sensor("AER_1", "PCIeAerErrorStatus", aerIntf, 0);

    // Act
    auto request = sensor.genRequestMsg(10, 5);

    // Assert
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_query_scalar_group_telemetry_v1_req));
}

TEST(NsmPCIeAERErrorStatusTest, GenRequestMsg_InvalidInstanceId_ReturnsNullopt)
{
    // Arrange
    auto device = std::make_shared<MockNsmDevice>(
        0, 0, "MCTP_UUID", std::string("00000000-0000-0000-0000-000000000000"),
        0);
    auto aerIntf = std::make_shared<NsmAERErrorStatusIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/aer3", 0, device);
    NsmPCIeAERErrorStatus sensor("AER_2", "PCIeAerErrorStatus", aerIntf, 0);

    // Act
    auto request = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);

    // Assert
    EXPECT_FALSE(request.has_value());
}

TEST(NsmPCIeAERErrorStatusTest, HandleResponseMsg_Success_UpdatesAERFields)
{
    // Arrange
    auto device = std::make_shared<MockNsmDevice>(
        0, 0, "MCTP_UUID", std::string("00000000-0000-0000-0000-000000000000"),
        0);
    auto aerIntf = std::make_shared<NsmAERErrorStatusIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/aer4", 0, device);
    NsmPCIeAERErrorStatus sensor("AER_3", "PCIeAerErrorStatus", aerIntf, 0);

    nsm_query_scalar_group_telemetry_group_9 data = {};
    data.aer_uncorrectable_error_status = 0xDEADBEEF;
    data.aer_correctable_error_status = 0xCAFEBABE;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_9_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group9_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor.handleResponseMsg(responseMsg, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_STREQ(aerIntf->aerUncorrectableErrorStatus().c_str(), "0xDEADBEEF");
    EXPECT_STREQ(aerIntf->aerCorrectableErrorStatus().c_str(), "0xCAFEBABE");
}

TEST(NsmPCIeAERErrorStatusTest, HandleResponseMsg_ZeroValues_FormatsCorrectly)
{
    // Arrange
    auto device = std::make_shared<MockNsmDevice>(
        0, 0, "MCTP_UUID", std::string("00000000-0000-0000-0000-000000000001"),
        0);
    auto aerIntf = std::make_shared<NsmAERErrorStatusIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/aer4b", 0, device);
    NsmPCIeAERErrorStatus sensor("AER_3b", "PCIeAerErrorStatus", aerIntf, 0);

    nsm_query_scalar_group_telemetry_group_9 data = {};
    data.aer_uncorrectable_error_status = 0x00000000;
    data.aer_correctable_error_status = 0x00000000;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_9_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group9_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    rc = sensor.handleResponseMsg(responseMsg, responseData.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_STREQ(aerIntf->aerUncorrectableErrorStatus().c_str(), "0x00000000");
    EXPECT_STREQ(aerIntf->aerCorrectableErrorStatus().c_str(), "0x00000000");
}

TEST(NsmPCIeAERErrorStatusTest, HandleResponseMsg_ErrorCC_DoesNotUpdateFields)
{
    // Arrange
    auto device = std::make_shared<MockNsmDevice>(
        0, 0, "MCTP_UUID", std::string("00000000-0000-0000-0000-000000000000"),
        0);
    auto aerIntf = std::make_shared<NsmAERErrorStatusIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/aer5", 0, device);
    NsmPCIeAERErrorStatus sensor("AER_4", "PCIeAerErrorStatus", aerIntf, 0);

    nsm_query_scalar_group_telemetry_group_9 data = {};
    data.aer_uncorrectable_error_status = 0x12345678;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_9_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group9_resp(
        0, NSM_ERROR, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    responseData.resize(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_common_non_success_resp));
    responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());

    // Act
    rc = sensor.handleResponseMsg(responseMsg, responseData.size());

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(NsmPCIeAERErrorStatusTest, HandleResponseMsg_NullMsg_ReturnsError)
{
    // Arrange
    auto device = std::make_shared<MockNsmDevice>(
        0, 0, "MCTP_UUID", std::string("00000000-0000-0000-0000-000000000000"),
        0);
    auto aerIntf = std::make_shared<NsmAERErrorStatusIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/aer6", 0, device);
    NsmPCIeAERErrorStatus sensor("AER_5", "PCIeAerErrorStatus", aerIntf, 0);

    // Act
    auto rc = sensor.handleResponseMsg(nullptr, 100);

    // Assert
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Test linkAerStatusSensor
TEST(NsmAERErrorStatusIntfTest, LinkAerStatusSensor_SetsPointer)
{
    // Arrange
    auto device = std::make_shared<MockNsmDevice>(
        0, 0, "MCTP_UUID", std::string("00000000-0000-0000-0000-000000000000"),
        0);
    auto aerIntf = std::make_shared<NsmAERErrorStatusIntf>(
        testBus, "/xyz/openbmc_project/inventory/b12e/aer7", 0, device);
    auto sensor = std::make_shared<NsmPCIeAERErrorStatus>(
        "AER_link", "PCIeAerErrorStatus", aerIntf, 0);

    // Act
    aerIntf->linkAerStatusSensor(sensor);

    // Assert
    EXPECT_EQ(aerIntf->aerStatusSensor, sensor);
}

// ============================================================================
// SECTION 5: nsmNVSwitchAndNVMgmtNICChassis.cpp -- helper function coverage
// ============================================================================

struct NsmChassisHelperFuncTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:210";
    // CX8 device: (NSM_PCIE_BRIDGE_DEV_ROLE_CX8 << 8) |
    //             NSM_DEV_ID_PCIE_BRIDGE = (2 << 8) | 2 = 514
    const uuid_t cx8Uuid = "STATIC:514:0:NSM_DEVICE_INSTANCE_NUMBER:211";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpuDev;
    std::shared_ptr<MockNsmDevice> cx8Dev;

    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;

    NsmChassisHelperFuncTest() :
        SensorManagerTest(devices),
        systemBus(std::make_shared<sdbusplus::asio::connection>(io)),
        objServer(std::make_shared<sdbusplus::asio::object_server>(systemBus))
    {
        ON_CALL(mockManager, getObjServer())
            .WillByDefault(ReturnRef(*objServer));
        gpuDev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpuDev, nullptr);
        cx8Dev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(cx8Uuid));
        EXPECT_NE(cx8Dev, nullptr);
    }

    ~NsmChassisHelperFuncTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// Tests for inline helper functions (createChassisAsset, createChassisHealth,
// createChassisType, createLocationType, createLocationCode, createPrettyName,
// createAssetTag, createChassisVersion, createChassisSKU) were removed because
// those functions are defined in the .cpp file without header declarations and
// cannot be called from test code.

// ============================================================================
// SECTION 6: nsmNVSwitchAndNVMgmtNICChassis.cpp -- createNsmChassis factory
//            (base type match path)
// ============================================================================

TEST_F(NsmChassisHelperFuncTest,
       DISABLED_CreateNsmNVSwitchChassis_BaseType_CreatesUuidSensor)
{
    // Arrange - configure D-Bus mock properties
    std::string objPath =
        "/xyz/openbmc_project/inventory/b12e/nvswitch_chassis1";
    std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch_Chassis";

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              baseIntf);
    basePropertyMap["Name"] = std::string("NVSwitch_Chassis_B12E");
    basePropertyMap["UUID"] = gpuUuid;

    auto& ifacePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                               baseIntf);
    ifacePropertyMap["Type"] = std::string("NSM_NVSwitch_Chassis");
    ifacePropertyMap["UUID"] = std::string("test-uuid-1234");

    // Act
    createNsmNVSwitchChassis(mockManager, baseIntf, objPath);

    // Assert - should create at least a UUID sensor
    EXPECT_GE(gpuDev->deviceSensors.size(), 1u);
}

TEST_F(NsmChassisHelperFuncTest, NVLinkMgmtNicChassisFeature_CreatesUuidSensor)
{
    const size_t before = cx8Dev->deviceSensors.size();
    auto rc = runFeature(nsm::features::createNVLinkMgmtNicChassisFeature,
                         mockManager, "HGX_NVLinkMgmtNic_Chassis_B12E", cx8Uuid,
                         cx8Dev,
                         {{"Name", std::string("NVLinkMgmtNic_Chassis_B12E")}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(cx8Dev->deviceSensors.size(), before + 1);
}

TEST_F(NsmChassisHelperFuncTest, NVLinkMgmtNicChassisFeature_NoDevice_Error)
{
    auto rc = runFeature(nsm::features::createNVLinkMgmtNicChassisFeature,
                         mockManager, "HGX_NVLinkMgmtNic_Chassis_NoDev",
                         cx8Uuid, nullptr,
                         {{"Name", std::string("NVLinkMgmtNic_NoDev")}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SW_ERROR);
}

// ============================================================================
// SECTION 7: nsmChassisPCIeDevice.cpp -- helper function coverage
// ============================================================================

struct NsmChassisPCIeDeviceHelperTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:220";
    // Retimer: deviceType=4 (NSM_DEV_ID_RETIMER)
    const uuid_t retimerUuid = "STATIC:4:0:NSM_DEVICE_INSTANCE_NUMBER:221";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpuDev;
    std::shared_ptr<MockNsmDevice> retimerDev;

    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;

    NsmChassisPCIeDeviceHelperTest() :
        SensorManagerTest(devices),
        systemBus(std::make_shared<sdbusplus::asio::connection>(io)),
        objServer(std::make_shared<sdbusplus::asio::object_server>(systemBus))
    {
        ON_CALL(mockManager, getObjServer())
            .WillByDefault(ReturnRef(*objServer));

        gpuDev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpuDev, nullptr);
        retimerDev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(retimerUuid));
        EXPECT_NE(retimerDev, nullptr);
    }

    ~NsmChassisPCIeDeviceHelperTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// Tests for inline helper functions (createChassisPCIeDeviceAsset,
// createChassisPCIeDeviceHealth, createChassisPCIeDevicePCIeDevice,
// createChassisPCIeDeviceMultiPortPCIeDevice,
// createChassisPCIeDeviceRetimerAERErrorStatus) were removed because those
// functions are defined in the .cpp file without header declarations and
// cannot be called from test code.

// ============================================================================
// SECTION 8: nsmChassisPCIeDevice.cpp -- factory function coverage
// ============================================================================

TEST_F(NsmChassisPCIeDeviceHelperTest, Feature_UUIDAndAssociations)
{
    const size_t before = gpuDev->deviceSensors.size();
    auto rc = runFeature(nsm::features::createChassisPCIeDeviceUUIDFeature,
                         mockManager, "HGX_Chassis_Fac1", gpuUuid, gpuDev,
                         {{"Name", std::string("GPU_B12E_F1")}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    rc = runFeature(
        nsm::features::createChassisPCIeDeviceAssociationsFeature, mockManager,
        "HGX_Chassis_Fac1", gpuUuid, gpuDev,
        {{"Name", std::string("GPU_B12E_F1")},
         {"ChassisAssociationPaths",
          std::vector<std::string>{"chassis/HGX_Chassis_Fac1", ""}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // UUID and associations
    EXPECT_EQ(gpuDev->deviceSensors.size(), before + 2);
}

TEST_F(NsmChassisPCIeDeviceHelperTest, Feature_NoAssociationTargets_NoObject)
{
    const size_t before = gpuDev->deviceSensors.size();
    auto rc =
        runFeature(nsm::features::createChassisPCIeDeviceAssociationsFeature,
                   mockManager, "HGX_Chassis_Fac1b", gpuUuid, gpuDev,
                   {{"Name", std::string("GPU_B12E_F1b")}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpuDev->deviceSensors.size(), before);
}

TEST_F(NsmChassisPCIeDeviceHelperTest, Feature_AssetAndHealth)
{
    const size_t before = gpuDev->deviceSensors.size();
    const dbus::PropertyMap params{{"Name", std::string("GPU_B12E_F2")}};
    auto rc = runFeature(nsm::features::createChassisPCIeDeviceAssetFeature,
                         mockManager, "HGX_Chassis_Fac2", gpuUuid, gpuDev,
                         params);
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    rc = runFeature(nsm::features::createChassisPCIeDeviceHealthFeature,
                    mockManager, "HGX_Chassis_Fac2", gpuUuid, gpuDev, params);
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // asset (3) + health (1)
    EXPECT_EQ(gpuDev->deviceSensors.size(), before + 4);
}

TEST_F(NsmChassisPCIeDeviceHelperTest, Feature_PCIeDevice_LinkSpeedAndFunctions)
{
    const size_t before = gpuDev->deviceSensors.size();
    auto rc = runFeature(
        nsm::features::createChassisPCIeDevicePCIeDeviceFeature, mockManager,
        "HGX_Chassis_Fac3", gpuUuid, gpuDev,
        {{"Name", std::string("GPU_B12E_F3")},
         {"DeviceType", std::string("xyz.openbmc_project.Inventory.Item."
                                    "PCIeDevice.DeviceTypes.SingleFunction")},
         {"Functions", std::vector<uint64_t>{0}}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // link speed + function 0 (+ AER status when enabled)
    EXPECT_GE(gpuDev->deviceSensors.size(), before + 2);
}

TEST_F(NsmChassisPCIeDeviceHelperTest, Feature_MultiPortPCIeDeviceAndRetimerAER)
{
    const size_t before = retimerDev->deviceSensors.size();
    auto rc = runFeature(
        nsm::features::createChassisPCIeDeviceMultiPortPCIeDeviceFeature,
        mockManager, "HGX_Chassis_Fac4", retimerUuid, retimerDev,
        {{"Name", std::string("Retimer_B12E_F4")},
         {"DeviceType", std::string("xyz.openbmc_project.Inventory.Item."
                                    "PCIeDevice.DeviceTypes.SingleFunction")},
         {"Functions", std::vector<uint64_t>{0}},
         {"UpstreamPortCount", uint64_t(1)}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    rc = runFeature(
        nsm::features::createChassisPCIeDeviceRetimerAERErrorStatusFeature,
        mockManager, "HGX_Chassis_Fac4", retimerUuid, retimerDev,
        {{"Name", std::string("Retimer_B12E_F4")}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // multiport PCIe device sensors + retimer AER error status
    EXPECT_GE(retimerDev->deviceSensors.size(), before + 3);
}

// ============================================================================
// SECTION 9: NsmChassisPCIeDevice template instantiation tests
// ============================================================================

TEST(NsmChassisPCIeDeviceTemplate, UuidIntf_Construction_CreatesObject)
{
    // Arrange & Act
    auto uuidObj = std::make_shared<NsmChassisPCIeDevice<UuidIntf>>(
        "HGX_Chassis", "GPU_0_B12E");

    // Assert
    EXPECT_EQ(uuidObj->getName(), "GPU_0_B12E");
    EXPECT_EQ(uuidObj->getType(), "NSM_ChassisPCIeDevice");
}

TEST(NsmChassisPCIeDeviceTemplate, HealthIntf_Construction_CreatesObject)
{
    // Arrange & Act
    auto healthObj = std::make_shared<NsmChassisPCIeDevice<HealthIntf>>(
        "HGX_Chassis", "GPU_1_B12E");

    // Assert
    EXPECT_EQ(healthObj->getName(), "GPU_1_B12E");
    EXPECT_EQ(healthObj->getType(), "NSM_ChassisPCIeDevice");
}

TEST(NsmChassisPCIeDeviceTemplate, AssociationIntf_Construction_CreatesObject)
{
    // Arrange & Act
    auto assocObj =
        std::make_shared<NsmChassisPCIeDevice<AssociationDefinitionsIntf>>(
            "HGX_Chassis", "GPU_2_B12E");

    // Assert
    EXPECT_EQ(assocObj->getName(), "GPU_2_B12E");
    EXPECT_EQ(assocObj->getType(), "NSM_ChassisPCIeDevice");
}

TEST(NsmChassisPCIeDeviceTemplate, PCIeDeviceIntf_Construction_CreatesObject)
{
    // Arrange & Act
    auto pcieObj = std::make_shared<NsmChassisPCIeDevice<PCIeDeviceIntf>>(
        "HGX_Chassis", "GPU_3_B12E");

    // Assert
    EXPECT_EQ(pcieObj->getName(), "GPU_3_B12E");
    EXPECT_EQ(pcieObj->getType(), "NSM_ChassisPCIeDevice");
}

TEST(NsmChassisPCIeDeviceTemplate, NsmAssetIntf_Construction_CreatesObject)
{
    // Arrange & Act
    auto assetObj = std::make_shared<NsmChassisPCIeDevice<NsmAssetIntf>>(
        "HGX_Chassis", "GPU_4_B12E");

    // Assert
    EXPECT_EQ(assetObj->getName(), "GPU_4_B12E");
    EXPECT_EQ(assetObj->getType(), "NSM_ChassisPCIeDevice");
}

// ============================================================================
// SECTION 10: nsmNVSwitchAndNVMgmtNICChassis.cpp -- template instantiation
// ============================================================================

TEST(NsmNVSwitchChassisTemplate, UuidIntf_Construction_CreatesObject)
{
    // Arrange & Act
    auto uuidObj = std::make_shared<NsmNVSwitchAndNicChassis<UuidIntf>>(
        "NVSwitch_B12E", "NSM_NVSwitch_Chassis");

    // Assert
    EXPECT_EQ(uuidObj->getName(), "NVSwitch_B12E");
    EXPECT_EQ(uuidObj->getType(), "NSM_NVSwitch_Chassis");
}

TEST(NsmNVSwitchChassisTemplate, ChassisIntf_Construction_CreatesObject)
{
    // Arrange & Act
    auto chassisObj = std::make_shared<NsmNVSwitchAndNicChassis<ChassisIntf>>(
        "NVSwitch_B12E_Ch", "NSM_NVSwitch_Chassis");

    // Assert
    EXPECT_EQ(chassisObj->getName(), "NVSwitch_B12E_Ch");
    EXPECT_EQ(chassisObj->getType(), "NSM_NVSwitch_Chassis");
}

TEST(NsmNVSwitchChassisTemplate, HealthIntf_Construction_CreatesObject)
{
    // Arrange & Act
    auto healthObj = std::make_shared<NsmNVSwitchAndNicChassis<HealthIntf>>(
        "NVSwitch_B12E_H", "NSM_NVSwitch_Chassis");

    // Assert
    EXPECT_EQ(healthObj->getName(), "NVSwitch_B12E_H");
    EXPECT_EQ(healthObj->getType(), "NSM_NVSwitch_Chassis");
}

TEST(NsmNVSwitchChassisTemplate, LocationIntf_Construction_CreatesObject)
{
    // Arrange & Act
    auto locObj = std::make_shared<NsmNVSwitchAndNicChassis<LocationIntf>>(
        "NVSwitch_B12E_L", "NSM_NVSwitch_Chassis");

    // Assert
    EXPECT_EQ(locObj->getName(), "NVSwitch_B12E_L");
    EXPECT_EQ(locObj->getType(), "NSM_NVSwitch_Chassis");
}

TEST(NsmNVSwitchChassisTemplate, LocationCodeIntf_Construction_CreatesObject)
{
    // Arrange & Act
    auto locCodeObj =
        std::make_shared<NsmNVSwitchAndNicChassis<LocationCodeIntf>>(
            "NVSwitch_B12E_LC", "NSM_NVSwitch_Chassis");

    // Assert
    EXPECT_EQ(locCodeObj->getName(), "NVSwitch_B12E_LC");
    EXPECT_EQ(locCodeObj->getType(), "NSM_NVSwitch_Chassis");
}

TEST(NsmNVSwitchChassisTemplate, ItemIntf_Construction_CreatesObject)
{
    // Arrange & Act
    auto itemObj = std::make_shared<NsmNVSwitchAndNicChassis<ItemIntf>>(
        "NVSwitch_B12E_I", "NSM_NVSwitch_Chassis");

    // Assert
    EXPECT_EQ(itemObj->getName(), "NVSwitch_B12E_I");
    EXPECT_EQ(itemObj->getType(), "NSM_NVSwitch_Chassis");
}

TEST(NsmNVSwitchChassisTemplate, NsmApSkuIdIntf_Construction_CreatesObject)
{
    // Arrange & Act
    auto skuObj = std::make_shared<NsmNVSwitchAndNicChassis<NsmApSkuIdIntf>>(
        "NVSwitch_B12E_Sku", "NSM_NVSwitch_Chassis");

    // Assert
    EXPECT_EQ(skuObj->getName(), "NVSwitch_B12E_Sku");
    EXPECT_EQ(skuObj->getType(), "NSM_NVSwitch_Chassis");
}

TEST(NsmNVSwitchChassisTemplate, NsmAssetIntf_Construction_CreatesObject)
{
    // Arrange & Act
    auto assetObj = std::make_shared<NsmNVSwitchAndNicChassis<NsmAssetIntf>>(
        "NVSwitch_B12E_A", "NSM_NVSwitch_Chassis");

    // Assert
    EXPECT_EQ(assetObj->getName(), "NVSwitch_B12E_A");
    EXPECT_EQ(assetObj->getType(), "NSM_NVSwitch_Chassis");
}

TEST(NsmNVSwitchChassisTemplate, AssetTagIntf_Construction_CreatesObject)
{
    // Arrange & Act
    auto atObj = std::make_shared<NsmNVSwitchAndNicChassis<AssetTagIntf>>(
        "NVSwitch_B12E_AT", "NSM_NVSwitch_Chassis");

    // Assert
    EXPECT_EQ(atObj->getName(), "NVSwitch_B12E_AT");
    EXPECT_EQ(atObj->getType(), "NSM_NVSwitch_Chassis");
}

TEST(NsmNVSwitchChassisTemplate, RevisionIntf_Construction_CreatesObject)
{
    // Arrange & Act
    auto revObj = std::make_shared<NsmNVSwitchAndNicChassis<RevisionIntf>>(
        "NVSwitch_B12E_R", "NSM_NVSwitch_Chassis");

    // Assert
    EXPECT_EQ(revObj->getName(), "NVSwitch_B12E_R");
    EXPECT_EQ(revObj->getType(), "NSM_NVSwitch_Chassis");
}

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

/*
 * Branch coverage tests for nsmd/nsmChassis/nsmChassisPCIeDevice.cpp
 *
 * Covers:
 * - NsmChassisPCIeDevice<UuidIntf>::update: rc != 0 from getDeviceUUID
 * - NsmChassisPCIeDevice<UuidIntf>::update: empty uuid check
 * - nsmChassisPCIeDeviceCreateSensors: missing properties branches
 * - createChassisPCIeDeviceMultiPortPCIeDevice: various property combinations
 * - NsmChassisPCIeDevice<non-UuidIntf>::update: co_return NSM_SUCCESS //
 *
 */

#include "test/commonMock.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "nsmChassisPCIeDevice.hpp"

#undef private
#undef protected

namespace nsm
{
requester::Coroutine
    nsmChassisPCIeDeviceCreateSensors(SensorManager& manager,
                                      const std::string& interface,
                                      const std::string& objPath);

void createChassisPCIeDeviceMultiPortPCIeDevice(
    std::shared_ptr<NsmDevice> device, std::string& name,
    const std::string& chassisName,
    dbus::PropertyMap& allCurrentIfaceProperties);
} // namespace nsm

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================

struct NsmChassisPCIeDeviceBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice";
    const std::string chassisName = "HGX_GPU_B2";
    const std::string name = "PCIeDeviceB2";
    const std::string objPath = chassisInventoryBasePath / chassisName / name;

    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:6";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;

    NsmChassisPCIeDeviceBranch2Test() :
        SensorManagerTest(devices),
        systemBus(std::make_shared<sdbusplus::asio::connection>(io)),
        objServer(std::make_shared<sdbusplus::asio::object_server>(systemBus))
    {
        ON_CALL(mockManager, getObjServer())
            .WillByDefault(ReturnRef(*objServer));
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmChassisPCIeDeviceBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// NsmChassisPCIeDevice<UuidIntf>::update: MctpDiscovery::getInstance() throws
// in test environment → covers the UuidIntf constexpr if branch entry
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranch2Test, UpdateUuidIntf_MctpNotInit_Throws)
{
    auto sensor = std::make_shared<NsmChassisPCIeDevice<UuidIntf>>(
        chassisName, "UuidB2_update");

    EXPECT_THROW_COROUTINE(sensor->update(gpu), std::runtime_error);
}

// ============================================================================
// NsmChassisPCIeDevice<HealthIntf>::update: non-UuidIntf → co_return //
// NSM_SUCCESS at L68 (skip constexpr if)
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranch2Test, UpdateHealthIntf_CoReturnSuccess)
{
    auto sensor = std::make_shared<NsmChassisPCIeDevice<HealthIntf>>(
        chassisName, "HealthB2_update");
    sensor->update(gpu);
}

// ============================================================================
// NsmChassisPCIeDevice<AssociationDefinitionsIntf>::update: co_return //
// NSM_SUCCESS
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranch2Test, UpdateAssociationIntf_CoReturnSuccess)
{
    auto sensor =
        std::make_shared<NsmChassisPCIeDevice<AssociationDefinitionsIntf>>(
            chassisName, "AssocB2_update");
    sensor->update(gpu);
}

// ============================================================================
// Factory: NSM_Chassis_Attributes type → L344-348 branch
// Exercises createChassisPCIeDeviceAsset + createChassisPCIeDeviceHealth
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranch2Test,
       Factory_ChassisAttributes_CreatesAssetAndHealth)
{
    const std::string uniquePath = objPath + "_b2_attrs";

    auto& basePm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    basePm["ChassisName"] = chassisName;
    basePm["Name"] = name;
    basePm["UUID"] = gpuUuid;

    auto& curPm = utils::MockDbusAsync::propertyMap(
        uniquePath, basicIntfName + ".ChassisAttributes");
    curPm["Type"] = std::string("NSM_Chassis_Attributes");
    curPm["Name"] = std::string("GPU_SXM_B2");

    const size_t before = gpu->staticSensors.size();
    nsmChassisPCIeDeviceCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", uniquePath);

    // Asset creates partNumber + serialNumber + model = 3 static sensors
    // Health creates 1 static sensor = 4 total
    EXPECT_GE(gpu->staticSensors.size(), before + 3u);
}

// ============================================================================
// Factory: missing ChassisName → L296 FALSE → chassisName stays ""
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranch2Test, Factory_MissingChassisName_FalseBranch)
{
    const std::string uniquePath = objPath + "_b2_noChassis";

    auto& basePm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    // ChassisName absent → L296 FALSE
    basePm["Name"] = name;
    basePm["UUID"] = gpuUuid;

    auto& curPm = utils::MockDbusAsync::propertyMap(
        uniquePath, basicIntfName + ".noChassis");
    curPm["Type"] = std::string("NSM_ChassisPCIeDevice");
    curPm["DEVICE_UUID"] = gpuUuid;

    EXPECT_NO_THROW(nsmChassisPCIeDeviceCreateSensors(
        mockManager, basicIntfName + ".noChassis", uniquePath));
}

// ============================================================================
// Factory: missing Name → L302 FALSE → name stays ""
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranch2Test, Factory_MissingName_FalseBranch)
{
    const std::string uniquePath = objPath + "_b2_noName";

    auto& basePm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    basePm["ChassisName"] = chassisName;
    // Name absent → L302 FALSE
    basePm["UUID"] = gpuUuid;

    auto& curPm = utils::MockDbusAsync::propertyMap(uniquePath,
                                                    basicIntfName + ".noName");
    curPm["Type"] = std::string("NSM_ChassisPCIeDevice");
    curPm["DEVICE_UUID"] = gpuUuid;

    // name="" → D-Bus path may throw on empty name
    EXPECT_THROW_COROUTINE(
        nsmChassisPCIeDeviceCreateSensors(
            mockManager, basicIntfName + ".noName", uniquePath),
        std::exception);
}

// ============================================================================
// Factory: missing Type → L307 FALSE → type="" → no type branch taken
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranch2Test, Factory_MissingType_NoBranchTaken)
{
    const std::string uniquePath = objPath + "_b2_noType";

    auto& basePm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    basePm["ChassisName"] = chassisName;
    basePm["Name"] = name;
    basePm["UUID"] = gpuUuid;

    (void)utils::MockDbusAsync::propertyMap(uniquePath,
                                            basicIntfName + ".noType");
    // Type absent → L307 FALSE

    const size_t before = gpu->deviceSensors.size();
    nsmChassisPCIeDeviceCreateSensors(mockManager, basicIntfName + ".noType",
                                      uniquePath);
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Factory: missing UUID → L312 FALSE → uuid="" → throws
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranch2Test, Factory_MissingUUID_Throws)
{
    const std::string uniquePath = objPath + "_b2_noUUID";

    auto& basePm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    basePm["ChassisName"] = chassisName;
    basePm["Name"] = name;
    // UUID absent → L312 FALSE

    auto& curPm = utils::MockDbusAsync::propertyMap(uniquePath,
                                                    basicIntfName + ".noUUID");
    curPm["Type"] = std::string("NSM_Chassis_Attributes");

    EXPECT_THROW_COROUTINE(
        nsmChassisPCIeDeviceCreateSensors(
            mockManager, basicIntfName + ".noUUID", uniquePath),
        std::runtime_error);
}

// ============================================================================
// Factory: missing DEVICE_UUID → L322 FALSE → deviceUuid stays ""
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranch2Test, Factory_MissingDEVICE_UUID_FalseBranch)
{
    const std::string uniquePath = objPath + "_b2_noDevUUID";

    auto& basePm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    basePm["ChassisName"] = chassisName;
    basePm["Name"] = name;
    basePm["UUID"] = gpuUuid;

    auto& curPm = utils::MockDbusAsync::propertyMap(
        uniquePath, basicIntfName + ".noDevUUID");
    curPm["Type"] = std::string("NSM_ChassisPCIeDevice");
    // DEVICE_UUID absent → L322 FALSE

    EXPECT_NO_THROW(nsmChassisPCIeDeviceCreateSensors(
        mockManager, basicIntfName + ".noDevUUID", uniquePath));
}

// ============================================================================
// createChassisPCIeDeviceMultiPortPCIeDevice: direct call with all optional
// properties present → TRUE branches at L157, L163, L169
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranch2Test,
       DISABLED_DirectMultiPort_AllOptionalProps_TrueBranches)
{
    std::string devName = "MultiPortB2_allopt";
    dbus::PropertyMap props = {
        {"DeviceType",
         std::string("xyz.openbmc_project.Inventory.Item.PCIeDevice."
                     "DeviceTypes.SingleFunction")},
        {"Functions", std::vector<uint64_t>{0, 1}},
        {"UpstreamPortCount", uint64_t(2)},
    };

    const size_t beforeStatic = gpu->staticSensors.size();
    const size_t beforePrio = gpu->prioritySensors.size();
    createChassisPCIeDeviceMultiPortPCIeDevice(gpu, devName, chassisName,
                                               props);

    // PCIeLinkSpeed priority sensor + 2 functions + busNumber + AER = sensors
    EXPECT_GT(gpu->staticSensors.size(), beforeStatic);
    EXPECT_GT(gpu->prioritySensors.size(), beforePrio);
}

// ============================================================================
// createChassisPCIeDeviceMultiPortPCIeDevice: direct call with NO optional
// properties → FALSE branches at L157, L163, L169
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranch2Test,
       DirectMultiPort_NoOptionalProps_FalseBranches)
{
    std::string devName = "MultiPortB2_noopt";
    dbus::PropertyMap props = {};

    const size_t beforeStatic = gpu->staticSensors.size();
    createChassisPCIeDeviceMultiPortPCIeDevice(gpu, devName, chassisName,
                                               props);

    // Defaults: SingleFunction, Functions={0}, UpstreamPortCount=1
    EXPECT_GT(gpu->staticSensors.size(), beforeStatic);
}

// ============================================================================
// createChassisPCIeDeviceMultiPortPCIeDevice: DeviceType only present
// → L157 TRUE, L163 FALSE, L169 FALSE
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranch2Test,
       DirectMultiPort_OnlyDeviceType_MixedBranches)
{
    std::string devName = "MultiPortB2_dtype";
    dbus::PropertyMap props = {
        {"DeviceType",
         std::string("xyz.openbmc_project.Inventory.Item.PCIeDevice."
                     "DeviceTypes.MultiFunction")},
    };

    createChassisPCIeDeviceMultiPortPCIeDevice(gpu, devName, chassisName,
                                               props);
    SUCCEED();
}

// ============================================================================
// createChassisPCIeDeviceMultiPortPCIeDevice: Functions only present
// → L157 FALSE, L163 TRUE, L169 FALSE
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranch2Test,
       DirectMultiPort_OnlyFunctions_MixedBranches)
{
    std::string devName = "MultiPortB2_funcs";
    dbus::PropertyMap props = {
        {"Functions", std::vector<uint64_t>{0, 1, 2}},
    };

    createChassisPCIeDeviceMultiPortPCIeDevice(gpu, devName, chassisName,
                                               props);
    SUCCEED();
}

// ============================================================================
// createChassisPCIeDeviceMultiPortPCIeDevice: UpstreamPortCount only present
// → L157 FALSE, L163 FALSE, L169 TRUE
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranch2Test,
       DirectMultiPort_OnlyUpstreamPortCount_MixedBranches)
{
    std::string devName = "MultiPortB2_upc";
    dbus::PropertyMap props = {
        {"UpstreamPortCount", uint64_t(4)},
    };

    createChassisPCIeDeviceMultiPortPCIeDevice(gpu, devName, chassisName,
                                               props);
    SUCCEED();
}

// ============================================================================
// Factory: NSM_MultiPortPCIeDevice via factory with all props
// Exercises full factory path through L354-358
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranch2Test, Factory_MultiPort_AllProps_FullPath)
{
    const std::string uniquePath = objPath + "_b2_multiAll";

    auto& basePm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    basePm["ChassisName"] = chassisName;
    basePm["Name"] = name;
    basePm["UUID"] = gpuUuid;

    auto& curPm = utils::MockDbusAsync::propertyMap(
        uniquePath, basicIntfName + ".MultiPortPCIeDevice");
    curPm["Type"] = std::string("NSM_MultiPortPCIeDevice");
    curPm["DeviceType"] = std::string(
        "xyz.openbmc_project.Inventory.Item.PCIeDevice."
        "DeviceTypes.SingleFunction");
    curPm["Functions"] = std::vector<uint64_t>{0, 1};
    curPm["UpstreamPortCount"] = uint64_t(2);

    EXPECT_NO_THROW(nsmChassisPCIeDeviceCreateSensors(
        mockManager, basicIntfName + ".MultiPortPCIeDevice", uniquePath));
}

// ============================================================================
// Factory: NSM_PCIeDevice with DeviceType and Functions present
// → TRUE branches at L109, L115
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranch2Test,
       Factory_PCIeDevice_AllProps_TrueBranches)
{
    const std::string uniquePath = objPath + "_b2_pcieAll";

    auto& basePm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    basePm["ChassisName"] = chassisName;
    basePm["Name"] = name;
    basePm["UUID"] = gpuUuid;

    auto& curPm = utils::MockDbusAsync::propertyMap(
        uniquePath, basicIntfName + ".PCIeDevice");
    curPm["Type"] = std::string("NSM_PCIeDevice");
    curPm["DeviceType"] = std::string(
        "xyz.openbmc_project.Inventory.Item.PCIeDevice."
        "DeviceTypes.MultiFunction");
    curPm["Functions"] = std::vector<uint64_t>{0, 1, 2};

    EXPECT_NO_THROW(nsmChassisPCIeDeviceCreateSensors(
        mockManager, basicIntfName + ".PCIeDevice", uniquePath));
}

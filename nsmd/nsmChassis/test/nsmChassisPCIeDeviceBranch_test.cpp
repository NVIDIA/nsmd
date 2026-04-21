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
 * Branch coverage for nsmd/nsmChassis/nsmChassisPCIeDevice.cpp
 *
 * Covers:
 * - nsmChassisPCIeDeviceCreateSensors: FALSE branches for optional base props
 *   (ChassisName, Name, UUID absent), FALSE for DEVICE_UUID absent
 * - NSM_MultiPortPCIeDevice type: FALSE branches for DeviceType, Functions,
 *   UpstreamPortCount absent in createChassisPCIeDeviceMultiPortPCIeDevice
 * - NSM_PCIeDevice type: FALSE branches for DeviceType, Functions absent
 * - Unknown type: no type branch taken
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "nsmChassisPCIeDevice.hpp"

using namespace nsm;

namespace nsm
{
requester::Coroutine
    nsmChassisPCIeDeviceCreateSensors(SensorManager& manager,
                                      const std::string& interface,
                                      const std::string& objPath);
} // namespace nsm

struct NsmChassisPCIeDeviceBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisPCIeDevice";
    const std::string chassisName = "HGX_GPU_Branch_PCIeDevice";
    const std::string name = "PCIeBranchDevice1";
    const std::string objPath = chassisInventoryBasePath / chassisName / name;

    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:5";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;

    NsmChassisPCIeDeviceBranchTest() :
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

    ~NsmChassisPCIeDeviceBranchTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// Factory: ChassisName absent → L296 FALSE → chassisName stays ""
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranchTest, Factory_MissingChassisName_FalseBranch)
{
    const std::string uniquePath = objPath + "_noChassisName";

    auto& basePm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    // ChassisName absent → L296 FALSE
    basePm["Name"] = name;
    basePm["UUID"] = gpuUuid;

    auto& curPm = utils::MockDbusAsync::propertyMap(uniquePath,
                                                    basicIntfName + ".main");
    curPm["Type"] = std::string("NSM_ChassisPCIeDevice");
    curPm["DEVICE_UUID"] = gpuUuid;

    EXPECT_NO_THROW(nsmChassisPCIeDeviceCreateSensors(
        mockManager, basicIntfName + ".main", uniquePath));
}

// ============================================================================
// Factory: Name absent → L302 FALSE → name stays ""
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranchTest, Factory_MissingName_FalseBranch)
{
    const std::string uniquePath = objPath + "_noName";

    auto& basePm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    basePm["ChassisName"] = chassisName;
    // Name absent → L302 FALSE
    basePm["UUID"] = gpuUuid;

    auto& curPm = utils::MockDbusAsync::propertyMap(uniquePath,
                                                    basicIntfName + ".main2");
    curPm["Type"] = std::string("NSM_ChassisPCIeDevice");
    curPm["DEVICE_UUID"] = gpuUuid;

    // name="" → D-Bus object construction with empty path throws SdBusError
    EXPECT_THROW_COROUTINE(
        nsmChassisPCIeDeviceCreateSensors(mockManager, basicIntfName + ".main2",
                                          uniquePath),
        std::exception);
}

// ============================================================================
// Factory: Type absent → L307 FALSE → type="" → no type branch taken
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranchTest, Factory_MissingType_FalseBranch)
{
    const std::string uniquePath = objPath + "_noType";

    auto& basePm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    basePm["ChassisName"] = chassisName;
    basePm["Name"] = name;
    basePm["UUID"] = gpuUuid;

    // Type absent → L307 FALSE → type="" → no branch taken → NSM_SUCCESS
    (void)utils::MockDbusAsync::propertyMap(uniquePath,
                                            basicIntfName + ".notypeif");
    // Intentionally no "Type" key

    const size_t before = gpu->deviceSensors.size();
    EXPECT_NO_THROW(nsmChassisPCIeDeviceCreateSensors(
        mockManager, basicIntfName + ".notypeif", uniquePath));
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Factory: UUID absent → L312 FALSE → uuid="" → device may be null
// Use unknown type so no crash on null device
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranchTest, Factory_MissingUUID_UnknownType)
{
    const std::string uniquePath = objPath + "_noUUID";

    auto& basePm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    basePm["ChassisName"] = chassisName;
    basePm["Name"] = name;
    // UUID absent → L312 FALSE

    auto& curPm = utils::MockDbusAsync::propertyMap(uniquePath,
                                                    basicIntfName + ".noidif");
    curPm["Type"] = std::string("NSM_Unknown_Type_NoBranch");

    // UUID="" → getNsmDeviceFromStaticUUID throws (invalid format)
    EXPECT_THROW_COROUTINE(
        nsmChassisPCIeDeviceCreateSensors(
            mockManager, basicIntfName + ".noidif", uniquePath),
        std::runtime_error);
}

// ============================================================================
// Factory: NSM_ChassisPCIeDevice with DEVICE_UUID absent → L322 FALSE
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranchTest,
       Factory_NSMChassisPCIeDevice_MissingDEVICE_UUID)
{
    const std::string uniquePath = objPath + "_noDEVUUID";

    auto& basePm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    basePm["ChassisName"] = chassisName;
    basePm["Name"] = name;
    basePm["UUID"] = gpuUuid;

    auto& curPm = utils::MockDbusAsync::propertyMap(uniquePath,
                                                    basicIntfName + ".main3");
    curPm["Type"] = std::string("NSM_ChassisPCIeDevice");
    // DEVICE_UUID absent → L322 FALSE → deviceUuid stays ""

    EXPECT_NO_THROW(nsmChassisPCIeDeviceCreateSensors(
        mockManager, basicIntfName + ".main3", uniquePath));
}

// ============================================================================
// NSM_MultiPortPCIeDevice: DeviceType, Functions, UpstreamPortCount all
// absent → FALSE branches at L157, L163, L169 in
// createChassisPCIeDeviceMultiPortPCIeDevice
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranchTest,
       NSM_MultiPortPCIeDevice_NoOptionalProps_FalseBranches)
{
    const std::string uniquePath = objPath + "_multiport";

    auto& basePm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    basePm["ChassisName"] = chassisName;
    basePm["Name"] = name;
    basePm["UUID"] = gpuUuid;

    auto& curPm = utils::MockDbusAsync::propertyMap(
        uniquePath, basicIntfName + ".MultiPortPCIeDevice");
    curPm["Type"] = std::string("NSM_MultiPortPCIeDevice");
    // DeviceType, Functions, UpstreamPortCount absent → L157, L163, L169 FALSE

    EXPECT_NO_THROW(nsmChassisPCIeDeviceCreateSensors(
        mockManager, basicIntfName + ".MultiPortPCIeDevice", uniquePath));
}

// ============================================================================
// NSM_PCIeDevice: DeviceType absent → L112 FALSE,
// Functions absent → L118 FALSE in createChassisPCIeDevicePCIeDevice
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranchTest,
       NSM_PCIeDevice_NoDeviceTypeAndFunctions_FalseBranches)
{
    const std::string uniquePath = objPath + "_pciedev_noopt";

    auto& basePm = utils::MockDbusAsync::propertyMap(uniquePath, basicIntfName);
    basePm["ChassisName"] = chassisName;
    basePm["Name"] = name;
    basePm["UUID"] = gpuUuid;

    auto& curPm = utils::MockDbusAsync::propertyMap(
        uniquePath, basicIntfName + ".PCIeDeviceNoOpt");
    curPm["Type"] = std::string("NSM_PCIeDevice");
    // DeviceType absent → L112 FALSE, Functions absent → L118 FALSE

    EXPECT_NO_THROW(nsmChassisPCIeDeviceCreateSensors(
        mockManager, basicIntfName + ".PCIeDeviceNoOpt", uniquePath));
}

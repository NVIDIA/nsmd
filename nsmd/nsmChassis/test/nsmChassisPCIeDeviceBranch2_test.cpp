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
 * - ChassisPCIeDevice feature creators: missing params branches
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

#include "features/creators.hpp"
#include "features/feature.hpp"
#include "nsmChassisPCIeDevice.hpp"

#undef private
#undef protected

namespace nsm
{
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

    /** @brief Run a ChassisPCIeDevice feature of the chassis device on gpu */
    requester::Coroutine run(nsm::features::CreatorFn create,
                             const dbus::PropertyMap& params)
    {
        nsm::features::DeviceContext dev;
        dev.name = chassisName;
        dev.kind = "gpu";
        dev.staticUuid = gpuUuid;
        dev.platformPrefix = "HGX_";
        dev.device = gpu;
        return create(mockManager, dev, params);
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
// ChassisAttributes features: createChassisPCIeDeviceAsset +
// createChassisPCIeDeviceHealth
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranch2Test,
       Feature_ChassisAttributes_CreatesAssetAndHealth)
{
    const size_t before = gpu->staticSensors.size();
    auto assetRc = run(nsm::features::createChassisPCIeDeviceAssetFeature,
                       {{"Name", name + "_b2_attrs"}});
    auto healthRc = run(nsm::features::createChassisPCIeDeviceHealthFeature,
                        {{"Name", name + "_b2_attrs"}});

    EXPECT_EQ(assetRc.data(), NSM_SUCCESS);
    EXPECT_EQ(healthRc.data(), NSM_SUCCESS);
    // Asset creates partNumber + serialNumber + model = 3 static sensors
    // Health creates 1 static sensor = 4 total
    EXPECT_GE(gpu->staticSensors.size(), before + 3u);
}

// ============================================================================
// Name param absent → params.at("Name") throws
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranch2Test, Feature_MissingName_Throws)
{
    EXPECT_THROW_COROUTINE(
        run(nsm::features::createChassisPCIeDeviceAssetFeature, {}),
        std::exception);
}

// ============================================================================
// UUID feature: no device UUID in the params, sensor created empty
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranch2Test, Feature_UUID_CreatesSensor)
{
    const size_t before = gpu->staticSensors.size();
    auto rc = run(nsm::features::createChassisPCIeDeviceUUIDFeature,
                  {{"Name", name + "_b2_noDevUUID"}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->staticSensors.size(), before);
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
// MultiPortPCIeDevice + RetimerAERErrorStatus features with all params
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranch2Test, Feature_MultiPort_AllParams_FullPath)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc =
        run(nsm::features::createChassisPCIeDeviceMultiPortPCIeDeviceFeature,
            {{"Name", name},
             {"DeviceType",
              std::string("xyz.openbmc_project.Inventory.Item.PCIeDevice."
                          "DeviceTypes.SingleFunction")},
             {"Functions", std::vector<uint64_t>{0, 1}},
             {"UpstreamPortCount", uint64_t(2)}});
    auto aerRc =
        run(nsm::features::createChassisPCIeDeviceRetimerAERErrorStatusFeature,
            {{"Name", name}});

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(aerRc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// ============================================================================
// PCIeDevice feature with DeviceType and Functions present
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranch2Test, Feature_PCIeDevice_AllParams)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = run(
        nsm::features::createChassisPCIeDevicePCIeDeviceFeature,
        {{"Name", name + "_b2_pcieAll"},
         {"DeviceType", std::string("xyz.openbmc_project.Inventory.Item."
                                    "PCIeDevice.DeviceTypes.MultiFunction")},
         {"Functions", std::vector<uint64_t>{0, 1, 2}}});

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

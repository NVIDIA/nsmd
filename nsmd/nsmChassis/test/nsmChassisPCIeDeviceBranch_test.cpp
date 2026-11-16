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
 * Branch coverage for the ChassisPCIeDevice feature creators of
 * nsmd/nsmChassis/nsmChassisPCIeDevice.cpp
 *
 * Covers:
 * - Name param absent: the creators throw
 * - UUID feature: sensor created empty (read over MCTP in update())
 * - Associations feature: no ChassisAssociationPaths → no sensor
 * - MultiPortPCIeDevice: DeviceType, Functions, UpstreamPortCount absent in
 *   createChassisPCIeDeviceMultiPortPCIeDevice
 * - PCIeDevice: DeviceType, Functions absent
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

using namespace nsm;

struct NsmChassisPCIeDeviceBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "HGX_GPU_Branch_PCIeDevice";
    const std::string name = "PCIeBranchDevice1";

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
// Name param absent → params.at("Name") throws
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranchTest, Feature_MissingName_Throws)
{
    EXPECT_THROW_COROUTINE(
        run(nsm::features::createChassisPCIeDeviceUUIDFeature, {}),
        std::out_of_range);
}

// ============================================================================
// UUID feature: UUID sensor created without a device UUID
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranchTest, Feature_UUID_CreatesStaticSensor)
{
    const size_t before = gpu->staticSensors.size();
    auto rc = run(nsm::features::createChassisPCIeDeviceUUIDFeature,
                  {{"Name", name}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->staticSensors.size(), before + 1);
}

// ============================================================================
// Associations feature: no ChassisAssociationPaths → no sensor
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranchTest, Feature_Associations_NoPaths_NoSensor)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = run(nsm::features::createChassisPCIeDeviceAssociationsFeature,
                  {{"Name", name}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// ============================================================================
// MultiPortPCIeDevice: DeviceType, Functions, UpstreamPortCount all absent →
// defaults in createChassisPCIeDeviceMultiPortPCIeDevice
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranchTest,
       MultiPortPCIeDevice_NoOptionalParams_Defaults)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc =
        run(nsm::features::createChassisPCIeDeviceMultiPortPCIeDeviceFeature,
            {{"Name", name + "_multiport"}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// ============================================================================
// PCIeDevice: DeviceType and Functions absent → defaults in
// createChassisPCIeDevicePCIeDevice
// ============================================================================

TEST_F(NsmChassisPCIeDeviceBranchTest,
       PCIeDevice_NoDeviceTypeAndFunctions_Defaults)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = run(nsm::features::createChassisPCIeDevicePCIeDeviceFeature,
                  {{"Name", name + "_pciedev_noopt"}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

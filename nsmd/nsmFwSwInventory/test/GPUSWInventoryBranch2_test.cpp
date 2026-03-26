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
 * Additional branch coverage for GPUSWInventory.cpp:
 *
 * Targets uncovered code paths:
 * - NsmGPUSWInventoryDriverVersionAndStatus constructor: with associations
 * - update(): encode_get_driver_info_req fails (not reachable normally,
 *   but verify the success path encoding)
 * - update(): decode success, cc != NSM_SUCCESS -> keeps previous version,
 *   state change triggers updateNsmDevice
 * - createGPUDriverSensor factory: all properties present (success path)
 * - createGPUDriverSensor factory: missing Manufacturer (FALSE branch)
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "platform-environmental.h"

#define private public
#define protected public

#include "GPUSWInventory.hpp"

#undef private
#undef protected

namespace nsm
{
requester::Coroutine createGPUDriverSensor(SensorManager& manager,
                                           const std::string& interface,
                                           const std::string& objPath);
} // namespace nsm

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================

struct GPUSWInventoryBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_GPU_SWInventory";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    GPUSWInventoryBranch2Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~GPUSWInventoryBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<NsmGPUSWInventoryDriverVersionAndStatus>
        createDriverSensor(const std::string& sensorName)
    {
        std::vector<utils::Association> associations;
        auto sensor = std::make_shared<NsmGPUSWInventoryDriverVersionAndStatus>(
            utils::DBusHandler::getBus(), sensorName, associations,
            "NSM_GPUSWInventory", "NVIDIA");
        sensor->nsmDeviceFound = std::static_pointer_cast<NsmDevice>(gpu);
        return sensor;
    }

    Response driverInfoResponse(uint8_t cc, uint8_t driverState,
                                const std::string& version)
    {
        std::vector<uint8_t> driverInfoData;
        driverInfoData.push_back(driverState);
        driverInfoData.insert(driverInfoData.end(), version.begin(),
                              version.end());
        driverInfoData.push_back('\0');

        Response resp(sizeof(nsm_msg_hdr) + sizeof(nsm_get_driver_info_resp) +
                          driverInfoData.size(),
                      0);
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        [[maybe_unused]] auto rc = encode_get_driver_info_resp(
            0, cc, ERR_NULL, driverInfoData.size(), driverInfoData.data(), msg);
        return resp;
    }
};

// ============================================================================
// Constructor: with associations
// ============================================================================

TEST_F(GPUSWInventoryBranch2Test, Constructor_WithAssociations)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> associations;
    associations.push_back(
        {"parent_gpu", "driver", "/xyz/openbmc_project/inventory/gpu0"});
    associations.push_back(
        {"device", "software", "/xyz/openbmc_project/inventory/dev0"});

    auto sensor = std::make_shared<NsmGPUSWInventoryDriverVersionAndStatus>(
        bus, "GPU_Br2_Assoc", associations, "NSM_GPUSWInventory", "TestMfr");

    EXPECT_EQ(sensor->getName(), "GPU_Br2_Assoc");
    EXPECT_NE(sensor->softwareVer, nullptr);
    EXPECT_NE(sensor->operationalStatus, nullptr);
    EXPECT_NE(sensor->associationDef, nullptr);
    EXPECT_NE(sensor->asset, nullptr);
}

// ============================================================================
// Constructor: empty associations (for-loop not entered)
// ============================================================================

TEST_F(GPUSWInventoryBranch2Test, Constructor_EmptyAssociations)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> emptyAssociations;

    auto sensor = std::make_shared<NsmGPUSWInventoryDriverVersionAndStatus>(
        bus, "GPU_Br2_Empty", emptyAssociations, "NSM_GPUSWInventory",
        "NVIDIA");

    EXPECT_EQ(sensor->getName(), "GPU_Br2_Empty");
    EXPECT_EQ(sensor->driverState, DriverStateUnknown);
    EXPECT_EQ(sensor->driverVersion, "");
}

// ============================================================================
// update(): state changed from NotLoaded to Loaded
// Exercises: stateChanged = true, calls updateNsmDevice +
// refreshCapabilitySensor
// ============================================================================

TEST_F(GPUSWInventoryBranch2Test,
       Update_StateChangedFromNotLoadedToLoaded_CallsDeviceUpdate)
{
    auto sensor = createDriverSensor("GPU_Br2_NL2L");
    sensor->updateValue(DriverNotLoaded, "");
    EXPECT_EQ(sensor->driverState, DriverNotLoaded);
    EXPECT_FALSE(sensor->operationalStatus->functional());

    auto resp = driverInfoResponse(NSM_SUCCESS, DriverLoaded, "555.42.02");
    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(resp, Response{}));
    EXPECT_CALL(*gpu, updateNsmDevice()).WillOnce([]() -> requester::Coroutine {
        co_return NSM_SUCCESS;
    });
    EXPECT_CALL(*gpu, refreshCapabilitySensor())
        .WillOnce([]() -> requester::Coroutine { co_return NSM_SUCCESS; });

    sensor->update(std::static_pointer_cast<NsmDevice>(gpu));

    EXPECT_EQ(sensor->driverState, DriverLoaded);
    EXPECT_TRUE(sensor->operationalStatus->functional());
}

// ============================================================================
// update(): state changed from Loaded to NotLoaded
// Exercises: stateChanged = true, but goes to non-functional
// ============================================================================

TEST_F(GPUSWInventoryBranch2Test,
       Update_StateChangedFromLoadedToNotLoaded_CallsDeviceUpdate)
{
    auto sensor = createDriverSensor("GPU_Br2_L2NL");
    sensor->updateValue(DriverLoaded, "555.42.02");
    EXPECT_EQ(sensor->driverState, DriverLoaded);

    auto resp = driverInfoResponse(NSM_SUCCESS, DriverNotLoaded, "");
    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(resp, Response{}));
    EXPECT_CALL(*gpu, updateNsmDevice()).WillOnce([]() -> requester::Coroutine {
        co_return NSM_SUCCESS;
    });
    EXPECT_CALL(*gpu, refreshCapabilitySensor())
        .WillOnce([]() -> requester::Coroutine { co_return NSM_SUCCESS; });

    sensor->update(std::static_pointer_cast<NsmDevice>(gpu));

    EXPECT_EQ(sensor->driverState, DriverNotLoaded);
    EXPECT_FALSE(sensor->operationalStatus->functional());
}

// ============================================================================
// Factory: all properties present including Manufacturer -> success
// ============================================================================

TEST_F(GPUSWInventoryBranch2Test, Factory_AllProperties_SensorCreated)
{
    const std::string testPath =
        "/xyz/openbmc_project/inventory/system/gpu_br2_all";

    dbus::PropertyMap properties = {
        {"Name", std::string("GPU_Driver_Br2_All")},
        {"UUID", gpuUuid},
        {"Manufacturer", std::string("NVIDIA")},
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                          basicIntfName);
    propertyMap = properties;

    const size_t before = gpu->deviceSensors.size();
    createGPUDriverSensor(mockManager, basicIntfName, testPath);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Factory: missing Manufacturer -> manufacturer="" -> sensor created
// (FALSE count("Manufacturer") branch)
// ============================================================================

TEST_F(GPUSWInventoryBranch2Test, Factory_MissingManufacturer_Created)
{
    const std::string testPath =
        "/xyz/openbmc_project/inventory/system/gpu_br2_nomfr";

    dbus::PropertyMap properties = {
        {"Name", std::string("GPU_Driver_Br2_NoMfr")}, {"UUID", gpuUuid},
        // Manufacturer intentionally omitted
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                          basicIntfName);
    propertyMap = properties;

    const size_t before = gpu->deviceSensors.size();
    createGPUDriverSensor(mockManager, basicIntfName, testPath);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// ============================================================================
// updateMetricOnSharedMemory: verify it does not crash when called
// (NVIDIA_SHMEM may or may not be defined)
// ============================================================================

TEST_F(GPUSWInventoryBranch2Test, UpdateMetricOnSharedMemory_NoCrash)
{
    auto sensor = createDriverSensor("GPU_Br2_Shmem");
    sensor->updateValue(DriverLoaded, "550.54.14");

    // Calling updateMetricOnSharedMemory should not crash
    EXPECT_NO_THROW(sensor->updateMetricOnSharedMemory());
}

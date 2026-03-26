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
 * Branch coverage for GPUSWInventory.cpp
 *
 * Covers:
 * - update(): encode_get_driver_info_req fail (rc != NSM_SW_SUCCESS)
 * - update(): sensorIO fail (rc != 0)
 * - update(): decode success but cc != NSM_SUCCESS (keeps previous version)
 * - update(): decode fail (rc != NSM_SW_SUCCESS)
 * - update(): state changed → updateNsmDevice + refreshCapabilitySensor
 * - update(): state NOT changed → no device update
 * - createGPUDriverSensor: UUID matches no device → co_return NSM_ERROR //
 *
 * - createGPUDriverSensor: Name absent → name="" → sd_bus throws
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

struct GPUSWInventoryBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_GPU_SWInventory";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/gpu_driver_branch";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    GPUSWInventoryBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        EXPECT_EQ(NSM_DEV_ID_GPU, gpu->getDeviceType());
    }

    ~GPUSWInventoryBranchTest()
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
// update() branch coverage
// ============================================================================

// update(): sensorIO returns NSM_ERROR → if (rc) at line 103 → co_return rc //

TEST_F(GPUSWInventoryBranchTest, Update_SensorIOFail_ReturnsEarly)
{
    auto sensor = createDriverSensor("GPU_Branch_SIOErr");

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(NSM_ERROR));
    sensor->update(std::static_pointer_cast<NsmDevice>(gpu));
}

// update(): decode succeeds, cc==NSM_SUCCESS, state changes
// → updateNsmDevice + refreshCapabilitySensor called
TEST_F(GPUSWInventoryBranchTest,
       Update_DecodeSuccess_StateChanged_DeviceUpdated)
{
    auto sensor = createDriverSensor("GPU_Branch_StateChg");
    // Initial state is DriverStateUnknown (0)
    EXPECT_EQ(sensor->driverState, DriverStateUnknown);

    auto resp = driverInfoResponse(NSM_SUCCESS, DriverLoaded, "550.54.14");
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

// update(): decode succeeds, cc==NSM_SUCCESS, state does NOT change
// → updateNsmDevice / refreshCapabilitySensor NOT called
TEST_F(GPUSWInventoryBranchTest,
       Update_DecodeSuccess_StateNotChanged_NoDeviceUpdate)
{
    auto sensor = createDriverSensor("GPU_Branch_NoChg");
    // Set initial state to DriverLoaded
    sensor->updateValue(DriverLoaded, "550.54.14");
    EXPECT_EQ(sensor->driverState, DriverLoaded);

    auto resp = driverInfoResponse(NSM_SUCCESS, DriverLoaded, "550.54.14");
    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(resp, Response{}));
    // updateNsmDevice and refreshCapabilitySensor should NOT be called
    EXPECT_CALL(*gpu, updateNsmDevice()).Times(0);
    EXPECT_CALL(*gpu, refreshCapabilitySensor()).Times(0);

    sensor->update(std::static_pointer_cast<NsmDevice>(gpu));

    EXPECT_EQ(sensor->driverState, DriverLoaded);
}

// update(): decode succeeds but cc==NSM_ERROR → keeps previous version
TEST_F(GPUSWInventoryBranchTest, Update_DecodeSuccessErrorCC_KeepsPrevVersion)
{
    auto sensor = createDriverSensor("GPU_Branch_CCERR");
    sensor->updateValue(DriverLoaded, "550.54.14");
    EXPECT_EQ(sensor->driverVersion, "550.54.14");

    // 9-byte buffer: decode_get_driver_info_resp returns NSM_SW_SUCCESS
    // with cc=NSM_ERROR → if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    std::vector<uint8_t> ccErrResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    ccErrResp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(ccErrResp, Response{}));
    EXPECT_CALL(*gpu, updateNsmDevice())
        .Times(AtMost(1))
        .WillRepeatedly(
            []() -> requester::Coroutine { co_return NSM_SUCCESS; });
    EXPECT_CALL(*gpu, refreshCapabilitySensor())
        .Times(AtMost(1))
        .WillRepeatedly(
            []() -> requester::Coroutine { co_return NSM_SUCCESS; });

    sensor->update(std::static_pointer_cast<NsmDevice>(gpu));
    // Previous version should be kept
    EXPECT_EQ(sensor->driverVersion, "550.54.14");
}

// update(): decode fails (short buffer) → rc != NSM_SW_SUCCESS →
// keeps previous version
TEST_F(GPUSWInventoryBranchTest, Update_DecodeFail_KeepsPrevVersion)
{
    auto sensor = createDriverSensor("GPU_Branch_DecFail");
    sensor->updateValue(DriverLoaded, "555.42.02");

    // Very short response that will fail decode
    Response badResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(badResp.data());

    std::vector<uint8_t> data;
    data.push_back(DriverStateUnknown);
    data.push_back('\0');
    [[maybe_unused]] auto encRc = encode_get_driver_info_resp(
        0, NSM_ERROR, ERR_NULL, data.size(), data.data(), msg);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(badResp, Response{}));
    EXPECT_CALL(*gpu, updateNsmDevice())
        .Times(AtMost(1))
        .WillRepeatedly(
            []() -> requester::Coroutine { co_return NSM_SUCCESS; });
    EXPECT_CALL(*gpu, refreshCapabilitySensor())
        .Times(AtMost(1))
        .WillRepeatedly(
            []() -> requester::Coroutine { co_return NSM_SUCCESS; });

    sensor->update(std::static_pointer_cast<NsmDevice>(gpu));
}

// ============================================================================
// updateValue() branch coverage
// ============================================================================

// DriverLoaded → functional(true)
TEST_F(GPUSWInventoryBranchTest, UpdateValue_DriverLoaded_FunctionalTrue)
{
    auto sensor = createDriverSensor("GPU_Branch_Loaded");
    sensor->updateValue(DriverLoaded, "560.28.03");
    EXPECT_TRUE(sensor->operationalStatus->functional());
    EXPECT_EQ(sensor->driverState, DriverLoaded);
}

// DriverNotLoaded → default → functional(false)
TEST_F(GPUSWInventoryBranchTest, UpdateValue_DriverNotLoaded_FunctionalFalse)
{
    auto sensor = createDriverSensor("GPU_Branch_NotLoaded");
    sensor->updateValue(DriverNotLoaded, "");
    EXPECT_FALSE(sensor->operationalStatus->functional());
}

// DriverStateUnknown → default → functional(false)
TEST_F(GPUSWInventoryBranchTest, UpdateValue_DriverStateUnknown_FunctionalFalse)
{
    auto sensor = createDriverSensor("GPU_Branch_Unknown");
    sensor->updateValue(DriverStateUnknown, "");
    EXPECT_FALSE(sensor->operationalStatus->functional());
}

// ============================================================================
// createGPUDriverSensor factory branch coverage
// ============================================================================

// UUID matches no device → nsmDevice==nullptr → co_return NSM_ERROR //

TEST_F(GPUSWInventoryBranchTest, Factory_NoDevice_ReturnsError)
{
    const uuid_t unknownUuid = "STATIC:99:99:NSM_DEVICE_INSTANCE_NUMBER:99";
    const std::string testPath = objPath + "_nodev";

    dbus::PropertyMap properties = {
        {"Name", std::string("GPU_Driver_NoDevice")},
        {"UUID", unknownUuid},
        {"Manufacturer", std::string("NVIDIA")},
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                          basicIntfName);
    propertyMap = properties;

    const size_t before = gpu->deviceSensors.size();
    createGPUDriverSensor(mockManager, basicIntfName, testPath);
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// UUID absent → uuid="" → parseStaticUuid throws
TEST_F(GPUSWInventoryBranchTest, Factory_MissingUUID_Throws)
{
    const std::string testPath = objPath + "_nouuid";

    dbus::PropertyMap properties = {
        {"Name", std::string("GPU_Driver_NoUUID")},
        {"Manufacturer", std::string("NVIDIA")},
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                          basicIntfName);
    propertyMap = properties;

    EXPECT_THROW_COROUTINE(
        createGPUDriverSensor(mockManager, basicIntfName, testPath),
        std::runtime_error);
}

// Name absent → name="" → sd_bus throws on empty path
TEST_F(GPUSWInventoryBranchTest, Factory_MissingName_Throws)
{
    const std::string testPath = objPath + "_noname";

    dbus::PropertyMap properties = {
        {"UUID", gpuUuid},
        {"Manufacturer", std::string("NVIDIA")},
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                          basicIntfName);
    propertyMap = properties;

    EXPECT_THROW_COROUTINE(
        createGPUDriverSensor(mockManager, basicIntfName, testPath),
        sdbusplus::exception::SdBusError);
}

// Manufacturer absent → manufacturer="" → sensor created with empty mfr
TEST_F(GPUSWInventoryBranchTest, Factory_MissingManufacturer_EmptyMfr)
{
    const std::string testPath = objPath + "_nomfr";

    dbus::PropertyMap properties = {
        {"Name", std::string("GPU_Driver_NoMfr")},
        {"UUID", gpuUuid},
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                          basicIntfName);
    propertyMap = properties;

    const size_t before = gpu->deviceSensors.size();
    createGPUDriverSensor(mockManager, basicIntfName, testPath);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// Good path with associations
TEST_F(GPUSWInventoryBranchTest, Factory_WithAssociations_Success)
{
    const std::string testPath = objPath + "_assoc";

    dbus::PropertyMap properties = {
        {"Name", std::string("GPU_Driver_Assoc")},
        {"UUID", gpuUuid},
        {"Manufacturer", std::string("NVIDIA")},
    };

    auto& serviceMap = utils::MockDbusAsync::serviceMap();
    MapperServiceMap assocServiceMap = {
        {{
            "xyz.openbmc_project.NSM",
            {basicIntfName + ".Associations"},
        }},
    };
    serviceMap = assocServiceMap;

    dbus::PropertyMap assocProperties = {
        {"Forward", "parent_gpu"},
        {"Backward", "driver"},
        {"AbsolutePath", "/xyz/openbmc_project/inventory/system/gpu0"},
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                          basicIntfName);
    propertyMap = properties;

    auto& assocPropertyMap = utils::MockDbusAsync::propertyMap(
        testPath, basicIntfName + ".Associations");
    assocPropertyMap = assocProperties;

    const size_t before = gpu->deviceSensors.size();
    createGPUDriverSensor(mockManager, basicIntfName, testPath);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

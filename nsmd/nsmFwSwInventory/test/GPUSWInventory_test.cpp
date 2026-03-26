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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "platform-environmental.h"

#define private public
#define protected public

#include "GPUSWInventory.hpp"

namespace nsm
{
requester::Coroutine createGPUDriverSensor(SensorManager& manager,
                                           const std::string& interface,
                                           const std::string& objPath);
} // namespace nsm

using namespace nsm;

auto bus = sdbusplus::bus::new_default();

TEST(NsmGPUSWInventoryDriverVersionAndStatus, Constructor)
{
    std::string name = "GPU0_Driver";
    std::string type = "NSM_GPUSWInventory";
    std::string manufacturer = "NVIDIA";

    std::vector<utils::Association> associations;
    associations.push_back(
        {"parent_gpu", "driver", "/xyz/openbmc_project/inventory/system/gpu0"});

    NsmGPUSWInventoryDriverVersionAndStatus gpuSW(bus, name, associations, type,
                                                  manufacturer);

    EXPECT_EQ(gpuSW.getName(), name);
    EXPECT_EQ(gpuSW.getType(), type);
    EXPECT_NE(gpuSW.softwareVer, nullptr);
    EXPECT_NE(gpuSW.operationalStatus, nullptr);
    EXPECT_NE(gpuSW.associationDef, nullptr);
    EXPECT_NE(gpuSW.asset, nullptr);
}

TEST(NsmGPUSWInventoryDriverVersionAndStatus, UpdateValueDriverLoaded)
{
    std::string name = "GPU1_Driver";
    std::string type = "NSM_GPUSWInventory";
    std::string manufacturer = "NVIDIA";

    std::vector<utils::Association> associations;
    NsmGPUSWInventoryDriverVersionAndStatus gpuSW(bus, name, associations, type,
                                                  manufacturer);

    uint8_t driverState = DriverLoaded;
    std::string driverVersion = "550.54.14";

    gpuSW.updateValue(driverState, driverVersion);

    EXPECT_EQ(gpuSW.driverState, DriverLoaded);
    EXPECT_EQ(gpuSW.driverVersion, driverVersion);
    EXPECT_TRUE(gpuSW.operationalStatus->functional());
    EXPECT_EQ(gpuSW.softwareVer->version(), driverVersion);
}

TEST(NsmGPUSWInventoryDriverVersionAndStatus, UpdateValueDriverNotLoaded)
{
    std::string name = "GPU2_Driver";
    std::string type = "NSM_GPUSWInventory";
    std::string manufacturer = "NVIDIA";

    std::vector<utils::Association> associations;
    NsmGPUSWInventoryDriverVersionAndStatus gpuSW(bus, name, associations, type,
                                                  manufacturer);

    uint8_t driverState = DriverNotLoaded;
    std::string driverVersion = "";

    gpuSW.updateValue(driverState, driverVersion);

    EXPECT_EQ(gpuSW.driverState, DriverNotLoaded);
    EXPECT_EQ(gpuSW.driverVersion, driverVersion);
    EXPECT_FALSE(gpuSW.operationalStatus->functional());
}

TEST(NsmGPUSWInventoryDriverVersionAndStatus, UpdateValueDriverStateUnknown)
{
    std::string name = "GPU3_Driver";
    std::string type = "NSM_GPUSWInventory";
    std::string manufacturer = "NVIDIA";

    std::vector<utils::Association> associations;
    NsmGPUSWInventoryDriverVersionAndStatus gpuSW(bus, name, associations, type,
                                                  manufacturer);

    uint8_t driverState = DriverStateUnknown;
    std::string driverVersion = "";

    gpuSW.updateValue(driverState, driverVersion);

    EXPECT_EQ(gpuSW.driverState, DriverStateUnknown);
    EXPECT_FALSE(gpuSW.operationalStatus->functional());
}

TEST(NsmGPUSWInventoryDriverVersionAndStatus, UpdateValueMultipleTimes)
{
    std::string name = "GPU4_Driver";
    std::string type = "NSM_GPUSWInventory";
    std::string manufacturer = "NVIDIA";

    std::vector<utils::Association> associations;
    NsmGPUSWInventoryDriverVersionAndStatus gpuSW(bus, name, associations, type,
                                                  manufacturer);

    // First update: Driver loaded
    gpuSW.updateValue(DriverLoaded, "550.54.14");
    EXPECT_TRUE(gpuSW.operationalStatus->functional());
    EXPECT_EQ(gpuSW.softwareVer->version(), "550.54.14");

    // Second update: Driver not loaded
    gpuSW.updateValue(DriverNotLoaded, "");
    EXPECT_FALSE(gpuSW.operationalStatus->functional());
    EXPECT_EQ(gpuSW.softwareVer->version(), "");

    // Third update: Driver loaded again with new version
    gpuSW.updateValue(DriverLoaded, "560.28.03");
    EXPECT_TRUE(gpuSW.operationalStatus->functional());
    EXPECT_EQ(gpuSW.softwareVer->version(), "560.28.03");
}

struct NsmGPUSWInventoryTestFixture :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_GPU_SWInventory";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/gpu_driver";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:2";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmGPUSWInventoryTestFixture() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmGPUSWInventoryTestFixture()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmGPUSWInventoryTestFixture, goodTestCreateGPUDriverSensor)
{
    dbus::PropertyMap properties = {
        {"Name", std::string("GPU_Driver")},
        {"UUID", gpuUuid},
        {"Manufacturer", std::string("NVIDIA")},
    };

    // Setup associations
    auto& serviceMap = utils::MockDbusAsync::serviceMap();
    MapperServiceMap assocServiceMap = {
        {
            {
                "xyz.openbmc_project.NSM",
                {
                    basicIntfName + ".Associations",
                },
            },
        },
    };
    serviceMap = assocServiceMap;

    dbus::PropertyMap assocProperties = {
        {"Forward", "parent_gpu"},
        {"Backward", "driver"},
        {"AbsolutePath", "/xyz/openbmc_project/inventory/system/gpu0"},
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = properties;

    auto& assocPropertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".Associations");
    assocPropertyMap = assocProperties;

    createGPUDriverSensor(mockManager, basicIntfName, objPath);

    EXPECT_EQ(2, gpu->deviceSensors.size());

    // Skip msgTypes sensor at index 0
    auto driverSensor =
        std::dynamic_pointer_cast<NsmGPUSWInventoryDriverVersionAndStatus>(
            gpu->deviceSensors[1]);
    EXPECT_NE(driverSensor, nullptr);
}

TEST_F(NsmGPUSWInventoryTestFixture, badTestMissingUUID)
{
    dbus::PropertyMap properties = {
        {"Name", std::string("GPU_Driver")},
        {"Manufacturer", std::string("NVIDIA")},
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = properties;

    EXPECT_THROW_COROUTINE(
        createGPUDriverSensor(mockManager, basicIntfName, objPath),
        std::runtime_error);
}

TEST_F(NsmGPUSWInventoryTestFixture, testDriverSensorUpdate)
{
    std::string name = "GPU_Driver_Update";
    std::string type = "NSM_GPUSWInventory";
    std::string manufacturer = "NVIDIA";

    std::vector<utils::Association> associations;
    auto driverSensor =
        std::make_shared<NsmGPUSWInventoryDriverVersionAndStatus>(
            utils::DBusHandler::getBus(), name, associations, type,
            manufacturer);
    driverSensor->nsmDeviceFound = std::static_pointer_cast<NsmDevice>(gpu);

    // Mock driver info response
    Response driverInfoResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_driver_info_resp) + 64, 0);
    auto msg = reinterpret_cast<nsm_msg*>(driverInfoResp.data());

    std::string driverVersion = "550.54.14";
    // Create driver info data: driver_state (1 byte) + driver_version (string)
    std::vector<uint8_t> driverInfoData;
    driverInfoData.push_back(DriverLoaded); // driver_state
    driverInfoData.insert(driverInfoData.end(), driverVersion.begin(),
                          driverVersion.end());
    driverInfoData.push_back('\0'); // null terminator

    auto rc = encode_get_driver_info_resp(0, NSM_SUCCESS, ERR_NULL,
                                          driverInfoData.size(),
                                          driverInfoData.data(), msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(driverInfoResp, Response{}));

    // Expect calls to updateNsmDevice and refreshCapabilitySensor when state
    // changes
    EXPECT_CALL(*gpu, updateNsmDevice()).WillOnce([]() -> requester::Coroutine {
        co_return NSM_SUCCESS;
    });
    EXPECT_CALL(*gpu, refreshCapabilitySensor())
        .WillOnce([]() -> requester::Coroutine { co_return NSM_SUCCESS; });

    driverSensor->update(std::static_pointer_cast<NsmDevice>(gpu));

    EXPECT_EQ(driverSensor->driverState, DriverLoaded);
    // Compare only the actual version string (trim null padding)
    EXPECT_EQ(std::string(driverSensor->driverVersion.c_str()), driverVersion);
}

TEST_F(NsmGPUSWInventoryTestFixture, testDriverSensorUpdateError)
{
    std::string name = "GPU_Driver_Error";
    std::string type = "NSM_GPUSWInventory";
    std::string manufacturer = "NVIDIA";

    std::vector<utils::Association> associations;
    auto driverSensor =
        std::make_shared<NsmGPUSWInventoryDriverVersionAndStatus>(
            utils::DBusHandler::getBus(), name, associations, type,
            manufacturer);
    driverSensor->nsmDeviceFound = std::static_pointer_cast<NsmDevice>(gpu);

    // Mock error response
    Response errorResp(sizeof(nsm_msg_hdr) + sizeof(nsm_get_driver_info_resp) +
                           MAX_VERSION_STRING_SIZE,
                       0);
    auto msg = reinterpret_cast<nsm_msg*>(errorResp.data());

    // Create driver info data for error case
    std::vector<uint8_t> driverInfoData;
    driverInfoData.push_back(DriverStateUnknown); // driver_state
    driverInfoData.push_back('\0');               // empty version string

    auto rc = encode_get_driver_info_resp(0, NSM_ERROR, 0x1234,
                                          driverInfoData.size(),
                                          driverInfoData.data(), msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(errorResp, Response{}));

    // May or may not call updateNsmDevice/refreshCapabilitySensor depending on
    // state change
    EXPECT_CALL(*gpu, updateNsmDevice())
        .Times(testing::AtMost(1))
        .WillRepeatedly(
            []() -> requester::Coroutine { co_return NSM_SUCCESS; });
    EXPECT_CALL(*gpu, refreshCapabilitySensor())
        .Times(testing::AtMost(1))
        .WillRepeatedly(
            []() -> requester::Coroutine { co_return NSM_SUCCESS; });

    driverSensor->update(std::static_pointer_cast<NsmDevice>(gpu));
}

TEST_F(NsmGPUSWInventoryTestFixture, testDriverSensorUpdateMultipleVersions)
{
    std::string name = "GPU_Driver_Multi";
    std::string type = "NSM_GPUSWInventory";
    std::string manufacturer = "NVIDIA";

    std::vector<utils::Association> associations;
    auto driverSensor =
        std::make_shared<NsmGPUSWInventoryDriverVersionAndStatus>(
            utils::DBusHandler::getBus(), name, associations, type,
            manufacturer);
    driverSensor->nsmDeviceFound = std::static_pointer_cast<NsmDevice>(gpu);

    // Test multiple driver versions
    std::vector<std::pair<uint8_t, std::string>> testCases = {
        {DriverLoaded, "550.54.14"},
        {DriverLoaded, "555.42.02"},
        {DriverNotLoaded, ""},
        {DriverLoaded, "560.28.03"},
        {DriverStateUnknown, ""}};

    // Expect multiple calls to updateNsmDevice and refreshCapabilitySensor
    EXPECT_CALL(*gpu, updateNsmDevice())
        .Times(testing::AtLeast(1))
        .WillRepeatedly(
            []() -> requester::Coroutine { co_return NSM_SUCCESS; });
    EXPECT_CALL(*gpu, refreshCapabilitySensor())
        .Times(testing::AtLeast(1))
        .WillRepeatedly(
            []() -> requester::Coroutine { co_return NSM_SUCCESS; });

    for (const auto& [state, version] : testCases)
    {
        Response driverInfoResp(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_driver_info_resp) + 64, 0);
        auto msg = reinterpret_cast<nsm_msg*>(driverInfoResp.data());

        // Create driver info data: driver_state (1 byte) + driver_version
        // (string)
        std::vector<uint8_t> driverInfoData;
        driverInfoData.push_back(state); // driver_state
        driverInfoData.insert(driverInfoData.end(), version.begin(),
                              version.end());
        driverInfoData.push_back('\0'); // null terminator

        auto rc = encode_get_driver_info_resp(0, NSM_SUCCESS, ERR_NULL,
                                              driverInfoData.size(),
                                              driverInfoData.data(), msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);

        EXPECT_CALL(*gpu, sensorIO)
            .WillOnce(mockSensorIO(driverInfoResp, Response{}));

        auto coro =
            driverSensor->update(std::static_pointer_cast<NsmDevice>(gpu));
        // Wait for coroutine to complete
        while (!coro.done())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        EXPECT_EQ(driverSensor->driverState, state);
        if (state == DriverLoaded)
        {
            EXPECT_TRUE(driverSensor->operationalStatus->functional());
        }
        else
        {
            EXPECT_FALSE(driverSensor->operationalStatus->functional());
        }
    }
}

TEST_F(NsmGPUSWInventoryTestFixture, badTestCreateGPUDriverSensorMissingName)
{
    dbus::PropertyMap properties = {
        {"UUID", gpuUuid},
        {"Manufacturer", std::string("NVIDIA")},
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath + "_noname",
                                                          basicIntfName);
    propertyMap = properties;

    // Should still work, name might be empty
    EXPECT_THROW_COROUTINE(
        createGPUDriverSensor(mockManager, basicIntfName, objPath + "_noname"),
        sdbusplus::exception::SdBusError);
}

TEST_F(NsmGPUSWInventoryTestFixture, testDriverSensorUpdateResponseSizeError)
{
    std::string name = "GPU_Driver_BadSize";
    std::string type = "NSM_GPUSWInventory";
    std::string manufacturer = "NVIDIA";

    std::vector<utils::Association> associations;
    auto driverSensor =
        std::make_shared<NsmGPUSWInventoryDriverVersionAndStatus>(
            utils::DBusHandler::getBus(), name, associations, type,
            manufacturer);
    driverSensor->nsmDeviceFound = std::static_pointer_cast<NsmDevice>(gpu);

    // Mock response with incomplete data (header only, no driver info)
    Response badSizeResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(badSizeResp.data());

    // Use encode to create a proper minimal error response
    std::vector<uint8_t> driverInfoData;
    driverInfoData.push_back(DriverStateUnknown); // driver_state
    driverInfoData.push_back('\0');               // minimal version

    auto encodeRc = encode_get_driver_info_resp(0, NSM_ERROR, ERR_NULL,
                                                driverInfoData.size(),
                                                driverInfoData.data(), msg);
    EXPECT_EQ(encodeRc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(badSizeResp, Response{}));

    // May or may not call updateNsmDevice/refreshCapabilitySensor depending on
    // state change
    EXPECT_CALL(*gpu, updateNsmDevice())
        .Times(testing::AtMost(1))
        .WillRepeatedly(
            []() -> requester::Coroutine { co_return NSM_SUCCESS; });
    EXPECT_CALL(*gpu, refreshCapabilitySensor())
        .Times(testing::AtMost(1))
        .WillRepeatedly(
            []() -> requester::Coroutine { co_return NSM_SUCCESS; });

    auto coro = driverSensor->update(std::static_pointer_cast<NsmDevice>(gpu));
    // Wait for coroutine to complete
    while (!coro.done())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    // Should handle bad response size
}

TEST_F(NsmGPUSWInventoryTestFixture,
       testCreateGPUDriverSensorWithMultipleAssociations)
{
    dbus::PropertyMap properties = {
        {"Name", std::string("GPU_Driver_MultiAssoc")},
        {"UUID", gpuUuid},
        {"Manufacturer", std::string("NVIDIA")},
    };

    // Setup multiple associations
    auto& serviceMap = utils::MockDbusAsync::serviceMap();
    MapperServiceMap assocServiceMap = {
        {
            {
                "xyz.openbmc_project.NSM",
                {
                    basicIntfName + ".Associations",
                    basicIntfName + ".Associations1",
                    basicIntfName + ".Associations2",
                },
            },
        },
    };
    serviceMap = assocServiceMap;

    dbus::PropertyMap assocProperties0 = {
        {"Forward", "parent_gpu"},
        {"Backward", "driver"},
        {"AbsolutePath", "/xyz/openbmc_project/inventory/system/gpu0"},
    };
    dbus::PropertyMap assocProperties1 = {
        {"Forward", "parent_chassis"},
        {"Backward", "software"},
        {"AbsolutePath", "/xyz/openbmc_project/inventory/system/chassis"},
    };
    dbus::PropertyMap assocProperties2 = {
        {"Forward", "parent_system"},
        {"Backward", "driver_software"},
        {"AbsolutePath", "/xyz/openbmc_project/inventory/system"},
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath + "_multiassoc", basicIntfName);
    propertyMap = properties;

    auto& assocPropertyMap0 = utils::MockDbusAsync::propertyMap(
        objPath + "_multiassoc", basicIntfName + ".Associations");
    assocPropertyMap0 = assocProperties0;

    auto& assocPropertyMap1 = utils::MockDbusAsync::propertyMap(
        objPath + "_multiassoc", basicIntfName + ".Associations1");
    assocPropertyMap1 = assocProperties1;

    auto& assocPropertyMap2 = utils::MockDbusAsync::propertyMap(
        objPath + "_multiassoc", basicIntfName + ".Associations2");
    assocPropertyMap2 = assocProperties2;

    createGPUDriverSensor(mockManager, basicIntfName, objPath + "_multiassoc");

    EXPECT_GE(gpu->deviceSensors.size(), 1);
}

// update(): sensorIO returns NSM_ERROR (transport failure) →
// if (rc) at line 103 taken → co_return rc at line 106.
TEST_F(NsmGPUSWInventoryTestFixture, testDriverSensorUpdate_SensorIOError)
{
    std::string name = "GPU_Driver_SIOErr";
    std::vector<utils::Association> associations;
    auto driverSensor =
        std::make_shared<NsmGPUSWInventoryDriverVersionAndStatus>(
            utils::DBusHandler::getBus(), name, associations,
            "NSM_GPUSWInventory", "NVIDIA");
    driverSensor->nsmDeviceFound = std::static_pointer_cast<NsmDevice>(gpu);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(NSM_ERROR));
    driverSensor->update(std::static_pointer_cast<NsmDevice>(gpu));
}

// createGPUDriverSensor: "Manufacturer" key absent in property map →
// manufacturer="" → sensor still created (covers false branch of
// if (count("Manufacturer")) at line ~162 in GPUSWInventory.cpp).
TEST_F(NsmGPUSWInventoryTestFixture,
       testCreateGPUDriverSensorMissingManufacturer)
{
    dbus::PropertyMap properties = {
        {"Name", std::string("GPU_Driver_NoMfr")}, {"UUID", gpuUuid},
        // "Manufacturer" intentionally absent → manufacturer=""
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath + "_nomfr",
                                                          basicIntfName);
    propertyMap = properties;

    const size_t before = gpu->deviceSensors.size();
    createGPUDriverSensor(mockManager, basicIntfName, objPath + "_nomfr");
    // Sensor is still created with empty manufacturer
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// createGPUDriverSensor: UUID present and valid format but matches no
// NsmDevice → nsmDevice==nullptr → logs error and co_returns NSM_ERROR //
// (covers the !nsmDevice error path in GPUSWInventory.cpp
// factory)
TEST_F(NsmGPUSWInventoryTestFixture, badTestCreateGPUDriverSensorNoDevice)
{
    // UUID is valid format but doesn't match any registered NsmDevice
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
    // Should not throw – logs error and returns NSM_ERROR
    createGPUDriverSensor(mockManager, basicIntfName, testPath);
    // No sensor added since device was not found
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// GPUSWInventory.cpp L121 second operand of ||:
// rc==NSM_SW_SUCCESS but cc==NSM_ERROR.
// 9-byte buffer: decode_get_driver_info_resp calls decode_reason_code_and_cc
// which returns NSM_SW_SUCCESS with cc=NSM_ERROR → second operand TRUE.
TEST_F(NsmGPUSWInventoryTestFixture,
       testDriverSensorUpdateDecodeSuccessNonZeroCC)
{
    std::string name = "GPU_Driver_CC";
    std::string type = "NSM_GPUSWInventory";
    std::string manufacturer = "NVIDIA";

    std::vector<utils::Association> associations;
    auto driverSensor =
        std::make_shared<NsmGPUSWInventoryDriverVersionAndStatus>(
            utils::DBusHandler::getBus(), name, associations, type,
            manufacturer);
    driverSensor->nsmDeviceFound = std::static_pointer_cast<NsmDevice>(gpu);

    // 9-byte buffer with completion_code=NSM_ERROR at payload[1].
    // decode_reason_code_and_cc returns NSM_SW_SUCCESS with cc=NSM_ERROR.
    std::vector<uint8_t> ccErrResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    ccErrResp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(ccErrResp, Response{}));

    EXPECT_CALL(*gpu, updateNsmDevice())
        .Times(testing::AtMost(1))
        .WillRepeatedly(
            []() -> requester::Coroutine { co_return NSM_SUCCESS; });
    EXPECT_CALL(*gpu, refreshCapabilitySensor())
        .Times(testing::AtMost(1))
        .WillRepeatedly(
            []() -> requester::Coroutine { co_return NSM_SUCCESS; });

    driverSensor->update(std::static_pointer_cast<NsmDevice>(gpu));
}

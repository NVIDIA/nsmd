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
using ::testing::ElementsAre;
using namespace ::testing;

#include "base.h"
#include "platform-environmental.h"

#include "nsmAssetIntf.hpp"

#include <sdbusplus/bus.hpp>

#define private public
#define protected public

#include "NVLinkManagementNICSWInventory.hpp"

static auto& bus = utils::DBusHandler::getBus();
static const std::string sensorName("dummy_sensor");
static const std::string sensorType("dummy_type");
static const std::string manufacturer("dummy_manufacturer");
static const std::vector<utils::Association>
    associations({{"chassis", "all_sensors",
                   "/xyz/openbmc_project/inventory/dummy_device"}});

TEST(NsmSWInventoryDriverVersionAndStatus, GoodTest)
{
    nsm::NsmSWInventoryDriverVersionAndStatus FWSensor(
        bus, sensorName, associations, sensorType, manufacturer);

    EXPECT_EQ(FWSensor.name, sensorName);
    EXPECT_EQ(FWSensor.type, sensorType);

    enum8 driverState = 2;
    std::string version = "Mock";
    FWSensor.updateValue(driverState, version);
    EXPECT_EQ(FWSensor.softwareVer_->version(), version);
}

TEST(NsmSWInventoryDriverVersionAndStatus, HandleResponseMsgSuccess)
{
    auto bus = sdbusplus::bus::new_default();
    nsm::NsmSWInventoryDriverVersionAndStatus sensor(
        bus, sensorName, associations, sensorType, manufacturer);

    std::vector<uint8_t> responseMsg{
        0x10,
        0xDE,                            // PCI VID: NVIDIA 0x10DE
        0x00,                            // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
        0x89,                            // OCP_TYPE=8, OCP_VER=9
        NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
        NSM_GET_DRIVER_INFO,             // command
        0,                               // completion code
        0,
        0,
        6,
        0, // data size
        2,
        'M',
        'o',
        'c',
        'k',
        '\0'};

    uint8_t result = sensor.handleResponseMsg(
        reinterpret_cast<nsm_msg*>(responseMsg.data()), responseMsg.size());

    EXPECT_EQ(result, NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.driverState_, 2);
    EXPECT_EQ(sensor.driverVersion_, "Mock");
}

TEST(NsmSWInventoryDriverVersionAndStatus, HandleNullResponseMsg)
{
    nsm::NsmSWInventoryDriverVersionAndStatus sensor(
        bus, sensorName, associations, sensorType, manufacturer);

    uint8_t result = sensor.handleResponseMsg(nullptr, 0);

    EXPECT_EQ(result, NSM_SW_ERROR);
}

TEST(NsmSWInventoryDriverVersionAndStatus, NonNullTerminatedDriverVersion)
{
    nsm::NsmSWInventoryDriverVersionAndStatus sensor(
        bus, sensorName, associations, sensorType, manufacturer);

    std::vector<uint8_t> responseMsg{
        0x10,
        0xDE,                            // PCI VID: NVIDIA 0x10DE
        0x00,                            // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
        0x89,                            // OCP_TYPE=8, OCP_VER=9
        NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
        NSM_GET_DRIVER_INFO,             // command
        0,                               // completion code
        0,
        0,
        6,
        0, // data size
        2,
        'M',
        'o',
        'c',
        'k',
        '!'};

    uint8_t result = sensor.handleResponseMsg(
        reinterpret_cast<nsm_msg*>(responseMsg.data()), responseMsg.size());

    EXPECT_EQ(result, NSM_SW_ERROR_LENGTH);
}

TEST(NsmSWInventoryDriverVersionAndStatus, ExceedinglyLongDriverVersion)
{
    nsm::NsmSWInventoryDriverVersionAndStatus sensor(
        bus, sensorName, associations, sensorType, manufacturer);

    // Initialize a response message vector with enough space for headers
    // and a too-long driver version string
    std::vector<uint8_t> responseMsg{
        0x10,
        0xDE,                            // PCI VID: NVIDIA 0x10DE
        0x00,                            // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
        0x89,                            // OCP_TYPE=8, OCP_VER=9
        NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
        NSM_GET_DRIVER_INFO,             // command
        0,                               // completion code
        0,
        0,
        110,
        0};

    responseMsg.push_back(2); // Driver state

    // Generate a driver version string that is MAX_VERSION_STRING_SIZE + 10
    // character too long
    for (int i = 0; i <= MAX_VERSION_STRING_SIZE; ++i)
    {
        responseMsg.push_back('A'); // Filling the buffer with 'A's
    }

    uint8_t result = sensor.handleResponseMsg(
        reinterpret_cast<nsm_msg*>(responseMsg.data()), responseMsg.size());

    EXPECT_EQ(result, NSM_SW_ERROR_LENGTH);
}

// ============================================================================
// addSensor<T> instantiation coverage
// ============================================================================

struct NsmSWInventoryAddSensorTest :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;

    NsmSWInventoryAddSensorTest() : SensorManagerTest(devices) {}

    ~NsmSWInventoryAddSensorTest()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<MockNsmDevice> makeDevice()
    {
        return std::make_shared<MockNsmDevice>(NSM_DEV_ID_PCIE_BRIDGE, 0,
                                               "MCTP_EID", "12", 0);
    }
};

TEST_F(NsmSWInventoryAddSensorTest,
       AddSensorNsmSWInventoryDriverVersionAndStatus)
{
    auto device = makeDevice();
    auto sensor = std::make_shared<nsm::NsmSWInventoryDriverVersionAndStatus>(
        bus, sensorName, associations, sensorType, manufacturer);
    size_t before = device->deviceSensors.size();
    device->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(device->deviceSensors.size(), before);
}

// Error-CC path: decode_get_driver_info_resp succeeds (rc=NSM_SW_SUCCESS)
// but cc=NSM_ERROR from the device → if (rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS)
// is false → updateValue NOT called → return cc ? cc : rc; → cc branch taken.
TEST(NsmSWInventoryDriverVersionAndStatus,
     HandleResponseMsg_ErrorCC_ReturnsError)
{
    nsm::NsmSWInventoryDriverVersionAndStatus sensor(
        bus, sensorName, associations, sensorType, manufacturer);

    // Same layout as HandleResponseMsgSuccess but with NSM_ERROR CC.
    std::vector<uint8_t> responseMsg{
        0x10,
        0xDE,                            // PCI VID: NVIDIA 0x10DE
        0x00,                            // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
        0x89,                            // OCP_TYPE=8, OCP_VER=9
        NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
        NSM_GET_DRIVER_INFO,             // command
        NSM_ERROR,                       // completion code = NSM_ERROR
        0,
        0,
        6,
        0, // data size
        2,
        'M',
        'o',
        'c',
        'k',
        '\0'};

    uint8_t result = sensor.handleResponseMsg(
        reinterpret_cast<nsm_msg*>(responseMsg.data()), responseMsg.size());

    EXPECT_NE(result, NSM_SW_SUCCESS);
}

// updateValue: driverState != 2 → default branch → functional(false)
TEST(NsmSWInventoryDriverVersionAndStatus,
     UpdateValue_NonRunningState_FunctionalFalse)
{
    nsm::NsmSWInventoryDriverVersionAndStatus sensor(
        bus, sensorName, associations, sensorType, manufacturer);

    sensor.updateValue(0, "v1.0"); // state 0 → default → functional(false)
    EXPECT_FALSE(sensor.operationalStatus_->functional());
    EXPECT_EQ(sensor.driverState_, 0);
    EXPECT_EQ(sensor.driverVersion_, "v1.0");
}

// genRequestMsg: encode succeeds → returns valid request
TEST(NsmSWInventoryDriverVersionAndStatus, GenRequestMsg_Success_ReturnsRequest)
{
    nsm::NsmSWInventoryDriverVersionAndStatus sensor(
        bus, sensorName, associations, sensorType, manufacturer);

    auto request = sensor.genRequestMsg(0, 0);
    EXPECT_TRUE(request.has_value());
    EXPECT_FALSE(request->empty());
}

TEST(NsmSWInventoryDriverVersionAndStatus,
     BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    nsm::NsmSWInventoryDriverVersionAndStatus sensor(
        bus, sensorName, associations, sensorType, manufacturer);
    auto request = sensor.genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// Constructor: non-empty associations → loop body runs
TEST(NsmSWInventoryDriverVersionAndStatus,
     Constructor_WithAssociations_AssociationListSet)
{
    nsm::NsmSWInventoryDriverVersionAndStatus sensor(
        bus, "sensor2", associations, sensorType, manufacturer);
    EXPECT_NE(sensor.associationDef_, nullptr);
    EXPECT_EQ(sensor.asset_->manufacturer(), manufacturer);
}

// =============================================================================
// createNsmNVLinkManagerDriverSensor factory branch coverage
// Note: the factory function has internal (static) linkage, so it must be
// exercised via NsmObjectFactory::instance().createObjects().
// =============================================================================

#include "nsmObjectFactory.hpp"

using namespace nsm;

struct NsmNVLinkManagerSWInventoryFactoryTest :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_NVLinkManagementSWInventory";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmNVLinkManagerSWInventoryFactoryTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmNVLinkManagerSWInventoryFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }

    dbus::PropertyMap validProps = {
        {"Name", std::string("NVLinkMgr0")},
        {"Priority", bool(true)},
        {"UUID", gpuUuid},
        {"Manufacturer", std::string("NVIDIA")},
    };
};

// All properties present → sensor created (TRUE branches for all count()
// checks)
TEST_F(NsmNVLinkManagerSWInventoryFactoryTest, Factory_AllProperties_Success)
{
    const std::string testPath = "/xyz/test/nvlink/all_props";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm = validProps;

    const size_t before = gpu->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, testPath);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// Missing "Name" → FALSE branch for count("Name") → name="" →
// NsmSWInventoryDriverVersionAndStatus constructor throws SdBusError
// (invalid D-Bus path) → caught by createObjects → no sensor
TEST_F(NsmNVLinkManagerSWInventoryFactoryTest, Factory_MissingName_NoSensor)
{
    const std::string testPath = "/xyz/test/nvlink/no_name";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm = validProps;
    pm.erase("Name");

    const size_t before = gpu->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, testPath);
    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// Missing "Priority" → FALSE branch for count("Priority") → priority=false →
// sensor created with priority=false
TEST_F(NsmNVLinkManagerSWInventoryFactoryTest,
       Factory_MissingPriority_SensorCreated)
{
    const std::string testPath = "/xyz/test/nvlink/no_priority";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm = validProps;
    pm["Name"] = std::string("NVLinkMgr_NoPriority"); // unique name
    pm.erase("Priority");

    const size_t before = gpu->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, testPath);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// Missing "Manufacturer" → FALSE branch for count("Manufacturer") →
// manufacturer="" → sensor created
TEST_F(NsmNVLinkManagerSWInventoryFactoryTest,
       Factory_MissingManufacturer_SensorCreated)
{
    const std::string testPath = "/xyz/test/nvlink/no_mfr";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm = validProps;
    pm["Name"] = std::string("NVLinkMgr_NoMfr"); // unique name
    pm.erase("Manufacturer");

    const size_t before = gpu->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, testPath);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// Missing "UUID" → FALSE branch for count("UUID") → uuid="" →
// parseStaticUuid("") throws → caught by createObjects → no sensor
TEST_F(NsmNVLinkManagerSWInventoryFactoryTest, Factory_MissingUUID_NoSensor)
{
    const std::string testPath = "/xyz/test/nvlink/no_uuid";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm = validProps;
    pm.erase("UUID");

    const size_t before = gpu->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, intf, testPath);
    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// handleResponseMsg: rc==NSM_SW_SUCCESS, cc==NSM_ERROR →
// `if (rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS)` false branch →
// softwareVer_ NOT updated.
TEST(NsmSWInventoryDriverVersionAndStatus,
     HandleResponseMsg_DecodeSuccessNonZeroCC_SkipsUpdate)
{
    nsm::NsmSWInventoryDriverVersionAndStatus sensor(
        bus, sensorName, associations, sensorType, manufacturer);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

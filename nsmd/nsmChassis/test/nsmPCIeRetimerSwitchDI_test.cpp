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

#define private public
#define protected public

#include "base.h"

#include "nsmDeviceInventory/nsmPCIeRetimerSwitchDI.hpp"

#undef private
#undef protected

using namespace nsm;

// ============================================================================
// SECTION 3: nsmPCIeRetimerSwitchDI.cpp -- Uncovered function tests
// ============================================================================

struct NsmPCIeRetimerSwitchDIBatch11F :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "PCIeRetimerDI_B11F";
    const std::string type = "NSM_PCIeRetimer_Switch";
    const uuid_t deviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:54";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPCIeRetimerSwitchDIBatch11F() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPCIeRetimerSwitchDIBatch11F()
    {
        cleanupDeviceSensors(devices);
    }
};

// -- NsmPCIeRetimerSwitchDI basic constructor (non-multiport)
TEST_F(NsmPCIeRetimerSwitchDIBatch11F,
       Constructor_NonMultiport_SetsDeviceIndexAndPCIeType)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> associations;
    associations.push_back(
        {"chassis", "contained_by",
         "/xyz/openbmc_project/inventory/system/chassis/HGX"});
    std::string invPath = "/xyz/openbmc_project/inventory/batch11f/retimer/";
    uint8_t deviceIdx = 5;

    // Act
    auto retimerDI = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, name, associations, type, invPath, deviceIdx);

    // Assert
    EXPECT_NE(retimerDI, nullptr);
    EXPECT_EQ(retimerDI->getName(), name);
    EXPECT_EQ(retimerDI->getType(), type);
    EXPECT_EQ(retimerDI->deviceIndex, deviceIdx);
    EXPECT_FALSE(retimerDI->isMultiPciePortEnabled);
    EXPECT_NE(retimerDI->switchIntf, nullptr);
    EXPECT_NE(retimerDI->associationDefIntf, nullptr);
}

// -- NsmPCIeRetimerSwitchDI multiport constructor
TEST_F(NsmPCIeRetimerSwitchDIBatch11F,
       Constructor_Multiport_SetsPortVariablesAndEnabled)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> associations;
    std::string invPath = "/xyz/openbmc_project/inventory/batch11f/retimer_mp/";
    uint8_t deviceIdx = 3;
    uint8_t portType = 1;
    uint8_t portIndex = 2;
    uint8_t upstreamPort = 4;

    // Act
    auto retimerDI = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, "MultiPortSwitch_B11F", associations, type, invPath, deviceIdx,
        portType, portIndex, upstreamPort);

    // Assert
    EXPECT_NE(retimerDI, nullptr);
    EXPECT_TRUE(retimerDI->isMultiPciePortEnabled);
    EXPECT_EQ(retimerDI->multiPortType, portType);
    EXPECT_EQ(retimerDI->multiPortIndex, portIndex);
    EXPECT_EQ(retimerDI->multiPortUpstreamPort, upstreamPort);
    EXPECT_EQ(retimerDI->deviceIndex, deviceIdx);
}

// -- NsmPCIeRetimerSwitchDI::update (non-multiport, successful response)
TEST_F(NsmPCIeRetimerSwitchDIBatch11F,
       Update_NonMultiport_SuccessResponse_UpdatesDeviceAndVendorId)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> associations;
    std::string invPath = "/xyz/openbmc_project/inventory/batch11f/rt_up/";
    uint8_t deviceIdx = 2;
    auto retimerDI = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, "RetimerUpdate_B11F", associations, type, invPath, deviceIdx);

    // Build response with group0 telemetry data
    struct nsm_query_scalar_group_telemetry_group_0 groupData = {};
    groupData.pci_device_id = 0x1234;
    groupData.pci_vendor_id = 0x10DE;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group0_resp(
        0, NSM_SUCCESS, ERR_NULL, &groupData, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(response));

    // Act
    retimerDI->update(gpu);

    // Assert
    EXPECT_EQ(retimerDI->switchIntf->deviceId(), "0x1234");
    EXPECT_EQ(retimerDI->switchIntf->vendorId(), "0x10de");
}

// -- NsmPCIeRetimerSwitchDI::update (non-multiport, error response)
TEST_F(NsmPCIeRetimerSwitchDIBatch11F,
       Update_NonMultiport_ErrorCC_DoesNotUpdateIds)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> associations;
    std::string invPath = "/xyz/openbmc_project/inventory/batch11f/rt_err/";
    uint8_t deviceIdx = 3;
    auto retimerDI = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, "RetimerErr_B11F", associations, type, invPath, deviceIdx);

    // Build error response
    struct nsm_query_scalar_group_telemetry_group_0 groupData = {};
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group0_resp(
        0, NSM_ERROR, ERR_NULL, &groupData, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(response));

    // Act
    retimerDI->update(gpu);

    // Assert - IDs should remain empty (default)
    EXPECT_EQ(retimerDI->switchIntf->deviceId(), "");
    EXPECT_EQ(retimerDI->switchIntf->vendorId(), "");
}

// -- NsmPCIeRetimerSwitchDI::update with sensorIO failure
TEST_F(NsmPCIeRetimerSwitchDIBatch11F, Update_SensorIOFailure_ReturnsEarly)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> associations;
    std::string invPath = "/xyz/openbmc_project/inventory/batch11f/rt_iofail/";
    uint8_t deviceIdx = 4;
    auto retimerDI = std::make_shared<NsmPCIeRetimerSwitchDI>(
        bus, "RetimerIOFail_B11F", associations, type, invPath, deviceIdx);

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(
            mockSensorIO(static_cast<nsm_completion_codes>(NSM_SW_ERROR)));

    // Act
    retimerDI->update(gpu);

    // Assert - IDs should remain empty
    EXPECT_EQ(retimerDI->switchIntf->deviceId(), "");
}

// ============================================================================
// addSensor<T> instantiation coverage (nsmDevice.hpp)
// ============================================================================

TEST_F(NsmPCIeRetimerSwitchDIBatch11F, AddSensorNsmPCIeRetimerSwitchDI)
{
    auto& dbus = utils::DBusHandler::getBus();
    std::vector<utils::Association> associations;
    std::string invPath = "/xyz/openbmc_project/inventory/batch11f/rt_as/";
    auto sensor = std::make_shared<NsmPCIeRetimerSwitchDI>(
        dbus, "RetimerDI_AS", associations, type, invPath, uint8_t(0));
    size_t before = gpu->deviceSensors.size();
    gpu->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

TEST_F(NsmPCIeRetimerSwitchDIBatch11F,
       AddSensorNsmPCIeRetimerSwitchGetClockState)
{
    auto& dbus = utils::DBusHandler::getBus();
    std::string invPath =
        "/xyz/openbmc_project/inventory/batch11f/rt_clock_as/";
    auto sensor = std::make_shared<NsmPCIeRetimerSwitchGetClockState>(
        dbus, "RetimerClock_AS", type, uint64_t(0), invPath);
    size_t before = gpu->deviceSensors.size();
    gpu->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

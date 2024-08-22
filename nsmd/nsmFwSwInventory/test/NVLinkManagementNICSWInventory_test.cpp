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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
using ::testing::ElementsAre;

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

    EXPECT_EQ(result, NSM_SW_ERROR_COMMAND_FAIL);
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

    EXPECT_EQ(result, NSM_SW_ERROR_COMMAND_FAIL);
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

    EXPECT_EQ(result, NSM_SW_ERROR_COMMAND_FAIL);
}

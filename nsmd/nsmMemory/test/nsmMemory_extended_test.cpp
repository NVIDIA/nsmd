/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION &
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

using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Return;

#include "base.h"
#include "platform-environmental.h"

#include <sdbusplus/bus.hpp>

#define private public
#define protected public

#include "../../test/commonMock.hpp"
#include "../nsmMemory.hpp"

#undef private
#undef protected

using namespace nsm;

auto bus = sdbusplus::bus::new_default();
std::string sensorName("test_memory_sensor");
std::string sensorType("test_memory_type");
std::string inventoryObjPath("/xyz/openbmc_project/inventory/test_memory");

// ============================================================================
// NsmRemappingAvailabilityBankCount Tests
// ============================================================================

TEST(NsmRemappingAvailabilityBankCount, Constructor_ValidParameters)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());

    NsmRemappingAvailabilityBankCount sensor(sensorName, sensorType,
                                             rowRemapIntf, inventoryObjPath);

    EXPECT_EQ(sensor.getName(), sensorName);
    EXPECT_EQ(sensor.getType(), sensorType);
    EXPECT_NE(sensor.rowRemapIntf, nullptr);
}

TEST(NsmRemappingAvailabilityBankCount, GenRequestMsg_ValidEid)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());

    NsmRemappingAvailabilityBankCount sensor(sensorName, sensorType,
                                             rowRemapIntf, inventoryObjPath);

    const uint8_t eid = 10;
    const uint8_t instanceId = 5;

    auto request = sensor.genRequestMsg(eid, instanceId);
    ASSERT_TRUE(request.has_value());

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    EXPECT_EQ(msg->hdr.instance_id, instanceId);
    EXPECT_EQ(msg->hdr.nvidia_msg_type, NSM_TYPE_PLATFORM_ENVIRONMENTAL);

    auto command = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_GET_ROW_REMAP_AVAILABILITY);
}

TEST(NsmRemappingAvailabilityBankCount, HandleResponseMsg_Success)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());

    NsmRemappingAvailabilityBankCount sensor(sensorName, sensorType,
                                             rowRemapIntf, inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) +
            sizeof(struct nsm_get_row_remap_availability_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint16_t reasonCode = ERR_NULL;
    struct nsm_row_remap_availability availability{};
    availability.no_remapping = 100;
    availability.low_remapping = 200;
    availability.partial_remapping = 300;
    availability.high_remapping = 400;
    availability.max_remapping = 500;

    uint8_t rc = encode_get_row_remap_availability_resp(
        0, NSM_SUCCESS, reasonCode, &availability, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msgLen = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msgLen);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(NsmRemappingAvailabilityBankCount, HandleResponseMsg_NullPointer)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());

    NsmRemappingAvailabilityBankCount sensor(sensorName, sensorType,
                                             rowRemapIntf, inventoryObjPath);

    uint8_t rc = sensor.handleResponseMsg(nullptr, 100);
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(NsmRemappingAvailabilityBankCount, HandleResponseMsg_InvalidLength)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());

    NsmRemappingAvailabilityBankCount sensor(sensorName, sensorType,
                                             rowRemapIntf, inventoryObjPath);

    std::vector<uint8_t> responseMsg(10, 0); // Too small
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ============================================================================
// NsmEccErrorCountsDram Tests
// ============================================================================

TEST(NsmEccErrorCountsDram, Constructor_ValidParameters)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(bus,
                                                     inventoryObjPath.c_str());

    NsmEccErrorCountsDram sensor(sensorName, sensorType, eccIntf,
                                 inventoryObjPath);

    EXPECT_EQ(sensor.getName(), sensorName);
    EXPECT_EQ(sensor.getType(), sensorType);
    EXPECT_NE(sensor.eccIntf, nullptr);
}

TEST(NsmEccErrorCountsDram, GenRequestMsg_ValidEid)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(bus,
                                                     inventoryObjPath.c_str());

    NsmEccErrorCountsDram sensor(sensorName, sensorType, eccIntf,
                                 inventoryObjPath);

    const uint8_t eid = 10;
    const uint8_t instanceId = 7;

    auto request = sensor.genRequestMsg(eid, instanceId);
    ASSERT_TRUE(request.has_value());

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    EXPECT_EQ(msg->hdr.instance_id, instanceId);
    EXPECT_EQ(msg->hdr.nvidia_msg_type, NSM_TYPE_PLATFORM_ENVIRONMENTAL);

    auto command = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_GET_ECC_ERROR_COUNTS);
}

TEST(NsmEccErrorCountsDram, HandleResponseMsg_Success)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(bus,
                                                     inventoryObjPath.c_str());

    NsmEccErrorCountsDram sensor(sensorName, sensorType, eccIntf,
                                 inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_ECC_error_counts_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint16_t reasonCode = ERR_NULL;
    struct nsm_ECC_error_counts counts{};
    counts.flags.byte = 0;
    counts.sram_corrected = 10;
    counts.sram_uncorrected_secded = 2;
    counts.sram_uncorrected_parity = 1;
    counts.dram_corrected = 100;
    counts.dram_uncorrected = 5;

    uint8_t rc = encode_get_ECC_error_counts_resp(0, NSM_SUCCESS, reasonCode,
                                                  &counts, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msgLen = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msgLen);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(NsmEccErrorCountsDram, HandleResponseMsg_NullPointer)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(bus,
                                                     inventoryObjPath.c_str());

    NsmEccErrorCountsDram sensor(sensorName, sensorType, eccIntf,
                                 inventoryObjPath);

    uint8_t rc = sensor.handleResponseMsg(nullptr, 100);
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(NsmEccErrorCountsDram, HandleResponseMsg_InvalidLength)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(bus,
                                                     inventoryObjPath.c_str());

    NsmEccErrorCountsDram sensor(sensorName, sensorType, eccIntf,
                                 inventoryObjPath);

    // Create a buffer with header but incomplete payload
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_common_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ============================================================================
// NsmMemCurrClockFreq Tests
// ============================================================================

TEST(NsmMemCurrClockFreq, Constructor_ValidParameters)
{
    auto dimmIntf = std::make_shared<DimmIntf>(bus, inventoryObjPath.c_str());

    NsmMemCurrClockFreq sensor(sensorName, sensorType, dimmIntf,
                               inventoryObjPath);

    EXPECT_EQ(sensor.getName(), sensorName);
    EXPECT_EQ(sensor.getType(), sensorType);
    EXPECT_NE(sensor.dimmIntf, nullptr);
}

TEST(NsmMemCurrClockFreq, GenRequestMsg_ValidEid)
{
    auto dimmIntf = std::make_shared<DimmIntf>(bus, inventoryObjPath.c_str());

    NsmMemCurrClockFreq sensor(sensorName, sensorType, dimmIntf,
                               inventoryObjPath);

    const uint8_t eid = 10;
    const uint8_t instanceId = 9;

    auto request = sensor.genRequestMsg(eid, instanceId);
    ASSERT_TRUE(request.has_value());

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    EXPECT_EQ(msg->hdr.instance_id, instanceId);
    EXPECT_EQ(msg->hdr.nvidia_msg_type, NSM_TYPE_PLATFORM_ENVIRONMENTAL);

    auto req =
        reinterpret_cast<const nsm_get_curr_clock_freq_req*>(msg->payload);
    EXPECT_EQ(req->clock_id, MEMORY_CLOCK);
}

TEST(NsmMemCurrClockFreq, HandleResponseMsg_Success)
{
    auto dimmIntf = std::make_shared<DimmIntf>(bus, inventoryObjPath.c_str());

    NsmMemCurrClockFreq sensor(sensorName, sensorType, dimmIntf,
                               inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_curr_clock_freq_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint16_t reasonCode = ERR_NULL;
    uint32_t clockFreq = 3200; // MHz

    uint8_t rc = encode_get_curr_clock_freq_resp(0, NSM_SUCCESS, reasonCode,
                                                 &clockFreq, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msgLen = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msgLen);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(NsmMemCurrClockFreq, HandleResponseMsg_NullPointer)
{
    auto dimmIntf = std::make_shared<DimmIntf>(bus, inventoryObjPath.c_str());

    NsmMemCurrClockFreq sensor(sensorName, sensorType, dimmIntf,
                               inventoryObjPath);

    uint8_t rc = sensor.handleResponseMsg(nullptr, 100);
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(NsmMemCurrClockFreq, HandleResponseMsg_ZeroLength)
{
    auto dimmIntf = std::make_shared<DimmIntf>(bus, inventoryObjPath.c_str());

    NsmMemCurrClockFreq sensor(sensorName, sensorType, dimmIntf,
                               inventoryObjPath);

    std::vector<uint8_t> responseMsg(100, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t rc = sensor.handleResponseMsg(response, 0);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ============================================================================
// NsmMemCapacity Tests
// ============================================================================

TEST(NsmMemCapacity, Constructor_ValidParameters)
{
    auto dimmIntf = std::make_shared<DimmIntf>(bus, inventoryObjPath.c_str());

    NsmMemCapacity sensor(sensorName, sensorType, dimmIntf);

    EXPECT_EQ(sensor.getName(), sensorName);
    EXPECT_EQ(sensor.getType(), sensorType);
    EXPECT_NE(sensor.dimmIntf, nullptr);
}

// ============================================================================
// NsmMemoryHealth Tests
// ============================================================================

TEST(NsmMemoryHealth, Constructor_ValidParameters)
{
    NsmMemoryHealth health(bus, sensorName, sensorType, inventoryObjPath);

    EXPECT_EQ(health.getName(), sensorName);
    EXPECT_EQ(health.getType(), sensorType);
}

// ============================================================================
// NsmLocationIntfMemory Tests
// ============================================================================

TEST(NsmLocationIntfMemory, Constructor_ValidParameters)
{
    NsmLocationIntfMemory location(bus, sensorName, sensorType,
                                   inventoryObjPath);

    EXPECT_EQ(location.getName(), sensorName);
    EXPECT_EQ(location.getType(), sensorType);
}

// ============================================================================
// NsmMemoryAssociation Tests
// ============================================================================

TEST(NsmMemoryAssociation, Constructor_ValidParameters)
{
    std::vector<utils::Association> associations;
    associations.push_back(
        {"chassis", "contains", "/xyz/openbmc_project/inventory/chassis"});

    NsmMemoryAssociation assoc(bus, sensorName, sensorType, inventoryObjPath,
                               associations);

    EXPECT_EQ(assoc.getName(), sensorName);
    EXPECT_EQ(assoc.getType(), sensorType);
}

TEST(NsmMemoryAssociation, Constructor_EmptyAssociations)
{
    std::vector<utils::Association> emptyAssociations;

    NsmMemoryAssociation assoc(bus, sensorName, sensorType, inventoryObjPath,
                               emptyAssociations);

    EXPECT_EQ(assoc.getName(), sensorName);
}

// ============================================================================
// NsmMemoryErrorCorrection Tests
// ============================================================================

TEST(NsmMemoryErrorCorrection, Constructor_ValidParameters)
{
    auto dimmIntf = std::make_shared<DimmIntf>(bus, inventoryObjPath.c_str());
    std::string correctionType =
        "xyz.openbmc_project.Inventory.Item.Dimm.Ecc.NoECC";

    NsmMemoryErrorCorrection ecc(sensorName, sensorType, dimmIntf,
                                 correctionType, inventoryObjPath);

    EXPECT_EQ(ecc.getName(), sensorName);
    EXPECT_EQ(ecc.getType(), sensorType);
    EXPECT_NE(ecc.dimmIntf, nullptr);
}

// ============================================================================
// NsmMemoryDeviceType Tests
// ============================================================================

TEST(NsmMemoryDeviceType, Constructor_ValidParameters)
{
    auto dimmIntf = std::make_shared<DimmIntf>(bus, inventoryObjPath.c_str());
    std::string memoryType =
        "xyz.openbmc_project.Inventory.Item.Dimm.DeviceType.HBM2";

    NsmMemoryDeviceType deviceType(sensorName, sensorType, dimmIntf, memoryType,
                                   inventoryObjPath);

    EXPECT_EQ(deviceType.getName(), sensorName);
    EXPECT_EQ(deviceType.getType(), sensorType);
    EXPECT_NE(deviceType.dimmIntf, nullptr);
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

TEST(NsmRowRemappingCounts, HandleResponseMsg_ErrorStatus)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(bus, inventoryObjPath.c_str());

    NsmRowRemappingCounts sensor(sensorName, sensorType, rowRemapIntf,
                                 inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_row_remapping_counts_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint16_t reasonCode = ERR_NOT_SUPPORTED;
    uint32_t correctableError = 0;
    uint32_t uncorrectableError = 0;

    uint8_t rc = encode_get_row_remapping_counts_resp(
        0, NSM_ERROR, reasonCode, correctableError, uncorrectableError,
        response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msgLen = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msgLen);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(NsmMinMemoryClockLimit, Constructor_ValidParameters)
{
    auto dimmIntf = std::make_shared<DimmIntf>(bus, inventoryObjPath.c_str());

    NsmMinMemoryClockLimit minClock(sensorName, sensorType, dimmIntf);

    EXPECT_EQ(minClock.getName(), sensorName);
    EXPECT_EQ(minClock.getType(), sensorType);
    EXPECT_NE(minClock.dimmIntf, nullptr);
}

TEST(NsmMaxMemoryClockLimit, Constructor_ValidParameters)
{
    auto dimmIntf = std::make_shared<DimmIntf>(bus, inventoryObjPath.c_str());

    NsmMaxMemoryClockLimit maxClock(sensorName, sensorType, dimmIntf);

    EXPECT_EQ(maxClock.getName(), sensorName);
    EXPECT_EQ(maxClock.getType(), sensorType);
    EXPECT_NE(maxClock.dimmIntf, nullptr);
}

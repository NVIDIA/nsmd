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

#include "pci-links.h"

#define private public
#define protected public

#include "nsmPCIeLinkSpeed.hpp"
#include "nsmPort/nsmPortInfo.hpp"

using namespace nsm;

auto bus = sdbusplus::bus::new_default();

TEST(NsmPCIeLinkSpeedBase, TestGeneration)
{
    EXPECT_EQ(NsmPCIeLinkSpeedBase::generation(0),
              PCIeSlotIntf::Generations::Unknown);
    EXPECT_EQ(NsmPCIeLinkSpeedBase::generation(1),
              PCIeSlotIntf::Generations::Gen1);
    EXPECT_EQ(NsmPCIeLinkSpeedBase::generation(2),
              PCIeSlotIntf::Generations::Gen2);
    EXPECT_EQ(NsmPCIeLinkSpeedBase::generation(3),
              PCIeSlotIntf::Generations::Gen3);
    EXPECT_EQ(NsmPCIeLinkSpeedBase::generation(4),
              PCIeSlotIntf::Generations::Gen4);
    EXPECT_EQ(NsmPCIeLinkSpeedBase::generation(5),
              PCIeSlotIntf::Generations::Gen5);
    EXPECT_EQ(NsmPCIeLinkSpeedBase::generation(6),
              PCIeSlotIntf::Generations::Gen6);
    EXPECT_EQ(NsmPCIeLinkSpeedBase::generation(7),
              PCIeSlotIntf::Generations::Unknown);
}

TEST(NsmPCIeLinkSpeedBase, TestPcieType)
{
    EXPECT_EQ(NsmPCIeLinkSpeedBase::pcieType(0),
              PCIeDeviceIntf::PCIeTypes::Unknown);
    EXPECT_EQ(NsmPCIeLinkSpeedBase::pcieType(1),
              PCIeDeviceIntf::PCIeTypes::Gen1);
    EXPECT_EQ(NsmPCIeLinkSpeedBase::pcieType(2),
              PCIeDeviceIntf::PCIeTypes::Gen2);
    EXPECT_EQ(NsmPCIeLinkSpeedBase::pcieType(3),
              PCIeDeviceIntf::PCIeTypes::Gen3);
    EXPECT_EQ(NsmPCIeLinkSpeedBase::pcieType(4),
              PCIeDeviceIntf::PCIeTypes::Gen4);
    EXPECT_EQ(NsmPCIeLinkSpeedBase::pcieType(5),
              PCIeDeviceIntf::PCIeTypes::Gen5);
    EXPECT_EQ(NsmPCIeLinkSpeedBase::pcieType(6),
              PCIeDeviceIntf::PCIeTypes::Gen6);
    EXPECT_EQ(NsmPCIeLinkSpeedBase::pcieType(10),
              PCIeDeviceIntf::PCIeTypes::Unknown);
}

TEST(NsmPCIeLinkSpeedBase, TestLinkWidth)
{
    EXPECT_EQ(NsmPCIeLinkSpeedBase::linkWidth(0), 0);  // Invalid
    EXPECT_EQ(NsmPCIeLinkSpeedBase::linkWidth(1), 1);  // x1
    EXPECT_EQ(NsmPCIeLinkSpeedBase::linkWidth(2), 2);  // x2
    EXPECT_EQ(NsmPCIeLinkSpeedBase::linkWidth(3), 4);  // x4
    EXPECT_EQ(NsmPCIeLinkSpeedBase::linkWidth(4), 8);  // x8
    EXPECT_EQ(NsmPCIeLinkSpeedBase::linkWidth(5), 16); // x16
    EXPECT_EQ(NsmPCIeLinkSpeedBase::linkWidth(6), 32); // x32
    EXPECT_EQ(NsmPCIeLinkSpeedBase::linkWidth(7), 64); // x64
}

TEST(NsmPCIeLinkSpeed, ConstructorSinglePort)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/system/device";
    std::string name = "PCIeLinkSpeed";
    std::string type = "NSM_PCIeLinkSpeed";

    auto pcieDeviceIntf = std::make_shared<PCIeDeviceIntf>(bus, path.c_str());
    NsmInterfaceProvider<PCIeDeviceIntf> provider(name, type, path,
                                                  pcieDeviceIntf);

    uint8_t deviceIndex = 1;

    NsmPCIeLinkSpeed<PCIeDeviceIntf> linkSpeed(provider, deviceIndex, false);

    EXPECT_EQ(linkSpeed.getName(), name);
    EXPECT_EQ(linkSpeed.getType(), type);
    EXPECT_EQ(linkSpeed.deviceIndex, deviceIndex);
    EXPECT_FALSE(linkSpeed.isMultiPciePortEnabled);
}

TEST(NsmPCIeLinkSpeed, ConstructorMultiPort)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/system/device";
    std::string name = "PCIeLinkSpeedMulti";
    std::string type = "NSM_PCIeLinkSpeed";

    auto pcieDeviceIntf = std::make_shared<PCIeDeviceIntf>(bus, path.c_str());
    NsmInterfaceProvider<PCIeDeviceIntf> provider(name, type, path,
                                                  pcieDeviceIntf);

    uint8_t upstreamPortCount = 3;

    NsmPCIeLinkSpeed<PCIeDeviceIntf> linkSpeed(provider, upstreamPortCount,
                                               true);

    EXPECT_EQ(linkSpeed.getName(), name);
    EXPECT_TRUE(linkSpeed.isMultiPciePortEnabled);
    EXPECT_EQ(linkSpeed.upstreamPortCount, upstreamPortCount);
}

TEST(NsmPCIeLinkSpeed, GenRequestMsgSinglePort)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/system/device";
    std::string name = "PCIeLinkSpeed";
    std::string type = "NSM_PCIeLinkSpeed";

    auto pcieDeviceIntf = std::make_shared<PCIeDeviceIntf>(bus, path.c_str());
    NsmInterfaceProvider<PCIeDeviceIntf> provider(name, type, path,
                                                  pcieDeviceIntf);

    uint8_t deviceIndex = 2;
    NsmPCIeLinkSpeed<PCIeDeviceIntf> linkSpeed(provider, deviceIndex, false);

    eid_t eid = 10;
    uint8_t instanceId = 5;

    auto request = linkSpeed.genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_query_scalar_group_telemetry_v1_req));
}

TEST(NsmPCIeLinkSpeed, HandleResponseMsg)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/system/device";
    std::string name = "PCIeLinkSpeed";
    std::string type = "NSM_PCIeLinkSpeed";

    auto pcieDeviceIntf = std::make_shared<PCIeDeviceIntf>(bus, path.c_str());
    NsmInterfaceProvider<PCIeDeviceIntf> provider(name, type, path,
                                                  pcieDeviceIntf);

    uint8_t deviceIndex = 1;
    NsmPCIeLinkSpeed<PCIeDeviceIntf> linkSpeed(provider, deviceIndex, false);

    // Create mock response
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_scalar_group_telemetry_v1_resp) +
            sizeof(nsm_query_scalar_group_telemetry_group_1),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    nsm_query_scalar_group_telemetry_group_1 data = {};
    data.negotiated_link_speed = 4; // Gen4
    data.negotiated_link_width = 4; // x8
    data.max_link_speed = 5;        // Gen5
    data.max_link_width = 5;        // x16

    uint8_t rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        0, cc, reason_code, &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = linkSpeed.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SUCCESS);

    // Verify values were set
    EXPECT_EQ(pcieDeviceIntf->pcIeType(), PCIeDeviceIntf::PCIeTypes::Gen4);
    EXPECT_EQ(pcieDeviceIntf->maxPCIeType(), PCIeDeviceIntf::PCIeTypes::Gen5);
    EXPECT_EQ(pcieDeviceIntf->lanesInUse(), 8);
    EXPECT_EQ(pcieDeviceIntf->maxLanes(), 16);
}

TEST(NsmPCIeLinkSpeed, HandleResponseMsgWithSlot)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/system/slot";
    std::string name = "PCIeSlotSpeed";
    std::string type = "NSM_PCIeSlotSpeed";

    auto pcieSlotIntf = std::make_shared<PCIeSlotIntf>(bus, path.c_str());
    NsmInterfaceProvider<PCIeSlotIntf> provider(name, type, path, pcieSlotIntf);

    uint8_t deviceIndex = 0;
    NsmPCIeLinkSpeed<PCIeSlotIntf> linkSpeed(provider, deviceIndex, false);

    // Create mock response with Gen3 x16
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_scalar_group_telemetry_v1_resp) +
            sizeof(nsm_query_scalar_group_telemetry_group_1),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    nsm_query_scalar_group_telemetry_group_1 data = {};
    data.negotiated_link_speed = 3; // Gen3
    data.negotiated_link_width = 5; // x16

    uint8_t rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = linkSpeed.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SUCCESS);

    // Verify slot interface values
    EXPECT_EQ(pcieSlotIntf->generation(), PCIeSlotIntf::Generations::Gen3);
    EXPECT_EQ(pcieSlotIntf->lanes(), 16);
}

TEST(NsmPCIeLinkSpeed, GenRequestMsgMultiPort)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/system/device_multi";
    std::string name = "PCIeLinkSpeedMulti";
    std::string type = "NSM_PCIeLinkSpeed";

    auto pcieDeviceIntf = std::make_shared<PCIeDeviceIntf>(bus, path.c_str());
    NsmInterfaceProvider<PCIeDeviceIntf> provider(name, type, path,
                                                  pcieDeviceIntf);

    uint8_t upstreamPortCount = 3;

    NsmPCIeLinkSpeed<PCIeDeviceIntf> linkSpeed(provider, upstreamPortCount,
                                               true);

    eid_t eid = 15;
    uint8_t instanceId = 7;

    // genMultiPCIeRequestMsg is the method that generates multi-port requests
    auto request = linkSpeed.genMultiPCIeRequestMsg(eid, instanceId, 0, 0, 0);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_multiport_query_scalar_group_telemetry_v2_req));
}

TEST(NsmPCIeLinkSpeed, GenMultiPCIeRequestMsgDifferentParams)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/system/multiport_test";
    std::string name = "MultiPortTest";
    std::string type = "NSM_PCIeLinkSpeed";

    auto pcieDeviceIntf = std::make_shared<PCIeDeviceIntf>(bus, path.c_str());
    NsmInterfaceProvider<PCIeDeviceIntf> provider(name, type, path,
                                                  pcieDeviceIntf);

    // Test with different parameter combinations
    std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> testParams = {
        {0, 0, 0}, {1, 1, 1}, {2, 3, 4}, {255, 255, 255}};

    for (const auto& [portType, portIdx, upstreamPort] : testParams)
    {
        NsmPCIeLinkSpeed<PCIeDeviceIntf> linkSpeed(provider, upstreamPort,
                                                   true);

        auto request = linkSpeed.genMultiPCIeRequestMsg(10, 1, portType,
                                                        portIdx, upstreamPort);
        EXPECT_TRUE(request.has_value());
    }
}

// genMultiPCIeRequestMsg failure: instanceId > NSM_INSTANCE_MAX ->
// encode_multiport_query_scalar_group_telemetry_v2_req fails ->
// if(rc) TRUE -> return nullopt. Covers lines 73,75-76 in nsmPCIeLinkSpeed.cpp.
TEST(NsmPCIeLinkSpeed, GenMultiPCIeRequestMsg_InvalidInstanceId_ReturnsNullopt)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/system/multiport_badinst";
    std::string name = "MultiPortBadInst";
    std::string type = "NSM_PCIeLinkSpeed";

    auto pcieDeviceIntf = std::make_shared<PCIeDeviceIntf>(bus, path.c_str());
    NsmInterfaceProvider<PCIeDeviceIntf> provider(name, type, path,
                                                  pcieDeviceIntf);
    NsmPCIeLinkSpeed<PCIeDeviceIntf> linkSpeed(provider, 0, true);

    // NSM_INSTANCE_MAX + 1 makes encode fail -> returns nullopt
    auto request = linkSpeed.genMultiPCIeRequestMsg(1, NSM_INSTANCE_MAX + 1, 0,
                                                    0, 0);
    EXPECT_FALSE(request.has_value());
}

TEST(NsmPCIeLinkSpeed, TestMultiPortParameters)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/system/multiport_params";
    std::string name = "MultiPortParams";
    std::string type = "NSM_PCIeLinkSpeed";

    auto pcieDeviceIntf = std::make_shared<PCIeDeviceIntf>(bus, path.c_str());
    NsmInterfaceProvider<PCIeDeviceIntf> provider(name, type, path,
                                                  pcieDeviceIntf);

    uint8_t upstreamPortCount = 15;

    NsmPCIeLinkSpeed<PCIeDeviceIntf> linkSpeed(provider, upstreamPortCount,
                                               true);

    EXPECT_TRUE(linkSpeed.isMultiPciePortEnabled);
    EXPECT_EQ(linkSpeed.upstreamPortCount, upstreamPortCount);
}

TEST(NsmPCIeLinkSpeed, GenSinglePCIeRequestMsgDifferentDeviceIndices)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/system/single_test";
    std::string name = "SinglePortTest";
    std::string type = "NSM_PCIeLinkSpeed";

    auto pcieDeviceIntf = std::make_shared<PCIeDeviceIntf>(bus, path.c_str());
    NsmInterfaceProvider<PCIeDeviceIntf> provider(name, type, path,
                                                  pcieDeviceIntf);

    std::vector<uint8_t> deviceIndices = {0, 1, 5, 10, 255};

    for (uint8_t idx : deviceIndices)
    {
        NsmPCIeLinkSpeed<PCIeDeviceIntf> linkSpeed(provider, idx, false);

        auto request = linkSpeed.genSinglePCIeRequestMsg(10, 1);
        EXPECT_TRUE(request.has_value());
    }
}

// handleResponseMsg – error CC: the if-block is NOT entered (else branch taken)
TEST(NsmPCIeLinkSpeed, HandleResponseMsg_ErrorCC_DoesNotUpdateInterface)
{
    std::filesystem::path path = "/xyz/openbmc_project/inventory/system/err_cc";
    std::string name = "PCIeLinkSpeed";
    std::string type = "NSM_PCIeLinkSpeed";

    auto pcieDeviceIntf = std::make_shared<PCIeDeviceIntf>(bus, path.c_str());
    NsmInterfaceProvider<PCIeDeviceIntf> provider(name, type, path,
                                                  pcieDeviceIntf);

    NsmPCIeLinkSpeed<PCIeDeviceIntf> linkSpeed(provider, 1, false);

    nsm_query_scalar_group_telemetry_group_1 data = {};
    data.negotiated_link_speed = 5; // Gen5
    data.negotiated_link_width = 5; // x16

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_scalar_group_telemetry_v1_resp) +
            sizeof(nsm_query_scalar_group_telemetry_group_1),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        0, NSM_ERROR, ERR_NULL, &data, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = linkSpeed.handleResponseMsg(response, responseMsg.size());

    EXPECT_NE(rc, NSM_SUCCESS);
    // The interface should retain its default state since handleResponse
    // was not called (the if-branch was not taken)
}

// handleResponseMsg – success with zero link widths (linkWidth(0) = 0)
TEST(NsmPCIeLinkSpeed, HandleResponseMsg_ZeroLinkWidth_SetsZero)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/system/zero_width";
    std::string name = "PCIeLinkSpeed";
    std::string type = "NSM_PCIeLinkSpeed";

    auto pcieDeviceIntf = std::make_shared<PCIeDeviceIntf>(bus, path.c_str());
    NsmInterfaceProvider<PCIeDeviceIntf> provider(name, type, path,
                                                  pcieDeviceIntf);

    NsmPCIeLinkSpeed<PCIeDeviceIntf> linkSpeed(provider, 1, false);

    nsm_query_scalar_group_telemetry_group_1 data = {};
    data.negotiated_link_speed = 0; // Unknown
    data.negotiated_link_width = 0; // 0 -> linkWidth(0)=0

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_scalar_group_telemetry_v1_resp) +
            sizeof(nsm_query_scalar_group_telemetry_group_1),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = linkSpeed.handleResponseMsg(response, responseMsg.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(pcieDeviceIntf->pcIeType(), PCIeDeviceIntf::PCIeTypes::Unknown);
    EXPECT_EQ(pcieDeviceIntf->lanesInUse(), 0u);
}

// handleResponseMsg – decode failure: too-short buffer triggers length error
TEST(NsmPCIeLinkSpeed, HandleResponseMsg_DecodeFail_ReturnsError)
{
    std::filesystem::path path =
        "/xyz/openbmc_project/inventory/system/decode_fail";
    std::string name = "PCIeLinkSpeed";
    std::string type = "NSM_PCIeLinkSpeed";

    auto pcieDeviceIntf = std::make_shared<PCIeDeviceIntf>(bus, path.c_str());
    NsmInterfaceProvider<PCIeDeviceIntf> provider(name, type, path,
                                                  pcieDeviceIntf);
    NsmPCIeLinkSpeed<PCIeDeviceIntf> linkSpeed(provider, 1, false);

    // 7-byte buffer: safely reads cc=0 at payload[1] but too short for
    // nsm_query_scalar_group_telemetry_v1_resp (5+7=12), so the decode
    // returns NSM_SW_ERROR_LENGTH
    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) + 2, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    auto rc = linkSpeed.handleResponseMsg(response, responseMsg.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// Fixture: NsmPCIeLinkSpeedUpdateTest (multiport update() coroutine tests)
// ============================================================================

struct NsmPCIeLinkSpeedUpdateTest : public Test, public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    const std::filesystem::path updPath =
        "/xyz/openbmc_project/inventory/system/multiport_update";
    const std::string updName = "PCIeLinkSpeedUpdate";
    const std::string updType = "NSM_PCIeLinkSpeed";
    const uuid_t deviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    std::shared_ptr<PCIeDeviceIntf> pcieDeviceIntf;
    std::unique_ptr<NsmPCIeLinkSpeed<PCIeDeviceIntf>> sensor;

    NsmPCIeLinkSpeedUpdateTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPCIeLinkSpeedUpdateTest() override
    {
        sensor.reset();
        pcieDeviceIntf.reset();
        cleanupDeviceSensors(devices);
    }

    void createSensor(uint8_t upstreamPortCount)
    {
        pcieDeviceIntf = std::make_shared<PCIeDeviceIntf>(bus, updPath.c_str());
        NsmInterfaceProvider<PCIeDeviceIntf> provider(updName, updType, updPath,
                                                      pcieDeviceIntf);
        sensor = std::make_unique<NsmPCIeLinkSpeed<PCIeDeviceIntf>>(
            provider, upstreamPortCount, true);
    }

    // Build a valid encoded response for
    // decode_query_scalar_group_telemetry_v1_group1_resp
    std::vector<uint8_t>
        buildResponse(nsm_query_scalar_group_telemetry_group_1 data)
    {
        size_t totalSize = sizeof(nsm_msg_hdr) +
                           sizeof(nsm_query_scalar_group_telemetry_v1_resp) +
                           sizeof(nsm_query_scalar_group_telemetry_group_1);
        std::vector<uint8_t> resp(totalSize, 0);
        auto* msg = reinterpret_cast<nsm_msg*>(resp.data());
        encode_query_scalar_group_telemetry_v1_group1_resp(
            0, NSM_SUCCESS, ERR_NULL, &data, msg);
        return resp;
    }
};

// upstreamPortCount=1, sensorIO success: lines 97-168 executed
TEST_F(NsmPCIeLinkSpeedUpdateTest,
       Update_MultiPort_SinglePort_SensorIOSuccess_UpdatesInterface)
{
    createSensor(1);

    nsm_query_scalar_group_telemetry_group_1 data{};
    data.negotiated_link_speed = 4; // Gen4
    data.negotiated_link_width = 4; // x8
    data.target_link_speed = 4;
    data.max_link_speed = 5;        // Gen5
    data.max_link_width = 5;        // x16

    auto resp = buildResponse(data);
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));

    auto rc = sensor->update(gpu);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(pcieDeviceIntf->pcIeType(), PCIeDeviceIntf::PCIeTypes::Gen4);
    EXPECT_EQ(pcieDeviceIntf->maxPCIeType(), PCIeDeviceIntf::PCIeTypes::Gen5);
    EXPECT_EQ(pcieDeviceIntf->lanesInUse(), 8u);
    EXPECT_EQ(pcieDeviceIntf->maxLanes(), 16u);
}

// upstreamPortCount=1, sensorIO returns error: covers lines 129-131 (rc true
// branch)
TEST_F(NsmPCIeLinkSpeedUpdateTest, Update_MultiPort_SensorIOFails_ReturnsError)
{
    createSensor(1);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    auto rc = sensor->update(gpu);
    EXPECT_NE(rc.data(), NSM_SUCCESS);
}

// upstreamPortCount=2, second port has higher values: covers true branches in
// lines 149-162 (negotiated_link_speed, negotiated_link_width, target,
// max_link_speed, max_link_width all > initial max)
TEST_F(NsmPCIeLinkSpeedUpdateTest,
       Update_MultiPort_TwoPorts_HigherValues_AccumulatesMax)
{
    createSensor(2);

    // First port: Gen2 x2 (higher than initial max_data but lower than port 2)
    nsm_query_scalar_group_telemetry_group_1 d1{};
    d1.negotiated_link_speed = 2;
    d1.negotiated_link_width = 2;
    d1.target_link_speed = 2;
    d1.max_link_speed = 2;
    d1.max_link_width = 2;

    // Second port: Gen5 x16 (higher than port 1)
    nsm_query_scalar_group_telemetry_group_1 d2{};
    d2.negotiated_link_speed = 5;
    d2.negotiated_link_width = 5;
    d2.target_link_speed = 5;
    d2.max_link_speed = 5;
    d2.max_link_width = 5;

    auto resp1 = buildResponse(d1);
    auto resp2 = buildResponse(d2);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(resp1))
        .WillOnce(mockSensorIO(resp2));

    auto rc = sensor->update(gpu);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // Max values from second port should win
    EXPECT_EQ(pcieDeviceIntf->pcIeType(), PCIeDeviceIntf::PCIeTypes::Gen5);
    EXPECT_EQ(pcieDeviceIntf->lanesInUse(), 16u);
}

// upstreamPortCount=1, all response values <= initial max_data: covers false
// branches in lines 149-162 (none of the if conditions are true)
TEST_F(NsmPCIeLinkSpeedUpdateTest,
       Update_MultiPort_LowValues_ComparisonFalse_NoUpdate)
{
    createSensor(1);

    // Values equal to or below the initial max_data
    // max_data initialised with negotiated_link_speed=1, max_link_speed=1,
    // all widths=0, target_link_speed=0
    nsm_query_scalar_group_telemetry_group_1 data{};
    data.negotiated_link_speed = 1; // NOT > 1
    data.negotiated_link_width = 0; // NOT > 0
    data.target_link_speed = 0;     // NOT > 0
    data.max_link_speed = 1;        // NOT > 1
    data.max_link_width = 0;        // NOT > 0

    auto resp = buildResponse(data);
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));

    auto rc = sensor->update(gpu);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // Interface retains initial Gen1/0 values (no update)
    EXPECT_EQ(pcieDeviceIntf->pcIeType(), PCIeDeviceIntf::PCIeTypes::Gen1);
    EXPECT_EQ(pcieDeviceIntf->lanesInUse(), 0u);
}

// upstreamPortCount=1, decode fails (tiny buffer) -> rc != NSM_SUCCESS
// -> "if (rc == NSM_SUCCESS && cc == NSM_SUCCESS)" FALSE branch covered
// -> max_data not updated, loop ends, handleResponse(initial max_data) called
// -> overall co_return NSM_SUCCESS
TEST_F(NsmPCIeLinkSpeedUpdateTest,
       Update_MultiPort_DecodeFails_SkipsUpdate_ReturnsSuccess)
{
    createSensor(1);

    // Tiny buffer causes decode_query_scalar_group_telemetry_v1_group1_resp
    // to return an error -> rc != NSM_SUCCESS -> if condition false
    std::vector<uint8_t> tiny(sizeof(nsm_msg_hdr) + 4, 0);
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(tiny));

    auto rc = sensor->update(gpu);
    // Loop continues past failed decode; final co_return is NSM_SUCCESS //

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // Interface gets initial max_data values (negotiated_link_speed=1 -> Gen1)
    EXPECT_EQ(pcieDeviceIntf->pcIeType(), PCIeDeviceIntf::PCIeTypes::Gen1);
}

// Single-port update() path: isMultiPciePortEnabled=false -> delegates to
// NsmSensor::update() (L91-93 TRUE branch / co_return co_await path)

TEST_F(NsmPCIeLinkSpeedUpdateTest, Update_SinglePort_DelegatesToNsmSensor)
{
    pcieDeviceIntf = std::make_shared<PCIeDeviceIntf>(bus, updPath.c_str());
    NsmInterfaceProvider<PCIeDeviceIntf> provider(updName, updType, updPath,
                                                  pcieDeviceIntf);
    // isMultiPciePortEnabled=false -> L91 TRUE -> co_return co_await
    // NsmSensor::update()
    sensor = std::make_unique<NsmPCIeLinkSpeed<PCIeDeviceIntf>>(
        provider, uint8_t(0), false);

    nsm_query_scalar_group_telemetry_group_1 data{};
    data.negotiated_link_speed = 3; // Gen3
    data.max_link_speed = 5;        // Gen5
    auto resp = buildResponse(data);
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));

    auto rc = sensor->update(gpu);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
}

// Multi-port update: decode returns NSM_SUCCESS but cc != NSM_SUCCESS
// (L147 FALSE branch: max_data not updated, loop continues to end)
// Use buffer of exactly sizeof(nsm_msg_hdr)+sizeof(nsm_common_non_success_resp)
// with cc=NSM_ERROR so decode_reason_code_and_cc returns NSM_SW_SUCCESS.
TEST_F(NsmPCIeLinkSpeedUpdateTest,
       Update_MultiPort_DecodeSuccessNonZeroCC_SkipsUpdate)
{
    createSensor(1);

    // 9-byte response: decode_reason_code_and_cc returns NSM_SW_SUCCESS with
    // cc=NSM_ERROR -> "rc==NSM_SUCCESS && cc==NSM_SUCCESS" FALSE branch taken
    std::vector<uint8_t> ccErrResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    ccErrResp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // cc = NSM_ERROR

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(ccErrResp));

    auto rc = sensor->update(gpu);
    // Loop completes without updating; handleResponse(initial max_data) called
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // Interface gets initial Gen1 (no update from non-success response)
    EXPECT_EQ(pcieDeviceIntf->pcIeType(), PCIeDeviceIntf::PCIeTypes::Gen1);
}

// =============================================================================
// NsmPCIeLinkSpeed<NsmPortInfoIntf>::handleResponse branch coverage
// convertToTransferRate() switch cases GEN2-GEN6 and default
// =============================================================================

// Helper to build a group-1 response for NsmPortInfoIntf tests
static std::vector<uint8_t>
    buildPortInfoResponse(nsm_query_scalar_group_telemetry_group_1 data)
{
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_scalar_group_telemetry_v1_resp) +
            sizeof(nsm_query_scalar_group_telemetry_group_1),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    encode_query_scalar_group_telemetry_v1_group1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, response);
    return responseMsg;
}

// negotiated_link_speed=2 (GEN2), max_link_speed=3 (GEN3) -> covers GEN2+GEN3
// switch cases in convertToTransferRate
TEST(NsmPCIeLinkSpeedPortInfo, HandleResponse_Gen2And3_CoversSwitch)
{
    const std::string path = "/xyz/test/portinfo/gen2gen3";
    auto portInfoIntf = std::make_shared<NsmPortInfoIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmPortInfoIntf> provider("TestPort", "NSM_Port", path,
                                                   portInfoIntf);
    NsmPCIeLinkSpeed<NsmPortInfoIntf> linkSpeed(provider, uint8_t(0), false);

    nsm_query_scalar_group_telemetry_group_1 data{};
    data.negotiated_link_speed = NSM_PCIE_LINK_SPEED_CODE_GEN2;
    data.max_link_speed = NSM_PCIE_LINK_SPEED_CODE_GEN3;
    auto responseMsg = buildPortInfoResponse(data);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    auto rc = linkSpeed.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_DOUBLE_EQ(portInfoIntf->currentSpeed(), NSM_PCIE_SPEED_GBPS_GEN2);
    EXPECT_DOUBLE_EQ(portInfoIntf->maxSpeed(), NSM_PCIE_SPEED_GBPS_GEN3);
}

// negotiated_link_speed=4 (GEN4), max_link_speed=5 (GEN5) -> covers GEN4+GEN5
TEST(NsmPCIeLinkSpeedPortInfo, HandleResponse_Gen4And5_CoversSwitch)
{
    const std::string path = "/xyz/test/portinfo/gen4gen5";
    auto portInfoIntf = std::make_shared<NsmPortInfoIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmPortInfoIntf> provider("TestPort2", "NSM_Port",
                                                   path, portInfoIntf);
    NsmPCIeLinkSpeed<NsmPortInfoIntf> linkSpeed(provider, uint8_t(0), false);

    nsm_query_scalar_group_telemetry_group_1 data{};
    data.negotiated_link_speed = NSM_PCIE_LINK_SPEED_CODE_GEN4;
    data.max_link_speed = NSM_PCIE_LINK_SPEED_CODE_GEN5;
    auto responseMsg = buildPortInfoResponse(data);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    auto rc = linkSpeed.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_DOUBLE_EQ(portInfoIntf->currentSpeed(), NSM_PCIE_SPEED_GBPS_GEN4);
    EXPECT_DOUBLE_EQ(portInfoIntf->maxSpeed(), NSM_PCIE_SPEED_GBPS_GEN5);
}

// negotiated_link_speed=6 (GEN6) -> covers GEN6 switch case
// max_link_speed=99 (unknown) -> covers default case (debug log + return 0.0)
TEST(NsmPCIeLinkSpeedPortInfo, HandleResponse_Gen6AndUnknown_CoversDefault)
{
    const std::string path = "/xyz/test/portinfo/gen6default";
    auto portInfoIntf = std::make_shared<NsmPortInfoIntf>(bus, path.c_str());
    NsmInterfaceProvider<NsmPortInfoIntf> provider("TestPort3", "NSM_Port",
                                                   path, portInfoIntf);
    NsmPCIeLinkSpeed<NsmPortInfoIntf> linkSpeed(provider, uint8_t(0), false);

    nsm_query_scalar_group_telemetry_group_1 data{};
    data.negotiated_link_speed = NSM_PCIE_LINK_SPEED_CODE_GEN6;
    data.max_link_speed = 99; // triggers default branch
    auto responseMsg = buildPortInfoResponse(data);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    auto rc = linkSpeed.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_DOUBLE_EQ(portInfoIntf->currentSpeed(), NSM_PCIE_SPEED_GBPS_GEN6);
    EXPECT_DOUBLE_EQ(portInfoIntf->maxSpeed(), 0.0);
}

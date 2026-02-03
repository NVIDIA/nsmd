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
using namespace ::testing;

#include "pci-links.h"

#define private public
#define protected public

#include "nsmPCIeLinkSpeed.hpp"

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

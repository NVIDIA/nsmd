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
using namespace ::testing;

#include "pci-links.h"

#include <sdbusplus/bus.hpp>

#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmRetimerPort.hpp"

using namespace nsm;

static auto bus = sdbusplus::bus::new_default();

// Common inventory object path used across tests
static const std::string inventoryObjPath =
    "/xyz/openbmc_project/inventory/system/retimer/port0";

// ============================================================================
// NsmPort Tests
// ============================================================================

TEST(NsmPort, ConstructorBasic)
{
    std::string portName = "retimer_port_0";
    std::string type = "NSM_PCIeRetimer";
    std::vector<utils::Association> associations;

    NsmPort port(bus, portName, type, associations, inventoryObjPath);

    EXPECT_EQ(port.portName, portName);
    EXPECT_EQ(port.getName(), portName);
    EXPECT_EQ(port.getType(), type);
}

TEST(NsmPort, ConstructorWithAssociations)
{
    std::string portName = "retimer_port_1";
    std::string type = "NSM_PCIeRetimer";
    std::vector<utils::Association> associations;
    associations.push_back({"parent_retimer", "port",
                            "/xyz/openbmc_project/inventory/system/retimer0"});
    associations.push_back(
        {"connected", "port", "/xyz/openbmc_project/inventory/system/device0"});

    std::string objPath = "/xyz/openbmc_project/inventory/system/retimer/port1";

    NsmPort port(bus, portName, type, associations, objPath);

    EXPECT_EQ(port.portName, portName);
    EXPECT_EQ(port.getName(), portName);
}

TEST(NsmPort, ConstructorEmptyAssociations)
{
    std::string portName = "retimer_port_2";
    std::string type = "NSM_PCIeRetimer";
    std::vector<utils::Association> associations;

    std::string objPath = "/xyz/openbmc_project/inventory/system/retimer/port2";

    NsmPort port(bus, portName, type, associations, objPath);

    EXPECT_EQ(port.portName, portName);
}

// ============================================================================
// NsmPCIeECCGroup1 Tests
// ============================================================================

TEST(NsmPCIeECCGroup1, ConstructorDeviceIndex)
{
    std::string name = "group1_sensor";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    auto portWidthIntf =
        std::make_shared<PortWidthIntf>(bus, inventoryObjPath.c_str());
    uint8_t deviceIndex = 1;

    NsmPCIeECCGroup1 group(name, type, inventoryObjPath, portInfoIntf,
                           portWidthIntf, deviceIndex);

    EXPECT_EQ(group.getName(), name);
    EXPECT_EQ(group.getType(), type);
    EXPECT_EQ(portInfoIntf->maxSpeed(), 0);
    EXPECT_EQ(portInfoIntf->currentSpeed(), 0);
    EXPECT_EQ(portInfoIntf->targetSpeed(), 0);
    EXPECT_EQ(portWidthIntf->width(), 0u);
    EXPECT_EQ(portWidthIntf->activeWidth(), 0u);
}

TEST(NsmPCIeECCGroup1, ConstructorMultiPort)
{
    std::string name = "group1_multi_sensor";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    auto portWidthIntf =
        std::make_shared<PortWidthIntf>(bus, inventoryObjPath.c_str());
    auto pcieClockModeIntf =
        std::make_shared<PCIeClockModeIntf>(bus, inventoryObjPath.c_str());
    uint8_t multiPortType = NSM_PORT_TYPE_UPSTREAM;
    uint8_t multiPortIndex = 0;
    uint8_t multiPortUpstreamPortNumber = 1;

    NsmPCIeECCGroup1 group(name, type, inventoryObjPath, portInfoIntf,
                           portWidthIntf, pcieClockModeIntf, multiPortType,
                           multiPortIndex, multiPortUpstreamPortNumber);

    EXPECT_EQ(group.getName(), name);
    EXPECT_EQ(group.getType(), type);
    EXPECT_EQ(portInfoIntf->maxSpeed(), 0);
    EXPECT_EQ(portInfoIntf->currentSpeed(), 0);
    EXPECT_EQ(portInfoIntf->targetSpeed(), 0);
    EXPECT_EQ(portWidthIntf->width(), 0u);
    EXPECT_EQ(portWidthIntf->activeWidth(), 0u);
}

TEST(NsmPCIeECCGroup1, HandleResponseMsgSuccess)
{
    std::string name = "group1_resp";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    auto portWidthIntf =
        std::make_shared<PortWidthIntf>(bus, inventoryObjPath.c_str());
    auto pcieClockModeIntf =
        std::make_shared<PCIeClockModeIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup1 group(name, type, inventoryObjPath, portInfoIntf,
                           portWidthIntf, pcieClockModeIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    // Arrange: build a valid group1 response
    struct nsm_query_scalar_group_telemetry_group_1 data = {};
    data.negotiated_link_speed = 5; // 32.0 Gbps
    data.negotiated_link_width = 4; // 2^(4-1) = 8
    data.target_link_speed = 4;     // 16.0 Gbps
    data.max_link_speed = 6;        // 64.0 Gbps
    data.max_link_width = 5;        // 2^(5-1) = 16

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_DOUBLE_EQ(portInfoIntf->currentSpeed(), 32.0);
    EXPECT_DOUBLE_EQ(portInfoIntf->targetSpeed(), 16.0);
    EXPECT_DOUBLE_EQ(portInfoIntf->maxSpeed(), 64.0);
    EXPECT_EQ(portWidthIntf->activeWidth(), 8u);
    EXPECT_EQ(portWidthIntf->width(), 16u);
}

TEST(NsmPCIeECCGroup1, HandleResponseMsgError)
{
    std::string name = "group1_err";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    auto portWidthIntf =
        std::make_shared<PortWidthIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup1 group(name, type, inventoryObjPath, portInfoIntf,
                           portWidthIntf, uint8_t(1));

    // Arrange: build an error response (cc = NSM_ERROR)
    struct nsm_query_scalar_group_telemetry_group_1 data = {};
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_scalar_group_telemetry_v1_group1_resp(0, NSM_ERROR, ERR_NULL,
                                                       &data, responseMsg);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_NE(cc, NSM_SUCCESS);
}

TEST(NsmPCIeECCGroup1, ConvertEncodedSpeedToGbps)
{
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    auto portWidthIntf =
        std::make_shared<PortWidthIntf>(bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup1 group("test", "NSM_PCIeRetimer", inventoryObjPath,
                           portInfoIntf, portWidthIntf, uint8_t(1));

    EXPECT_DOUBLE_EQ(group.convertEncodedSpeedToGbps(1), 2.5);
    EXPECT_DOUBLE_EQ(group.convertEncodedSpeedToGbps(2), 5.0);
    EXPECT_DOUBLE_EQ(group.convertEncodedSpeedToGbps(3), 8.0);
    EXPECT_DOUBLE_EQ(group.convertEncodedSpeedToGbps(4), 16.0);
    EXPECT_DOUBLE_EQ(group.convertEncodedSpeedToGbps(5), 32.0);
    EXPECT_DOUBLE_EQ(group.convertEncodedSpeedToGbps(6), 64.0);
    // Unknown speed
    EXPECT_DOUBLE_EQ(group.convertEncodedSpeedToGbps(0), 0.0);
    EXPECT_DOUBLE_EQ(group.convertEncodedSpeedToGbps(7), 0.0);
    EXPECT_DOUBLE_EQ(group.convertEncodedSpeedToGbps(99), 0.0);
}

TEST(NsmPCIeECCGroup1, ConvertEncodedWidthToActualWidth)
{
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    auto portWidthIntf =
        std::make_shared<PortWidthIntf>(bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup1 group("test", "NSM_PCIeRetimer", inventoryObjPath,
                           portInfoIntf, portWidthIntf, uint8_t(1));

    EXPECT_EQ(group.convertEncodedWidthToActualWidth(1), 1u);  // 2^0
    EXPECT_EQ(group.convertEncodedWidthToActualWidth(2), 2u);  // 2^1
    EXPECT_EQ(group.convertEncodedWidthToActualWidth(3), 4u);  // 2^2
    EXPECT_EQ(group.convertEncodedWidthToActualWidth(4), 8u);  // 2^3
    EXPECT_EQ(group.convertEncodedWidthToActualWidth(5), 16u); // 2^4
    EXPECT_EQ(group.convertEncodedWidthToActualWidth(6), 32u); // 2^5
    // Out of range
    EXPECT_EQ(group.convertEncodedWidthToActualWidth(0), 0u);
    EXPECT_EQ(group.convertEncodedWidthToActualWidth(7), 0u);
    EXPECT_EQ(group.convertEncodedWidthToActualWidth(100), 0u);
}

TEST(NsmPCIeECCGroup1, GenRequestMsg)
{
    std::string name = "group1_req";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    auto portWidthIntf =
        std::make_shared<PortWidthIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup1 group(name, type, inventoryObjPath, portInfoIntf,
                           portWidthIntf, uint8_t(1));

    auto request = group.genRequestMsg(12, 30);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), sizeof(nsm_msg_hdr));
}

TEST(NsmPCIeECCGroup1, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    std::string name = "group1_bad";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    auto portWidthIntf =
        std::make_shared<PortWidthIntf>(bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup1 group(name, type, inventoryObjPath, portInfoIntf,
                           portWidthIntf, uint8_t(1));
    auto request = group.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST(NsmPCIeECCGroup1, GenRequestMsgMultiPort)
{
    std::string name = "group1_multi_req";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    auto portWidthIntf =
        std::make_shared<PortWidthIntf>(bus, inventoryObjPath.c_str());
    auto pcieClockModeIntf =
        std::make_shared<PCIeClockModeIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup1 group(name, type, inventoryObjPath, portInfoIntf,
                           portWidthIntf, pcieClockModeIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    auto request = group.genRequestMsg(12, 30);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), sizeof(nsm_msg_hdr));
}

TEST(NsmPCIeECCGroup1, HandleResponseMsgAllSpeedWidthCombinations)
{
    std::string name = "group1_combos";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    auto portWidthIntf =
        std::make_shared<PortWidthIntf>(bus, inventoryObjPath.c_str());
    auto pcieClockModeIntf =
        std::make_shared<PCIeClockModeIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup1 group(name, type, inventoryObjPath, portInfoIntf,
                           portWidthIntf, pcieClockModeIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    // Arrange: Gen1 speed, width=1
    struct nsm_query_scalar_group_telemetry_group_1 data = {};
    data.negotiated_link_speed = 1; // 2.5 Gbps
    data.negotiated_link_width = 1; // 1
    data.target_link_speed = 2;     // 5.0 Gbps
    data.max_link_speed = 3;        // 8.0 Gbps
    data.max_link_width = 3;        // 4

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_DOUBLE_EQ(portInfoIntf->currentSpeed(), 2.5);
    EXPECT_DOUBLE_EQ(portInfoIntf->targetSpeed(), 5.0);
    EXPECT_DOUBLE_EQ(portInfoIntf->maxSpeed(), 8.0);
    EXPECT_EQ(portWidthIntf->activeWidth(), 1u);
    EXPECT_EQ(portWidthIntf->width(), 4u);
}

TEST(NsmPCIeECCGroup1, HandleResponseMsgUnknownSpeedAndWidth)
{
    std::string name = "group1_unknown";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    auto portWidthIntf =
        std::make_shared<PortWidthIntf>(bus, inventoryObjPath.c_str());
    auto pcieClockModeIntf =
        std::make_shared<PCIeClockModeIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup1 group(name, type, inventoryObjPath, portInfoIntf,
                           portWidthIntf, pcieClockModeIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    // Arrange: unknown speed (0) and out-of-range width (7)
    struct nsm_query_scalar_group_telemetry_group_1 data = {};
    data.negotiated_link_speed = 0;
    data.negotiated_link_width = 7;
    data.target_link_speed = 99;
    data.max_link_speed = 0;
    data.max_link_width = 0;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_DOUBLE_EQ(portInfoIntf->currentSpeed(), 0.0);
    EXPECT_DOUBLE_EQ(portInfoIntf->targetSpeed(), 0.0);
    EXPECT_DOUBLE_EQ(portInfoIntf->maxSpeed(), 0.0);
    EXPECT_EQ(portWidthIntf->activeWidth(), 0u);
    EXPECT_EQ(portWidthIntf->width(), 0u);
}

// ============================================================================
// NsmPCIeECCGroup2 Tests
// ============================================================================

TEST(NsmPCIeECCGroup2, ConstructorDeviceIndex)
{
    std::string name = "group2_sensor";
    std::string type = "NSM_PCIeRetimer";
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     inventoryObjPath.c_str());

    NsmPCIeECCGroup2 group(name, type, inventoryObjPath, pcieEccIntf,
                           uint8_t(1));

    EXPECT_EQ(group.getName(), name);
    EXPECT_EQ(group.getType(), type);
    EXPECT_EQ(pcieEccIntf->nonfeCount(), 0u);
    EXPECT_EQ(pcieEccIntf->feCount(), 0u);
    EXPECT_EQ(pcieEccIntf->ceCount(), 0u);
    EXPECT_EQ(pcieEccIntf->unsupportedRequestCount(), 0u);
}

TEST(NsmPCIeECCGroup2, ConstructorMultiPort)
{
    std::string name = "group2_multi_sensor";
    std::string type = "NSM_PCIeRetimer";
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     inventoryObjPath.c_str());

    NsmPCIeECCGroup2 group(name, type, inventoryObjPath, pcieEccIntf,
                           NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0), uint8_t(2));

    EXPECT_EQ(group.getName(), name);
    EXPECT_EQ(group.getType(), type);
    EXPECT_EQ(pcieEccIntf->nonfeCount(), 0u);
    EXPECT_EQ(pcieEccIntf->feCount(), 0u);
    EXPECT_EQ(pcieEccIntf->ceCount(), 0u);
    EXPECT_EQ(pcieEccIntf->unsupportedRequestCount(), 0u);
}

TEST(NsmPCIeECCGroup2, HandleResponseMsgSuccess)
{
    std::string name = "group2_resp";
    std::string type = "NSM_PCIeRetimer";
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     inventoryObjPath.c_str());

    NsmPCIeECCGroup2 group(name, type, inventoryObjPath, pcieEccIntf,
                           uint8_t(1));

    // Arrange
    struct nsm_query_scalar_group_telemetry_group_2 data = {};
    data.non_fatal_errors = 10;
    data.fatal_errors = 20;
    data.correctable_errors = 30;
    data.unsupported_request_count = 40;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_2_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group2_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(pcieEccIntf->nonfeCount(), 10u);
    EXPECT_EQ(pcieEccIntf->feCount(), 20u);
    EXPECT_EQ(pcieEccIntf->ceCount(), 30u);
    EXPECT_EQ(pcieEccIntf->unsupportedRequestCount(), 40u);
}

TEST(NsmPCIeECCGroup2, HandleResponseMsgError)
{
    std::string name = "group2_err";
    std::string type = "NSM_PCIeRetimer";
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     inventoryObjPath.c_str());

    NsmPCIeECCGroup2 group(name, type, inventoryObjPath, pcieEccIntf,
                           uint8_t(1));

    // Arrange: error response
    struct nsm_query_scalar_group_telemetry_group_2 data = {};
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_2_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_scalar_group_telemetry_v1_group2_resp(0, NSM_ERROR, ERR_NULL,
                                                       &data, responseMsg);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_NE(cc, NSM_SUCCESS);
}

TEST(NsmPCIeECCGroup2, GenRequestMsg)
{
    std::string name = "group2_req";
    std::string type = "NSM_PCIeRetimer";
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     inventoryObjPath.c_str());

    NsmPCIeECCGroup2 group(name, type, inventoryObjPath, pcieEccIntf,
                           uint8_t(1));

    auto request = group.genRequestMsg(12, 30);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), sizeof(nsm_msg_hdr));
}

// ============================================================================
// NsmPCIeECCGroup3 Tests
// ============================================================================

TEST(NsmPCIeECCGroup3, ConstructorDeviceIndex)
{
    std::string name = "group3_sensor";
    std::string type = "NSM_PCIeRetimer";
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     inventoryObjPath.c_str());

    NsmPCIeECCGroup3 group(name, type, inventoryObjPath, pcieEccIntf,
                           uint8_t(1));

    EXPECT_EQ(group.getName(), name);
    EXPECT_EQ(group.getType(), type);
    EXPECT_EQ(pcieEccIntf->l0ToRecoveryCount(), 0u);
}

TEST(NsmPCIeECCGroup3, ConstructorMultiPort)
{
    std::string name = "group3_multi_sensor";
    std::string type = "NSM_PCIeRetimer";
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     inventoryObjPath.c_str());

    NsmPCIeECCGroup3 group(name, type, inventoryObjPath, pcieEccIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    EXPECT_EQ(group.getName(), name);
    EXPECT_EQ(group.getType(), type);
    EXPECT_EQ(pcieEccIntf->l0ToRecoveryCount(), 0u);
}

TEST(NsmPCIeECCGroup3, HandleResponseMsgSuccess)
{
    std::string name = "group3_resp";
    std::string type = "NSM_PCIeRetimer";
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     inventoryObjPath.c_str());

    NsmPCIeECCGroup3 group(name, type, inventoryObjPath, pcieEccIntf,
                           uint8_t(1));

    // Arrange
    struct nsm_query_scalar_group_telemetry_group_3 data = {};
    data.L0ToRecoveryCount = 42;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_3_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group3_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(pcieEccIntf->l0ToRecoveryCount(), 42u);
}

TEST(NsmPCIeECCGroup3, HandleResponseMsgError)
{
    std::string name = "group3_err";
    std::string type = "NSM_PCIeRetimer";
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     inventoryObjPath.c_str());

    NsmPCIeECCGroup3 group(name, type, inventoryObjPath, pcieEccIntf,
                           uint8_t(1));

    // Arrange: error response
    struct nsm_query_scalar_group_telemetry_group_3 data = {};
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_3_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_scalar_group_telemetry_v1_group3_resp(0, NSM_ERROR, ERR_NULL,
                                                       &data, responseMsg);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_NE(cc, NSM_SUCCESS);
}

TEST(NsmPCIeECCGroup3, GenRequestMsg)
{
    std::string name = "group3_req";
    std::string type = "NSM_PCIeRetimer";
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     inventoryObjPath.c_str());

    NsmPCIeECCGroup3 group(name, type, inventoryObjPath, pcieEccIntf,
                           uint8_t(1));

    auto request = group.genRequestMsg(12, 30);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), sizeof(nsm_msg_hdr));
}

// ============================================================================
// NsmPCIeECCGroup4 Tests
// ============================================================================

TEST(NsmPCIeECCGroup4, ConstructorDeviceIndex)
{
    std::string name = "group4_sensor";
    std::string type = "NSM_PCIeRetimer";
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     inventoryObjPath.c_str());

    NsmPCIeECCGroup4 group(name, type, inventoryObjPath, pcieEccIntf,
                           uint8_t(1));

    EXPECT_EQ(group.getName(), name);
    EXPECT_EQ(group.getType(), type);
    EXPECT_EQ(pcieEccIntf->replayCount(), 0u);
    EXPECT_EQ(pcieEccIntf->replayRolloverCount(), 0u);
    EXPECT_EQ(pcieEccIntf->nakSentCount(), 0u);
    EXPECT_EQ(pcieEccIntf->nakReceivedCount(), 0u);
    EXPECT_EQ(pcieEccIntf->fcTimeoutErrors(), 0u);
    EXPECT_EQ(pcieEccIntf->badTLPCount(), 0u);
    EXPECT_EQ(pcieEccIntf->receiverErrorCount(), 0u);
}

TEST(NsmPCIeECCGroup4, ConstructorMultiPort)
{
    std::string name = "group4_multi_sensor";
    std::string type = "NSM_PCIeRetimer";
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     inventoryObjPath.c_str());

    NsmPCIeECCGroup4 group(name, type, inventoryObjPath, pcieEccIntf,
                           NSM_PORT_TYPE_DOWNSTREAM, uint8_t(1), uint8_t(3));

    EXPECT_EQ(group.getName(), name);
    EXPECT_EQ(group.getType(), type);
    EXPECT_EQ(pcieEccIntf->replayCount(), 0u);
    EXPECT_EQ(pcieEccIntf->replayRolloverCount(), 0u);
    EXPECT_EQ(pcieEccIntf->nakSentCount(), 0u);
    EXPECT_EQ(pcieEccIntf->nakReceivedCount(), 0u);
    EXPECT_EQ(pcieEccIntf->fcTimeoutErrors(), 0u);
    EXPECT_EQ(pcieEccIntf->badTLPCount(), 0u);
    EXPECT_EQ(pcieEccIntf->receiverErrorCount(), 0u);
}

TEST(NsmPCIeECCGroup4, HandleResponseMsgSuccess)
{
    std::string name = "group4_resp";
    std::string type = "NSM_PCIeRetimer";
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     inventoryObjPath.c_str());

    NsmPCIeECCGroup4 group(name, type, inventoryObjPath, pcieEccIntf,
                           uint8_t(1));

    // Arrange
    struct nsm_query_scalar_group_telemetry_group_4 data = {};
    data.replay_cnt = 100;
    data.replay_rollover_cnt = 200;
    data.NAK_sent_cnt = 300;
    data.NAK_recv_cnt = 400;
    data.FC_timeout_err_cnt = 500;
    data.bad_TLP_cnt = 600;
    data.recv_err_cnt = 700;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_4_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group4_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(pcieEccIntf->replayCount(), 100u);
    EXPECT_EQ(pcieEccIntf->replayRolloverCount(), 200u);
    EXPECT_EQ(pcieEccIntf->nakSentCount(), 300u);
    EXPECT_EQ(pcieEccIntf->nakReceivedCount(), 400u);
    EXPECT_EQ(pcieEccIntf->fcTimeoutErrors(), 500u);
    EXPECT_EQ(pcieEccIntf->badTLPCount(), 600u);
    EXPECT_EQ(pcieEccIntf->receiverErrorCount(), 700u);
}

TEST(NsmPCIeECCGroup4, HandleResponseMsgError)
{
    std::string name = "group4_err";
    std::string type = "NSM_PCIeRetimer";
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     inventoryObjPath.c_str());

    NsmPCIeECCGroup4 group(name, type, inventoryObjPath, pcieEccIntf,
                           uint8_t(1));

    // Arrange: error response
    struct nsm_query_scalar_group_telemetry_group_4 data = {};
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_4_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_scalar_group_telemetry_v1_group4_resp(0, NSM_ERROR, ERR_NULL,
                                                       &data, responseMsg);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_NE(cc, NSM_SUCCESS);
}

TEST(NsmPCIeECCGroup4, GenRequestMsg)
{
    std::string name = "group4_req";
    std::string type = "NSM_PCIeRetimer";
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     inventoryObjPath.c_str());

    NsmPCIeECCGroup4 group(name, type, inventoryObjPath, pcieEccIntf,
                           uint8_t(1));

    auto request = group.genRequestMsg(12, 30);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), sizeof(nsm_msg_hdr));
}

// ============================================================================
// NsmPCIeECCGroup5 Tests
// ============================================================================

TEST(NsmPCIeECCGroup5, ConstructorDeviceIndex)
{
    std::string name = "group5_sensor";
    std::string type = "NSM_PCIeRetimer";
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup5 group(name, type, inventoryObjPath, portMetricsOem2Intf,
                           uint8_t(1));

    EXPECT_EQ(group.getName(), name);
    EXPECT_EQ(group.getType(), type);
    EXPECT_EQ(portMetricsOem2Intf->txBytes(), 0u);
    EXPECT_EQ(portMetricsOem2Intf->rxBytes(), 0u);
}

TEST(NsmPCIeECCGroup5, ConstructorMultiPort)
{
    std::string name = "group5_multi_sensor";
    std::string type = "NSM_PCIeRetimer";
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup5 group(name, type, inventoryObjPath, portMetricsOem2Intf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    EXPECT_EQ(group.getName(), name);
    EXPECT_EQ(group.getType(), type);
    EXPECT_EQ(portMetricsOem2Intf->txBytes(), 0u);
    EXPECT_EQ(portMetricsOem2Intf->rxBytes(), 0u);
}

TEST(NsmPCIeECCGroup5, HandleResponseMsgSuccess)
{
    std::string name = "group5_resp";
    std::string type = "NSM_PCIeRetimer";
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup5 group(name, type, inventoryObjPath, portMetricsOem2Intf,
                           uint8_t(1));

    // Arrange: Dwords are multiplied by BYTES_PER_DWORD (4)
    struct nsm_query_scalar_group_telemetry_group_5 data = {};
    data.PCIeTXDwords = 1000;
    data.PCIeRXDwords = 2000;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_5_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group5_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(portMetricsOem2Intf->txBytes(),
              static_cast<uint64_t>(1000) * BYTES_PER_DWORD);
    EXPECT_EQ(portMetricsOem2Intf->rxBytes(),
              static_cast<uint64_t>(2000) * BYTES_PER_DWORD);
}

TEST(NsmPCIeECCGroup5, HandleResponseMsgError)
{
    std::string name = "group5_err";
    std::string type = "NSM_PCIeRetimer";
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup5 group(name, type, inventoryObjPath, portMetricsOem2Intf,
                           uint8_t(1));

    // Arrange: error response
    struct nsm_query_scalar_group_telemetry_group_5 data = {};
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_5_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_scalar_group_telemetry_v1_group5_resp(0, NSM_ERROR, ERR_NULL,
                                                       &data, responseMsg);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_NE(cc, NSM_SUCCESS);
}

TEST(NsmPCIeECCGroup5, HandleResponseMsgDwordConversionLargeValues)
{
    std::string name = "group5_large";
    std::string type = "NSM_PCIeRetimer";
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup5 group(name, type, inventoryObjPath, portMetricsOem2Intf,
                           uint8_t(1));

    // Arrange: large dword values to test uint64_t conversion
    struct nsm_query_scalar_group_telemetry_group_5 data = {};
    data.PCIeTXDwords = 0xFFFFFFFF;
    data.PCIeRXDwords = 0x80000000;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_5_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group5_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(portMetricsOem2Intf->txBytes(),
              static_cast<uint64_t>(0xFFFFFFFF) * BYTES_PER_DWORD);
    EXPECT_EQ(portMetricsOem2Intf->rxBytes(),
              static_cast<uint64_t>(0x80000000) * BYTES_PER_DWORD);
}

TEST(NsmPCIeECCGroup5, HandleResponseMsgZeroDwords)
{
    std::string name = "group5_zero";
    std::string type = "NSM_PCIeRetimer";
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup5 group(name, type, inventoryObjPath, portMetricsOem2Intf,
                           uint8_t(1));

    // Arrange: zero dwords
    struct nsm_query_scalar_group_telemetry_group_5 data = {};
    data.PCIeTXDwords = 0;
    data.PCIeRXDwords = 0;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_5_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group5_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(portMetricsOem2Intf->txBytes(), 0u);
    EXPECT_EQ(portMetricsOem2Intf->rxBytes(), 0u);
}

TEST(NsmPCIeECCGroup5, GenRequestMsg)
{
    std::string name = "group5_req";
    std::string type = "NSM_PCIeRetimer";
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup5 group(name, type, inventoryObjPath, portMetricsOem2Intf,
                           uint8_t(1));

    auto request = group.genRequestMsg(12, 30);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), sizeof(nsm_msg_hdr));
}

// ============================================================================
// NsmPCIeECCGroup8 Tests
// ============================================================================

TEST(NsmPCIeECCGroup8, ConstructorDeviceIndex)
{
    std::string name = "group8_sensor";
    std::string type = "NSM_PCIeRetimer";
    auto laneErrorIntf =
        std::make_shared<LaneErrorIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup8 group(name, type, laneErrorIntf, uint8_t(1),
                           inventoryObjPath);

    EXPECT_EQ(group.getName(), name);
    EXPECT_EQ(group.getType(), type);
}

TEST(NsmPCIeECCGroup8, ConstructorMultiPort)
{
    std::string name = "group8_multi_sensor";
    std::string type = "NSM_PCIeRetimer";
    auto laneErrorIntf =
        std::make_shared<LaneErrorIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup8 group(name, type, laneErrorIntf, inventoryObjPath,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    EXPECT_EQ(group.getName(), name);
    EXPECT_EQ(group.getType(), type);
}

TEST(NsmPCIeECCGroup8, HandleResponseMsgSuccess)
{
    std::string name = "group8_resp";
    std::string type = "NSM_PCIeRetimer";
    auto laneErrorIntf =
        std::make_shared<LaneErrorIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup8 group(name, type, laneErrorIntf, uint8_t(1),
                           inventoryObjPath);

    // Arrange: set lane error counts
    struct nsm_query_scalar_group_telemetry_group_8 data = {};
    for (int i = 0; i < TOTAL_PCIE_LANE_COUNT; i++)
    {
        data.error_counts[i] = static_cast<uint32_t>(i * 10);
    }

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_8_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group8_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(cc, NSM_SUCCESS);
    auto laneErrors = laneErrorIntf->rxErrorsPerLane();
    ASSERT_EQ(laneErrors.size(), static_cast<size_t>(TOTAL_PCIE_LANE_COUNT));
    for (int i = 0; i < TOTAL_PCIE_LANE_COUNT; i++)
    {
        EXPECT_EQ(laneErrors[i], static_cast<uint32_t>(i * 10));
    }
}

TEST(NsmPCIeECCGroup8, HandleResponseMsgError)
{
    std::string name = "group8_err";
    std::string type = "NSM_PCIeRetimer";
    auto laneErrorIntf =
        std::make_shared<LaneErrorIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup8 group(name, type, laneErrorIntf, uint8_t(1),
                           inventoryObjPath);

    // Arrange: error response
    struct nsm_query_scalar_group_telemetry_group_8 data = {};
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_8_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_scalar_group_telemetry_v1_group8_resp(0, NSM_ERROR, ERR_NULL,
                                                       &data, responseMsg);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_NE(cc, NSM_SUCCESS);
}

TEST(NsmPCIeECCGroup8, HandleResponseMsgAllZeroLaneErrors)
{
    std::string name = "group8_zero";
    std::string type = "NSM_PCIeRetimer";
    auto laneErrorIntf =
        std::make_shared<LaneErrorIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup8 group(name, type, laneErrorIntf, uint8_t(1),
                           inventoryObjPath);

    // Arrange: all zeros
    struct nsm_query_scalar_group_telemetry_group_8 data = {};
    memset(&data, 0, sizeof(data));

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_8_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group8_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(cc, NSM_SUCCESS);
    auto laneErrors = laneErrorIntf->rxErrorsPerLane();
    ASSERT_EQ(laneErrors.size(), static_cast<size_t>(TOTAL_PCIE_LANE_COUNT));
    for (int i = 0; i < TOTAL_PCIE_LANE_COUNT; i++)
    {
        EXPECT_EQ(laneErrors[i], 0u);
    }
}

TEST(NsmPCIeECCGroup8, GenRequestMsg)
{
    std::string name = "group8_req";
    std::string type = "NSM_PCIeRetimer";
    auto laneErrorIntf =
        std::make_shared<LaneErrorIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup8 group(name, type, laneErrorIntf, uint8_t(1),
                           inventoryObjPath);

    auto request = group.genRequestMsg(12, 30);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), sizeof(nsm_msg_hdr));
}

// ============================================================================
// NsmPCIeECCGroup9 Tests
// ============================================================================

TEST(NsmPCIeECCGroup9, ConstructorDeviceIndex)
{
    std::string name = "group9_sensor";
    std::string type = "NSM_PCIeRetimer";
    auto aerIntf = std::make_shared<NsmRetimerAERErrorStatusIntf>(
        bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup9 group(name, type, inventoryObjPath, aerIntf, uint8_t(1));

    EXPECT_EQ(group.getName(), name);
    EXPECT_EQ(group.getType(), type);
    EXPECT_NE(aerIntf->aerUncorrectableErrorStatus().find("0x00000000"),
              std::string::npos);
    EXPECT_NE(aerIntf->aerCorrectableErrorStatus().find("0x00000000"),
              std::string::npos);
}

TEST(NsmPCIeECCGroup9, ConstructorMultiPort)
{
    std::string name = "group9_multi_sensor";
    std::string type = "NSM_PCIeRetimer";
    auto aerIntf = std::make_shared<NsmRetimerAERErrorStatusIntf>(
        bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup9 group(name, type, inventoryObjPath, aerIntf,
                           NSM_PORT_TYPE_DOWNSTREAM, uint8_t(2), uint8_t(4));

    EXPECT_EQ(group.getName(), name);
    EXPECT_EQ(group.getType(), type);
    EXPECT_NE(aerIntf->aerUncorrectableErrorStatus().find("0x00000000"),
              std::string::npos);
    EXPECT_NE(aerIntf->aerCorrectableErrorStatus().find("0x00000000"),
              std::string::npos);
}

TEST(NsmPCIeECCGroup9, HandleResponseMsgSuccess)
{
    std::string name = "group9_resp";
    std::string type = "NSM_PCIeRetimer";
    auto aerIntf = std::make_shared<NsmRetimerAERErrorStatusIntf>(
        bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup9 group(name, type, inventoryObjPath, aerIntf, uint8_t(1));

    // Arrange
    struct nsm_query_scalar_group_telemetry_group_9 data = {};
    data.aer_uncorrectable_error_status = 0xDEADBEEF;
    data.aer_correctable_error_status = 0x12345678;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_9_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group9_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_NE(aerIntf->aerUncorrectableErrorStatus().find("0xDEADBEEF"),
              std::string::npos);
    EXPECT_NE(aerIntf->aerCorrectableErrorStatus().find("0x12345678"),
              std::string::npos);
}

TEST(NsmPCIeECCGroup9, HandleResponseMsgError)
{
    std::string name = "group9_err";
    std::string type = "NSM_PCIeRetimer";
    auto aerIntf = std::make_shared<NsmRetimerAERErrorStatusIntf>(
        bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup9 group(name, type, inventoryObjPath, aerIntf, uint8_t(1));

    // Arrange: error response
    struct nsm_query_scalar_group_telemetry_group_9 data = {};
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_9_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_scalar_group_telemetry_v1_group9_resp(0, NSM_ERROR, ERR_NULL,
                                                       &data, responseMsg);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_NE(cc, NSM_SUCCESS);
}

TEST(NsmPCIeECCGroup9, HandleResponseMsgHexFormatZero)
{
    std::string name = "group9_zero";
    std::string type = "NSM_PCIeRetimer";
    auto aerIntf = std::make_shared<NsmRetimerAERErrorStatusIntf>(
        bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup9 group(name, type, inventoryObjPath, aerIntf, uint8_t(1));

    // Arrange: zero values verify hex format "0x00000000"
    struct nsm_query_scalar_group_telemetry_group_9 data = {};
    data.aer_uncorrectable_error_status = 0;
    data.aer_correctable_error_status = 0;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_9_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group9_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_NE(aerIntf->aerUncorrectableErrorStatus().find("0x00000000"),
              std::string::npos);
    EXPECT_NE(aerIntf->aerCorrectableErrorStatus().find("0x00000000"),
              std::string::npos);
}

TEST(NsmPCIeECCGroup9, HandleResponseMsgHexFormatMaxValue)
{
    std::string name = "group9_max";
    std::string type = "NSM_PCIeRetimer";
    auto aerIntf = std::make_shared<NsmRetimerAERErrorStatusIntf>(
        bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup9 group(name, type, inventoryObjPath, aerIntf, uint8_t(1));

    // Arrange: max uint32 values
    struct nsm_query_scalar_group_telemetry_group_9 data = {};
    data.aer_uncorrectable_error_status = 0xFFFFFFFF;
    data.aer_correctable_error_status = 0xFFFFFFFF;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_9_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group9_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_NE(aerIntf->aerUncorrectableErrorStatus().find("0xFFFFFFFF"),
              std::string::npos);
    EXPECT_NE(aerIntf->aerCorrectableErrorStatus().find("0xFFFFFFFF"),
              std::string::npos);
}

TEST(NsmPCIeECCGroup9, GenRequestMsg)
{
    std::string name = "group9_req";
    std::string type = "NSM_PCIeRetimer";
    auto aerIntf = std::make_shared<NsmRetimerAERErrorStatusIntf>(
        bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup9 group(name, type, inventoryObjPath, aerIntf, uint8_t(1));

    auto request = group.genRequestMsg(12, 30);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), sizeof(nsm_msg_hdr));
}

// ============================================================================
// NsmPCIeECCGroup10 Tests
// ============================================================================

struct NsmPCIeECCGroup10RetimerFixture :
    public testing::Test,
    public SensorManagerTest
{
    NsmDeviceTable devices{};
    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;

    NsmPCIeECCGroup10RetimerFixture() :
        SensorManagerTest(devices),
        systemBus(std::make_shared<sdbusplus::asio::connection>(io)),
        objServer(std::make_shared<sdbusplus::asio::object_server>(systemBus))
    {
        ON_CALL(mockManager, getObjServer())
            .WillByDefault(ReturnRef(*objServer));
    }
};

TEST_F(NsmPCIeECCGroup10RetimerFixture, ConstructorDeviceIndex)
{
    std::string name = "group10_sensor";
    std::string type = "NSM_PCIeRetimer";

    NsmPCIeECCGroup10 group(name, type, inventoryObjPath, uint8_t(1));

    EXPECT_EQ(group.getName(), name);
    EXPECT_EQ(group.getType(), type);
}

TEST_F(NsmPCIeECCGroup10RetimerFixture, ConstructorMultiPort)
{
    std::string name = "group10_multi_sensor";
    std::string type = "NSM_PCIeRetimer";

    NsmPCIeECCGroup10 group(name, type, inventoryObjPath,
                            NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(2));

    EXPECT_EQ(group.getName(), name);
    EXPECT_EQ(group.getType(), type);
}

TEST_F(NsmPCIeECCGroup10RetimerFixture, HandleResponseMsgSuccess)
{
    std::string name = "group10_resp";
    std::string type = "NSM_PCIeRetimer";

    NsmPCIeECCGroup10 group(name, type, inventoryObjPath, uint8_t(1));

    // Arrange
    struct nsm_query_scalar_group_telemetry_group_10 data = {};
    data.outbound_read_tlp_count = 100;
    data.outbound_write_tlp_count = 200;
    data.outbound_completion_tlp_count = 300;
    data.dwords_transferred_in_outbound_read_tlp_high = 0;
    data.dwords_transferred_in_outbound_read_tlp_low = 500;
    data.dwords_transferred_in_outbound_write_tlp_high = 0;
    data.dwords_transferred_in_outbound_write_tlp_low = 600;
    data.dwords_transferred_in_outbound_completion = 700;
    data.read_requests_dropped_tag_unavailable = 10;
    data.read_requests_dropped_credit_exhaustion = 20;
    data.read_requests_dropped_credit_not_posted = 30;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_10_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group10_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(group.outboundReadPktCount, 100u);
    EXPECT_EQ(group.outboundWritePktCount, 200u);
    EXPECT_EQ(group.outboundTLPCount, 300u);
    // dwords * 4 = bytes
    EXPECT_EQ(group.outboundReadTransfer, static_cast<uint64_t>(500) * 4);
    EXPECT_EQ(group.outboundWriteTransfer, static_cast<uint64_t>(600) * 4);
    EXPECT_EQ(group.outboundTLPsTransfer, static_cast<uint64_t>(700) * 4);
    EXPECT_EQ(group.reqDroppedTag, 10u);
    EXPECT_EQ(group.reqDroppedCreditCompletion, 20u);
    EXPECT_EQ(group.reqDroppedNonPostCredit, 30u);
}

TEST_F(NsmPCIeECCGroup10RetimerFixture, HandleResponseMsgError)
{
    std::string name = "group10_err";
    std::string type = "NSM_PCIeRetimer";

    NsmPCIeECCGroup10 group(name, type, inventoryObjPath, uint8_t(1));

    // Arrange: error response
    struct nsm_query_scalar_group_telemetry_group_10 data = {};
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_10_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_scalar_group_telemetry_v1_group10_resp(0, NSM_ERROR, ERR_NULL,
                                                        &data, responseMsg);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_NE(cc, NSM_SUCCESS);
}

TEST_F(NsmPCIeECCGroup10RetimerFixture, HandleResponseMsgHighDwordTransfer)
{
    std::string name = "group10_high";
    std::string type = "NSM_PCIeRetimer";

    NsmPCIeECCGroup10 group(name, type, inventoryObjPath, uint8_t(1));

    // Arrange: test 64-bit dword assembly with high bits
    struct nsm_query_scalar_group_telemetry_group_10 data = {};
    data.outbound_read_tlp_count = 1;
    data.outbound_write_tlp_count = 1;
    data.outbound_completion_tlp_count = 1;
    data.dwords_transferred_in_outbound_read_tlp_high = 1;
    data.dwords_transferred_in_outbound_read_tlp_low = 0;
    data.dwords_transferred_in_outbound_write_tlp_high = 2;
    data.dwords_transferred_in_outbound_write_tlp_low = 0;
    data.dwords_transferred_in_outbound_completion = 0;
    data.read_requests_dropped_tag_unavailable = 0;
    data.read_requests_dropped_credit_exhaustion = 0;
    data.read_requests_dropped_credit_not_posted = 0;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_10_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group10_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(cc, NSM_SUCCESS);
    uint64_t expectedReadTransfer = (static_cast<uint64_t>(1) << 32) * 4;
    EXPECT_EQ(group.outboundReadTransfer, expectedReadTransfer);
    uint64_t expectedWriteTransfer = (static_cast<uint64_t>(2) << 32) * 4;
    EXPECT_EQ(group.outboundWriteTransfer, expectedWriteTransfer);
}

TEST_F(NsmPCIeECCGroup10RetimerFixture, HandleResponseMsgCombinedHighLowDwords)
{
    std::string name = "group10_combo";
    std::string type = "NSM_PCIeRetimer";

    NsmPCIeECCGroup10 group(name, type, inventoryObjPath, uint8_t(1));

    // Arrange: test combined high and low dword values
    struct nsm_query_scalar_group_telemetry_group_10 data = {};
    data.outbound_read_tlp_count = 0;
    data.outbound_write_tlp_count = 0;
    data.outbound_completion_tlp_count = 0;
    data.dwords_transferred_in_outbound_read_tlp_high = 0x00000001;
    data.dwords_transferred_in_outbound_read_tlp_low = 0x00000002;
    data.dwords_transferred_in_outbound_write_tlp_high = 0x00000003;
    data.dwords_transferred_in_outbound_write_tlp_low = 0x00000004;
    data.dwords_transferred_in_outbound_completion = 5;
    data.read_requests_dropped_tag_unavailable = 0;
    data.read_requests_dropped_credit_exhaustion = 0;
    data.read_requests_dropped_credit_not_posted = 0;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_10_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group10_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(cc, NSM_SUCCESS);
    uint64_t expectedRead = ((static_cast<uint64_t>(1) << 32) | 2) * 4;
    EXPECT_EQ(group.outboundReadTransfer, expectedRead);
    uint64_t expectedWrite = ((static_cast<uint64_t>(3) << 32) | 4) * 4;
    EXPECT_EQ(group.outboundWriteTransfer, expectedWrite);
    EXPECT_EQ(group.outboundTLPsTransfer, static_cast<uint64_t>(5) * 4);
}

TEST_F(NsmPCIeECCGroup10RetimerFixture, GenRequestMsg)
{
    std::string name = "group10_req";
    std::string type = "NSM_PCIeRetimer";

    NsmPCIeECCGroup10 group(name, type, inventoryObjPath, uint8_t(1));

    auto request = group.genRequestMsg(12, 30);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), sizeof(nsm_msg_hdr));
}

TEST_F(NsmPCIeECCGroup10RetimerFixture, GenRequestMsgMultiPort)
{
    std::string name = "group10_multi_req";
    std::string type = "NSM_PCIeRetimer";

    NsmPCIeECCGroup10 group(name, type, inventoryObjPath,
                            NSM_PORT_TYPE_DOWNSTREAM, uint8_t(1), uint8_t(3));

    auto request = group.genRequestMsg(12, 30);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), sizeof(nsm_msg_hdr));
}

// Cover the includeInboundCounters=true branch in registerProperties() and
// handleResponseMsg() (uses extended group-10 telemetry struct)
TEST_F(NsmPCIeECCGroup10RetimerFixture, HandleResponseMsgInboundCounters)
{
    std::string name = "group10_inbound";
    std::string type = "NSM_PCIeRetimer";

    // true → uses extended decode path
    NsmPCIeECCGroup10 group(name, type, inventoryObjPath, uint8_t(1), true);

    // Build an extended group-10 response
    struct nsm_query_scalar_group_telemetry_group_10_extended data = {};
    data.outbound_write_tlp_count = 111;
    data.dwords_transferred_in_outbound_write_tlp_high = 0;
    data.dwords_transferred_in_outbound_write_tlp_low = 222;
    data.read_requests_dropped_tag_unavailable = 5;
    data.read_requests_dropped_credit_exhaustion = 6;
    data.read_requests_dropped_credit_not_posted = 7;
    data.inbound_completion_tlp_count = 333;
    data.inbound_completion_tlp_bytes_high = 0;
    data.inbound_completion_tlp_bytes_low = 444;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_10_extended_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group10_extended_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(group.outboundWritePktCount, 111u);
    EXPECT_EQ(group.outboundWriteTransfer, static_cast<uint64_t>(222) * 4);
    EXPECT_EQ(group.reqDroppedTag, 5u);
    EXPECT_EQ(group.reqDroppedCreditCompletion, 6u);
    EXPECT_EQ(group.reqDroppedNonPostCredit, 7u);
    EXPECT_EQ(group.inboundTLPCount, 333u);
    EXPECT_EQ(group.inboundTLPsTransfer, static_cast<uint64_t>(444) * 4);
}

// ============================================================================
// NsmRetimerAERErrorStatusIntf Tests
// ============================================================================

TEST(NsmRetimerAERErrorStatusIntf, ClearAERStatus)
{
    NsmRetimerAERErrorStatusIntf aerIntf(bus, inventoryObjPath.c_str());

    // clearAERStatus returns an empty object_path
    auto result = aerIntf.clearAERStatus();
    EXPECT_EQ(result, sdbusplus::message::object_path());
}

// ============================================================================
// NsmPCIeECCGroup7 Tests
// ============================================================================

TEST(NsmPCIeECCGroup7, ConstructorWithDeviceIndex)
{
    std::string name = "group7_sensor";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup7 group(name, type, inventoryObjPath, portInfoIntf,
                           uint8_t(1));

    EXPECT_EQ(group.getName(), name);
    EXPECT_EQ(group.getType(), type);
    EXPECT_NE(group.portInfoIntf, nullptr);
}

TEST(NsmPCIeECCGroup7, ConstructorWithMultiPort)
{
    std::string name = "group7_multi";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup7 group(name, type, inventoryObjPath, portInfoIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    EXPECT_EQ(group.getName(), name);
    EXPECT_EQ(group.getType(), type);
    EXPECT_NE(group.portInfoIntf, nullptr);
}

TEST(NsmPCIeECCGroup7, ConstructorWithPCIeDeviceIntf)
{
    std::string name = "group7_pcie_device";
    std::string type = "NSM_PCIeRetimer";
    auto pcieDeviceIntf =
        std::make_shared<PCIeDeviceIntf>(bus, inventoryObjPath.c_str());
    std::vector<uint64_t> functionIds = {0, 1, 2, 3, 4, 5, 6, 7};

    NsmPCIeECCGroup7 group(name, type, pcieDeviceIntf, functionIds,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    EXPECT_EQ(group.getName(), name);
    EXPECT_EQ(group.getType(), type);
    EXPECT_NE(group.pcieDeviceIntf, nullptr);
    EXPECT_EQ(group.functionIds.size(), 8u);
}

TEST(NsmPCIeECCGroup7, HandleResponseMsgSuccessRootPort)
{
    std::string name = "group7_resp";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup7 group(name, type, inventoryObjPath, portInfoIntf,
                           uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_7 data = {};
    data.port_type = NSM_PCIE_PORT_TYPE_ROOT_PORT;
    data.pcie_bus_number = 42;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group7_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(portInfoIntf->type(), PortType::RootPort);
}

TEST(NsmPCIeECCGroup7, HandleResponseMsgSuccessUpstreamPort)
{
    std::string name = "group7_upstream";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup7 group(name, type, inventoryObjPath, portInfoIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_7 data = {};
    data.port_type = NSM_PCIE_PORT_TYPE_UPSTREAM;
    data.pcie_bus_number = 5;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_scalar_group_telemetry_v1_group7_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, responseMsg);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(portInfoIntf->type(), PortType::UpstreamPort);
}

TEST(NsmPCIeECCGroup7, HandleResponseMsgSuccessDownstreamPort)
{
    std::string name = "group7_downstream";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup7 group(name, type, inventoryObjPath, portInfoIntf,
                           uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_7 data = {};
    data.port_type = NSM_PCIE_PORT_TYPE_DOWNSTREAM;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_scalar_group_telemetry_v1_group7_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, responseMsg);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(portInfoIntf->type(), PortType::DownstreamPort);
}

TEST(NsmPCIeECCGroup7, HandleResponseMsgSuccessEndpointPort)
{
    std::string name = "group7_endpoint";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup7 group(name, type, inventoryObjPath, portInfoIntf,
                           uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_7 data = {};
    data.port_type = NSM_PCIE_PORT_TYPE_ENDPOINT;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_scalar_group_telemetry_v1_group7_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, responseMsg);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(portInfoIntf->type(), PortType::EndpointPort);
}

TEST(NsmPCIeECCGroup7, HandleResponseMsgSuccessUnknownPortType)
{
    std::string name = "group7_unknown_port";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup7 group(name, type, inventoryObjPath, portInfoIntf,
                           uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_7 data = {};
    data.port_type = 0xFF; // Unknown port type -> defaults to DownstreamPort

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_scalar_group_telemetry_v1_group7_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, responseMsg);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(portInfoIntf->type(), PortType::DownstreamPort);
}

TEST(NsmPCIeECCGroup7, HandleResponseMsgSuccessPCIeDeviceAllFunctions)
{
    std::string name = "group7_pcie_all_funcs";
    std::string type = "NSM_PCIeRetimer";
    auto pcieDeviceIntf =
        std::make_shared<PCIeDeviceIntf>(bus, inventoryObjPath.c_str());
    std::vector<uint64_t> functionIds = {0, 1, 2, 3, 4, 5, 6, 7};

    NsmPCIeECCGroup7 group(name, type, pcieDeviceIntf, functionIds,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_7 data = {};
    data.pcie_bus_number = 15;
    data.port_type = NSM_PCIE_PORT_TYPE_UPSTREAM;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_scalar_group_telemetry_v1_group7_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, responseMsg);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(pcieDeviceIntf->function0BusNumber(), 15u);
    EXPECT_EQ(pcieDeviceIntf->function1BusNumber(), 15u);
    EXPECT_EQ(pcieDeviceIntf->function2BusNumber(), 15u);
    EXPECT_EQ(pcieDeviceIntf->function3BusNumber(), 15u);
    EXPECT_EQ(pcieDeviceIntf->function4BusNumber(), 15u);
    EXPECT_EQ(pcieDeviceIntf->function5BusNumber(), 15u);
    EXPECT_EQ(pcieDeviceIntf->function6BusNumber(), 15u);
    EXPECT_EQ(pcieDeviceIntf->function7BusNumber(), 15u);
}

TEST(NsmPCIeECCGroup7, HandleResponseMsgError)
{
    std::string name = "group7_err";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup7 group(name, type, inventoryObjPath, portInfoIntf,
                           uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_7 data = {};
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_scalar_group_telemetry_v1_group7_resp(0, NSM_ERROR, ERR_NULL,
                                                       &data, responseMsg);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

TEST(NsmPCIeECCGroup7, GenRequestMsg)
{
    std::string name = "group7_req";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup7 group(name, type, inventoryObjPath, portInfoIntf,
                           uint8_t(1));

    auto request = group.genRequestMsg(12, 30);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), sizeof(nsm_msg_hdr));
}

// ============================================================================
// NsmPCIeVectorGroup1 Tests
// ============================================================================

TEST(NsmPCIeVectorGroup1, Constructor)
{
    std::string name = "vec1_lane0";
    std::string type = "NSM_PCIeRetimer";
    std::string laneObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port0/Lanes/0";
    auto laneErrorIntf =
        std::make_shared<PCIeLaneErrorIntf>(bus, laneObjPath.c_str());
    auto laneAssocIntf =
        std::make_shared<AssociationDefIntf>(bus, laneObjPath.c_str());

    NsmPCIeVectorGroup1 sensor(name, type, laneObjPath, laneErrorIntf,
                               laneAssocIntf, uint8_t(0),
                               NSM_PCIE_LINK_SPEED_CODE_GEN5,
                               NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    EXPECT_EQ(sensor.getName(), name);
    EXPECT_EQ(sensor.getType(), type);
    EXPECT_EQ(laneErrorIntf->cdrErrorCount(), 0u);
}

TEST(NsmPCIeVectorGroup1, GenRequestMsg)
{
    std::string name = "vec1_gen_req";
    std::string type = "NSM_PCIeRetimer";
    std::string laneObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port0/Lanes/1";
    auto laneErrorIntf =
        std::make_shared<PCIeLaneErrorIntf>(bus, laneObjPath.c_str());
    auto laneAssocIntf =
        std::make_shared<AssociationDefIntf>(bus, laneObjPath.c_str());

    NsmPCIeVectorGroup1 sensor(name, type, laneObjPath, laneErrorIntf,
                               laneAssocIntf, uint8_t(2),
                               NSM_PCIE_LINK_SPEED_CODE_GEN4,
                               NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    auto request = sensor.genRequestMsg(10, 5);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), sizeof(nsm_msg_hdr));
}

TEST(NsmPCIeVectorGroup1, HandleResponseMsgSuccess)
{
    std::string name = "vec1_resp";
    std::string type = "NSM_PCIeRetimer";
    std::string laneObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port0/Lanes/2";
    auto laneErrorIntf =
        std::make_shared<PCIeLaneErrorIntf>(bus, laneObjPath.c_str());
    auto laneAssocIntf =
        std::make_shared<AssociationDefIntf>(bus, laneObjPath.c_str());

    NsmPCIeVectorGroup1 sensor(name, type, laneObjPath, laneErrorIntf,
                               laneAssocIntf, uint8_t(0),
                               NSM_PCIE_LINK_SPEED_CODE_GEN5,
                               NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    // Arrange: build valid response
    struct nsm_query_vector_group_1_data data = {};
    data.cdr_error_per_lane = 12345;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_vector_data_sources_v2_resp) +
        sizeof(uint32_t));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_vector_group_telemetry_v2_group1_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Act
    auto cc = sensor.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(laneErrorIntf->cdrErrorCount(), 12345u);
}

TEST(NsmPCIeVectorGroup1, HandleResponseMsgError)
{
    std::string name = "vec1_err";
    std::string type = "NSM_PCIeRetimer";
    std::string laneObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port0/Lanes/3";
    auto laneErrorIntf =
        std::make_shared<PCIeLaneErrorIntf>(bus, laneObjPath.c_str());
    auto laneAssocIntf =
        std::make_shared<AssociationDefIntf>(bus, laneObjPath.c_str());

    NsmPCIeVectorGroup1 sensor(name, type, laneObjPath, laneErrorIntf,
                               laneAssocIntf, uint8_t(0),
                               NSM_PCIE_LINK_SPEED_CODE_GEN5,
                               NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    // Arrange: error response
    struct nsm_query_vector_group_1_data data = {};
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_vector_data_sources_v2_resp) +
        sizeof(uint32_t));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_vector_group_telemetry_v2_group1_resp(0, NSM_ERROR, ERR_NULL,
                                                       &data, responseMsg);

    // Act
    auto cc = sensor.handleResponseMsg(responseMsg, response.size());

    // Assert
    EXPECT_NE(cc, NSM_SUCCESS);
}

// ============================================================================
// NsmPCIeLaneManager Tests
// ============================================================================

TEST(NsmPCIeLaneManager, Constructor)
{
    std::string name = "lane_manager";
    std::string type = "NSM_PCIeRetimer";
    std::string portObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port0";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                       portObjPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                         portObjPath.c_str());

    NsmPCIeLaneManager manager(name, type, portObjPath, portInfoIntf,
                               portWidthIntf, NSM_PORT_TYPE_UPSTREAM,
                               uint8_t(0), uint8_t(1));

    EXPECT_EQ(manager.getName(), name);
    EXPECT_EQ(manager.getType(), type);
    EXPECT_EQ(manager.lanesInitialized, false);
    EXPECT_EQ(manager.currentLaneCount, 0);
}

TEST(NsmPCIeLaneManager, ConvertSpeedGbpsToLinkSpeedCodeAllSpeeds)
{
    std::string name = "lane_mgr_speed";
    std::string type = "NSM_PCIeRetimer";
    std::string portObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port1";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                       portObjPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                         portObjPath.c_str());

    NsmPCIeLaneManager manager(name, type, portObjPath, portInfoIntf,
                               portWidthIntf, NSM_PORT_TYPE_UPSTREAM,
                               uint8_t(0), uint8_t(1));

    EXPECT_EQ(manager.convertSpeedGbpsToLinkSpeedCode(64.0),
              NSM_PCIE_LINK_SPEED_CODE_GEN6);
    EXPECT_EQ(manager.convertSpeedGbpsToLinkSpeedCode(100.0),
              NSM_PCIE_LINK_SPEED_CODE_GEN6);
    EXPECT_EQ(manager.convertSpeedGbpsToLinkSpeedCode(32.0),
              NSM_PCIE_LINK_SPEED_CODE_GEN5);
    EXPECT_EQ(manager.convertSpeedGbpsToLinkSpeedCode(16.0),
              NSM_PCIE_LINK_SPEED_CODE_GEN4);
    EXPECT_EQ(manager.convertSpeedGbpsToLinkSpeedCode(8.0),
              NSM_PCIE_LINK_SPEED_CODE_GEN3);
    EXPECT_EQ(manager.convertSpeedGbpsToLinkSpeedCode(5.0),
              NSM_PCIE_LINK_SPEED_CODE_GEN2);
    EXPECT_EQ(manager.convertSpeedGbpsToLinkSpeedCode(2.5),
              NSM_PCIE_LINK_SPEED_CODE_GEN1);
    EXPECT_EQ(manager.convertSpeedGbpsToLinkSpeedCode(0.0),
              NSM_PCIE_LINK_SPEED_CODE_UNKNOWN);
    EXPECT_EQ(manager.convertSpeedGbpsToLinkSpeedCode(1.0),
              NSM_PCIE_LINK_SPEED_CODE_UNKNOWN);
}

TEST(NsmPCIeLaneManager, HasLaneSensorFalseWhenEmpty)
{
    std::string name = "lane_mgr_has";
    std::string type = "NSM_PCIeRetimer";
    std::string portObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port2";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                       portObjPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                         portObjPath.c_str());

    NsmPCIeLaneManager manager(name, type, portObjPath, portInfoIntf,
                               portWidthIntf, NSM_PORT_TYPE_UPSTREAM,
                               uint8_t(0), uint8_t(1));

    EXPECT_FALSE(manager.hasLaneSensor(0));
    EXPECT_FALSE(manager.hasLaneSensor(1));
    EXPECT_FALSE(manager.hasLaneSensor(7));
}

TEST(NsmPCIeLaneManager, ClearAllLaneSensors)
{
    std::string name = "lane_mgr_clear";
    std::string type = "NSM_PCIeRetimer";
    std::string portObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port3";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                       portObjPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                         portObjPath.c_str());

    NsmPCIeLaneManager manager(name, type, portObjPath, portInfoIntf,
                               portWidthIntf, NSM_PORT_TYPE_UPSTREAM,
                               uint8_t(0), uint8_t(1));

    // Simulate initialized state
    manager.lanesInitialized = true;
    manager.currentLaneCount = 4;

    manager.clearAllLaneSensors();

    EXPECT_FALSE(manager.lanesInitialized);
    EXPECT_EQ(manager.currentLaneCount, 0);
    EXPECT_TRUE(manager.laneSensors.empty());
}

struct NsmPCIeLaneManagerFixture :
    public testing::Test,
    public SensorManagerTest
{
    NsmDeviceTable devices{};
    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;

    NsmPCIeLaneManagerFixture() :
        SensorManagerTest(devices),
        systemBus(std::make_shared<sdbusplus::asio::connection>(io)),
        objServer(std::make_shared<sdbusplus::asio::object_server>(systemBus))
    {
        ON_CALL(mockManager, getObjServer())
            .WillByDefault(ReturnRef(*objServer));
    }
};

TEST_F(NsmPCIeLaneManagerFixture, UpdateInvalidZeroLanes)
{
    std::string name = "lane_mgr_update_zero";
    std::string type = "NSM_PCIeRetimer";
    std::string portObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port4";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                       portObjPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                         portObjPath.c_str());

    NsmPCIeLaneManager manager(name, type, portObjPath, portInfoIntf,
                               portWidthIntf, NSM_PORT_TYPE_UPSTREAM,
                               uint8_t(0), uint8_t(1));

    auto device = std::make_shared<MockNsmDevice>(NSM_DEV_ID_GPU, 0, "Type",
                                                  "GPU", 0);

    // Zero active lanes → invalid
    portWidthIntf->activeWidth(0);
    portInfoIntf->currentSpeed(32.0); // GEN5

    auto coroutine = manager.update(device);
    EXPECT_EQ(coroutine.data(), NSM_SUCCESS);
    EXPECT_FALSE(manager.lanesInitialized);
    EXPECT_EQ(manager.laneSensors.size(), 0u);
}

TEST_F(NsmPCIeLaneManagerFixture, UpdateInvalidSpeed)
{
    std::string name = "lane_mgr_update_badspeed";
    std::string type = "NSM_PCIeRetimer";
    std::string portObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port5";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                       portObjPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                         portObjPath.c_str());

    NsmPCIeLaneManager manager(name, type, portObjPath, portInfoIntf,
                               portWidthIntf, NSM_PORT_TYPE_UPSTREAM,
                               uint8_t(0), uint8_t(1));

    auto device = std::make_shared<MockNsmDevice>(NSM_DEV_ID_GPU, 0, "Type",
                                                  "GPU", 0);

    // 4 lanes but 0.0 Gbps → unknown speed code
    portWidthIntf->activeWidth(4);
    portInfoIntf->currentSpeed(0.0); // unknown speed

    auto coroutine = manager.update(device);
    EXPECT_EQ(coroutine.data(), NSM_SUCCESS);
    EXPECT_FALSE(manager.lanesInitialized);
    EXPECT_EQ(manager.laneSensors.size(), 0u);
}

TEST_F(NsmPCIeLaneManagerFixture, UpdateValidCreateLaneSensors)
{
    std::string name = "lane_mgr_update_valid";
    std::string type = "NSM_PCIeRetimer";
    std::string portObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port6";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                       portObjPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                         portObjPath.c_str());

    NsmPCIeLaneManager manager(name, type, portObjPath, portInfoIntf,
                               portWidthIntf, NSM_PORT_TYPE_UPSTREAM,
                               uint8_t(0), uint8_t(1));

    auto device = std::make_shared<MockNsmDevice>(NSM_DEV_ID_GPU, 0, "Type",
                                                  "GPU", 0);

    // 4 active lanes at GEN5 speed
    portWidthIntf->activeWidth(4);
    portInfoIntf->currentSpeed(32.0); // GEN5

    auto coroutine = manager.update(device);
    EXPECT_EQ(coroutine.data(), NSM_SUCCESS);
    EXPECT_TRUE(manager.lanesInitialized);
    EXPECT_EQ(manager.currentLaneCount, 4);
    EXPECT_EQ(manager.laneSensors.size(), 4u);
    EXPECT_TRUE(manager.hasLaneSensor(0));
    EXPECT_TRUE(manager.hasLaneSensor(1));
    EXPECT_TRUE(manager.hasLaneSensor(2));
    EXPECT_TRUE(manager.hasLaneSensor(3));
    EXPECT_FALSE(manager.hasLaneSensor(4));
}

TEST_F(NsmPCIeLaneManagerFixture, UpdateAlreadyInitializedSameConfig)
{
    std::string name = "lane_mgr_update_same";
    std::string type = "NSM_PCIeRetimer";
    std::string portObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port7";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                       portObjPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                         portObjPath.c_str());

    NsmPCIeLaneManager manager(name, type, portObjPath, portInfoIntf,
                               portWidthIntf, NSM_PORT_TYPE_UPSTREAM,
                               uint8_t(0), uint8_t(1));

    auto device = std::make_shared<MockNsmDevice>(NSM_DEV_ID_GPU, 0, "Type",
                                                  "GPU", 0);

    portWidthIntf->activeWidth(2);
    portInfoIntf->currentSpeed(16.0); // GEN4

    // First update: initialize
    manager.update(device);
    ASSERT_TRUE(manager.lanesInitialized);
    ASSERT_EQ(manager.laneSensors.size(), 2u);

    // Second update with same config: no change
    auto coroutine = manager.update(device);
    EXPECT_EQ(coroutine.data(), NSM_SUCCESS);
    EXPECT_TRUE(manager.lanesInitialized);
    EXPECT_EQ(manager.laneSensors.size(), 2u);
}

TEST_F(NsmPCIeLaneManagerFixture, UpdateLaneConfigChanged)
{
    std::string name = "lane_mgr_update_changed";
    std::string type = "NSM_PCIeRetimer";
    std::string portObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port8";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                       portObjPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                         portObjPath.c_str());

    NsmPCIeLaneManager manager(name, type, portObjPath, portInfoIntf,
                               portWidthIntf, NSM_PORT_TYPE_UPSTREAM,
                               uint8_t(0), uint8_t(1));

    auto device = std::make_shared<MockNsmDevice>(NSM_DEV_ID_GPU, 0, "Type",
                                                  "GPU", 0);

    // First update: 4 lanes at GEN5
    portWidthIntf->activeWidth(4);
    portInfoIntf->currentSpeed(32.0); // GEN5
    manager.update(device);
    ASSERT_TRUE(manager.lanesInitialized);
    ASSERT_EQ(manager.laneSensors.size(), 4u);
    ASSERT_EQ(manager.currentLaneCount, 4);

    // Release old sensor references from device so D-Bus objects can be
    // destroyed before recreating at the same paths
    device->roundRobinSensors.clear();
    device->deviceSensors.clear();

    // Second update: 2 lanes at GEN4 (config changed → clears and recreates)
    portWidthIntf->activeWidth(2);
    portInfoIntf->currentSpeed(16.0); // GEN4
    auto coroutine = manager.update(device);
    EXPECT_EQ(coroutine.data(), NSM_SUCCESS);
    EXPECT_TRUE(manager.lanesInitialized);
    EXPECT_EQ(manager.currentLaneCount, 2);
    EXPECT_EQ(manager.laneSensors.size(), 2u);
}

TEST_F(NsmPCIeLaneManagerFixture, UpdateClearsInvalidAfterInitialized)
{
    std::string name = "lane_mgr_update_clear_after";
    std::string type = "NSM_PCIeRetimer";
    std::string portObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port9";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                       portObjPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                         portObjPath.c_str());

    NsmPCIeLaneManager manager(name, type, portObjPath, portInfoIntf,
                               portWidthIntf, NSM_PORT_TYPE_UPSTREAM,
                               uint8_t(0), uint8_t(1));

    auto device = std::make_shared<MockNsmDevice>(NSM_DEV_ID_GPU, 0, "Type",
                                                  "GPU", 0);

    // First: initialize with valid lanes
    portWidthIntf->activeWidth(4);
    portInfoIntf->currentSpeed(32.0);
    manager.update(device);
    ASSERT_TRUE(manager.lanesInitialized);

    // Now update with invalid (zero lanes) → should clear
    portWidthIntf->activeWidth(0);
    auto coroutine = manager.update(device);
    EXPECT_EQ(coroutine.data(), NSM_SUCCESS);
    EXPECT_FALSE(manager.lanesInitialized);
    EXPECT_EQ(manager.laneSensors.size(), 0u);
}

// ============================================================================
// addSensor<T> instantiation coverage
// ============================================================================

TEST_F(NsmPCIeECCGroup10RetimerFixture, AddSensorNsmPCIeECCGroup10)
{
    auto device = std::make_shared<MockNsmDevice>(NSM_DEV_ID_PCIE_BRIDGE, 0,
                                                  "MCTP_EID", "12", 0);
    std::string uniquePath = inventoryObjPath + "/addSensorTest";
    auto sensor = std::make_shared<NsmPCIeECCGroup10>(
        std::string("group10_as"), std::string("NSM_PCIeRetimer"), uniquePath,
        uint8_t(0));
    size_t before = device->deviceSensors.size();
    device->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(device->deviceSensors.size(), before);
}

// ==========================================================================
// HandleResponseMsg decode failure tests (rc != NSM_SW_SUCCESS branch)
//
// Each test uses a sizeof(nsm_msg_hdr) + 2 = 7 byte buffer so that
// decode_reason_code_and_cc safely reads cc at payload[1] (zero = NSM_SUCCESS)
// but the subsequent minimum-size length check in the decode function fails
// (NSM_SW_ERROR_LENGTH), covering the rc != NSM_SW_SUCCESS short-circuit
// branch of the compound condition `if (rc == NSM_SW_SUCCESS && cc ==
// NSM_SUCCESS)`.
// ==========================================================================

TEST(NsmPCIeECCGroup1, HandleResponseMsgDecodeFail)
{
    std::string name = "group1_dec";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    auto portWidthIntf =
        std::make_shared<PortWidthIntf>(bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup1 group(name, type, inventoryObjPath, portInfoIntf,
                           portWidthIntf, uint8_t(1));

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + 2, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(NsmPCIeECCGroup2, HandleResponseMsgDecodeFail)
{
    std::string name = "group2_dec";
    std::string type = "NSM_PCIeRetimer";
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    NsmPCIeECCGroup2 group(name, type, inventoryObjPath, pcieEccIntf,
                           uint8_t(1));

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + 2, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(NsmPCIeECCGroup3, HandleResponseMsgDecodeFail)
{
    std::string name = "group3_dec";
    std::string type = "NSM_PCIeRetimer";
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    NsmPCIeECCGroup3 group(name, type, inventoryObjPath, pcieEccIntf,
                           uint8_t(1));

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + 2, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(NsmPCIeECCGroup4, HandleResponseMsgDecodeFail)
{
    std::string name = "group4_dec";
    std::string type = "NSM_PCIeRetimer";
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    NsmPCIeECCGroup4 group(name, type, inventoryObjPath, pcieEccIntf,
                           uint8_t(1));

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + 2, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(NsmPCIeECCGroup5, HandleResponseMsgDecodeFail)
{
    std::string name = "group5_dec";
    std::string type = "NSM_PCIeRetimer";
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup5 group(name, type, inventoryObjPath, portMetricsOem2Intf,
                           uint8_t(1));

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + 2, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(NsmPCIeECCGroup8, HandleResponseMsgDecodeFail)
{
    std::string name = "group8_dec";
    std::string type = "NSM_PCIeRetimer";
    auto laneErrorIntf =
        std::make_shared<LaneErrorIntf>(bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup8 group(name, type, laneErrorIntf, uint8_t(1),
                           inventoryObjPath);

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + 2, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(NsmPCIeECCGroup9, HandleResponseMsgDecodeFail)
{
    std::string name = "group9_dec";
    std::string type = "NSM_PCIeRetimer";
    auto aerIntf = std::make_shared<NsmRetimerAERErrorStatusIntf>(
        bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup9 group(name, type, inventoryObjPath, aerIntf, uint8_t(1));

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + 2, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST_F(NsmPCIeECCGroup10RetimerFixture, HandleResponseMsgDecodeFail)
{
    std::string name = "group10_dec";
    std::string type = "NSM_PCIeRetimer";
    NsmPCIeECCGroup10 group(name, type, inventoryObjPath, uint8_t(1));

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + 2, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmPCIeECCGroup7 Tests
// ============================================================================

TEST(NsmPCIeECCGroup7, ConstructorDeviceIndex)
{
    std::string name = "group7_sensor";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup7 group(name, type, inventoryObjPath, portInfoIntf,
                           uint8_t(1));

    EXPECT_EQ(group.getName(), name);
    EXPECT_EQ(group.getType(), type);
}

TEST(NsmPCIeECCGroup7, ConstructorMultiPort_PortInfoIntf)
{
    std::string name = "group7_multi_sensor";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup7 group(name, type, inventoryObjPath, portInfoIntf,
                           NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0), uint8_t(1));

    EXPECT_EQ(group.getName(), name);
    EXPECT_EQ(group.getType(), type);
}

TEST(NsmPCIeECCGroup7, ConstructorMultiPort_PCIeDeviceIntf)
{
    std::string name = "group7_pcie_dev";
    std::string type = "NSM_PCIeRetimer";
    auto pcieDeviceIntf =
        std::make_shared<PCIeDeviceIntf>(bus, inventoryObjPath.c_str());
    std::vector<uint64_t> functionIds = {0, 1, 2};

    NsmPCIeECCGroup7 group(name, type, pcieDeviceIntf, functionIds,
                           NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0), uint8_t(1));

    EXPECT_EQ(group.getName(), name);
    EXPECT_EQ(group.getType(), type);
}

static std::vector<uint8_t> makeGroup7Response(uint32_t portType,
                                               uint32_t busNumber)
{
    struct nsm_query_scalar_group_telemetry_group_7 data = {};
    data.port_type = portType;
    data.pcie_bus_number = busNumber;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_scalar_group_telemetry_v1_group7_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, responseMsg);
    return response;
}

TEST(NsmPCIeECCGroup7, HandleResponseMsg_PortInfo_EndpointType)
{
    std::string name = "group7_endpoint";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup7 group(name, type, inventoryObjPath, portInfoIntf,
                           uint8_t(1));

    auto response = makeGroup7Response(NSM_PCIE_PORT_TYPE_ENDPOINT,
                                       uint32_t(5));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(portInfoIntf->type(), PortType::EndpointPort);
}

TEST(NsmPCIeECCGroup7, HandleResponseMsg_PortInfo_RootPortType)
{
    std::string name = "group7_root";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup7 group(name, type, inventoryObjPath, portInfoIntf,
                           uint8_t(1));

    auto response = makeGroup7Response(NSM_PCIE_PORT_TYPE_ROOT_PORT,
                                       uint32_t(0));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(portInfoIntf->type(), PortType::RootPort);
}

TEST(NsmPCIeECCGroup7, HandleResponseMsg_PortInfo_UpstreamType)
{
    std::string name = "group7_upstream";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup7 group(name, type, inventoryObjPath, portInfoIntf,
                           uint8_t(1));

    auto response = makeGroup7Response(NSM_PCIE_PORT_TYPE_UPSTREAM,
                                       uint32_t(0));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(portInfoIntf->type(), PortType::UpstreamPort);
}

TEST(NsmPCIeECCGroup7, HandleResponseMsg_PortInfo_DownstreamType)
{
    std::string name = "group7_downstream";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup7 group(name, type, inventoryObjPath, portInfoIntf,
                           uint8_t(1));

    auto response = makeGroup7Response(NSM_PCIE_PORT_TYPE_DOWNSTREAM,
                                       uint32_t(0));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(portInfoIntf->type(), PortType::DownstreamPort);
}

TEST(NsmPCIeECCGroup7, HandleResponseMsg_PCIeDevice_AllFunctions)
{
    std::string name = "group7_pcie_funcs";
    std::string type = "NSM_PCIeRetimer";
    auto pcieDeviceIntf =
        std::make_shared<PCIeDeviceIntf>(bus, inventoryObjPath.c_str());
    std::vector<uint64_t> functionIds = {0, 1, 2, 3, 4, 5, 6, 7};

    NsmPCIeECCGroup7 group(name, type, pcieDeviceIntf, functionIds,
                           NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0), uint8_t(1));

    auto response = makeGroup7Response(NSM_PCIE_PORT_TYPE_ENDPOINT, 42);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(pcieDeviceIntf->function0BusNumber(), uint8_t(42));
    EXPECT_EQ(pcieDeviceIntf->function1BusNumber(), uint8_t(42));
    EXPECT_EQ(pcieDeviceIntf->function2BusNumber(), uint8_t(42));
    EXPECT_EQ(pcieDeviceIntf->function3BusNumber(), uint8_t(42));
    EXPECT_EQ(pcieDeviceIntf->function4BusNumber(), uint8_t(42));
    EXPECT_EQ(pcieDeviceIntf->function5BusNumber(), uint8_t(42));
    EXPECT_EQ(pcieDeviceIntf->function6BusNumber(), uint8_t(42));
    EXPECT_EQ(pcieDeviceIntf->function7BusNumber(), uint8_t(42));
}

TEST(NsmPCIeECCGroup7, HandleResponseMsg_PCIeDevice_PartialFunctions)
{
    std::string name = "group7_pcie_partial";
    std::string type = "NSM_PCIeRetimer";
    auto pcieDeviceIntf =
        std::make_shared<PCIeDeviceIntf>(bus, inventoryObjPath.c_str());
    // Only set functions 2, 5 — others stay at default
    std::vector<uint64_t> functionIds = {2, 5};

    NsmPCIeECCGroup7 group(name, type, pcieDeviceIntf, functionIds,
                           NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0), uint8_t(1));

    auto response = makeGroup7Response(NSM_PCIE_PORT_TYPE_ENDPOINT, 10);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(pcieDeviceIntf->function2BusNumber(), uint8_t(10));
    EXPECT_EQ(pcieDeviceIntf->function5BusNumber(), uint8_t(10));
}

TEST(NsmPCIeECCGroup7, HandleResponseMsg_ErrorCC)
{
    std::string name = "group7_err";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup7 group(name, type, inventoryObjPath, portInfoIntf,
                           uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_7 data = {};
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_scalar_group_telemetry_v1_group7_resp(0, NSM_ERROR, ERR_NULL,
                                                       &data, responseMsg);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

TEST(NsmPCIeECCGroup7, HandleResponseMsg_DecodeFail)
{
    std::string name = "group7_dec";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup7 group(name, type, inventoryObjPath, portInfoIntf,
                           uint8_t(1));

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + 2, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(NsmPCIeECCGroup7, GenRequestMsg_WithPortType)
{
    std::string name = "group7_req";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup7 group(name, type, inventoryObjPath, portInfoIntf,
                           NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0), uint8_t(1));

    auto request = group.genRequestMsg(12, 30);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), sizeof(nsm_msg_hdr));
}

// ============================================================================
// NsmPCIeVectorGroup1 Tests
// ============================================================================

TEST(NsmPCIeVectorGroup1, Constructor_WithPortType)
{
    std::string name = "vector_g1_lane0";
    std::string type = "NSM_PCIeRetimer";
    std::string laneObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port0/Lanes/0";
    auto laneErrorIntf =
        std::make_shared<PCIeLaneErrorIntf>(bus, laneObjPath.c_str());
    auto laneAssocIntf =
        std::make_shared<AssociationDefIntf>(bus, laneObjPath.c_str());

    NsmPCIeVectorGroup1 group(name, type, laneObjPath, laneErrorIntf,
                              laneAssocIntf, uint8_t(0),
                              NSM_PCIE_LINK_SPEED_CODE_GEN5,
                              NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0), uint8_t(1));

    EXPECT_EQ(group.getName(), name);
    EXPECT_EQ(group.getType(), type);
    EXPECT_EQ(laneErrorIntf->cdrErrorCount(), uint64_t(0));
}

TEST(NsmPCIeVectorGroup1, GenRequestMsg_WithPortType)
{
    std::string name = "vector_g1_req";
    std::string type = "NSM_PCIeRetimer";
    std::string laneObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port0/Lanes/1";
    auto laneErrorIntf =
        std::make_shared<PCIeLaneErrorIntf>(bus, laneObjPath.c_str());
    auto laneAssocIntf =
        std::make_shared<AssociationDefIntf>(bus, laneObjPath.c_str());

    NsmPCIeVectorGroup1 group(name, type, laneObjPath, laneErrorIntf,
                              laneAssocIntf, uint8_t(1),
                              NSM_PCIE_LINK_SPEED_CODE_GEN4,
                              NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(2));

    auto request = group.genRequestMsg(5, 10);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), sizeof(nsm_msg_hdr));
}

TEST(NsmPCIeVectorGroup1, HandleResponseMsg_Success)
{
    std::string name = "vector_g1_resp";
    std::string type = "NSM_PCIeRetimer";
    std::string laneObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port0/Lanes/2";
    auto laneErrorIntf =
        std::make_shared<PCIeLaneErrorIntf>(bus, laneObjPath.c_str());
    auto laneAssocIntf =
        std::make_shared<AssociationDefIntf>(bus, laneObjPath.c_str());

    NsmPCIeVectorGroup1 group(name, type, laneObjPath, laneErrorIntf,
                              laneAssocIntf, uint8_t(2),
                              NSM_PCIE_LINK_SPEED_CODE_GEN5,
                              NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0), uint8_t(1));

    struct nsm_query_vector_group_1_data data = {};
    data.cdr_error_per_lane = 12345;

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_common_resp) +
                                  sizeof(nsm_query_vector_group_1_data));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_vector_group_telemetry_v2_group1_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());

    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(laneErrorIntf->cdrErrorCount(), uint64_t(12345));
}

TEST(NsmPCIeVectorGroup1, HandleResponseMsg_DecodeFailure)
{
    std::string name = "vector_g1_dec";
    std::string type = "NSM_PCIeRetimer";
    std::string laneObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port0/Lanes/3";
    auto laneErrorIntf =
        std::make_shared<PCIeLaneErrorIntf>(bus, laneObjPath.c_str());
    auto laneAssocIntf =
        std::make_shared<AssociationDefIntf>(bus, laneObjPath.c_str());

    NsmPCIeVectorGroup1 group(name, type, laneObjPath, laneErrorIntf,
                              laneAssocIntf, uint8_t(3),
                              NSM_PCIE_LINK_SPEED_CODE_GEN3,
                              NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0), uint8_t(1));

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + 2, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmPCIeLaneManager Tests
// ============================================================================

TEST(NsmPCIeLaneManager, Constructor_Downstream)
{
    std::string name = "lane_manager";
    std::string type = "NSM_PCIeRetimer";
    std::string portObjPath = inventoryObjPath;
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                       portObjPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                         portObjPath.c_str());

    NsmPCIeLaneManager mgr(name, type, portObjPath, portInfoIntf, portWidthIntf,
                           NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0), uint8_t(1));

    EXPECT_EQ(mgr.getName(), name);
    EXPECT_EQ(mgr.getType(), type);
}

TEST(NsmPCIeLaneManager, ConvertSpeedGbpsToLinkSpeedCode)
{
    std::string name = "lane_mgr_speed";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    auto portWidthIntf =
        std::make_shared<PortWidthIntf>(bus, inventoryObjPath.c_str());
    NsmPCIeLaneManager mgr(name, type, inventoryObjPath, portInfoIntf,
                           portWidthIntf, NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0),
                           uint8_t(1));

    // Test each generation threshold
    EXPECT_EQ(mgr.convertSpeedGbpsToLinkSpeedCode(64.0),
              NSM_PCIE_LINK_SPEED_CODE_GEN6);
    EXPECT_EQ(mgr.convertSpeedGbpsToLinkSpeedCode(100.0),
              NSM_PCIE_LINK_SPEED_CODE_GEN6);
    EXPECT_EQ(mgr.convertSpeedGbpsToLinkSpeedCode(32.0),
              NSM_PCIE_LINK_SPEED_CODE_GEN5);
    EXPECT_EQ(mgr.convertSpeedGbpsToLinkSpeedCode(40.0),
              NSM_PCIE_LINK_SPEED_CODE_GEN5);
    EXPECT_EQ(mgr.convertSpeedGbpsToLinkSpeedCode(16.0),
              NSM_PCIE_LINK_SPEED_CODE_GEN4);
    EXPECT_EQ(mgr.convertSpeedGbpsToLinkSpeedCode(20.0),
              NSM_PCIE_LINK_SPEED_CODE_GEN4);
    EXPECT_EQ(mgr.convertSpeedGbpsToLinkSpeedCode(8.0),
              NSM_PCIE_LINK_SPEED_CODE_GEN3);
    EXPECT_EQ(mgr.convertSpeedGbpsToLinkSpeedCode(10.0),
              NSM_PCIE_LINK_SPEED_CODE_GEN3);
    EXPECT_EQ(mgr.convertSpeedGbpsToLinkSpeedCode(5.0),
              NSM_PCIE_LINK_SPEED_CODE_GEN2);
    EXPECT_EQ(mgr.convertSpeedGbpsToLinkSpeedCode(7.0),
              NSM_PCIE_LINK_SPEED_CODE_GEN2);
    EXPECT_EQ(mgr.convertSpeedGbpsToLinkSpeedCode(2.5),
              NSM_PCIE_LINK_SPEED_CODE_GEN1);
    EXPECT_EQ(mgr.convertSpeedGbpsToLinkSpeedCode(4.0),
              NSM_PCIE_LINK_SPEED_CODE_GEN1);
    EXPECT_EQ(mgr.convertSpeedGbpsToLinkSpeedCode(1.0),
              NSM_PCIE_LINK_SPEED_CODE_UNKNOWN);
    EXPECT_EQ(mgr.convertSpeedGbpsToLinkSpeedCode(0.0),
              NSM_PCIE_LINK_SPEED_CODE_UNKNOWN);
}

TEST(NsmPCIeLaneManager, HasLaneSensor_EmptyMap)
{
    std::string name = "lane_mgr_has";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    auto portWidthIntf =
        std::make_shared<PortWidthIntf>(bus, inventoryObjPath.c_str());
    NsmPCIeLaneManager mgr(name, type, inventoryObjPath, portInfoIntf,
                           portWidthIntf, NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0),
                           uint8_t(1));

    EXPECT_FALSE(mgr.hasLaneSensor(0));
    EXPECT_FALSE(mgr.hasLaneSensor(7));
    EXPECT_FALSE(mgr.hasLaneSensor(15));
}

TEST(NsmPCIeLaneManager, ClearAllLaneSensors_Downstream)
{
    std::string name = "lane_mgr_clear";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    auto portWidthIntf =
        std::make_shared<PortWidthIntf>(bus, inventoryObjPath.c_str());
    NsmPCIeLaneManager mgr(name, type, inventoryObjPath, portInfoIntf,
                           portWidthIntf, NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0),
                           uint8_t(1));

    // Should be callable even with empty state
    mgr.clearAllLaneSensors();
    EXPECT_FALSE(mgr.hasLaneSensor(0));
    EXPECT_EQ(mgr.currentLaneCount, size_t(0));
    EXPECT_FALSE(mgr.lanesInitialized);
}

// ============================================================================
// NsmPCIeLaneManager - createLaneSensor / update branch coverage
// ============================================================================
// Each test uses a unique portObjPath to avoid D-Bus "FileExists" errors
// when lane interface objects are registered at portObjPath + "/Lanes/<idx>".

TEST(NsmPCIeLaneManager, CreateLaneSensor_NewLane)
{
    // createLaneSensor: happy-path – lane did not exist yet
    const std::string portObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port20";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                       portObjPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                         portObjPath.c_str());

    NsmPCIeLaneManager mgr("lane_mgr_new", "NSM_PCIeRetimer", portObjPath,
                           portInfoIntf, portWidthIntf,
                           NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0), uint8_t(1));

    auto nsmDevice = std::make_shared<NiceMock<MockNsmDevice>>(
        0, 0, "MCTP_UUID", "test-uuid-lane-new", NSM_DEV_ROLE_RESERVED);

    mgr.createLaneSensor(nsmDevice, uint8_t(0), NSM_PCIE_LINK_SPEED_CODE_GEN5);

    EXPECT_TRUE(mgr.hasLaneSensor(0));
    EXPECT_EQ(mgr.laneSensors.size(), size_t(1));
}

TEST(NsmPCIeLaneManager, CreateLaneSensor_AlreadyExists)
{
    // createLaneSensor: second call with same laneIdx → early return, no dup
    const std::string portObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port21";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                       portObjPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                         portObjPath.c_str());

    NsmPCIeLaneManager mgr("lane_mgr_dup", "NSM_PCIeRetimer", portObjPath,
                           portInfoIntf, portWidthIntf,
                           NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0), uint8_t(1));

    auto nsmDevice = std::make_shared<NiceMock<MockNsmDevice>>(
        0, 0, "MCTP_UUID", "test-uuid-lane-dup", NSM_DEV_ROLE_RESERVED);

    mgr.createLaneSensor(nsmDevice, uint8_t(0), NSM_PCIE_LINK_SPEED_CODE_GEN5);
    // Second call: hasLaneSensor returns true → early return
    mgr.createLaneSensor(nsmDevice, uint8_t(0), NSM_PCIE_LINK_SPEED_CODE_GEN4);

    EXPECT_EQ(mgr.laneSensors.size(), size_t(1));
}

TEST(NsmPCIeLaneManager, Update_InvalidLaneCountZero)
{
    // update: activeLaneCount == 0 → validLaneCount false → return NSM_SUCCESS
    const std::string portObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port22";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                       portObjPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                         portObjPath.c_str());

    NsmPCIeLaneManager mgr("lane_mgr_zero", "NSM_PCIeRetimer", portObjPath,
                           portInfoIntf, portWidthIntf,
                           NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0), uint8_t(1));

    portWidthIntf->activeWidth(0);    // invalid: 0 lanes
    portInfoIntf->currentSpeed(32.0); // valid: GEN5

    auto nsmDevice = std::make_shared<NiceMock<MockNsmDevice>>(
        0, 0, "MCTP_UUID", "test-uuid-lane-zero", NSM_DEV_ROLE_RESERVED);

    mgr.update(nsmDevice);

    EXPECT_FALSE(mgr.lanesInitialized);
    EXPECT_EQ(mgr.laneSensors.size(), size_t(0));
}

TEST(NsmPCIeLaneManager, Update_InvalidLaneCountExceedsMax)
{
    // update: activeLaneCount > NSM_PCIE_LANE_COUNT_MAX → validLaneCount false
    const std::string portObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port23";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                       portObjPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                         portObjPath.c_str());

    NsmPCIeLaneManager mgr("lane_mgr_over", "NSM_PCIeRetimer", portObjPath,
                           portInfoIntf, portWidthIntf,
                           NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0), uint8_t(1));

    portWidthIntf->activeWidth(NSM_PCIE_LANE_COUNT_MAX + 1); // 33, over max
    portInfoIntf->currentSpeed(32.0);                        // valid GEN5

    auto nsmDevice = std::make_shared<NiceMock<MockNsmDevice>>(
        0, 0, "MCTP_UUID", "test-uuid-lane-over", NSM_DEV_ROLE_RESERVED);

    mgr.update(nsmDevice);

    EXPECT_FALSE(mgr.lanesInitialized);
    EXPECT_EQ(mgr.laneSensors.size(), size_t(0));
}

TEST(NsmPCIeLaneManager, Update_InvalidSpeedCode)
{
    // update: currentSpeed below GEN1 → speedCode UNKNOWN → validSpeedCode
    // false
    const std::string portObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port24";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                       portObjPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                         portObjPath.c_str());

    NsmPCIeLaneManager mgr("lane_mgr_ispd", "NSM_PCIeRetimer", portObjPath,
                           portInfoIntf, portWidthIntf,
                           NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0), uint8_t(1));

    portWidthIntf->activeWidth(4);   // valid
    portInfoIntf->currentSpeed(1.0); // < GEN1 (2.5 Gbps) → UNKNOWN

    auto nsmDevice = std::make_shared<NiceMock<MockNsmDevice>>(
        0, 0, "MCTP_UUID", "test-uuid-lane-ispd", NSM_DEV_ROLE_RESERVED);

    mgr.update(nsmDevice);

    EXPECT_FALSE(mgr.lanesInitialized);
    EXPECT_EQ(mgr.laneSensors.size(), size_t(0));
}

TEST(NsmPCIeLaneManager, Update_FirstInit_CreatesSensors)
{
    // update: valid config → initialize lane sensors for the first time
    const std::string portObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port25";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                       portObjPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                         portObjPath.c_str());

    NsmPCIeLaneManager mgr("lane_mgr_init", "NSM_PCIeRetimer", portObjPath,
                           portInfoIntf, portWidthIntf,
                           NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0), uint8_t(1));

    portWidthIntf->activeWidth(4);    // 4 lanes
    portInfoIntf->currentSpeed(32.0); // GEN5

    auto nsmDevice = std::make_shared<NiceMock<MockNsmDevice>>(
        0, 0, "MCTP_UUID", "test-uuid-lane-init", NSM_DEV_ROLE_RESERVED);

    mgr.update(nsmDevice);

    EXPECT_TRUE(mgr.lanesInitialized);
    EXPECT_EQ(mgr.currentLaneCount, uint8_t(4));
    EXPECT_EQ(mgr.currentSpeedCode, NSM_PCIE_LINK_SPEED_CODE_GEN5);
    for (uint8_t i = 0; i < 4; ++i)
    {
        EXPECT_TRUE(mgr.hasLaneSensor(i));
    }
    EXPECT_EQ(mgr.laneSensors.size(), size_t(4));
}

TEST(NsmPCIeLaneManager, Update_SameConfig_NoRecreation)
{
    // update called twice with same valid config → no clear, no recreation
    const std::string portObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port26";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                       portObjPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                         portObjPath.c_str());

    NsmPCIeLaneManager mgr("lane_mgr_same", "NSM_PCIeRetimer", portObjPath,
                           portInfoIntf, portWidthIntf,
                           NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0), uint8_t(1));

    portWidthIntf->activeWidth(2);
    portInfoIntf->currentSpeed(16.0); // GEN4

    auto nsmDevice = std::make_shared<NiceMock<MockNsmDevice>>(
        0, 0, "MCTP_UUID", "test-uuid-lane-same", NSM_DEV_ROLE_RESERVED);

    mgr.update(nsmDevice); // first: initializes 2 lanes
    ASSERT_TRUE(mgr.lanesInitialized);
    ASSERT_EQ(mgr.currentLaneCount, uint8_t(2));

    mgr.update(nsmDevice); // second: same config → no change

    EXPECT_TRUE(mgr.lanesInitialized);
    EXPECT_EQ(mgr.currentLaneCount, uint8_t(2));
    EXPECT_EQ(mgr.laneSensors.size(), size_t(2));
}

TEST(NsmPCIeLaneManager, Update_ConfigChanged_ClearsAndRecreates)
{
    // update: config changes (lane count) → clearAllLaneSensors + recreate
    const std::string portObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port27";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                       portObjPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                         portObjPath.c_str());

    NsmPCIeLaneManager mgr("lane_mgr_chg", "NSM_PCIeRetimer", portObjPath,
                           portInfoIntf, portWidthIntf,
                           NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0), uint8_t(1));

    portWidthIntf->activeWidth(2);
    portInfoIntf->currentSpeed(32.0); // GEN5

    auto nsmDevice = std::make_shared<NiceMock<MockNsmDevice>>(
        0, 0, "MCTP_UUID", "test-uuid-lane-chg", NSM_DEV_ROLE_RESERVED);

    mgr.update(nsmDevice); // init: 2 lanes
    ASSERT_TRUE(mgr.lanesInitialized);
    ASSERT_EQ(mgr.currentLaneCount, uint8_t(2));

    // Release nsmDevice's references to old lane sensors so their D-Bus
    // objects are destroyed before creating new objects at the same paths.
    nsmDevice->deviceSensors.clear();
    for (auto& [pt, queue] : nsmDevice->sensors)
    {
        queue.clear();
    }

    portWidthIntf->activeWidth(1); // config changed: 1 lane
    mgr.update(nsmDevice);         // clear + recreate

    EXPECT_TRUE(mgr.lanesInitialized);
    EXPECT_EQ(mgr.currentLaneCount, uint8_t(1));
    EXPECT_TRUE(mgr.hasLaneSensor(0));
    EXPECT_FALSE(mgr.hasLaneSensor(1)); // old lane gone
    EXPECT_EQ(mgr.laneSensors.size(), size_t(1));
}

TEST(NsmPCIeLaneManager, Update_InvalidAfterInit_ClearsLanes)
{
    // update: after valid init, invalid config → clearAllLaneSensors
    const std::string portObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port28";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                       portObjPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                         portObjPath.c_str());

    NsmPCIeLaneManager mgr("lane_mgr_inv_a", "NSM_PCIeRetimer", portObjPath,
                           portInfoIntf, portWidthIntf,
                           NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0), uint8_t(1));

    portWidthIntf->activeWidth(2);
    portInfoIntf->currentSpeed(32.0); // GEN5

    auto nsmDevice = std::make_shared<NiceMock<MockNsmDevice>>(
        0, 0, "MCTP_UUID", "test-uuid-lane-inv-a", NSM_DEV_ROLE_RESERVED);

    mgr.update(nsmDevice); // init: 2 lanes
    ASSERT_TRUE(mgr.lanesInitialized);

    portWidthIntf->activeWidth(0); // now invalid: 0 lanes
    mgr.update(nsmDevice); // lanesInitialized=true → clearAllLaneSensors

    EXPECT_FALSE(mgr.lanesInitialized);
    EXPECT_EQ(mgr.currentLaneCount, size_t(0));
    EXPECT_EQ(mgr.laneSensors.size(), size_t(0));
}

// ============================================================================
// NsmPCIeECCGroup10 - includeInboundCounters=true branch coverage
// ============================================================================

TEST_F(NsmPCIeECCGroup10RetimerFixture, ConstructorWithInboundCounters)
{
    std::string name = "group10_inbound";
    std::string type = "NSM_PCIeRetimer";

    // includeInboundCounters=true covers !includeInboundCounters false branch
    // in registerProperties()
    NsmPCIeECCGroup10 group(name, type, inventoryObjPath, uint8_t(1), true);

    EXPECT_EQ(group.getName(), name);
    EXPECT_TRUE(group.includeInboundCounters);
}

TEST_F(NsmPCIeECCGroup10RetimerFixture, HandleResponseMsgInboundCountersSuccess)
{
    std::string name = "group10_inbound_resp";
    std::string type = "NSM_PCIeRetimer";

    NsmPCIeECCGroup10 group(name, type, inventoryObjPath, uint8_t(1), true);

    struct nsm_query_scalar_group_telemetry_group_10_extended data = {};
    data.outbound_write_tlp_count = 200;
    data.dwords_transferred_in_outbound_write_tlp_high = 0;
    data.dwords_transferred_in_outbound_write_tlp_low = 600;
    data.read_requests_dropped_tag_unavailable = 10;
    data.read_requests_dropped_credit_exhaustion = 20;
    data.read_requests_dropped_credit_not_posted = 30;
    data.inbound_completion_tlp_count = 500;
    data.inbound_completion_tlp_bytes_high = 0;
    data.inbound_completion_tlp_bytes_low = 1000;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_10_extended_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group10_extended_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());

    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(group.outboundWritePktCount, 200u);
    EXPECT_EQ(group.outboundWriteTransfer, static_cast<uint64_t>(600) * 4);
    EXPECT_EQ(group.reqDroppedTag, 10u);
    EXPECT_EQ(group.reqDroppedCreditCompletion, 20u);
    EXPECT_EQ(group.reqDroppedNonPostCredit, 30u);
    EXPECT_EQ(group.inboundTLPCount, 500u);
    EXPECT_EQ(group.inboundTLPsTransfer, static_cast<uint64_t>(1000) * 4);
}

TEST_F(NsmPCIeECCGroup10RetimerFixture, HandleResponseMsgInboundCountersError)
{
    std::string name = "group10_inbound_err";
    std::string type = "NSM_PCIeRetimer";

    NsmPCIeECCGroup10 group(name, type, inventoryObjPath, uint8_t(1), true);

    struct nsm_query_scalar_group_telemetry_group_10_extended data = {};
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_10_extended_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_scalar_group_telemetry_v1_group10_extended_resp(
        0, NSM_ERROR, ERR_NULL, &data, responseMsg);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

TEST_F(NsmPCIeECCGroup10RetimerFixture,
       HandleResponseMsgInboundCountersDecodeFail)
{
    std::string name = "group10_inbound_dec";
    std::string type = "NSM_PCIeRetimer";

    NsmPCIeECCGroup10 group(name, type, inventoryObjPath, uint8_t(1), true);

    // Short buffer causes decode failure for extended response
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + 2, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmPCIeVectorGroup1 - CC error branch coverage
// ============================================================================

TEST(NsmPCIeVectorGroup1, HandleResponseMsg_CCError)
{
    std::string name = "vector_g1_cc_err";
    std::string type = "NSM_PCIeRetimer";
    std::string laneObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port0/Lanes/4";
    auto laneErrorIntf =
        std::make_shared<PCIeLaneErrorIntf>(bus, laneObjPath.c_str());
    auto laneAssocIntf =
        std::make_shared<AssociationDefIntf>(bus, laneObjPath.c_str());

    NsmPCIeVectorGroup1 group(name, type, laneObjPath, laneErrorIntf,
                              laneAssocIntf, uint8_t(4),
                              NSM_PCIE_LINK_SPEED_CODE_GEN3,
                              NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0), uint8_t(1));

    struct nsm_query_vector_group_1_data data = {};
    data.cdr_error_per_lane = 0;

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_common_resp) +
                                  sizeof(nsm_query_vector_group_1_data));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_vector_group_telemetry_v2_group1_resp(
        0, NSM_ERROR, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

// ============================================================================
// NsmPCIeECCGroup7 - null both portInfoIntf and pcieDeviceIntf
// ============================================================================

TEST(NsmPCIeECCGroup7, HandleResponseMsg_NullBothIntf)
{
    std::string name = "group7_null_both";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup7 group(name, type, inventoryObjPath, portInfoIntf,
                           uint8_t(1));

    // Force both interfaces to null to test "neither block executed" path
    group.portInfoIntf = nullptr;
    // pcieDeviceIntf is already nullptr by default

    auto response = makeGroup7Response(NSM_PCIE_PORT_TYPE_DOWNSTREAM,
                                       uint32_t(42));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto cc = group.handleResponseMsg(responseMsg, response.size());

    // Should succeed (decode ok) but not update any interface
    EXPECT_EQ(cc, NSM_SUCCESS);
}

// ============================================================================
// NsmPCIeECCGroup1 additional branch coverage
// ============================================================================

TEST(NsmPCIeECCGroup1, ConvertEncodedSizeToBytes)
{
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    auto portWidthIntf =
        std::make_shared<PortWidthIntf>(bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup1 group("test_size", "NSM_PCIeRetimer", inventoryObjPath,
                           portInfoIntf, portWidthIntf, uint8_t(1));

    // size <= 5 branch: 128 << size
    EXPECT_EQ(group.convertEncodedSizeToBytes(0), size_t(128));  // 128 << 0
    EXPECT_EQ(group.convertEncodedSizeToBytes(1), size_t(256));  // 128 << 1
    EXPECT_EQ(group.convertEncodedSizeToBytes(2), size_t(512));  // 128 << 2
    EXPECT_EQ(group.convertEncodedSizeToBytes(3), size_t(1024)); // 128 << 3
    EXPECT_EQ(group.convertEncodedSizeToBytes(4), size_t(2048)); // 128 << 4
    EXPECT_EQ(group.convertEncodedSizeToBytes(5), size_t(4096)); // 128 << 5
    // size > 5 branch: returns 0
    EXPECT_EQ(group.convertEncodedSizeToBytes(6), size_t(0));
    EXPECT_EQ(group.convertEncodedSizeToBytes(99), size_t(0));
}

TEST(NsmPCIeECCGroup1, ConvertEncodedClockModeToEnum)
{
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    auto portWidthIntf =
        std::make_shared<PortWidthIntf>(bus, inventoryObjPath.c_str());
    auto pcieClockModeIntf =
        std::make_shared<PCIeClockModeIntf>(bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup1 group("test_clk", "NSM_PCIeRetimer", inventoryObjPath,
                           portInfoIntf, portWidthIntf, pcieClockModeIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    using ClockMode =
        sdbusplus::xyz::openbmc_project::PCIe::server::PCIeClockMode::ClockMode;

    // SEPARATE (0)
    EXPECT_EQ(group.convertEncodedClockModeToEnum(NSM_PCIE_CLOCK_MODE_SEPARATE),
              ClockMode::SeparateClockMode);
    // COMMON (1)
    EXPECT_EQ(group.convertEncodedClockModeToEnum(NSM_PCIE_CLOCK_MODE_COMMON),
              ClockMode::CommonClockMode);
    // Unknown → default case
    EXPECT_EQ(group.convertEncodedClockModeToEnum(99), ClockMode::Unknown);
    EXPECT_EQ(group.convertEncodedClockModeToEnum(255), ClockMode::Unknown);
}

TEST(NsmPCIeECCGroup1, HandleResponseMsgCommonClockMode)
{
    std::string name = "group1_clk";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    auto portWidthIntf =
        std::make_shared<PortWidthIntf>(bus, inventoryObjPath.c_str());
    auto pcieClockModeIntf =
        std::make_shared<PCIeClockModeIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup1 group(name, type, inventoryObjPath, portInfoIntf,
                           portWidthIntf, pcieClockModeIntf,
                           NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0), uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_1 data = {};
    data.negotiated_link_speed = 4;       // 16.0 Gbps
    data.negotiated_link_width = 3;       // 4
    data.target_link_speed = 3;           // 8.0 Gbps
    data.max_link_speed = 5;              // 32.0 Gbps
    data.max_link_width = 4;              // 8
    data.max_read_request_size_bytes = 5; // 4096 bytes
    data.max_payload_size_bytes = 3;      // 1024 bytes
    data.clock_mode = NSM_PCIE_CLOCK_MODE_COMMON;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(portInfoIntf->maxReadRequestSizeBytes(), size_t(4096));
    EXPECT_EQ(portInfoIntf->maxPayloadSizeBytes(), size_t(1024));

    using ClockMode =
        sdbusplus::xyz::openbmc_project::PCIe::server::PCIeClockMode::ClockMode;
    EXPECT_EQ(pcieClockModeIntf->commonClockMode(), ClockMode::CommonClockMode);
}

TEST(NsmPCIeECCGroup1, HandleResponseMsgUnknownClockAndOversizeFields)
{
    // Cover: convertEncodedSizeToBytes > 5, convertEncodedClockModeToEnum
    // default
    std::string name = "group1_unk";
    std::string type = "NSM_PCIeRetimer";
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    auto portWidthIntf =
        std::make_shared<PortWidthIntf>(bus, inventoryObjPath.c_str());
    auto pcieClockModeIntf =
        std::make_shared<PCIeClockModeIntf>(bus, inventoryObjPath.c_str());

    NsmPCIeECCGroup1 group(name, type, inventoryObjPath, portInfoIntf,
                           portWidthIntf, pcieClockModeIntf,
                           NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0), uint8_t(1));

    struct nsm_query_scalar_group_telemetry_group_1 data = {};
    data.max_read_request_size_bytes = 6; // > 5 → convertEncodedSizeToBytes = 0
    data.max_payload_size_bytes = 10;     // > 5 → convertEncodedSizeToBytes = 0
    data.clock_mode = 99;                 // unknown → ClockMode::Unknown

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(portInfoIntf->maxReadRequestSizeBytes(), size_t(0));
    EXPECT_EQ(portInfoIntf->maxPayloadSizeBytes(), size_t(0));

    using ClockMode =
        sdbusplus::xyz::openbmc_project::PCIe::server::PCIeClockMode::ClockMode;
    EXPECT_EQ(pcieClockModeIntf->commonClockMode(), ClockMode::Unknown);
}

// ============================================================================
// BadGenReq tests: NSM_INSTANCE_MAX+1 → genRequestMsg returns nullopt
// ============================================================================

TEST(NsmPCIeECCGroup2, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    NsmPCIeECCGroup2 group("g2_badreq", "NSM_PCIeRetimer", inventoryObjPath,
                           pcieEccIntf, uint8_t(1));
    auto request = group.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST(NsmPCIeECCGroup3, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    NsmPCIeECCGroup3 group("g3_badreq", "NSM_PCIeRetimer", inventoryObjPath,
                           pcieEccIntf, uint8_t(1));
    auto request = group.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST(NsmPCIeECCGroup4, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    NsmPCIeECCGroup4 group("g4_badreq", "NSM_PCIeRetimer", inventoryObjPath,
                           pcieEccIntf, uint8_t(1));
    auto request = group.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST(NsmPCIeECCGroup5, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup5 group("g5_badreq", "NSM_PCIeRetimer", inventoryObjPath,
                           portMetricsOem2Intf, uint8_t(1));
    auto request = group.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST(NsmPCIeECCGroup7, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup7 group("g7_badreq", "NSM_PCIeRetimer", inventoryObjPath,
                           portInfoIntf, uint8_t(1));
    auto request = group.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST(NsmPCIeECCGroup8, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    auto laneErrorIntf =
        std::make_shared<LaneErrorIntf>(bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup8 group("g8_badreq", "NSM_PCIeRetimer", laneErrorIntf,
                           uint8_t(1), inventoryObjPath);
    auto request = group.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST(NsmPCIeECCGroup9, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    auto aerIntf = std::make_shared<NsmRetimerAERErrorStatusIntf>(
        bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup9 group("g9_badreq", "NSM_PCIeRetimer", inventoryObjPath,
                           aerIntf, uint8_t(1));
    auto request = group.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmPCIeECCGroup10RetimerFixture,
       BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    NsmPCIeECCGroup10 group("g10_badreq", "NSM_PCIeRetimer", inventoryObjPath,
                            uint8_t(1));
    auto request = group.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST(NsmPCIeVectorGroup1, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    std::string laneObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port0/Lanes/0";
    auto laneErrorIntf =
        std::make_shared<PCIeLaneErrorIntf>(bus, laneObjPath.c_str());
    auto laneAssocIntf =
        std::make_shared<AssociationDefIntf>(bus, laneObjPath.c_str());
    NsmPCIeVectorGroup1 group("vg1_badreq", "NSM_PCIeRetimer", laneObjPath,
                              laneErrorIntf, laneAssocIntf, uint8_t(0),
                              NSM_PCIE_LINK_SPEED_CODE_GEN5,
                              NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));
    auto request = group.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// =============================================================================
// createNsmPCIeRetimerPorts and createNsmMultiPCIeRetimerPorts factory tests
// (static removed from nsmRetimerPort.cpp to allow forward declaration)
// =============================================================================

namespace nsm
{
requester::Coroutine createNsmPCIeRetimerPorts(SensorManager& manager,
                                               const std::string& interface,
                                               const std::string& objPath);
requester::Coroutine
    createNsmMultiPCIeRetimerPorts(SensorManager& manager,
                                   const std::string& interface,
                                   const std::string& objPath);
} // namespace nsm

struct NsmPCIeRetimerPortsFactoryFixture :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    // CX8: combined = (NSM_PCIE_BRIDGE_DEV_ROLE_CX8 << 8) |
    //                  NSM_DEV_ID_PCIE_BRIDGE = (2<<8)|2 = 514
    const uuid_t retimerUuid = "STATIC:514:0:NSM_DEVICE_INSTANCE_NUMBER:91";
    // CX9: combined = (NSM_PCIE_BRIDGE_DEV_ROLE_CX9 << 8) |
    //                  NSM_DEV_ID_PCIE_BRIDGE = (3<<8)|2 = 770
    const uuid_t cx9Uuid = "STATIC:770:0:NSM_DEVICE_INSTANCE_NUMBER:92";

    const std::string retimerIntf =
        "xyz.openbmc_project.Configuration.NSM_PCIeRetimer_PCIeLink";
    const std::string multiPortIntf =
        "xyz.openbmc_project.Configuration.NSM_PCIeRetimer_MultiPCIeLink";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> retimer;
    std::shared_ptr<MockNsmDevice> cx9Device;

    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;

    NsmPCIeRetimerPortsFactoryFixture() :
        SensorManagerTest(devices),
        systemBus(std::make_shared<sdbusplus::asio::connection>(io)),
        objServer(std::make_shared<sdbusplus::asio::object_server>(systemBus))
    {
        ON_CALL(mockManager, getObjServer())
            .WillByDefault(ReturnRef(*objServer));

        retimer = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(retimerUuid));
        EXPECT_NE(retimer, nullptr);

        cx9Device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(cx9Uuid));
        EXPECT_NE(cx9Device, nullptr);
    }

    ~NsmPCIeRetimerPortsFactoryFixture()
    {
        cleanupDeviceSensors(devices);
    }

    // Full property map for createNsmPCIeRetimerPorts
    const dbus::PropertyMap retimerPortProps = {
        {"Name", std::string("RetimerPort")},
        {"Priority", false},
        {"Count", uint64_t(1)},
        {"DeviceInstance", uint64_t(0)},
        {"InventoryObjPath",
         std::string("/xyz/openbmc_project/inventory/retimer_ports/")},
        {"UUID", retimerUuid},
        {"PortProtocol",
         std::string("xyz.openbmc_project.Inventory.Decorator.PortInfo."
                     "PortProtocol.PCIe")},
        {"PortType",
         std::string("xyz.openbmc_project.Inventory.Decorator.PortInfo."
                     "PortType.UpstreamPort")},
    };

    // Full property map for createNsmMultiPCIeRetimerPorts (all required)
    const dbus::PropertyMap multiPortProps = {
        {"Name", std::string("MultiPort")},
        {"Priority", false},
        {"Counts", std::vector<uint64_t>{1}},
        {"InventoryObjPath",
         std::string("/xyz/openbmc_project/inventory/multiport/")},
        {"UUID", retimerUuid},
        {"PortType",
         std::string("xyz.openbmc_project.Inventory.Decorator.PortInfo."
                     "PortType.UpstreamPort")},
        {"UpstreamPortNumbers", std::vector<uint64_t>{0}},
    };
};

// All 8 count() TRUE branches taken → sensors created (3 per port)
TEST_F(NsmPCIeRetimerPortsFactoryFixture, Factory_AllProperties_SensorsCreated)
{
    const std::string testPath = "/xyz/test/retimer_ports/all_props";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, retimerIntf);
    pm = retimerPortProps;

    const size_t before = retimer->deviceSensors.size();
    createNsmPCIeRetimerPorts(mockManager, retimerIntf, testPath);
    // count=1 → 1 port created with 3 sensors (Group1, Group3, Group7)
    EXPECT_GT(retimer->deviceSensors.size(), before);
}

// "Count" absent → count=0 → for loop never runs → no sensors added
// FALSE branch at line 1390 taken
TEST_F(NsmPCIeRetimerPortsFactoryFixture, Factory_MissingCount_NoSensorsCreated)
{
    const std::string testPath = "/xyz/test/retimer_ports/no_count";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, retimerIntf);
    pm = retimerPortProps;
    pm.erase("Count"); // count stays 0 → loop body never executes

    const size_t before = retimer->deviceSensors.size();
    createNsmPCIeRetimerPorts(mockManager, retimerIntf, testPath);
    EXPECT_EQ(retimer->deviceSensors.size(), before);
}

// "Name" absent → name="" → portName="_0" → sensors created
// FALSE branch at line 1380 taken
TEST_F(NsmPCIeRetimerPortsFactoryFixture,
       Factory_MissingName_DefaultNamePortCreated)
{
    const std::string testPath = "/xyz/test/retimer_ports/no_name";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, retimerIntf);
    pm = retimerPortProps;
    pm.erase("Name"); // name="" → portName="_0"

    const size_t before = retimer->deviceSensors.size();
    createNsmPCIeRetimerPorts(mockManager, retimerIntf, testPath);
    EXPECT_GT(retimer->deviceSensors.size(), before);
}

// "Priority" absent → priority=false (default bool{}) → sensors added
// FALSE branch at line 1385 taken
TEST_F(NsmPCIeRetimerPortsFactoryFixture,
       Factory_MissingPriority_DefaultPriorityPortCreated)
{
    const std::string testPath = "/xyz/test/retimer_ports/no_priority";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, retimerIntf);
    pm = retimerPortProps;
    pm["Name"] =
        std::string("RetimerPort_NoPrio"); // unique name for D-Bus path
    pm.erase("Priority");                  // priority stays false

    const size_t before = retimer->deviceSensors.size();
    createNsmPCIeRetimerPorts(mockManager, retimerIntf, testPath);
    EXPECT_GT(retimer->deviceSensors.size(), before);
}

// "DeviceInstance" absent → deviceInstance=0 →
// deviceIndex=PCIE_RETIMER_DEVICE_INDEX_START FALSE branch at line 1395 taken
TEST_F(NsmPCIeRetimerPortsFactoryFixture,
       Factory_MissingDeviceInstance_DefaultIndexPortCreated)
{
    const std::string testPath = "/xyz/test/retimer_ports/no_devinst";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, retimerIntf);
    pm = retimerPortProps;
    pm["Name"] = std::string("RetimerPort_NoDevInst"); // unique name
    pm.erase("DeviceInstance"); // deviceInstance=0 → deviceIndex=START

    const size_t before = retimer->deviceSensors.size();
    createNsmPCIeRetimerPorts(mockManager, retimerIntf, testPath);
    EXPECT_GT(retimer->deviceSensors.size(), before);
}

// "UUID" absent → uuid="" → parseStaticUuid("") throws in mock
// FALSE branch at line 1407 taken (count("UUID") == 0)
TEST_F(NsmPCIeRetimerPortsFactoryFixture, Factory_MissingUUID_ThrowsInvalidUUID)
{
    const std::string testPath = "/xyz/test/retimer_ports/no_uuid";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, retimerIntf);
    pm = retimerPortProps;
    pm.erase("UUID"); // uuid="" → mock throws on parseStaticUuid("")

    // mock::getNsmDeviceFromStaticUUID("") throws std::runtime_error
    EXPECT_THROW_COROUTINE(
        createNsmPCIeRetimerPorts(mockManager, retimerIntf, testPath),
        std::runtime_error);
}

// "InventoryObjPath", "PortProtocol", "PortType" absent AND count=0
// → three FALSE branches taken; loop doesn't run so no throw
// Lines 1401, 1412, 1418 FALSE branches covered
TEST_F(NsmPCIeRetimerPortsFactoryFixture,
       Factory_MissingInvPathProtocolType_ZeroCount_NoBranches)
{
    const std::string testPath =
        "/xyz/test/retimer_ports/no_invpath_proto_type";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, retimerIntf);
    pm = retimerPortProps;
    pm.erase("InventoryObjPath"); // inventoryObjPath=""
    pm.erase("PortProtocol");     // portProtocol=""
    pm.erase("PortType");         // portType=""
    pm["Count"] = uint64_t(0);    // loop skipped → no throw on convert("")

    const size_t before = retimer->deviceSensors.size();
    createNsmPCIeRetimerPorts(mockManager, retimerIntf, testPath);
    // count=0 → no sensors added; all three FALSE branches taken
    EXPECT_EQ(retimer->deviceSensors.size(), before);
}

// =============================================================================
// createNsmMultiPCIeRetimerPorts factory tests
// =============================================================================

// UpstreamPort → portTypeVal=NSM_PORT_TYPE_UPSTREAM → sensors + config info
TEST_F(NsmPCIeRetimerPortsFactoryFixture,
       MultiFactory_UpstreamPort_SensorsAndConfigCreated)
{
    const std::string testPath = "/xyz/test/multiport/upstream";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, multiPortIntf);
    pm = multiPortProps;

    const size_t before = retimer->deviceSensors.size();
    createNsmMultiPCIeRetimerPorts(mockManager, multiPortIntf, testPath);
    EXPECT_GT(retimer->deviceSensors.size(), before);
}

// DownstreamPort → portTypeVal=NSM_PORT_TYPE_DOWNSTREAM → sensors created
// but createNsmPCIePortConfigurationInfo NOT called (portTypeVal != UPSTREAM)
TEST_F(NsmPCIeRetimerPortsFactoryFixture,
       MultiFactory_DownstreamPort_SensorsCreatedNoConfigInfo)
{
    const std::string testPath = "/xyz/test/multiport/downstream";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, multiPortIntf);
    pm = multiPortProps;
    pm["Name"] = std::string("DownPort");
    pm["PortType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo."
        "PortType.DownstreamPort");

    const size_t beforeRetimer = retimer->deviceSensors.size();
    createNsmMultiPCIeRetimerPorts(mockManager, multiPortIntf, testPath);
    EXPECT_GT(retimer->deviceSensors.size(), beforeRetimer);
}

// PortType is no longer read from EM config (dynamic discovery via
// ListPCIePorts determines port types at runtime). Factory always succeeds
// if UUID is valid — a NsmPCIePortDiscovery sensor is created regardless
// of any PortType property in the config.
TEST_F(NsmPCIeRetimerPortsFactoryFixture,
       MultiFactory_NoPortType_StillCreatesDiscoverySensor)
{
    const std::string testPath = "/xyz/test/multiport/no_port_type";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, multiPortIntf);
    pm = multiPortProps;
    pm["Name"] = std::string("NoPortTypePort");

    const size_t before = retimer->deviceSensors.size();
    createNsmMultiPCIeRetimerPorts(mockManager, multiPortIntf, testPath);
    EXPECT_GT(retimer->deviceSensors.size(), before);
}

// UUID="" → parseStaticUuid("") throws in mock (invalid format)
TEST_F(NsmPCIeRetimerPortsFactoryFixture,
       MultiFactory_EmptyUUID_ThrowsInvalidUUID)
{
    const std::string testPath = "/xyz/test/multiport/empty_uuid";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, multiPortIntf);
    pm = multiPortProps;
    pm["Name"] = std::string("MultiPort_EmptyUUID");
    pm["UUID"] = std::string(""); // blank uuid → mock throws on getNsmDevice

    EXPECT_THROW_COROUTINE(
        createNsmMultiPCIeRetimerPorts(mockManager, multiPortIntf, testPath),
        std::runtime_error);
}

// CX9 device → getDeviceRole() == NSM_PCIE_BRIDGE_DEV_ROLE_CX9 →
// includeInboundCounters=true → NsmPCIeECCGroup10 created with inbound counters
TEST_F(NsmPCIeRetimerPortsFactoryFixture,
       MultiFactory_CX9Device_IncludeInboundCounters)
{
    const std::string testPath = "/xyz/test/multiport/cx9";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, multiPortIntf);
    pm = multiPortProps;
    pm["Name"] = std::string("CX9Port");
    pm["UUID"] = cx9Uuid; // CX9 device

    const size_t before = cx9Device->deviceSensors.size();
    createNsmMultiPCIeRetimerPorts(mockManager, multiPortIntf, testPath);
    EXPECT_GT(cx9Device->deviceSensors.size(), before);
}

// NsmPCIeLaneManager::genRequestMsg always returns nullopt (lines 377-380)
TEST(NsmPCIeLaneManager, GenRequestMsg_AlwaysReturnsNullopt)
{
    std::string portObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port_gen";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                       portObjPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                         portObjPath.c_str());
    NsmPCIeLaneManager manager("lane_gen", "NSM_PCIeRetimer", portObjPath,
                               portInfoIntf, portWidthIntf,
                               NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    auto result = manager.genRequestMsg(0, 0);
    EXPECT_FALSE(result.has_value());
}

// NsmPCIeLaneManager::handleResponseMsg always returns NSM_SUCCESS (lines
// 382-386)
TEST(NsmPCIeLaneManager, HandleResponseMsg_AlwaysReturnsSuccess)
{
    std::string portObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port_hrm";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                       portObjPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                         portObjPath.c_str());
    NsmPCIeLaneManager manager("lane_hrm", "NSM_PCIeRetimer", portObjPath,
                               portInfoIntf, portWidthIntf,
                               NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 4, 0);
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());
    EXPECT_EQ(manager.handleResponseMsg(msg, buf.size()), NSM_SUCCESS);
}

// ─── Branch coverage: rc==NSM_SW_SUCCESS && cc!=NSM_SUCCESS ─────────────────

// NsmPCIeECCGroup1::handleResponseMsg: non-success CC via 9-byte buffer →
// `if (rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS)` false branch
TEST(NsmPCIeECCGroup1, HandleResponseMsg_DecodeSuccessNonZeroCC_SkipsUpdate)
{
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    auto portWidthIntf =
        std::make_shared<PortWidthIntf>(bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup1 group("g1_ccbranch", "NSM_PCIeRetimer", inventoryObjPath,
                           portInfoIntf, portWidthIntf, uint8_t(0));

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = group.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// NsmPCIeECCGroup2::handleResponseMsg: non-success CC via 9-byte buffer →
// `if (rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS)` false branch
TEST(NsmPCIeECCGroup2, HandleResponseMsg_DecodeSuccessNonZeroCC_SkipsUpdate)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    NsmPCIeECCGroup2 group("g2_ccbranch", "NSM_PCIeRetimer", inventoryObjPath,
                           pcieEccIntf, uint8_t(0));

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = group.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ─── Branch coverage: PCIeECCGroup 3-10: rc==NSM_SW_SUCCESS && cc!=NSM_SUCCESS

TEST(NsmPCIeECCGroup3, HandleResponseMsg_DecodeSuccessNonZeroCC_SkipsUpdate)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    NsmPCIeECCGroup3 group("g3_cc", "NSM_PCIeRetimer", inventoryObjPath,
                           pcieEccIntf, uint8_t(0));
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto rc = group.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(NsmPCIeECCGroup4, HandleResponseMsg_DecodeSuccessNonZeroCC_SkipsUpdate)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus,
                                                     inventoryObjPath.c_str());
    NsmPCIeECCGroup4 group("g4_cc", "NSM_PCIeRetimer", inventoryObjPath,
                           pcieEccIntf, uint8_t(0));
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto rc = group.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(NsmPCIeECCGroup5, HandleResponseMsg_DecodeSuccessNonZeroCC_SkipsUpdate)
{
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup5 group("g5_cc", "NSM_PCIeRetimer", inventoryObjPath,
                           portMetricsOem2Intf, uint8_t(0));
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto rc = group.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(NsmPCIeECCGroup7, HandleResponseMsg_DecodeSuccessNonZeroCC_SkipsUpdate)
{
    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup7 group("g7_cc", "NSM_PCIeRetimer", inventoryObjPath,
                           portInfoIntf, uint8_t(0));
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto rc = group.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(NsmPCIeECCGroup8, HandleResponseMsg_DecodeSuccessNonZeroCC_SkipsUpdate)
{
    auto laneErrorIntf =
        std::make_shared<LaneErrorIntf>(bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup8 group("g8_cc", "NSM_PCIeRetimer", laneErrorIntf,
                           uint8_t(0), inventoryObjPath);
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto rc = group.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(NsmPCIeECCGroup9, HandleResponseMsg_DecodeSuccessNonZeroCC_SkipsUpdate)
{
    auto aerIntf = std::make_shared<NsmRetimerAERErrorStatusIntf>(
        bus, inventoryObjPath.c_str());
    NsmPCIeECCGroup9 group("g9_cc", "NSM_PCIeRetimer", inventoryObjPath,
                           aerIntf, uint8_t(0));
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto rc = group.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST_F(NsmPCIeECCGroup10RetimerFixture,
       HandleResponseMsg_DecodeSuccessNonZeroCC_SkipsUpdate)
{
    NsmPCIeECCGroup10 group("g10_cc", "NSM_PCIeRetimer", inventoryObjPath,
                            uint8_t(0));
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto rc = group.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// Branch 12: NsmPCIeVectorGroup1 L1201 second operand — rc==NSM_SW_SUCCESS but
// cc!=NSM_SUCCESS → 9-byte buffer causes decode_reason_code_and_cc to return
// NSM_SW_SUCCESS with cc=NSM_ERROR → rc=0 (FALSE) but cc!=0 (TRUE).
TEST(NsmPCIeVectorGroup1, HandleResponseMsg_DecodeSuccessNonZeroCC_SkipsUpdate)
{
    std::string laneObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port0/Lanes/5";
    auto laneErrorIntf =
        std::make_shared<PCIeLaneErrorIntf>(bus, laneObjPath.c_str());
    auto laneAssocIntf =
        std::make_shared<AssociationDefIntf>(bus, laneObjPath.c_str());
    NsmPCIeVectorGroup1 group("vg1_cc_br12", "NSM_PCIeRetimer", laneObjPath,
                              laneErrorIntf, laneAssocIntf, uint8_t(5),
                              NSM_PCIE_LINK_SPEED_CODE_GEN3,
                              NSM_PORT_TYPE_DOWNSTREAM, uint8_t(0), uint8_t(1));

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto rc = group.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// NsmPCIeECCGroup10::handleResponseMsg with includeInboundCounters=true:
// rc==NSM_SW_SUCCESS && cc!=NSM_SUCCESS (9-byte buffer) →
// `if (rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS)` FALSE branch at L962
TEST_F(NsmPCIeECCGroup10RetimerFixture,
       HandleResponseMsg_InboundCounters_DecodeSuccessNonZeroCC_SkipsUpdate)
{
    NsmPCIeECCGroup10 group("g10_inbound_cc", "NSM_PCIeRetimer",
                            inventoryObjPath, uint8_t(0), true);
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto rc = group.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// NsmPCIeLaneManager::genRequestMsg (nsmRetimerPort.hpp L377-381)
// Inline override always returns std::nullopt — covers the method body.
TEST_F(NsmPCIeLaneManagerFixture, GenRequestMsg_AlwaysReturnsNullopt)
{
    std::string name = "lane_mgr_genreq";
    std::string type = "NSM_PCIeRetimer";
    std::string portObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port_genreq";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                       portObjPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                         portObjPath.c_str());
    NsmPCIeLaneManager manager(name, type, portObjPath, portInfoIntf,
                               portWidthIntf, NSM_PORT_TYPE_UPSTREAM,
                               uint8_t(0), uint8_t(1));

    // genRequestMsg is the inline override that always returns std::nullopt
    auto result = manager.genRequestMsg(eid_t(0), uint8_t(0));
    EXPECT_FALSE(result.has_value());
}

// NsmPCIeLaneManager::handleResponseMsg (nsmRetimerPort.hpp L382-387)
// Inline override always returns NSM_SUCCESS — covers the method body.
TEST_F(NsmPCIeLaneManagerFixture, HandleResponseMsg_AlwaysReturnsSuccess)
{
    std::string name = "lane_mgr_handle";
    std::string type = "NSM_PCIeRetimer";
    std::string portObjPath =
        "/xyz/openbmc_project/inventory/system/retimer/port_handle";
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus,
                                                       portObjPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus,
                                                         portObjPath.c_str());
    NsmPCIeLaneManager manager(name, type, portObjPath, portInfoIntf,
                               portWidthIntf, NSM_PORT_TYPE_UPSTREAM,
                               uint8_t(0), uint8_t(1));

    // handleResponseMsg is the inline override that always returns NSM_SUCCESS
    auto rc = manager.handleResponseMsg(nullptr, 0);
    EXPECT_EQ(rc, NSM_SUCCESS);
}

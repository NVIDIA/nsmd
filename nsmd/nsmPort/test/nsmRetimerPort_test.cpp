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

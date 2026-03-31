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

#include "nsmFabricManager.hpp"

class NsmFabricManagerTest : public Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();

    void SetUp() override
    {
        // Reset singleton for each test
        nsm::NsmAggregateFabricManagerState::nsmAggregateFabricManagerState
            .reset();
        nsm::NsmAggregateFabricManagerState::associatedFabricManagerIntfs
            .clear();
        nsm::NsmAggregateFabricManagerState::associatedOperationalStatusIntfs
            .clear();
        nsm::NsmAggregateFabricManagerState::aggregateFMObjPath = "";
    }
};

TEST_F(NsmFabricManagerTest, GenRequestMsg_Success)
{
    std::string name = "FabricManager0";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate";
    std::string description = "Test Fabric Manager";

    nsm::NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                       inventoryObjPathFM, bus, description);

    eid_t eid = 10;
    uint8_t instanceId = 1;

    auto requestMsg = fmState.genRequestMsg(eid, instanceId);

    ASSERT_TRUE(requestMsg.has_value());
    EXPECT_EQ(requestMsg->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_fabric_manager_state_req));

    // Verify the request message structure
    auto msgPtr = reinterpret_cast<const struct nsm_msg*>(requestMsg->data());
    EXPECT_EQ(msgPtr->hdr.instance_id, instanceId);
    EXPECT_EQ(msgPtr->hdr.request, 1);
}

TEST_F(NsmFabricManagerTest, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    std::string name = "FabricManager_Bad";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager_bad/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate_bad";
    std::string description = "Test Fabric Manager Bad";

    nsm::NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                       inventoryObjPathFM, bus, description);

    auto requestMsg = fmState.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(requestMsg.has_value());
}

TEST_F(NsmFabricManagerTest, HandleResponseMsg_Success_StateOffline)
{
    std::string name = "FabricManager0";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate";
    std::string description = "Test Fabric Manager";

    nsm::NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                       inventoryObjPathFM, bus, description);

    uint8_t instanceId = 1;
    struct nsm_fabric_manager_state_data fmData = {
        .fm_state = NSM_FM_STATE_OFFLINE,
        .report_status = NSM_FM_REPORT_STATUS_RECEIVED,
        .last_restart_timestamp = 1000,
        .duration_since_last_restart_sec = 100};

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_common_resp) +
                                     sizeof(nsm_fabric_manager_state_data));
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto rc = encode_get_fabric_manager_state_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &fmData, responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = fmState.handleResponseMsg(responseMsgPtr, responseMsg.size());

    EXPECT_EQ(result, NSM_SUCCESS);

    // Verify interface values
    auto fabricManagerIntf = fmState.getFabricManagerIntf();
    ASSERT_NE(fabricManagerIntf, nullptr);
    EXPECT_EQ(fabricManagerIntf->reportStatus(), nsm::FMReportStatus::Received);
    EXPECT_EQ(fabricManagerIntf->fmState(), nsm::FMState::Offline);
    EXPECT_EQ(fabricManagerIntf->lastRestartTime(), 1000);
    EXPECT_EQ(fabricManagerIntf->lastRestartDuration(), 100);

    auto operaStatusIntf = fmState.getOperaStatusIntf();
    ASSERT_NE(operaStatusIntf, nullptr);
    EXPECT_EQ(operaStatusIntf->state(), nsm::OpState::Starting);
}

TEST_F(NsmFabricManagerTest, HandleResponseMsg_Success_StateStandby)
{
    std::string name = "FabricManager0";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate";
    std::string description = "Test Fabric Manager";

    nsm::NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                       inventoryObjPathFM, bus, description);

    uint8_t instanceId = 1;
    struct nsm_fabric_manager_state_data fmData = {
        .fm_state = NSM_FM_STATE_STANDBY,
        .report_status = NSM_FM_REPORT_STATUS_RECEIVED,
        .last_restart_timestamp = 2000,
        .duration_since_last_restart_sec = 200};

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_common_resp) +
                                     sizeof(nsm_fabric_manager_state_data));
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto rc = encode_get_fabric_manager_state_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &fmData, responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = fmState.handleResponseMsg(responseMsgPtr, responseMsg.size());

    EXPECT_EQ(result, NSM_SUCCESS);

    auto fabricManagerIntf = fmState.getFabricManagerIntf();
    EXPECT_EQ(fabricManagerIntf->reportStatus(), nsm::FMReportStatus::Received);
    EXPECT_EQ(fabricManagerIntf->fmState(), nsm::FMState::Standby);
    EXPECT_EQ(fabricManagerIntf->lastRestartTime(), 2000);
    EXPECT_EQ(fabricManagerIntf->lastRestartDuration(), 200);

    auto operaStatusIntf = fmState.getOperaStatusIntf();
    EXPECT_EQ(operaStatusIntf->state(), nsm::OpState::StandbyOffline);
}

TEST_F(NsmFabricManagerTest, HandleResponseMsg_Success_StateConfigured)
{
    std::string name = "FabricManager0";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate";
    std::string description = "Test Fabric Manager";

    nsm::NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                       inventoryObjPathFM, bus, description);

    uint8_t instanceId = 1;
    struct nsm_fabric_manager_state_data fmData = {
        .fm_state = NSM_FM_STATE_CONFIGURED,
        .report_status = NSM_FM_REPORT_STATUS_RECEIVED,
        .last_restart_timestamp = 3000,
        .duration_since_last_restart_sec = 300};

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_common_resp) +
                                     sizeof(nsm_fabric_manager_state_data));
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto rc = encode_get_fabric_manager_state_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &fmData, responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = fmState.handleResponseMsg(responseMsgPtr, responseMsg.size());

    EXPECT_EQ(result, NSM_SUCCESS);

    auto fabricManagerIntf = fmState.getFabricManagerIntf();
    EXPECT_EQ(fabricManagerIntf->reportStatus(), nsm::FMReportStatus::Received);
    EXPECT_EQ(fabricManagerIntf->fmState(), nsm::FMState::Configured);

    auto operaStatusIntf = fmState.getOperaStatusIntf();
    EXPECT_EQ(operaStatusIntf->state(), nsm::OpState::Enabled);
}

TEST_F(NsmFabricManagerTest, HandleResponseMsg_Success_StateTimeout)
{
    std::string name = "FabricManager0";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate";
    std::string description = "Test Fabric Manager";

    nsm::NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                       inventoryObjPathFM, bus, description);

    uint8_t instanceId = 1;
    struct nsm_fabric_manager_state_data fmData = {
        .fm_state = NSM_FM_STATE_RESERVED_TIMEOUT,
        .report_status = NSM_FM_REPORT_STATUS_RECEIVED,
        .last_restart_timestamp = 4000,
        .duration_since_last_restart_sec = 400};

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_common_resp) +
                                     sizeof(nsm_fabric_manager_state_data));
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto rc = encode_get_fabric_manager_state_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &fmData, responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = fmState.handleResponseMsg(responseMsgPtr, responseMsg.size());

    EXPECT_EQ(result, NSM_SUCCESS);

    auto fabricManagerIntf = fmState.getFabricManagerIntf();
    EXPECT_EQ(fabricManagerIntf->fmState(), nsm::FMState::Timeout);

    auto operaStatusIntf = fmState.getOperaStatusIntf();
    EXPECT_EQ(operaStatusIntf->state(), nsm::OpState::UnavailableOffline);
}

TEST_F(NsmFabricManagerTest, HandleResponseMsg_Success_StateError)
{
    std::string name = "FabricManager0";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate";
    std::string description = "Test Fabric Manager";

    nsm::NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                       inventoryObjPathFM, bus, description);

    uint8_t instanceId = 1;
    struct nsm_fabric_manager_state_data fmData = {
        .fm_state = NSM_FM_STATE_ERROR,
        .report_status = NSM_FM_REPORT_STATUS_RECEIVED,
        .last_restart_timestamp = 5000,
        .duration_since_last_restart_sec = 500};

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_common_resp) +
                                     sizeof(nsm_fabric_manager_state_data));
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto rc = encode_get_fabric_manager_state_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &fmData, responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = fmState.handleResponseMsg(responseMsgPtr, responseMsg.size());

    EXPECT_EQ(result, NSM_SUCCESS);

    auto fabricManagerIntf = fmState.getFabricManagerIntf();
    EXPECT_EQ(fabricManagerIntf->fmState(), nsm::FMState::Error);

    auto operaStatusIntf = fmState.getOperaStatusIntf();
    EXPECT_EQ(operaStatusIntf->state(), nsm::OpState::UnavailableOffline);
}

TEST_F(NsmFabricManagerTest, HandleResponseMsg_ReportStatusNotReceived)
{
    std::string name = "FabricManager0";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate";
    std::string description = "Test Fabric Manager";

    nsm::NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                       inventoryObjPathFM, bus, description);

    uint8_t instanceId = 1;
    struct nsm_fabric_manager_state_data fmData = {
        .fm_state = NSM_FM_STATE_CONFIGURED,
        .report_status = NSM_FM_REPORT_STATUS_NOT_RECEIVED,
        .last_restart_timestamp = 6000,
        .duration_since_last_restart_sec = 600};

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_common_resp) +
                                     sizeof(nsm_fabric_manager_state_data));
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto rc = encode_get_fabric_manager_state_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &fmData, responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = fmState.handleResponseMsg(responseMsgPtr, responseMsg.size());

    EXPECT_EQ(result, NSM_SUCCESS);

    auto fabricManagerIntf = fmState.getFabricManagerIntf();
    EXPECT_EQ(fabricManagerIntf->reportStatus(),
              nsm::FMReportStatus::NotReceived);
    EXPECT_EQ(fabricManagerIntf->fmState(), nsm::FMState::Unknown);

    auto operaStatusIntf = fmState.getOperaStatusIntf();
    EXPECT_EQ(operaStatusIntf->state(), nsm::OpState::StandbyOffline);
}

TEST_F(NsmFabricManagerTest, HandleResponseMsg_ReportStatusTimeout)
{
    std::string name = "FabricManager0";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate";
    std::string description = "Test Fabric Manager";

    nsm::NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                       inventoryObjPathFM, bus, description);

    uint8_t instanceId = 1;
    struct nsm_fabric_manager_state_data fmData = {
        .fm_state = NSM_FM_STATE_CONFIGURED,
        .report_status = NSM_FM_REPORT_STATUS_TIMEOUT,
        .last_restart_timestamp = 7000,
        .duration_since_last_restart_sec = 700};

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_common_resp) +
                                     sizeof(nsm_fabric_manager_state_data));
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto rc = encode_get_fabric_manager_state_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &fmData, responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = fmState.handleResponseMsg(responseMsgPtr, responseMsg.size());

    EXPECT_EQ(result, NSM_SUCCESS);

    auto fabricManagerIntf = fmState.getFabricManagerIntf();
    EXPECT_EQ(fabricManagerIntf->reportStatus(), nsm::FMReportStatus::Timeout);
    EXPECT_EQ(fabricManagerIntf->fmState(), nsm::FMState::Unknown);

    auto operaStatusIntf = fmState.getOperaStatusIntf();
    EXPECT_EQ(operaStatusIntf->state(), nsm::OpState::StandbyOffline);
}

TEST_F(NsmFabricManagerTest, HandleResponseMsg_ErrorCC_ReturnsError)
{
    std::string name = "FabricManager0";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate";
    std::string description = "Test Fabric Manager";

    nsm::NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                       inventoryObjPathFM, bus, description);

    uint8_t instanceId = 1;
    struct nsm_fabric_manager_state_data fmData = {
        .fm_state = NSM_FM_STATE_CONFIGURED,
        .report_status = NSM_FM_REPORT_STATUS_RECEIVED,
        .last_restart_timestamp = 1000,
        .duration_since_last_restart_sec = 100};

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_common_resp) +
                                     sizeof(nsm_fabric_manager_state_data));
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto rc = encode_get_fabric_manager_state_resp(
        instanceId, NSM_ERROR, ERR_NULL, &fmData, responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = fmState.handleResponseMsg(responseMsgPtr, responseMsg.size());

    // Error CC: if-block not entered, returns error
    EXPECT_NE(result, NSM_SUCCESS);
}

TEST_F(NsmFabricManagerTest, HandleResponseMsg_Success_DefaultFmState)
{
    std::string name = "FabricManager0";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate";
    std::string description = "Test Fabric Manager";

    nsm::NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                       inventoryObjPathFM, bus, description);

    uint8_t instanceId = 1;
    // NSM_FM_STATE_RESERVED (0x00) is not in the switch → hits default case
    struct nsm_fabric_manager_state_data fmData = {
        .fm_state = NSM_FM_STATE_RESERVED,
        .report_status = NSM_FM_REPORT_STATUS_RECEIVED,
        .last_restart_timestamp = 8000,
        .duration_since_last_restart_sec = 800};

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_common_resp) +
                                     sizeof(nsm_fabric_manager_state_data));
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto rc = encode_get_fabric_manager_state_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &fmData, responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = fmState.handleResponseMsg(responseMsgPtr, responseMsg.size());

    EXPECT_EQ(result, NSM_SUCCESS);

    auto fabricManagerIntf = fmState.getFabricManagerIntf();
    EXPECT_EQ(fabricManagerIntf->fmState(), nsm::FMState::Unknown);

    auto operaStatusIntf = fmState.getOperaStatusIntf();
    EXPECT_EQ(operaStatusIntf->state(), nsm::OpState::StandbyOffline);
}

TEST_F(NsmFabricManagerTest, HandleResponseMsg_Success_DefaultReportStatus)
{
    std::string name = "FabricManager0";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate";
    std::string description = "Test Fabric Manager";

    nsm::NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                       inventoryObjPathFM, bus, description);

    uint8_t instanceId = 1;
    // NSM_FM_REPORT_STATUS_RESERVED (0x00) is not in the switch → default case
    struct nsm_fabric_manager_state_data fmData = {
        .fm_state = NSM_FM_STATE_CONFIGURED,
        .report_status = NSM_FM_REPORT_STATUS_RESERVED,
        .last_restart_timestamp = 9000,
        .duration_since_last_restart_sec = 900};

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_common_resp) +
                                     sizeof(nsm_fabric_manager_state_data));
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto rc = encode_get_fabric_manager_state_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, &fmData, responseMsgPtr);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = fmState.handleResponseMsg(responseMsgPtr, responseMsg.size());

    EXPECT_EQ(result, NSM_SUCCESS);

    auto fabricManagerIntf = fmState.getFabricManagerIntf();
    EXPECT_EQ(fabricManagerIntf->reportStatus(), nsm::FMReportStatus::Unknown);
    EXPECT_EQ(fabricManagerIntf->fmState(), nsm::FMState::Unknown);

    auto operaStatusIntf = fmState.getOperaStatusIntf();
    EXPECT_EQ(operaStatusIntf->state(), nsm::OpState::StandbyOffline);
}

// Fixture for aggregate aggregation tests. Creates two switch-level interfaces
// and registers them with the aggregate singleton.
class NsmAggregateFabricManagerTest : public NsmFabricManagerTest
{
  protected:
    const std::string aggregatePath =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate";
    const std::string description = "Aggregate Fabric Manager";

    std::shared_ptr<nsm::FabricManagerIntf> sw1FM;
    std::shared_ptr<nsm::OperaStatusIntf> sw1OS;
    std::shared_ptr<nsm::FabricManagerIntf> sw2FM;
    std::shared_ptr<nsm::OperaStatusIntf> sw2OS;
    std::shared_ptr<nsm::NsmAggregateFabricManagerState> aggregate;

    void SetUp() override
    {
        NsmFabricManagerTest::SetUp();

        sw1FM = std::make_shared<nsm::FabricManagerIntf>(
            bus,
            "/xyz/openbmc_project/inventory/system/fabric_manager/Switch0");
        sw1OS = std::make_shared<nsm::OperaStatusIntf>(
            bus,
            "/xyz/openbmc_project/inventory/system/fabric_manager/Switch0");
        sw2FM = std::make_shared<nsm::FabricManagerIntf>(
            bus,
            "/xyz/openbmc_project/inventory/system/fabric_manager/Switch1");
        sw2OS = std::make_shared<nsm::OperaStatusIntf>(
            bus,
            "/xyz/openbmc_project/inventory/system/fabric_manager/Switch1");

        aggregate = nsm::NsmAggregateFabricManagerState::getInstance(
            aggregatePath, sw1FM, sw1OS, description);
        nsm::NsmAggregateFabricManagerState::getInstance(aggregatePath, sw2FM,
                                                         sw2OS, description);
    }
};

// Design doc table row 1: Pending + Pending -> Pending, Unknown
TEST_F(NsmAggregateFabricManagerTest, Aggregate_Pending_Pending)
{
    sw1FM->reportStatus(nsm::FMReportStatus::NotReceived);
    sw1FM->fmState(nsm::FMState::Unknown);
    sw2FM->reportStatus(nsm::FMReportStatus::NotReceived);
    sw2FM->fmState(nsm::FMState::Unknown);

    aggregate->updateAggregateFabricManagerState();

    EXPECT_EQ(aggregate->fabricManagerIntf->reportStatus(),
              nsm::FMReportStatus::NotReceived);
    EXPECT_EQ(aggregate->fabricManagerIntf->fmState(), nsm::FMState::Unknown);
}

// Design doc table row 2: Pending + Received -> Received, switch2 value
TEST_F(NsmAggregateFabricManagerTest, Aggregate_Pending_Received)
{
    sw1FM->reportStatus(nsm::FMReportStatus::NotReceived);
    sw1FM->fmState(nsm::FMState::Unknown);
    sw2FM->reportStatus(nsm::FMReportStatus::Received);
    sw2FM->fmState(nsm::FMState::Standby);

    aggregate->updateAggregateFabricManagerState();

    EXPECT_EQ(aggregate->fabricManagerIntf->reportStatus(),
              nsm::FMReportStatus::Received);
    EXPECT_EQ(aggregate->fabricManagerIntf->fmState(), nsm::FMState::Standby);
}

// Design doc table row 3: Pending + Timeout -> Timeout, Unknown
TEST_F(NsmAggregateFabricManagerTest, Aggregate_Pending_Timeout)
{
    sw1FM->reportStatus(nsm::FMReportStatus::NotReceived);
    sw1FM->fmState(nsm::FMState::Unknown);
    sw2FM->reportStatus(nsm::FMReportStatus::Timeout);
    sw2FM->fmState(nsm::FMState::Unknown);

    aggregate->updateAggregateFabricManagerState();

    EXPECT_EQ(aggregate->fabricManagerIntf->reportStatus(),
              nsm::FMReportStatus::Timeout);
    EXPECT_EQ(aggregate->fabricManagerIntf->fmState(), nsm::FMState::Unknown);
}

// Design doc table row 4: Received + Pending -> Received, switch1 value
TEST_F(NsmAggregateFabricManagerTest, Aggregate_Received_Pending)
{
    sw1FM->reportStatus(nsm::FMReportStatus::Received);
    sw1FM->fmState(nsm::FMState::Configured);
    sw2FM->reportStatus(nsm::FMReportStatus::NotReceived);
    sw2FM->fmState(nsm::FMState::Unknown);

    aggregate->updateAggregateFabricManagerState();

    EXPECT_EQ(aggregate->fabricManagerIntf->reportStatus(),
              nsm::FMReportStatus::Received);
    EXPECT_EQ(aggregate->fabricManagerIntf->fmState(),
              nsm::FMState::Configured);
}

// Design doc table row 5: Received + Received (same) -> Received, that value
TEST_F(NsmAggregateFabricManagerTest, Aggregate_Received_Received_SameState)
{
    sw1FM->reportStatus(nsm::FMReportStatus::Received);
    sw1FM->fmState(nsm::FMState::Configured);
    sw2FM->reportStatus(nsm::FMReportStatus::Received);
    sw2FM->fmState(nsm::FMState::Configured);

    aggregate->updateAggregateFabricManagerState();

    EXPECT_EQ(aggregate->fabricManagerIntf->reportStatus(),
              nsm::FMReportStatus::Received);
    EXPECT_EQ(aggregate->fabricManagerIntf->fmState(),
              nsm::FMState::Configured);
}

// Design doc table row 5 (mismatch): Received + Received (different) ->
// Received, Unknown
TEST_F(NsmAggregateFabricManagerTest,
       Aggregate_Received_Received_DifferentState)
{
    sw1FM->reportStatus(nsm::FMReportStatus::Received);
    sw1FM->fmState(nsm::FMState::Configured);
    sw2FM->reportStatus(nsm::FMReportStatus::Received);
    sw2FM->fmState(nsm::FMState::Standby);

    aggregate->updateAggregateFabricManagerState();

    EXPECT_EQ(aggregate->fabricManagerIntf->reportStatus(),
              nsm::FMReportStatus::Received);
    EXPECT_EQ(aggregate->fabricManagerIntf->fmState(), nsm::FMState::Unknown);
}

// Design doc table row 6: Received + Timeout -> Received, switch1 value
TEST_F(NsmAggregateFabricManagerTest, Aggregate_Received_Timeout)
{
    sw1FM->reportStatus(nsm::FMReportStatus::Received);
    sw1FM->fmState(nsm::FMState::Configured);
    sw2FM->reportStatus(nsm::FMReportStatus::Timeout);
    sw2FM->fmState(nsm::FMState::Unknown);

    aggregate->updateAggregateFabricManagerState();

    EXPECT_EQ(aggregate->fabricManagerIntf->reportStatus(),
              nsm::FMReportStatus::Received);
    EXPECT_EQ(aggregate->fabricManagerIntf->fmState(),
              nsm::FMState::Configured);
}

// Design doc table row 7: Timeout + Pending -> Timeout, Unknown
TEST_F(NsmAggregateFabricManagerTest, Aggregate_Timeout_Pending)
{
    sw1FM->reportStatus(nsm::FMReportStatus::Timeout);
    sw1FM->fmState(nsm::FMState::Unknown);
    sw2FM->reportStatus(nsm::FMReportStatus::NotReceived);
    sw2FM->fmState(nsm::FMState::Unknown);

    aggregate->updateAggregateFabricManagerState();

    EXPECT_EQ(aggregate->fabricManagerIntf->reportStatus(),
              nsm::FMReportStatus::Timeout);
    EXPECT_EQ(aggregate->fabricManagerIntf->fmState(), nsm::FMState::Unknown);
}

// Design doc table row 8: Timeout + Received -> Received, switch2 value
TEST_F(NsmAggregateFabricManagerTest, Aggregate_Timeout_Received)
{
    sw1FM->reportStatus(nsm::FMReportStatus::Timeout);
    sw1FM->fmState(nsm::FMState::Unknown);
    sw2FM->reportStatus(nsm::FMReportStatus::Received);
    sw2FM->fmState(nsm::FMState::Standby);

    aggregate->updateAggregateFabricManagerState();

    EXPECT_EQ(aggregate->fabricManagerIntf->reportStatus(),
              nsm::FMReportStatus::Received);
    EXPECT_EQ(aggregate->fabricManagerIntf->fmState(), nsm::FMState::Standby);
}

// Design doc table row 9: Timeout + Timeout -> Timeout, Unknown
TEST_F(NsmAggregateFabricManagerTest, Aggregate_Timeout_Timeout)
{
    sw1FM->reportStatus(nsm::FMReportStatus::Timeout);
    sw1FM->fmState(nsm::FMState::Unknown);
    sw2FM->reportStatus(nsm::FMReportStatus::Timeout);
    sw2FM->fmState(nsm::FMState::Unknown);

    aggregate->updateAggregateFabricManagerState();

    EXPECT_EQ(aggregate->fabricManagerIntf->reportStatus(),
              nsm::FMReportStatus::Timeout);
    EXPECT_EQ(aggregate->fabricManagerIntf->fmState(), nsm::FMState::Unknown);
}

TEST_F(NsmFabricManagerTest, AggregateFabricManager_SingletonBehavior)
{
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate";
    std::string description = "Aggregate Fabric Manager";

    auto fabricManagerIntf1 = std::make_shared<nsm::FabricManagerIntf>(
        bus, "/xyz/openbmc_project/inventory/system/fabric_manager/FM0");
    auto operaStatusIntf1 = std::make_shared<nsm::OperaStatusIntf>(
        bus, "/xyz/openbmc_project/inventory/system/fabric_manager/FM0");

    auto instance1 = nsm::NsmAggregateFabricManagerState::getInstance(
        inventoryObjPathFM, fabricManagerIntf1, operaStatusIntf1, description);

    ASSERT_NE(instance1, nullptr);

    auto fabricManagerIntf2 = std::make_shared<nsm::FabricManagerIntf>(
        bus, "/xyz/openbmc_project/inventory/system/fabric_manager/FM1");
    auto operaStatusIntf2 = std::make_shared<nsm::OperaStatusIntf>(
        bus, "/xyz/openbmc_project/inventory/system/fabric_manager/FM1");

    auto instance2 = nsm::NsmAggregateFabricManagerState::getInstance(
        inventoryObjPathFM, fabricManagerIntf2, operaStatusIntf2, description);

    // Should return the same singleton instance
    EXPECT_EQ(instance1, instance2);

    // Both interfaces should be registered
    EXPECT_EQ(nsm::NsmAggregateFabricManagerState::associatedFabricManagerIntfs
                  .size(),
              2);
    EXPECT_EQ(
        nsm::NsmAggregateFabricManagerState::associatedOperationalStatusIntfs
            .size(),
        2);
}

TEST_F(NsmFabricManagerTest, HandleResponseMsg_DecodeFail_ReturnsError)
{
    std::string name = "FabricManager0";
    std::string type = "FabricManager";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fabric_manager/";
    std::string inventoryObjPathFM =
        "/xyz/openbmc_project/inventory/system/fabric_manager_aggregate";
    std::string description = "Test Fabric Manager";

    nsm::NsmFabricManagerState fmState(name, type, inventoryObjPath,
                                       inventoryObjPathFM, bus, description);

    // 7-byte buffer: safely reads cc=0 at payload[1] but too short for
    // nsm_get_fabric_manager_state_resp so decode returns NSM_SW_ERROR_LENGTH
    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) + 2, 0);
    auto responseMsgPtr = reinterpret_cast<struct nsm_msg*>(responseMsg.data());

    auto result = fmState.handleResponseMsg(responseMsgPtr, responseMsg.size());

    EXPECT_NE(result, NSM_SUCCESS);
}

// ==========================================================================
// SECTION: updateAggregateFabricManagerState multi-FM branch coverage
//
// Each test creates two NsmFabricManagerState objects sharing the same
// aggregate path, then exercises specific branches inside
// updateAggregateFabricManagerState() that require more than one associated
// FM to reach.
// ==========================================================================

// Helper: build a valid Received response with the given fm_state
static std::vector<uint8_t> makeReceivedResponse(uint8_t fmState,
                                                 uint16_t timestamp = 100,
                                                 uint16_t duration = 10)
{
    struct nsm_fabric_manager_state_data d = {
        .fm_state = fmState,
        .report_status = NSM_FM_REPORT_STATUS_RECEIVED,
        .last_restart_timestamp = timestamp,
        .duration_since_last_restart_sec = duration};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                             sizeof(nsm_fabric_manager_state_data));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_fabric_manager_state_resp(1, NSM_SUCCESS, ERR_NULL, &d, msg);
    return buf;
}

// Branch: associatedFabricManagerIntfs[idx]->reportStatus() != Received →
// continue Exercises the "skip not-received FMs" early-continue in the
// for-loop.
TEST_F(NsmFabricManagerTest, UpdateAggregate_OneNotReceived_ContinuesBranch)
{
    // Two FMs share the same aggregate path.
    std::string type = "FabricManager";
    std::string name0 = "FM0", name1 = "FM1";
    std::string path0 = "/test/multi0/fm0/";
    std::string path1 = "/test/multi0/fm1/";
    std::string pathFM = "/test/multi0/aggregate";
    std::string desc = "Test FM";

    nsm::NsmFabricManagerState fm0(name0, type, path0, pathFM, bus, desc);
    nsm::NsmFabricManagerState fm1(name1, type, path1, pathFM, bus, desc);

    EXPECT_EQ(nsm::NsmAggregateFabricManagerState::associatedFabricManagerIntfs
                  .size(),
              2u);

    // Send a Received/Offline response only for FM0.
    // updateAggregateFabricManagerState() will:
    //   idx=0 (FM0): reportStatus==Received → NOT != Received → else branch
    //   idx=1 (FM1): reportStatus==NotReceived → != Received → CONTINUE ←
    //   target
    auto resp = makeReceivedResponse(NSM_FM_STATE_OFFLINE);
    auto* respPtr = reinterpret_cast<nsm_msg*>(resp.data());
    auto result = fm0.handleResponseMsg(respPtr, resp.size());
    EXPECT_EQ(result, NSM_SUCCESS);
}

// Branch: reportStatus == Received (second received FM, same fmState) →
// continue Exercises the inner-if TRUE path with matching states in the second
// iteration.
TEST_F(NsmFabricManagerTest,
       UpdateAggregate_TwoReceived_SameState_ContinuesBranch)
{
    std::string type = "FabricManager";
    std::string name0 = "FM0", name1 = "FM1";
    std::string path0 = "/test/multi1/fm0/";
    std::string path1 = "/test/multi1/fm1/";
    std::string pathFM = "/test/multi1/aggregate";
    std::string desc = "Test FM";

    nsm::NsmFabricManagerState fm0(name0, type, path0, pathFM, bus, desc);
    nsm::NsmFabricManagerState fm1(name1, type, path1, pathFM, bus, desc);

    // Set FM0 to Received/Offline.
    auto resp = makeReceivedResponse(NSM_FM_STATE_OFFLINE);
    auto* respPtr = reinterpret_cast<nsm_msg*>(resp.data());
    fm0.handleResponseMsg(respPtr, resp.size());

    // Now set FM1 to Received/Offline (same state as FM0).
    // updateAggregateFabricManagerState():
    //   idx=0 (FM0): → else (first received, sets fmState=Offline)
    //   idx=1 (FM1): → inner if(Received==Received) TRUE
    //                → fmState(Offline)==FM1->fmState(Offline) → CONTINUE ←
    //                target
    fm1.handleResponseMsg(respPtr, resp.size());
}

// Branch: reportStatus == Received, fmState != FM[idx]->fmState → Unknown +
// break Exercises the inner-else path when two received FMs disagree on state.
TEST_F(NsmFabricManagerTest,
       UpdateAggregate_TwoReceived_DifferentStates_UnknownBranch)
{
    std::string type = "FabricManager";
    std::string name0 = "FM0", name1 = "FM1";
    std::string path0 = "/test/multi2/fm0/";
    std::string path1 = "/test/multi2/fm1/";
    std::string pathFM = "/test/multi2/aggregate";
    std::string desc = "Test FM";

    nsm::NsmFabricManagerState fm0(name0, type, path0, pathFM, bus, desc);
    nsm::NsmFabricManagerState fm1(name1, type, path1, pathFM, bus, desc);

    // Set FM0 to Received/Offline.
    auto resp0 = makeReceivedResponse(NSM_FM_STATE_OFFLINE);
    auto* respPtr0 = reinterpret_cast<nsm_msg*>(resp0.data());
    fm0.handleResponseMsg(respPtr0, resp0.size());

    // Set FM1 to Received/Configured (DIFFERENT state).
    // updateAggregateFabricManagerState():
    //   idx=0 (FM0): → else → sets fmState=Offline
    //   idx=1 (FM1): → inner if(Received==Received) TRUE
    //                → fmState(Offline) != FM1->fmState(Configured)
    //                → else: fmState=Unknown + break ← target
    auto resp1 = makeReceivedResponse(NSM_FM_STATE_CONFIGURED);
    auto* respPtr1 = reinterpret_cast<nsm_msg*>(resp1.data());
    fm1.handleResponseMsg(respPtr1, resp1.size());
}

// ==========================================================================
// SECTION: NsmAggregateFabricManagerState::getInstance path-mismatch branch
// ==========================================================================

// Branch: getInstance called a second time with a different inventoryObjPath
// → else-if (inventoryObjPath != aggregateFMObjPath) → logs error, returns
// singleton
TEST_F(NsmFabricManagerTest, GetInstance_PathMismatch_LogsError)
{
    // First call: create singleton at pathA.
    auto fmIntf0 = std::make_shared<nsm::FabricManagerIntf>(bus,
                                                            "/test/fm_mm/fm0");
    auto opIntf0 = std::make_shared<nsm::OperaStatusIntf>(bus,
                                                          "/test/fm_mm/fm0");
    auto inst1 = nsm::NsmAggregateFabricManagerState::getInstance(
        "/test/fm_mm/pathA", fmIntf0, opIntf0, "desc");
    ASSERT_NE(inst1, nullptr);
    EXPECT_EQ(nsm::NsmAggregateFabricManagerState::aggregateFMObjPath,
              "/test/fm_mm/pathA");

    // Second call with a DIFFERENT path → mismatch branch: error logged,
    // singleton unchanged, new intfs still pushed.
    auto fmIntf1 = std::make_shared<nsm::FabricManagerIntf>(bus,
                                                            "/test/fm_mm/fm1");
    auto opIntf1 = std::make_shared<nsm::OperaStatusIntf>(bus,
                                                          "/test/fm_mm/fm1");
    auto inst2 = nsm::NsmAggregateFabricManagerState::getInstance(
        "/test/fm_mm/pathB", fmIntf1, opIntf1, "desc");

    // Same singleton is returned.
    EXPECT_EQ(inst1, inst2);
    // Aggregate path stays as pathA (not updated on mismatch).
    EXPECT_EQ(nsm::NsmAggregateFabricManagerState::aggregateFMObjPath,
              "/test/fm_mm/pathA");
    // Both intfs were registered despite mismatch.
    EXPECT_EQ(nsm::NsmAggregateFabricManagerState::associatedFabricManagerIntfs
                  .size(),
              2u);
}

// handleResponseMsg: rc==NSM_SW_SUCCESS, cc==NSM_ERROR →
// `if (rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS)` false branch →
// state NOT updated.
TEST_F(NsmFabricManagerTest,
       HandleResponseMsg_DecodeSuccessNonZeroCC_SkipsUpdate)
{
    std::string fmName = "FM_ccbranch";
    std::string fmType = "FabricManager";
    std::string fmPath = "/xyz/openbmc_project/inventory/system/fm_ccbranch/";
    std::string fmAgg =
        "/xyz/openbmc_project/inventory/system/fm_aggregate_ccbranch";
    std::string fmDesc = "FM branch test";
    nsm::NsmFabricManagerState fmState(fmName, fmType, fmPath, fmAgg, bus,
                                       fmDesc);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = fmState.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

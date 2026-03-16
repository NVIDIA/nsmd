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

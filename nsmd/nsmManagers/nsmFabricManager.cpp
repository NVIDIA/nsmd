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

#include "nsmFabricManager.hpp"

#include "dBusAsyncUtils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{

NsmFabricManagerState::NsmFabricManagerState(
    const std::string& name, const std::string& type,
    std::string& inventoryObjPath,
    std::shared_ptr<FabricManagerIntf> fabricMgrIntf,
    std::shared_ptr<OperaStatusIntf> opStateIntf,
    std::shared_ptr<ManagementServiceIntf> mgmtSerIntf) :
    NsmObject(name, type),
    fabricManagerIntf(fabricMgrIntf), operationalStatusIntf(opStateIntf),
    managementServiceIntf(mgmtSerIntf)
{
    lg2::debug("NsmFabricManagerState: {NAME}", "NAME", name.c_str());
    objPath = inventoryObjPath + name;
}

requester::Coroutine NsmFabricManagerState::update(SensorManager& manager,
                                                   eid_t eid)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_fabric_manager_state_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_fabric_manager_state_req(0, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_fabric_manager_state_req failed. eid={EID} rc={RC} obj={OBJ}",
            "EID", eid, "RC", rc, "OBJ", objPath);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::error(
            "NsmFabricManagerState SendRecvNsmMsg failed for {EID} eid with rc={RC}",
            "EID", eid, "RC", rc);
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataLen = 0;
    struct nsm_fabric_manager_state_data fmStateData;

    rc = decode_get_fabric_manager_state_resp(responseMsg.get(), responseLen,
                                              &cc, &reason_code, &dataLen,
                                              &fmStateData);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        // update values
        switch (fmStateData.fm_state)
        {
            case NSM_FM_STATE_OFFLINE:
                fabricManagerIntf->fmState(FMState::Offline);
                operationalStatusIntf->state(OpState::Starting);
                break;
            case NSM_FM_STATE_STANDBY:
                fabricManagerIntf->fmState(FMState::Standby);
                operationalStatusIntf->state(OpState::StandbyOffline);
                break;
            case NSM_FM_STATE_CONFIGURED:
                fabricManagerIntf->fmState(FMState::Configured);
                operationalStatusIntf->state(OpState::Enabled);
                break;
            case NSM_FM_STATE_RESERVED_TIMEOUT:
                fabricManagerIntf->fmState(FMState::Timeout);
                operationalStatusIntf->state(OpState::UnavailableOffline);
                break;
            case NSM_FM_STATE_ERROR:
                fabricManagerIntf->fmState(FMState::Error);
                operationalStatusIntf->state(OpState::UnavailableOffline);
                break;
            default:
                fabricManagerIntf->fmState(FMState::Unknown);
                operationalStatusIntf->state(OpState::None);
                break;
        }

        switch (fmStateData.report_status)
        {
            case NSM_FM_REPORT_STATUS_NOT_RECEIVED:
                fabricManagerIntf->reportStatus(FMReportStatus::NotReceived);
                break;
            case NSM_FM_REPORT_STATUS_RECEIVED:
                fabricManagerIntf->reportStatus(FMReportStatus::Received);
                break;
            case NSM_FM_REPORT_STATUS_TIMEOUT:
                fabricManagerIntf->reportStatus(FMReportStatus::Timeout);
                break;
            default:
                fabricManagerIntf->reportStatus(FMReportStatus::Unknown);
                break;
        }

        fabricManagerIntf->lastRestartTime(fmStateData.last_restart_timestamp);
        fabricManagerIntf->lastRestartDuration(
            fmStateData.duration_since_last_restart_sec);

        co_return NSM_SW_SUCCESS;
    }

    lg2::error(
        "responseHandler: decode_get_fabric_manager_state_resp unsuccessfull for {EID} eid. reasonCode={RSNCOD} cc={CC} rc={RC}",
        "EID", eid, "RSNCOD", reason_code, "CC", cc, "RC", rc);

    co_return NSM_SW_ERROR_COMMAND_FAIL;
}
} // namespace nsm

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
#include "interfaceWrapper.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <string>
#include <vector>

namespace nsm
{

std::shared_ptr<NsmAggregateFabricManagerState>
    NsmAggregateFabricManagerState::nsmAggregateFabricManagerState;
std::vector<std::shared_ptr<OperaStatusIntf>>
    NsmAggregateFabricManagerState::associatedOperationalStatusIntfs(0);
std::vector<std::shared_ptr<FabricManagerIntf>>
    NsmAggregateFabricManagerState::associatedFabricManagerIntfs(0);
std::string NsmAggregateFabricManagerState::aggregateFMObjPath("");

NsmAggregateFabricManagerState::NsmAggregateFabricManagerState(
    const std::string& inventoryObjPath, const std::string& description)
{
    auto& bus = utils::DBusHandler::getBus();
    fabricManagerIntf =
        std::make_shared<FabricManagerIntf>(bus, inventoryObjPath.c_str());
    operationalStatusIntf =
        std::make_shared<OperationalStatusIntf>(bus, inventoryObjPath.c_str());
    managementServiceIntf =
        std::make_shared<ManagementServiceIntf>(bus, inventoryObjPath.c_str());
    itemIntf = std::make_shared<ItemIntf>(bus, inventoryObjPath.c_str());
    itemIntf->description(description);
    itemIntf->prettyName("");
}

std::shared_ptr<NsmAggregateFabricManagerState>
    NsmAggregateFabricManagerState::getInstance(
        const std::string& inventoryObjPath,
        std::shared_ptr<FabricManagerIntf> associatedFabricManagerIntf,
        std::shared_ptr<OperaStatusIntf> associatedOperationalStatusIntf,
        const std::string& description)
{
    if (!nsmAggregateFabricManagerState)
    {
        nsmAggregateFabricManagerState =
            std::shared_ptr<NsmAggregateFabricManagerState>(
                new NsmAggregateFabricManagerState(inventoryObjPath,
                                                   description));
        aggregateFMObjPath = inventoryObjPath;
    }
    else if (inventoryObjPath != aggregateFMObjPath)
    {
        lg2::error(
            "InventoryObjPath mismatch in NvSwitches config file, existing FabricManager path: {EXISTING_PATH}, provided FabricManager path: {PROVIDED_PATH}",
            "EXISTING_PATH", aggregateFMObjPath, "PROVIDED_PATH",
            inventoryObjPath);
    }
    associatedFabricManagerIntfs.push_back(associatedFabricManagerIntf);
    associatedOperationalStatusIntfs.push_back(associatedOperationalStatusIntf);
    return nsmAggregateFabricManagerState;
}

void NsmAggregateFabricManagerState::updateAggregateFabricManagerState()
{
    size_t idx;
    FMState fmState = FMState::Unknown;
    FMReportStatus reportStatus = FMReportStatus::Unknown;
    OpState state = OpState::StandbyOffline;
    uint16_t lastRestartTime = 0;
    uint16_t lastRestartDuration = 0;

    for (idx = 0; idx < associatedFabricManagerIntfs.size(); idx++)
    {
        if (associatedFabricManagerIntfs[idx]->reportStatus() !=
            FMReportStatus::Received)
        {
            continue;
        }
        if (reportStatus == FMReportStatus::Received)
        {
            if (fmState == associatedFabricManagerIntfs[idx]->fmState())
            {
                continue;
            }
            else
            {
                fmState = FMState::Unknown;
                break;
            }
        }
        else
        {
            reportStatus = associatedFabricManagerIntfs[idx]->reportStatus();
            fmState = associatedFabricManagerIntfs[idx]->fmState();
            lastRestartTime =
                associatedFabricManagerIntfs[idx]->lastRestartTime();
            lastRestartDuration =
                associatedFabricManagerIntfs[idx]->lastRestartDuration();
        }
    }

    fabricManagerIntf->reportStatus(reportStatus);
    fabricManagerIntf->fmState(fmState);
    fabricManagerIntf->lastRestartTime(lastRestartTime);
    fabricManagerIntf->lastRestartDuration(lastRestartDuration);
    operationalStatusIntf->state(state);
}

NsmFabricManagerState::NsmFabricManagerState(const std::string& name,
                                             const std::string& type,
                                             std::string& inventoryObjPath,
                                             std::string& inventoryObjPathFM,
                                             sdbusplus::bus_t& bus,
                                             std::string& description) :
    NsmSensor(name, type)
{
    lg2::info("NsmFabricManagerState: {NAME}", "NAME", name.c_str());
    objPath = inventoryObjPath + name;

    fabricManagerIntf = std::make_shared<FabricManagerIntf>(bus,
                                                            objPath.c_str());

    operationalStatusIntf = std::make_shared<OperaStatusIntf>(bus,
                                                              objPath.c_str());
    nsmAggregateFabricManagerState =
        NsmAggregateFabricManagerState::getInstance(
            inventoryObjPathFM, fabricManagerIntf, operationalStatusIntf,
            description);
}

std::shared_ptr<FabricManagerIntf> NsmFabricManagerState::getFabricManagerIntf()
{
    return fabricManagerIntf;
}

std::shared_ptr<OperaStatusIntf> NsmFabricManagerState::getOperaStatusIntf()
{
    return operationalStatusIntf;
}

std::shared_ptr<NsmAggregateFabricManagerState>
    NsmFabricManagerState::getAggregateFabricManagerState()
{
    return nsmAggregateFabricManagerState;
}

std::optional<std::vector<uint8_t>>
    NsmFabricManagerState::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_fabric_manager_state_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_fabric_manager_state_req(instanceId, requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_fabric_manager_state_req failed. eid={EID} rc={RC} obj={OBJ}",
            "EID", eid, "RC", rc, "OBJ", objPath);
        return std::nullopt;
    }

    return request;
}

uint8_t
    NsmFabricManagerState::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataLen = 0;
    struct nsm_fabric_manager_state_data fmStateData;
    auto rc = decode_get_fabric_manager_state_resp(
        responseMsg, responseLen, &cc, &reason_code, &dataLen, &fmStateData);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        // update values
        switch (fmStateData.report_status)
        {
            case NSM_FM_REPORT_STATUS_NOT_RECEIVED:
            {
                // Report not received yet hence ignore
                fabricManagerIntf->reportStatus(FMReportStatus::NotReceived);
                break;
            }
            case NSM_FM_REPORT_STATUS_RECEIVED:
            {
                // Report received hence update everything
                fabricManagerIntf->reportStatus(FMReportStatus::Received);

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
                        operationalStatusIntf->state(
                            OpState::UnavailableOffline);
                        break;
                    case NSM_FM_STATE_ERROR:
                        fabricManagerIntf->fmState(FMState::Error);
                        operationalStatusIntf->state(
                            OpState::UnavailableOffline);
                        break;
                    default:
                        fabricManagerIntf->fmState(FMState::Unknown);
                        operationalStatusIntf->state(OpState::StandbyOffline);
                        break;
                }

                fabricManagerIntf->lastRestartTime(
                    fmStateData.last_restart_timestamp);
                fabricManagerIntf->lastRestartDuration(
                    fmStateData.duration_since_last_restart_sec);
                break;
            }
            case NSM_FM_REPORT_STATUS_TIMEOUT:
            {
                // Report timedout hence update to unknown and ignore timestamps
                fabricManagerIntf->reportStatus(FMReportStatus::Timeout);
                fabricManagerIntf->fmState(FMState::Unknown);
                operationalStatusIntf->state(OpState::StandbyOffline);
                break;
            }
            default:
                fabricManagerIntf->reportStatus(FMReportStatus::Unknown);
                fabricManagerIntf->fmState(FMState::Unknown);
                operationalStatusIntf->state(OpState::StandbyOffline);
                break;
        }
        nsmAggregateFabricManagerState->updateAggregateFabricManagerState();
        clearErrorBitMap("decode_get_fabric_manager_state_resp");
        return NSM_SW_SUCCESS;
    }

    logHandleResponseMsg("decode_get_fabric_manager_state_resp", reason_code,
                         cc, rc);
    return NSM_SW_ERROR_COMMAND_FAIL;
}
} // namespace nsm

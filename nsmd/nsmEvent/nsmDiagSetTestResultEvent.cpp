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

#include "nsmDiagSetTestResultEvent.hpp"

#include "diagnostics.h"

#include "dBusAsyncUtils.hpp"
#include "nsmDevice.hpp"
#include "nsmPreBootDiagStateClient.hpp"
#include "sensorManager.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmDiagSetTestResultEvent::NsmDiagSetTestResultEvent(
    const std::string& name, const std::string& type,
    std::weak_ptr<NsmDevice> device) : NsmEvent(name, type), device(device)
{}

int NsmDiagSetTestResultEvent::handle(eid_t eid, NsmType /*type*/,
                                      NsmEventId /*eventId*/,
                                      const nsm_msg* event, size_t eventLen)
{
    uint8_t eventClass{};
    uint16_t eventState{};
    uint8_t tid{};
    uint16_t testErrorCode{};
    uint8_t dynamicDataSize{};
    uint8_t dynamicData[NSM_DIAG_MAX_DYNAMIC_DATA_SIZE]{};

    auto nsmDevice = device.lock();

    auto rc = decode_nsm_diag_set_test_result_event(
        event, eventLen, &eventClass, &eventState, &tid, &testErrorCode,
        &dynamicDataSize, dynamicData);
    if (rc != NSM_SW_SUCCESS)
    {
        if (!nsmDevice ||
            nsmDevice->shouldLog("PreBootDiag:decodeSetTestResult",
                                 nsm_sw_codes(rc)))
        {
            lg2::error(
                "PreBootDiag: decode setDiagTestResult failed rc={RC} EID={EID}",
                "RC", rc, "EID", eid);
        }
        return rc;
    }

    nlohmann::json result;
    result["Tid"] = static_cast<int>(tid);
    result["Eid"] = static_cast<int>(eid);
    result["Result"] = static_cast<int>(testErrorCode);
    result["ResultMaskSize"] = static_cast<int>(dynamicDataSize);
    nlohmann::json maskArray = nlohmann::json::array();
    for (uint8_t i = 0; i < dynamicDataSize; ++i)
    {
        maskArray.push_back(static_cast<int>(dynamicData[i]));
    }
    result["ResultMask"] = maskArray;

    postStateUpdate(preBootDiagState::resultReceived, result.dump());
    lg2::info("PreBootDiag: ResultReceived TID={TID} errorCode={ERR} EID={EID}",
              "TID", lg2::hex, tid, "ERR", lg2::hex, testErrorCode, "EID", eid);
    return NSM_SW_SUCCESS;
}

static requester::Coroutine
    createNsmDiagSetTestResultEvent(SensorManager& manager,
                                    const std::string& interface,
                                    const std::string& objPath)
{
    auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

    uuid_t uuid{};
    if (allCurrentIfaceProperties.count("UUID"))
    {
        uuid = std::get<uuid_t>(allCurrentIfaceProperties.at("UUID"));
    }
    std::string name{};
    if (allCurrentIfaceProperties.count("Name"))
    {
        name = std::get<std::string>(allCurrentIfaceProperties.at("Name"));
    }
    name = utils::makeDBusNameValid(name);

    auto type = interface.substr(interface.find_last_of('.') + 1);
    auto nsmDevice = manager.getNsmDeviceFromStaticUUID(uuid);
    if (!nsmDevice)
    {
        lg2::error(
            "PreBootDiag: SetTestResult PDI matches no NsmDevice UUID={UUID} NAME={NAME}",
            "UUID", uuid, "NAME", name);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    auto event = std::make_shared<NsmDiagSetTestResultEvent>(name, type,
                                                             nsmDevice);
    nsmDevice->addDeviceEvent(event, NSM_TYPE_DIAGNOSTIC,
                              NSM_DIAG_SET_TEST_RESULT_EVENT);
    lg2::info(
        "PreBootDiag: Registered SetTestResult event UUID={UUID} NAME={NAME}",
        "UUID", uuid, "NAME", name);
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmDiagSetTestResultEvent,
    "xyz.openbmc_project.Configuration.NSM_Event_DiagSetTestResult");

} // namespace nsm

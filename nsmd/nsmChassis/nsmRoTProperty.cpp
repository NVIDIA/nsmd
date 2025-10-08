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

#include "nsmRoTProperty.hpp"

#include "nsmFirmwareUtilsCommon.hpp"
#include "sensorManager.hpp"

namespace nsm
{

void NsmInbandUpdatePolicy::updatePolicy(bool state)
{
    const auto [objPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    if (objPath.empty())
    {
        throw Common::Error::Unavailable();
    }
    auto request = std::make_shared<Request>(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_firmware_set_rot_property_req_command));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    struct ::nsm_firmware_set_rot_property_req nsm_req;

    nsm_req.component_classification = htole16(classification);
    nsm_req.component_classification_index = index;
    nsm_req.component_identifier = htole16(identifier);
    // Property 1: In-band Update Policy + Lifespan
    nsm_req.property = NSM_ROT_PROPERTY_INBAND_UPDATE_POLICY;

    if (state)
    {
        nsm_req.argument_data[0] = NSM_ROT_INBAND_UPDATE_POLICY_ENABLE;
    }
    else
    {
        nsm_req.argument_data[0] = NSM_ROT_INBAND_UPDATE_POLICY_DISABLE;
    }

    nsm_req.argument_data[1] = NSM_ROT_INBAND_UPDATE_POLICY_LIFESPAN_PERSISTENT;

    nsm_req.argument_length = ARGUMENT_DATA_LENGTH;

    auto rc = encode_nsm_firmware_set_rot_property_req(0, &nsm_req, requestMsg);

    if (rc == NSM_SW_SUCCESS)
    {
        // reset the error code before sending the request
        errorCode(std::make_tuple(NSM_SW_SUCCESS, ""));
        policyAsyncHandler(request, statusIntf, valueIntf).detach();
        return;
    }
    lg2::error("encode_nsm_firmware_set_rot_property_req failed: rc={RC}", "RC",
               rc);

    auto error = getErrorCode(NSM_FW_SET_ROT_PROPERTY, rc);
    valueIntf->value(error);
    errorCode(error);

    if (rc == NSM_ERR_INVALID_DATA)
    {
        statusIntf->status(AsyncOperationStatusType::WriteFailure);
        throw Common::Error::InvalidArgument();
    }
    statusIntf->status(AsyncOperationStatusType::InternalFailure);
    throw Common::Error::InternalFailure();
}

requester::Coroutine NsmInbandUpdatePolicy::policyAsyncHandler(
    std::shared_ptr<Request> request,
    std::shared_ptr<AsyncStatusIntf> statusIntf,
    std::shared_ptr<AsyncValueIntf> valueIntf)
{
    SensorManager& manager = SensorManager::getInstance();
    auto device = manager.getNsmDeviceFromStaticUUID(uuid);
    auto eid = device->getEid();
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    uint8_t cc = 0;
    uint16_t reasonCode = 0;
    auto rc = co_await device->postPatchIO(eid, *request, responseMsg,
                                           responseLen);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("policyAsyncHandler: postPatchNsmCommand error :"
                   " eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        auto error = getErrorCode(NSM_FW_SET_ROT_PROPERTY, rc);
        valueIntf->value(error);
        errorCode(error);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        co_return rc;
    }

    rc = decode_nsm_firmware_set_rot_property_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode);

    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "decode_nsm_firmware_set_rot_property_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);

        auto error = getErrorCode(NSM_FW_SET_ROT_PROPERTY, cc, reasonCode);
        valueIntf->value(error);
        errorCode(error);
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        co_return NSM_SW_ERROR;
    }

    rc = co_await nsmSensor.update(device);
    statusIntf->status(rc == NSM_SW_SUCCESS
                           ? AsyncOperationStatusType::Success
                           : AsyncOperationStatusType::InternalFailure);
    co_return rc;
}

void NsmInbandUpdatePolicy::updateProperties(
    const struct ::nsm_firmware_erot_state_info_resp& erot_info)
{
    using InbandPolicyState =
        sdbusplus::common::com::nvidia::InbandUpdatePolicy::InbandPolicyState;

    InbandPolicyState policyState = InbandPolicyState::NotApplicable;
    switch (erot_info.fq_resp_hdr.inband_update_policy)
    {
        case NSM_ROT_INBAND_UPDATE_POLICY_ENABLE:
            policyState = InbandPolicyState::Enabled;
            break;
        case NSM_ROT_INBAND_UPDATE_POLICY_DISABLE:
            policyState = InbandPolicyState::Disabled;
            break;
        default:
            policyState = InbandPolicyState::NotApplicable;
            break;
    }

    inbandUpdatePolicy(policyState);
}

NsmInbandUpdatePolicyObject::NsmInbandUpdatePolicyObject(
    sdbusplus::bus::bus& bus, const std::string& chassisName,
    const uuid_t& uuid, uint16_t classificationIn, uint16_t identifierIn,
    uint8_t indexIn) :
    NsmSensor(chassisName, "NSM_ChassisRoT"), objectPath(getPath(chassisName)),
    classification(classificationIn), identifier(identifierIn), index(indexIn)
{
    lg2::info("NsmInbandUpdatePolicy: create object: {PATH}", "PATH",
              objectPath.c_str());
    nsmInbandUpdatePolicy = std::make_unique<NsmInbandUpdatePolicy>(
        bus, objectPath, uuid, classification, identifier, index, *this);
}

std::optional<std::vector<uint8_t>>
    NsmInbandUpdatePolicyObject::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_get_erot_state_info_req));
    struct ::nsm_firmware_erot_state_info_req erot_req = {};
    erot_req.component_classification = htole16(classification);
    erot_req.component_classification_index = index;
    erot_req.component_identifier = htole16(identifier);
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_nsm_query_get_erot_state_parameters_req(
        instanceId, &erot_req, requestMsg);
    if (rc)
    {
        lg2::error("encode_nsm_query_get_erot_state_parameters_req failed."
                   " eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmInbandUpdatePolicyObject::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    nsm_firmware_erot_state_info_resp erot_info = {};

    auto rc = decode_nsm_query_get_erot_state_parameters_resp(
        responseMsg, responseLen, &cc, &reason_code, &erot_info);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        auto error = getErrorCode(NSM_FW_SET_ROT_PROPERTY, cc, reason_code);
        nsmInbandUpdatePolicy->errorCode(error);
        lg2::error(
            "Response message error: rc={RC}, cc={CC}, reasonCode={REASONCODE}",
            "RC", rc, "CC", (int)cc, "REASONCODE", (int)reason_code);
        free(erot_info.slot_info);
        return cc;
    }

    nsmInbandUpdatePolicy->updateProperties(erot_info);
    free(erot_info.slot_info);
    return cc ? cc : rc;
}

} // namespace nsm

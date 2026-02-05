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

/**
 * @brief Helper function to map NSM reason codes to ImageCopy ErrorCode enum
 *
 * Maps the NSM firmware reason codes to the D-Bus ImageCopy interface ErrorCode
 * enumeration values.
 *
 * Error code mappings:
 * - 0x103: NoBootComplete - RoT has not received boot complete from AP
 * - 0x104: UpdateInProgress - A firmware update is currently in progress
 * - 0x105: ImageCopyInProgress - An image copy is currently in progress
 * - 0x106: ImageCopyCompleted - An image copy was completed successfully
 * - 0x107: FlashWearMitigation - Flash wear mitigation policy is active
 * - 0x108: IncompleteComponentSet - Additional components required in request
 *
 * @param reasonCode The reason code from the NSM response
 * @return sdbusplus::common::com::nvidia::ImageCopy::ErrorCode
 */
static sdbusplus::common::com::nvidia::ImageCopy::ErrorCode
    mapReasonCodeToErrorCode(uint16_t reasonCode)
{
    using ErrorCode = sdbusplus::common::com::nvidia::ImageCopy::ErrorCode;

    switch (reasonCode)
    {
        case ERR_NO_BOOT_COMPLETE:
            return ErrorCode::NoBootComplete;
        case ERR_UPDATE_IN_PROGRESS:
            return ErrorCode::UpdateInProgress;
        case ERR_IMAGE_COPY_IN_PROGRESS:
            return ErrorCode::ImageCopyInProgress;
        case ERR_IMAGE_COPY_COMPLETED:
            return ErrorCode::ImageCopyCompleted;
        case ERR_FLASH_WEAR_MITIGATION:
            return ErrorCode::FlashWearMitigation;
        case ERR_INCOMPLETE_COMPONENT_SET:
            return ErrorCode::IncompleteComponentSet;
        case ERR_NULL:
        default:
            return ErrorCode::None;
    }
}

void NsmInbandUpdatePolicy::updateProperties(
    const struct ::nsm_firmware_erot_state_info_resp& erot_info)
{
    using InbandPolicyState =
        sdbusplus::common::com::nvidia::InbandUpdatePolicy::InbandPolicyState;

    switch (erot_info.fq_resp_hdr.inband_update_policy)
    {
        case NSM_ROT_INBAND_UPDATE_POLICY_ENABLE:
            inbandUpdatePolicy(InbandPolicyState::Enabled);
            break;
        case NSM_ROT_INBAND_UPDATE_POLICY_DISABLE:
            inbandUpdatePolicy(InbandPolicyState::Disabled);
            break;
        default:
            lg2::error(
                "Invalid inband_update_policy value: {VALUE}, InbandUpdatePolicy property not updated",
                "VALUE", erot_info.fq_resp_hdr.inband_update_policy);
            break;
    }
}

NsmInbandUpdatePolicyObject::NsmInbandUpdatePolicyObject(
    sdbusplus::bus::bus& bus, const std::string& chassisName,
    uint16_t classificationIn, uint16_t identifierIn, uint8_t indexIn) :
    NsmSensor(chassisName, "NSM_ChassisRoT"), objectPath(getPath(chassisName)),
    classification(classificationIn), identifier(identifierIn), index(indexIn)
{
    lg2::info("NsmInbandUpdatePolicy: create object: {PATH}", "PATH",
              objectPath.c_str());
    nsmInbandUpdatePolicy = std::make_unique<NsmInbandUpdatePolicy>(
        bus, objectPath, classification, identifier, index, *this);
}

std::optional<std::vector<uint8_t>>
    NsmInbandUpdatePolicyObject::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_get_erot_state_info_req));
    struct ::nsm_firmware_erot_state_info_req erot_req = {};
    erot_req.component_classification = classification;
    erot_req.component_classification_index = index;
    erot_req.component_identifier = identifier;
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

    if (shouldLog("decode_nsm_query_get_erot_state_parameters_resp",
                  reason_code, cc, rc))
    {
        LG2_ERROR(
            "decode_nsm_query_get_erot_state_parameters_resp failed. reasonCode={REASONCODE}, cc={CC}, rc={RC}",
            "REASONCODE", reason_code, "CC", cc, "RC", rc);
    }

    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        free(erot_info.slot_info);
        return cc ? cc : rc;
    }

    nsmInbandUpdatePolicy->updateProperties(erot_info);
    free(erot_info.slot_info);
    return cc ? cc : rc;
}

requester::Coroutine InbandUpdatePolicyHandler::updateInbandUpdatePolicy(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device, uint16_t classification,
    uint16_t identifier, uint8_t index)
{
    using InbandPolicyState =
        sdbusplus::common::com::nvidia::InbandUpdatePolicy::InbandPolicyState;

    const auto* policyString = std::get_if<std::string>(&value);
    if (policyString == nullptr)
    {
        LG2_ERROR("updateInbandUpdatePolicy: invalid value type");
        *status = AsyncOperationStatusType::InvalidArgument;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    auto policyStateOpt = sdbusplus::common::com::nvidia::InbandUpdatePolicy::
        convertStringToInbandPolicyState(*policyString);
    if (!policyStateOpt.has_value())
    {
        LG2_ERROR("updateInbandUpdatePolicy: invalid policy string: {POLICY}",
                  "POLICY", *policyString);
        *status = AsyncOperationStatusType::InvalidArgument;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    auto policyState = policyStateOpt.value();
    uint8_t policyValue;
    if (policyState == InbandPolicyState::Enabled)
    {
        policyValue = NSM_ROT_INBAND_UPDATE_POLICY_ENABLE;
    }
    else if (policyState == InbandPolicyState::Disabled)
    {
        policyValue = NSM_ROT_INBAND_UPDATE_POLICY_DISABLE;
    }
    else
    {
        LG2_ERROR("updateInbandUpdatePolicy: unsupported policy state");
        *status = AsyncOperationStatusType::InvalidArgument;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    auto eid = device->getEid();
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_set_rot_property_req_command));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
    struct ::nsm_firmware_set_rot_property_req nsm_req = {};

    nsm_req.component_classification = classification;
    nsm_req.component_classification_index = index;
    nsm_req.component_identifier = identifier;
    nsm_req.property = NSM_ROT_PROPERTY_INBAND_UPDATE_POLICY;
    nsm_req.argument_data[0] = policyValue;
    nsm_req.argument_data[1] = NSM_ROT_INBAND_UPDATE_POLICY_LIFESPAN_PERSISTENT;
    nsm_req.argument_length = 2;

    auto rc = encode_nsm_firmware_set_rot_property_req(0, &nsm_req, requestMsg);
    if (shouldLog("encode_nsm_firmware_set_rot_property_req", uint16_t(0),
                  uint8_t(0), rc))
    {
        LG2_ERROR(
            "updateInbandUpdatePolicy: encode_nsm_firmware_set_rot_property_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
    }
    if (rc != NSM_SW_SUCCESS)
    {
        *status = AsyncOperationStatusType::InternalFailure;
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await device->postPatchIO(eid, request, responseMsg, responseLen);

    if (shouldLog("postPatchIO", uint16_t(0), uint8_t(0), rc))
    {
        LG2_ERROR(
            "updateInbandUpdatePolicy: postPatchIO failed. eid={EID} rc={RC}",
            "EID", eid, "RC", utils::nsmSwCodeToString(rc));
    }
    if (rc != NSM_SW_SUCCESS)
    {
        *status = AsyncOperationStatusType::WriteFailure;
        co_return rc;
    }

    uint8_t cc = 0;
    uint16_t reasonCode = 0;
    rc = decode_nsm_firmware_set_rot_property_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode);

    if (shouldLog("decode_nsm_firmware_set_rot_property_resp", reasonCode, cc,
                  rc))
    {
        LG2_ERROR(
            "updateInbandUpdatePolicy: decode_nsm_firmware_set_rot_property_resp failed. reasonCode={REASONCODE}, cc={CC}, rc={RC}",
            "REASONCODE", utils::nsmReasonCodeToString(reasonCode), "CC",
            utils::nsmCompletionCodeToString(cc), "RC",
            utils::nsmSwCodeToString(rc));
    }
    if ((rc != NSM_SW_SUCCESS) || (cc != NSM_SUCCESS))
    {
        *status = AsyncOperationStatusType::WriteFailure;
        co_return cc ? cc : rc;
    }

    *status = AsyncOperationStatusType::Success;
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine updateInbandUpdatePolicyHandler(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device, uint16_t classification,
    uint16_t identifier, uint8_t index)
{
    InbandUpdatePolicyHandler handler;
    co_return co_await handler.updateInbandUpdatePolicy(
        value, status, device, classification, identifier, index);
}

void NsmImageCopy::setImageCopyResult(std::shared_ptr<AsyncValueIntf> valueIntf,
                                      uint8_t cc, uint16_t reasonCode,
                                      ImageCopyRequestStatus status,
                                      uint16_t errorCodeReason)
{
    auto error = getErrorCode(NSM_FW_IMAGE_COPY_CONTROL, cc, reasonCode);
    valueIntf->value(error);
    errorCode(mapReasonCodeToErrorCode(errorCodeReason));
    imageCopyRequestStatus(status);
}

requester::Coroutine
    NsmImageCopy::getActiveSlotComponentInfo(const std::string& objectPath,
                                             ComponentInfo& componentInfo)
{
    const std::string chassisPath =
        std::string("/xyz/openbmc_project/inventory/system/chassis");

    const std::string nsmRoTSlotInterface =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot";
    const std::string nsmRoTSlotAssociationsInterface =
        "xyz.openbmc_project.Configuration.NSM_RoT_Slot.Associations0";
    const std::string entityManagerService =
        "xyz.openbmc_project.EntityManager";

    GetSubTreeResponse slotsResponse = utils::DBusHandler().getSubtree(
        chassisPath, 0, {nsmRoTSlotInterface, nsmRoTSlotAssociationsInterface});

    for (const auto& [slotPath, mapServiceInterfaces] : slotsResponse)
    {
        if (mapServiceInterfaces.empty())
        {
            continue;
        }

        try
        {
            auto associations = co_await utils::coGetAllDbusProperty(
                entityManagerService.c_str(), slotPath.c_str(),
                nsmRoTSlotAssociationsInterface.c_str());

            if (associations.count("AbsolutePath"))
            {
                std::string absolutePath =
                    std::get<std::string>(associations.at("AbsolutePath"));
                if (absolutePath != objectPath)
                {
                    continue;
                }
            }
            else
            {
                continue;
            }
        }
        catch (const std::exception& e)
        {
            lg2::error("Exception getting associations: {ERROR}", "ERROR",
                       e.what());
            continue;
        }

        try
        {
            auto slotProperties = co_await utils::coGetAllDbusProperty(
                entityManagerService.c_str(), slotPath.c_str(),
                nsmRoTSlotInterface.c_str());

            if (slotProperties.count("SlotType"))
            {
                std::string slotType =
                    std::get<std::string>(slotProperties.at("SlotType"));

                if (slotType == "Active")
                {
                    auto classificationVal = std::get<uint64_t>(
                        slotProperties.at("ComponentClassification"));
                    auto identifierVal = std::get<uint64_t>(
                        slotProperties.at("ComponentIdentifier"));
                    auto indexVal =
                        std::get<uint64_t>(slotProperties.at("ComponentIndex"));

                    if (classificationVal > UINT16_MAX ||
                        identifierVal > UINT16_MAX || indexVal > UINT8_MAX)
                    {
                        lg2::error(
                            "Component property value out of range: "
                            "classification={CLASSIFICATION}, identifier={IDENTIFIER}, index={INDEX}",
                            "CLASSIFICATION", classificationVal, "IDENTIFIER",
                            identifierVal, "INDEX", indexVal);
                        continue;
                    }

                    ComponentInfo info;
                    info.classification =
                        static_cast<uint16_t>(classificationVal);
                    info.identifier = static_cast<uint16_t>(identifierVal);
                    info.index = static_cast<uint8_t>(indexVal);
                    componentInfo = info;

                    co_return NSM_SW_SUCCESS;
                }
            }
        }
        catch (const std::exception& e)
        {
            lg2::error("Exception getting slot properties: {ERROR}", "ERROR",
                       e.what());
            continue;
        }
    }

    co_return NSM_SW_ERROR;
}

void NsmImageCopy::requestImageCopy(
    std::vector<sdbusplus::message::object_path> objectPaths)
{
    // Check if an image copy operation is already in progress
    if (ImageCopyRequestStatus() == ImageCopyRequestStatus::Processing)
    {
        lg2::error("Cannot initiate image copy: operation already in progress");
        throw Common::Error::Unavailable();
    }

    const auto [objPath, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();
    if (objPath.empty())
    {
        imageCopyRequestStatus(ImageCopyRequestStatus::Rejected);
        throw Common::Error::Unavailable();
    }

    // Set status to InProgress when initiating the operation
    imageCopyRequestStatus(ImageCopyRequestStatus::Processing);

    // Start a timeout timer to prevent getting stuck in InProgress state
    imageCopyTimeoutTimer = std::make_shared<sdbusplus::Timer>([this]() {
        lg2::warning(
            "Image copy operation timed out, resetting status to None");
        imageCopyRequestStatus(ImageCopyRequestStatus::None);
        errorCode(ErrorCode::TimedOut);
        return false; // Don't restart timer
    });
    imageCopyTimeoutTimer->start(std::chrono::minutes(2));

    // Convert string_path_wrapper to std::string
    std::vector<std::string> paths;
    paths.reserve(objectPaths.size());
    for (const auto& path : objectPaths)
    {
        paths.emplace_back(path.str);
    }

    initiateImageCopyAsync(paths, statusIntf, valueIntf).detach();
}

requester::Coroutine NsmImageCopy::initiateImageCopyAsync(
    std::vector<std::string> objectPaths,
    std::shared_ptr<AsyncStatusIntf> statusIntf,
    std::shared_ptr<AsyncValueIntf> valueIntf)
{
    try
    {
        errorCode(mapReasonCodeToErrorCode(ERR_NULL));

        std::vector<ComponentInfo> componentInfos;

        for (const auto& objPath : objectPaths)
        {
            ComponentInfo componentInfo;
            uint8_t rc = co_await getActiveSlotComponentInfo(objPath,
                                                             componentInfo);
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error("Failed to get component info for {PATH}, rc={RC}",
                           "PATH", objPath, "RC", rc);
                setImageCopyResult(valueIntf, NSM_SW_ERROR, ERR_NULL,
                                   ImageCopyRequestStatus::Rejected);
                // Stop the timeout timer since operation completed
                if (imageCopyTimeoutTimer)
                {
                    imageCopyTimeoutTimer->stop();
                }
                statusIntf->status(AsyncOperationStatusType::InternalFailure);
                co_return rc;
            }
            else
            {
                componentInfos.push_back(componentInfo);
            }
        }

        if (componentInfos.empty())
        {
            setImageCopyResult(valueIntf, NSM_ERR_INVALID_DATA, ERR_NULL,
                               ImageCopyRequestStatus::Rejected);
            // Stop the timeout timer since operation completed
            if (imageCopyTimeoutTimer)
            {
                imageCopyTimeoutTimer->stop();
            }
            statusIntf->status(AsyncOperationStatusType::WriteFailure);
            co_return NSM_SW_ERROR;
        }

        uint8_t cc = NSM_SUCCESS;
        uint16_t reasonCode = 0;
        co_await imageCopyAsyncHandler(componentInfos, cc, reasonCode);

        if (cc != NSM_SUCCESS)
        {
            lg2::error(
                "Image copy operation returned error, cc={CC}, reasonCode={REASONCODE}",
                "CC", cc, "REASONCODE", reasonCode);
            setImageCopyResult(valueIntf, cc, reasonCode,
                               ImageCopyRequestStatus::Rejected, reasonCode);
            // Stop the timeout timer since operation completed
            if (imageCopyTimeoutTimer)
            {
                imageCopyTimeoutTimer->stop();
            }
            statusIntf->status(AsyncOperationStatusType::InternalFailure);
            co_return NSM_SW_ERROR;
        }

        setImageCopyResult(valueIntf, NSM_SUCCESS, 0,
                           ImageCopyRequestStatus::Accepted);
        // Stop the timeout timer since operation completed successfully
        if (imageCopyTimeoutTimer)
        {
            imageCopyTimeoutTimer->stop();
        }
        statusIntf->status(AsyncOperationStatusType::Success);
        co_return NSM_SW_SUCCESS;
    }
    catch (const std::exception& e)
    {
        lg2::error("Unhandled exception in initiateImageCopyAsync: {ERROR}",
                   "ERROR", e.what());
        setImageCopyResult(valueIntf, NSM_SW_ERROR, ERR_NULL,
                           ImageCopyRequestStatus::Rejected);
        // Stop the timeout timer since operation completed
        if (imageCopyTimeoutTimer)
        {
            imageCopyTimeoutTimer->stop();
        }
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        co_return NSM_SW_ERROR;
    }
    catch (...)
    {
        lg2::error("Unknown exception in initiateImageCopyAsync");
        setImageCopyResult(valueIntf, NSM_SW_ERROR, ERR_NULL,
                           ImageCopyRequestStatus::Rejected);
        // Stop the timeout timer since operation completed
        if (imageCopyTimeoutTimer)
        {
            imageCopyTimeoutTimer->stop();
        }
        statusIntf->status(AsyncOperationStatusType::InternalFailure);
        co_return NSM_SW_ERROR;
    }
}

requester::Coroutine NsmImageCopy::imageCopyAsyncHandler(
    const std::vector<ComponentInfo>& componentInfos, uint8_t& cc,
    uint16_t& reasonCode)
{
    cc = NSM_ERROR;
    reasonCode = ERR_NULL;

    // Get the device from the UUID
    SensorManager& manager = SensorManager::getInstance();
    auto device = manager.getNsmDeviceFromStaticUUID(uuid);
    if (!device)
    {
        lg2::error("Failed to get device for UUID: {UUID}", "UUID",
                   std::string(uuid.begin(), uuid.end()));
        co_return NSM_SW_ERROR;
    }

    auto eid = device->getEid();

    // Calculate message size: base + component entries
    uint8_t componentCount = static_cast<uint8_t>(componentInfos.size());
    size_t msgSize =
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_req) +
        sizeof(nsm_firmware_image_copy_control_req) +
        (componentCount * sizeof(nsm_firmware_image_copy_component_entry));

    auto request = std::make_shared<Request>(msgSize);
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    struct ::nsm_firmware_image_copy_control_req nsm_req;

    nsm_req.request_type = NSM_IMAGE_COPY_INITIATE_IMAGE_COPY;
    nsm_req.component_count = componentCount;

    // Convert ComponentInfo to nsm_firmware_image_copy_component_entry
    std::vector<nsm_firmware_image_copy_component_entry> componentEntries;
    componentEntries.reserve(componentInfos.size());
    for (const auto& component : componentInfos)
    {
        nsm_firmware_image_copy_component_entry entry;
        entry.component_classification = component.classification;
        entry.component_identifier = component.identifier;
        entry.component_classification_index = component.index;
        componentEntries.push_back(entry);
    }

    auto rc = encode_nsm_firmware_image_copy_control_req(
        0, &nsm_req, componentEntries.data(), requestMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("encode_nsm_firmware_image_copy_control_req failed: rc={RC}",
                   "RC", rc);
        reasonCode = static_cast<uint16_t>(rc);
        co_return rc;
    }

    // Send the request to the device
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await device->postPatchIO(eid, *request, responseMsg, responseLen);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("postPatchIO failed for chassis {UUID}, eid={EID}, rc={RC}",
                   "UUID", std::string(uuid.begin(), uuid.end()), "EID", eid,
                   "RC", utils::nsmSwCodeToString(rc));
        reasonCode = static_cast<uint16_t>(rc);
        co_return rc;
    }

    // Decode the response
    uint8_t decodedCc = 0;
    uint16_t decodedReasonCode = 0;
    rc = decode_nsm_firmware_image_copy_control_initiate_copy_resp(
        responseMsg.get(), responseLen, &decodedCc, &decodedReasonCode);

    // Set output parameters with decoded values
    cc = decodedCc;
    reasonCode = decodedReasonCode;

    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "decode_nsm_firmware_image_copy_control_initiate_copy_resp failure for chassis {UUID} | "
            "reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
            "UUID", std::string(uuid.begin(), uuid.end()), "REASONCODE",
            reasonCode, "CC", cc, "RC", rc);
        co_return NSM_SW_ERROR;
    }

    co_return NSM_SW_SUCCESS;
}

NsmImageCopyObject::NsmImageCopyObject(sdbusplus::bus::bus& bus,
                                       const std::string& chassisName,
                                       const uuid_t& uuid) :
    NsmObject(chassisName, "NSM_ChassisRoT"), objectPath(getPath(chassisName))
{
    lg2::info("NsmImageCopy: create object: {PATH}", "PATH",
              objectPath.c_str());
    nsmImageCopy = std::make_unique<NsmImageCopy>(bus, objectPath, uuid, *this);
}

NsmImageCopyPolicy::NsmImageCopyPolicy(sdbusplus::bus::bus& bus,
                                       const std::string& objPath,
                                       NsmSensor& nsmSensor) :
    ImageCopyPolicyIntf(bus, objPath.c_str()), nsmSensor(nsmSensor)
{}

void NsmImageCopyPolicy::updateProperties(
    const struct ::nsm_firmware_erot_state_info_resp& erot_info)
{
    using ImageCopyPolicyState =
        sdbusplus::common::com::nvidia::ImageCopyPolicy::ImageCopyPolicyState;

    bool invalidValue = false;
    switch (erot_info.fq_resp_hdr.background_copy_policy)
    {
        case NSM_ROT_REDUNDANCY_POLICY_MANUAL_BACKGROUND_COPY:
            imageCopyPolicy(ImageCopyPolicyState::Manual);
            break;
        case NSM_ROT_REDUNDANCY_POLICY_AUTOMATIC_BACKGROUND_COPY:
            imageCopyPolicy(ImageCopyPolicyState::Automatic);
            break;
        default:
            invalidValue = true;
            break;
    }

    if (shouldLog("Invalid background_copy_policy", invalidValue) &&
        invalidValue)
    {
        LG2_ERROR(
            "Invalid background_copy_policy value: {VALUE}, ImageCopyPolicy property not updated",
            "VALUE", erot_info.fq_resp_hdr.background_copy_policy);
    }
}

NsmImageCopyPolicyObject::NsmImageCopyPolicyObject(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    uint16_t classificationIn, uint16_t identifierIn, uint8_t indexIn) :
    NsmSensor(name, type), objectPath(getPath(name)),
    classification(classificationIn), identifier(identifierIn), index(indexIn)
{
    lg2::info("NsmImageCopyPolicyObject: create object: {PATH}", "PATH",
              objectPath.c_str());
    imageCopyPolicyObject =
        std::make_unique<NsmImageCopyPolicy>(bus, objectPath, *this);
}

std::optional<std::vector<uint8_t>>
    NsmImageCopyPolicyObject::genRequestMsg(__attribute__((unused)) eid_t eid,
                                            uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_get_erot_state_info_req));
    struct ::nsm_firmware_erot_state_info_req erot_req = {};
    erot_req.component_classification = classification;
    erot_req.component_classification_index = index;
    erot_req.component_identifier = identifier;
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_nsm_query_get_erot_state_parameters_req(
        instanceId, &erot_req, requestMsg);
    if (rc)
    {
        std::string msg = utils::requestMsgToHexString(request);
        lg2::error(
            "encode_nsm_query_get_erot_state_parameters_req failed: eid={EID}, rc={RC}, NSM_Request={MSG}",
            "EID", eid, "RC", rc, "MSG", msg);
        return std::nullopt;
    }
    return request;
}

uint8_t NsmImageCopyPolicyObject::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    nsm_firmware_erot_state_info_resp erot_info = {};

    auto rc = decode_nsm_query_get_erot_state_parameters_resp(
        responseMsg, responseLen, &cc, &reason_code, &erot_info);

    if (shouldLog("decode_nsm_query_get_erot_state_parameters_resp",
                  reason_code, cc, rc))
    {
        LG2_ERROR(
            "decode_nsm_query_get_erot_state_parameters_resp failed: rc={RC}, cc={CC}, reasonCode={REASONCODE}",
            "RC", rc, "CC", (int)cc, "REASONCODE", (int)reason_code);
    }

    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        free(erot_info.slot_info);
        return cc ? cc : rc;
    }

    imageCopyPolicyObject->updateProperties(erot_info);
    free(erot_info.slot_info);
    return cc ? cc : rc;
}

requester::Coroutine ImageCopyPolicyHandler::updateImageCopyPolicy(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device, uint16_t classification,
    uint16_t identifier, uint8_t index)
{
    using ImageCopyPolicyState =
        sdbusplus::common::com::nvidia::ImageCopyPolicy::ImageCopyPolicyState;

    const auto* policyString = std::get_if<std::string>(&value);
    if (policyString == nullptr)
    {
        LG2_ERROR("updateImageCopyPolicy: invalid value type");
        *status = AsyncOperationStatusType::InvalidArgument;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    auto policyStateOpt = sdbusplus::common::com::nvidia::ImageCopyPolicy::
        convertStringToImageCopyPolicyState(*policyString);
    if (!policyStateOpt.has_value())
    {
        LG2_ERROR("updateImageCopyPolicy: invalid policy string: {POLICY}",
                  "POLICY", *policyString);
        *status = AsyncOperationStatusType::InvalidArgument;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    auto policyState = policyStateOpt.value();
    uint8_t policyValue;
    if (policyState == ImageCopyPolicyState::Manual)
    {
        policyValue = NSM_ROT_REDUNDANCY_POLICY_MANUAL_BACKGROUND_COPY;
    }
    else if (policyState == ImageCopyPolicyState::Automatic)
    {
        policyValue = NSM_ROT_REDUNDANCY_POLICY_AUTOMATIC_BACKGROUND_COPY;
    }
    else
    {
        LG2_ERROR("updateImageCopyPolicy: unsupported policy state");
        *status = AsyncOperationStatusType::InvalidArgument;
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    auto eid = device->getEid();
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_set_rot_property_req_command));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
    struct ::nsm_firmware_set_rot_property_req nsm_req = {};

    nsm_req.component_classification = htole16(classification);
    nsm_req.component_classification_index = index;
    nsm_req.component_identifier = htole16(identifier);
    nsm_req.property = NSM_ROT_PROPERTY_REDUNDANCY_POLICY;
    nsm_req.argument_data[0] = policyValue;
    nsm_req.argument_data[1] = NSM_ROT_REDUNDANCY_POLICY_LIFESPAN_PERSISTENT;
    nsm_req.argument_length = 2;

    auto rc = encode_nsm_firmware_set_rot_property_req(0, &nsm_req, requestMsg);
    if (shouldLog("encode_nsm_firmware_set_rot_property_req", uint16_t(0),
                  uint8_t(0), rc))
    {
        LG2_ERROR(
            "updateImageCopyPolicy: encode_nsm_firmware_set_rot_property_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
    }
    if (rc != NSM_SW_SUCCESS)
    {
        *status = AsyncOperationStatusType::InternalFailure;
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await device->postPatchIO(eid, request, responseMsg, responseLen);

    if (shouldLog("postPatchIO", uint16_t(0), uint8_t(0), rc))
    {
        LG2_ERROR(
            "updateImageCopyPolicy: postPatchIO failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
    }
    if (rc != NSM_SW_SUCCESS)
    {
        *status = AsyncOperationStatusType::WriteFailure;
        co_return rc;
    }

    uint8_t cc = 0;
    uint16_t reasonCode = 0;
    rc = decode_nsm_firmware_set_rot_property_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode);

    if (shouldLog("decode_nsm_firmware_set_rot_property_resp", reasonCode, cc,
                  rc))
    {
        LG2_ERROR(
            "updateImageCopyPolicy: decode_nsm_firmware_set_rot_property_resp failed. reasonCode={REASONCODE}, cc={CC}, rc={RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    }
    if ((rc != NSM_SW_SUCCESS) || (cc != NSM_SUCCESS))
    {
        *status = AsyncOperationStatusType::WriteFailure;
        co_return cc ? cc : rc;
    }

    *status = AsyncOperationStatusType::Success;
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine updateImageCopyPolicyHandler(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device, uint16_t classification,
    uint16_t identifier, uint8_t index)
{
    ImageCopyPolicyHandler handler;
    co_return co_await handler.updateImageCopyPolicy(
        value, status, device, classification, identifier, index);
}

} // namespace nsm

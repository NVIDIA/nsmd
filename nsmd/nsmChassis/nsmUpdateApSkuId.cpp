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

#include "nsmUpdateApSkuId.hpp"

#include "nsmFirmwareUtilsCommon.hpp"
#include "sensorManager.hpp"
#include "utils.hpp"

#include <endian.h>

#include <phosphor-logging/lg2.hpp>

#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>

namespace nsm
{

std::string formatApSkuId(uint32_t skuIdValue)
{
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setw(8) << std::setfill('0')
        << std::uppercase << skuIdValue;
    return oss.str();
}

/**
 * @brief Validates the format of an AP SKU ID string
 * Expected format: "0x" or "0X" followed by exactly 8 hexadecimal digits
 * Examples: "0x12345678", "0xABCDEF12"
 *
 * @param skuId The SKU ID string to validate
 * @param errorMsg Output parameter for error message if validation fails
 * @return true if the format is valid, false otherwise
 */
bool validateApSkuIdFormat(const std::string& skuId, std::string& errorMsg)
{
    constexpr size_t expectedLength = 10; // "0x" + 8 hex digits

    // Check length
    if (skuId.length() != expectedLength)
    {
        std::ostringstream oss;
        oss << "Invalid SKU ID format: must be 0x followed by 8 hex digits (length "
            << skuId.length() << " != " << expectedLength << "): " << skuId;
        errorMsg = oss.str();
        return false;
    }

    // Check prefix
    if (skuId.find("0x") != 0 && skuId.find("0X") != 0)
    {
        errorMsg = "Invalid SKU ID format: must start with 0x or 0X prefix: " +
                   skuId;
        return false;
    }

    // Verify all characters after "0x" are valid hexadecimal digits
    for (size_t i = 2; i < skuId.length(); ++i)
    {
        char c = skuId[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F')))
        {
            std::ostringstream oss;
            oss << "Invalid SKU ID format: contains non-hexadecimal character at position "
                << i << ": " << skuId;
            errorMsg = oss.str();
            return false;
        }
    }

    return true;
}

/**
 * @brief Handler for async SKU update operation
 * This is called when someone writes to the SKU property via
 * com.nvidia.Async.Set
 */
requester::Coroutine updateApSkuIdHandler(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device, uint16_t classification,
    uint16_t identifier, uint8_t index)
{
    const std::string* skuId = std::get_if<std::string>(&value);

    if (!skuId)
    {
        lg2::error("Invalid argument type for updateApSkuIdHandler");
        *status = AsyncOperationStatusType::InvalidArgument;
        co_return NSM_SW_ERROR_DATA;
    }

    // Validate SKU ID format
    std::string errorMsg;
    if (!validateApSkuIdFormat(*skuId, errorMsg))
    {
        lg2::error("{ERROR}", "ERROR", errorMsg);
        *status = AsyncOperationStatusType::InvalidArgument;
        // coverity[missing_return]
        co_return NSM_SW_ERROR_DATA;
    }

    uint32_t skuIdValue = 0;
    size_t pos = 0;
    unsigned long parsedValue = 0;
    try
    {
        parsedValue = std::stoul(*skuId, &pos, 16);
    }
    catch (const std::exception& e)
    {
        lg2::error("Invalid SKU ID format (parse error): {SKUID}", "SKUID",
                   *skuId);
        *status = AsyncOperationStatusType::InvalidArgument;
        // coverity[missing_return]
        co_return NSM_SW_ERROR_DATA;
    }

    if (pos != skuId->length())
    {
        lg2::error("Invalid SKU ID format (extra characters): {SKUID}", "SKUID",
                   *skuId);
        *status = AsyncOperationStatusType::InvalidArgument;
        // coverity[missing_return]
        co_return NSM_SW_ERROR_DATA;
    }

    if (parsedValue > std::numeric_limits<uint32_t>::max())
    {
        lg2::error("SKU ID exceeds uint32_t range: {SKUID}", "SKUID", *skuId);
        *status = AsyncOperationStatusType::InvalidArgument;
        // coverity[missing_return]
        co_return NSM_SW_ERROR_DATA;
    }

    skuIdValue = static_cast<uint32_t>(parsedValue);

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_set_rot_property_req_command));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
    struct ::nsm_firmware_set_rot_property_req nsm_req = {};

    // Use AP firmware component identification values
    nsm_req.component_classification = htole16(classification);
    nsm_req.component_classification_index = index;
    nsm_req.component_identifier = htole16(identifier);

    // Set property to AP SKU ID
    nsm_req.property = NSM_ROT_PROPERTY_AP_SKU_ID;
    nsm_req.argument_length = NSM_ROT_AP_SKU_ID_ARGUMENT_LENGTH;

    // Store SKU ID in first 4 bytes
    memcpy(&nsm_req.argument_data[0], &skuIdValue, sizeof(uint32_t));

    // Store lifespan (persistent) in the 5th byte
    nsm_req.argument_data[4] = NSM_ROT_AP_SKU_ID_LIFESPAN_PERSISTENT;

    auto rc = encode_nsm_firmware_set_rot_property_req(0, &nsm_req, requestMsg);
    std::string msg = utils::requestMsgToHexString(request);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_nsm_firmware_set_rot_property_req failed: rc={RC}, skuId={SKUID}, NSM_Request={MSG}",
            "RC", rc, "SKUID", skuIdValue, "MSG", msg);
        *status = AsyncOperationStatusType::InternalFailure;
        // coverity[missing_return]
        co_return rc;
    }

    auto eid = device->getEid();
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await device->postPatchIO(eid, request, responseMsg, responseLen);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "updateApSkuIdHandler: postPatchIO error: eid={EID}, rc={RC}, skuId={SKUID}, NSM_Request={MSG}",
            "EID", eid, "RC", rc, "SKUID", skuIdValue, "MSG", msg);
        *status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = 0;
    uint16_t reasonCode = 0;
    rc = decode_nsm_firmware_set_rot_property_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode);

    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "decode_nsm_firmware_set_rot_property_resp failure: eid={EID}, reasonCode={REASONCODE}, cc={CC}, rc={RC}, skuId={SKUID}, NSM_Request={MSG}",
            "EID", eid, "REASONCODE", reasonCode, "CC", cc, "RC", rc, "SKUID",
            skuIdValue, "MSG", msg);
        *status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return cc ? cc : rc;
    }

    lg2::info("AP SKU ID updated successfully: {SKUID}", "SKUID", skuIdValue);

    // Note: The D-Bus property will be updated by the framework via
    // sensor->update() after this handler returns successfully

    *status = AsyncOperationStatusType::Success;

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

} // namespace nsm

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

#include "nsmSetCpuOperatingConfig.hpp"

#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{
enum class clockLimitFlag
{
    PERSISTENCE = 0,
    CLEAR = 1
};

requester::Coroutine getMinGraphicsClockLimit(uint32_t& minClockLimit,
                                              eid_t eid)
{
    SensorManager& manager = SensorManager::getInstance();
    uint8_t propertyIdentifier = MINIMUM_GRAPHICS_CLOCK_LIMIT;
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "getMinGraphicsClockLimit: encode_get_inventory_information_req failed. "
            "eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::error("SendRecvNsmMsg failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }
    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t value;
    std::vector<uint8_t> data(4, 0);
    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reason_code, &dataSize,
                                               data.data());

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        minClockLimit = le32toh(value);
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }
    else
    {
        lg2::error(
            "getMinGraphicsClockLimit: decode_get_inventory_information_resp with reasonCode = {REASONCODE},cc = {CC}and rc={RC}",
            "REASONCODE", reason_code, "CC", cc, "RC", rc);
    }
    co_return NSM_SW_ERROR_COMMAND_FAIL;
}

requester::Coroutine getMaxGraphicsClockLimit(uint32_t& maxClockLimit,
                                              eid_t eid)
{
    SensorManager& manager = SensorManager::getInstance();
    uint8_t propertyIdentifier = MAXIMUM_GRAPHICS_CLOCK_LIMIT;
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_inventory_information_req(0, propertyIdentifier,
                                                   requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "getMaxGraphicsClockLimit: encode_get_inventory_information_req failed. "
            "eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::error("SendRecvNsmMsg failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        // coverity[missing_return]
        co_return rc;
    }
    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_NULL;
    uint16_t dataSize = 0;
    uint32_t value;
    std::vector<uint8_t> data(4, 0);
    rc = decode_get_inventory_information_resp(responseMsg.get(), responseLen,
                                               &cc, &reason_code, &dataSize,
                                               data.data());

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS && dataSize == sizeof(value))
    {
        memcpy(&value, &data[0], sizeof(value));
        maxClockLimit = le32toh(value);
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }
    else
    {
        lg2::error(
            "getMaxGraphicsClockLimit: decode_get_inventory_information_resp with reasonCode = {REASONCODE},cc = {CC}and rc={RC}",
            "REASONCODE", reason_code, "CC", cc, "RC", rc);
    }
    co_return NSM_SW_ERROR_COMMAND_FAIL;
}

requester::Coroutine setClockLimitOnDevice(uint8_t clockId, bool speedLocked,
                                           uint32_t requestedSpeedLimit,
                                           AsyncOperationStatusType* status,
                                           std::shared_ptr<NsmDevice> device)
{
    SensorManager& manager = SensorManager::getInstance();
    auto eid = manager.getEid(device);

    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_clock_limit_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    uint32_t minClockLimit;
    auto rc = co_await getMinGraphicsClockLimit(minClockLimit, eid);
    if (rc)
    {
        lg2::error(
            "error while doing getMinGraphicsClockLimit eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint32_t maxClockLimit;
    rc = co_await getMaxGraphicsClockLimit(maxClockLimit, eid);
    if (rc)
    {
        lg2::error(
            "error while doing getMinGraphicsClockLimit eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    if (requestedSpeedLimit < minClockLimit ||
        requestedSpeedLimit > maxClockLimit)
    {
        lg2::error("invalid argument for speed limit");
        *status = AsyncOperationStatusType::InvalidArgument;
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t flag;
    uint32_t limitMin;
    uint32_t limitMax;

    flag = uint8_t(clockLimitFlag::PERSISTENCE);
    if (speedLocked)
    {
        limitMax = requestedSpeedLimit;
        limitMin = requestedSpeedLimit;
    }
    else
    {
        limitMax = requestedSpeedLimit;
        limitMin = minClockLimit;
    }
    // first argument instanceid=0 is irrelevant
    rc = encode_set_clock_limit_req(0, clockId, flag, limitMin, limitMax,
                                    requestMsg);

    if (rc)
    {
        lg2::error(
            "setClockLimitOnDevice encode_set_clock_limit_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc)
    {
        lg2::error(
            "setClockLimitOnDevice SendRecvNsmMsg failed for while setting clock limits "
            "eid={EID} rc={RC}",
            "EID", eid, "RC", rc);

        *status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint16_t data_size = 0;
    rc = decode_set_clock_limit_resp(responseMsg.get(), responseLen, &cc,
                                     &reason_code, &data_size);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        co_return NSM_SW_SUCCESS;
    }
    else
    {
        lg2::error(
            "setClockLimitOnDevice decode_set_clock_limit_resp failed. eid={EID} CC={CC} reasoncode={RC} RC={A}",
            "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
}

requester::Coroutine
    setCPUSpeedConfig(uint8_t clockId, const AsyncSetOperationValueType& value,
                      [[maybe_unused]] AsyncOperationStatusType* status,
                      std::shared_ptr<NsmDevice> device)
{
    const std::tuple<bool, uint32_t>* config =
        std::get_if<std::tuple<bool, uint32_t>>(&value);

    if (!config)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    const auto rc = co_await setClockLimitOnDevice(
        clockId, std::get<0>(*config), std::get<1>(*config), status, device);
    // coverity[missing_return]
    co_return rc;
}

} // namespace nsm

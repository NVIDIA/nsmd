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

#include "nsmPortDisableFuture.hpp"

#include "common/types.hpp"
#include "deviceManager.hpp"
#include "nsmCommon/sharedMemCommon.hpp"
#include "nsmDevice.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"
#include "utils.hpp"

namespace nsm
{

requester::Coroutine NsmDevicePortDisableFuture::update(SensorManager& manager,
                                                        eid_t eid)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_port_disable_future_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    auto rc = encode_get_port_disable_future_req(0, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_get_port_disable_future_req failed. eid={EID} rc={RC}",
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
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    bitfield8_t mask[PORT_MASK_DATA_SIZE];

    rc = decode_get_port_disable_future_resp(responseMsg.get(), responseLen,
                                             &cc, &reasonCode, &mask[0]);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        // parse the mask and update the dbus property
        std::vector<uint8_t> maskArray;
        utils::convertBitMaskToVector(maskArray, mask, PORT_MASK_DATA_SIZE);
        this->pdi().portDisableFuture(maskArray);
    }
    else
    {
        lg2::error(
            "responseHandler: decode_get_port_disable_future_resp unsuccessfull. reasonCode={RSNCOD} cc={CC} rc={RC}",
            "RSNCOD", reasonCode, "CC", cc, "RC", rc);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

requester::Coroutine NsmDevicePortDisableFuture::setDevicePortDisableFuture(
    bitfield8_t* mask, [[maybe_unused]] AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    SensorManager& manager = SensorManager::getInstance();
    auto eid = manager.getEid(device);
    lg2::info("setDevicePortDisableFuture for EID: {EID}", "EID", eid);

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_port_disable_future_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // first argument instanceid=0 is irrelevant
    auto rc = encode_set_port_disable_future_req(0, mask, requestMsg);

    if (rc)
    {
        lg2::error(
            "setDevicePortDisableFuture encode_set_MIG_mode_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        *status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc_ = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                               responseLen);
    if (rc_)
    {
        lg2::error(
            "setDevicePortDisableFuture SendRecvNsmMsgSync failed for while setting MigMode "
            "eid={EID} rc={RC}",
            "EID", eid, "RC", rc_);
        *status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    rc = decode_set_port_disable_future_resp(responseMsg.get(), responseLen,
                                             &cc, &reason_code);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        lg2::info("setDevicePortDisableFuture for EID: {EID} completed", "EID",
                  eid);
    }
    else
    {
        lg2::error(
            "setDevicePortDisableFuture decode_set_port_disable_future_resp failed. eid={EID} CC={CC} reasoncode={RC} RC={A}",
            "EID", eid, "CC", cc, "RC", reason_code, "A", rc);
        lg2::error("throwing write failure exception");
        *status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine NsmDevicePortDisableFuture::setPortDisableFuture(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device)
{
    const std::vector<uint8_t>* portNumToDisable =
        std::get_if<std::vector<uint8_t>>(&value);
    if (!portNumToDisable)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    if (asyncPatchInProgress)
    {
        // do not allow patch if already in process
        lg2::error(
            "throwing unavailable exception since patch is already in progress");
        *status = AsyncOperationStatusType::Unavailable;
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }

    asyncPatchInProgress = true;
    std::vector<uint8_t> ports = *portNumToDisable;
    bitfield8_t mask[PORT_MASK_DATA_SIZE] = {0};

    for (size_t i = 0; i < ports.size(); ++i)
    {
        size_t byteIndex = ports[i] / 8;
        size_t bitOffset = ports[i] % 8;

        switch (bitOffset)
        {
            case 0:
                mask[byteIndex].bits.bit0 = true;
                break;
            case 1:
                mask[byteIndex].bits.bit1 = true;
                break;
            case 2:
                mask[byteIndex].bits.bit2 = true;
                break;
            case 3:
                mask[byteIndex].bits.bit3 = true;
                break;
            case 4:
                mask[byteIndex].bits.bit4 = true;
                break;
            case 5:
                mask[byteIndex].bits.bit5 = true;
                break;
            case 6:
                mask[byteIndex].bits.bit6 = true;
                break;
            case 7:
                mask[byteIndex].bits.bit7 = true;
                break;
        }
    }

    const auto rc = co_await setDevicePortDisableFuture(mask, status, device);
    asyncPatchInProgress = false;
    // coverity[missing_return]
    co_return rc;
}

} // namespace nsm

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
#include "nsmSensor.hpp"

#include "sensorManager.hpp"

namespace nsm
{
requester::Coroutine NsmSensor::update(SensorManager& manager, eid_t eid)
{
    auto requestMsg = genRequestMsg(eid, 0);
    if (!requestMsg.has_value())
    {
        lg2::error(
            "NsmSensor::update: genRequestMsg failed, name={NAME}, eid={EID}",
            "NAME", getName(), "EID", eid);
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc = co_await manager.SendRecvNsmMsg(eid, *requestMsg, responseMsg,
                                              responseLen);
    if (rc)
    {
        // coverity[missing_return]
        co_return rc;
    }

    rc = handleResponseMsg(responseMsg.get(), responseLen);
    // coverity[missing_return]
    co_return rc;
}

bool NsmSensor::equals(const NsmSensor& other) const
{
    // name and type are used only for debbuging purposes
    // comparing shall be only based on the request data sended to the device
    auto requestMsg = const_cast<NsmSensor*>(this)->genRequestMsg(0, 0);
    auto sensorRequestMsg = const_cast<NsmSensor&>(other).genRequestMsg(0, 0);
    return requestMsg && sensorRequestMsg && *requestMsg == *sensorRequestMsg;
}

bool NsmSensor::operator==(const NsmSensor& other) const
{
    return equals(other);
}

} // namespace nsm

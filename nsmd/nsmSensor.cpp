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

#ifdef LTTNG_TRACING
#include "tracepoints/nsmd-tp.h"
#endif

namespace nsm
{
requester::Coroutine NsmSensor::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    auto requestMsg = genRequestMsg(nsmDevice->getEid(), 0);
    if (!requestMsg.has_value())
    {
        lg2::error(
            "NsmSensor::update: genRequestMsg failed, name={NAME}, eid={EID}",
            "NAME", getName(), "EID", nsmDevice->getEid());
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }

#ifdef LTTNG_TRACING
    lttng_ust_tracepoint(nsmd, sensor_polling_request_generated,
                         nsmDevice->getEid(), requestMsg.value().data(),
                         this->getName().c_str());
#endif

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), *requestMsg,
                                           responseMsg, responseLen);
    if (rc)
    {
        // coverity[missing_return]
        co_return rc;
    }

    rc = handleResponseMsg(responseMsg.get(), responseLen);

#ifdef LTTNG_TRACING
    lttng_ust_tracepoint(nsmd, dbus_sensor_reading_updated, nsmDevice->getEid(),
                         responseMsg.get(), this->getName().c_str());
#endif

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

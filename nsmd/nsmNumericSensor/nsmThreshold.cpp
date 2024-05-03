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

#include "nsmThreshold.hpp"

#include "platform-environmental.h"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{
std::optional<std::vector<uint8_t>>
    NsmThreshold::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_read_thermal_parameter_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc =
        encode_read_thermal_parameter_req(instanceId, sensorId, requestPtr);
    if (rc)
    {
        lg2::error("encode_read_thermal_parameter_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmThreshold::handleResponseMsg(const struct nsm_msg* responseMsg,
                                        size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    int32_t threshold = 0;

    auto rc = decode_read_thermal_parameter_resp(responseMsg, responseLen, &cc,
                                                 &reason_code, &threshold);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        sensorValue->updateReading(threshold);
    }
    else
    {
        sensorValue->updateReading(std::numeric_limits<double>::quiet_NaN());

        lg2::error(
            "handleResponseMsg: decode_read_thermal_parameter_resp failed. "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);

        if (rc == NSM_SW_SUCCESS)
        {
            return NSM_SW_ERROR_COMMAND_FAIL;
        }
    }

    return rc;
}
} // namespace nsm
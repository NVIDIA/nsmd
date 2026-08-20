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

#include "nsmSensorAggregator.hpp"

#include "network-ports.h"
#include "platform-environmental.h"

#include "nsmDevice.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{
NsmSensorAggregator::NsmSensorAggregator(const std::string& name,
                                         const std::string& type) :
    NsmSensor(name, type)
{
    sampleTags.reserve(256);
}

uint8_t NsmSensorAggregator::handleResponseMsg(const nsm_msg* responseMsg,
                                               size_t responseLen)
{
    uint8_t returnValue = NSM_SW_SUCCESS;
    uint8_t cc{};
    uint16_t telemetryCount{};
    size_t consumedLen{};
    auto responseData = reinterpret_cast<const uint8_t*>(responseMsg);

    auto rc = decode_aggregate_resp(responseMsg, responseLen, &consumedLen, &cc,
                                    &telemetryCount);

    if (shouldLog("decode_aggregate_resp", uint16_t(0), cc, rc))
    {
        LG2_ERROR("decode_aggregate_resp | cc: {CC}, rc: {RC}", "CC", cc, "RC",
                  rc);
    }
    if (cc != NSM_SUCCESS || rc != NSM_SW_SUCCESS)
    {
        return cc ? cc : rc;
    }
    sampleTags.clear();
    while (telemetryCount--)
    {
        uint8_t tag = 0;
        bool valid = false;
        const uint8_t* data = nullptr;
        size_t dataLen = 0;

        responseLen -= consumedLen;
        responseData += consumedLen;

        auto sample =
            reinterpret_cast<const nsm_aggregate_resp_sample*>(responseData);

        rc = decode_aggregate_resp_sample(sample, responseLen, &consumedLen,
                                          &tag, &valid, &data, &dataLen);

        if (rc != NSM_SW_SUCCESS)
        {
            if (shouldLog("decode_aggregate_resp_sample", nsm_sw_codes(rc)))
            {
                lg2::error(
                    "responseHandler: decode_aggregate_resp_sample failed. "
                    "Type={TYPE}, Tag={TAG}, sensor={NAME}, rc={RC}, "
                    "valid_bit={VALID}",
                    "TYPE", getType(), "TAG", tag, "NAME", getName(), "RC", rc,
                    "VALID", valid);
            }

            continue;
        }
        sampleTags.push_back(tag);
        rc = handleSample(
            TelemetrySample{tag, static_cast<uint8_t>(dataLen), data, valid});
        if (rc != NSM_SW_SUCCESS && returnValue == NSM_SW_SUCCESS)
        {
            lg2::debug(
                "responseHandler: decoding failed for one or more samples. "
                "Type={TYPE}, sensor={NAME}, rc={RC}",
                "TYPE", getType(), "NAME", getName(), "RC", rc);
            returnValue = rc;
        }
    }

    return returnValue;
}

NsmSensorAggregatorPaginated::NsmSensorAggregatorPaginated(
    const std::string& name, const std::string& type) : NsmSensor(name, type)
{}

uint8_t NsmSensorAggregatorPaginated::handleResponseMsg(
    const nsm_msg* /*responseMsg*/, size_t /*responseLen*/)
{
    /* update() drives the full paginated loop directly.
     * This method satisfies the NsmSensor pure-virtual contract but is
     * not called in normal operation. */
    lg2::error("NsmSensorAggregatorPaginated::handleResponseMsg called "
               "unexpectedly, name={NAME}",
               "NAME", getName());
    return NSM_SW_ERROR;
}

requester::Coroutine
    NsmSensorAggregatorPaginated::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    seqToken = 0x00000000;
    uint32_t pageNum = 0;

    do
    {
        auto requestMsg = genRequestMsg(nsmDevice->getEid(), 0);
        if (!requestMsg.has_value())
        {
            lg2::error("NsmSensorAggregatorPaginated: genRequestMsg failed, "
                       "name={NAME}",
                       "NAME", getName());
            resetState();
            seqToken = 0x00000000;
            // coverity[missing_return]
            co_return NSM_SW_ERROR;
        }

        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        auto rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), *requestMsg,
                                               responseMsg, responseLen);
        if (rc)
        {
            lg2::error("NsmSensorAggregatorPaginated: sensorIO failed, "
                       "name={NAME}, page={PAGE}, rc={RC}",
                       "NAME", getName(), "PAGE", pageNum, "RC", rc);
            resetState();
            seqToken = 0x00000000;
            // coverity[missing_return]
            co_return rc;
        }

        uint8_t cc{};
        uint16_t telemetryCount{};
        size_t consumedLen{};
        auto responseData = reinterpret_cast<const uint8_t*>(responseMsg.get());

        rc = decode_aggregate_resp(responseMsg.get(), responseLen, &consumedLen,
                                   &cc, &telemetryCount);
        if (cc != NSM_SUCCESS || rc != NSM_SW_SUCCESS)
        {
            if (shouldLog("NsmSensorAggregatorPaginated:decode_aggregate_resp",
                          nsm_completion_codes(cc), nsm_sw_codes(rc)))
            {
                lg2::error(
                    "NsmSensorAggregatorPaginated: decode_aggregate_resp "
                    "failed, name={NAME}, page={PAGE}, cc={CC}, rc={RC}",
                    "NAME", getName(), "PAGE", pageNum, "CC", cc, "RC", rc);
            }
            resetState();
            seqToken = 0x00000000;
            // coverity[missing_return]
            co_return cc ? cc : static_cast<uint8_t>(rc);
        }

        uint32_t nextToken = 0x00000000;

        while (telemetryCount--)
        {
            uint8_t tag = 0;
            bool valid = false;
            const uint8_t* data = nullptr;
            size_t dataLen = 0;

            if (consumedLen == 0 || consumedLen > responseLen)
            {
                // A malformed sample must not underflow responseLen and
                // cascade into out-of-bounds reads on subsequent iterations.
                // Abort the whole session instead of advancing past the
                // received bytes.
                rc = NSM_SW_ERROR_LENGTH;
                if (shouldLog("NsmSensorAggregatorPaginated:consumedLen",
                              nsm_sw_codes(rc)))
                {
                    lg2::error(
                        "NsmSensorAggregatorPaginated: consumedLen out of "
                        "bounds, name={NAME}, page={PAGE}, rc={RC}",
                        "NAME", getName(), "PAGE", pageNum, "RC", rc);
                }
                resetState();
                seqToken = 0x00000000;
                // coverity[missing_return]
                co_return rc;
            }

            responseLen -= consumedLen;
            responseData += consumedLen;

            auto sample = reinterpret_cast<const nsm_aggregate_resp_sample*>(
                responseData);

            rc = decode_aggregate_resp_sample(sample, responseLen, &consumedLen,
                                              &tag, &valid, &data, &dataLen);
            if (rc != NSM_SW_SUCCESS)
            {
                if (shouldLog("NsmSensorAggregatorPaginated:"
                              "decode_aggregate_resp_sample",
                              nsm_sw_codes(rc)))
                {
                    lg2::error("NsmSensorAggregatorPaginated: "
                               "decode_aggregate_resp_sample failed, "
                               "name={NAME}, page={PAGE}, tag={TAG}, rc={RC}",
                               "NAME", getName(), "PAGE", pageNum, "TAG", tag,
                               "RC", rc);
                }
                // consumedLen from a failed decode isn't trustworthy (it's
                // written before decode_aggregate_resp_sample's own length
                // check), and continuing would use it to advance
                // responseData/responseLen with a cursor that may be past
                // the buffer. Abort the whole session rather than just this
                // page.
                resetState();
                seqToken = 0x00000000;
                // coverity[missing_return]
                co_return rc;
            }

            if (tag == static_cast<uint8_t>(SpecialTag::SEQUENCE_TOKEN))
            {
                rc = decode_sequence_token_record(data, dataLen, &nextToken);
                if (rc != NSM_SW_SUCCESS)
                {
                    if (shouldLog("NsmSensorAggregatorPaginated:"
                                  "decode_sequence_token_record",
                                  nsm_sw_codes(rc)))
                    {
                        lg2::error("NsmSensorAggregatorPaginated: "
                                   "decode_sequence_token_record failed, "
                                   "name={NAME}, page={PAGE}, dataLen={LEN}, "
                                   "rc={RC}",
                                   "NAME", getName(), "PAGE", pageNum, "LEN",
                                   dataLen, "RC", rc);
                    }
                    resetState();
                    seqToken = 0x00000000;
                    // coverity[missing_return]
                    co_return rc;
                }
            }
            else
            {
                rc = handleSample(TelemetrySample{
                    tag, static_cast<uint8_t>(dataLen), data, valid});
                if (rc != NSM_SW_SUCCESS)
                {
                    if (shouldLog("NsmSensorAggregatorPaginated:handleSample",
                                  nsm_sw_codes(rc)))
                    {
                        lg2::error("NsmSensorAggregatorPaginated: handleSample "
                                   "failed, name={NAME}, page={PAGE}, "
                                   "tag={TAG}, rc={RC}",
                                   "NAME", getName(), "PAGE", pageNum, "TAG",
                                   tag, "RC", rc);
                    }
                    resetState();
                    seqToken = 0x00000000;
                    // coverity[missing_return]
                    co_return rc;
                }
            }
        }

        if (++pageNum > kMaxPages ||
            (seqToken != 0x00000000 && nextToken == seqToken))
        {
            // shouldLog's bool overload treats `true` as failure (logs on
            // first occurrence, suppresses repeats) -- we're always in a
            // stall condition here, regardless of which check tripped it.
            if (shouldLog("NsmSensorAggregatorPaginated:pagination_stall",
                          true))
            {
                lg2::error("NsmSensorAggregatorPaginated: pagination did not "
                           "terminate, name={NAME}, page={PAGE}, token={TOKEN}",
                           "NAME", getName(), "PAGE", pageNum, "TOKEN",
                           nextToken);
            }
            resetState();
            seqToken = 0x00000000;
            // coverity[missing_return]
            co_return NSM_SW_ERROR;
        }
        seqToken = nextToken;

    } while (seqToken != 0x00000000);

    lg2::debug("NsmSensorAggregatorPaginated: update complete, "
               "name={NAME}, pages={PAGES}",
               "NAME", getName(), "PAGES", pageNum);

    postUpdate();
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

} // namespace nsm

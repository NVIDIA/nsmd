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

#pragma once

#include "base.h"

#include "nsmSensor.hpp"

#include <cstdint>

namespace nsm
{
/** @class NsmSensorAggregator
 *
 * Abstract class to provide common functionalities of NSM Aggregate command.
 * To add a support for new NSM Aggregate command, derive publicly from it
 * and put command specific details (typically calls to libnsm) in its two pure
 * virtual methods.
 */
class NsmSensorAggregator : public NsmSensor
{
  public:
    NsmSensorAggregator(const std::string& name, const std::string& type);
    virtual ~NsmSensorAggregator() = default;

    uint8_t handleResponseMsg(const nsm_msg* responseMsg,
                              size_t responseLen) final;

    struct TelemetrySample
    {
        uint8_t tag;
        uint8_t data_len;
        const uint8_t* data;
        bool valid;
    };

  private:
    /** @brief this function will be called for each telemetry sample found in
     * response message. You might want to call updateSensorReading after
     * decoding value from the data. Special Tag values (i.e. timestamp, uuid
     * etc) are expected to be handled by this function.
     *
     *  @param[in] tag - tag of telemetry sample
     *  @param[in] data - data of telemetry sample
     *  @param[in] data_len - number of bytes in data of telemetry sample
     *  @return nsm_completion_codes
     */
    virtual int handleSample(const TelemetrySample& sample) = 0;

  protected:
    enum SpecialTag : uint8_t
    {
        UUID = 0xFE,
        TIMESTAMP = 0xFF
    };
    std::vector<uint8_t> sampleTags;
};
/** @class NsmSensorAggregatorPaginated
 *
 * Abstract base class for sensors using paginated NSM aggregate commands.
 *
 * Implements "composite sensor" scheduling: update() drives the full
 * multi-page Sequence Token loop. The sensor holds its position in the
 * nsmd polling queue for the entire duration and releases only after the
 * final page (seqToken == 0) is received.
 *
 * Derived classes implement:
 *   genRequestMsg()  — encode the command's NSM request embedding seqToken
 *   handleSample()   — per-sample callback for non-metadata samples
 *
 * NSM aggregate metadata tags: 0xFD Sequence Token, 0xFE UUID, 0xFF
 * Timestamp. The base class only recognizes and handles 0xFD internally;
 * derived classes never see tag 0xFD in handleSample(). Tags 0xFE and
 * 0xFF are not filtered here and would reach handleSample() like any
 * other tag if a device ever sent them.
 */
class NsmSensorAggregatorPaginated : public NsmSensor
{
  public:
    NsmSensorAggregatorPaginated(const std::string& name,
                                 const std::string& type);
    virtual ~NsmSensorAggregatorPaginated() = default;

    /** @brief Multi-page update coroutine — composite sensor implementation.
     *
     * Drives the Sequence Token loop. Holds the polling queue position
     * for the entire duration. final override — derived classes must NOT
     * override update().
     */
    requester::Coroutine
        update(std::shared_ptr<NsmDevice> nsmDevice) override final;

    /** @brief handleResponseMsg is not used in paginated mode.
     *
     * update() drives the full loop directly. This override satisfies
     * the NsmSensor pure-virtual contract but should never be called.
     * Returns NSM_SW_ERROR if called.
     */
    uint8_t handleResponseMsg(const nsm_msg* responseMsg,
                              size_t responseLen) override final;

    struct TelemetrySample
    {
        uint8_t tag;
        uint8_t data_len;
        const uint8_t* data;
        bool valid;
    };

  protected:
    /** Upper bound on pages per update() session — guards against a device
     * that never terminates pagination (Sequence Token stuck or looping). */
    static constexpr uint32_t kMaxPages = 256;

    /** Current Sequence Token for the in-progress multi-page session.
     * 0x00000000 between sessions. Derived classes read this in
     * genRequestMsg() to embed it in the request. */
    uint32_t seqToken{0x00000000};

    enum SpecialTag : uint8_t
    {
        SEQUENCE_TOKEN = 0xFD,
        UUID = 0xFE,
        TIMESTAMP = 0xFF
    };

    /** @brief Called after the final page is received successfully.
     * Default no-op. Derived classes override to publish accumulated data. */
    virtual void postUpdate() {}

    /** @brief Called when the session is aborted due to error.
     * Default no-op. Derived classes override to discard partial state. */
    virtual void resetState() {}

  private:
    /** @brief Per-sample callback. Called for every tag except 0xFD
     * (Sequence Token), which the base class handles internally. Tags
     * 0xFE (UUID) and 0xFF (Timestamp) are also NSM aggregate metadata
     * tags but are not filtered here, so they are forwarded here too.
     * Return NSM_SW_SUCCESS on success; non-zero is non-fatal. */
    virtual int handleSample(const TelemetrySample& sample) = 0;
};

} // namespace nsm

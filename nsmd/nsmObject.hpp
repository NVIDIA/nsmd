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
#include "requester/handler.hpp"
#include "types.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <tal.hpp>

static constexpr const uint64_t INIT_TIMESTAMP =
    std::numeric_limits<uint64_t>().min();

static constexpr const uint64_t DEFAULT_RR_REFRESH_LIMIT_IN_USEC =
    DEFAULT_RR_REFRESH_LIMIT_IN_MS * 1000;

namespace nsm
{

class SensorManager;
class NsmObject
{
  public:
    NsmObject() = delete;
    NsmObject(const std::string& name, const std::string& type,
              bool isLongRunning = false) :
        isLongRunning(isLongRunning),
        name(name), type(type)
    {}
    NsmObject(const NsmObject& copy) : name(copy.name), type(copy.type) {}
    virtual ~NsmObject() = default;
    const std::string& getName() const
    {
        return name;
    }
    const std::string& getType() const
    {
        return type;
    }
    const std::string& getDeviceIdentifier() const
    {
        return deviceIdentifier;
    }

    void setDeviceIdentifier(const std::string deviceType)
    {
        deviceIdentifier = deviceType;
    }

    void setLastUpdatedTimeStamp(const uint64_t currentTimestampInUsec)
    {
        lastUpdatedTimeStampInUsec = currentTimestampInUsec;
    }

    inline bool needsUpdate(const uint64_t& currentTimestampInUsec) const
    {
        const uint64_t deltaInUsec = currentTimestampInUsec -
                                     lastUpdatedTimeStampInUsec;
        return (deltaInUsec > refreshLimitInUsec);
    }

    void logHandleResponseMsg(const std::string funcName,
                              const uint16_t& reason_code, const int& cc,
                              const int& rc)
    {
        if (!shouldLogError(cc, rc))
        {
            return;
        }
        lg2::error(
            "handleResponseMsg: {FUNCNAME} | Device={DEVID} sensor={NAME} "
            "with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "FUNCNAME", funcName, "DEVID", getDeviceIdentifier(), "NAME",
            getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
    }

    bool shouldLogError(const int& cc, const int& rc)
    {
        if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
        {
            return false;
        }
        if (cc == NSM_SUCCESS)
        {
            rc_map.isAnyBitSet = true;
            return !utils::isbitfield256_tBitSet(rc_map.bitMap, rc);
        }
        cc_map.isAnyBitSet = true;
        return !utils::isbitfield256_tBitSet(cc_map.bitMap, cc);
    }

    void clearErrorBitMap(std::string funcName)
    {
        if (cc_map.isAnyBitSet)
        {
            lg2::error(
                "handleResponseMsg: {FUNCNAME} | Device={DEVID} sensor={NAME} "
                "request SUCCESSFUL | CC Code(s) Cleared : [{CCCLEAREDBITS}]",
                "FUNCNAME", funcName, "DEVID", getDeviceIdentifier(), "NAME",
                getName(), "CCCLEAREDBITS",
                utils::bitfield256_tGetSetBits(cc_map.bitMap));
        }
        if (rc_map.isAnyBitSet)
        {
            lg2::error(
                "handleResponseMsg: {FUNCNAME} | Device={DEVID} sensor={NAME} "
                "request SUCCESSFUL | RC Code(s) Cleared : [{RCCLEAREDBITS}]",
                "FUNCNAME", funcName, "DEVID", getDeviceIdentifier(), "NAME",
                getName(), "RCCLEAREDBITS",
                utils::bitfield256_tGetSetBits(rc_map.bitMap));
        }
        // Clear the bitmaps
        for (int i = 0; i < 8; i++)
        {
            cc_map.bitMap.fields[i].byte = 0;
            rc_map.bitMap.fields[i].byte = 0;
        }
        cc_map.isAnyBitSet = false;
        rc_map.isAnyBitSet = false;
    }

    virtual requester::Coroutine update([[maybe_unused]] SensorManager& manager,
                                        [[maybe_unused]] eid_t eid)
    {
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }

    virtual void handleOfflineState()
    {
        return;
    }

    virtual void updateMetricOnSharedMemory() {}

    bool isRefreshed = false;
    bool isStatic = false;
    bool isLongRunning = false;

  private:
    const std::string name;
    const std::string type;
    uint64_t lastUpdatedTimeStampInUsec = INIT_TIMESTAMP;
    uint64_t refreshLimitInUsec = DEFAULT_RR_REFRESH_LIMIT_IN_USEC;
    utils::bitfield256_err_code cc_map;
    utils::bitfield256_err_code rc_map;
    std::string deviceIdentifier;
    // deviceIdentifier = deviceName_deviceInstanceNumber
};
} // namespace nsm

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

#include "nsmGPIOStateChangeEvent.hpp"

#include "base.h"
#include "device-capability-discovery.h"

#include "dBusAsyncUtils.hpp"
#include "sensorManager.hpp"

#include <fmt/args.h>

#include <phosphor-logging/lg2.hpp>

namespace nsm
{
NsmGPIOStateChangeEvent::NsmGPIOStateChangeEvent(
    const std::string& name, const std::string& type,
    std::shared_ptr<GPIOStateIntf> gpioStateIntf) :
    NsmEvent(name, type), gpioStateIntf(gpioStateIntf)
{
    lg2::debug("NsmGPIOStateChangeEvent: Name {NAME}.", "NAME", name);
}

int NsmGPIOStateChangeEvent::handle(eid_t eid, NsmType /*type*/,
                                    NsmEventId /*eventId*/,
                                    const nsm_msg* event, size_t eventLen)
{
    lg2::info("Received gpio state change event from EID {EID}.", "EID", eid);

    uint8_t eventClass{};
    uint16_t eventState{};
    struct nsm_gpio_state_change_event_payload* payload = nullptr;

    auto rc = decode_nsm_gpio_state_change_event(event, eventLen, &eventClass,
                                                 &eventState, &payload);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "decode_nsm_gpio_state_change_event failed. rc={RC}, EID={SRC}, Name={NAME}",
            "RC", rc, "SRC", eid, "NAME", getName());

        return rc;
    }
    std::vector<uint16_t> gpioIndexesChanged;

    if (gpioStateIntf != nullptr)
    {
        uint64_t timestampNs = ((uint64_t)payload->timestamp_high << 32) |
                               payload->timestamp_low;
        gpioStateIntf->lastChangeTime(timestampNs);
        auto currentState = gpioStateIntf->lineStates();

        for (size_t i = 0; i < payload->num_gpio_events; i++)
        {
            uint16_t gpioIndex = payload->gpio_events[i].gpio_index;
            bool gpioValue = payload->gpio_events[i].gpio_value;

            // Add GPIO index to the changed list
            gpioIndexesChanged.push_back(gpioIndex);

            // Update the current state with the new value
            currentState[gpioIndex] = gpioValue;
            lg2::debug("GPIO state change: index={IDX}, value={VAL}", "IDX",
                       gpioIndex, "VAL", gpioValue);
        }

        if (!currentState.empty())
        {
            gpioStateIntf->lineStates(currentState);
            gpioStateIntf->gpioChanged(gpioIndexesChanged);
        }
    }

    return NSM_SW_SUCCESS;
}
} // namespace nsm

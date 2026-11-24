/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION &
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

#include "libnsm/network-ports.h"

#include "asyncOperationManager.hpp"
#include "nsmSensor.hpp"

#include <com/nvidia/PowerCapping/Profile/server.hpp>
#include <sdbusplus/server/object.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace nsm
{

using PowerProfileServer =
    sdbusplus::com::nvidia::PowerCapping::server::Profile;
using PowerProfileIntf = sdbusplus::server::object_t<PowerProfileServer>;

/** @brief Power profiles using NSM Type 1 Get/Set Power Profile (0x0D/0x0C),
 *         published as com.nvidia.PowerCapping.Profile.
 *
 *  The interface exists only while its values come from the device: it is
 *  added on the first successful Get Power Profile and removed when the
 *  device goes offline. */
class NsmSwitchPowerProfile : public NsmSensor
{
  public:
    /** Profile ID, name, and whether it is the active profile. */
    using ProfileEntry = std::tuple<uint8_t, std::string, bool>;

    /** Name of the hardware default profile (ID 0) when the device does not
     *  name it. */
    static constexpr auto hardwareDefaultProfileName = "HardwareDefault";

    NsmSwitchPowerProfile(const std::string& name, const std::string& type,
                          sdbusplus::bus_t& bus, const std::string& objPath);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void handleOfflineState() override;

    /** @brief Async set handler for ActiveProfileId. Success means the
     *         device accepted the request; the dispatcher then re-polls this
     *         sensor so the published properties show the device state. */
    requester::Coroutine
        setActiveProfile(const AsyncSetOperationValueType& value,
                         AsyncOperationStatusType* status,
                         std::shared_ptr<NsmDevice> device);

    uint8_t activeProfileId() const
    {
        return activeId;
    }
    const std::vector<ProfileEntry>& availableProfiles() const
    {
        return profiles;
    }
    /** @brief The published interface, or nullptr while not on D-Bus. */
    const PowerProfileIntf* dbusInterface() const
    {
        return profileIntf.get();
    }

  private:
    /** @brief Decode a Get Power Profile reply and publish it.
     *  @return decoder status; cc and reasonCode receive the device codes */
    int decodeAndPublish(const nsm_msg* responseMsg, size_t responseLen,
                         uint8_t& cc, uint16_t& reasonCode);

    sdbusplus::bus_t& bus;
    std::string objPath;
    std::unique_ptr<PowerProfileIntf> profileIntf;
    uint8_t activeId = NSM_POWER_PROFILE_ID_HW_DEFAULT;
    std::vector<ProfileEntry> profiles;
    std::set<uint8_t> knownIds;
};

/** @brief Create the power profile sensor for one NVSwitch on
 *         <objPath><name>/Oem/Nvidia/PowerProfile. */
void createNsmSwitchPowerProfile(std::shared_ptr<NsmDevice> device,
                                 sdbusplus::bus_t& bus,
                                 const std::string& objPath,
                                 const std::string& name,
                                 const std::string& type);

} // namespace nsm

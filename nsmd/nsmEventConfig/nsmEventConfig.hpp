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

#include "device-capability-discovery.h"

#include "nsmObject.hpp"

namespace nsm
{
/**
 * @brief Class for configuring NSM event settings
 *
 * This class handles the configuration of event settings for NSM-capable
 * devices. It inherits from NsmObject to integrate with the NSM framework.
 *
 * The class manages:
 * - Setting event source configurations
 * - Configuring event acknowledgement settings
 * - Converting event IDs to bitmasks for device communication
 */
class NsmEventConfig : public NsmObject
{
  public:
    /**
     * @brief Constructs a new NsmEventConfig object
     *
     * @param name The name identifier for this configuration
     * @param type The type identifier for this configuration
     * @param messageType The NSM message type for events
     * @param srcEventIds Vector of source event IDs to configure
     * @param ackEventIds Vector of acknowledgement event IDs to configure
     */
    NsmEventConfig(const std::string& name, const std::string& type,
                   uint8_t messageType, std::vector<uint64_t>& srcEventIds,
                   std::vector<uint64_t>& ackEventIds);

    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  private:
    void convertIdsToMask(std::vector<uint64_t>& eventIds,
                          std::vector<bitfield8_t>& bitfields);
    /**
     * @brief Configures event sources on the device
     *
     * @param manager Reference to the sensor manager
     * @param eid Device endpoint ID
     * @param nvidiaMessageType NSM message type
     * @param eventIdMasks Bitmasks of event IDs to configure
     * @return Coroutine that resolves to operation status
     */
    requester::Coroutine
        setCurrentEventSources(SensorManager& manager, eid_t eid,
                               uint8_t nvidiaMessageType,
                               std::vector<bitfield8_t>& eventIdMasks);
    /**
     * @brief Configures event acknowledgement settings
     *
     * @param manager Reference to the sensor manager
     * @param eid Device endpoint ID
     * @param nvidiaMessageType NSM message type
     * @param eventIdMasks Bitmasks of event IDs to configure
     * @return Coroutine that resolves to operation status
     */
    requester::Coroutine
        configureEventAcknowledgement(SensorManager& manager, eid_t eid,
                                      uint8_t nvidiaMessageType,
                                      std::vector<bitfield8_t>& eventIdMasks);

    uint8_t messageType;
    std::vector<bitfield8_t> srcEventMask;
    std::vector<bitfield8_t> ackEventMask;
};

/**
 * @brief Class for retrieving and validating NSM event configurations
 *
 * This class handles querying and validating event configurations from NSM
 * devices. It works in conjunction with NsmEventConfig to ensure proper event
 * setup.
 */
class NsmGetEventConfig : public NsmObject
{
  public:
    /**
     * @brief Constructs a new NsmGetEventConfig object
     *
     * @param name The name identifier for this configuration
     * @param type The type identifier for this configuration
     * @param messageType The NSM message type for events
     * @param srcEventIds Vector of source event IDs to validate
     * @param eventConfig Associated NsmEventConfig instance
     */
    NsmGetEventConfig(const std::string& name, const std::string& type,
                      uint8_t messageType, std::vector<uint64_t>& srcEventIds,
                      std::shared_ptr<NsmEventConfig> eventConfig);
    requester::Coroutine update(SensorManager& manager, eid_t eid) override;
    /**
     * @brief Validates configured event IDs against supported events
     *
     * @param eid Device endpoint ID
     * @param supported_event_sources Array of supported event bitmasks
     * @return true if all configured events are supported, false otherwise
     */
    bool validateEventIds(
        eid_t eid, bitfield8_t supported_event_sources[EVENT_SOURCES_LENGTH]);

  private:
    /**
     * @brief Converts event IDs to bitmask format
     *
     * @param eventIds Vector of event IDs to convert
     * @param bitfields Vector to store resulting bitmasks
     */
    void convertIdsToMask(std::vector<uint64_t>& eventIds,
                          std::vector<bitfield8_t>& bitfields);
    uint8_t messageType;
    std::vector<bitfield8_t> srcEventMask;
    std::shared_ptr<NsmEventConfig> eventConfig;
};

} // namespace nsm

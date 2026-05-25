/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "common/types.hpp"
#include "dBusAsyncUtils.hpp"
#include "nsmDevice.hpp"

#include <memory>
#include <string>
#include <vector>

namespace nsm
{

/** Return true when entity-manager DeviceModesSupported enables idx 24. */
bool isLldpModeIndexSupported(const std::vector<int64_t>& deviceModesSupported);

/** Look up the parent NSM_NetworkAdapter EM config by UUID. */
requester::Coroutine coIsLldpPacketSupported(uuid_t uuid, bool& supported);

/** Register per-port RX/TX GetLLDPPacket poll sensors on the round-robin
 *  queue. portNumber is the 0-based NSM port index. */
void createLldpPacketSensorsForPort(sdbusplus::bus::bus& bus,
                                    const std::shared_ptr<NsmDevice>& nsmDevice,
                                    const std::string& sensorNamePrefix,
                                    const std::string& type,
                                    const std::string& portInventoryPath,
                                    uint16_t portNumber, bool priority);

/** Register the adapter-level LLDP mode sensor and async set handlers for
 *  TXMode, RXMode, and DCBXMode. */
void createLldpModeSensor(sdbusplus::bus::bus& bus,
                          const std::shared_ptr<NsmDevice>& nsmDevice,
                          const std::string& name, const std::string& type,
                          const std::string& networkAdapterPath);

} // namespace nsm

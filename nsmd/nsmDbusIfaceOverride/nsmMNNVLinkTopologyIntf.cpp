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

#include "nsmMNNVLinkTopologyIntf.hpp"

namespace nsm
{

NsmMNNVLinkTopologyIntf::NsmMNNVLinkTopologyIntf(sdbusplus::bus::bus& bus,
                                                 const char* path) :
    MNNVLinkTopologyIntf(bus, path)
{
    MNNVLinkTopologyIntf::chassisSerialNumber("NA");
    MNNVLinkTopologyIntf::ibguid("NA");
    MNNVLinkTopologyIntf::traySerialNumber("NA");
    MNNVLinkTopologyIntf::traySlotNumber(std::numeric_limits<uint32_t>::max());
    MNNVLinkTopologyIntf::traySlotIndex(std::numeric_limits<uint32_t>::max());
    MNNVLinkTopologyIntf::hostID(std::numeric_limits<uint32_t>::max());
    MNNVLinkTopologyIntf::peerType("NA");
    MNNVLinkTopologyIntf::moduleID(std::numeric_limits<uint32_t>::max());
};
} // namespace nsm

/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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

#include "nsmAsioInterfaceBase.hpp"

#include <string>

namespace nsm
{

/**
 * @class NsmAsioPortInfoInterface
 * @brief ASIO wrapper for PortInfo D-Bus interface with selective property
 *        registration.
 *
 * Registers only the speed-related properties needed for single-port GPU PCIe
 * and FPGA ports (Type, Protocol, CurrentSpeed, MaxSpeed, TargetSpeed),
 * avoiding MaxReadRequestSizeBytes and MaxPayloadSizeBytes which are only
 * applicable to multi-port devices.
 */
class NsmAsioPortInfoInterface : public NsmAsioInterfaceBase
{
  public:
    static std::unique_ptr<NsmAsioPortInfoInterface>
        createSinglePortDevice(sdbusplus::asio::object_server& objServer,
                               const std::string& objectPath,
                               const std::string& portType,
                               const std::string& portProtocol);

    NsmAsioPortInfoInterface(sdbusplus::asio::object_server& objServer,
                             const std::string& objectPath,
                             const std::string& portType,
                             const std::string& portProtocol);

    void updateSpeeds(double currentSpeed, double maxSpeed, double targetSpeed);

    double getCurrentSpeed() const
    {
        return currentSpeed;
    }

  private:
    bool initialize() override;
    double currentSpeed = 0;
};

} // namespace nsm

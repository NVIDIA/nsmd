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
#include "platform-environmental.h"

#include "globals.hpp"
#include "nsmDevice.hpp"
#include "nsmNumericSensor.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Sensor/Value/server.hpp>
#include <xyz/openbmc_project/Time/EpochTime/server.hpp>

#include <iostream>
#include <limits>

namespace nsm
{
class NsmNumericSensorShmem;
using RequesterHandler = requester::Handler<requester::Request>;
using SensorUnit = sdbusplus::xyz::openbmc_project::Sensor::server::Value::Unit;
using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using AssociationDefinitionsInft = object_t<Association::server::Definitions>;
using ValueIntf = object_t<Sensor::server::Value>;
using TimestampIntf = object_t<Time::server::EpochTime>;

class NsmNumericSensorComposite : public NsmObject
{
  public:
    NsmNumericSensorComposite(
        sdbusplus::bus::bus& bus, const std::string& name,
        const std::vector<utils::Association>& associations,
        const std::string& type, const std::string& path
#ifdef NVIDIA_SHMEM
        ,
        std::unique_ptr<NsmNumericSensorShmem> shmemSensor
#endif
    );
    void updateCompositeReading(std::string childName, double value);

  private:
    std::unique_ptr<ValueIntf> valueIntf = nullptr;
    std::unique_ptr<AssociationDefinitionsInft> associationDefinitionsInft =
        nullptr;
    std::map<std::string, double> childValues;
#ifdef NVIDIA_SHMEM
    std::unique_ptr<NsmNumericSensorShmem> shmemSensor;
#endif
};
} // namespace nsm

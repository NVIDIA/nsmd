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
#include "libnsm/network-ports.h"

#include "common/types.hpp"
#include "globals.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <com/nvidia/Histogram/BucketInfo/server.hpp>
#include <com/nvidia/Histogram/Decorator/Format/server.hpp>
#include <com/nvidia/Histogram/Decorator/SupportedHistogram/server.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>

#include <unordered_map>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using AssociationDefinitionsInft = object_t<Association::server::Definitions>;
using BucketInfoIntf = sdbusplus::server::object_t<
    sdbusplus::server::com::nvidia::histogram::BucketInfo>;
using SupportedHistogramIntf = sdbusplus::server::object_t<
    sdbusplus::server::com::nvidia::histogram::decorator::SupportedHistogram>;
using FormatIntf = sdbusplus::server::object_t<
    sdbusplus::server::com::nvidia::histogram::decorator::Format>;
using BucketUnits =
    sdbusplus::common::com::nvidia::histogram::decorator::Format::BucketUnits;

using BucketDataTypes = sdbusplus::common::com::nvidia::histogram::decorator::
    Format::BucketDataTypes;
using HistogramIds = sdbusplus::common::com::nvidia::histogram::decorator::
    SupportedHistogram::HistogramIds;

class NsmHistogramFormat : public NsmSensor
{
  public:
    NsmHistogramFormat(
        sdbusplus::bus::bus& bus, std::string& name, const std::string& type,
        std::shared_ptr<FormatIntf>& formatIntf,
        std::shared_ptr<BucketInfoIntf>& bucketInfoIntf, std::string& objPath,
        std::vector<std::tuple<std::string, std::string, std::string>>&
            associationsList,
        uint32_t histogramId, uint16_t parameter);
    NsmHistogramFormat() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    std::shared_ptr<FormatIntf> formatIntf = nullptr;
    std::shared_ptr<BucketInfoIntf> bucketInfoIntf = nullptr;
    std::unique_ptr<AssociationDefinitionsInft> associationDefIntf = nullptr;
    std::string histogramName;
    std::string deviceType;
    uint32_t histogramId;
    uint16_t parameter;
};

class NsmHistogramData : public NsmSensor
{
  public:
    NsmHistogramData(std::string& name, const std::string& type,
                     std::shared_ptr<FormatIntf>& formatIntf,
                     std::shared_ptr<BucketInfoIntf>& bucketInfoIntf,
                     uint32_t histogramId, uint16_t parameter);
    NsmHistogramData() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    std::shared_ptr<FormatIntf> formatIntf = nullptr;
    std::shared_ptr<BucketInfoIntf> bucketInfoIntf = nullptr;
    std::string histogramName;
    std::string deviceType;
    uint32_t histogramId;
    uint16_t parameter;
};

} // namespace nsm

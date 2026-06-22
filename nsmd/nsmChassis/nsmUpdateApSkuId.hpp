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

#include "firmware-utils.h"

#include "asyncOperationManager.hpp"
#include "nsmApSkuId.hpp"
#include "nsmObjectFactory.hpp"
#include "utils.hpp"

#include <com/nvidia/UpdateSKU/server.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <memory>

namespace nsm
{

using namespace sdbusplus::server;
using UpdateSKUIntf = object_t<sdbusplus::server::com::nvidia::UpdateSKU>;

/**
 * @brief Marker interface to indicate this chassis supports SKU updates
 * Clients can check for the presence of com.nvidia.UpdateSKU interface
 * to determine if the chassis supports SKU update operations via
 * com.nvidia.Async.Set on the SKU property.
 */
class NsmUpdateApSkuIdIntf : public NsmObject
{
  public:
    NsmUpdateApSkuIdIntf(sdbusplus::bus_t& bus, const std::string& name,
                         const std::string& type,
                         const std::string& inventoryObjPath) :
        NsmObject(name, type), updateSkuIntf(std::make_unique<UpdateSKUIntf>(
                                   bus, inventoryObjPath.c_str()))
    {}

    virtual ~NsmUpdateApSkuIdIntf() = default;

  private:
    std::unique_ptr<UpdateSKUIntf> updateSkuIntf;
};

/**
 * @brief Format AP SKU ID as hex string with 0x prefix
 * @param skuIdValue The SKU ID value to format
 * @return Formatted string (e.g., "0x00001234")
 */
std::string formatApSkuId(uint32_t skuIdValue);

/**
 * @brief Handler for async SKU update operation
 * This function is registered with addAsyncSetOperation to handle
 * writes to the SKU property on the SKU D-Bus interface.
 */
requester::Coroutine updateApSkuIdHandler(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device, uint16_t classification,
    uint16_t identifier, uint8_t index);

} // namespace nsm

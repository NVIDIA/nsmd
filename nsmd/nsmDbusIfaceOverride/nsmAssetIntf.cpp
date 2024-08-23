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

#include "nsmAssetIntf.hpp"

namespace nsm
{
using AssetIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset>;

NsmAssetIntf::NsmAssetIntf(sdbusplus::bus::bus& bus, const char* path) :
    AssetIntf(bus, path)
{
    AssetIntf::sku("NA");
    AssetIntf::name("NA");
    AssetIntf::partNumber("NA");
    AssetIntf::serialNumber("NA");
    AssetIntf::manufacturer("NA");
    AssetIntf::buildDate("0000-00-00T00:00:00Z");
    AssetIntf::model("NA");
};
} // namespace nsm

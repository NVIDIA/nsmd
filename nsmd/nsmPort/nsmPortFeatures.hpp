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

#include "features/feature.hpp"

/** @file Feature creators defined in nsmPort/ */

namespace nsm::features
{

// nsmRetimerPort.cpp
requester::Coroutine
    createMultiPCIeLinkFeature(SensorManager& manager, const DeviceContext& dev,
                               const dbus::PropertyMap& params);

// nsmZone.cpp
requester::Coroutine createFabricsZoneFeature(SensorManager& manager,
                                              const DeviceContext& dev,
                                              const dbus::PropertyMap& params);

// nsmGpuPciePort.cpp
requester::Coroutine createGpuPCIeFeature(SensorManager& manager,
                                          const DeviceContext& dev,
                                          const dbus::PropertyMap& params);
requester::Coroutine
    createGpuPCIePortInfoFeature(SensorManager& manager,
                                 const DeviceContext& dev,
                                 const dbus::PropertyMap& params);

// nsmPort.cpp
requester::Coroutine createNVLinkFeature(SensorManager& manager,
                                         const DeviceContext& dev,
                                         const dbus::PropertyMap& params);

} // namespace nsm::features

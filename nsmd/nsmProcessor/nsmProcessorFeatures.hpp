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

/** @file Feature creators defined in nsmProcessor/ */

namespace nsm::features
{

// nsmProcessor.cpp
requester::Coroutine createProcessorFeature(SensorManager& manager,
                                            const DeviceContext& dev,
                                            const dbus::PropertyMap& params);
requester::Coroutine
    createProcessorAttributesFeature(SensorManager& manager,
                                     const DeviceContext& dev,
                                     const dbus::PropertyMap& params);
requester::Coroutine createPowerCapFeature(SensorManager& manager,
                                           const DeviceContext& dev,
                                           const dbus::PropertyMap& params);
requester::Coroutine
    createWorkloadPowerProfileFeature(SensorManager& manager,
                                      const DeviceContext& dev,
                                      const dbus::PropertyMap& params);
requester::Coroutine
    createProcessorPerformanceFeature(SensorManager& manager,
                                      const DeviceContext& dev,
                                      const dbus::PropertyMap& params);
requester::Coroutine
    createReconfigPermissionsFeature(SensorManager& manager,
                                     const DeviceContext& dev,
                                     const dbus::PropertyMap& params);

} // namespace nsm::features

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

/** @file Feature creators defined in nsmChassis/ */

namespace nsm::features
{

// nsmChassis.cpp
requester::Coroutine
    createChassisAttributesFeature(SensorManager& manager,
                                   const DeviceContext& dev,
                                   const dbus::PropertyMap& params);
requester::Coroutine createChassisFeature(SensorManager& manager,
                                          const DeviceContext& dev,
                                          const dbus::PropertyMap& params);
requester::Coroutine
    createChassisAssociationsFeature(SensorManager& manager,
                                     const DeviceContext& dev,
                                     const dbus::PropertyMap& params);

// nsmNVSwitchAndNVMgmtNICChassis.cpp
requester::Coroutine
    createNVLinkMgmtNicChassisFeature(SensorManager& manager,
                                      const DeviceContext& dev,
                                      const dbus::PropertyMap& params);
requester::Coroutine createNVLinkMgmtNicChassisAttributesFeature(
    SensorManager& manager, const DeviceContext& dev,
    const dbus::PropertyMap& params);

// nsmNVSwitchAndNVMgmtNICChassisAssembly.cpp
requester::Coroutine
    createNVLinkMgmtNicChassisAssemblyFeature(SensorManager& manager,
                                              const DeviceContext& dev,
                                              const dbus::PropertyMap& params);

// nsmChassisAssembly.cpp
requester::Coroutine
    createChassisAssemblyFeature(SensorManager& manager,
                                 const DeviceContext& dev,
                                 const dbus::PropertyMap& params);

// nsmChassisPCIeDevice.cpp
requester::Coroutine
    createChassisPCIeDeviceUUIDFeature(SensorManager& manager,
                                       const DeviceContext& dev,
                                       const dbus::PropertyMap& params);
requester::Coroutine
    createChassisPCIeDeviceAssociationsFeature(SensorManager& manager,
                                               const DeviceContext& dev,
                                               const dbus::PropertyMap& params);
requester::Coroutine
    createChassisPCIeDeviceAssetFeature(SensorManager& manager,
                                        const DeviceContext& dev,
                                        const dbus::PropertyMap& params);
requester::Coroutine
    createChassisPCIeDeviceHealthFeature(SensorManager& manager,
                                         const DeviceContext& dev,
                                         const dbus::PropertyMap& params);
requester::Coroutine
    createChassisPCIeDevicePCIeDeviceFeature(SensorManager& manager,
                                             const DeviceContext& dev,
                                             const dbus::PropertyMap& params);
requester::Coroutine createChassisPCIeDeviceMultiPortPCIeDeviceFeature(
    SensorManager& manager, const DeviceContext& dev,
    const dbus::PropertyMap& params);
requester::Coroutine createChassisPCIeDeviceRetimerAERErrorStatusFeature(
    SensorManager& manager, const DeviceContext& dev,
    const dbus::PropertyMap& params);
#if defined(ENABLE_PCIE_LTSSM_STATE)
requester::Coroutine
    createChassisPCIeDeviceLTSSMStateFeature(SensorManager& manager,
                                             const DeviceContext& dev,
                                             const dbus::PropertyMap& params);
#endif
#if defined(ENABLE_CLOCK_OUTPUT_STATE)
requester::Coroutine createChassisPCIeDeviceClockOutputEnableStateFeature(
    SensorManager& manager, const DeviceContext& dev,
    const dbus::PropertyMap& params);
#endif

// nsmErot.cpp
requester::Coroutine createRoTFeature(SensorManager& manager,
                                      const DeviceContext& dev,
                                      const dbus::PropertyMap& params);
requester::Coroutine
    createFWComponentTypeFeature(SensorManager& manager,
                                 const DeviceContext& dev,
                                 const dbus::PropertyMap& params);

// nsmIRoTResponder.cpp
requester::Coroutine
    createIRoTChassisTypeFeature(SensorManager& manager,
                                 const DeviceContext& dev,
                                 const dbus::PropertyMap& params);
requester::Coroutine
    createIRoTLocationTypeFeature(SensorManager& manager,
                                  const DeviceContext& dev,
                                  const dbus::PropertyMap& params);
requester::Coroutine
    createIRoTLocationContextFeature(SensorManager& manager,
                                     const DeviceContext& dev,
                                     const dbus::PropertyMap& params);
requester::Coroutine
    createIRoTAssociationsFeature(SensorManager& manager,
                                  const DeviceContext& dev,
                                  const dbus::PropertyMap& params);
requester::Coroutine createIRoTAssetFeature(SensorManager& manager,
                                            const DeviceContext& dev,
                                            const dbus::PropertyMap& params);
requester::Coroutine createIRoTHealthFeature(SensorManager& manager,
                                             const DeviceContext& dev,
                                             const dbus::PropertyMap& params);
requester::Coroutine
    createIRoTLocationCodeFeature(SensorManager& manager,
                                  const DeviceContext& dev,
                                  const dbus::PropertyMap& params);
requester::Coroutine
    createIRoTFieldReplaceableFeature(SensorManager& manager,
                                      const DeviceContext& dev,
                                      const dbus::PropertyMap& params);
requester::Coroutine
    createIRoTSPDMResponderFeature(SensorManager& manager,
                                   const DeviceContext& dev,
                                   const dbus::PropertyMap& params);
requester::Coroutine createIRoTSKUFeature(SensorManager& manager,
                                          const DeviceContext& dev,
                                          const dbus::PropertyMap& params);

// nsmLeakDetection.cpp
requester::Coroutine
    createLeakDetectionFeature(SensorManager& manager, const DeviceContext& dev,
                               const dbus::PropertyMap& params);

// nsmGpuClockControl.cpp
requester::Coroutine createClockLimitFeature(SensorManager& manager,
                                             const DeviceContext& dev,
                                             const dbus::PropertyMap& params);

} // namespace nsm::features

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

/** @file Every CreatorFn that device lists may reference. Each module
 *  declares its creators in <module>Features.hpp, next to the .cpp files that
 *  define them; each creator must match CreatorFn.
 */

#include "nsmChassis/nsmChassisFeatures.hpp"
#include "nsmDebugToken/nsmDebugTokenFeatures.hpp"
#include "nsmDeviceInventory/nsmDeviceInventoryFeatures.hpp"
#include "nsmDot/nsmDotFeatures.hpp"
#include "nsmEvent/nsmEventFeatures.hpp"
#include "nsmEventConfig/nsmEventConfigFeatures.hpp"
#include "nsmFwSwInventory/nsmFwSwInventoryFeatures.hpp"
#include "nsmGPM/nsmGPMFeatures.hpp"
#include "nsmMemory/nsmMemoryFeatures.hpp"
#include "nsmNumericSensor/nsmNumericSensorFeatures.hpp"
#include "nsmPort/nsmPortFeatures.hpp"
#include "nsmProcessor/nsmProcessorFeatures.hpp"

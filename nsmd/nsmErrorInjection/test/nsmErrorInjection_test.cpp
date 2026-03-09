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

#include "device-configuration.h"

#include <gtest/gtest.h>

#define private public
#define protected public

#include "../nsmErrorInjection.hpp"

using namespace nsm;

using Type = ErrorInjectionCapabilityIntf::Type;

TEST(GetErrorInjectionBitPosition, MemoryErrors)
{
    EXPECT_EQ(EI_MEMORY_ERRORS,
              getErrorInjectionBitPosition(Type::MemoryErrors));
}

TEST(GetErrorInjectionBitPosition, PCIeErrors)
{
    EXPECT_EQ(EI_PCI_ERRORS, getErrorInjectionBitPosition(Type::PCIeErrors));
}

TEST(GetErrorInjectionBitPosition, NVLinkErrors)
{
    EXPECT_EQ(EI_NVLINK_ERRORS,
              getErrorInjectionBitPosition(Type::NVLinkErrors));
}

TEST(GetErrorInjectionBitPosition, ThermalErrors)
{
    EXPECT_EQ(EI_THERMAL_ERRORS,
              getErrorInjectionBitPosition(Type::ThermalErrors));
}

TEST(GetErrorInjectionBitPosition, FatalErrors)
{
    EXPECT_EQ(EI_DEVICE_ERRORS,
              getErrorInjectionBitPosition(Type::FatalErrors));
}

TEST(GetErrorInjectionBitPosition, PortRecoveryErrors)
{
    EXPECT_EQ(EI_DEVICE_ERRORS,
              getErrorInjectionBitPosition(Type::PortRecoveryErrors));
}

TEST(GetErrorInjectionBitPosition, USBBridgeEmulationErrors)
{
    EXPECT_EQ(EI_DEVICE_ERRORS,
              getErrorInjectionBitPosition(Type::USBBridgeEmulationErrors));
}

TEST(GetErrorInjectionBitPosition, LeakDetectionErrors)
{
    EXPECT_EQ(EI_DEVICE_ERRORS,
              getErrorInjectionBitPosition(Type::LeakDetectionErrors));
}

TEST(GetErrorInjectionBitPosition, GPIOSpoofingErrors)
{
    EXPECT_EQ(EI_GPIO_SPOOFING,
              getErrorInjectionBitPosition(Type::GPIOSpoofingErrors));
}

TEST(GetErrorInjectionBitPosition, UnknownReturnsZero)
{
    EXPECT_EQ(0, getErrorInjectionBitPosition(Type::Unknown));
}

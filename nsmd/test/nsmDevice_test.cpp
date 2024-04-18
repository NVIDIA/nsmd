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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
using ::testing::ElementsAre;

#include "base.h"
#include "platform-environmental.h"

#include <sdbusplus/bus.hpp>

#define private public
#define protected public

#include "nsmDevice.hpp"

TEST(nsmDevice, GoodTest)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";

    nsm::NsmDevice nsmDevice(uuid);
    EXPECT_EQ(nsmDevice.uuid, uuid);

    uint8_t setMode = 2;
    nsmDevice.setEventMode(2);
    auto getMode = nsmDevice.getEventMode();
    EXPECT_EQ(setMode, getMode);
}

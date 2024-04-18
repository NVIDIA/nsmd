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

#include "utils.hpp"

#include <sdbusplus/bus.hpp>

TEST(ConvertUUIDToString, testGoodConversionToString)
{
    std::vector<uint8_t> intUUID{0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                                 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
                                 0x0c, 0x0d, 0x0e, 0x0f};

    // convert intUUID to stringUUID
    uuid_t stringUUID = utils::convertUUIDToString(intUUID);

    EXPECT_STREQ(stringUUID.c_str(), "00010203-0405-0607-0809-0a0b0c0d0e0f\0");
}

TEST(ConvertUUIDToString, testGBadConversionToString)
{
    std::vector<uint8_t> intUUID{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

    // convert intUUID to stringUUID
    uuid_t stringUUID = utils::convertUUIDToString(intUUID);

    EXPECT_STREQ(stringUUID.c_str(), "");
}

TEST(makeDBusNameValid, Functional)
{
    const std::vector<std::array<std::string, 2>> data{
        {{"HGX_GPU_SXM 1 DRAM_0_Temp_0", "HGX_GPU_SXM_1_DRAM_0_Temp_0"}},

        {{"HGX_GPU_SXM    1 &^* DRAM_0_Temp_0", "HGX_GPU_SXM_1_DRAM_0_Temp_0"}},

        {{"/xyz/openbmc_project/inventory/system/processors/GPU_SXM_1",
          "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_1"}},

        {{"/xyz/openbmc_project/inventory/system/processors/GPU_SXM 1 DRAM_0",
          "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_1_DRAM_0"}},

        {{"xyz.openbmc_project.Configuration.NSM_Temp",
          "xyz.openbmc_project.Configuration.NSM_Temp"}},

        {{"xyz.openbmc_project.Sensor.HGX_GPU_SXM 1 DRAM_0_Temp_0",
          "xyz.openbmc_project.Sensor.HGX_GPU_SXM_1_DRAM_0_Temp_0"}},
    };

    for (const auto& e : data)
    {
        EXPECT_STREQ(utils::makeDBusNameValid(e[0]).c_str(), e[1].c_str());
    }
}

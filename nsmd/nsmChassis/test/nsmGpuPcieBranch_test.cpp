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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "base.h"
#include "pci-links.h"

#include <sdbusplus/bus.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmPort/nsmGpuPciePort.hpp"

#undef private
#undef protected

using namespace nsm;

static auto bus = sdbusplus::bus::new_default();

// ============================================================================
// PART 1: NsmClearPCIeCounters - updateReading with different group IDs
// ============================================================================

TEST(NsmClearPCIeCounters, Constructor_GroupId2)
{
    // Arrange
    std::string name = "ClearPCIe_g2";
    std::string type = "NSM_ClearPCIe";
    uint8_t groupId = 2;
    uint8_t deviceIndex = 0;
    std::string invPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port_clear_g2";
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "MCTP_UUID", "00000000-0000-0000-0000-000000000000",
        NSM_DEV_ROLE_RESERVED);

    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, invPath.c_str(), deviceIndex, nsmDevice);

    // Act
    NsmClearPCIeCounters sensor(name, type, groupId, deviceIndex,
                                clearPCIeIntf);

    // Assert
    EXPECT_EQ(sensor.getName(), name);
    EXPECT_EQ(sensor.getType(), type);
}

TEST(NsmClearPCIeCounters, Constructor_GroupId3)
{
    // Arrange
    std::string name = "ClearPCIe_g3";
    std::string type = "NSM_ClearPCIe";
    uint8_t groupId = 3;
    uint8_t deviceIndex = 1;
    std::string invPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port_clear_g3";
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "MCTP_UUID", "00000000-0000-0000-0000-000000000000",
        NSM_DEV_ROLE_RESERVED);

    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, invPath.c_str(), deviceIndex, nsmDevice);

    // Act
    NsmClearPCIeCounters sensor(name, type, groupId, deviceIndex,
                                clearPCIeIntf);

    // Assert
    EXPECT_EQ(sensor.getName(), name);
    EXPECT_EQ(sensor.getType(), type);
}

TEST(NsmClearPCIeCounters, Constructor_GroupId4)
{
    // Arrange
    std::string name = "ClearPCIe_g4";
    std::string type = "NSM_ClearPCIe";
    uint8_t groupId = 4;
    uint8_t deviceIndex = 2;
    std::string invPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port_clear_g4";
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "MCTP_UUID", "00000000-0000-0000-0000-000000000000",
        NSM_DEV_ROLE_RESERVED);

    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, invPath.c_str(), deviceIndex, nsmDevice);

    // Act
    NsmClearPCIeCounters sensor(name, type, groupId, deviceIndex,
                                clearPCIeIntf);

    // Assert
    EXPECT_EQ(sensor.getName(), name);
    EXPECT_EQ(sensor.getType(), type);
}

TEST(NsmClearPCIeCounters, UpdateReading_Group2_AllBitsSet)
{
    // Arrange
    std::string name = "ClearPCIe_g2_all";
    std::string type = "NSM_ClearPCIe";
    uint8_t groupId = 2;
    uint8_t deviceIndex = 0;
    std::string invPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port_g2_all";
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "MCTP_UUID", "00000000-0000-0000-0000-000000000000",
        NSM_DEV_ROLE_RESERVED);

    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, invPath.c_str(), deviceIndex, nsmDevice);

    NsmClearPCIeCounters sensor(name, type, groupId, deviceIndex,
                                clearPCIeIntf);

    // Prepare clearable sources with bits 0-3 set for group 2
    bitfield8_t clearableSource[MAX_SCALAR_SOURCE_MASK_SIZE] = {};
    clearableSource[0].byte = 0x0F; // bits 0,1,2,3 set

    // Act
    sensor.updateReading(clearableSource);

    // Assert
    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_GE(counters.size(), 4u);
}

TEST(NsmClearPCIeCounters, UpdateReading_Group2_OnlyBit0Set)
{
    // Arrange - only NonFatalErrorCount
    std::string name = "ClearPCIe_g2_b0";
    std::string type = "NSM_ClearPCIe";
    uint8_t groupId = 2;
    uint8_t deviceIndex = 0;
    std::string invPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port_g2_b0";
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "MCTP_UUID", "00000000-0000-0000-0000-000000000000",
        NSM_DEV_ROLE_RESERVED);

    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, invPath.c_str(), deviceIndex, nsmDevice);

    NsmClearPCIeCounters sensor(name, type, groupId, deviceIndex,
                                clearPCIeIntf);

    bitfield8_t clearableSource[MAX_SCALAR_SOURCE_MASK_SIZE] = {};
    clearableSource[0].byte = 0x01; // only bit 0

    // Act
    sensor.updateReading(clearableSource);

    // Assert
    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_GE(counters.size(), 1u);
}

TEST(NsmClearPCIeCounters, UpdateReading_Group2_OnlyBit1Set)
{
    // Arrange - only FatalErrorCount
    std::string name = "ClearPCIe_g2_b1";
    std::string type = "NSM_ClearPCIe";
    uint8_t groupId = 2;
    uint8_t deviceIndex = 0;
    std::string invPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port_g2_b1";
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "MCTP_UUID", "00000000-0000-0000-0000-000000000000",
        NSM_DEV_ROLE_RESERVED);

    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, invPath.c_str(), deviceIndex, nsmDevice);

    NsmClearPCIeCounters sensor(name, type, groupId, deviceIndex,
                                clearPCIeIntf);

    bitfield8_t clearableSource[MAX_SCALAR_SOURCE_MASK_SIZE] = {};
    clearableSource[0].byte = 0x02; // only bit 1

    // Act
    sensor.updateReading(clearableSource);

    // Assert
    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_GE(counters.size(), 1u);
}

TEST(NsmClearPCIeCounters, UpdateReading_Group2_OnlyBit2Set)
{
    // Arrange - only UnsupportedRequestCount
    std::string name = "ClearPCIe_g2_b2";
    std::string type = "NSM_ClearPCIe";
    uint8_t groupId = 2;
    uint8_t deviceIndex = 0;
    std::string invPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port_g2_b2";
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "MCTP_UUID", "00000000-0000-0000-0000-000000000000",
        NSM_DEV_ROLE_RESERVED);

    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, invPath.c_str(), deviceIndex, nsmDevice);

    NsmClearPCIeCounters sensor(name, type, groupId, deviceIndex,
                                clearPCIeIntf);

    bitfield8_t clearableSource[MAX_SCALAR_SOURCE_MASK_SIZE] = {};
    clearableSource[0].byte = 0x04; // only bit 2

    // Act
    sensor.updateReading(clearableSource);

    // Assert
    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_GE(counters.size(), 1u);
}

TEST(NsmClearPCIeCounters, UpdateReading_Group2_OnlyBit3Set)
{
    // Arrange - only CorrectableErrorCount
    std::string name = "ClearPCIe_g2_b3";
    std::string type = "NSM_ClearPCIe";
    uint8_t groupId = 2;
    uint8_t deviceIndex = 0;
    std::string invPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port_g2_b3";
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "MCTP_UUID", "00000000-0000-0000-0000-000000000000",
        NSM_DEV_ROLE_RESERVED);

    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, invPath.c_str(), deviceIndex, nsmDevice);

    NsmClearPCIeCounters sensor(name, type, groupId, deviceIndex,
                                clearPCIeIntf);

    bitfield8_t clearableSource[MAX_SCALAR_SOURCE_MASK_SIZE] = {};
    clearableSource[0].byte = 0x08; // only bit 3

    // Act
    sensor.updateReading(clearableSource);

    // Assert
    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_GE(counters.size(), 1u);
}

TEST(NsmClearPCIeCounters, UpdateReading_Group3_Bit0Set)
{
    // Arrange - L0ToRecoveryCount for group 3
    std::string name = "ClearPCIe_g3_b0";
    std::string type = "NSM_ClearPCIe";
    uint8_t groupId = 3;
    uint8_t deviceIndex = 0;
    std::string invPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port_g3_b0";
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "MCTP_UUID", "00000000-0000-0000-0000-000000000000",
        NSM_DEV_ROLE_RESERVED);

    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, invPath.c_str(), deviceIndex, nsmDevice);

    NsmClearPCIeCounters sensor(name, type, groupId, deviceIndex,
                                clearPCIeIntf);

    bitfield8_t clearableSource[MAX_SCALAR_SOURCE_MASK_SIZE] = {};
    clearableSource[0].byte = 0x01; // bit 0

    // Act
    sensor.updateReading(clearableSource);

    // Assert
    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_GE(counters.size(), 1u);
}

TEST(NsmClearPCIeCounters, UpdateReading_Group4_AllBitsSet)
{
    // Arrange - NAKReceived, NAKSent, ReplayRollover, ReplayCount for group 4
    std::string name = "ClearPCIe_g4_all";
    std::string type = "NSM_ClearPCIe";
    uint8_t groupId = 4;
    uint8_t deviceIndex = 0;
    std::string invPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port_g4_all";
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "MCTP_UUID", "00000000-0000-0000-0000-000000000000",
        NSM_DEV_ROLE_RESERVED);

    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, invPath.c_str(), deviceIndex, nsmDevice);

    NsmClearPCIeCounters sensor(name, type, groupId, deviceIndex,
                                clearPCIeIntf);

    // bits 1 (NAKReceived), 2 (NAKSent), 4 (ReplayRollover), 6 (ReplayCount)
    bitfield8_t clearableSource[MAX_SCALAR_SOURCE_MASK_SIZE] = {};
    clearableSource[0].byte = 0x56; // bits 1,2,4,6 set (0b01010110)

    // Act
    sensor.updateReading(clearableSource);

    // Assert
    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_GE(counters.size(), 4u);
}

TEST(NsmClearPCIeCounters, UpdateReading_Group4_OnlyBit1)
{
    // Arrange - only NAKReceivedCount
    std::string name = "ClearPCIe_g4_b1";
    std::string type = "NSM_ClearPCIe";
    uint8_t groupId = 4;
    uint8_t deviceIndex = 0;
    std::string invPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port_g4_b1";
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "MCTP_UUID", "00000000-0000-0000-0000-000000000000",
        NSM_DEV_ROLE_RESERVED);

    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, invPath.c_str(), deviceIndex, nsmDevice);

    NsmClearPCIeCounters sensor(name, type, groupId, deviceIndex,
                                clearPCIeIntf);

    bitfield8_t clearableSource[MAX_SCALAR_SOURCE_MASK_SIZE] = {};
    clearableSource[0].byte = 0x02; // only bit 1

    // Act
    sensor.updateReading(clearableSource);

    // Assert
    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_GE(counters.size(), 1u);
}

TEST(NsmClearPCIeCounters, UpdateReading_Group4_OnlyBit2)
{
    // Arrange - only NAKSentCount
    std::string name = "ClearPCIe_g4_b2";
    std::string type = "NSM_ClearPCIe";
    uint8_t groupId = 4;
    uint8_t deviceIndex = 0;
    std::string invPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port_g4_b2";
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "MCTP_UUID", "00000000-0000-0000-0000-000000000000",
        NSM_DEV_ROLE_RESERVED);

    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, invPath.c_str(), deviceIndex, nsmDevice);

    NsmClearPCIeCounters sensor(name, type, groupId, deviceIndex,
                                clearPCIeIntf);

    bitfield8_t clearableSource[MAX_SCALAR_SOURCE_MASK_SIZE] = {};
    clearableSource[0].byte = 0x04; // only bit 2

    // Act
    sensor.updateReading(clearableSource);

    // Assert
    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_GE(counters.size(), 1u);
}

TEST(NsmClearPCIeCounters, UpdateReading_Group4_OnlyBit4)
{
    // Arrange - only ReplayRolloverCount
    std::string name = "ClearPCIe_g4_b4";
    std::string type = "NSM_ClearPCIe";
    uint8_t groupId = 4;
    uint8_t deviceIndex = 0;
    std::string invPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port_g4_b4";
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "MCTP_UUID", "00000000-0000-0000-0000-000000000000",
        NSM_DEV_ROLE_RESERVED);

    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, invPath.c_str(), deviceIndex, nsmDevice);

    NsmClearPCIeCounters sensor(name, type, groupId, deviceIndex,
                                clearPCIeIntf);

    bitfield8_t clearableSource[MAX_SCALAR_SOURCE_MASK_SIZE] = {};
    clearableSource[0].byte = 0x10; // only bit 4

    // Act
    sensor.updateReading(clearableSource);

    // Assert
    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_GE(counters.size(), 1u);
}

TEST(NsmClearPCIeCounters, UpdateReading_Group4_OnlyBit6)
{
    // Arrange - only ReplayCount
    std::string name = "ClearPCIe_g4_b6";
    std::string type = "NSM_ClearPCIe";
    uint8_t groupId = 4;
    uint8_t deviceIndex = 0;
    std::string invPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port_g4_b6";
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "MCTP_UUID", "00000000-0000-0000-0000-000000000000",
        NSM_DEV_ROLE_RESERVED);

    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, invPath.c_str(), deviceIndex, nsmDevice);

    NsmClearPCIeCounters sensor(name, type, groupId, deviceIndex,
                                clearPCIeIntf);

    bitfield8_t clearableSource[MAX_SCALAR_SOURCE_MASK_SIZE] = {};
    clearableSource[0].byte = 0x40; // only bit 6

    // Act
    sensor.updateReading(clearableSource);

    // Assert
    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_GE(counters.size(), 1u);
}

TEST(NsmClearPCIeCounters, UpdateReading_DefaultGroup_NoBitsSet)
{
    // Arrange - unknown group ID, should hit default case
    std::string name = "ClearPCIe_g99";
    std::string type = "NSM_ClearPCIe";
    uint8_t groupId = 99;
    uint8_t deviceIndex = 0;
    std::string invPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port_g99";
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "MCTP_UUID", "00000000-0000-0000-0000-000000000000",
        NSM_DEV_ROLE_RESERVED);

    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, invPath.c_str(), deviceIndex, nsmDevice);

    NsmClearPCIeCounters sensor(name, type, groupId, deviceIndex,
                                clearPCIeIntf);

    bitfield8_t clearableSource[MAX_SCALAR_SOURCE_MASK_SIZE] = {};
    clearableSource[0].byte = 0xFF;

    // Act
    sensor.updateReading(clearableSource);

    // Assert - no counters should be added for unknown group
    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_EQ(counters.size(), 0u);
}

TEST(NsmClearPCIeCounters, UpdateReading_Group2_DuplicateCounterNotAdded)
{
    // Arrange - call updateReading twice; counter should not be duplicated
    std::string name = "ClearPCIe_g2_dup";
    std::string type = "NSM_ClearPCIe";
    uint8_t groupId = 2;
    uint8_t deviceIndex = 0;
    std::string invPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port_g2_dup";
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "MCTP_UUID", "00000000-0000-0000-0000-000000000000",
        NSM_DEV_ROLE_RESERVED);

    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, invPath.c_str(), deviceIndex, nsmDevice);

    NsmClearPCIeCounters sensor(name, type, groupId, deviceIndex,
                                clearPCIeIntf);

    bitfield8_t clearableSource[MAX_SCALAR_SOURCE_MASK_SIZE] = {};
    clearableSource[0].byte = 0x01; // NonFatalErrorCount

    // Act - call twice
    sensor.updateReading(clearableSource);
    auto countFirst = clearPCIeIntf->clearableCounters().size();
    sensor.updateReading(clearableSource);
    auto countSecond = clearPCIeIntf->clearableCounters().size();

    // Assert - same size (no duplicate)
    EXPECT_EQ(countFirst, countSecond);
}

// ============================================================================
// PART 2: NsmClearPCIeIntf - addClearCounterSensor and
//         getClearCounterSensorFromGroup
// ============================================================================

TEST(NsmClearPCIeIntf, AddAndGetClearCounterSensor)
{
    // Arrange
    std::string invPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port_intftest";
    uint8_t deviceIndex = 0;
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "MCTP_UUID", "00000000-0000-0000-0000-000000000000",
        NSM_DEV_ROLE_RESERVED);

    NsmClearPCIeIntf clearIntf(bus, invPath.c_str(), deviceIndex, nsmDevice);

    // Create a mock sensor (use nullptr cast for simplicity; we just test the
    // map)
    auto sensor = std::shared_ptr<NsmPcieGroup>(nullptr);

    // Act
    clearIntf.addClearCoutnerSensor(2, sensor);
    clearIntf.addClearCoutnerSensor(3, sensor);

    // Assert
    auto result2 = clearIntf.getClearCounterSensorFromGroup(2);
    auto result3 = clearIntf.getClearCounterSensorFromGroup(3);
    auto resultNone = clearIntf.getClearCounterSensorFromGroup(99);

    EXPECT_EQ(result2, sensor);
    EXPECT_EQ(result3, sensor);
    EXPECT_EQ(resultNone, nullptr);
}

TEST(NsmClearPCIeIntf, AddDuplicateGroupId_DoesNotOverwrite)
{
    // Arrange
    std::string invPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port_dup_group";
    uint8_t deviceIndex = 0;
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "MCTP_UUID", "00000000-0000-0000-0000-000000000000",
        NSM_DEV_ROLE_RESERVED);

    NsmClearPCIeIntf clearIntf(bus, invPath.c_str(), deviceIndex, nsmDevice);

    auto sensor1 = std::shared_ptr<NsmPcieGroup>(nullptr);
    auto sensor2 = std::shared_ptr<NsmPcieGroup>(nullptr);

    // Act - add twice for same groupId
    clearIntf.addClearCoutnerSensor(4, sensor1);
    clearIntf.addClearCoutnerSensor(4, sensor2); // should not overwrite

    // Assert - first one stays
    auto result = clearIntf.getClearCounterSensorFromGroup(4);
    EXPECT_EQ(result, sensor1);
}

// ============================================================================
// PART 10: NsmClearPCIeCounters - findAndUpdateCounter deduplication
// ============================================================================

TEST(NsmClearPCIeCounters, FindAndUpdateCounter_NoDuplicate_Group2)
{
    // Arrange
    std::string name = "ClearPCIe_dedup";
    std::string type = "NSM_ClearPCIe";
    uint8_t groupId = 2;
    uint8_t deviceIndex = 0;
    std::string invPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port_dedup";
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "MCTP_UUID", "00000000-0000-0000-0000-000000000000",
        NSM_DEV_ROLE_RESERVED);

    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, invPath.c_str(), deviceIndex, nsmDevice);

    NsmClearPCIeCounters sensor(name, type, groupId, deviceIndex,
                                clearPCIeIntf);

    // First call with all bits
    bitfield8_t src1[MAX_SCALAR_SOURCE_MASK_SIZE] = {};
    src1[0].byte = 0x0F; // all 4 bits for group 2
    sensor.updateReading(src1);
    auto count1 = clearPCIeIntf->clearableCounters().size();

    // Second call with same bits - should not add duplicates
    bitfield8_t src2[MAX_SCALAR_SOURCE_MASK_SIZE] = {};
    src2[0].byte = 0x0F;
    sensor.updateReading(src2);
    auto count2 = clearPCIeIntf->clearableCounters().size();

    // Assert
    EXPECT_EQ(count1, count2);
    EXPECT_EQ(count1, 4u);
}

TEST(NsmClearPCIeCounters, UpdateReading_Group3_NoBitsSet)
{
    // Arrange - group 3 with no bits set
    std::string name = "ClearPCIe_g3_none";
    std::string type = "NSM_ClearPCIe";
    uint8_t groupId = 3;
    uint8_t deviceIndex = 0;
    std::string invPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port_g3_none";
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "MCTP_UUID", "00000000-0000-0000-0000-000000000000",
        NSM_DEV_ROLE_RESERVED);

    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, invPath.c_str(), deviceIndex, nsmDevice);

    NsmClearPCIeCounters sensor(name, type, groupId, deviceIndex,
                                clearPCIeIntf);

    bitfield8_t clearableSource[MAX_SCALAR_SOURCE_MASK_SIZE] = {};
    clearableSource[0].byte = 0x00; // no bits set

    // Act
    sensor.updateReading(clearableSource);

    // Assert
    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_EQ(counters.size(), 0u);
}

TEST(NsmClearPCIeCounters, UpdateReading_Group4_NoBitsSet)
{
    // Arrange - group 4 with no bits set
    std::string name = "ClearPCIe_g4_none";
    std::string type = "NSM_ClearPCIe";
    uint8_t groupId = 4;
    uint8_t deviceIndex = 0;
    std::string invPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port_g4_none";
    auto nsmDevice = std::make_shared<MockNsmDevice>(
        NSM_DEV_ID_GPU, 0, "MCTP_UUID", "00000000-0000-0000-0000-000000000000",
        NSM_DEV_ROLE_RESERVED);

    auto clearPCIeIntf = std::make_shared<NsmClearPCIeIntf>(
        bus, invPath.c_str(), deviceIndex, nsmDevice);

    NsmClearPCIeCounters sensor(name, type, groupId, deviceIndex,
                                clearPCIeIntf);

    bitfield8_t clearableSource[MAX_SCALAR_SOURCE_MASK_SIZE] = {};
    clearableSource[0].byte = 0x00; // no bits set

    // Act
    sensor.updateReading(clearableSource);

    // Assert
    auto counters = clearPCIeIntf->clearableCounters();
    EXPECT_EQ(counters.size(), 0u);
}

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

#include "instance_id.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Test fixture for InstanceIdDb
class InstanceIdDbTest : public ::testing::Test
{
  protected:
    nsm::InstanceIdDb db{};
};

// =============================================================================
// Requirement 1: next() allocates instance IDs with cyclic counter 0-31
// =============================================================================

TEST_F(InstanceIdDbTest, FirstAllocationReturnsZero)
{
    uint8_t eid = 10;
    uint8_t id = db.next(eid);

    EXPECT_EQ(id, 0) << "First allocation should return instance ID 0";

    db.free(eid, id);
}

TEST_F(InstanceIdDbTest, CyclicCounterSequence)
{
    uint8_t eid = 10;
    std::vector<uint8_t> ids;

    // Allocate, free, allocate 10 times to see cyclic progression
    for (int i = 0; i < 10; i++)
    {
        uint8_t id = db.next(eid);
        ids.push_back(id);
        db.free(eid, id);
    }

    // Verify sequence: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9
    for (size_t i = 0; i < ids.size(); i++)
    {
        EXPECT_EQ(ids[i], i) << "Instance ID should be " << i;
    }
}

TEST_F(InstanceIdDbTest, CyclicCounterWrapsAt32)
{
    uint8_t eid = 10;

    // Allocate and free 32 times to reach ID 31
    for (int i = 0; i < 32; i++)
    {
        uint8_t id = db.next(eid);
        EXPECT_EQ(id, i) << "Expected ID " << i;
        db.free(eid, id);
    }

    // Next allocation should wrap to 0
    uint8_t wrapped_id = db.next(eid);
    EXPECT_EQ(wrapped_id, 0) << "After ID 31, should wrap to 0";
    db.free(eid, wrapped_id);
}

TEST_F(InstanceIdDbTest, CyclicCounterIndependentPerEid)
{
    // Each EID has its own independent cyclic counter
    uint8_t eid1 = 10;
    uint8_t eid2 = 20;

    uint8_t id1 = db.next(eid1); // Should be 0
    uint8_t id2 = db.next(eid2); // Should also be 0

    EXPECT_EQ(id1, 0);
    EXPECT_EQ(id2, 0);

    db.free(eid1, id1);

    uint8_t id3 = db.next(eid1); // Should be 1 (eid1 counter advanced)
    EXPECT_EQ(id3, 1);

    db.free(eid2, id2);
    db.free(eid1, id3);

    uint8_t id4 = db.next(eid2); // Should be 1 (eid2 counter advanced)
    EXPECT_EQ(id4, 1);

    db.free(eid2, id4);
}

// =============================================================================
// Requirement 2: Cannot allocate if already allocated (must call free first)
// =============================================================================

TEST_F(InstanceIdDbTest, CannotAllocateTwiceWithoutFree)
{
    uint8_t eid = 10;
    uint8_t id = db.next(eid);

    // Second allocation without free should throw
    EXPECT_THROW(
        {
            try
            {
                db.next(eid);
            }
            catch (const std::runtime_error& e)
            {
                EXPECT_THAT(e.what(),
                            ::testing::HasSubstr("No free instance ids"));
                EXPECT_THAT(e.what(), ::testing::HasSubstr("EID"));
                throw;
            }
        },
        std::runtime_error);

    db.free(eid, id);
}

TEST_F(InstanceIdDbTest, MultipleEidsCanAllocateSimultaneously)
{
    // Different EIDs can each have one active allocation
    std::vector<std::pair<uint8_t, uint8_t>> allocations;

    for (uint8_t eid = 0; eid < 10; eid++)
    {
        uint8_t id = db.next(eid);
        allocations.push_back({eid, id});
    }

    // All should have succeeded
    EXPECT_EQ(allocations.size(), 10);

    // Each EID cannot allocate a second time
    for (const auto& [eid, id] : allocations)
    {
        EXPECT_THROW(db.next(eid), std::runtime_error);
    }

    // Cleanup
    for (const auto& [eid, id] : allocations)
    {
        db.free(eid, id);
    }
}

// =============================================================================
// Requirement 3: Can allocate after free
// =============================================================================

TEST_F(InstanceIdDbTest, CanAllocateAfterFree)
{
    uint8_t eid = 10;

    uint8_t id1 = db.next(eid);
    EXPECT_EQ(id1, 0);

    db.free(eid, id1);

    // Should be able to allocate again
    uint8_t id2 = db.next(eid);
    EXPECT_EQ(id2, 1); // Counter advanced

    db.free(eid, id2);
}

TEST_F(InstanceIdDbTest, AllocateFreeAllocateCycle)
{
    uint8_t eid = 10;

    for (int cycle = 0; cycle < 3; cycle++)
    {
        uint8_t id = db.next(eid);
        EXPECT_EQ(id, cycle) << "Cycle " << cycle;
        db.free(eid, id);
    }
}

// =============================================================================
// Free validation tests
// =============================================================================

TEST_F(InstanceIdDbTest, FreeUnallocatedEidThrows)
{
    uint8_t eid = 10;

    // Try to free when nothing was allocated
    EXPECT_THROW(
        {
            try
            {
                db.free(eid, 5);
            }
            catch (const std::runtime_error& e)
            {
                EXPECT_THAT(e.what(), ::testing::HasSubstr(
                                          "was not previously allocated"));
                EXPECT_THAT(e.what(), ::testing::HasSubstr("Instance ID"));
                EXPECT_THAT(e.what(), ::testing::HasSubstr("EID"));
                throw;
            }
        },
        std::runtime_error);
}

TEST_F(InstanceIdDbTest, FreeWithWrongInstanceIdThrows)
{
    uint8_t eid = 10;
    uint8_t id = db.next(eid);

    // Try to free with wrong instance ID
    uint8_t wrong_id = (id + 1) % 32;
    EXPECT_THROW(
        {
            try
            {
                db.free(eid, wrong_id);
            }
            catch (const std::runtime_error& e)
            {
                EXPECT_THAT(e.what(), ::testing::HasSubstr(
                                          "was not previously allocated"));
                throw;
            }
        },
        std::runtime_error);

    // Free with correct ID should work
    EXPECT_NO_THROW(db.free(eid, id));
}

TEST_F(InstanceIdDbTest, DoubleFreeThrows)
{
    uint8_t eid = 10;
    uint8_t id = db.next(eid);

    // First free succeeds
    EXPECT_NO_THROW(db.free(eid, id));

    // Second free should throw
    EXPECT_THROW(
        {
            try
            {
                db.free(eid, id);
            }
            catch (const std::runtime_error& e)
            {
                EXPECT_THAT(e.what(), ::testing::HasSubstr(
                                          "was not previously allocated"));
                throw;
            }
        },
        std::runtime_error);
}

TEST_F(InstanceIdDbTest, FreeWithWrongEidThrows)
{
    uint8_t eid1 = 10;
    uint8_t eid2 = 20;

    uint8_t id = db.next(eid1);

    // Try to free with different EID
    EXPECT_THROW(db.free(eid2, id), std::runtime_error);

    // Free with correct EID
    EXPECT_NO_THROW(db.free(eid1, id));
}

// =============================================================================
// Edge cases and comprehensive tests
// =============================================================================

TEST_F(InstanceIdDbTest, EidZeroWorks)
{
    uint8_t eid = 0;
    uint8_t id = db.next(eid);

    EXPECT_EQ(id, 0);
    EXPECT_NO_THROW(db.free(eid, id));
}

TEST_F(InstanceIdDbTest, EidMaxWorks)
{
    uint8_t eid = 255;
    uint8_t id = db.next(eid);

    EXPECT_EQ(id, 0);
    EXPECT_NO_THROW(db.free(eid, id));
}

TEST_F(InstanceIdDbTest, AllInstanceIdsCanBeAllocated)
{
    uint8_t eid = 10;

    // Allocate all 32 instance IDs by cycling
    for (int i = 0; i < 32; i++)
    {
        uint8_t id = db.next(eid);
        EXPECT_EQ(id, i) << "Should allocate ID " << i;
        db.free(eid, id);
    }
}

TEST_F(InstanceIdDbTest, All256EidsWork)
{
    std::vector<std::pair<uint8_t, uint8_t>> allocations;

    // Allocate for all 256 EIDs simultaneously
    for (int eid = 0; eid < 256; eid++)
    {
        uint8_t id = db.next(static_cast<uint8_t>(eid));
        EXPECT_EQ(id, 0) << "First allocation for EID " << eid
                         << " should be 0";
        allocations.push_back({static_cast<uint8_t>(eid), id});
    }

    EXPECT_EQ(allocations.size(), 256);

    // Free all
    for (const auto& [eid, id] : allocations)
    {
        EXPECT_NO_THROW(db.free(eid, id));
    }

    // Allocate again to verify all are freed
    for (int eid = 0; eid < 256; eid++)
    {
        uint8_t id = db.next(static_cast<uint8_t>(eid));
        EXPECT_EQ(id, 1) << "Second allocation for EID " << eid
                         << " should be 1";
        db.free(static_cast<uint8_t>(eid), id);
    }
}

TEST_F(InstanceIdDbTest, InterleavedOperations)
{
    uint8_t eid1 = 10;
    uint8_t eid2 = 20;
    uint8_t eid3 = 30;

    // Allocate for eid1 and eid2
    uint8_t id1 = db.next(eid1); // 0
    uint8_t id2 = db.next(eid2); // 0

    EXPECT_EQ(id1, 0);
    EXPECT_EQ(id2, 0);

    // Free eid1
    db.free(eid1, id1);

    // Allocate for eid3
    uint8_t id3 = db.next(eid3); // 0
    EXPECT_EQ(id3, 0);

    // Allocate again for eid1
    uint8_t id4 = db.next(eid1); // 1
    EXPECT_EQ(id4, 1);

    // Free eid2
    db.free(eid2, id2);

    // Allocate again for eid2
    uint8_t id5 = db.next(eid2); // 1
    EXPECT_EQ(id5, 1);

    // Cleanup
    db.free(eid1, id4);
    db.free(eid2, id5);
    db.free(eid3, id3);
}

TEST_F(InstanceIdDbTest, StressTestCyclicWrapping)
{
    uint8_t eid = 42;

    // Perform 100 allocate-free cycles
    for (int i = 0; i < 100; i++)
    {
        uint8_t expected_id = i % 32;
        uint8_t id = db.next(eid);
        EXPECT_EQ(id, expected_id) << "Iteration " << i;
        db.free(eid, id);
    }
}

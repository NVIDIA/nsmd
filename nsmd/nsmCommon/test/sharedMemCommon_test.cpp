/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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

/*
 * Tests for nsmd/nsmCommon/sharedMemCommon.cpp
 *
 * cacheTALData()               - covered by existing processor/inventory tests
 *                                (pushes to static vector, no shmem access)
 * updateAggregateTelemetryOnTAL() - covered here: with non-empty telemetryData
 *                                   the for-loop body (line 47) executes.
 *                                   tal::TelemetryAggregator safely no-ops
 *                                   when talInit==false.
 * initTALNamespace()           - covered here: calls namespaceInit + reserve.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#include "config.h"

#ifdef NVIDIA_SHMEM

#define private public
#define protected public

#include "sharedMemCommon.hpp"

using namespace nsm_shmem_utils;

// =============================================================================
// updateAggregateTelemetryOnTAL
// =============================================================================

// When telemetryData is non-empty, the for-loop body (data.timestamp =
// timestamp) executes. tal::TelemetryAggregator::updateAggregateTelemetry is
// safe to call when talInit==false – it logs once and returns.
TEST(SharedMemoryManager,
     UpdateAggregateTelemetryOnTAL_NonEmptyData_ClearsVector)
{
    // Arrange: populate the static vector via cacheTALData
    std::vector<uint8_t> raw = {0x01, 0x02};
    nv::sensor_aggregation::DbusVariantType val{uint32_t{42}};
    SharedMemoryManager::cacheTALData("/test/path", "TestIface", "TestProp",
                                      raw, val, "/assoc/path", 0);
    ASSERT_FALSE(SharedMemoryManager::telemetryData.empty());

    // Act: call updateAggregateTelemetryOnTAL — covers loop body (line 47)
    // and the updateAggregateTelemetry + clear calls.
    EXPECT_NO_THROW(SharedMemoryManager::updateAggregateTelemetryOnTAL());

    // Assert: vector is cleared after the call
    EXPECT_TRUE(SharedMemoryManager::telemetryData.empty());
}

// When telemetryData is already empty, the loop body is skipped but the
// updateAggregateTelemetry and clear calls are still reached.
TEST(SharedMemoryManager, UpdateAggregateTelemetryOnTAL_EmptyData_NoOp)
{
    // Ensure the vector is empty (previous test cleared it)
    SharedMemoryManager::telemetryData.clear();

    EXPECT_NO_THROW(SharedMemoryManager::updateAggregateTelemetryOnTAL());
    EXPECT_TRUE(SharedMemoryManager::telemetryData.empty());
}

// =============================================================================
// initTALNamespace
// =============================================================================

// Covers lines 55-63: namespaceInit call and telemetryData.reserve(20).
// namespaceInit may return false in the unit-test environment (no real shmem),
// but it must not throw.
TEST(SharedMemoryManager, InitTALNamespace_DoesNotThrow)
{
    EXPECT_NO_THROW(SharedMemoryManager::initTALNamespace());
    // After init, telemetryData capacity is at least 20.
    EXPECT_GE(SharedMemoryManager::telemetryData.capacity(), 20u);
}

#endif // NVIDIA_SHMEM

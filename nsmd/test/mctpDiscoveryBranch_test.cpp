/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage tests for requester/mctp_endpoint_discovery.cpp
 *
 * Uses a helper TU (mctpDiscoveryBranchHelper.cpp) to avoid including
 * mctp_endpoint_discovery.hpp in the same TU as sdbusplus::asio::connection.
 *
 * BLOCKED: MctpDiscovery constructor throws "basic_string: construction
 * from null" because sdbusplus::bus::match::rules generates a string
 * from the bus unique_name which is null in the test D-Bus environment.
 * The std::optional<match_t> production fix moved emplace into try/catch,
 * but the exception propagates before being caught (possibly from
 * match_rules:: helper itself, not from sd_bus_add_match).
 * Instance tests are disabled until match_rules null-safety is fixed.
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "base.h"

#include <sdbusplus/bus.hpp>

#include <gtest/gtest.h>

#define private public
#define protected public
#include "nsmDevice.hpp"
#undef private
#undef protected

using namespace nsm;
using namespace mctp;

// Wrappers from mctpDiscoveryBranchHelper.cpp
namespace mctp
{
void resetMctpDiscoveryForTest();
MctpDiscovery& getMctpDiscoveryInstance();
void testLogProberSummaries();
} // namespace mctp

// ============================================================================
struct MctpDiscoveryStaticTest : public Test
{
    MctpDiscoveryStaticTest()
    {
        mctp::resetMctpDiscoveryForTest();
    }
    ~MctpDiscoveryStaticTest()
    {
        mctp::resetMctpDiscoveryForTest();
    }
};

TEST_F(MctpDiscoveryStaticTest, GetInstance_Throws)
{
    EXPECT_THROW(mctp::getMctpDiscoveryInstance(), std::runtime_error);
}

TEST_F(MctpDiscoveryStaticTest, LogProber_NoCrash)
{
    EXPECT_NO_THROW(mctp::testLogProberSummaries());
}

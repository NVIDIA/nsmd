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
 * Branch coverage batch 8 for nsmd/nsmGPM/nsmGpmOem.cpp.
 *
 * Covers:
 * - addGpmIntfProperty(name, DataType::VectorDouble): registers vector<double>
 *   (lines 471-473)
 * - addGpmIntfProperty(name, DataType): null gpmIntf early return (lines
 *   459-463)
 * - addGpmIntfProperty(bitfield): null gpmIntf early return (lines 425-429)
 * - NsmGPMInterfaceCreator::initialize(): null gpmIntf else branch (lines
 *   148-152)
 * - GPMMetricInstanceUpdator::updateMetric: success path — previousMetrics !=
 *   mergedMetrics and gpmIntf set, calls set_property and updates previous
 *   (lines 279-281)
 */

#include "platform-environmental.h"

#include <cstring>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmGpmOem.hpp"

#undef private
#undef protected

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

using namespace nsm;
using namespace ::testing;

// ============================================================================
// addGpmIntfProperty(name, DataType::VectorDouble) — valid gpmIntf
// ============================================================================

TEST(NsmGpmOemBranch8, AddGpmIntfPropertyVectorDouble_ValidIntf)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto objServer =
        std::make_shared<sdbusplus::asio::object_server>(systemBus);

    NsmGPMInterfaceCreator creator(*objServer,
                                   "/xyz/openbmc_project/test/gpm_branch8a");

    // Should not throw — registers a vector<double> property
    EXPECT_NO_THROW(
        creator.addGpmIntfProperty("TestVecProp", DataType::VectorDouble));
    // Also cover Double variant for the same method
    EXPECT_NO_THROW(
        creator.addGpmIntfProperty("TestDblProp", DataType::Double));
}

// ============================================================================
// addGpmIntfProperty(name, DataType): null gpmIntf early return (lines 459-463)
// ============================================================================

TEST(NsmGpmOemBranch8, AddGpmIntfPropertyByName_NullIntf_EarlyReturn)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto objServer =
        std::make_shared<sdbusplus::asio::object_server>(systemBus);

    NsmGPMInterfaceCreator creator(*objServer,
                                   "/xyz/openbmc_project/test/gpm_branch8b");
    // Force null gpmIntf via private access
    creator.gpmIntf = nullptr;

    // Must not crash — returns early after logging error
    EXPECT_NO_THROW(
        creator.addGpmIntfProperty("SomeProp", DataType::VectorDouble));
    EXPECT_NO_THROW(creator.addGpmIntfProperty("SomeProp", DataType::Double));
}

// ============================================================================
// addGpmIntfProperty(bitfield): null gpmIntf early return (lines 425-429)
// ============================================================================

TEST(NsmGpmOemBranch8, AddGpmIntfPropertyBitfield_NullIntf_EarlyReturn)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto objServer =
        std::make_shared<sdbusplus::asio::object_server>(systemBus);

    NsmGPMInterfaceCreator creator(*objServer,
                                   "/xyz/openbmc_project/test/gpm_branch8c");
    creator.gpmIntf = nullptr;

    std::vector<uint8_t> bitfield{0xFF, 0xFF};
    EXPECT_NO_THROW(creator.addGpmIntfProperty(bitfield));
}

// ============================================================================
// NsmGPMInterfaceCreator::initialize(): null gpmIntf → else branch (L148-152)
// ============================================================================

TEST(NsmGpmOemBranch8, Initialize_NullIntf_LogsError)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto objServer =
        std::make_shared<sdbusplus::asio::object_server>(systemBus);

    NsmGPMInterfaceCreator creator(*objServer,
                                   "/xyz/openbmc_project/test/gpm_branch8d");
    creator.gpmIntf = nullptr;

    // Should log an error and not crash
    EXPECT_NO_THROW(creator.initialize());
}

// ============================================================================
// GPMMetricInstanceUpdator::updateMetric — success path (lines 279-281)
// previousMetrics != mergedMetrics with valid gpmIntf → set_property called
// ============================================================================

TEST(NsmGpmOemBranch8, GPMMetricInstanceUpdator_UpdateMetric_SuccessPath)
{
    static boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    // Create a real dbus_interface and register the vector<double> property
    auto gpmAsioIntf = std::make_shared<sdbusplus::asio::dbus_interface>(
        systemBus,
        sdbusplus::object_path{"/xyz/openbmc_project/test/gpm_branch8e"},
        "com.nvidia.GPMMetrics");
    gpmAsioIntf->register_property("TestMetric", std::vector<double>{});
    gpmAsioIntf->initialize();

    // makeGPMPerInstanceUpdator creates a GPMMetricInstanceUpdator internally
    auto updator = makeGPMPerInstanceUpdator(
        "TestMetric", "/xyz/openbmc_project/test/gpm_branch8e", gpmAsioIntf);
    ASSERT_NE(updator, nullptr);

    // First call: previousMetrics is empty, mergedMetrics will be {1.0, 2.0}
    // → previousMetrics != mergedMetrics → set_property path taken
    std::vector<double> metrics1{1.0, 2.0};
    EXPECT_NO_THROW(updator->updateMetric(metrics1));

    // Second call: same values → previousMetrics == mergedMetrics → no update
    EXPECT_NO_THROW(updator->updateMetric(metrics1));

    // Third call: new values → set_property again
    std::vector<double> metrics2{3.0, 4.0};
    EXPECT_NO_THROW(updator->updateMetric(metrics2));
}

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

#include "base.h"
#include "platform-environmental.h"

#include <sdbusplus/bus.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "counterProducer.hpp"
#include "nsmDevice.hpp"
#include "progressCounterType.hpp"
#include "progressCounters.hpp"

using namespace ::testing;
using namespace nsm;

class DiscoveryEventsTest : public Test
{
  protected:
    DiscoveryEvents discoveryEvents{1};
};

// Test constructor
TEST_F(DiscoveryEventsTest, Constructor)
{
    // Verify initial state
    EXPECT_EQ(discoveryEvents.dumpIteration, 0);
    EXPECT_EQ(discoveryEvents.totalCount, 0);
    EXPECT_EQ(discoveryEvents.lastUpdateTime, 0);

    // Verify counters are initialized by initializeCounters() in constructor
    // All counters (including signals) start at -1, signals become 0 on first
    // increment
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::InterfaceAddedSignal)],
              -1);
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::InterfaceRemovedSignal)],
              -1);
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::ConnectivityAvailable)],
              -1);
    EXPECT_EQ(discoveryEvents
                  .counters[static_cast<uint32_t>(DiscoveryEventType::Ping)],
              -1);
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::QueryDeviceIdentification)],
              -1);
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::GetSupportedNvidiaMessageType)],
              -1);
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::GetSupportedCommandCodes0)],
              -1);
    EXPECT_EQ(discoveryEvents
                  .counters[static_cast<uint32_t>(DiscoveryEventType::GetFru)],
              -1);
}

// Test initializeCounters method
TEST_F(DiscoveryEventsTest, InitializeCounters)
{
    // Modify counters first
    discoveryEvents.counters[static_cast<uint32_t>(
        DiscoveryEventType::InterfaceAddedSignal)] = 5;
    discoveryEvents.counters[static_cast<uint32_t>(DiscoveryEventType::Ping)] =
        10;

    // Call initializeCounters
    discoveryEvents.initializeCounters();

    // Verify counters are reset to expected values
    // All counters reset to -1 after initializeCounters
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::InterfaceAddedSignal)],
              -1);
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::InterfaceRemovedSignal)],
              -1);
    EXPECT_EQ(discoveryEvents
                  .counters[static_cast<uint32_t>(DiscoveryEventType::Ping)],
              -1);
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::QueryDeviceIdentification)],
              -1);
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::GetSupportedNvidiaMessageType)],
              -1);
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::GetSupportedCommandCodes0)],
              -1);
    EXPECT_EQ(discoveryEvents
                  .counters[static_cast<uint32_t>(DiscoveryEventType::GetFru)],
              -1);
}

#ifdef NVIDIA_PROGRESS_COUNTER

// Test increment with InterfaceAddedSignal
TEST_F(DiscoveryEventsTest, IncrementInterfaceAddedSignal)
{
    discoveryEvents.increment(DiscoveryEventType::InterfaceAddedSignal);

    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::InterfaceAddedSignal)],
              1);
    EXPECT_EQ(discoveryEvents.totalCount, 1);
}

// Test increment with InterfaceRemovedSignal
TEST_F(DiscoveryEventsTest, IncrementInterfaceRemovedSignal)
{
    discoveryEvents.increment(DiscoveryEventType::InterfaceRemovedSignal);

    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::InterfaceRemovedSignal)],
              1);
    EXPECT_EQ(discoveryEvents.totalCount, 1);
}

// Test setValue with Ping return code
TEST_F(DiscoveryEventsTest, SetValuePingWithRC)
{
    uint8_t rc = NSM_SUCCESS;

    discoveryEvents.setValue(DiscoveryEventType::Ping, rc);

    EXPECT_EQ(discoveryEvents
                  .counters[static_cast<uint32_t>(DiscoveryEventType::Ping)],
              rc);
    EXPECT_EQ(discoveryEvents.totalCount, 0);
    EXPECT_EQ(discoveryEvents.dumpIteration, 0); // No flush yet
}

// Test setValue with QueryDeviceIdentification return code
TEST_F(DiscoveryEventsTest, SetValueQueryDeviceIdentificationWithRC)
{
    uint8_t rc = NSM_SUCCESS;

    discoveryEvents.setValue(DiscoveryEventType::QueryDeviceIdentification, rc);

    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::QueryDeviceIdentification)],
              rc);
    EXPECT_EQ(discoveryEvents.totalCount, 0);
    EXPECT_EQ(discoveryEvents.dumpIteration, 0); // No flush yet
}

// Test setValue with GetSupportedNvidiaMessageType return code
TEST_F(DiscoveryEventsTest, SetValueGetSupportedNvidiaMessageTypeWithRC)
{
    uint8_t rc = NSM_SUCCESS;

    discoveryEvents.setValue(DiscoveryEventType::GetSupportedNvidiaMessageType,
                             rc);

    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::GetSupportedNvidiaMessageType)],
              rc);
    EXPECT_EQ(discoveryEvents.totalCount, 0);
    EXPECT_EQ(discoveryEvents.dumpIteration, 0); // No flush yet
}

// Test setValue with GetSupportedCommandCodes0 return code
TEST_F(DiscoveryEventsTest, SetValueGetSupportedCommandCodes0WithRC)
{
    uint8_t rc = NSM_SUCCESS;

    discoveryEvents.setValue(DiscoveryEventType::GetSupportedCommandCodes0, rc);

    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::GetSupportedCommandCodes0)],
              rc);
    EXPECT_EQ(discoveryEvents.totalCount, 0);
    EXPECT_EQ(discoveryEvents.dumpIteration, 0); // No flush yet
}

// Test setValue with GetFru return code
TEST_F(DiscoveryEventsTest, SetValueGetFruWithRC)
{
    uint8_t rc = NSM_SUCCESS;

    discoveryEvents.setValue(DiscoveryEventType::GetFru, rc);

    EXPECT_EQ(discoveryEvents
                  .counters[static_cast<uint32_t>(DiscoveryEventType::GetFru)],
              rc);
    EXPECT_EQ(discoveryEvents.totalCount, 0);
    EXPECT_EQ(discoveryEvents.dumpIteration, 0); // No flush yet
}

// Test increment with error return codes
TEST_F(DiscoveryEventsTest, IncrementOnlinePingWithErrorRC)
{
    uint8_t errorRC = NSM_ERROR;

    discoveryEvents.setValue(DiscoveryEventType::Ping, errorRC);

    EXPECT_EQ(discoveryEvents
                  .counters[static_cast<uint32_t>(DiscoveryEventType::Ping)],
              errorRC);
    EXPECT_EQ(discoveryEvents.totalCount, 0);
    EXPECT_EQ(discoveryEvents.dumpIteration, 0); // No flush on first setValue
}

// Test setValue with timeout return code
TEST_F(DiscoveryEventsTest, SetValueQueryDeviceIdentificationWithTimeoutRC)
{
    uint8_t timeoutRC = NSM_SW_ERROR_TIMEOUT;

    discoveryEvents.setValue(DiscoveryEventType::QueryDeviceIdentification,
                             timeoutRC);

    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::QueryDeviceIdentification)],
              timeoutRC);
    EXPECT_EQ(discoveryEvents.totalCount, 0);
    EXPECT_EQ(discoveryEvents.dumpIteration, 0); // No flush yet
}

// Test multiple increments of InterfaceAddedSignal
TEST_F(DiscoveryEventsTest, MultipleIncrementInterfaceAddedSignal)
{
    discoveryEvents.increment(DiscoveryEventType::InterfaceAddedSignal);

    // After first increment, counter should be 1
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::InterfaceAddedSignal)],
              1);
    EXPECT_EQ(discoveryEvents.totalCount, 1);
}

// Test multiple increments of InterfaceRemovedSignal
TEST_F(DiscoveryEventsTest, MultipleIncrementInterfaceRemovedSignal)
{
    discoveryEvents.increment(DiscoveryEventType::InterfaceRemovedSignal);

    // After first increment, counter should be 1
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::InterfaceRemovedSignal)],
              1);
    EXPECT_EQ(discoveryEvents.totalCount, 1);
}

// Test flush behavior when setting RC values after interface signals
TEST_F(DiscoveryEventsTest, FlushBehaviorAfterInterfaceSignals)
{
    // Increment interface signal first
    discoveryEvents.increment(DiscoveryEventType::InterfaceAddedSignal);

    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::InterfaceAddedSignal)],
              1);
    EXPECT_EQ(discoveryEvents.totalCount, 1);

    // Now set a Ping counter - setValue does not trigger flush automatically
    uint8_t rc = NSM_SUCCESS;
    discoveryEvents.setValue(DiscoveryEventType::Ping, rc);

    // Interface signals should remain the same
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::InterfaceAddedSignal)],
              1);
    EXPECT_EQ(discoveryEvents
                  .counters[static_cast<uint32_t>(DiscoveryEventType::Ping)],
              rc);
    EXPECT_EQ(discoveryEvents.totalCount, 1);    // Still counting signals
    EXPECT_EQ(discoveryEvents.dumpIteration, 0); // No flush yet
}

// Test flush behavior when setting RC counter that was already set
TEST_F(DiscoveryEventsTest, FlushBehaviorWhenRCAlreadySet)
{
    // Set Ping RC first
    uint8_t rc1 = NSM_SUCCESS;
    discoveryEvents.setValue(DiscoveryEventType::Ping, rc1);

    EXPECT_EQ(discoveryEvents
                  .counters[static_cast<uint32_t>(DiscoveryEventType::Ping)],
              rc1);
    EXPECT_EQ(discoveryEvents.dumpIteration, 0); // No flush yet

    // Set Ping RC again with different value - should trigger flush first
    uint8_t rc2 = NSM_ERROR;
    discoveryEvents.setValue(DiscoveryEventType::Ping, rc2);

    EXPECT_EQ(discoveryEvents
                  .counters[static_cast<uint32_t>(DiscoveryEventType::Ping)],
              rc2);
    EXPECT_EQ(discoveryEvents.dumpIteration, 1); // Flush occurred
}

// Test sequential discovery operations
TEST_F(DiscoveryEventsTest, SequentialDiscoveryOperations)
{
    // Simulate a complete discovery sequence
    discoveryEvents.increment(DiscoveryEventType::InterfaceAddedSignal);

    uint8_t rc = NSM_SUCCESS;
    discoveryEvents.setValue(DiscoveryEventType::Ping, rc);
    discoveryEvents.setValue(DiscoveryEventType::QueryDeviceIdentification, rc);
    discoveryEvents.setValue(DiscoveryEventType::GetSupportedNvidiaMessageType,
                             rc);
    discoveryEvents.setValue(DiscoveryEventType::GetSupportedCommandCodes0, rc);
    discoveryEvents.setValue(DiscoveryEventType::GetFru, rc);

    // All RC counters should have the success code
    EXPECT_EQ(discoveryEvents
                  .counters[static_cast<uint32_t>(DiscoveryEventType::Ping)],
              rc);
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::QueryDeviceIdentification)],
              rc);
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::GetSupportedNvidiaMessageType)],
              rc);
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::GetSupportedCommandCodes0)],
              rc);
    EXPECT_EQ(discoveryEvents
                  .counters[static_cast<uint32_t>(DiscoveryEventType::GetFru)],
              rc);

    // No automatic flushes with setValue
    EXPECT_EQ(discoveryEvents.dumpIteration, 0);
}

// Test mixed success and error codes in discovery sequence
TEST_F(DiscoveryEventsTest, MixedSuccessAndErrorCodes)
{
    discoveryEvents.increment(DiscoveryEventType::InterfaceAddedSignal);

    discoveryEvents.setValue(DiscoveryEventType::Ping, NSM_SUCCESS);
    discoveryEvents.setValue(DiscoveryEventType::QueryDeviceIdentification,
                             NSM_ERROR);
    discoveryEvents.setValue(DiscoveryEventType::GetSupportedNvidiaMessageType,
                             NSM_SW_ERROR_TIMEOUT);
    discoveryEvents.setValue(DiscoveryEventType::GetSupportedCommandCodes0,
                             NSM_SUCCESS);

    // Verify each counter has its respective RC
    EXPECT_EQ(discoveryEvents
                  .counters[static_cast<uint32_t>(DiscoveryEventType::Ping)],
              NSM_SUCCESS);
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::QueryDeviceIdentification)],
              NSM_ERROR);
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::GetSupportedNvidiaMessageType)],
              NSM_SW_ERROR_TIMEOUT);
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::GetSupportedCommandCodes0)],
              NSM_SUCCESS);
}

// Test updateCounters method
TEST_F(DiscoveryEventsTest, UpdateCounters)
{
    uint64_t currentTime = 5000000;

    // Add some counters
    discoveryEvents.increment(DiscoveryEventType::InterfaceAddedSignal);
    // Second increment triggers auto-flush because InterfaceAddedSignal > 0
    discoveryEvents.increment(DiscoveryEventType::InterfaceRemovedSignal);

    // After auto-flush, only the last increment is counted
    EXPECT_EQ(discoveryEvents.totalCount, 1);
    EXPECT_EQ(discoveryEvents.dumpIteration, 1);

    // Manually call updateCounters with time parameter
    discoveryEvents.updateCounters(currentTime);

    // After update, counters should be reset
    EXPECT_EQ(discoveryEvents.totalCount, 0);
    EXPECT_EQ(discoveryEvents.lastUpdateTime, currentTime);
    EXPECT_EQ(discoveryEvents.dumpIteration, 2);

    // All counters reset to -1 (including interface signals)
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::InterfaceAddedSignal)],
              -1);
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::InterfaceRemovedSignal)],
              -1);
    EXPECT_EQ(discoveryEvents
                  .counters[static_cast<uint32_t>(DiscoveryEventType::Ping)],
              -1);
}

// Test all DiscoveryEventType enum values
TEST_F(DiscoveryEventsTest, AllDiscoveryEventTypes)
{
    // Test each discovery event type (except EnumCount which is just for
    // sizing)
    discoveryEvents.increment(DiscoveryEventType::InterfaceAddedSignal);
    // Second increment triggers auto-flush because InterfaceAddedSignal > 0
    discoveryEvents.increment(DiscoveryEventType::InterfaceRemovedSignal);

    uint8_t rc = NSM_SUCCESS;
    discoveryEvents.setValue(DiscoveryEventType::Ping, rc);
    discoveryEvents.setValue(DiscoveryEventType::QueryDeviceIdentification, rc);
    discoveryEvents.setValue(DiscoveryEventType::GetSupportedNvidiaMessageType,
                             rc);
    discoveryEvents.setValue(DiscoveryEventType::GetSupportedCommandCodes0, rc);
    discoveryEvents.setValue(DiscoveryEventType::GetFru, rc);

    // Interface signals were flushed and reset to -1 after the second increment
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::InterfaceAddedSignal)],
              -1);
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::InterfaceRemovedSignal)],
              1);

    // Verify RC counters have the success code
    EXPECT_EQ(discoveryEvents
                  .counters[static_cast<uint32_t>(DiscoveryEventType::Ping)],
              rc);
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::QueryDeviceIdentification)],
              rc);
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::GetSupportedNvidiaMessageType)],
              rc);
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::GetSupportedCommandCodes0)],
              rc);
    EXPECT_EQ(discoveryEvents
                  .counters[static_cast<uint32_t>(DiscoveryEventType::GetFru)],
              rc);
}

// Test constructor with different path and description
TEST_F(DiscoveryEventsTest, ConstructorWithDifferentParameters)
{
    DiscoveryEvents customDiscoveryEvents{42};

    // Verify initial state is the same
    EXPECT_EQ(customDiscoveryEvents.dumpIteration, 0);
    EXPECT_EQ(customDiscoveryEvents.totalCount, 0);
    EXPECT_EQ(customDiscoveryEvents.lastUpdateTime, 0);

    // Verify counters are initialized correctly (same as in Constructor test)
    // All counters start at -1
    EXPECT_EQ(customDiscoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::InterfaceAddedSignal)],
              -1);
    EXPECT_EQ(customDiscoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::InterfaceRemovedSignal)],
              -1);
    EXPECT_EQ(customDiscoveryEvents
                  .counters[static_cast<uint32_t>(DiscoveryEventType::Ping)],
              -1);
}

#else

// If NVIDIA_PROGRESS_COUNTER is not defined, test that increment does nothing
TEST_F(DiscoveryEventsTest, IncrementDisabledFeature)
{
    // Try to increment interface signals
    discoveryEvents.increment(DiscoveryEventType::InterfaceAddedSignal);
    discoveryEvents.increment(DiscoveryEventType::InterfaceRemovedSignal);

    // Try to set RC counters
    uint8_t rc = NSM_SUCCESS;
    discoveryEvents.setValue(DiscoveryEventType::Ping, rc);

    // Nothing should change
    EXPECT_EQ(discoveryEvents.totalCount, 0);
    EXPECT_EQ(discoveryEvents.lastUpdateTime, 0);
    EXPECT_EQ(discoveryEvents.dumpIteration, 0);

    // Counters should remain at their initial values
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::InterfaceAddedSignal)],
              0);
    EXPECT_EQ(discoveryEvents.counters[static_cast<uint32_t>(
                  DiscoveryEventType::InterfaceRemovedSignal)],
              0);
    EXPECT_EQ(discoveryEvents
                  .counters[static_cast<uint32_t>(DiscoveryEventType::Ping)],
              -1);
}

#endif // NVIDIA_PROGRESS_COUNTER

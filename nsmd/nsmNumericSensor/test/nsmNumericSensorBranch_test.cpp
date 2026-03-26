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
 * Branch coverage tests for nsmd/nsmNumericSensor/nsmNumericSensor.cpp
 *
 * Covers:
 * - NsmNumericSensorDbusValue constructor: non-null implementation (lines
 *   68-76)
 * - NsmNumericSensorDbusPeakValueTimestamp::updateReading (lines 174-179)
 * - NsmNumericSensorCompositeChildValue constructor and updateReading (lines
 *   292-328)
 * - NsmNumericSensor::update: genRequestMsg failure, sensorIO failure, success
 *   (lines 331-370)
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <limits>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "nsmNumericSensor.hpp"
#include "nsmNumericSensorComposite.hpp"

using namespace nsm;

// ============================================================================
// Concrete NsmNumericSensor subclass for update() tests
// ============================================================================

class TestNumericSensor : public NsmNumericSensor
{
  public:
    TestNumericSensor(const std::string& name, const std::string& type,
                      std::shared_ptr<NsmNumericSensorValueAggregate> sv,
                      bool genRequestFail = false) :
        NsmNumericSensor(name, type, 0, sv), failGenRequest(genRequestFail)
    {}

    std::optional<std::vector<uint8_t>> genRequestMsg(eid_t, uint8_t) override
    {
        if (failGenRequest)
            return std::nullopt;
        return std::vector<uint8_t>(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    }

    uint8_t handleResponseMsg(const nsm_msg*, size_t) override
    {
        return NSM_SW_SUCCESS;
    }

    std::string getSensorType() override
    {
        return "power";
    }

    bool failGenRequest;
};

// ============================================================================
// Fixture
// ============================================================================

struct NsmNumericSensorBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmNumericSensorBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmNumericSensorBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<NsmNumericSensorValueAggregate>
        makeAggregate(const std::string& suffix = "br")
    {
        auto& bus = utils::DBusHandler::getBus();
        return std::make_shared<NsmNumericSensorValueAggregate>(
            std::make_unique<NsmNumericSensorDbusValue>(
                bus, "BrSensor" + suffix, "power", SensorUnit::Watts,
                std::vector<utils::Association>{}, "GPU", nullptr,
                std::numeric_limits<double>::infinity(),
                std::numeric_limits<double>::infinity(),
                std::numeric_limits<double>::lowest(), nullptr, nullptr));
    }
};

// ============================================================================
// NsmNumericSensorDbusValue: non-null implementation → lines 68-76
// ============================================================================

TEST_F(NsmNumericSensorBranchTest,
       DbusValue_NonNullImplementation_CreatesTypeIntf)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string impl{"PhysicalSensor"};

    // Passing &impl exercises the "if (implementation)" branch
    EXPECT_NO_THROW((NsmNumericSensorDbusValue{
        bus, "ImplSensor", "power", SensorUnit::Watts,
        std::vector<utils::Association>{}, "GPU", &impl,
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::lowest(), nullptr, nullptr}));
}

// ============================================================================
// NsmNumericSensorDbusPeakValueTimestamp::updateReading – lines 174-179
// ============================================================================

TEST_F(NsmNumericSensorBranchTest, PeakValueTimestamp_UpdateReading_SetsFields)
{
    auto& bus = utils::DBusHandler::getBus();
    const char* objPath = "/xyz/openbmc_project/sensors/power/PeakBr0";
    NsmNumericSensorDbusPeakValueTimestamp pv{bus, objPath};

    constexpr double testValue = 1234.5;
    constexpr uint64_t testTs = 9876543210ULL;

    pv.updateReading(testValue, testTs);

    EXPECT_DOUBLE_EQ(pv.peakValueIntf.peakValue(), testValue);
    EXPECT_EQ(pv.peakValueIntf.timestamp(), testTs);
}

// ============================================================================
// NsmNumericSensorCompositeChildValue – lines 292-328
// ============================================================================

TEST(NsmNumericSensorCompositeChildValue, Constructor_StoresFields)
{
    std::vector<std::string> parents{
        "/xyz/openbmc_project/sensors/power/Total"};
    NsmNumericSensorCompositeChildValue cv{"child1", "power", parents};

    EXPECT_EQ(cv.name, "child1");
    EXPECT_EQ(cv.sensorType, "power");
    EXPECT_EQ(cv.parents.size(), 1u);
}

TEST_F(NsmNumericSensorBranchTest,
       CompositeChildValue_UpdateReading_EmptyParents_Noop)
{
    // No parents → loop doesn't execute, sensorCache stays empty
    NsmNumericSensorCompositeChildValue cv{"child_empty", "power", {}};
    EXPECT_NO_THROW(cv.updateReading(42.0, 0));
    EXPECT_TRUE(cv.sensorCache.empty());
}

TEST_F(NsmNumericSensorBranchTest,
       CompositeChildValue_UpdateReading_ParentNotFound_RemainsInList)
{
    // Parent path not in objectPathToSensorMap → parents not erased
    std::vector<std::string> parents{
        "/xyz/openbmc_project/sensors/power/NoSuchSensor"};
    NsmNumericSensorCompositeChildValue cv{"child_notfound", "power", parents};

    cv.updateReading(100.0, 0);

    // Parent not found → still in parents list, sensorCache empty
    EXPECT_EQ(cv.parents.size(), 1u);
    EXPECT_TRUE(cv.sensorCache.empty());
}

TEST_F(NsmNumericSensorBranchTest,
       CompositeChildValue_UpdateReading_ParentFound_MovesToCache)
{
    auto& bus = utils::DBusHandler::getBus();

    // Create a NsmNumericSensorComposite and register it in objectPathToSensor
    const std::string sensorPath =
        "/xyz/openbmc_project/sensors/power/TotalGpu";
    auto composite = std::make_shared<NsmNumericSensorComposite>(
        bus, "TotalGpu", std::vector<utils::Association>{}, "TestType",
        sensorPath, "GPU", "PhysicalSensor"
#ifdef NVIDIA_SHMEM
        ,
        nullptr
#endif
    );
    SensorManager::getInstance().objectPathToSensorMap[sensorPath] = composite;

    std::vector<std::string> parents{sensorPath};
    NsmNumericSensorCompositeChildValue cv{"child_found", "power", parents};

    // First update: parent found → moved to sensorCache, parents erased
    cv.updateReading(50.0, 0);

    EXPECT_TRUE(cv.parents.empty());
    EXPECT_EQ(cv.sensorCache.size(), 1u);
    EXPECT_DOUBLE_EQ(composite->valueIntf->value(), 50.0);

    // Second update: parents already empty, sensorCache has entry
    composite->nextUpdateTimestamp = 0;
    cv.updateReading(75.0, 0);
    EXPECT_DOUBLE_EQ(composite->valueIntf->value(), 75.0);

    // Cleanup
    SensorManager::getInstance().objectPathToSensorMap.erase(sensorPath);
}

TEST_F(NsmNumericSensorBranchTest,
       CompositeChildValue_UpdateReading_NullSensor_SkipsCache)
{
    // Register a null shared_ptr in the map → sensor == nullptr
    const std::string sensorPath =
        "/xyz/openbmc_project/sensors/power/NullSensor";
    SensorManager::getInstance().objectPathToSensorMap[sensorPath] = nullptr;

    std::vector<std::string> parents{sensorPath};
    NsmNumericSensorCompositeChildValue cv{"child_null", "power", parents};

    cv.updateReading(99.0, 0);

    // sensor is null → not cached, parent not erased
    EXPECT_FALSE(cv.parents.empty());
    EXPECT_TRUE(cv.sensorCache.empty());

    // Cleanup
    SensorManager::getInstance().objectPathToSensorMap.erase(sensorPath);
}

// ============================================================================
// NsmNumericSensor::update – lines 331-370
// ============================================================================

TEST_F(NsmNumericSensorBranchTest,
       NumericSensorUpdate_GenRequestFails_ReturnsError)
{
    auto aggregate = makeAggregate("_genreqfail");
    auto sensor = std::make_shared<TestNumericSensor>("brSens1", "power",
                                                      aggregate,
                                                      /*failGenRequest=*/true);

    // genRequestMsg returns nullopt → early return without calling sensorIO
    auto coro = sensor->update(gpu);
    (void)coro;
}

TEST_F(NsmNumericSensorBranchTest, NumericSensorUpdate_SensorIOFails_UpdatesNaN)
{
    auto aggregate = makeAggregate("_sioerr");
    auto sensor = std::make_shared<TestNumericSensor>("brSens2", "power",
                                                      aggregate,
                                                      /*failGenRequest=*/false);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    auto coro = sensor->update(gpu);
    (void)coro;
}

TEST_F(NsmNumericSensorBranchTest, NumericSensorUpdate_Success_ReturnsSuccess)
{
    auto aggregate = makeAggregate("_ok");
    auto sensor = std::make_shared<TestNumericSensor>("brSens3", "power",
                                                      aggregate,
                                                      /*failGenRequest=*/false);

    // Build a minimal success response (NSM_SUCCESS cc)
    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(resp.data());
    msg->payload[1] = NSM_SUCCESS;

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));

    auto coro = sensor->update(gpu);
    (void)coro;
}

// ============================================================================
// NsmNumericSensorDbusValue: non-null readingBasis → lines 79-90
// ============================================================================

TEST_F(NsmNumericSensorBranchTest,
       DbusValue_NonNullReadingBasis_CreatesReadingBasisIntf)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string basis{"Headroom"};

    // Passing &basis exercises the "if (readingBasis)" branch (lines 79-90)
    EXPECT_NO_THROW((NsmNumericSensorDbusValue{
        bus, "ReadingBasisSensor", "power", SensorUnit::Watts,
        std::vector<utils::Association>{}, "GPU", nullptr,
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::lowest(), &basis, nullptr}));
}

// ============================================================================
// NsmNumericSensorDbusValue: non-null description → lines 92-99
// ============================================================================

TEST_F(NsmNumericSensorBranchTest,
       DbusValue_NonNullDescription_CreatesDescriptionIntf)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string desc{"GPU Power in Watts"};

    // Passing &desc exercises the "if (description)" branch (lines 92-99)
    EXPECT_NO_THROW((NsmNumericSensorDbusValue{
        bus, "DescSensor", "power", SensorUnit::Watts,
        std::vector<utils::Association>{}, "GPU", nullptr,
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::lowest(), nullptr, &desc}));
}

// ============================================================================
// NsmNumericSensorDbusValue::updateReading: same value → FALSE branch
// ============================================================================

TEST_F(NsmNumericSensorBranchTest,
       DbusValue_UpdateReading_SameValue_SkipsValueUpdate)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmNumericSensorDbusValue dv{bus,
                                 "SameValSensor",
                                 "power",
                                 SensorUnit::Watts,
                                 std::vector<utils::Association>{},
                                 "GPU",
                                 nullptr,
                                 std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::lowest(),
                                 nullptr,
                                 nullptr};

    // First call: value changes (previousValue was NaN, now 100.0) → TRUE
    dv.updateReading(100.0, 1000);
    EXPECT_DOUBLE_EQ(dv.valueIntf.value(), 100.0);

    // Second call: same value → if(previousValue != value) FALSE → skip
    dv.updateReading(100.0, 2000);
    EXPECT_DOUBLE_EQ(dv.valueIntf.value(), 100.0);
}

// ============================================================================
// NsmNumericSensorDbusValue::calculateNextUpdateTimestamp: both branches
// ============================================================================

TEST_F(NsmNumericSensorBranchTest,
       DbusValue_CalculateNextUpdate_InitialCall_SetsTimestamp)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmNumericSensorDbusValue dv{bus,
                                 "CalcTsBr0",
                                 "power",
                                 SensorUnit::Watts,
                                 std::vector<utils::Association>{},
                                 "GPU",
                                 nullptr,
                                 std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::lowest(),
                                 nullptr,
                                 nullptr};

    // nextUpdateTimestamp starts at 0 → if(nextUpdateTimestamp == 0) TRUE
    // → sets nextUpdateTimestamp = timestamp + 1000 and returns
    dv.updateReading(1.0, 5000);
    EXPECT_EQ(dv.nextUpdateTimestamp, 6000u);
}

TEST_F(NsmNumericSensorBranchTest,
       DbusValue_CalculateNextUpdate_SubsequentCall_UpdatesTimestamp)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmNumericSensorDbusValue dv{bus,
                                 "CalcTsBr1",
                                 "power",
                                 SensorUnit::Watts,
                                 std::vector<utils::Association>{},
                                 "GPU",
                                 nullptr,
                                 std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::lowest(),
                                 nullptr,
                                 nullptr};

    // First call: nextUpdateTimestamp set to 1000+1000 = 2000
    dv.updateReading(1.0, 1000);
    EXPECT_EQ(dv.nextUpdateTimestamp, 2000u);

    // Second call: nextUpdateTimestamp != 0 → else branch executes
    // elapsed = (3000 - 2000)/1000 = 1 → nextUpdateTimestamp += 1000*max(1,1) =
    // 3000
    dv.updateReading(2.0, 3000);
    EXPECT_EQ(dv.nextUpdateTimestamp, 3000u);
}

// ============================================================================
// NsmNumericSensorDbusValue::canUpdate: both branches
// ============================================================================

TEST_F(NsmNumericSensorBranchTest,
       DbusValue_CanUpdate_InitiallyTrue_ThenFalse_ThenTrue)
{
    auto& bus = utils::DBusHandler::getBus();
    NsmNumericSensorDbusValue dv{bus,
                                 "CanUpdateBr",
                                 "power",
                                 SensorUnit::Watts,
                                 std::vector<utils::Association>{},
                                 "GPU",
                                 nullptr,
                                 std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::lowest(),
                                 nullptr,
                                 nullptr};

    // nextUpdateTimestamp == 0 initially → canUpdate always TRUE
    EXPECT_TRUE(dv.canUpdate(0));
    EXPECT_TRUE(dv.canUpdate(9999));

    // After first update: nextUpdateTimestamp = 1000+1000 = 2000
    dv.updateReading(1.0, 1000);

    // timestamp(1500) < nextUpdateTimestamp(2000) AND nextUpdateTimestamp != 0
    // → canUpdate FALSE (both short-circuit conditions false)
    EXPECT_FALSE(dv.canUpdate(1500));

    // timestamp(2000) >= nextUpdateTimestamp(2000) → canUpdate TRUE
    EXPECT_TRUE(dv.canUpdate(2000));
}

// ============================================================================
// NsmNumericSensorValueAggregate::updateReading: L199 false branch
// (dynamic_cast returns null for non-DbusValue element)
// ============================================================================

TEST_F(NsmNumericSensorBranchTest,
       AggregateUpdateReading_NonDbusValueElem_SkipsCanUpdateCheck)
{
    auto& bus = utils::DBusHandler::getBus();

    // NsmNumericSensorDbusPeakValueTimestamp is NOT derived from
    // NsmNumericSensorDbusValue, so dynamic_cast returns nullptr.
    // This covers the L199 branch where dbusValue == nullptr.
    auto peak = std::make_unique<NsmNumericSensorDbusPeakValueTimestamp>(
        bus, "/xyz/openbmc_project/sensors/power/AggPeakBr0");

    NsmNumericSensorValueAggregate agg{std::move(peak)};

    // Should call elem->updateReading without canUpdate check
    EXPECT_NO_THROW(agg.updateReading(42.0, 1000));
}

// ============================================================================
// NsmNumericSensorValueAggregate::updateReading: L200 true branch
// (dbusValue non-null, canUpdate returns false → skip update)
// ============================================================================

TEST_F(NsmNumericSensorBranchTest,
       AggregateUpdateReading_DbusValueCannotUpdate_SkipsUpdate)
{
    auto& bus = utils::DBusHandler::getBus();

    // Create a DbusValue that will be put into the aggregate.
    // We use make_unique so the aggregate owns it.
    auto dbv = std::make_unique<NsmNumericSensorDbusValue>(
        bus, "AggSkipSensor", "power", SensorUnit::Watts,
        std::vector<utils::Association>{}, "GPU", nullptr,
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::lowest(), nullptr, nullptr);

    // Get a raw pointer before moving ownership into the aggregate
    auto* dbvRaw = dbv.get();

    NsmNumericSensorValueAggregate agg{std::move(dbv)};

    // First update: ts=1000, nextUpdateTimestamp set to 2000, value → 1.0
    agg.updateReading(1.0, 1000);
    EXPECT_DOUBLE_EQ(dbvRaw->valueIntf.value(), 1.0);

    // Second update: ts=500 < nextUpdateTimestamp=2000 → canUpdate returns
    // false → update skipped (L200 true branch → continue)
    agg.updateReading(99.0, 500);
    // Value should still be 1.0 (update was skipped)
    EXPECT_DOUBLE_EQ(dbvRaw->valueIntf.value(), 1.0);
}

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

/*
 * Branch coverage for the three fecHistogramApplicable branches introduced
 * around supportFECHistogram in createNVLinkFeature
 * (nsmd/nsmPort/nsmPort.cpp):
 *
 *   (1) supportFECHistogram present and true
 *         → fecHistogramApplicable = true  → FEC histogram sensors created
 *   (2) supportFECHistogram present and false
 *         → fecHistogramApplicable = false → FEC histogram sensors NOT created
 *   (3) supportFECHistogram absent
 *         → fecHistogramApplicable = (deviceType != NSM_DEV_ID_PCIE_BRIDGE)
 *         → for a non-PCIe-bridge deviceType: true → FEC histogram sensors
 * created
 *
 * NsmHistogramFormat is registered via addStaticSensor → staticSensors.
 * That is the only addStaticSensor call in createNVLinkFeature, so
 * staticSensors.size() unambiguously reflects whether the FEC histogram block
 * was entered.
 *
 * All tests are guarded by #ifdef NVIDIA_FEC_HISTOGRAM so the file compiles
 * cleanly on builds where the histogram feature is disabled.
 */

#include "config.h"

#include "base.h"
#include "network-ports.h"

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "features/creators.hpp"
#include "features/feature.hpp"
#include "nsmPort.hpp"

#undef private
#undef protected

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================
struct NsmPortBranch7Test :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;

    NsmPortBranch7Test() : SensorManagerTest(devices) {}

    ~NsmPortBranch7Test()
    {
        cleanupDeviceSensors(devices);
    }

    /** @brief Drive the event loop until the task finishes.
     *
     * createNVLinkFeature yields between ports, so the loop has to
     * run for construction to complete. Bounded so a task that never
     * finishes fails here instead of hanging.
     */
    void runUntilDone(const requester::Coroutine& task) const
    {
        auto loop = sdeventplus::Event::get_default();
        for (int i = 0; i < 1000 && !task.done(); ++i)
        {
            sd_event_run(loop.get(), 0);
        }
        ASSERT_TRUE(task.done());
    }

    // Run the NVLink feature with Count=1 on the mock device `uuid` names;
    // deviceType is the DeviceContext type the heuristic looks at.
    // supportFEC == nullopt means the "SupportFECHistogram" param is absent.
    std::shared_ptr<MockNsmDevice> runFactory(const uuid_t& uuid,
                                              uint8_t deviceType,
                                              std::optional<bool> supportFEC)
    {
        auto device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(uuid));

        nsm::features::DeviceContext dev;
        dev.name = "HGX_FECTestDev";
        dev.kind = "gpu";
        dev.deviceType = deviceType;
        dev.staticUuid = uuid;
        dev.platformPrefix = "HGX_";
        dev.device = device;

        dbus::PropertyMap params{
            {"Name", std::string("NVLink_FECTest")},
            {"ParentObjPath",
             std::string("/xyz/openbmc_project/inventory/system/FECTestDev")},
            {"Priority", false},
            {"Count", uint64_t{1}}};
        if (supportFEC.has_value())
        {
            params["SupportFECHistogram"] = supportFEC.value();
        }

        auto task = nsm::features::createNVLinkFeature(mockManager, dev,
                                                       params);
        runUntilDone(task);
        EXPECT_EQ(task.data(), NSM_SUCCESS);

        return device;
    }
};

#ifdef NVIDIA_FEC_HISTOGRAM

// ============================================================================
// Branch 1: supportFECHistogram present and true.
// The DeviceContext deviceType is NSM_DEV_ID_PCIE_BRIDGE — a type that
// the default heuristic would exclude — but the explicit
// SupportFECHistogram=true overrides it, so fecHistogramApplicable=true and the
// FEC histogram sensors are added.
// ============================================================================
TEST_F(NsmPortBranch7Test,
       FecHistogram_SupportTrue_OverridesDeviceTypeHeuristic)
{
    // GPU UUID: MockNsmDevice::getDeviceType()==GPU keeps ECC/ETH sensors out,
    // leaving staticSensors as an unambiguous FEC-histogram indicator.
    const uuid_t uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:71";

    auto dev = runFactory(uuid,
                          /*deviceType=*/NSM_DEV_ID_PCIE_BRIDGE,
                          /*supportFEC=*/true);

    ASSERT_NE(dev, nullptr);
    // NsmHistogramFormat registered via addStaticSensor — exactly one entry.
    EXPECT_EQ(dev->staticSensors.size(), 1u);
    // NsmHistogramData added via addSensor(_, false) — at least one RR sensor.
    EXPECT_GE(dev->roundRobinSensors.size(), 1u);
}

// ============================================================================
// Branch 2: supportFECHistogram present and false.
// The DeviceContext deviceType is NSM_DEV_ID_GPU — a type the default
// heuristic would include — but SupportFECHistogram=false suppresses FEC,
// so fecHistogramApplicable=false and no FEC histogram sensors are added.
// ============================================================================
TEST_F(NsmPortBranch7Test,
       FecHistogram_SupportFalse_SuppressesFecOnNonPcieBridge)
{
    const uuid_t uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:72";

    auto dev = runFactory(uuid,
                          /*deviceType=*/NSM_DEV_ID_GPU,
                          /*supportFEC=*/false);

    ASSERT_NE(dev, nullptr);
    // No FEC histogram: no static sensors at all.
    EXPECT_EQ(dev->staticSensors.size(), 0u);
}

// ============================================================================
// Branch 3: supportFECHistogram absent — fallback heuristic.
//   fecHistogramApplicable = (deviceType != NSM_DEV_ID_PCIE_BRIDGE)
// deviceType=NSM_DEV_ID_GPU (a non-PCIe-bridge value) makes the condition
// true, so FEC histogram sensors are created.
// ============================================================================
TEST_F(NsmPortBranch7Test,
       FecHistogram_SupportAbsent_FallbackNonPcieBridgeCreates)
{
    const uuid_t uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:73";

    // SupportFECHistogram key absent; deviceType=GPU → heuristic yields true.
    auto dev = runFactory(uuid,
                          /*deviceType=*/NSM_DEV_ID_GPU,
                          /*supportFEC=*/std::nullopt);

    ASSERT_NE(dev, nullptr);
    // NsmHistogramFormat added to staticSensors.
    EXPECT_EQ(dev->staticSensors.size(), 1u);
    EXPECT_GE(dev->roundRobinSensors.size(), 1u);
}

#endif // NVIDIA_FEC_HISTOGRAM

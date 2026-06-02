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
 * around supportFECHistogram in createNsmPortSensorGeneric
 * (nsmd/nsmPort/nsmPort.cpp, lines ~1960-1968):
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
 * That is the only addStaticSensor call in createNsmPortSensorGeneric, so
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

#include "nsmPort.hpp"

#undef private
#undef protected

using namespace nsm;

namespace nsm
{
requester::Coroutine createNsmPortSensorGeneric(SensorManager& manager,
                                                const std::string& interface,
                                                const std::string& objPath);
} // namespace nsm

// ============================================================================
// Fixture
// ============================================================================
struct NsmPortBranch7Test :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string portInterface =
        "xyz.openbmc_project.Configuration.NSM_NVLink";

    NsmDeviceTable devices;

    NsmPortBranch7Test() : SensorManagerTest(devices) {}

    ~NsmPortBranch7Test()
    {
        cleanupDeviceSensors(devices);
    }

    // Run the factory with Count=1 and the given configuration.
    // supportFEC == nullopt means the "SupportFECHistogram" key is absent.
    std::shared_ptr<MockNsmDevice> runFactory(const std::string& objPath,
                                              const uuid_t& uuid,
                                              uint64_t emDeviceType,
                                              std::optional<bool> supportFEC)
    {
        auto& propMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          portInterface);
        propMap["Name"] = std::string("NVLink_FECTest");
        propMap["UUID"] = uuid;
        propMap["ParentObjPath"] =
            std::string("/xyz/openbmc_project/inventory/system/FECTestDev");
        propMap["Priority"] = false;
        propMap["Count"] = uint64_t{1};
        propMap["DeviceType"] = emDeviceType;
        if (supportFEC.has_value())
        {
            propMap["SupportFECHistogram"] = supportFEC.value();
        }

        createNsmPortSensorGeneric(mockManager, portInterface, objPath);

        return std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(uuid));
    }
};

#ifdef NVIDIA_FEC_HISTOGRAM

// ============================================================================
// Branch 1: supportFECHistogram present and true.
// The EM config is set to DeviceType=NSM_DEV_ID_PCIE_BRIDGE — a type that
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
    const std::string objPath = "/xyz/openbmc_project/config/port_fec_b7_1";

    auto dev = runFactory(objPath, uuid,
                          /*emDeviceType=*/uint64_t{NSM_DEV_ID_PCIE_BRIDGE},
                          /*supportFEC=*/true);

    ASSERT_NE(dev, nullptr);
    // NsmHistogramFormat registered via addStaticSensor — exactly one entry.
    EXPECT_EQ(dev->staticSensors.size(), 1u);
    // NsmHistogramData added via addSensor(_, false) — at least one RR sensor.
    EXPECT_GE(dev->roundRobinSensors.size(), 1u);
}

// ============================================================================
// Branch 2: supportFECHistogram present and false.
// The EM config is set to DeviceType=NSM_DEV_ID_GPU — a type the default
// heuristic would include — but SupportFECHistogram=false suppresses FEC,
// so fecHistogramApplicable=false and no FEC histogram sensors are added.
// ============================================================================
TEST_F(NsmPortBranch7Test,
       FecHistogram_SupportFalse_SuppressesFecOnNonPcieBridge)
{
    const uuid_t uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:72";
    const std::string objPath = "/xyz/openbmc_project/config/port_fec_b7_2";

    auto dev = runFactory(objPath, uuid,
                          /*emDeviceType=*/uint64_t{NSM_DEV_ID_GPU},
                          /*supportFEC=*/false);

    ASSERT_NE(dev, nullptr);
    // No FEC histogram: no static sensors at all.
    EXPECT_EQ(dev->staticSensors.size(), 0u);
}

// ============================================================================
// Branch 3: supportFECHistogram absent — fallback heuristic.
//   fecHistogramApplicable = (deviceType != NSM_DEV_ID_PCIE_BRIDGE)
// DeviceType=NSM_DEV_ID_GPU (a non-PCIe-bridge value) makes the condition
// true, so FEC histogram sensors are created.
// ============================================================================
TEST_F(NsmPortBranch7Test,
       FecHistogram_SupportAbsent_FallbackNonPcieBridgeCreates)
{
    const uuid_t uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:73";
    const std::string objPath = "/xyz/openbmc_project/config/port_fec_b7_3";

    // SupportFECHistogram key absent; emDeviceType=GPU → heuristic yields true.
    auto dev = runFactory(objPath, uuid,
                          /*emDeviceType=*/uint64_t{NSM_DEV_ID_GPU},
                          /*supportFEC=*/std::nullopt);

    ASSERT_NE(dev, nullptr);
    // NsmHistogramFormat added to staticSensors.
    EXPECT_EQ(dev->staticSensors.size(), 1u);
    EXPECT_GE(dev->roundRobinSensors.size(), 1u);
}

#endif // NVIDIA_FEC_HISTOGRAM

/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION &
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
 * Unit tests for the GPU PCIe_0 and PCIe_0 PortInfo feature creators of
 * nsmGpuPciePort.cpp and the PortInfo group 1 response handling.
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "libnsm/pci-links.h"

#include "features/creators.hpp"
#include "features/feature.hpp"
#include "nsmCommon/nsmPcieGroup.hpp"
#include "nsmGpuPciePort.hpp"

#undef private
#undef protected

using namespace nsm;

namespace
{

constexpr auto healthOk =
    "xyz.openbmc_project.State.Decorator.Health.HealthType.OK";
constexpr auto powerOn = "xyz.openbmc_project.State.Chassis.PowerState.On";
constexpr auto upstreamPort =
    "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort";
constexpr auto pcieProtocol =
    "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe";

} // namespace

struct GpuPciePortFeatureTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    GpuPciePortFeatureTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~GpuPciePortFeatureTest() override
    {
        cleanupDeviceSensors(devices);
    }

    features::DeviceContext makeDev(bool withDevice = true) const
    {
        features::DeviceContext dev;
        dev.name = "HGX_GPU_SXM_1";
        dev.kind = "gpu";
        dev.platformPrefix = "HGX_";
        dev.staticUuid = gpuUuid;
        if (withDevice)
        {
            dev.device = gpu;
        }
        return dev;
    }

    static dbus::PropertyMap pcieParams()
    {
        return {{"Name", std::string("GPU_1 PCIe_0")},
                {"Health", std::string(healthOk)},
                {"ChasisPowerState", std::string(powerOn)},
                {"DeviceIndex", uint64_t{0}},
                {"ClearableScalarGroup", std::vector<uint64_t>{2, 3, 4}},
                {"AllStatesAssociationsEnabled", true}};
    }

    static dbus::PropertyMap portInfoParams()
    {
        return {{"Name", std::string("GPU_1 PCIe_0")},
                {"PortType", std::string(upstreamPort)},
                {"PortProtocol", std::string(pcieProtocol)},
                {"DeviceIndex", uint64_t{0}},
                {"Priority", false}};
    }

    std::shared_ptr<NsmPCIePortInfoGroup1> findPortInfoGroup() const
    {
        for (const auto& sensor : gpu->roundRobinSensors)
        {
            if (auto found =
                    std::dynamic_pointer_cast<NsmPCIePortInfoGroup1>(sensor))
            {
                return found;
            }
        }
        return nullptr;
    }

    static std::vector<uint8_t> group1Response(uint8_t cc = NSM_SUCCESS)
    {
        nsm_query_scalar_group_telemetry_group_1 data = {};
        data.negotiated_link_speed = 5;
        data.max_link_speed = 6;
        data.target_link_speed = 4;
        data.negotiated_link_width = 4;
        data.max_link_width = 5;

        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
        auto* msg = reinterpret_cast<nsm_msg*>(response.data());
        EXPECT_EQ(encode_query_scalar_group_telemetry_v1_group1_resp(
                      0, cc, cc == NSM_SUCCESS ? ERR_NULL : 0x0101, &data, msg),
                  NSM_SW_SUCCESS);
        return response;
    }
};

// ---------------------------------------------------------------------------
// PCIe_0 feature
// ---------------------------------------------------------------------------

TEST_F(GpuPciePortFeatureTest, Pcie_NoNsmDevice_Fails)
{
    auto rc = features::createGpuPCIeFeature(mockManager, makeDev(false),
                                             pcieParams());

    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_FALSE(rc.exception());
}

TEST_F(GpuPciePortFeatureTest, Pcie_Success_CreatesTheSensors)
{
    const size_t deviceBefore = gpu->deviceSensors.size();
    const size_t staticBefore = gpu->staticSensors.size();
    const size_t roundRobinBefore = gpu->roundRobinSensors.size();

    auto rc = features::createGpuPCIeFeature(mockManager, makeDev(),
                                             pcieParams());

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());
    // Port and PortInfo interface sensors
    EXPECT_GE(gpu->deviceSensors.size(), deviceBefore + 2);
    // One clear-counter sensor per ClearableScalarGroup entry
    EXPECT_GE(gpu->staticSensors.size(), staticBefore + 3);
    EXPECT_GT(gpu->roundRobinSensors.size(), roundRobinBefore);
}

TEST_F(GpuPciePortFeatureTest, Pcie_WithoutAssociationsOrClearableGroups)
{
    auto params = pcieParams();
    params["AllStatesAssociationsEnabled"] = false;
    params["ClearableScalarGroup"] = std::vector<uint64_t>{};

    const size_t staticBefore = gpu->staticSensors.size();
    auto rc = features::createGpuPCIeFeature(mockManager, makeDev(), params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(gpu->staticSensors.size(), staticBefore);
}

TEST_F(GpuPciePortFeatureTest, Pcie_MinimalParams_UseDefaults)
{
    dbus::PropertyMap params{{"Name", std::string("GPU_1 PCIe_0")},
                             {"Health", std::string(healthOk)},
                             {"ChasisPowerState", std::string(powerOn)}};

    auto rc = features::createGpuPCIeFeature(mockManager, makeDev(), params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());
}

TEST_F(GpuPciePortFeatureTest, Pcie_InvalidHealthString_Throws)
{
    auto params = pcieParams();
    params["Health"] = std::string("NotAHealthType");

    // Coverage builds run coroutines inline: the exception then surfaces in
    // the call itself instead of in the coroutine's exception()
    bool failed = false;
    try
    {
        auto rc = features::createGpuPCIeFeature(mockManager, makeDev(),
                                                 params);
        failed = rc.exception() != nullptr;
    }
    catch (const std::exception&)
    {
        failed = true;
    }

    EXPECT_TRUE(failed);
}

// ---------------------------------------------------------------------------
// PCIe_0 PortInfo feature and group 1 response
// ---------------------------------------------------------------------------

TEST_F(GpuPciePortFeatureTest, PortInfo_NoNsmDevice_Fails)
{
    auto rc = features::createGpuPCIePortInfoFeature(
        mockManager, makeDev(false), portInfoParams());

    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_FALSE(rc.exception());
}

TEST_F(GpuPciePortFeatureTest, PortInfo_Success_AddsTheGroup1Sensor)
{
    ASSERT_EQ(findPortInfoGroup(), nullptr);

    auto rc = features::createGpuPCIePortInfoFeature(mockManager, makeDev(),
                                                     portInfoParams());

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());
    EXPECT_NE(findPortInfoGroup(), nullptr);
}

TEST_F(GpuPciePortFeatureTest, PortInfo_PriorityTrue_StillSucceeds)
{
    auto params = portInfoParams();
    params["Priority"] = true;

    auto rc = features::createGpuPCIePortInfoFeature(mockManager, makeDev(),
                                                     params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
}

TEST_F(GpuPciePortFeatureTest, Group1_HandleResponse_Success)
{
    ASSERT_EQ(features::createGpuPCIePortInfoFeature(mockManager, makeDev(),
                                                     portInfoParams())
                  .data(),
              NSM_SUCCESS);
    auto group = findPortInfoGroup();
    ASSERT_NE(group, nullptr);
    auto response = group1Response();

    EXPECT_EQ(
        group->handleResponseMsg(
            reinterpret_cast<const nsm_msg*>(response.data()), response.size()),
        NSM_SUCCESS);
    // negotiated speed code 5 is 32 Gbps
    EXPECT_DOUBLE_EQ(group->portInfoIntf->getCurrentSpeed(), 32.0);
}

TEST_F(GpuPciePortFeatureTest, Group1_HandleResponse_ErrorAndTruncated)
{
    ASSERT_EQ(features::createGpuPCIePortInfoFeature(mockManager, makeDev(),
                                                     portInfoParams())
                  .data(),
              NSM_SUCCESS);
    auto group = findPortInfoGroup();
    ASSERT_NE(group, nullptr);

    auto error = group1Response(NSM_ERROR);
    EXPECT_NE(group->handleResponseMsg(
                  reinterpret_cast<const nsm_msg*>(error.data()), error.size()),
              NSM_SUCCESS);

    EXPECT_NE(group->handleResponseMsg(
                  reinterpret_cast<const nsm_msg*>(error.data()), 2),
              NSM_SUCCESS);
}

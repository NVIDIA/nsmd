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

// Additional branch coverage for nsmGpuClockControl.cpp
// Targets:
//   - NsmChassisClockControl constructor: interface creation, associations
//   - NsmChassisClockControl handleResponseMsg: success and failure paths
//   - NsmChassisClockControl genRequestMsg: success path

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "platform-environmental.h"

#include "nsmGpuClockControl.hpp"
#include "test/commonMock.hpp"

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================
struct NsmGpuClockControlBranch3Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string invPath =
        "/xyz/openbmc_project/inventory/system/clockctrl_br3";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmGpuClockControlBranch3Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmGpuClockControlBranch3Test()
    {
        cleanupDeviceSensors(devices);
    }

    static std::vector<uint8_t> decodeFail()
    {
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
        return buf;
    }
};

// ===========================================================================
// NsmChassisClockControl constructor: basic creation with empty associations
// ===========================================================================
TEST_F(NsmGpuClockControlBranch3Test, Constructor_EmptyAssociations)
{
    auto& bus = utils::DBusHandler::getBus();
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, invPath.c_str());
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);
    std::vector<utils::Association> assocs;
    std::string type = "NSM_ClockControl";

    NsmChassisClockControl sensor(
        bus, "ClockCtrl_CtorEmpty", cpuConfigIntf, clearClockIntf, assocs, type,
        invPath + "/ctor_empty",
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU",
        "com.nvidia.ClockMode.Mode.MaximumPerformance");

    EXPECT_NE(sensor.decoratorAreaIntf, nullptr);
    EXPECT_NE(sensor.associationDefinitionsInft, nullptr);
    EXPECT_NE(sensor.processorModeIntf, nullptr);
    EXPECT_EQ(sensor.inventoryObjPath, invPath + "/ctor_empty");
}

// ===========================================================================
// NsmChassisClockControl constructor: creation with non-empty associations
// ===========================================================================
TEST_F(NsmGpuClockControlBranch3Test, Constructor_WithAssociations)
{
    auto& bus = utils::DBusHandler::getBus();
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, invPath.c_str());
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);
    std::vector<utils::Association> assocs = {
        {"chassis", "all_controls",
         "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_0"},
        {"parent", "child",
         "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_1"}};
    std::string type = "NSM_ClockControl";

    NsmChassisClockControl sensor(
        bus, "ClockCtrl_CtorAssoc", cpuConfigIntf, clearClockIntf, assocs, type,
        invPath + "/ctor_assoc",
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU",
        "com.nvidia.ClockMode.Mode.MaximumPerformance");

    EXPECT_NE(sensor.associationDefinitionsInft, nullptr);
    auto assocList = sensor.associationDefinitionsInft->associations();
    EXPECT_EQ(assocList.size(), 2u);
    EXPECT_EQ(std::get<0>(assocList[0]), "chassis");
    EXPECT_EQ(std::get<1>(assocList[0]), "all_controls");
    EXPECT_EQ(std::get<0>(assocList[1]), "parent");
    EXPECT_EQ(std::get<1>(assocList[1]), "child");
}

// ===========================================================================
// NsmChassisClockControl constructor: different clockMode
// ===========================================================================
TEST_F(NsmGpuClockControlBranch3Test, Constructor_DifferentClockMode)
{
    auto& bus = utils::DBusHandler::getBus();
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, invPath.c_str());
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);
    std::vector<utils::Association> assocs;
    std::string type = "NSM_ClockControl";

    NsmChassisClockControl sensor(
        bus, "ClockCtrl_CtorMode", cpuConfigIntf, clearClockIntf, assocs, type,
        invPath + "/ctor_mode",
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU",
        "com.nvidia.ClockMode.Mode.MaximumPerformance");

    EXPECT_NE(sensor.processorModeIntf, nullptr);
    EXPECT_EQ(sensor.processorModeIntf->clockMode(), Mode::MaximumPerformance);
}

// ===========================================================================
// NsmChassisClockControl::genRequestMsg - success
// ===========================================================================
TEST_F(NsmGpuClockControlBranch3Test, GenRequestMsg_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, invPath.c_str());
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);
    std::vector<utils::Association> assocs;
    std::string type = "NSM_ClockControl";
    NsmChassisClockControl sensor(
        bus, "ClockCtrl_B3GenOK", cpuConfigIntf, clearClockIntf, assocs, type,
        invPath + "/b3_gen_ok",
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU",
        "com.nvidia.ClockMode.Mode.MaximumPerformance");

    auto request = sensor.genRequestMsg(10, 1);
    EXPECT_TRUE(request.has_value());
    EXPECT_GE(request->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_limit_req));
}

// ===========================================================================
// NsmChassisClockControl::handleResponseMsg - success path
// ===========================================================================
TEST_F(NsmGpuClockControlBranch3Test, HandleResponseMsg_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, invPath.c_str());
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);
    std::vector<utils::Association> assocs;
    std::string type = "NSM_ClockControl";
    NsmChassisClockControl sensor(
        bus, "ClockCtrl_B3HRSucc", cpuConfigIntf, clearClockIntf, assocs, type,
        invPath + "/b3_hr_succ",
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU",
        "com.nvidia.ClockMode.Mode.MaximumPerformance");

    struct nsm_clock_limit clockLimit = {};
    clockLimit.requested_limit_min = 500;
    clockLimit.requested_limit_max = 1800;

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_clock_limit_resp));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_clock_limit_resp(0, NSM_SUCCESS, ERR_NULL, &clockLimit,
                                          msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = sensor.handleResponseMsg(msg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    auto limits = cpuConfigIntf->requestedSpeedLimits();
    EXPECT_EQ(std::get<0>(limits), 500u);
    EXPECT_EQ(std::get<1>(limits), 1800u);
}

// ===========================================================================
// NsmChassisClockControl::handleResponseMsg - failure (error CC)
// ===========================================================================
TEST_F(NsmGpuClockControlBranch3Test, HandleResponseMsg_ErrorCC)
{
    auto& bus = utils::DBusHandler::getBus();
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, invPath.c_str());
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);
    std::vector<utils::Association> assocs;
    std::string type = "NSM_ClockControl";
    NsmChassisClockControl sensor(
        bus, "ClockCtrl_B3HRErr", cpuConfigIntf, clearClockIntf, assocs, type,
        invPath + "/b3_hr_err",
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU",
        "com.nvidia.ClockMode.Mode.MaximumPerformance");

    auto resp = decodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto cc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

// ===========================================================================
// NsmChassisClockControl::handleResponseMsg - decode success but non-zero CC
// ===========================================================================
TEST_F(NsmGpuClockControlBranch3Test, HandleResponseMsg_DecodeSuccessNonZeroCC)
{
    auto& bus = utils::DBusHandler::getBus();
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, invPath.c_str());
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);
    std::vector<utils::Association> assocs;
    std::string type = "NSM_ClockControl";
    NsmChassisClockControl sensor(
        bus, "ClockCtrl_B3NZCC", cpuConfigIntf, clearClockIntf, assocs, type,
        invPath + "/b3_hr_nzcc",
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU",
        "com.nvidia.ClockMode.Mode.MaximumPerformance");

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_header_info hdr = {};
    hdr.nsm_msg_type = NSM_RESPONSE;
    hdr.instance_id = 0;
    hdr.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;
    pack_nsm_header(&hdr, &msg->hdr);
    encode_reason_code(NSM_ERROR, ERR_NULL, NSM_GET_CLOCK_LIMIT, msg);

    auto cc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_EQ(cc, NSM_ERROR);
}

// ===========================================================================
// NsmChassisClockControl::handleResponseMsg - success updates speed limits
// ===========================================================================
TEST_F(NsmGpuClockControlBranch3Test,
       HandleResponseMsg_Success_UpdatesSpeedLimits)
{
    auto& bus = utils::DBusHandler::getBus();
    auto cpuConfigIntf =
        std::make_shared<CpuOperatingConfigIntf>(bus, invPath.c_str());
    // Set initial limits
    cpuConfigIntf->requestedSpeedLimits(std::make_tuple(100u, 1000u));
    auto clearClockIntf =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, invPath.c_str(), gpu);
    std::vector<utils::Association> assocs;
    std::string type = "NSM_ClockControl";
    NsmChassisClockControl sensor(
        bus, "ClockCtrl_B3Update", cpuConfigIntf, clearClockIntf, assocs, type,
        invPath + "/b3_update",
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU",
        "com.nvidia.ClockMode.Mode.MaximumPerformance");

    // Verify initial limits
    auto initialLimits = cpuConfigIntf->requestedSpeedLimits();
    EXPECT_EQ(std::get<0>(initialLimits), 100u);
    EXPECT_EQ(std::get<1>(initialLimits), 1000u);

    // Build a success response with new limits
    struct nsm_clock_limit clockLimit = {};
    clockLimit.requested_limit_min = 750;
    clockLimit.requested_limit_max = 2500;

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_clock_limit_resp));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_clock_limit_resp(0, NSM_SUCCESS, ERR_NULL, &clockLimit,
                                          msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = sensor.handleResponseMsg(msg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);

    // Verify limits were updated
    auto updatedLimits = cpuConfigIntf->requestedSpeedLimits();
    EXPECT_EQ(std::get<0>(updatedLimits), 750u);
    EXPECT_EQ(std::get<1>(updatedLimits), 2500u);
}

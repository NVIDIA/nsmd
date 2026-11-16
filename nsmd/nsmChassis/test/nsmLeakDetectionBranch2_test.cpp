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
 * Branch coverage tests for NsmLeakDetection in
 * nsmd/nsmChassis/nsmLeakDetection.cpp (handleResponseMsg,
 * getSensorInterfaces). Feature creation is covered by
 * nsmLeakDetection_test.cpp (createLeakDetectionFeature).
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "libnsm/platform-environmental.h"

#include "nsmLeakDetection.hpp"

#undef private
#undef protected

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================
struct NsmLeakDetectionBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:1";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmLeakDetectionBranch2Test() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    ~NsmLeakDetectionBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// handleResponseMsg - cc != 0 path (cc ? cc : rc returns cc)
// ============================================================================
TEST_F(NsmLeakDetectionBranch2Test, HandleResponseMsg_CcNonZero_ReturnsCc)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "LeakResp_cc";
    auto ld = std::make_shared<NsmLeakDetection>(
        name, "NSM_LeakDetection", bus, std::vector<uint64_t>{1},
        std::vector<std::string>{"sensor_cc"},
        "/xyz/openbmc_project/inventory/system/chassis/HGX_BR", 5.0, 0.0);

    // Build response with cc = NSM_ERROR
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto hdr = reinterpret_cast<nsm_common_non_success_resp*>(msg->payload);
    hdr->completion_code = NSM_ERROR;
    hdr->reason_code = htole16(ERR_NULL);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // ensure valgrind-safe

    auto rc = ld->handleResponseMsg(msg, buf.size());
    // cc != 0 -> returns cc
    EXPECT_EQ(rc, NSM_ERROR);
}

// ============================================================================
// handleResponseMsg - rc != 0, cc == 0 path (cc ? cc : rc returns rc)
// ============================================================================
TEST_F(NsmLeakDetectionBranch2Test, HandleResponseMsg_RcNonZero_ReturnsRc)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "LeakResp_rc";
    auto ld = std::make_shared<NsmLeakDetection>(
        name, "NSM_LeakDetection", bus, std::vector<uint64_t>{1},
        std::vector<std::string>{"sensor_rc"},
        "/xyz/openbmc_project/inventory/system/chassis/HGX_BR", 5.0, 0.0);

    // Tiny buffer -> decode fails with rc != 0, cc stays 0
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    // completion_code stays 0, but buffer too small for valid decode

    auto rc = ld->handleResponseMsg(msg, buf.size());
    // rc != 0, cc == 0 -> returns rc (the false branch of cc?cc:rc)
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// getSensorInterfaces - sensorId found (returns tuple)
// ============================================================================
TEST_F(NsmLeakDetectionBranch2Test, GetSensorInterfaces_Found)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "LeakIntf";
    auto ld = std::make_shared<NsmLeakDetection>(
        name, "NSM_LeakDetection", bus, std::vector<uint64_t>{5},
        std::vector<std::string>{"sensor_intf"},
        "/xyz/openbmc_project/inventory/system/chassis/HGX_BR", 5.0, 0.0);

    auto result = ld->getSensorInterfaces(5);
    EXPECT_TRUE(result.has_value());
}

// ============================================================================
// getSensorInterfaces - sensorId not found (returns nullopt)
// ============================================================================
TEST_F(NsmLeakDetectionBranch2Test, GetSensorInterfaces_NotFound)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "LeakIntf2";
    auto ld = std::make_shared<NsmLeakDetection>(
        name, "NSM_LeakDetection", bus, std::vector<uint64_t>{5},
        std::vector<std::string>{"sensor_intf2"},
        "/xyz/openbmc_project/inventory/system/chassis/HGX_BR", 5.0, 0.0);

    auto result = ld->getSensorInterfaces(99);
    EXPECT_FALSE(result.has_value());
}

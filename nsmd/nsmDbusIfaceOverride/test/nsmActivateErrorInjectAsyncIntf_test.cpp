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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "device-configuration.h"

#include "nsmDbusIfaceOverride/nsmActivateErrorInjectAsyncIntf.hpp"

using namespace nsm;

static std::vector<uint8_t> makeActivateResp(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_activate_error_injection_payload_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, msg);
    return buf;
}

struct NsmActivateErrorInjectTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmActivateErrorInjectTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmActivateErrorInjectTest()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<NsmActivateErrorInjectionPayloadIntf>
        makeIntf(const char* path)
    {
        return std::make_shared<NsmActivateErrorInjectionPayloadIntf>(
            utils::DBusHandler::getBus(), path, 1u, 2u, gpu);
    }
};

// doActivateErrorInjectionPayload: postPatchIO fails → WriteFailure
TEST_F(NsmActivateErrorInjectTest, DoActivate_PostPatchIOFails_WriteFailure)
{
    auto intf = makeIntf("/xyz/openbmc_project/ei/activate_piof");
    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        utils::DBusHandler::getBus(),
        "/xyz/openbmc_project/async/status/ei_piof");
    statusIntf->status(AsyncOperationStatusType::InProgress);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeActivateResp(), NSM_ERROR));

    auto coro = intf->doActivateErrorInjectionPayload(statusIntf);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// doActivateErrorInjectionPayload: postPatchIO succeeds, error CC →
// WriteFailure
TEST_F(NsmActivateErrorInjectTest, DoActivate_ErrorCC_WriteFailure)
{
    auto intf = makeIntf("/xyz/openbmc_project/ei/activate_errcc");
    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        utils::DBusHandler::getBus(),
        "/xyz/openbmc_project/async/status/ei_errcc");
    statusIntf->status(AsyncOperationStatusType::InProgress);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeActivateResp(NSM_ERROR)));

    auto coro = intf->doActivateErrorInjectionPayload(statusIntf);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// doActivateErrorInjectionPayload: success → statusInterface = Success
TEST_F(NsmActivateErrorInjectTest, DoActivate_Success_StatusSuccess)
{
    auto intf = makeIntf("/xyz/openbmc_project/ei/activate_ok");
    auto statusIntf = std::make_shared<AsyncStatusIntf>(
        utils::DBusHandler::getBus(),
        "/xyz/openbmc_project/async/status/ei_ok");
    statusIntf->status(AsyncOperationStatusType::InProgress);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeActivateResp()));

    auto coro = intf->doActivateErrorInjectionPayload(statusIntf);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// activate: async operation objects available → returns non-empty object path
TEST_F(NsmActivateErrorInjectTest, Activate_ObjectsAvailable_ReturnsObjectPath)
{
    auto intf = makeIntf("/xyz/openbmc_project/ei/activate_ok2");

    // Provide a response so the detached coroutine can complete cleanly
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillRepeatedly(mockPostPatchIO(makeActivateResp()));

    auto objectPath = intf->activate();
    EXPECT_FALSE(objectPath.str.empty());
}

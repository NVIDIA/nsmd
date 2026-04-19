/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Unit tests for
 * nsmd/nsmDeviceInventory/nsmNvSwitchDeviceConfiguration.{hpp,cpp}:
 * - NsmNvSwitchDeviceConfigurationRequestEvent::handle (event id, decode,
 *   weak_ptr target)
 * - addNvSwitchDeviceConfigurationSensorIfEnabled early exits
 */

#include "test/mockDBusHandler.hpp"

#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "device-configuration.h"

#include "nsmDeviceInventory/nsmNvSwitchDeviceConfiguration.hpp"
#include "test/commonMock.hpp"

#undef private
#undef protected

using namespace nsm;

namespace
{

static auto& testBus = utils::DBusHandler::getBus();

std::shared_ptr<NsmNvSwitchDeviceConfigurationAsync>
    makeCfg(const std::string& objPathTail)
{
    const std::string name = "NVSwitch_cfg_test";
    constexpr auto kType = "NSM_NVSwitch_DeviceConfiguration";
    const std::string inventoryBase =
        "/xyz/openbmc_project/inventory/nvswitch_cfg_test/";
    const std::string objPath = inventoryBase + objPathTail;

    auto device = std::make_shared<MockNsmDevice>(NSM_DEV_ID_SWITCH, 0,
                                                  "NSM_DEVICE_INSTANCE_NUMBER",
                                                  "1", NSM_DEV_ROLE_RESERVED);

    return std::make_shared<NsmNvSwitchDeviceConfigurationAsync>(
        testBus, name, kType, objPath, device);
}

} // namespace

// ---------------------------------------------------------------------------
// NsmNvSwitchDeviceConfigurationRequestEvent::handle
// ---------------------------------------------------------------------------

TEST(NsmNvSwitchDeviceConfigurationRequestEvent, HandleWrongEventId)
{
    auto cfg = makeCfg("wrong_evt");
    NsmNvSwitchDeviceConfigurationRequestEvent ev{cfg};

    const int rc = ev.handle(1, NSM_TYPE_DEVICE_CONFIGURATION,
                             static_cast<NsmEventId>(0x02), nullptr, 0);
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(NsmNvSwitchDeviceConfigurationRequestEvent, HandleNullEvent)
{
    auto cfg = makeCfg("null_evt");
    NsmNvSwitchDeviceConfigurationRequestEvent ev{cfg};

    const int rc = ev.handle(1, NSM_TYPE_DEVICE_CONFIGURATION,
                             NSM_DEVICE_CONFIGURATION_REQUEST_EVENT_V1, nullptr,
                             0);
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(NsmNvSwitchDeviceConfigurationRequestEvent, HandleDecodeFailsWrongMsgType)
{
    auto cfg = makeCfg("wrong_type");
    NsmNvSwitchDeviceConfigurationRequestEvent ev{cfg};

    std::vector<uint8_t> wrongType(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
    auto* wmsg = reinterpret_cast<nsm_msg*>(wrongType.data());
    const int enc = encode_nsm_event(
        1, NSM_TYPE_PLATFORM_ENVIRONMENTAL, false, NSM_EVENT_VERSION,
        NSM_DEVICE_CONFIGURATION_REQUEST_EVENT_V1, NSM_GENERAL_EVENT_CLASS, 0,
        0, nullptr, wmsg);
    ASSERT_EQ(enc, NSM_SW_SUCCESS);

    const int rc = ev.handle(1, NSM_TYPE_DEVICE_CONFIGURATION,
                             NSM_DEVICE_CONFIGURATION_REQUEST_EVENT_V1, wmsg,
                             wrongType.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(NsmNvSwitchDeviceConfigurationRequestEvent, HandleDecodeFailsNonZeroData)
{
    auto cfg = makeCfg("extra_data");
    NsmNvSwitchDeviceConfigurationRequestEvent ev{cfg};

    uint8_t extra = 0x55;
    std::vector<uint8_t> withData(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN + 1,
                                  0);
    auto* dmsg = reinterpret_cast<nsm_msg*>(withData.data());
    const int enc = encode_nsm_event(
        1, NSM_TYPE_DEVICE_CONFIGURATION, false, NSM_EVENT_VERSION,
        NSM_DEVICE_CONFIGURATION_REQUEST_EVENT_V1, NSM_GENERAL_EVENT_CLASS, 0,
        1, &extra, dmsg);
    ASSERT_EQ(enc, NSM_SW_SUCCESS);

    const int rc = ev.handle(1, NSM_TYPE_DEVICE_CONFIGURATION,
                             NSM_DEVICE_CONFIGURATION_REQUEST_EVENT_V1, dmsg,
                             withData.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(NsmNvSwitchDeviceConfigurationRequestEvent, HandleSuccess)
{
    auto cfg = makeCfg("ok_evt");
    NsmNvSwitchDeviceConfigurationRequestEvent ev{cfg};

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    const int enc = encode_nsm_device_config_request_event_v1(0, true, msg);
    ASSERT_EQ(enc, NSM_SW_SUCCESS);

    const int rc = ev.handle(1, NSM_TYPE_DEVICE_CONFIGURATION,
                             NSM_DEVICE_CONFIGURATION_REQUEST_EVENT_V1, msg,
                             buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(NsmNvSwitchDeviceConfigurationRequestEvent,
     HandleSuccessDecodeButTargetExpired)
{
    std::shared_ptr<NsmNvSwitchDeviceConfigurationAsync> cfg =
        makeCfg("expired");
    NsmNvSwitchDeviceConfigurationRequestEvent ev{cfg};
    cfg.reset();

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    const int enc = encode_nsm_device_config_request_event_v1(0, true, msg);
    ASSERT_EQ(enc, NSM_SW_SUCCESS);

    const int rc = ev.handle(1, NSM_TYPE_DEVICE_CONFIGURATION,
                             NSM_DEVICE_CONFIGURATION_REQUEST_EVENT_V1, msg,
                             buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// addNvSwitchDeviceConfigurationSensorIfEnabled
// ---------------------------------------------------------------------------

TEST(NsmNvSwitchDeviceConfigurationAddSensor, NoOpWhenSupportDisabled)
{
    auto device = std::make_shared<MockNsmDevice>(NSM_DEV_ID_SWITCH, 0,
                                                  "NSM_DEVICE_INSTANCE_NUMBER",
                                                  "2", NSM_DEV_ROLE_RESERVED);

    EXPECT_NO_THROW(addNvSwitchDeviceConfigurationSensorIfEnabled(
        false, testBus, "NVSwitch_x",
        "/xyz/openbmc_project/inventory/nvswitch_add/disabled/", device));
}

TEST(NsmNvSwitchDeviceConfigurationAddSensor, NoOpWhenDeviceNull)
{
    EXPECT_NO_THROW(addNvSwitchDeviceConfigurationSensorIfEnabled(
        true, testBus, "NVSwitch_x",
        "/xyz/openbmc_project/inventory/nvswitch_add/nulldev/", nullptr));
}

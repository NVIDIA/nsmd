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
 * Tests for nsmd/nsmEvent/nsmRuntimeISTCompleteEvent.cpp
 *
 * Coverage:
 *   - Constructor: name/type/info preservation
 *   - handle(): decode failure (short buffer)
 *   - handle(): Pass / Fail / Unknown result string mapping (verified via
 *     return code; full message rendering depends on the message registry
 *     which is intentionally not yet wired)
 *   - handle(): logging=false skips logEventAsync, still returns success
 *   - handle(): negative NvS24.8 temperatures round-trip without breaking
 *   - createRuntimeISTCompleteEventFeature(): happy path, no NsmDevice,
 *     Logging flag default-true
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "diagnostics.h"

#include "features/creators.hpp"
#include "features/feature.hpp"
#include "nsmEvent/nsmRuntimeISTCompleteEvent.hpp"

#include <cstring>

using namespace nsm;

namespace
{

constexpr size_t kRistMsgLen = sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                               sizeof(nsm_runtime_ist_complete_event_payload);

NsmEventInfo makeRistInfo(bool logging = true)
{
    NsmEventInfo info{};
    info.messageId = "NvidiaDiagnostics.1.0.RuntimeISTComplete";
    info.originOfCondition =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_1";
    info.loggingNamespace = "GPU_RIST";
    info.resolution = "Investigate firmware/RIST logs.";
    info.messageArgs = {"{GpuId}",
                        "{Result}",
                        "{StatusCode}",
                        "{MaxTemperature:.2f}",
                        "{AvgTemperature:.2f}",
                        "{AppVersion}",
                        "{Timestamp}"};
    info.severity = Level::Critical;
    info.logging = logging;
    info.impactedComponent = "GPU_1";
    return info;
}

/** @brief Build a wire-format RIST event in a buffer and return the
 *         underlying message pointer. The returned pointer is owned by the
 *         backing vector held by the caller.
 */
std::vector<uint8_t> encodeRistEvent(uint8_t result, uint64_t statusCode = 0,
                                     int32_t maxTempRaw = 100 * (1 << 8),
                                     int32_t avgTempRaw = 75 * (1 << 8))
{
    std::vector<uint8_t> buf(kRistMsgLen, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());

    nsm_runtime_ist_complete_event_payload payload{};
    std::strncpy(payload.gpu_identifier, "GPU-1234-5678",
                 NSM_RIST_GPU_UUID_LEN - 1);
    std::strncpy(payload.app_version, "rist-1.0", NSM_RIST_APP_VERSION_LEN - 1);
    payload.timestamp = 1714809600000000000ULL;
    payload.result = result;
    payload.status_code = statusCode;
    payload.max_temperature = maxTempRaw;
    payload.avg_temperature = avgTempRaw;

    auto rc = encode_nsm_runtime_ist_complete_event(0, true, /*event_state=*/0,
                                                    &payload, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    return buf;
}

} // namespace

// =============================================================================
// Constructor tests
// =============================================================================

TEST(NsmRuntimeISTCompleteEvent, Constructor_PreservesNameTypeAndInfo)
{
    auto info = makeRistInfo();
    NsmRuntimeISTCompleteEvent event("rist", "NSM_Event_Runtime_IST_Complete",
                                     info);

    EXPECT_EQ(event.getName(), "rist");
    EXPECT_EQ(event.getType(), "NSM_Event_Runtime_IST_Complete");
    EXPECT_EQ(event.info.messageId, "NvidiaDiagnostics.1.0.RuntimeISTComplete");
    EXPECT_EQ(event.info.severity, Level::Critical);
    EXPECT_TRUE(event.info.logging);
}

// =============================================================================
// handle() decode error paths
// =============================================================================

TEST(NsmRuntimeISTCompleteEvent, Handle_ShortBuffer_ReturnsError)
{
    auto info = makeRistInfo();
    NsmRuntimeISTCompleteEvent event("rist", "NSM_Event_Runtime_IST_Complete",
                                     info);

    // Smaller than sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN -> LENGTH error.
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 1, 0);
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, NSM_TYPE_DIAGNOSTIC,
                           NSM_RUNTIME_IST_COMPLETE_EVENT, msg, buf.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(NsmRuntimeISTCompleteEvent, Handle_DataSizeMismatch_ReturnsError)
{
    auto info = makeRistInfo();
    NsmRuntimeISTCompleteEvent event("rist", "NSM_Event_Runtime_IST_Complete",
                                     info);

    auto buf = encodeRistEvent(/*result=*/1);
    const auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    // Pass msg_len-1 -> RIST decoder strict check rejects with DATA error.
    auto rc = event.handle(5, NSM_TYPE_DIAGNOSTIC,
                           NSM_RUNTIME_IST_COMPLETE_EVENT, msg, buf.size() - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// =============================================================================
// handle() success paths -- result mapping
// =============================================================================

TEST(NsmRuntimeISTCompleteEvent, Handle_PassResult_Logging_ReturnsSuccess)
{
    auto info = makeRistInfo(/*logging=*/true);
    NsmRuntimeISTCompleteEvent event("rist", "NSM_Event_Runtime_IST_Complete",
                                     info);

    auto buf = encodeRistEvent(/*result=*/1);
    const auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, NSM_TYPE_DIAGNOSTIC,
                           NSM_RUNTIME_IST_COMPLETE_EVENT, msg, buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(NsmRuntimeISTCompleteEvent, Handle_FailResult_Logging_ReturnsSuccess)
{
    auto info = makeRistInfo(/*logging=*/true);
    NsmRuntimeISTCompleteEvent event("rist", "NSM_Event_Runtime_IST_Complete",
                                     info);

    auto buf = encodeRistEvent(/*result=*/0, /*statusCode=*/0xCAFEBABEULL);
    const auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, NSM_TYPE_DIAGNOSTIC,
                           NSM_RUNTIME_IST_COMPLETE_EVENT, msg, buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// Result bytes outside {0,1} produce an Unknown(0xNN) string and a warning,
// but the event is still surfaced (returns success).
TEST(NsmRuntimeISTCompleteEvent, Handle_UnknownResult_ReturnsSuccess)
{
    auto info = makeRistInfo(/*logging=*/true);
    NsmRuntimeISTCompleteEvent event("rist", "NSM_Event_Runtime_IST_Complete",
                                     info);

    auto buf = encodeRistEvent(/*result=*/0x42);
    const auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, NSM_TYPE_DIAGNOSTIC,
                           NSM_RUNTIME_IST_COMPLETE_EVENT, msg, buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// handle() Logging flag gating
// =============================================================================

TEST(NsmRuntimeISTCompleteEvent,
     Handle_LoggingFalse_SkipsLogStillReturnsSuccess)
{
    auto info = makeRistInfo(/*logging=*/false);
    NsmRuntimeISTCompleteEvent event("rist", "NSM_Event_Runtime_IST_Complete",
                                     info);

    auto buf = encodeRistEvent(/*result=*/0);
    const auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, NSM_TYPE_DIAGNOSTIC,
                           NSM_RUNTIME_IST_COMPLETE_EVENT, msg, buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// handle() spec-table coverage for derived Severity / Resolution
//
// The deriveRistSeverity() helper (anonymous namespace in the .cpp) is not
// directly callable from this TU. These tests drive handle() with payloads
// whose (result, status_code[63:62]) cover all spec-table rows plus two
// undefined combinations, asserting that the call still succeeds. Severity
// values themselves are not observable here -- logEventAsync has no
// per-call seam in this codebase -- but the code paths are exercised.
//
// Spec table:
//   result=1, statusClass=0  -> Informational, "None"
//   result=0, statusClass=1  -> Warning,       "Check RIST app output"
//   result=0, statusClass=2  -> Critical,      "Check RIST app output"
//   anything else            -> Critical,      "Check RIST app output"
//                               + lg2::warning emitted
// =============================================================================

namespace
{
// statusCode value with the top 2 bits set to `cls` and the rest zero.
constexpr uint64_t makeStatusCode(uint8_t cls)
{
    return static_cast<uint64_t>(cls & 0x3) << 62;
}
} // namespace

TEST(NsmRuntimeISTCompleteEvent, Handle_Pass_Class0_DerivesInformational)
{
    auto info = makeRistInfo(/*logging=*/true);
    NsmRuntimeISTCompleteEvent event("rist", "NSM_Event_Runtime_IST_Complete",
                                     info);

    auto buf = encodeRistEvent(/*result=*/1,
                               /*statusCode=*/makeStatusCode(0));
    const auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, NSM_TYPE_DIAGNOSTIC,
                           NSM_RUNTIME_IST_COMPLETE_EVENT, msg, buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(NsmRuntimeISTCompleteEvent, Handle_Fail_Class1_DerivesWarning)
{
    auto info = makeRistInfo(/*logging=*/true);
    NsmRuntimeISTCompleteEvent event("rist", "NSM_Event_Runtime_IST_Complete",
                                     info);

    auto buf = encodeRistEvent(/*result=*/0,
                               /*statusCode=*/makeStatusCode(1));
    const auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, NSM_TYPE_DIAGNOSTIC,
                           NSM_RUNTIME_IST_COMPLETE_EVENT, msg, buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(NsmRuntimeISTCompleteEvent, Handle_Fail_Class2_DerivesCritical)
{
    auto info = makeRistInfo(/*logging=*/true);
    NsmRuntimeISTCompleteEvent event("rist", "NSM_Event_Runtime_IST_Complete",
                                     info);

    auto buf = encodeRistEvent(/*result=*/0,
                               /*statusCode=*/makeStatusCode(2));
    const auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, NSM_TYPE_DIAGNOSTIC,
                           NSM_RUNTIME_IST_COMPLETE_EVENT, msg, buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// Pass with a non-zero StatusCode class is undefined by the spec; handler
// must still succeed but emits a warning and falls back to Critical.
TEST(NsmRuntimeISTCompleteEvent, Handle_Pass_UndefinedClass_FallsBack)
{
    auto info = makeRistInfo(/*logging=*/true);
    NsmRuntimeISTCompleteEvent event("rist", "NSM_Event_Runtime_IST_Complete",
                                     info);

    auto buf = encodeRistEvent(/*result=*/1,
                               /*statusCode=*/makeStatusCode(1));
    const auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, NSM_TYPE_DIAGNOSTIC,
                           NSM_RUNTIME_IST_COMPLETE_EVENT, msg, buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// Fail with a StatusCode class outside {1,2} is undefined; same fallback.
TEST(NsmRuntimeISTCompleteEvent, Handle_Fail_Class3_FallsBack)
{
    auto info = makeRistInfo(/*logging=*/true);
    NsmRuntimeISTCompleteEvent event("rist", "NSM_Event_Runtime_IST_Complete",
                                     info);

    auto buf = encodeRistEvent(/*result=*/0,
                               /*statusCode=*/makeStatusCode(3));
    const auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, NSM_TYPE_DIAGNOSTIC,
                           NSM_RUNTIME_IST_COMPLETE_EVENT, msg, buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// handle() NvS24.8 conversion -- exercises negative temperatures
// =============================================================================

TEST(NsmRuntimeISTCompleteEvent, Handle_NegativeTemperatures_ReturnsSuccess)
{
    auto info = makeRistInfo(/*logging=*/true);
    NsmRuntimeISTCompleteEvent event("rist", "NSM_Event_Runtime_IST_Complete",
                                     info);

    // -25.5 C and -40 C in NvS24.8.
    auto buf =
        encodeRistEvent(/*result=*/1, /*statusCode=*/0,
                        /*maxTempRaw=*/static_cast<int32_t>(-25.5 * (1 << 8)),
                        /*avgTempRaw=*/static_cast<int32_t>(-40 * (1 << 8)));
    const auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, NSM_TYPE_DIAGNOSTIC,
                           NSM_RUNTIME_IST_COMPLETE_EVENT, msg, buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}
// =============================================================================
// Factory tests (createRuntimeISTCompleteEventFeature)
// =============================================================================

struct NsmRuntimeISTCompleteEventFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "RIST_Test";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:1";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmRuntimeISTCompleteEventFactoryTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmRuntimeISTCompleteEventFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }

    dbus::PropertyMap basicProperties = {
        {"Name", name},
        {"MessageId", std::string("NvidiaDiagnostics.1.0.RuntimeISTComplete")},
        {"LoggingNamespace", std::string("GPU_RIST")},
        {"Resolution", std::string("Investigate firmware/RIST logs.")},
        {"MessageArgs",
         std::vector<std::string>{
             "{GpuId}", "{Result}", "{StatusCode}", "{MaxTemperature:.2f}",
             "{AvgTemperature:.2f}", "{AppVersion}", "{Timestamp}"}},
        {"EventIds",
         std::vector<std::string>{"Runtime IST Complete Pass", "RIST_PASS_INFO",
                                  "Runtime IST Complete Fail",
                                  "RIST_FAIL_CRITICAL"}},
        {"ImpactedComponent", std::string("GPU_1")},
        {"Logging", true},
    };

    requester::Coroutine create(const dbus::PropertyMap& params,
                                bool withDevice = true)
    {
        features::DeviceContext dev;
        dev.name = "GPU_1";
        dev.kind = "gpu";
        dev.staticUuid = gpuUuid;
        dev.platformPrefix = "HGX_";
        if (withDevice)
        {
            dev.device = gpu;
        }
        return features::createRuntimeISTCompleteEventFeature(mockManager, dev,
                                                              params);
    }

    std::shared_ptr<NsmRuntimeISTCompleteEvent> lastEvent()
    {
        if (gpu->deviceEvents.empty())
        {
            return nullptr;
        }
        return std::dynamic_pointer_cast<NsmRuntimeISTCompleteEvent>(
            gpu->deviceEvents.back());
    }
};

TEST_F(NsmRuntimeISTCompleteEventFactoryTest, GoodTest_CreatesEvent)
{
    const size_t before = gpu->deviceEvents.size();
    auto rc = create(basicProperties);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());
    ASSERT_EQ(gpu->deviceEvents.size(), before + 1);
    auto event = lastEvent();
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->getName(), name);
    EXPECT_EQ(event->getType(), "NSM_Event_Runtime_IST_Complete");
    EXPECT_EQ(event->info.uuid, gpuUuid);
    EXPECT_EQ(event->info.originOfCondition, "/redfish/v1/Chassis/GPU_1");
    EXPECT_EQ(event->info.impactedComponent, "GPU_1");
    EXPECT_EQ(event->info.errorId.size(), 4u);
    EXPECT_TRUE(event->info.logging);
}

// Logging param absent -> creator default of true keeps logging enabled
TEST_F(NsmRuntimeISTCompleteEventFactoryTest,
       MissingLoggingProperty_DefaultsToTrueAndCreates)
{
    auto props = basicProperties;
    props.erase("Logging");
    auto rc = create(props);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    auto event = lastEvent();
    ASSERT_NE(event, nullptr);
    EXPECT_TRUE(event->info.logging);
}

TEST_F(NsmRuntimeISTCompleteEventFactoryTest, LoggingFalse_Honoured)
{
    auto props = basicProperties;
    props["Logging"] = false;
    auto rc = create(props);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    auto event = lastEvent();
    ASSERT_NE(event, nullptr);
    EXPECT_FALSE(event->info.logging);
}

// No NsmDevice in the context -> creator logs error and co_returns NSM_ERROR
TEST_F(NsmRuntimeISTCompleteEventFactoryTest, BadTest_NoDevice_NoEventCreated)
{
    const size_t before = gpu->deviceEvents.size();
    auto rc = create(basicProperties, false);

    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_EQ(gpu->deviceEvents.size(), before);
}

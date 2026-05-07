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

#pragma once

#include "diagnostics.h"

#include "asyncOperationManager.hpp"
#include "nsmDevice.hpp"
#include "sensorManager.hpp"

#include <com/nvidia/PreBootDiag/Config/System/server.hpp>
#include <com/nvidia/PreBootDiag/Config/TID/server.hpp>
#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

namespace nsm
{

using PreBootDiagConfigSystemIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::PreBootDiag::Config::server::System>;
using PreBootDiagConfigTIDIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::PreBootDiag::Config::server::TID>;

namespace prebootdiag
{

/// Read a JSON integer field, validate it falls in [min, max], and
/// throw InvalidArgument with a logged context on failure. Avoids the
/// silent modulo-256 wrap of nlohmann::json::get<uint8_t>() — see
/// design §5.2.3 which requires explicit range validation.
inline int getBoundedInt(const nlohmann::json& j, const char* key, int minVal,
                         int maxVal, eid_t eid)
{
    if (!j.contains(key))
    {
        lg2::error("PreBootDiag: missing field {KEY} EID={EID}", "KEY", key,
                   "EID", eid);
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }
    int v = j[key].get<int>();
    if (v < minVal || v > maxVal)
    {
        lg2::error(
            "PreBootDiag: {KEY}={VAL} out of range [{MIN}, {MAX}] EID={EID}",
            "KEY", key, "VAL", v, "MIN", minVal, "MAX", maxVal, "EID", eid);
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }
    return v;
}

inline int getBoundedIntOr(const nlohmann::json& j, const char* key,
                           int defaultVal, int minVal, int maxVal, eid_t eid)
{
    if (!j.contains(key))
    {
        return defaultVal;
    }
    return getBoundedInt(j, key, minVal, maxVal, eid);
}

inline requester::Coroutine sendSystemConfig(std::shared_ptr<NsmDevice> dev,
                                             eid_t eid,
                                             const nlohmann::json& json_in)
{
    // The upstream prebootdiag service stores DiagSystemConfig as a
    // 1-element array for schema parity with DiagConfig (which is
    // genuinely per-TID). Accept both `{…}` and `[{…}]` so it doesn't
    // matter which side does the unwrap.
    const auto& json = (json_in.is_array() && !json_in.empty()) ? json_in[0]
                                                                : json_in;

    // Spec §5.2.3 — ConfigType ∈ [0, 1], TestDuration ∈ [0, 3].
    uint8_t configType =
        static_cast<uint8_t>(getBoundedInt(json, "ConfigType", 0, 1, eid));
    uint8_t testDuration =
        static_cast<uint8_t>(getBoundedInt(json, "TestDuration", 0, 3, eid));

    std::vector<uint8_t> dynamicData;
    if (json.contains("DynamicData"))
    {
        // Validate the JSON node *before* materializing it into a
        // vector — otherwise a 100MB DynamicData entry would force the
        // daemon to allocate that buffer just to reject it.
        const auto& ddNode = json["DynamicData"];
        if (!ddNode.is_array())
        {
            lg2::error(
                "PreBootDiag: SystemConfig DynamicData is not an array EID={EID}",
                "EID", eid);
            throw sdbusplus::error::xyz::openbmc_project::common::
                InvalidArgument{};
        }
        if (ddNode.size() > NSM_DIAG_MAX_DYNAMIC_DATA_SIZE)
        {
            lg2::error(
                "PreBootDiag: SystemConfig DynamicData too large ({SZ} > {MAX}) EID={EID}",
                "SZ", ddNode.size(), "MAX", NSM_DIAG_MAX_DYNAMIC_DATA_SIZE,
                "EID", eid);
            throw sdbusplus::error::xyz::openbmc_project::common::
                InvalidArgument{};
        }
        dynamicData = ddNode.get<std::vector<uint8_t>>();
    }

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_diag_set_system_config_req) - 1 +
                    dynamicData.size());
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    auto rc = encode_diag_set_system_config_req(
        0, configType, testDuration,
        dynamicData.empty() ? nullptr : dynamicData.data(),
        static_cast<uint8_t>(dynamicData.size()), requestMsg);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("PreBootDiag: encode SystemConfig failed rc={RC} EID={EID}",
                   "RC", rc, "EID", eid);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc_ = co_await dev->postPatchIO(eid, request, responseMsg,
                                         responseLen);
    if (rc_)
    {
        lg2::error(
            "PreBootDiag: SystemConfig postPatchIO failed EID={EID} rc={RC}",
            "EID", eid, "RC", rc_);
        co_return rc_;
    }

    lg2::info(
        "PreBootDiag: SystemConfig sent to EID={EID} configType={CT} duration={DUR}",
        "EID", eid, "CT", configType, "DUR", testDuration);
    co_return NSM_SW_SUCCESS;
}

inline requester::Coroutine sendTidConfigs(std::shared_ptr<NsmDevice> dev,
                                           eid_t eid,
                                           const nlohmann::json& json)
{
    auto configs = json.is_array() ? json : nlohmann::json::array({json});

    // Spec §5.2.3 — TID range 0x01..0x0A, so a Set call should never
    // carry more than 10 entries. Cap here so a malicious or buggy
    // caller can't pin the daemon doing thousands of MCTP roundtrips.
    constexpr size_t maxTidConfigs = 10;
    if (configs.size() > maxTidConfigs)
    {
        lg2::error(
            "PreBootDiag: too many TID configs ({COUNT} > {MAX}) EID={EID}",
            "COUNT", configs.size(), "MAX", maxTidConfigs, "EID", eid);
        co_return NSM_SW_ERROR_DATA;
    }

    const size_t total = configs.size();
    int failures = 0;

    for (const auto& tidCfg : configs)
    {
        // Spec §5.2.3 / §10.1 — per-entry validation. Use a small
        // try/catch so a single bad entry doesn't abort the batch
        // (getBoundedInt throws InvalidArgument on out-of-range).
        uint8_t tid;
        uint8_t testDuration;
        uint16_t loops;
        uint8_t logLevel;
        std::vector<uint8_t> dynamicData;
        try
        {
            tid = static_cast<uint8_t>(
                getBoundedInt(tidCfg, "Tid", 0x01, 0x0A, eid));
            testDuration = static_cast<uint8_t>(
                getBoundedIntOr(tidCfg, "TestDuration", 0, 0, 3, eid));
            loops = static_cast<uint16_t>(
                getBoundedIntOr(tidCfg, "Loops", 1, 0, 0xFFFF, eid));
            logLevel = static_cast<uint8_t>(
                getBoundedIntOr(tidCfg, "LogLevel", 0, 0, 3, eid));
        }
        catch (const sdbusplus::exception_t&)
        {
            ++failures;
            continue;
        }

        if (tidCfg.contains("DynamicData"))
        {
            // Validate the JSON node *before* materializing it. A
            // caller could otherwise force a 100MB allocation just to
            // have the request rejected by the cap below.
            const auto& ddNode = tidCfg["DynamicData"];
            if (!ddNode.is_array())
            {
                lg2::error(
                    "PreBootDiag: DynamicData is not an array TID=0x{TID} EID={EID}",
                    "TID", lg2::hex, tid, "EID", eid);
                ++failures;
                continue;
            }
            if (ddNode.size() > NSM_DIAG_MAX_TID_DYNAMIC_DATA_SIZE)
            {
                lg2::error(
                    "PreBootDiag: DynamicData too large ({SZ} > {MAX}) for TID=0x{TID} EID={EID}",
                    "SZ", ddNode.size(), "MAX",
                    NSM_DIAG_MAX_TID_DYNAMIC_DATA_SIZE, "TID", lg2::hex, tid,
                    "EID", eid);
                ++failures;
                continue;
            }
            dynamicData = ddNode.get<std::vector<uint8_t>>();
        }
        uint8_t dynSize = static_cast<uint8_t>(dynamicData.size());

        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_diag_set_tid_config_req) - 1 + dynSize);
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

        auto rc = encode_diag_set_tid_config_req(
            0, tid, testDuration, loops, logLevel, dynSize,
            dynamicData.empty() ? nullptr : dynamicData.data(), requestMsg);

        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "PreBootDiag: encode TIDConfig failed rc={RC} TID=0x{TID} EID={EID}",
                "RC", rc, "TID", lg2::hex, tid, "EID", eid);
            ++failures;
            continue;
        }

        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        auto rc_ = co_await dev->postPatchIO(eid, request, responseMsg,
                                             responseLen);
        if (rc_)
        {
            lg2::error(
                "PreBootDiag: TIDConfig postPatchIO failed TID=0x{TID} EID={EID} rc={RC}",
                "TID", lg2::hex, tid, "EID", eid, "RC", rc_);
            ++failures;
            continue;
        }

        lg2::info(
            "PreBootDiag: TIDConfig sent to EID={EID} TID=0x{TID} duration={DUR} loops={LOOPS}",
            "EID", eid, "TID", lg2::hex, tid, "DUR", testDuration, "LOOPS",
            loops);
    }

    if (failures > 0)
    {
        lg2::error(
            "PreBootDiag: {N}/{TOTAL} TID config(s) failed to send EID={EID}",
            "N", failures, "TOTAL", total, "EID", eid);
    }

    co_return failures > 0 ? NSM_SW_ERROR : NSM_SW_SUCCESS;
}

/// Shared async-set handler body for both interfaces. Parses the JSON
/// payload and dispatches to the type-specific send helper. Writes
/// `*status` for failure cases per the AsyncSetOperationHandler contract.
inline requester::Coroutine handleConfigSet(
    const AsyncSetOperationValueType& value, AsyncOperationStatusType* status,
    std::shared_ptr<NsmDevice> device,
    requester::Coroutine (*sender)(std::shared_ptr<NsmDevice>, eid_t,
                                   const nlohmann::json&))
{
    const std::string* configJson = std::get_if<std::string>(&value);
    if (!configJson)
    {
        *status = AsyncOperationStatusType::InvalidArgument;
        co_return NSM_SW_ERROR_DATA;
    }
    eid_t eid = device->getEid();

    try
    {
        auto json = nlohmann::json::parse(*configJson);
        int rc = co_await sender(device, eid, json);
        if (rc != NSM_SW_SUCCESS)
        {
            *status = AsyncOperationStatusType::WriteFailure;
        }
        co_return rc;
    }
    catch (const nlohmann::json::exception& e)
    {
        lg2::error("PreBootDiag: JSON parse failed: {ERR}", "ERR", e.what());
        *status = AsyncOperationStatusType::InvalidArgument;
        co_return NSM_SW_ERROR_DATA;
    }
    catch (
        const sdbusplus::error::xyz::openbmc_project::common::InvalidArgument&)
    {
        *status = AsyncOperationStatusType::InvalidArgument;
        co_return NSM_SW_ERROR_DATA;
    }
    catch (...)
    {
        *status = AsyncOperationStatusType::InternalFailure;
        co_return NSM_SW_ERROR;
    }
}

} // namespace prebootdiag

/// Hosts com.nvidia.PreBootDiag.Config.System on the per-CPU object path.
///
/// The `Value` property is a write-only sentinel: clients push the JSON
/// configuration via com.nvidia.Async.Set on the same object path, not via
/// org.freedesktop.DBus.Properties.Set. The auto-generated getter returns
/// the default-constructed empty string (we never store the value
/// internally — the payload lives only inside the dispatcher's handler
/// invocation, which encodes and forwards it to the CPU over MCTP), and
/// the setter override below rejects any direct Properties.Set with
/// NotAllowed. Don't be surprised by `busctl get-property ... Value`
/// returning "" — by design.
class NsmPreBootDiagConfigSystemIntf : public PreBootDiagConfigSystemIntf
{
  public:
    NsmPreBootDiagConfigSystemIntf(sdbusplus::bus::bus& bus, const char* path) :
        PreBootDiagConfigSystemIntf(bus, path)
    {}

    // Reject direct property writes — clients must go through Async.Set.
    std::string value(std::string /*newValue*/) override
    {
        lg2::warning(
            "PreBootDiag.Config.System: direct Properties.Set rejected — use Async.Set");
        throw sdbusplus::error::xyz::openbmc_project::common::NotAllowed{};
    }

    requester::Coroutine setValueAsync(const AsyncSetOperationValueType& value,
                                       AsyncOperationStatusType* status,
                                       std::shared_ptr<NsmDevice> device)
    {
        co_return co_await prebootdiag::handleConfigSet(
            value, status, std::move(device), &prebootdiag::sendSystemConfig);
    }
};

/// Hosts com.nvidia.PreBootDiag.Config.TID on the per-CPU object path.
/// Same write-only-via-Async.Set semantic as NsmPreBootDiagConfigSystemIntf
/// (see that class's doc for details). Direct Properties.Set is rejected
/// with NotAllowed; the Get always returns "".
class NsmPreBootDiagConfigTIDIntf : public PreBootDiagConfigTIDIntf
{
  public:
    NsmPreBootDiagConfigTIDIntf(sdbusplus::bus::bus& bus, const char* path) :
        PreBootDiagConfigTIDIntf(bus, path)
    {}

    std::string value(std::string /*newValue*/) override
    {
        lg2::warning(
            "PreBootDiag.Config.TID: direct Properties.Set rejected — use Async.Set");
        throw sdbusplus::error::xyz::openbmc_project::common::NotAllowed{};
    }

    requester::Coroutine setValueAsync(const AsyncSetOperationValueType& value,
                                       AsyncOperationStatusType* status,
                                       std::shared_ptr<NsmDevice> device)
    {
        co_return co_await prebootdiag::handleConfigSet(
            value, status, std::move(device), &prebootdiag::sendTidConfigs);
    }
};

} // namespace nsm

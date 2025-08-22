/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "globals.hpp"
#include "nsmDevice.hpp"
#include "nsmInterface.hpp"

#include <com/nvidia/ErrorInjection/ErrorInjection/server.hpp>
#include <com/nvidia/ErrorInjection/ErrorInjectionCapability/server.hpp>
#include <com/nvidia/ErrorInjection/ErrorInjectionPayload/server.hpp>

namespace nsm
{
using ErrorInjectionIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::ErrorInjection::server::ErrorInjection>;
using ErrorInjectionCapabilityIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::ErrorInjection::server::ErrorInjectionCapability>;
using ErrorInjectionPayloadIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::ErrorInjection::server::ErrorInjectionPayload>;

class NsmErrorInjection :
    public NsmSensor,
    public NsmInterfaceContainer<ErrorInjectionIntf>
{
  public:
    NsmErrorInjection(const NsmInterfaceProvider<ErrorInjectionIntf>& provider);
    NsmErrorInjection() = delete;

    std::optional<Request> genRequestMsg(eid_t eid,
                                         uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
};

class NsmErrorInjectionSupported :
    public NsmSensor,
    public NsmInterfaceContainer<ErrorInjectionCapabilityIntf>
{
  public:
    NsmErrorInjectionSupported(
        const NsmInterfaceProvider<ErrorInjectionCapabilityIntf>& provider);
    NsmErrorInjectionSupported() = delete;

    std::optional<Request> genRequestMsg(eid_t eid,
                                         uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
};

class NsmErrorInjectionEnabled : public NsmErrorInjectionSupported
{
  public:
    using NsmErrorInjectionSupported::NsmErrorInjectionSupported;

    std::optional<Request> genRequestMsg(eid_t eid,
                                         uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
};

class NsmErrorInjectionPayload :
    public NsmSensor,
    public NsmInterfaceContainer<ErrorInjectionPayloadIntf>
{
  public:
    NsmErrorInjectionPayload(
        const NsmInterfaceProvider<ErrorInjectionPayloadIntf>& provider,
        uint32_t errorInjectionId);
    NsmErrorInjectionPayload() = delete;
    std::optional<Request> genRequestMsg(eid_t eid,
                                         uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    uint32_t errorInjectionId;
};

} // namespace nsm

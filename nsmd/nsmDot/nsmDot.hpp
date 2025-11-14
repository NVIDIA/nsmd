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

#pragma once

#include "asyncOperationManager.hpp"
#include "nsmObjectFactory.hpp"
#include "types.hpp"

#include <com/nvidia/Dot/Action/server.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace nsm
{
using DotActionIntf =
    sdbusplus::server::object_t<sdbusplus::server::com::nvidia::dot::Action>;

/**
 * @brief Object that provides DOT (Device Ownership Transfer) functionality.
 *
 * This class implements DOT functionality by inheriting from
 * com.nvidia.Dot.Action interface. It provides methods to install CAK/LAK keys
 * and bypass DOT authentication. The class handles asynchronous operations for
 * DOT management.
 */
class NsmDotObject : public NsmObject, public DotActionIntf
{
  public:
    /**
     * @brief Constructs a new NsmDotObject
     *
     * @param bus D-Bus bus interface for communication
     * @param name Object name for the DOT object instance
     * @param uuid Device UUID associated with this DOT object
     */
    NsmDotObject(sdbusplus::bus::bus& bus, const std::string& name,
                 const uuid_t& uuid);

    /**
     * @brief Install DOT CAK (Code Authentication Key)
     *
     * Initiates an asynchronous operation to install CAK and LAK keys.
     * The operation returns immediately with an object path for monitoring
     * status.
     *
     * @param cakKeyAuthScheme CAK key authentication scheme (0: ECDSA, 1:
     * Hybrid)
     * @param cakEcdsaKey CAK ECDSA key data (base64 or hex string)
     * @param cakLmsKey CAK LMS key data (base64 or hex string, can be empty for
     * ECDSA-only)
     * @param lakKeyAuthScheme LAK key authentication scheme (0: ECDSA, 1:
     * Hybrid)
     * @param lakEcdsaKey LAK ECDSA key data (base64 or hex string)
     * @param lakLmsKey LAK LMS key data (base64 or hex string, can be empty for
     * ECDSA-only)
     * @param lockDisable Lock disable flag (false: lock enabled, true: lock
     * disabled)
     * @param minSvn Minimum Security Version Number
     * @return Object path for monitoring the async operation status
     * @throws Common::Error::InvalidArgument if key data is invalid
     * @throws Common::Error::InternalFailure if encoding fails
     * @throws Common::Error::Unavailable if async operation manager is
     * unavailable
     */
    sdbusplus::message::object_path
        dotCAKInstall(DotActionIntf::KeyAuthScheme cakKeyAuthScheme,
                      std::string cakEcdsaKey, std::string cakLmsKey,
                      DotActionIntf::KeyAuthScheme lakKeyAuthScheme,
                      std::string lakEcdsaKey, std::string lakLmsKey,
                      bool lockDisable, uint32_t minSvn) override;

    /**
     * @brief Bypass DOT CAK installation
     *
     * Initiates an asynchronous operation to bypass DOT authentication.
     * The operation returns immediately with an object path for monitoring
     * status.
     *
     * @return Object path for monitoring the async operation status
     * @throws Common::Error::InternalFailure if encoding fails
     * @throws Common::Error::Unavailable if async operation manager is
     * unavailable
     */
    sdbusplus::message::object_path bypass() override;

  private:
    /**
     * @brief Async handler for DOT CAK Install operation
     *
     * @param cakKeyAuthScheme CAK authentication scheme
     * @param cakEcdsaKey CAK ECDSA key (base64/hex string)
     * @param cakLmsKey CAK LMS key (base64/hex string)
     * @param lakKeyAuthScheme LAK authentication scheme
     * @param lakEcdsaKey LAK ECDSA key (base64/hex string)
     * @param lakLmsKey LAK LMS key (base64/hex string)
     * @param lockDisable Lock disable flag
     * @param minSvn Minimum SVN
     * @param statusIntf Async status interface
     * @param valueIntf Async value interface
     * @return Coroutine result code
     */
    requester::Coroutine dotCAKInstallAsyncHandler(
        DotActionIntf::KeyAuthScheme cakKeyAuthScheme, std::string cakEcdsaKey,
        std::string cakLmsKey, DotActionIntf::KeyAuthScheme lakKeyAuthScheme,
        std::string lakEcdsaKey, std::string lakLmsKey, bool lockDisable,
        uint32_t minSvn, std::shared_ptr<AsyncStatusIntf> statusIntf,
        std::shared_ptr<AsyncValueIntf> valueIntf);

    /**
     * @brief Async handler for DOT CAK Bypass operation
     *
     * @param statusIntf Async status interface
     * @param valueIntf Async value interface
     * @return Coroutine result code
     */
    requester::Coroutine
        bypassAsyncHandler(std::shared_ptr<AsyncStatusIntf> statusIntf,
                           std::shared_ptr<AsyncValueIntf> valueIntf);

    /**
     * @brief Handle send errors for DOT operations
     *
     * @param sendRc Send return code
     * @param eid Endpoint ID
     * @param statusIntf Async status interface
     * @param valueIntf Async value interface
     */
    void handleSendError(int sendRc, int eid,
                         std::shared_ptr<AsyncStatusIntf> statusIntf,
                         std::shared_ptr<AsyncValueIntf> valueIntf);

    /** Device UUID */
    uuid_t uuid;

    /** Object path base for DOT operations */
    inline static const std::filesystem::path dotObjectBasePath{
        "/xyz/openbmc_project/DOT"};
};

} // namespace nsm

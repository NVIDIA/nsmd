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
#include "nsmSensor.hpp"
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

class NsmDotStatusObject;

/**
 * @brief Parameters for DOT Lock operation
 */
struct DotLockParams
{
    DotActionIntf::KeyAuthScheme cakKeyAuthScheme;
    std::string cakEcdsaKey;
    std::string cakLmsKey;
    DotActionIntf::KeyAuthScheme lakKeyAuthScheme;
    std::string lakEcdsaKey;
    std::string lakLmsKey;
    DotActionIntf::UnlockMethod unlockMethod;
    std::string staticChallenge;
    DotActionIntf::KeyAuthScheme lockSignatureAuthScheme;
    std::string ecdsaSignature;
    std::string lmsSignature;
};

/**
 * @brief Parameters for DOT Disable operation
 */
struct DotDisableParams
{
    DotActionIntf::KeyAuthScheme lakKeyAuthScheme;
    std::string lakEcdsaKey;
    std::string lakLmsKey;
    DotActionIntf::UnlockMethod unlockMethod;
    std::string staticChallenge;
    DotActionIntf::KeyAuthScheme disableSignatureAuthScheme;
    std::string ecdsaSignature;
    std::string lmsSignature;
};

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

    /**
     * @brief Lock DOT and transition from volatile to locked state
     *
     * @param cakKeyAuthScheme CAK key authentication scheme
     * @param cakEcdsaKey CAK ECDSA key data
     * @param cakLmsKey CAK LMS key data
     * @param lakKeyAuthScheme LAK key authentication scheme
     * @param lakEcdsaKey LAK ECDSA key data
     * @param lakLmsKey LAK LMS key data
     * @param unlockMethod Unlock method for lock operation
     * @param staticChallenge Static challenge (required when UnlockMethod is
     * StaticValue)
     * @param lockSignatureAuthScheme Signature authentication scheme
     * @param ecdsaSignature ECDSA signature
     * @param lmsSignature LMS signature
     * @return Object path for monitoring the async operation status
     * @throws Common::Error::InvalidArgument if parameters are invalid
     * @throws Common::Error::InternalFailure if encoding fails
     * @throws Common::Error::Unavailable if async operation manager is
     * unavailable
     */
    sdbusplus::message::object_path lock(
        DotActionIntf::KeyAuthScheme cakKeyAuthScheme, std::string cakEcdsaKey,
        std::string cakLmsKey, DotActionIntf::KeyAuthScheme lakKeyAuthScheme,
        std::string lakEcdsaKey, std::string lakLmsKey,
        DotActionIntf::UnlockMethod unlockMethod, std::string staticChallenge,
        DotActionIntf::KeyAuthScheme lockSignatureAuthScheme,
        std::string ecdsaSignature, std::string lmsSignature) override;

    /**
     * @brief Request unlock challenge from device
     *
     * Retrieves a 32-byte challenge for DOT unlock operation.
     *
     * @param unlockType Type of unlock (OwnerUnlock or VendorUnlock)
     * @return Object path for monitoring the async operation status
     */
    sdbusplus::message::object_path
        unlockChallenge(DotActionIntf::UnlockType unlockType) override;

    /**
     * @brief Unlock DOT device
     *
     * Initiates an asynchronous unlock operation with signature verification.
     * Total signature size is always 1840 bytes in the request.
     *
     * @param unlockSignatureAuthScheme Signature auth scheme (Ecdsa or Hybrid)
     * @param ecdsaSignature ECDSA signature data (base64 or hex):
     *        - ECDSA-only: 96 bytes or 1840 bytes (with 1744B padding)
     *        - Hybrid: 96 bytes only
     * @param lmsSignature LMS signature data (base64 or hex):
     *        - ECDSA-only: empty
     *        - Hybrid: 1744 bytes
     * @return Object path for monitoring the async operation status
     * @throws Common::Error::InvalidArgument if signature data is invalid
     * @throws Common::Error::Unavailable if async operation manager unavailable
     */
    sdbusplus::message::object_path
        unlock(DotActionIntf::KeyAuthScheme unlockSignatureAuthScheme,
               std::string ecdsaSignature, std::string lmsSignature) override;

    /**
     * @brief Get DOT information
     *
     * Retrieves DOT blob, version, fuse change state, and transfers remaining.
     *
     * @return Object path for monitoring the async operation status
     */
    sdbusplus::message::object_path getInfo() override;

    /**
     * @brief Rotate CAK (Code Authentication Key)
     *
     * Initiates an asynchronous operation to rotate the CAK key.
     * The operation is authenticated using a LAK signature.
     *
     * @param cakKeyAuthScheme New CAK key authentication scheme
     * @param cakEcdsaKey New CAK ECDSA key data (base64 or hex string)
     * @param cakLmsKey New CAK LMS key data (base64 or hex string)
     * @param cakRotateSignatureAuthScheme Signature authentication scheme
     * @param ecdsaSignature ECDSA signature data (base64 or hex string)
     * @param lmsSignature LMS signature data (base64 or hex string)
     * @return Object path for monitoring the async operation status
     * @throws Common::Error::InvalidArgument if key or signature data is
     * invalid
     * @throws Common::Error::InternalFailure if encoding fails
     * @throws Common::Error::Unavailable if async operation manager is
     * unavailable
     */
    sdbusplus::message::object_path
        cakRotate(DotActionIntf::KeyAuthScheme cakKeyAuthScheme,
                  std::string cakEcdsaKey, std::string cakLmsKey,
                  DotActionIntf::KeyAuthScheme cakRotateSignatureAuthScheme,
                  std::string ecdsaSignature,
                  std::string lmsSignature) override;

    /**
     * @brief Disable DOT and transition from uninitialized to disabled state
     *
     * @param lakKeyAuthScheme LAK key authentication scheme
     * @param lakEcdsaKey LAK ECDSA key data
     * @param lakLmsKey LAK LMS key data
     * @param unlockMethod Unlock method for disable operation
     * @param staticChallenge Static challenge (required when UnlockMethod is
     * StaticValue)
     * @param disableSignatureAuthScheme Signature authentication scheme
     * @param ecdsaSignature ECDSA signature
     * @param lmsSignature LMS signature
     * @return Object path for monitoring the async operation status
     * @throws Common::Error::InvalidArgument if parameters are invalid
     * @throws Common::Error::InternalFailure if encoding fails
     * @throws Common::Error::Unavailable if async operation manager is
     * unavailable
     */
    sdbusplus::message::object_path
        disable(DotActionIntf::KeyAuthScheme lakKeyAuthScheme,
                std::string lakEcdsaKey, std::string lakLmsKey,
                DotActionIntf::UnlockMethod unlockMethod,
                std::string staticChallenge,
                DotActionIntf::KeyAuthScheme disableSignatureAuthScheme,
                std::string ecdsaSignature, std::string lmsSignature) override;

    /**
     * @brief Override DOT to reset ownership state
     *
     * Override DOT when valid DOT data can no longer be recovered. Requires
     * vendor signature signed by Nvidia vendor key. UnlockChallenge with
     * VendorUnlock type should be used prior to Override.
     *
     * @param vendorSignatureAuthScheme Vendor signature authentication scheme
     * @param ecdsaSignature ECDSA signature with challenge from
     * UnlockChallenge
     * @param lmsSignature LMS signature with challenge from UnlockChallenge
     * @return Object path for monitoring the async operation status
     * @throws Common::Error::InvalidArgument if signature data is invalid
     * @throws Common::Error::InternalFailure if encoding fails
     * @throws Common::Error::Unavailable if async operation manager is
     * unavailable
     */
    sdbusplus::message::object_path
        override(DotActionIntf::KeyAuthScheme vendorSignatureAuthScheme,
                 std::string ecdsaSignature, std::string lmsSignature) override;

    /**
     * @brief Recover corrupted DOT data using backup data
     *
     * Recover corrupted DOT data using backup data. Unlike Override, this
     * method does not require vendor signature. UnlockChallenge should be used
     * prior to Recovery.
     *
     * @param dotData File descriptor containing backup DOT data
     * @return Object path for monitoring the async operation status
     * @throws Common::Error::InvalidArgument if DOT data is invalid
     * @throws Common::Error::InternalFailure if encoding fails
     * @throws Common::Error::Unavailable if async operation manager is
     * unavailable
     */
    sdbusplus::message::object_path
        recoverDOT(sdbusplus::message::unix_fd dotData) override;

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
     * @brief Async handler for DOT Lock operation
     *
     * @param params Lock operation parameters
     * @param statusIntf Async status interface
     * @param valueIntf Async value interface
     * @return Coroutine result code
     */
    requester::Coroutine
        lockAsyncHandler(const DotLockParams& params,
                         std::shared_ptr<AsyncStatusIntf> statusIntf,
                         std::shared_ptr<AsyncValueIntf> valueIntf);

    /**
     * @brief Async handler for DOT Unlock Challenge operation
     *
     * @param unlockType Type of unlock (Owner or Vendor)
     * @param statusIntf Async status interface
     * @param valueIntf Async value interface
     * @return Coroutine result code
     */
    requester::Coroutine
        unlockChallengeAsyncHandler(DotActionIntf::UnlockType unlockType,
                                    std::shared_ptr<AsyncStatusIntf> statusIntf,
                                    std::shared_ptr<AsyncValueIntf> valueIntf);

    /**
     * @brief Async handler for DOT Unlock operation
     *
     * @param unlockSignatureAuthScheme Signature authentication scheme
     * @param ecdsaSignature ECDSA signature
     * @param lmsSignature LMS signature
     * @param statusIntf Async status interface
     * @param valueIntf Async value interface
     * @return Coroutine result code
     */
    requester::Coroutine unlockAsyncHandler(
        DotActionIntf::KeyAuthScheme unlockSignatureAuthScheme,
        std::string ecdsaSignature, std::string lmsSignature,
        std::shared_ptr<AsyncStatusIntf> statusIntf,
        std::shared_ptr<AsyncValueIntf> valueIntf);

    /**
     * @brief Async handler for DOT Get Info operation
     *
     * @param statusIntf Async status interface
     * @param valueIntf Async value interface
     * @return Coroutine result code
     */
    requester::Coroutine
        getInfoAsyncHandler(std::shared_ptr<AsyncStatusIntf> statusIntf,
                            std::shared_ptr<AsyncValueIntf> valueIntf);

    /**
     * @brief Async handler for DOT CAK Rotate operation
     *
     * @param cakKeyAuthScheme New CAK key authentication scheme
     * @param cakEcdsaKey New CAK ECDSA key data (base64/hex string)
     * @param cakLmsKey New CAK LMS key data (base64/hex string)
     * @param cakRotateSignatureAuthScheme Signature authentication scheme
     * @param ecdsaSignature ECDSA signature data (base64/hex string)
     * @param lmsSignature LMS signature data (base64/hex string)
     * @param statusIntf Async status interface
     * @param valueIntf Async value interface
     * @return Coroutine result code
     */
    requester::Coroutine cakRotateAsyncHandler(
        DotActionIntf::KeyAuthScheme cakKeyAuthScheme, std::string cakEcdsaKey,
        std::string cakLmsKey,
        DotActionIntf::KeyAuthScheme cakRotateSignatureAuthScheme,
        std::string ecdsaSignature, std::string lmsSignature,
        std::shared_ptr<AsyncStatusIntf> statusIntf,
        std::shared_ptr<AsyncValueIntf> valueIntf);

    /**
     * @brief Async handler for DOT Disable operation
     *
     * @param params Disable operation parameters
     * @param statusIntf Async status interface
     * @param valueIntf Async value interface
     * @return Coroutine result code
     */
    requester::Coroutine
        disableAsyncHandler(const DotDisableParams& params,
                            std::shared_ptr<AsyncStatusIntf> statusIntf,
                            std::shared_ptr<AsyncValueIntf> valueIntf);

    /**
     * @brief Async handler for DOT Override operation
     *
     * @param vendorSignatureAuthScheme Vendor signature authentication scheme
     * @param ecdsaSignature ECDSA signature
     * @param lmsSignature LMS signature
     * @param statusIntf Async status interface
     * @param valueIntf Async value interface
     * @return Coroutine result code
     */
    requester::Coroutine overrideAsyncHandler(
        DotActionIntf::KeyAuthScheme vendorSignatureAuthScheme,
        std::string ecdsaSignature, std::string lmsSignature,
        std::shared_ptr<AsyncStatusIntf> statusIntf,
        std::shared_ptr<AsyncValueIntf> valueIntf);

    /**
     * @brief Async handler for DOT Recovery operation
     *
     * @param dotData File descriptor containing backup DOT blob
     * @param statusIntf Async status interface
     * @param valueIntf Async value interface
     * @return Coroutine result code
     */
    requester::Coroutine
        recoverDOTAsyncHandler(sdbusplus::message::unix_fd dotData,
                               std::shared_ptr<AsyncStatusIntf> statusIntf,
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

    /**
     * @brief Build and validate CAK key authentication data
     *
     * @param cakKeyAuthScheme CAK authentication scheme
     * @param cakEcdsaKey CAK ECDSA key string
     * @param cakLmsKey CAK LMS key string
     * @param output Output buffer for key auth data (148 bytes)
     * @param statusIntf Async status interface for error reporting
     * @param valueIntf Async value interface for error reporting
     * @return true if successful, false on error
     */
    bool buildAndValidateCAKAuthData(
        DotActionIntf::KeyAuthScheme cakKeyAuthScheme,
        const std::string& cakEcdsaKey, const std::string& cakLmsKey,
        uint8_t* output, std::shared_ptr<AsyncStatusIntf> statusIntf,
        std::shared_ptr<AsyncValueIntf> valueIntf);

    /**
     * @brief Build and validate LAK key authentication data
     *
     * @param lakKeyAuthScheme LAK authentication scheme
     * @param lakEcdsaKey LAK ECDSA key string
     * @param lakLmsKey LAK LMS key string
     * @param output Output buffer for key auth data (148 bytes)
     * @param statusIntf Async status interface for error reporting
     * @param valueIntf Async value interface for error reporting
     * @return true if successful, false on error
     */
    bool buildAndValidateLAKAuthData(
        DotActionIntf::KeyAuthScheme lakKeyAuthScheme,
        const std::string& lakEcdsaKey, const std::string& lakLmsKey,
        uint8_t* output, std::shared_ptr<AsyncStatusIntf> statusIntf,
        std::shared_ptr<AsyncValueIntf> valueIntf);

    /**
     * @brief Build and validate signature data
     *
     * @param signatureAuthScheme Signature authentication scheme
     * @param ecdsaSignature ECDSA signature string
     * @param lmsSignature LMS signature string
     * @param output Output buffer for signature (1840 bytes)
     * @param statusIntf Async status interface for error reporting
     * @param valueIntf Async value interface for error reporting
     * @return true if successful, false on error
     */
    bool buildAndValidateSignatureData(
        DotActionIntf::KeyAuthScheme signatureAuthScheme,
        const std::string& ecdsaSignature, const std::string& lmsSignature,
        uint8_t* output, std::shared_ptr<AsyncStatusIntf> statusIntf,
        std::shared_ptr<AsyncValueIntf> valueIntf);

    /** Device UUID */
    uuid_t uuid;

  public:
    /** Pointer to DOT status sensor for manual triggering */
    NsmDotStatusObject* dotStatusSensor_{nullptr};

    /** Object path base for DOT operations */
    inline static const std::filesystem::path dotObjectBasePath{
        "/xyz/openbmc_project/DOT"};
};

/**
 * @brief Sensor object for DOT Status polling
 *
 * This class implements periodic polling of DOT status by extending NsmSensor.
 * It follows the NSMD infrastructure pattern used by other sensors like
 * GetRoTInformation, QueryCodeAuth, and QuerySecurityVersion.
 */
class NsmDotStatusObject : public NsmSensor
{
  public:
    /**
     * @brief Constructs a new NsmDotStatusObject
     *
     * @param name Object name for the DOT status sensor
     * @param type Sensor type
     * @param dotActionIntf Pointer to DOT action interface for updating state
     * @param uuid Device UUID for this sensor
     */
    NsmDotStatusObject(const std::string& name, const std::string& type,
                       DotActionIntf* dotActionIntf, const uuid_t& uuid);

    /**
     * @brief Generate DOT status request message
     *
     * @param eid Endpoint ID
     * @param instanceId Instance ID for the request
     * @return Optional vector containing the request message
     */
    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

    /**
     * @brief Handle DOT status response message
     *
     * @param responseMsg Response message to handle
     * @param responseLen Length of response message
     * @return Completion code
     */
    uint8_t handleResponseMsg(const nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    /** Pointer to DOT action interface */
    DotActionIntf* dotActionIntf_;

    /** Device UUID */
    uuid_t uuid_;
};

} // namespace nsm

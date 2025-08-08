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

#include "common/types.hpp"
#include "nsmd/socket_handler.hpp"

#include <sdbusplus/bus/match.hpp>

#include <filesystem>
#include <initializer_list>
#include <vector>

namespace mctp
{

/** @class MctpDiscoveryHandlerIntf
 *
 * This abstract class defines the APIs for MctpDiscovery class has common
 * interface to execute function from different Manager Classes
 */
class MctpDiscoveryHandlerIntf
{
  public:
    virtual void handleMctpEndpoints(const MctpInfos& mctpInfos) = 0;
    virtual requester::Coroutine
        onlineMctpEndpoint([[maybe_unused]] const MctpInfo& mctpInfo)
    {
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }
    virtual requester::Coroutine
        offlineMctpEndpoint([[maybe_unused]] const MctpInfo& mctpInfo)
    {
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }
    virtual void handleMctpStateTransition(const std::string objPath,
                                           const bool state) = 0;
    virtual ~MctpDiscoveryHandlerIntf() {}
};

class MctpDiscovery
{
  public:
    MctpDiscovery() = delete;
    MctpDiscovery(const MctpDiscovery&) = delete;
    MctpDiscovery(MctpDiscovery&&) = delete;
    MctpDiscovery& operator=(const MctpDiscovery&) = delete;
    MctpDiscovery& operator=(MctpDiscovery&&) = delete;
    ~MctpDiscovery() = default;

    /** @brief Constructs the MCTP Discovery object to handle discovery of
     *         MCTP enabled devices
     *
     *  @param[in] bus - reference to systemd bus
     *  @param[in] list - initializer list to the MctpDiscoveryHandlerIntf
     */
    explicit MctpDiscovery(
        sdbusplus::bus::bus& bus, mctp_socket::Handler& handler,
        std::initializer_list<MctpDiscoveryHandlerIntf*> list);

  private:
    /** @brief reference to the systemd bus */
    sdbusplus::bus::bus& bus;
    mctp_socket::Handler& handler;

    /** @brief Used to watch for new MCTP endpoints */
    sdbusplus::bus::match_t mctpEndpointAddedSignal;

    /** @brief Used to watch for the removed MCTP endpoints */
    sdbusplus::bus::match_t mctpEndpointRemovedSignal;

    /** @brief map of Queue to store the pending property change signal from
     * mctp service */
    std::map<std::string, std::queue<sdbusplus::message::message>>
        mctpQueuedSignals;

    std::map<std::string, std::coroutine_handle<>> deviceStateChangeTaskHandles;
    requester::Coroutine deviceStateChangeTask(const std::string path);

    void discoverEndpoints(sdbusplus::message::message& msg);

    /**
     * @brief matcher rule for property changes of
     * xyz.openbmc_project.Object.Enable dbus object
     */
    std::map<std::string, sdbusplus::bus::match_t> enableMatches;

    /** @brief handler for mctpEndpointRemovedSignal */
    void cleanEndpoints(sdbusplus::message::message& msg);

    /**
     * @brief A callback for propertiesChanges signal enabled matches matcher
     * rule to invoke registered handlers (online/offline mctp endpoint)
     */
    void refreshEndpoints(sdbusplus::message::message& msg);

    /** @brief Process the D-Bus MCTP endpoint info and prepare data to be used
     *         for NSM discovery.
     *
     *  @param[in] interfaces - MCTP D-Bus information
     *  @param[out] mctpInfos - MCTP info for NSM discovery
     */
    void populateMctpInfo(const dbus::InterfaceMap& interfaces,
                          MctpInfos& mctpInfos);

    static constexpr uint8_t mctpTypeVDM = 0x7e;

    /** @brief MCTP endpoint interface name */
    const std::string mctpEndpointIntfName{"xyz.openbmc_project.MCTP.Endpoint"};

    const std::string mctpBindingIntfName{"xyz.openbmc_project.MCTP.Binding"};

    /** @brief UUID interface name */
    static constexpr std::string_view uuidEndpointIntfName{
        "xyz.openbmc_project.Common.UUID"};

    /** @brief Unix Socket interface name */
    static constexpr std::string_view unixSocketIntfName{
        "xyz.openbmc_project.Common.UnixSocket"};

    static constexpr std::string_view codeConstructEndpointIntfName{
        "au.com.codeconstruct.MCTP.Endpoint1"};

    std::vector<MctpDiscoveryHandlerIntf*> handlers;

    /** @brief Helper function to invoke registered handlers
     *
     *  @param[in] mctpInfos - information of discovered MCTP endpoints
     */
    void handleMctpEndpoints(const MctpInfos& mctpInfos);
};

} // namespace mctp

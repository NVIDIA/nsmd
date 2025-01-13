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

#include "utils.hpp"

#include <err.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

namespace nsmtool
{

namespace helper
{

constexpr uint8_t NSM_ENTITY_ID = 8;
using ordered_json = nlohmann::ordered_json;

/** @brief print the input message if verbose is enabled
 *
 *  @param[in]  verbose - verbosity flag - true/false
 *  @param[in]  msg         - message to print
 *  @param[in]  data        - data to print
 *
 *  @return - None
 */

template <class T>
void Logger(bool verbose, const char* msg, const T& data)
{
    if (verbose)
    {
        std::stringstream s;
        s << data;
        std::cout << msg << s.str() << std::endl;
    }
}

/** @brief Convert byte input into a hexadecimal string.
 *
 *  @param[in]  data - binary data to be converted
 *  @param[in]  len - length of data to be converted
 *
 *  @return - Hexadecimal string representation of the input data.
 */
static inline std::string bytesToHexString(const uint8_t* data, size_t len)
{
    std::stringstream ss;
    ss << std::hex;

    for (size_t i = 0; i < len; ++i)
    {
        ss << std::setw(2) << std::setfill('0') << (int)data[i];
    }

    return ss.str();
}

/** @brief Display in JSON format.
 *
 *  @param[in]  data - data to print in json
 *
 *  @return - None
 */
static inline void DisplayInJson(const ordered_json& data)
{
    std::cout << data.dump(4) << std::endl;
}

/** @brief MCTP socket read/recieve
 *
 *  @param[in]  requestMsg - Request message to compare against loopback
 *              message recieved from mctp socket
 *  @param[out] responseMsg - Response buffer recieved from mctp socket
 *  @param[in]  verbose - verbosity flag - true/false
 *
 *  @return -   0 on success.
 *             -1 or -errno on failure.
 */
int mctpSockSendRecv(const std::vector<uint8_t>& requestMsg,
                     std::vector<uint8_t>& responseMsg, bool verbose);

/** @brief bitfield variable parser
 *
 *  @param[out]  res - result variable in which parsed values will be kept
 *  @param[in]  key - key against which the value is to kept in ordered json
 *  @param[in]  value - the bitfield array value
 *  @param[in]  size - size of bitfield array
 *
 *  @return - None
 */
void parseBitfieldVar(ordered_json& res, const std::string& key,
                      const bitfield8_t* value, uint8_t size);

class CommandInterface
{
  public:
    explicit CommandInterface(const char* type, const char* name,
                              CLI::App* app) :
        nsmType(type),
        commandName(name), mctpEid(NSM_ENTITY_ID), verbose(false), instanceId(0)
    {
        app->add_option("-m,--mctp_eid", mctpEid, "MCTP endpoint ID");
        app->add_flag("-v, --verbose", verbose);
        app->callback([&]() { exec(); });
    }

    virtual ~CommandInterface() = default;

    virtual std::pair<int, std::vector<uint8_t>> createRequestMsg() = 0;

    virtual void parseResponseMsg(struct nsm_msg* responsePtr,
                                  size_t payloadLength) = 0;

    virtual void exec();

    int nsmSendRecv(std::vector<uint8_t>& requestMsg,
                    std::vector<uint8_t>& responseMsg);

    /**
     * @brief get MCTP endpoint ID
     *
     * @return uint8_t - MCTP endpoint ID
     */
    inline uint8_t getMCTPEID()
    {
        return mctpEid;
    }

  private:
    /** @brief Get MCTP demux daemon socket address
     *
     *  getMctpSockAddr does a D-Bus lookup for MCTP remote endpoint and return
     *  the unix socket info to be used for Tx/Rx
     *
     *  @param[in]  eid - Request MCTP endpoint
     *
     *  @return On success return the type, protocol and unit socket address, on
     *          failure the address will be empty
     */
    std::tuple<int, int, std::vector<uint8_t>>
        getMctpSockInfo(uint8_t remoteEID);
    const std::string nsmType;
    const std::string commandName;
    uint8_t mctpEid;
    bool verbose;

  protected:
    uint8_t instanceId;
};

} // namespace helper
} // namespace nsmtool

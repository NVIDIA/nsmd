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

class CommandInterface
{

  public:
    explicit CommandInterface(const char* type, const char* name,
                              CLI::App* app) :
        nsmType(type),
        commandName(name), mctp_eid(NSM_ENTITY_ID), verbose(false),
        instanceId(0)
    {
        app->add_option("-m,--mctp_eid", mctp_eid, "MCTP endpoint ID");
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
        return mctp_eid;
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
    uint8_t mctp_eid;
    bool verbose;

  protected:
    uint8_t instanceId;
};

} // namespace helper
} // namespace nsmtool

#pragma once

#include "eventManager.hpp"
#include "requester/handler.hpp"
#include "socket_manager.hpp"
#include "types.hpp"

#include <sdeventplus/event.hpp>
#include <sdeventplus/source/io.hpp>

#include <map>
#include <optional>
#include <unordered_map>

namespace mctp_socket
{

using PathName = std::string;

using namespace sdeventplus;
using namespace sdeventplus::source;

/** @class Handler
 *
 *  The Handler class abstracts the communication with multiple MCTP Tx/Rx
 *  daemons which supports different transport mechanisms. The initialisation of
 *  this class is driven by the discovery of MCTP.Endpoint interface which
 *  exposes the socket information to communicate with the endpoints.  This
 *  manager class handles the data to be read on the communication sockets by
 *  registering callbacks for EPOLLIN.
 */
class Handler
{
  public:
    Handler() = delete;
    Handler(const Handler&) = delete;
    Handler(Handler&&) = default;
    Handler& operator=(const Handler&) = delete;
    Handler& operator=(Handler&&) = default;
    virtual ~Handler() = default;

    const uint8_t MCTP_MSG_TYPE_VDM = 0x7e;

    /** @brief Constructor
     *
     *  @param[in] event - NSM daemon's main event loop
     *  @param[in] handler - NSM request handler
     *  @param[in] eventManager - NSM event Manager
     *  @param[in] verbose - Verbose tracing flag
     *  @param[in/out] manager - MCTP socket manager
     */
    explicit Handler(sdeventplus::Event& event,
                     requester::Handler<requester::Request>& handler,
                     nsm::EventManager& eventManager, Manager& manager,
                     bool verbose) :
        event(event),
        handler(handler), eventManager(eventManager), manager(manager),
        verbose(verbose)
    {}

    int registerMctpEndpoint(eid_t eid, int type, int protocol,
                             const std::vector<uint8_t>& pathName);

  private:
    sdeventplus::Event& event;
    requester::Handler<requester::Request>& handler;
    nsm::EventManager& eventManager;
    Manager& manager;
    bool verbose;

    SocketInfo initSocket(int type, int protocol,
                          const std::vector<uint8_t>& pathName);

    std::optional<Response>
        processRxMsg(const std::vector<uint8_t>& requestMsg);

    /** @brief Socket information for MCTP Tx/Rx daemons */
    std::map<std::vector<uint8_t>,
             std::tuple<std::unique_ptr<utils::CustomFD>, SendBufferSize,
                        std::unique_ptr<IO>>>
        socketInfoMap;
};

} // namespace mctp_socket
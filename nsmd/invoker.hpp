#pragma once

#include "common/types.hpp"
#include "responder_handler.hpp"

#include <map>
#include <memory>

namespace responder
{

/** @class Invoker
 *
 *  The handler class provides API to register NSM command handler to
 *  respond the nsm request message from endpoint.
 */
class Invoker
{
  public:
    /** @brief Register a handler for a NSM Type
     *
     *  @param[in] type - NSM type code
     *  @param[in] handler - NSM Type handler
     */
    void registerHandler(NsmType type, std::unique_ptr<CmdHandler> handler)
    {
        handlers.emplace(type, std::move(handler));
    }

    /** @brief Invoke a NSM command handler
     *
     *  @param[in] type - NSM type code
     *  @param[in] command - NSM command code
     *  @param[in] request - NSM request message
     *  @param[in] reqMsgLen - NSM request message size
     *  @return NSM response message
     */
    Response handle(NsmType Type, Command Command, const nsm_msg* request,
                    size_t reqMsgLen)
    {
        return handlers.at(Type)->handle(Command, request, reqMsgLen);
    }

  private:
    std::map<NsmType, std::unique_ptr<CmdHandler>> handlers;
};

} // namespace responder

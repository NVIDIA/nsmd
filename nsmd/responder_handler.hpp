#pragma once

#include "libnsm/base.h"

#include <cassert>
#include <functional>
#include <map>
#include <vector>

using NsmCommand = uint8_t;

namespace responder
{

using Response = std::vector<uint8_t>;
class CmdHandler;
using HandlerFunc =
    std::function<Response(const nsm_msg* request, size_t reqMsgLen)>;

class CmdHandler
{
  public:
    virtual ~CmdHandler() = default;
    /** @brief Invoke a NSM command handler
     *
     *  @param[in] command - command code
     *  @param[in] request - request message
     *  @param[in] reqMsgLen - request message size
     *  @return NSM response message
     */
    Response handle(NsmCommand Command, const nsm_msg* request,
                    size_t reqMsgLen)
    {
        return handlers.at(Command)(request, reqMsgLen);
    }

    /** @brief Create a response message containing only cc
     *
     *  @param[in] request - NSM request message
     *  @param[in] cc - Completion Code
     *  @return NSM response message
     */
    std::vector<uint8_t> ccOnlyResponse(const nsm_msg* request, uint8_t cc);

  protected:
    /** @brief map of NSM command code to handler - to be populated by derived
     *         classes.
     */
    std::map<NsmCommand, HandlerFunc> handlers;
};

} // namespace responder

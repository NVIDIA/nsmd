#pragma once

#include "common/types.hpp"
#include "eventHandler.hpp"

#include <phosphor-logging/lg2.hpp>

#include <map>
#include <memory>

namespace nsm
{

/** @class Event Manager
 *
 *  The Event Manager class provides API to register NSM event handler to
 *  acknowledge the NSM event from endpoint.
 */
class EventManager
{
  public:
    /** @brief Register a event handler for a NSM Type
     *
     *  @param[in] type - NSM type code
     *  @param[in] handler - NSM Type handler
     */
    void registerHandler(NsmType nsmType, std::unique_ptr<EventHandler> handler)
    {
        evenTypeHandlers.emplace(nsmType, std::move(handler));
    }

    /** @brief Invoke a NSM command handler
     *
     *  @param[in] type - NSM type code
     *  @param[in] eventId - NSM command code
     *  @param[in] event - NSM event message
     *  @param[in] eventLen - NSM event size
     *  @return NSM response message
     */
    std::optional<Response> handle(eid_t eid, NsmType nsmType, NsmEventId eventId,
                                   const nsm_msg* eventMsg, size_t eventLen)
    {
        if (evenTypeHandlers.find(nsmType) == evenTypeHandlers.end())
        {
            // unsupported event type
            lg2::info(
                "No event Type{NSMTYPE} handler found for received NSM event Type={NSMTYPE} ID={EVENTID} from EID={EID}.",
                "NSMTYPE", nsmType, "EVENTID", eventId, "EID", eid);
            return std::nullopt;
        }

        evenTypeHandlers.at(nsmType)->handle(eid, eventId, eventMsg, eventLen);

        struct nsm_event* event = (struct nsm_event*)eventMsg->payload;
        if (event->ackr)
        {
            return ackEvent(eventMsg->hdr.instance_id,
                            eventMsg->hdr.nvidia_msg_type, eventId);
        }
        return std::nullopt;
    }

    /** @brief Create a event ack message
     *
     *  @param[in] eventId - NSM Event ID
     *  @param[in] instanceId - NSM Instance ID
     *  @return NSM response message
     */
    std::optional<std::vector<uint8_t>>
        ackEvent(uint8_t instanceId, uint8_t nsmType, uint8_t eventId)

    {
        std::vector<uint8_t> ackMsg(sizeof(nsm_msg_hdr) +
                                    sizeof(nsm_event_ack));
        auto msg = reinterpret_cast<nsm_msg*>(ackMsg.data());
        auto rc =
            encode_nsm_event_acknowledgement(instanceId, nsmType, eventId, msg);
        if (rc != NSM_SUCCESS)
        {
            lg2::error(
                "encode_nsm_event_acknowledgement failed, rc={RC} isntanceId={INSTANCEID} NSM Type={NSMTYPE} EventId={EVENTID}",
                "RC", rc, "INSTANCEID", instanceId, "NSMTYPE", nsmType,
                "EVENTID", eventId);
            return std::nullopt;
        }

        return ackMsg;
    }

  private:
      /** @brief map of NSM type to event handler
     */
    std::map<NsmType, std::unique_ptr<EventHandler>> evenTypeHandlers;
};

} // namespace nsm

#pragma once

#include "nsmObject.hpp"

namespace nsm
{
class NsmEventConfig : public NsmObject
{
  public:
    NsmEventConfig(const std::string& name,
                   const std::string& type, uint8_t messageType,
                   std::vector<uint64_t>& srcEventIds,
                   std::vector<uint64_t>& ackEventIds);

    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  private:
    void convertIdsToMask(std::vector<uint64_t>& eventIds,
                          std::vector<bitfield8_t>& bitfields);
    requester::Coroutine
        setCurrentEventSources(SensorManager& manager, eid_t eid,
                               uint8_t nvidiaMessageType,
                               std::vector<bitfield8_t>& eventIdMasks);
    requester::Coroutine
        configureEventAcknowledgement(SensorManager& manager, eid_t eid,
                                      uint8_t nvidiaMessageType,
                                      std::vector<bitfield8_t>& eventIdMasks);

    uint8_t messageType;
    std::vector<bitfield8_t> srcEventMask;
    std::vector<bitfield8_t> ackEventMask;
};

} // namespace nsm

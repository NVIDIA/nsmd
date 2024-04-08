#pragma once

#include "nsmDevice.hpp"
#include "nsmObject.hpp"

namespace nsm
{
class NsmEventSetting : public NsmObject
{
  public:
    NsmEventSetting(const std::string& name, const std::string& type,
                    uint8_t eventGenerationSetting,
                    std::shared_ptr<NsmDevice> nsmDevice);

    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

    uint8_t getEventGenerationSetting()
    {
        return eventGenerationSetting;
    }

  private:
    requester::Coroutine setEventSubscription(SensorManager& manager, eid_t eid,
                                              uint8_t globalSettting,
                                              eid_t receiverEid);
    uint8_t eventGenerationSetting;
    std::shared_ptr<NsmDevice> nsmDevice;
};

} // namespace nsm

#include "nsmDevice.hpp"

#include "nsmNumericSensor/nsmNumericAggregator.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

std::shared_ptr<NsmNumericAggregator>
    NsmDevice::findAggregatorByType([[maybe_unused]] const std::string& type)
{
    std::shared_ptr<NsmNumericAggregator> aggregator{};

    auto itr =
        std::find_if(sensorAggregators.begin(), sensorAggregators.end(),
                     [&](const auto& e) { return e->getType() == type; });
    if (itr != sensorAggregators.end())
    {
        aggregator = *itr;
    }

    return aggregator;
}

std::shared_ptr<NsmDevice> findNsmDeviceByUUID(NsmDeviceTable& nsmDevices,
                                               const uuid_t& uuid)
{
    std::shared_ptr<NsmDevice> ret{};

    for (auto nsmDevice : nsmDevices)
    {
        if ((nsmDevice->uuid).substr(0, UUID_LEN) == uuid.substr(0, UUID_LEN))
        {
            ret = nsmDevice;
            break;
        }
    }
    return ret;
}

void NsmDevice::setEventMode(uint8_t mode)
{
    if (mode > GLOBAL_EVENT_GENERATION_ENABLE_PUSH)
    {
        lg2::error("event generation setting: invalid value={SETTING} provided",
                   "SETTING", mode);
        return;
    }
    eventMode = mode;
}

uint8_t NsmDevice::getEventMode()
{
    return eventMode;
}

} // namespace nsm

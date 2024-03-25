#include "nsmDevice.hpp"

namespace nsm
{

std::shared_ptr<NsmNumericAggregator> NsmDevice::findAggregatorByType([[maybe_unused]] std::string type)
{
    std::shared_ptr<NsmNumericAggregator> aggregator{};
    return aggregator;
}

std::shared_ptr<NsmDevice> findNsmDeviceByUUID(NsmDeviceTable& nsmDevices,
                                               uuid_t uuid)
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

}
#pragma once

#include "nsmObject.hpp"
#include "types.hpp"

struct nsm_msg;

namespace nsm
{
class NsmSensor : public NsmObject
{
  public:
    NsmSensor() = delete;
    NsmSensor(const std::string& name, const std::string& type) :
        NsmObject(name, type)
    {}

    virtual std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) = 0;

    virtual uint8_t handleResponseMsg(const nsm_msg* responseMsg,
                                      size_t responseLen) = 0;

    virtual requester::Coroutine update(SensorManager& manager,
                                        eid_t eid) override;
};

} // namespace nsm

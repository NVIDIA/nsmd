#pragma once

#include "platform-environmental.h"

#include "types.hpp"

namespace nsm
{
class NsmSensor
{
  public:
    NsmSensor(std::string& name, bool priority) : name(name), priority(priority)
    {}
    virtual ~NsmSensor() = default;
    virtual std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) = 0;
    virtual uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                                      size_t responseLen) = 0;
    bool isPriority()
    {
        return priority;
    }

    const std::string name;

  protected:
    bool priority;
};

} // namespace nsm
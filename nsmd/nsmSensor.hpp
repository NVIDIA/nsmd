#pragma once

#include "types.hpp"

struct nsm_msg;

namespace nsm
{
class NsmSensor
{
  public:
    NsmSensor(const std::string& name, const std::string& type) :
        name(name), type(type)
    {}

    virtual ~NsmSensor() = default;

    virtual std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) = 0;

    virtual uint8_t handleResponseMsg(const nsm_msg* responseMsg,
                                      size_t responseLen) = 0;

    const std::string& getName()
    {
        return name;
    }

    const std::string& getType()
    {
        return type;
    }

  private:
    const std::string name;
    const std::string type;
};

} // namespace nsm

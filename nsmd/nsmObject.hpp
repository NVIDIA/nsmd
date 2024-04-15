#pragma once

#include "requester/handler.hpp"
#include "types.hpp"

namespace nsm
{

class SensorManager;
class NsmObject
{
  public:
    NsmObject() = delete;
    NsmObject(const std::string& name, const std::string& type) :
        name(name), type(type)
    {}
    virtual ~NsmObject() = default;
    const std::string& getName()
    {
        return name;
    }

    const std::string& getType()
    {
        return type;
    }

    virtual requester::Coroutine update([[maybe_unused]] SensorManager& manager, [[maybe_unused]] eid_t eid)
    {
        co_return NSM_SW_SUCCESS;
    }

  private:
    const std::string name;
    const std::string type;
};
} // namespace nsm

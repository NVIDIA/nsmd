#pragma once

#include "types.hpp"

namespace nsm
{

class NsmObject
{
  public:
    NsmObject() = delete;
    NsmObject(const std::string& name,
              const std::string& type) :
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
   private:
    const std::string name;
    const std::string type;
};
} // namespace nsm

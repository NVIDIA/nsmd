#pragma once

#include "nsmDevice.hpp"
#include "nsmObject.hpp"
#include "nsmSensor.hpp"
#include "types.hpp"

#include <phosphor-logging/lg2.hpp>

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace nsm
{
// One level of macro indirection is required in order to resolve __COUNTER__,
// and get varname1 instead of varname__COUNTER__.
#define CONCAT(a, b) CONCAT_INNER(a, b)
#define CONCAT_INNER(a, b) a##b
#define UNIQUE_NAME(base) CONCAT(base, __FILE__##_##__COUNTER__)

using CreationFunction =
    std::function<void(const std::string& interface, const std::string& objPath,
                       NsmDeviceTable& nsmDevices)>;

#define REGISTER_NSM_CREATION_FUNCTION(func, interfaceName)                    \
    static void __attribute__((constructor)) CONCAT(_register_, __COUNTER__)()         \
    {                                                                          \
        auto& factory = NsmObjectFactory::instance();                          \
        factory.registerCreationFunction(func, interfaceName);                 \
    }

class NsmObjectFactory
{
  public:
    void operator=(const NsmObjectFactory&) = delete;
    NsmObjectFactory(NsmObjectFactory& other) = delete;

    static NsmObjectFactory& instance();

    void createObjects(const std::string& interface, const std::string& objPath,
                       NsmDeviceTable& nsmDevices);

    void registerCreationFunction(const CreationFunction& func,
                                  const std::string interfaceName);

    std::map<std::string, CreationFunction> creationFunctions;

  private:
    NsmObjectFactory() = default;
};

} // namespace nsm
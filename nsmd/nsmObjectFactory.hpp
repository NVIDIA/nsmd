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
    static void UNIQUE_NAME(_init_)()                                          \
    {                                                                          \
        auto factory = NsmObjectFactory::getInstance();                        \
        factory->registerCreationFunction(func, interfaceName);                \
    }                                                                          \
                                                                               \
    static void __attribute__((constructor)) UNIQUE_NAME(_register_)()         \
    {                                                                          \
        NsmObjectFactory::registerNsmObject(UNIQUE_NAME(_init_),               \
                                            interfaceName);                    \
    }

using InitFunction = std::function<void()>;

class NsmObjectFactory
{
  public:
    void operator=(const NsmObjectFactory&) = delete;
    NsmObjectFactory(NsmObjectFactory& other) = delete;

    static NsmObjectFactory* getInstance()
    {
        if (sInstance == nullptr)
        {
            sInstance = new NsmObjectFactory();
            for (auto func : initFunctions)
            {
                func();
            }
        }

        return sInstance;
    }

    static void registerNsmObject(const InitFunction& func,
                                  const std::string interfaceName)
    {
        lg2::info("register nsmObject: interfaceName={NAME}", "NAME",
                  interfaceName);
        initFunctions.push_back(func);
    }

    void createObjects(const std::string& interface, const std::string& objPath,
                       NsmDeviceTable& nsmDevices)
    {
        auto it = creationFunctions.find(interface);
        if (it != creationFunctions.end())
        {
            it->second(interface, objPath, nsmDevices);
        }
    }

    void registerCreationFunction(const CreationFunction& func,
                                  const std::string interfaceName)
    {
        creationFunctions[interfaceName] = func;
    }

    std::map<std::string, CreationFunction> creationFunctions;

  private:
    NsmObjectFactory()
    {}

    static std::vector<InitFunction> initFunctions;
    static NsmObjectFactory* sInstance;
};

} // namespace nsm
#include "nsmObjectFactory.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{
NsmObjectFactory& NsmObjectFactory::instance()
{
    static NsmObjectFactory instance;
    return instance;
}

void NsmObjectFactory::createObjects(SensorManager& manager,
                                     const std::string& interface,
                                     const std::string& objPath)
{
    auto it = creationFunctions.find(interface);
    if (it != creationFunctions.end())
    {
        try
        {
            it->second(manager, interface, objPath);
        }
        catch (const std::exception& e)
        {
            lg2::error(
                "NsmObjectFactory::createObjects error: '{ERROR}' for interface: {INTF} and path: {PATH}",
                "ERROR", e, "INTF", interface, "PATH", objPath);
        }
    }
}

void NsmObjectFactory::registerCreationFunction(const CreationFunction& func,
                                                const std::string interfaceName)
{
    lg2::info("register nsmObject: interfaceName={NAME}", "NAME",
              interfaceName);
    creationFunctions[interfaceName] = func;
}

} // namespace nsm
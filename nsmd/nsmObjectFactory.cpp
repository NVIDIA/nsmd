#include "nsmObjectFactory.hpp"

namespace nsm
{
NsmObjectFactory& NsmObjectFactory::instance()
{
    static NsmObjectFactory instance;
    return instance;
}

void NsmObjectFactory::createObjects(const std::string& interface,
                                     const std::string& objPath,
                                     NsmDeviceTable& nsmDevices)
{
    auto it =
        std::find_if(creationFunctions.begin(), creationFunctions.end(),
                     [&interface](const auto& it) {
                         return it.first.find(interface) != std::string::npos;
                     });
    if (it != creationFunctions.end())
    {
        it->second(interface, objPath, nsmDevices);
    }
}

void NsmObjectFactory::registerCreationFunction(const CreationFunction& func,
                                                const std::string interfaceName)
{
    creationFunctions[interfaceName] = func;
}

} // namespace nsm
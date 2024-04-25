#pragma once

#include "nsmSensor.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>

#include <stdexcept>

namespace nsm
{

template <typename IntfType>
using Interfaces = std::vector<std::shared_ptr<IntfType>>;

/**
 * @brief Base class for NsmInterfaceContainer and NsmInterfaceProvider
 *
 * @tparam IntfType type of the PDI (i.e. Asset, Dimension ect.)
 */
template <typename IntfType>
struct NsmInterfaces
{
    Interfaces<IntfType> interfaces;
    NsmInterfaces() = delete;
    NsmInterfaces(const Interfaces<IntfType>& interfaces) :
        interfaces(interfaces)
    {
        if (interfaces.empty())
        {
            throw std::runtime_error(
                "NsmInterfaces::NsmInterfaces - interfaces cannot be empty");
        }
    }
    /**
     * @brief Returns first pdi pointer from interfaces collection
     *
     * @return Reference to IntfType First PDI in interfaces collection
     */
    IntfType& pdi()
    {
        return *interfaces.front();
    }
};

/**
 * @brief Class which is creating and providing PDI object
 *
 * @tparam IntfType type of the PDI (i.e. Asset, Dimension ect.)
 */
template <typename IntfType>
class NsmInterfaceProvider : public NsmObject, public NsmInterfaces<IntfType>
{
  private:
    static auto createInterfaces(const dbus::Interfaces& objectsPaths)
    {
        Interfaces<IntfType> interfaces;
        for (const auto& path : objectsPaths)
        {
            interfaces.emplace_back(std::make_shared<IntfType>(
                utils::DBusHandler::getBus(), path.c_str()));
        }
        return interfaces;
    }

  public:
    NsmInterfaceProvider() = delete;
    NsmInterfaceProvider(const std::string& name, const std::string& type,
                         const dbus::Interfaces& objectsPaths) :
        NsmObject(name, type),
        NsmInterfaces<IntfType>(createInterfaces(objectsPaths))
    {}
    NsmInterfaceProvider(const std::string& name, const std::string& type,
                         const std::string& basePath) :
        NsmObject(name, type),
        NsmInterfaces<IntfType>(createInterfaces({basePath + name}))
    {}
    NsmInterfaceProvider(const std::string& name, const std::string& type,
                         const Interfaces<IntfType>& interfaces) :
        NsmObject(name, type), NsmInterfaces<IntfType>(interfaces)
    {}
    NsmInterfaceProvider(const std::string& name, const std::string& type,
                         std::shared_ptr<IntfType> pdi) :
        NsmObject(name, type),
        NsmInterfaces<IntfType>(Interfaces<IntfType>{pdi})
    {}
};

/**
 * @brief Class which is using PDI and stores shared PDIs objects collection
 *
 */
template <typename IntfType>
class NsmInterfaceContainer : protected NsmInterfaces<IntfType>
{
  public:
    NsmInterfaceContainer() = delete;
    NsmInterfaceContainer(const NsmInterfaceProvider<IntfType>& provider) :
        NsmInterfaces<IntfType>(provider.interfaces)
    {}
    NsmInterfaceContainer(const Interfaces<IntfType>& interfaces) :
        NsmInterfaces<IntfType>(interfaces)
    {}
    NsmInterfaceContainer(const std::shared_ptr<IntfType>& pdi) :
        NsmInterfaces<IntfType>(Interfaces<IntfType>{pdi})
    {}
};

} // namespace nsm
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
struct NsmInterfaceContainerBase
{
    Interfaces<IntfType> interfaces;
    NsmInterfaceContainerBase() = default;
    NsmInterfaceContainerBase(const Interfaces<IntfType>& interfaces) :
        interfaces(interfaces)
    {
        if (interfaces.empty())
        {
            throw std::runtime_error(
                "NsmInterfaceContainerBase::NsmInterfaceContainerBase - interfaces cannot be empty");
        }
    }
    /**
     * @brief Returns first pdi pointer from interfaces collection
     *
     * @return std::shared_ptr<IntfType> First PDI in interfaces collection
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
class NsmInterfaceProvider :
    public NsmObject,
    public NsmInterfaceContainerBase<IntfType>
{
  public:
    NsmInterfaceProvider() = delete;
    NsmInterfaceProvider(const std::string& name, const std::string& type,
                         const dbus::Interfaces& objectsPaths) :
        NsmObject(name, type)
    {
        if (objectsPaths.empty())
        {
            throw std::runtime_error(
                "NsmInterfaceProvider::NsmInterfaceProvider - objectsPaths cannot be empty");
        }
        for (const auto& path : objectsPaths)
        {
            NsmInterfaceContainerBase<IntfType>::interfaces.emplace_back(
                std::make_shared<IntfType>(utils::DBusHandler::getBus(),
                                           path.c_str()));
        }
    }
    NsmInterfaceProvider(const std::string& name, const std::string& type,
                         const std::string& basePath) :
        NsmObject(name, type),
        NsmInterfaceContainerBase<IntfType>(
            Interfaces<IntfType>{std::make_shared<IntfType>(
                utils::DBusHandler::getBus(), (basePath + name).c_str())})
    {}
    NsmInterfaceProvider(const std::string& name, const std::string& type,
                         const Interfaces<IntfType>& interfaces) :
        NsmObject(name, type), NsmInterfaceContainerBase<IntfType>(interfaces)
    {}
    NsmInterfaceProvider(const std::string& name, const std::string& type,
                         std::shared_ptr<IntfType> pdi) :
        NsmObject(name, type),
        NsmInterfaceContainerBase<IntfType>(Interfaces<IntfType>{pdi})
    {}
};

/**
 * @brief Class which is using PDI and stores shared PDIs objects collection
 *
 */
template <typename IntfType>
class NsmInterfaceContainer : protected NsmInterfaceContainerBase<IntfType>
{
  public:
    NsmInterfaceContainer() = delete;
    NsmInterfaceContainer(const NsmInterfaceProvider<IntfType>& provider) :
        NsmInterfaceContainerBase<IntfType>(provider.interfaces)
    {}
    NsmInterfaceContainer(const Interfaces<IntfType>& interfaces) :
        NsmInterfaceContainerBase<IntfType>(interfaces)
    {}
    NsmInterfaceContainer(const std::shared_ptr<IntfType>& pdi) :
        NsmInterfaceContainerBase<IntfType>(Interfaces<IntfType>{pdi})
    {}
};

} // namespace nsm
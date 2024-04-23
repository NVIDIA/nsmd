#pragma once

#include "nsmSensor.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <utils.hpp>

#include <stdexcept>

namespace nsm
{

/**
 * @brief Class which is using PDI and stores shared PDI base object
 *
 */
template <typename IntfType>
class NsmInterfaceContainer
{
  protected:
    std::shared_ptr<IntfType> pdi;

  public:
    NsmInterfaceContainer() = delete;
    NsmInterfaceContainer(std::shared_ptr<IntfType> pdi) : pdi(pdi)
    {}
};

/**
 * @brief Class which is creating and providing PDI object
 *
 * @tparam IntfType type of the PDI (i.e. Asset, Dimension ect.)
 */
template <typename IntfType>
class NsmInterfaceProvider : public NsmObject, public IntfType
{
  public:
    NsmInterfaceProvider() = delete;
    NsmInterfaceProvider(const std::string& name, const std::string& type,
                         const std::string& basePath) :
        NsmObject(name, type),
        IntfType(utils::DBusHandler::getBus(), (basePath + name).c_str())
    {}
};

} // namespace nsm
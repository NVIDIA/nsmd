#pragma once

#include "nsmSensor.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <utils.hpp>

#include <stdexcept>

namespace nsm
{

/**
 * @brief PDI base object type for providing casting back functionality
 *
 */
using NsmIntfBase = std::shared_ptr<NsmObject>;

/**
 * @brief Class which is using PDI and stores shared PDI base object (PDI type
 * casted to std::shared_ptr<NsmObject> for backword casting )
 *
 */
class NsmInterfaceContainer
{
  private:
    NsmIntfBase pdi;

  public:
    NsmInterfaceContainer() = delete;
    NsmInterfaceContainer(NsmIntfBase pdi) : pdi(pdi)
    {}

    /**
     * @brief Casts stored pdi shared base object to PDI type
     *
     * @tparam IntfType type of PDI to cast
     * @return IntfType& Reference to casted PDI object
     * @throw bad_cast when provided parent object is not inheriting after
     * IntfType
     */
    template <typename IntfType>
    IntfType& cast()
    {
        auto ptr = dynamic_pointer_cast<IntfType>(pdi);
        if (!ptr)
        {
            lg2::error("Couldn't cast std::shared_ptr<NsmObject> to {TYPE}",
                       "TYPE", typeid(IntfType).name());
            throw std::bad_cast();
        }
        return *ptr;
    }
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
        IntfType(utils::DBusHandler().getBus(), (basePath + name).c_str())
    {}
};

} // namespace nsm
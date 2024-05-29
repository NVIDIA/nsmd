#pragma once
#include "base.h"

#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"

#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/FpgaType/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Location/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Accelerator/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Health/server.hpp>

namespace nsm
{
using AcceleratorIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Accelerator>;
using accelaratorType = sdbusplus::xyz::openbmc_project::Inventory::Item::
    server::Accelerator::AcceleratorType;
using AssetIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset>;
using AssociationDefIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::association::Definitions>;
using LocationIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Location>;
using FpgaTypeIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::inventory::decorator::FpgaType>;
using HealthIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::state::decorator::Health>;

class NsmFpgaProcessor : public NsmObject
{
  public:
    NsmFpgaProcessor(sdbusplus::bus::bus& bus, std::string& name,
                     std::string& type, std::string& inventoryObjPath,
                     const std::vector<utils::Association>& associations,
                     const std::string& fpgaType,
                     const std::string& locationType, std::string& health);

  private:
    std::unique_ptr<AcceleratorIntf> acceleratorIntf;
    std::unique_ptr<AssetIntf> assetIntf;
    std::unique_ptr<AssociationDefIntf> associationDefIntf;
    std::unique_ptr<LocationIntf> locationIntf;
    std::unique_ptr<FpgaTypeIntf> fpgaTypeIntf;
    std::unique_ptr<HealthIntf> healthIntf;
};
} // namespace nsm

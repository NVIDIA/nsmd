#pragma once

#include "libnsm/platform-environmental.h"

#include "nsmInterface.hpp"

#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Dimension/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PowerLimit/server.hpp>

namespace nsm
{
using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;
using AssetIntf = object_t<Inventory::Decorator::server::Asset>;
using DimensionIntf = object_t<Inventory::Decorator::server::Dimension>;
using PowerLimitIntf = object_t<Inventory::Decorator::server::PowerLimit>;

class NsmInventoryProperty : public NsmSensor, public NsmInterfaceContainer
{
  public:
    NsmInventoryProperty(std::shared_ptr<NsmInterfaceProvider<AssetIntf>> pdi,
                         nsm_inventory_property_identifiers property);
    NsmInventoryProperty(
        std::shared_ptr<NsmInterfaceProvider<DimensionIntf>> pdi,
        nsm_inventory_property_identifiers property);
    NsmInventoryProperty(
        std::shared_ptr<NsmInterfaceProvider<PowerLimitIntf>> pdi,
        nsm_inventory_property_identifiers property);
    NsmInventoryProperty() = delete;

    std::optional<Request> genRequestMsg(eid_t eid,
                                         uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    nsm_inventory_property_identifiers property;
};

} // namespace nsm
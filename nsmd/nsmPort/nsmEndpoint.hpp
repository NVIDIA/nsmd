#pragma once

#include "common/types.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Endpoint/server.hpp>

namespace nsm
{
using AssociationDefIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::association::Definitions>;
using EndpointIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::inventory::item::Endpoint>;

class NsmEndpoint : public NsmObject
{
  public:
    NsmEndpoint(sdbusplus::bus::bus& bus, const std::string& name,
                const std::string& type,
                const std::vector<utils::Association>& associations,
                const std::string& fabricObjPath);

  private:
    std::unique_ptr<EndpointIntf> endpointIntf = nullptr;
    std::unique_ptr<AssociationDefIntf> associationDefIntf = nullptr;
};

} // namespace nsm

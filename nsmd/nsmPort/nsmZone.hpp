#pragma once

#include "common/types.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Inventory/Item/Zone/server.hpp>

namespace nsm
{
using ZoneIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::inventory::item::Zone>;

class NsmZone : public NsmObject
{
  public:
    NsmZone(sdbusplus::bus::bus& bus, const std::string& name,
            const std::string& type, const std::string& fabricObjPath,
            const std::string& zoneType);

  private:
    std::unique_ptr<ZoneIntf> zoneIntf = nullptr;
};

} // namespace nsm

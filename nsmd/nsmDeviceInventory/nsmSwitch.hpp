#pragma once

#include "libnsm/diagnostics.h"
#include "libnsm/network-ports.h"

#include "nsmDbusIfaceOverride/nsmResetIface.hpp"
#include "nsmInterface.hpp"
#include "utils.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <tal.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Common/UUID/server.hpp>
#include <xyz/openbmc_project/Control/Processor/Reset/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/NvSwitch/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Switch/server.hpp>

namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using UuidIntf = object_t<Common::server::UUID>;
using AssociationDefinitionsInft = object_t<Association::server::Definitions>;
using AssetIntf = object_t<Inventory::Decorator::server::Asset>;
using SwitchIntf = object_t<Inventory::Item::server::Switch>;
using NvSwitchIntf = object_t<Inventory::Item::server::NvSwitch>;
using ResetIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::control::processor::Reset>;

template <typename IntfType>
class NsmSwitchDI : public NsmInterfaceProvider<IntfType>
{
  public:
    NsmSwitchDI() = delete;
    NsmSwitchDI(const std::string& name, const std::string& inventoryObjPath) :
        NsmInterfaceProvider<IntfType>(name, "NSM_NVSwitch", inventoryObjPath),
        objPath(inventoryObjPath + name)
    {}

    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  private:
    std::string objPath;
};

class NsmSwitchDIReset : public NsmObject
{
  public:
    NsmSwitchDIReset(sdbusplus::bus::bus& bus, const std::string& name,
                     const std::string& type, std::string& inventoryObjPath,
                     std::shared_ptr<NsmDevice> device);

  private:
    std::shared_ptr<ResetIntf> resetIntf = nullptr;
    std::shared_ptr<NsmSwitchResetAsyncIntf> resetAsyncIntf = nullptr;
    std::string objPath;
};

} // namespace nsm

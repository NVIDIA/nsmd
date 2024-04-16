#pragma once
#include "platform-environmental.h"

#include "globals.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

#include <iostream>

namespace nsm
{
typedef uint8_t enum8;
using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;
using AssociationDefinitionsInft = object_t<Association::server::Definitions>;
using OperationalStatusIntf =
    object_t<State::Decorator::server::OperationalStatus>;
using AssetIntf = object_t<Inventory::Decorator::server::Asset>;
using SoftwareIntf = object_t<Software::server::Version>;

class NsmGPUSWInventoryDriverVersionAndStatus : public NsmObject
{
  public:
    NsmGPUSWInventoryDriverVersionAndStatus(
        sdbusplus::bus::bus& bus, const std::string& name,
        const std::vector<utils::Association>& associations,
        const std::string& type, const std::string& manufacturer);

    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  private:
    void updateValue(enum8 driverState, std::string driverVersion);
    std::unique_ptr<SoftwareIntf> softwareVer = nullptr;
    std::unique_ptr<OperationalStatusIntf> operationalStatus = nullptr;
    std::unique_ptr<AssociationDefinitionsInft> associationDef = nullptr;
    std::unique_ptr<AssetIntf> asset = nullptr;

    // to be consumed by unit tests
    enum8 driverState = 0;
    std::string driverVersion = "";
};
} // namespace nsm
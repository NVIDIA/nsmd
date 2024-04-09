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

#include <iostream>

namespace nsm
{
using RequesterHandler = requester::Handler<requester::Request>;

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using AssociationDefinitionsInft = object_t<Association::server::Definitions>;
using AssetIntf = object_t<Inventory::Decorator::server::Asset>;
using SoftwareIntf = object_t<Software::server::Version>;

class NsmPCIeRetimerFirmwareVersion : public NsmObject
{
  public:
    NsmPCIeRetimerFirmwareVersion(
        sdbusplus::bus::bus& bus, const std::string& name,
        const std::vector<utils::Association>& associations,
        const std::string& type, const std::string& manufacturer,
        const uint8_t instanceNumber);

    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  private:
    void updateValue(std::string val);
    std::unique_ptr<SoftwareIntf> softwareVer_ = nullptr;
    std::unique_ptr<AssociationDefinitionsInft> associationDef_ = nullptr;
    std::unique_ptr<AssetIntf> asset_ = nullptr;
    uint8_t instanceNumber;
};
} // namespace nsm

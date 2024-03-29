#pragma once
#include "platform-environmental.h"

#include "globals.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmSensor.hpp"

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
// using HealthIntf = object_t<State::Decorator::server::Health>;
using AssetIntf = object_t<Inventory::Decorator::server::Asset>;
using SoftwareIntf = object_t<Software::server::Version>;

class NsmSWInventoryDriverVersionAndStatus : public NsmSensor
{
  public:
    NsmSWInventoryDriverVersionAndStatus(sdbusplus::bus::bus& bus,
                                         const std::string& name,
                                         const std::string& type,
                                         const std::string& manufacturer);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateValue(enum8 driverState, char driverVersion[64]);
    std::unique_ptr<SoftwareIntf> softwareVer_ = nullptr;
    std::unique_ptr<OperationalStatusIntf> operationalStatus_ = nullptr;
    std::unique_ptr<AssociationDefinitionsInft> associationDef_ = nullptr;
    std::unique_ptr<AssetIntf> asset_ = nullptr;
    std::string assoc;
};
} // namespace nsm
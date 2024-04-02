#pragma once
#include "base.h"
#include "platform-environmental.h"

#include "nsmSensor.hpp"

#include <xyz/openbmc_project/Inventory/Item/Accelerator/server.hpp>
#include <com/nvidia/MigMode/server.hpp>
#include <xyz/openbmc_project/Memory/MemoryECC/server.hpp>

namespace nsm
{

using AcceleratorIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Accelerator>;
using accelaratorType = sdbusplus::xyz::openbmc_project::Inventory::Item::
    server::Accelerator::AcceleratorType;

class NsmAcceleratorIntf : public NsmObject
{
  public:
    NsmAcceleratorIntf(sdbusplus::bus::bus& bus, std::string& name,
                       std::string& type, std::string& inventoryObjPath);

  private:
    std::unique_ptr<AcceleratorIntf> acceleratorIntf = nullptr;
};

using MigModeIntf =
    sdbusplus::server::object_t<sdbusplus::com::nvidia::server::MigMode>;

class NsmMigMode : public NsmSensor
{
  public:
    NsmMigMode(sdbusplus::bus::bus& bus, std::string& name, std::string& type,
               std::string& inventoryObjPath);
    NsmMigMode() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(bitfield8_t flags);

    std::unique_ptr<MigModeIntf> migModeIntf = nullptr;
};

using EccModeIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Memory::server::MemoryECC>;

class NsmEccMode : public NsmSensor
{
  public:
    NsmEccMode(std::string& name, std::string& type,
               std::shared_ptr<EccModeIntf> eccIntf);
    NsmEccMode() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(bitfield8_t flags);

    std::shared_ptr<EccModeIntf> eccModeIntf = nullptr;
};

class NsmEccErrorCounts : public NsmSensor
{
  public:
    NsmEccErrorCounts(std::string& name, std::string& type,
                      std::shared_ptr<EccModeIntf> eccIntf);
    NsmEccErrorCounts() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(struct nsm_ECC_error_counts);

    std::shared_ptr<EccModeIntf> eccErrorCountIntf = nullptr;
};

} // namespace nsm
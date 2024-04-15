#pragma once
#include "base.h"
#include "pci-links.h"
#include "platform-environmental.h"

#include "nsmSensor.hpp"

#include <com/nvidia/Edpp/server.hpp>
#include <com/nvidia/MigMode/server.hpp>
#include <xyz/openbmc_project/Common/UUID/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Accelerator/server.hpp>
#include <xyz/openbmc_project/Memory/MemoryECC/server.hpp>
#include <xyz/openbmc_project/PCIe/PCIeECC/server.hpp>

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

using UuidIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Common::server::UUID>;
class NsmUuidIntf : public NsmObject
{
  public:
    NsmUuidIntf(sdbusplus::bus::bus& bus, std::string& name, std::string& type,
                std::string& inventoryObjPath, uuid_t uuid);

  private:
    std::unique_ptr<UuidIntf> uuidIntf = nullptr;
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

class NsmPcieGroup : public NsmSensor
{
  public:
    NsmPcieGroup(const std::string& name, const std::string& type,
                 uint8_t deviceId, uint8_t groupId);
    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

  private:
    uint8_t deviceId;
    uint8_t groupId;
};

using PCieEccIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::PCIe::server::PCIeECC>;

class NsmPciGroup2 : public NsmPcieGroup
{
  public:
    NsmPciGroup2(const std::string& name, const std::string& type,
                 std::shared_ptr<PCieEccIntf> pCieECCIntf, uint8_t deviceId);
    NsmPciGroup2() = default;

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(
        const struct nsm_query_scalar_group_telemetry_group_2& data);
    std::shared_ptr<PCieEccIntf> pCieEccIntf = nullptr;
};

class NsmPciGroup3 : public NsmPcieGroup
{
  public:
    NsmPciGroup3(const std::string& name, const std::string& type,
                 std::shared_ptr<PCieEccIntf> pCieECCIntf, uint8_t deviceId);
    NsmPciGroup3() = default;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(
        const struct nsm_query_scalar_group_telemetry_group_3& data);
    std::shared_ptr<PCieEccIntf> pCieEccIntf = nullptr;
};

class NsmPciGroup4 : public NsmPcieGroup
{
  public:
    NsmPciGroup4(const std::string& name, const std::string& type,
                 std::shared_ptr<PCieEccIntf> pCieECCIntf, uint8_t deviceId);
    NsmPciGroup4() = default;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(
        const struct nsm_query_scalar_group_telemetry_group_4& data);

    std::shared_ptr<PCieEccIntf> pCieEccIntf = nullptr;
};

using EDPpIntf =
    sdbusplus::server::object_t<sdbusplus::com::nvidia::server::Edpp>;

class EDPpLocal : public EDPpIntf
{
  public:
    EDPpLocal(sdbusplus::bus::bus& bus, const std::string& objPath) :
        EDPpIntf(bus, objPath.c_str(), action::emit_interface_added)
    {}

    void reset()
    {
        return;
    }
};

class NsmEDPpScalingFactor : public NsmSensor
{
  public:
    NsmEDPpScalingFactor(sdbusplus::bus::bus& bus, std::string& name,
                         std::string& type, std::string& inventoryObjPath);
    NsmEDPpScalingFactor() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(struct nsm_EDPp_scaling_factors scaling_factor);

    std::shared_ptr<EDPpLocal> eDPpIntf = nullptr;
};

} // namespace nsm
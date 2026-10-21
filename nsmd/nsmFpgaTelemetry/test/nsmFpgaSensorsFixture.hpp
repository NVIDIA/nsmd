// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#pragma once
#include "fakeFpga.hpp"
#include "nsmFpgaSensors.hpp"

using namespace nsm;
using namespace ::testing;

namespace
{
// The capture buffer of a rule that read one word
inline std::array<uint8_t, 4> wordBytes(uint32_t word)
{
    std::array<uint8_t, 4> bytes{};
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        bytes[i] = static_cast<uint8_t>(word >> (8 * i));
    }
    return bytes;
}
struct NsmFpgaSensorsTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    FakeFpga fpga;
    std::vector<std::weak_ptr<NsmFpgaStatus>> statuses;

    NsmFpgaSensorsTest() : SensorManagerTest(devices) {}

    ~NsmFpgaSensorsTest()
    {
        NsmFpgaController::clear();
        NsmFpgaStatus::clear();
        NsmFpgaGroup::clear();
        NsmFpgaPortObject::clear();
        cleanupDeviceSensors(devices);
        drainFpgaWatchers(statuses);
    }

    std::shared_ptr<NsmFpgaStatus> makeStatus(bool enabled)
    {
        auto status = std::make_shared<NsmFpgaStatus>(
            "HGX_FPGA_0",
            "/xyz/openbmc_project/inventory/system/processors/FPGA_0");
        status->setOnline(enabled);
        status->setTypeCheck(enabled);
        status->setDigestCheck(enabled);
        statuses.push_back(status);
        return status;
    }

    FpgaSensorInfo makeInfo(const std::string& item, const nsm_fpga_rule* rule)
    {
        FpgaSensorInfo info{};
        info.name = "HGX_Chassis_0_FPGA_0_" + item;
        info.type = "NSM_FpgaTelemetry";
        info.objectPath = "/xyz/openbmc_project/inventory/system/chassis/" +
                          info.name;
        info.rule = rule;
        info.physicalContext = "FPGA";
        info.associations.push_back(
            {"chassis", "all_states",
             "/xyz/openbmc_project/inventory/system/chassis/HGX_Chassis_0"});
        info.chassisAssociation =
            "/xyz/openbmc_project/inventory/system/chassis/HGX_Chassis_0";
        return info;
    }

    std::shared_ptr<MockNsmDevice> makeDevice()
    {
        auto device = std::make_shared<MockNsmDevice>(3, 0, "MCTP_EID", "17",
                                                      0);
        ON_CALL(*device, sensorIO(_, _, _, _, _))
            .WillByDefault(Invoke(&fpga, &FakeFpga::io));
        return device;
    }

    // A link item feeding one property of the Port object at portPath
    std::shared_ptr<NsmFpgaSensor>
        makePortItem(const std::string& item, const std::string& portPath,
                     const std::string& property,
                     std::shared_ptr<NsmFpgaStatus> status,
                     std::shared_ptr<NsmFpgaController> controller,
                     const nsm_fpga_rule* rule)
    {
        auto info = makeInfo(item, rule);
        info.objectPath = portPath;
        info.portProperty = property;
        info.portType =
            "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.BidirectionalPort";
        info.portProtocol =
            "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.Ethernet";
        return std::make_shared<NsmFpgaSensor>(utils::DBusHandler::getBus(),
                                               info, status, controller);
    }

    std::shared_ptr<NsmFpgaSensor>
        makeState(const std::string& item,
                  std::shared_ptr<NsmFpgaStatus> status,
                  std::shared_ptr<NsmFpgaController> controller,
                  const nsm_fpga_rule* rule,
                  uint64_t refreshIntervalUsec = RR_REFRESH_LIMIT_IN_USEC,
                  bool powerState = false)
    {
        auto info = makeInfo(item, rule);
        info.refreshIntervalUsec = refreshIntervalUsec;
        info.powerState = powerState;
        return std::make_shared<NsmFpgaSensor>(utils::DBusHandler::getBus(),
                                               info, status, controller);
    }
};
} // namespace

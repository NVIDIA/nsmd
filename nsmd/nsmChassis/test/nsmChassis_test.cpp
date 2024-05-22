/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "nsmChassis.hpp"
#include "nsmGpuPresenceAndPowerStatus.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmPowerSupplyStatus.hpp"

namespace nsm
{
void nsmChassisCreateSensors(SensorManager& manager,
                             const std::string& interface,
                             const std::string& objPath);
};

using namespace nsm;

struct NsmChassisTest : public testing::Test, public utils::DBusTest
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_Chassis";
    const std::string name = "HGX_GPU_SXM_1";
    const std::string objPath = chassisInventoryBasePath / name;
 
    const uuid_t gpuUuid = "992b3ec1-e468-f145-8686-409009062aa8";
    const uuid_t gpuDeviceUuid = "000b3ec1-0068-0045-0086-000009062aa8";
    const uuid_t fpgaUuid = "992b3ec1-e464-f145-8686-409009062aa8";
 
    NsmDeviceTable devices{
        {std::make_shared<NsmDevice>(gpuUuid)},
        {std::make_shared<NsmDevice>(fpgaUuid)},
    };
    NsmDevice& gpu = *devices[0];
    NsmDevice& fpga = *devices[1];
 
    NiceMock<MockSensorManager> mockManager{devices};
 
    const PropertyValuesCollection error = {
        {"Type", "NSM_GPU_cassis"},
    };
    const PropertyValuesCollection basic = {
        {"Name", name},
        {"Type", "NSM_Chassis"},
        {"UUID", gpuUuid},
        {"DEVICE_UUID", gpuDeviceUuid},
    };
    const PropertyValuesCollection fpgaProperties = {
        {"Name", name},
        {"Type", "NSM_Chassis"},
        {"UUID", fpgaUuid},
    };
    const PropertyValuesCollection asset = {
        {"Type", "NSM_Asset"},
        {"Manufacturer", "NVIDIA"},
    };
    const PropertyValuesCollection chassisType = {
        {"Type", "NSM_ChassisType"},
        {"ChassisType",
         "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module"},
    };
    const PropertyValuesCollection dimension = {
        {"Type", "NSM_Dimension"},
    };
    const PropertyValuesCollection location = {
        {"Type", "NSM_Location"},
        {"LocationType",
         "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"},
    };
    const PropertyValuesCollection locationCode = {
        {"Type", "NSM_LocationCode"},
        {"LocationCode", "SXM2"},
    };
    const PropertyValuesCollection health = {
        {"Type", "NSM_Health"},
        {"Health", "xyz.openbmc_project.State.Decorator.Health.HealthType.OK"},
    };
    const PropertyValuesCollection powerLimit = {
        {"Type", "NSM_PowerLimit"},
        {"Priority", false},
    };
    const PropertyValuesCollection operationalStatus = {
        {"Type", "NSM_OperationalStatus"},
        {"InstanceNumber", uint64_t(1)},
        {"InventoryObjPaths", std::vector<std::string>{objPath}},
        {"Priority", true},
    };
    const PropertyValuesCollection powerState = {
        {"Type", "NSM_PowerState"},
        {"InstanceNumber", uint64_t(2)},
        {"InventoryObjPaths",
         std::vector<std::string>{
             objPath,
             objPath + "/PCIeDevices/Device1",
         }},
        {"Priority", false},
    };
};
 
TEST_F(NsmChassisTest, badTestCreateDeviceSensors)
{
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(error, "Type")))
        .WillOnce(Return(get(basic, "UUID")));
    EXPECT_NO_THROW(
        nsmChassisCreateSensors(mockManager, basicIntfName, objPath));
    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.deviceSensors.size());
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(0, gpu.roundRobinSensors.size());
    EXPECT_EQ(0, gpu.deviceSensors.size());
}
TEST_F(NsmChassisTest, goodTestCreateDeviceSensors)
{
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(basic, "Type")))
        .WillOnce(Return(get(basic, "UUID")))
        .WillOnce(Return(get(basic, "DEVICE_UUID")));
    nsmChassisCreateSensors(mockManager, basicIntfName, objPath);
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(chassisType, "Type")))
        .WillOnce(Return(get(basic, "UUID")))
        .WillOnce(Return(get(chassisType, "ChassisType")));
    nsmChassisCreateSensors(mockManager, basicIntfName + ".ChassisType",
                            objPath);
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(health, "Type")))
        .WillOnce(Return(get(basic, "UUID")))
        .WillOnce(Return(get(health, "Health")));
    nsmChassisCreateSensors(mockManager, basicIntfName + ".Health", objPath);
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(location, "Type")))
        .WillOnce(Return(get(basic, "UUID")))
        .WillOnce(Return(get(location, "LocationType")));
    nsmChassisCreateSensors(mockManager, basicIntfName + ".Location", objPath);
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(locationCode, "Type")))
        .WillOnce(Return(get(basic, "UUID")))
        .WillOnce(Return(get(locationCode, "LocationCode")));
    nsmChassisCreateSensors(mockManager, basicIntfName + ".LocationCode",
                            objPath);
    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.deviceSensors.size());
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(0, gpu.roundRobinSensors.size());
    EXPECT_EQ(5, gpu.deviceSensors.size());
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<UuidIntf>>(
                           gpu.deviceSensors[0]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<ChassisIntf>>(
                           gpu.deviceSensors[1]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
                           gpu.deviceSensors[2]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<LocationIntf>>(
                           gpu.deviceSensors[3]));
    EXPECT_NE(nullptr,
              dynamic_pointer_cast<NsmInterfaceProvider<LocationCodeIntf>>(
                  gpu.deviceSensors[4]));
 
    EXPECT_EQ(gpuDeviceUuid, dynamic_pointer_cast<NsmInterfaceProvider<UuidIntf>>(
                           gpu.deviceSensors[0])
                           ->pdi()
                           .uuid());
    EXPECT_EQ(ChassisIntf::ChassisType::Module,
              dynamic_pointer_cast<NsmInterfaceProvider<ChassisIntf>>(
                  gpu.deviceSensors[1])
                  ->pdi()
                  .type());
    EXPECT_EQ(HealthIntf::HealthType::OK,
              dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
                  gpu.deviceSensors[2])
                  ->pdi()
                  .health());
    EXPECT_EQ(LocationIntf::LocationTypes::Embedded,
              dynamic_pointer_cast<NsmInterfaceProvider<LocationIntf>>(
                  gpu.deviceSensors[3])
                  ->pdi()
                  .locationType());
    EXPECT_EQ(get<std::string>(locationCode, "LocationCode"),
              dynamic_pointer_cast<NsmInterfaceProvider<LocationCodeIntf>>(
                  gpu.deviceSensors[4])
                  ->pdi()
                  .locationCode());
}

TEST_F(NsmChassisTest, goodTestCreateStaticSensors)
{
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .Times(6)
        .WillRepeatedly(
            [](eid_t, Request&, const nsm_msg**,
               size_t*) -> requester::Coroutine { co_return NSM_SUCCESS; });
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(asset, "Type")))
        .WillOnce(Return(get(basic, "UUID")))
        .WillOnce(Return(get(asset, "Manufacturer")));
    nsmChassisCreateSensors(mockManager, basicIntfName + ".Asset", objPath);
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(dimension, "Type")))
        .WillOnce(Return(get(basic, "UUID")));
    nsmChassisCreateSensors(mockManager, basicIntfName + ".Dimension", objPath);
    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.deviceSensors.size());
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(0, gpu.roundRobinSensors.size());
    EXPECT_EQ(6, gpu.deviceSensors.size());
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInventoryProperty<AssetIntf>>(
                           gpu.deviceSensors[0]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInventoryProperty<AssetIntf>>(
                           gpu.deviceSensors[1]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInventoryProperty<AssetIntf>>(
                           gpu.deviceSensors[2]));
    EXPECT_EQ(get<std::string>(asset, "Manufacturer"),
              dynamic_pointer_cast<NsmInventoryProperty<AssetIntf>>(
                  gpu.deviceSensors[2])
                  ->pdi()
                  .manufacturer());
    EXPECT_NE(nullptr,
              dynamic_pointer_cast<NsmInventoryProperty<DimensionIntf>>(
                  gpu.deviceSensors[3]));
    EXPECT_NE(nullptr,
              dynamic_pointer_cast<NsmInventoryProperty<DimensionIntf>>(
                  gpu.deviceSensors[4]));
    EXPECT_NE(nullptr,
              dynamic_pointer_cast<NsmInventoryProperty<DimensionIntf>>(
                  gpu.deviceSensors[5]));
}

TEST_F(NsmChassisTest, goodTestCreateDynamicSensors)
{
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(powerLimit, "Type")))
        .WillOnce(Return(get(basic, "UUID")))
        .WillOnce(Return(get(powerLimit, "Priority")));
    nsmChassisCreateSensors(mockManager, basicIntfName + ".PowerLimit",
                            objPath);
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(operationalStatus, "Type")))
        .WillOnce(Return(get(fpgaProperties, "UUID")))
        .WillOnce(Return(get(operationalStatus, "InstanceNumber")))
        .WillOnce(Return(get(operationalStatus, "InventoryObjPaths")))
        .WillOnce(Return(get(operationalStatus, "Priority")));
    nsmChassisCreateSensors(mockManager, basicIntfName + ".OperationalStatus",
                            objPath);
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(powerState, "Type")))
        .WillOnce(Return(get(fpgaProperties, "UUID")))
        .WillOnce(Return(get(powerState, "InstanceNumber")))
        .WillOnce(Return(get(powerState, "InventoryObjPaths")))
        .WillOnce(Return(get(powerState, "Priority")));
    nsmChassisCreateSensors(mockManager, basicIntfName + ".PowerState",
                            objPath);
    EXPECT_EQ(1, fpga.prioritySensors.size());
    EXPECT_EQ(1, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.deviceSensors.size());
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(2, gpu.roundRobinSensors.size());
    EXPECT_EQ(0, gpu.deviceSensors.size());
}

struct NsmInventoryPropertyTest : public NsmChassisTest
{
    NsmChassis<AssetIntf> chassisAsset{name};
    NsmChassis<DimensionIntf> chassisDimension{name};
    NsmChassis<PowerLimitIntf> chassisPowerLimit{name};
    void SetUp() override
    {
        EXPECT_EQ(chassisAsset.getName(), name);
        EXPECT_EQ(chassisAsset.getType(), "NSM_Chassis");
        EXPECT_EQ(chassisDimension.getName(), name);
        EXPECT_EQ(chassisDimension.getType(), "NSM_Chassis");
        EXPECT_EQ(chassisPowerLimit.getName(), name);
        EXPECT_EQ(chassisPowerLimit.getType(), "NSM_Chassis");
    }

    std::shared_ptr<NsmInventoryPropertyBase> sensor;
    void testRequest()
    {
        auto request = sensor->genRequestMsg(eid, instanceId);
        EXPECT_TRUE(request.has_value());
        EXPECT_EQ(request.value().size(),
                  sizeof(nsm_msg_hdr) +
                      sizeof(nsm_get_inventory_information_req));
        auto requestPtr =
            reinterpret_cast<struct nsm_msg*>(request.value().data());
        uint8_t decodedProperty = 0;
        auto rc = decode_get_inventory_information_req(
            requestPtr, request.value().size(), &decodedProperty);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        EXPECT_EQ(decodedProperty, (uint8_t)sensor->property);
    }
    void testResponse(uint8_t* data, size_t valueSize)
    {
        std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                      NSM_RESPONSE_CONVENTION_LEN + valueSize);
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        auto rc = encode_get_inventory_information_resp(
            instanceId, NSM_SUCCESS, ERR_NULL, valueSize, data, responseMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        rc = sensor->handleResponseMsg(responseMsg, response.size());
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
    }
};

TEST_F(NsmInventoryPropertyTest, goodTestPartNumberRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<AssetIntf>>(
        chassisAsset, BOARD_PART_NUMBER);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestPartNumberResponse)
{
    std::string partNumber = "PN12345";
    sensor = std::make_shared<NsmInventoryProperty<AssetIntf>>(
        chassisAsset, BOARD_PART_NUMBER);
    testResponse((uint8_t*)partNumber.c_str(), partNumber.size());
    EXPECT_EQ(chassisAsset.pdi().partNumber(), partNumber);
}
TEST_F(NsmInventoryPropertyTest, goodTestSerialNumberRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<AssetIntf>>(chassisAsset,
                                                               SERIAL_NUMBER);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestSerialNumberResponse)
{
    std::string serialNumber = "SN12345";
    sensor = std::make_shared<NsmInventoryProperty<AssetIntf>>(chassisAsset,
                                                               SERIAL_NUMBER);
    testResponse((uint8_t*)serialNumber.c_str(), serialNumber.size());
    EXPECT_EQ(chassisAsset.pdi().serialNumber(), serialNumber);
}
TEST_F(NsmInventoryPropertyTest, goodTestModelRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<AssetIntf>>(chassisAsset,
                                                               MARKETING_NAME);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestModelResponse)
{
    std::string model = "NV123";
    sensor = std::make_shared<NsmInventoryProperty<AssetIntf>>(chassisAsset,
                                                               MARKETING_NAME);
    testResponse((uint8_t*)model.c_str(), model.size());
    EXPECT_EQ(chassisAsset.pdi().model(), model);
}
TEST_F(NsmInventoryPropertyTest, goodTestDepthRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        chassisDimension, PRODUCT_LENGTH);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestDepthResponse)
{
    uint32_t depth = 850;
    sensor = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        chassisDimension, PRODUCT_LENGTH);
    testResponse((uint8_t*)&depth, sizeof(depth));
    EXPECT_EQ(chassisDimension.pdi().depth(), (double)depth);
}
TEST_F(NsmInventoryPropertyTest, goodTestHeightRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        chassisDimension, PRODUCT_HEIGHT);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestHeightResponse)
{
    uint32_t height = 2100;
    sensor = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        chassisDimension, PRODUCT_HEIGHT);
    testResponse((uint8_t*)&height, sizeof(height));
    EXPECT_EQ(chassisDimension.pdi().height(), (double)height);
}
TEST_F(NsmInventoryPropertyTest, goodTestWidthRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        chassisDimension, PRODUCT_WIDTH);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestWidthResponse)
{
    uint32_t width = 712;
    sensor = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        chassisDimension, PRODUCT_WIDTH);
    testResponse((uint8_t*)&width, sizeof(width));
    EXPECT_EQ(chassisDimension.pdi().width(), (double)width);
}
TEST_F(NsmInventoryPropertyTest, goodTestMinPowerWattsRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
        chassisPowerLimit, MINIMUM_DEVICE_POWER_LIMIT);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestMinPowerWattsResponse)
{
    uint32_t minPowerWatts = 100;
    sensor = std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
        chassisPowerLimit, MINIMUM_DEVICE_POWER_LIMIT);
    testResponse((uint8_t*)&minPowerWatts, sizeof(minPowerWatts));
    EXPECT_EQ(chassisPowerLimit.pdi().minPowerWatts(), (size_t)minPowerWatts);
}
TEST_F(NsmInventoryPropertyTest, goodTestMaxPowerWattsRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
        chassisPowerLimit, MAXIMUM_DEVICE_POWER_LIMIT);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestMaxPowerWattsResponse)
{
    uint32_t maxPowerWatts = 100;
    sensor = std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
        chassisPowerLimit, MAXIMUM_DEVICE_POWER_LIMIT);
    testResponse((uint8_t*)&maxPowerWatts, sizeof(maxPowerWatts));
    EXPECT_EQ(chassisPowerLimit.pdi().maxPowerWatts(), (size_t)maxPowerWatts);
}
TEST_F(NsmInventoryPropertyTest, badTestRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<AssetIntf>>(
        chassisAsset, BOARD_PART_NUMBER);
    auto request = sensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}
TEST_F(NsmInventoryPropertyTest, badTestResponseSize)
{
    sensor = std::make_shared<NsmInventoryProperty<AssetIntf>>(
        chassisAsset, BOARD_PART_NUMBER);
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) - 1);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_inventory_information_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, 0, nullptr, responseMsg);
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
TEST_F(NsmInventoryPropertyTest, badTestCompletionErrorResponse)
{
    uint8_t value = 0;
    sensor = std::make_shared<NsmInventoryProperty<AssetIntf>>(
        chassisAsset, BOARD_PART_NUMBER);
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  NSM_RESPONSE_CONVENTION_LEN + sizeof(value));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_inventory_information_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, sizeof(value), &value, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    struct nsm_get_inventory_information_resp* resp =
        (struct nsm_get_inventory_information_resp*)responseMsg->payload;
    resp->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}
TEST_F(NsmInventoryPropertyTest, badTestNotImplementedResponse)
{
    try
    {
        uint8_t value = 0;
        sensor = std::make_shared<NsmInventoryProperty<AssetIntf>>(
            chassisAsset, MEMORY_VENDOR);
        testResponse(&value, sizeof(value));
        FAIL() << "Expected std::runtime_error";
    }
    catch (std::runtime_error const& err)
    {
        EXPECT_EQ(err.what(), std::string("Not implemented PDI"));
    }
    catch (...)
    {
        FAIL() << "Expected std::runtime_error";
    }
}

struct NsmPowerSupplyStatusTest : public NsmChassisTest
{
    NsmChassis<PowerStateIntf> chassisPowerState{name};

    void SetUp() override
    {
        EXPECT_EQ(chassisPowerState.getName(), name);
        EXPECT_EQ(chassisPowerState.getType(), "NSM_Chassis");
    }
    std::shared_ptr<NsmPowerSupplyStatus> sensor;
    void init(uint8_t gpuInstanceId)
    {
        eid = 12;
        sensor = std::make_shared<NsmPowerSupplyStatus>(chassisPowerState,
                                                        gpuInstanceId);
        EXPECT_NE(sensor, nullptr);
        EXPECT_EQ(sensor->getName(), name);
        EXPECT_EQ(sensor->getType(), "NSM_Chassis");
        EXPECT_EQ(sensor->gpuInstanceId, gpuInstanceId);
    }
    void testResponse(uint8_t status)
    {
        std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                      sizeof(nsm_get_power_supply_status_resp));
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        auto rc = encode_get_power_supply_status_resp(
            instanceId, NSM_SUCCESS, ERR_NULL, status, responseMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        rc = sensor->handleResponseMsg(responseMsg, response.size());
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
    }
};

TEST_F(NsmPowerSupplyStatusTest, goodTestRequest)
{
    init(0);
    auto request = sensor->genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_supply_status_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.value().data());
    auto rc =
        decode_get_power_supply_status_req(requestPtr, request.value().size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}
TEST_F(NsmPowerSupplyStatusTest, goodTestResponse)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        init(i);
        using PowerState =
            sdbusplus::xyz::openbmc_project::State::server::Chassis::PowerState;
        for (auto state : {PowerState::Off, PowerState::On})
        {
            uint8_t status = (state == PowerState::On) << i;
            testResponse(status);
            EXPECT_EQ(chassisPowerState.pdi().currentPowerState(), state);
        }
    }
}
TEST_F(NsmPowerSupplyStatusTest, badTestRequest)
{
    init(0);
    auto request = sensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}
TEST_F(NsmPowerSupplyStatusTest, badTestResponseSize)
{
    init(0);
    uint8_t status = 0;
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_power_supply_status_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_power_supply_status_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, status, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor->handleResponseMsg(responseMsg, response.size() - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
TEST_F(NsmPowerSupplyStatusTest, badTestCompletionErrorResponse)
{
    init(0);
    uint8_t status = 0;
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_power_supply_status_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_power_supply_status_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, status, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    struct nsm_get_power_supply_status_resp* resp =
        (struct nsm_get_power_supply_status_resp*)responseMsg->payload;
    resp->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

struct NsmGpuPresenceAndPowerStatusTest : public NsmChassisTest
{
    NsmChassis<OperationalStatusIntf> chassisOperationalStatus{name};
    void SetUp() override
    {
        EXPECT_EQ(chassisOperationalStatus.getName(), name);
        EXPECT_EQ(chassisOperationalStatus.getType(), "NSM_Chassis");
    }
    std::shared_ptr<NsmGpuPresenceAndPowerStatus> sensor;
    void init(uint8_t gpuInstanceId)
    {
        eid = 12;
        sensor = std::make_shared<NsmGpuPresenceAndPowerStatus>(
            chassisOperationalStatus, gpuInstanceId);
        EXPECT_NE(sensor, nullptr);
        EXPECT_EQ(sensor->getName(), name);
        EXPECT_EQ(sensor->getType(), "NSM_Chassis");
        EXPECT_EQ(sensor->gpuInstanceId, gpuInstanceId);
    }
    void testResponse(uint8_t presence, uint8_t power_status)
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_gpu_presence_and_power_status_resp));
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        auto rc = encode_get_gpu_presence_and_power_status_resp(
            instanceId, NSM_SUCCESS, ERR_NULL, presence, power_status,
            responseMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        rc = sensor->handleResponseMsg(responseMsg, response.size());
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
    }
};

TEST_F(NsmGpuPresenceAndPowerStatusTest, goodTestRequest)
{
    init(0);
    auto request = sensor->genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_gpu_presence_and_power_status_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.value().data());
    auto rc = decode_get_gpu_presence_and_power_status_req(
        requestPtr, request.value().size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}
TEST_F(NsmGpuPresenceAndPowerStatusTest, goodTestGpuStatusEnabledResponse)
{
    // "State": "Enabled" if presence=active, power=active
    for (uint8_t i = 0; i < 8; i++)
    {
        init(i);
        using StateType = sdbusplus::xyz::openbmc_project::State::Decorator::
            server::OperationalStatus::StateType;
        testResponse(0x01 << i, 0x01 << i);
        EXPECT_EQ(chassisOperationalStatus.pdi().state(), StateType::Enabled);
    }
}
TEST_F(NsmGpuPresenceAndPowerStatusTest,
       goodTestGpuStatusUnavailableOfflineResponse)
{
    //"State": "UnavailableOffline" if presence=active,
    for (uint8_t i = 0; i < 8; i++)
    {
        init(i);
        using StateType = sdbusplus::xyz::openbmc_project::State::Decorator::
            server::OperationalStatus::StateType;
        testResponse(0x01 << i, 0x00);
        EXPECT_EQ(chassisOperationalStatus.pdi().state(),
                  StateType::UnavailableOffline);
    }
}
TEST_F(NsmGpuPresenceAndPowerStatusTest, goodTestGpuStatusAbsentResponse)
{
    // power=inactive "State": "Absent" if presence=inactive
    for (uint8_t i = 0; i < 8; i++)
    {
        init(i);
        using StateType = sdbusplus::xyz::openbmc_project::State::Decorator::
            server::OperationalStatus::StateType;
        testResponse(0, 0);
        EXPECT_EQ(chassisOperationalStatus.pdi().state(), StateType::Absent);
    }
}
TEST_F(NsmGpuPresenceAndPowerStatusTest, badTestRequest)
{
    init(0);
    auto request = sensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}
TEST_F(NsmGpuPresenceAndPowerStatusTest, badTestResponseSize)
{
    init(0);
    uint8_t presence = 0;
    uint8_t power_status = 0;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_get_gpu_presence_and_power_status_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_gpu_presence_and_power_status_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, presence, power_status, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor->handleResponseMsg(responseMsg, response.size() - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
TEST_F(NsmGpuPresenceAndPowerStatusTest, badTestCompletionErrorResponse)
{
    init(0);
    uint8_t presence = 0;
    uint8_t power_status = 0;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_get_gpu_presence_and_power_status_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_gpu_presence_and_power_status_resp(
        instanceId, NSM_SUCCESS, ERR_NULL, presence, power_status, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    struct nsm_get_gpu_presence_and_power_status_resp* resp =
        (struct nsm_get_gpu_presence_and_power_status_resp*)
            responseMsg->payload;
    resp->hdr.completion_code = NSM_ERROR;
    response.resize(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}
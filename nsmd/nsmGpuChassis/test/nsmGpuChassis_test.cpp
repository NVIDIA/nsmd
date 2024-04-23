#include <gmock/gmock.h>
#include <gtest/gtest.h>
using namespace ::testing;

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmGpuChassis.hpp"
#include "nsmGpuPresenceAndPowerStatus.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmPowerSupplyStatus.hpp"

using namespace nsm;
using namespace ::testing;

struct NsmGpuChassisTest : public testing::Test
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_GPU_Chassis";
    const std::string name = "HGX_GPU_SXM_1";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/chassis/" + name;
};

struct NsmInventoryPropertyTest : public NsmGpuChassisTest
{
    std::shared_ptr<NsmGpuChassis<AssetIntf>> chassisAsset =
        std::make_shared<NsmGpuChassis<AssetIntf>>(name);
    std::shared_ptr<NsmGpuChassis<DimensionIntf>> chassisDimension =
        std::make_shared<NsmGpuChassis<DimensionIntf>>(name);
    std::shared_ptr<NsmGpuChassis<PowerLimitIntf>> chassisPowerLimit =
        std::make_shared<NsmGpuChassis<PowerLimitIntf>>(name);

    void SetUp() override
    {
        EXPECT_EQ(chassisAsset->getName(), name);
        EXPECT_EQ(chassisAsset->getType(), "NSM_GPU_Chassis");
        EXPECT_EQ(chassisDimension->getName(), name);
        EXPECT_EQ(chassisDimension->getType(), "NSM_GPU_Chassis");
        EXPECT_EQ(chassisPowerLimit->getName(), name);
        EXPECT_EQ(chassisPowerLimit->getType(), "NSM_GPU_Chassis");
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
        chassisAsset, nsm_inventory_property_identifiers::BOARD_PART_NUMBER);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestPartNumberResponse)
{
    std::string partNumber = "PN12345";
    sensor = std::make_shared<NsmInventoryProperty<AssetIntf>>(
        chassisAsset, nsm_inventory_property_identifiers::BOARD_PART_NUMBER);
    testResponse((uint8_t*)partNumber.c_str(), partNumber.size());
    EXPECT_EQ(chassisAsset->partNumber(), partNumber);
}
TEST_F(NsmInventoryPropertyTest, goodTestSerialNumberRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<AssetIntf>>(
        chassisAsset, nsm_inventory_property_identifiers::SERIAL_NUMBER);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestSerialNumberResponse)
{
    std::string serialNumber = "SN12345";
    sensor = std::make_shared<NsmInventoryProperty<AssetIntf>>(
        chassisAsset, nsm_inventory_property_identifiers::SERIAL_NUMBER);
    testResponse((uint8_t*)serialNumber.c_str(), serialNumber.size());
    EXPECT_EQ(chassisAsset->serialNumber(), serialNumber);
}
TEST_F(NsmInventoryPropertyTest, goodTestModelRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<AssetIntf>>(
        chassisAsset, nsm_inventory_property_identifiers::MARKETING_NAME);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestModelResponse)
{
    std::string model = "NV123";
    sensor = std::make_shared<NsmInventoryProperty<AssetIntf>>(
        chassisAsset, nsm_inventory_property_identifiers::MARKETING_NAME);
    testResponse((uint8_t*)model.c_str(), model.size());
    EXPECT_EQ(chassisAsset->model(), model);
}
TEST_F(NsmInventoryPropertyTest, goodTestDepthRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        chassisDimension, nsm_inventory_property_identifiers::PRODUCT_LENGTH);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestDepthResponse)
{
    uint32_t depth = 850;
    sensor = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        chassisDimension, nsm_inventory_property_identifiers::PRODUCT_LENGTH);
    testResponse((uint8_t*)&depth, sizeof(depth));
    EXPECT_EQ(chassisDimension->depth(), (double)depth);
}
TEST_F(NsmInventoryPropertyTest, goodTestHeightRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        chassisDimension, nsm_inventory_property_identifiers::PRODUCT_HEIGHT);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestHeightResponse)
{
    uint32_t height = 2100;
    sensor = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        chassisDimension, nsm_inventory_property_identifiers::PRODUCT_HEIGHT);
    testResponse((uint8_t*)&height, sizeof(height));
    EXPECT_EQ(chassisDimension->height(), (double)height);
}
TEST_F(NsmInventoryPropertyTest, goodTestWidthRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        chassisDimension, nsm_inventory_property_identifiers::PRODUCT_WIDTH);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestWidthResponse)
{
    uint32_t width = 712;
    sensor = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        chassisDimension, nsm_inventory_property_identifiers::PRODUCT_WIDTH);
    testResponse((uint8_t*)&width, sizeof(width));
    EXPECT_EQ(chassisDimension->width(), (double)width);
}
TEST_F(NsmInventoryPropertyTest, goodTestMinPowerWattsRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
        chassisPowerLimit,
        nsm_inventory_property_identifiers::MINIMUM_DEVICE_POWER_LIMIT);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestMinPowerWattsResponse)
{
    uint32_t minPowerWatts = 100;
    sensor = std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
        chassisPowerLimit,
        nsm_inventory_property_identifiers::MINIMUM_DEVICE_POWER_LIMIT);
    testResponse((uint8_t*)&minPowerWatts, sizeof(minPowerWatts));
    EXPECT_EQ(chassisPowerLimit->minPowerWatts(), (size_t)minPowerWatts);
}
TEST_F(NsmInventoryPropertyTest, goodTestMaxPowerWattsRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
        chassisPowerLimit,
        nsm_inventory_property_identifiers::MAXIMUM_DEVICE_POWER_LIMIT);
    testRequest();
}
TEST_F(NsmInventoryPropertyTest, goodTestMaxPowerWattsResponse)
{
    uint32_t maxPowerWatts = 100;
    sensor = std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
        chassisPowerLimit,
        nsm_inventory_property_identifiers::MAXIMUM_DEVICE_POWER_LIMIT);
    testResponse((uint8_t*)&maxPowerWatts, sizeof(maxPowerWatts));
    EXPECT_EQ(chassisPowerLimit->maxPowerWatts(), (size_t)maxPowerWatts);
}
TEST_F(NsmInventoryPropertyTest, badTestRequest)
{
    sensor = std::make_shared<NsmInventoryProperty<AssetIntf>>(
        chassisAsset, nsm_inventory_property_identifiers::BOARD_PART_NUMBER);
    auto request = sensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}
TEST_F(NsmInventoryPropertyTest, badTestResponseSize)
{
    sensor = std::make_shared<NsmInventoryProperty<AssetIntf>>(
        chassisAsset, nsm_inventory_property_identifiers::BOARD_PART_NUMBER);
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
        chassisAsset, nsm_inventory_property_identifiers::BOARD_PART_NUMBER);
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
            chassisAsset, nsm_inventory_property_identifiers::MEMORY_VENDOR);
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

struct NsmPowerSupplyStatusTest : public NsmGpuChassisTest
{
    std::shared_ptr<NsmGpuChassis<PowerStateIntf>> chassisPowerState =
        std::make_shared<NsmGpuChassis<PowerStateIntf>>(name);

    void SetUp() override
    {
        EXPECT_EQ(chassisPowerState->getName(), name);
        EXPECT_EQ(chassisPowerState->getType(), "NSM_GPU_Chassis");
    }
    std::shared_ptr<NsmPowerSupplyStatus> sensor;
    void init(std::shared_ptr<NsmInterfaceProvider<PowerStateIntf>> pdi,
              uint8_t gpuInstanceId)
    {
        eid = 12;
        sensor = std::make_shared<NsmPowerSupplyStatus>(pdi, gpuInstanceId);
        EXPECT_NE(sensor, nullptr);
        EXPECT_EQ(sensor->getName(), name);
        EXPECT_EQ(sensor->getType(), "NSM_GPU_Chassis");
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
    init(chassisPowerState, 0);
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
        init(chassisPowerState, i);
        using PowerState =
            sdbusplus::xyz::openbmc_project::State::server::Chassis::PowerState;
        for (auto state : {PowerState::Off, PowerState::On})
        {
            uint8_t status = (state == PowerState::On) << i;
            testResponse(status);
            EXPECT_EQ(chassisPowerState->currentPowerState(), state);
        }
    }
}
TEST_F(NsmPowerSupplyStatusTest, badTestRequest)
{
    init(chassisPowerState, 0);
    auto request = sensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}
TEST_F(NsmPowerSupplyStatusTest, badTestResponseSize)
{
    init(chassisPowerState, 0);
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
    init(chassisPowerState, 0);
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

struct NsmGpuPresenceAndPowerStatusTest : public NsmGpuChassisTest
{
    std::shared_ptr<NsmGpuChassis<OperationalStatusIntf>>
        chassisOperationalStatus =
            std::make_shared<NsmGpuChassis<OperationalStatusIntf>>(name);

    void SetUp() override
    {
        EXPECT_EQ(chassisOperationalStatus->getName(), name);
        EXPECT_EQ(chassisOperationalStatus->getType(), "NSM_GPU_Chassis");
    }
    std::shared_ptr<NsmGpuPresenceAndPowerStatus> sensor;
    void init(std::shared_ptr<NsmInterfaceProvider<OperationalStatusIntf>> pdi,
              uint8_t gpuInstanceId)
    {
        eid = 12;
        sensor =
            std::make_shared<NsmGpuPresenceAndPowerStatus>(pdi, gpuInstanceId);
        EXPECT_NE(sensor, nullptr);
        EXPECT_EQ(sensor->getName(), name);
        EXPECT_EQ(sensor->getType(), "NSM_GPU_Chassis");
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
    init(chassisOperationalStatus, 0);
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
        init(chassisOperationalStatus, i);
        using StateType = sdbusplus::xyz::openbmc_project::State::Decorator::
            server::OperationalStatus::StateType;
        testResponse(0x01 << i, 0x01 << i);
        EXPECT_EQ(chassisOperationalStatus->state(), StateType::Enabled);
    }
}
TEST_F(NsmGpuPresenceAndPowerStatusTest,
       goodTestGpuStatusUnavailableOfflineResponse)
{
    //"State": "UnavailableOffline" if presence=active,
    for (uint8_t i = 0; i < 8; i++)
    {
        init(chassisOperationalStatus, i);
        using StateType = sdbusplus::xyz::openbmc_project::State::Decorator::
            server::OperationalStatus::StateType;
        testResponse(0x01 << i, 0x00);
        EXPECT_EQ(chassisOperationalStatus->state(),
                  StateType::UnavailableOffline);
    }
}
TEST_F(NsmGpuPresenceAndPowerStatusTest, goodTestGpuStatusAbsentResponse)
{
    // power=inactive "State": "Absent" if presence=inactive
    for (uint8_t i = 0; i < 8; i++)
    {
        init(chassisOperationalStatus, i);
        using StateType = sdbusplus::xyz::openbmc_project::State::Decorator::
            server::OperationalStatus::StateType;
        testResponse(0, 0);
        EXPECT_EQ(chassisOperationalStatus->state(), StateType::Absent);
    }
}
TEST_F(NsmGpuPresenceAndPowerStatusTest, badTestRequest)
{
    init(chassisOperationalStatus, 0);
    auto request = sensor->genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}
TEST_F(NsmGpuPresenceAndPowerStatusTest, badTestResponseSize)
{
    init(chassisOperationalStatus, 0);
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
    init(chassisOperationalStatus, 0);
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
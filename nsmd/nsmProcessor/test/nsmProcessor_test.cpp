#include <gmock/gmock.h>
#include <gtest/gtest.h>
using ::testing::_;
using ::testing::Args;
using ::testing::ElementsAreArray;

#include "base.h"
#include "platform-environmental.h"

#define private public
#define protected public

#include "../nsmProcessor.hpp"

using namespace nsm;

auto bus = sdbusplus::bus::new_default();
std::string sensorName("dummy_sensor");
std::string sensorType("dummy_type");
std::string inventoryObjPath("/xyz/openbmc_project/inventory/dummy_device");

TEST(nsmMigMode, GoodGenReq)
{  
    nsm::NsmMigMode migSensor(bus, sensorName, sensorType, inventoryObjPath);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = migSensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command =
        reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_GET_MIG_MODE);
    EXPECT_EQ(command->data_size, 0);

}

TEST(nsmMigMode, GoodHandleResp)
{  
    nsm::NsmMigMode migSensor(bus, sensorName, sensorType, inventoryObjPath);
    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_MIG_mode_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
    bitfield8_t flags;
    flags.byte = 1;
	uint16_t reason_code = ERR_NULL;

	uint8_t rc = encode_get_MIG_mode_resp(0, NSM_SUCCESS,
                                  reason_code, &flags, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = migSensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(nsmMigMode, BadHandleResp)
{
    nsm::NsmMigMode migSensor(bus, sensorName, sensorType, inventoryObjPath);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_MIG_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags;
    flags.byte = 1;
    uint16_t reason_code = ERR_NULL;

    uint8_t rc =
        encode_get_MIG_mode_resp(0, NSM_SUCCESS, reason_code, &flags, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = migSensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    rc = migSensor.handleResponseMsg(response, msg_len - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(nsmMigMode, GoodUpdateReading)
{  
    nsm::NsmMigMode migSensor(bus, sensorName, sensorType, inventoryObjPath);
    bitfield8_t flags;
    flags.byte = 1;
    migSensor.updateReading(flags);
    EXPECT_EQ(migSensor.migModeIntf->migModeEnabled(), flags.bits.bit0);
}


TEST(nsmEccMode, GoodGenReq)
{  
    auto eccIntf =
                std::make_shared<EccModeIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmEccMode eccModeSensor(sensorName, sensorType, eccIntf);
    
    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = eccModeSensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command =
        reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_GET_ECC_MODE);
    EXPECT_EQ(command->data_size, 0);

}

TEST(nsmEccMode, GoodHandleResp)
{  
    auto eccIntf =
                std::make_shared<EccModeIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmEccMode sensor(sensorName, sensorType, eccIntf);

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_ECC_mode_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
    bitfield8_t flags;
    flags.byte = 1;
	uint16_t reason_code = ERR_NULL;

	uint8_t rc = encode_get_ECC_mode_resp(0, NSM_SUCCESS,
                                  reason_code, &flags, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(nsmEccMode, GoodUpdateReading)
{  
    auto eccIntf =
                std::make_shared<EccModeIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmEccMode sensor(sensorName, sensorType, eccIntf);
    bitfield8_t flags;
    flags.byte = 1;
    sensor.updateReading(flags);
    EXPECT_EQ(sensor.eccModeIntf->eccModeEnabled(), flags.bits.bit0);
}

TEST(nsmEccMode, BadHandleResp)
{  
    auto eccIntf =
                std::make_shared<EccModeIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmEccMode sensor(sensorName, sensorType, eccIntf);

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_ECC_mode_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
    bitfield8_t flags;
    flags.byte = 1;
	uint16_t reason_code = ERR_NULL;

	uint8_t rc = encode_get_ECC_mode_resp(0, NSM_SUCCESS,
                                  reason_code, &flags, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = responseMsg.size();
    
    rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    rc = sensor.handleResponseMsg(response, msg_len - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(nsmEccErrorCounts, GoodGenReq)
{  
    auto eccIntf =
                std::make_shared<EccModeIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmEccErrorCounts eccErrorCntSensor(sensorName, sensorType, eccIntf);
    
    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = eccErrorCntSensor.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command =
        reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_GET_ECC_ERROR_COUNTS);
    EXPECT_EQ(command->data_size, 0);

}

TEST( nsmEccErrorCounts, GoodHandleResp)
{  
    auto eccIntf =
                std::make_shared<EccModeIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmEccErrorCounts sensor(sensorName, sensorType, eccIntf);
    
    struct nsm_ECC_error_counts errorCounts;
    errorCounts.flags.byte = 132;
    errorCounts.sram_corrected = 1234;
    errorCounts.sram_uncorrected_secded = 4532;
    errorCounts.sram_uncorrected_parity = 6567;
    errorCounts.dram_corrected = 9876;
    errorCounts.dram_uncorrected = 9654;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_ECC_error_counts_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    
    uint8_t rc = encode_get_ECC_error_counts_resp(0, NSM_SUCCESS,
                                  reason_code, &errorCounts, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    size_t msg_len = response.size();
    rc = sensor.handleResponseMsg(responseMsg, msg_len);

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}


TEST( nsmEccErrorCounts, GoodUpdateReading)
{  
    auto eccIntf =
                std::make_shared<EccModeIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmEccErrorCounts sensor(sensorName, sensorType, eccIntf);
    struct nsm_ECC_error_counts errorCounts;
    errorCounts.flags.byte = 132;
    errorCounts.sram_corrected = 1234;
    errorCounts.sram_uncorrected_secded = 4532;
    errorCounts.sram_uncorrected_parity = 6567;
    errorCounts.dram_corrected = 9876;
    errorCounts.dram_uncorrected = 9654;
    sensor.updateReading(errorCounts);
    EXPECT_EQ(sensor.eccErrorCountIntf->ceCount(), errorCounts.sram_corrected);
    EXPECT_EQ(sensor.eccErrorCountIntf->ueCount(), errorCounts.sram_uncorrected_secded +
                      errorCounts.sram_uncorrected_parity);
}

TEST( nsmEccErrorCounts, BadHandleResp)
{  
    auto eccIntf =
                std::make_shared<EccModeIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmEccErrorCounts sensor(sensorName, sensorType, eccIntf);
    
    struct nsm_ECC_error_counts errorCounts;
    errorCounts.flags.byte = 132;
    errorCounts.sram_corrected = 1234;
    errorCounts.sram_uncorrected_secded = 4532;
    errorCounts.sram_uncorrected_parity = 6567;
    errorCounts.dram_corrected = 9876;
    errorCounts.dram_uncorrected = 9654;
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_ECC_error_counts_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    uint16_t reason_code = ERR_NULL;
    
    uint8_t rc = encode_get_ECC_error_counts_resp(0, NSM_SUCCESS,
                                  reason_code, &errorCounts, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    
    size_t msg_len = response.size();

    rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    rc = sensor.handleResponseMsg(responseMsg, msg_len - 1);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}
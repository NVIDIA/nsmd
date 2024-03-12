#include "base.h"
#include "platform-environmental.h"
#include <gtest/gtest.h>

TEST(getInventoryInformation, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_req));

	uint8_t property_identifier = 0xab;
	uint8_t data_size = sizeof(property_identifier);

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_get_inventory_information_req(0, property_identifier,
						       request);

	struct nsm_get_inventory_information_req *req =
	    reinterpret_cast<struct nsm_get_inventory_information_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_INVENTORY_INFORMATION, req->hdr.command);
	EXPECT_EQ(data_size, req->hdr.data_size);
	EXPECT_EQ(property_identifier, req->property_identifier);
}

TEST(getInventoryInformation, testBadEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_req));

	auto rc = encode_get_inventory_information_req(0, 0, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(getInventoryInformation, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_INVENTORY_INFORMATION,   // command
	    1,				     // data size
	    0xab			     // property_identifier
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	size_t msg_len = requestMsg.size();

	uint8_t property_identifier = 0;
	auto rc = decode_get_inventory_information_req(request, msg_len,
						       &property_identifier);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0xab, property_identifier);
}

TEST(getInventoryInformation, testGoodEncodeResponse)
{
	std::vector<uint8_t> board_part_number{'1', '2', '3', '4'};

	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 NSM_RESPONSE_CONVENTION_LEN +
					 board_part_number.size());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t *inventory_information = board_part_number.data();
	uint16_t data_size = board_part_number.size();
	uint16_t reason_code = 0;

	auto rc = encode_get_inventory_information_resp(
	    0, NSM_SUCCESS, reason_code, data_size, inventory_information,
	    response);

	struct nsm_get_inventory_information_resp *resp =
	    reinterpret_cast<struct nsm_get_inventory_information_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_INVENTORY_INFORMATION, resp->hdr.command);
	EXPECT_EQ(data_size, le16toh(resp->hdr.data_size));
	EXPECT_EQ('1', resp->inventory_information[0]);
	EXPECT_EQ('2', resp->inventory_information[1]);
	EXPECT_EQ('3', resp->inventory_information[2]);
	EXPECT_EQ('4', resp->inventory_information[3]);
}

TEST(getInventoryInformation, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_INVENTORY_INFORMATION,   // command
	    0,				     // completion code
	    0,
	    0,
	    4,
	    0, // data size
	    '1',
	    '2',
	    '3',
	    '4'};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	uint8_t inventory_information[4];

	auto rc = decode_get_inventory_information_resp(
	    response, msg_len, &cc, &reason_code, &data_size,
	    inventory_information);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(4, data_size);
	EXPECT_EQ('1', inventory_information[0]);
	EXPECT_EQ('2', inventory_information[1]);
	EXPECT_EQ('3', inventory_information[2]);
	EXPECT_EQ('4', inventory_information[3]);
}

TEST(getTemperature, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t sensor_id = 0;

	auto rc = encode_get_temperature_reading_req(0, sensor_id, request);

	struct nsm_get_temperature_reading_req *req =
	    reinterpret_cast<struct nsm_get_temperature_reading_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_TEMPERATURE_READING, req->hdr.command);
	EXPECT_EQ(sizeof(sensor_id), req->hdr.data_size);
	EXPECT_EQ(sensor_id, req->sensor_id);
}

TEST(getTemperature, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_TEMPERATURE_READING,     // command
	    1,				     // data size
	    1				     // sensor_id
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	uint8_t sensor_id = 0;
	auto rc =
	    decode_get_temperature_reading_req(request, msg_len, &sensor_id);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(sensor_id, 1);
}

TEST(encode_get_temperature_reading_resp, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	real32_t temperature_reading = 12.34;
	uint16_t reasonCode = 0;

	auto rc = encode_get_temperature_reading_resp(
	    0, NSM_SUCCESS, reasonCode, temperature_reading, response);

	struct nsm_get_temperature_reading_resp *resp =
	    reinterpret_cast<struct nsm_get_temperature_reading_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_TEMPERATURE_READING, resp->hdr.command);
	EXPECT_EQ(sizeof(resp->reading), le16toh(resp->hdr.data_size));

	uint32_t data = 0;
	memcpy(&data, &resp->reading, sizeof(uint32_t));
	data = le32toh(data);
	real32_t reading = 0;
	memcpy(&reading, &data, sizeof(uint32_t));
	EXPECT_FLOAT_EQ(temperature_reading, reading);
}

TEST(decode_get_temperature_reading_resp, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_TEMPERATURE_READING,     // command
	    0,				     // completion code
	    0,
	    0,
	    4,
	    0, // data size
	    0xa4,
	    0x70,
	    0x45,
	    0x41 // temperature reading=12.34
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	real32_t temperature_reading = 0;

	auto rc = decode_get_temperature_reading_resp(
	    response, msg_len, &cc, &reasonCode, &temperature_reading);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_FLOAT_EQ(temperature_reading, 12.34);
}

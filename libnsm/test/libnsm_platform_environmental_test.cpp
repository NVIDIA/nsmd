#include "base.h"
#include "platform-environmental.h"
#include <gtest/gtest.h>

TEST(getInventoryInformation, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_req));

	uint32_t transfer_handle = 0xaabbaabb;

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc =
	    encode_get_inventory_information_req(0, transfer_handle, request);

	struct nsm_get_inventory_information_req *req =
	    reinterpret_cast<struct nsm_get_inventory_information_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_INVENTORY_INFORMATION, req->command);
	EXPECT_EQ(6, req->data_size);
	EXPECT_EQ(0, req->arg1);
	EXPECT_EQ(0, req->arg2);
	EXPECT_EQ(transfer_handle, le32toh(req->transfer_handle));
}

TEST(getInventoryInformation, testBadEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_req));

	auto rc = encode_get_inventory_information_req(0, 0, nullptr);

	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

TEST(getInventoryInformation, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x7e, // message type( VDM-PCI)
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_INVENTORY_INFORMATION,   // command
	    6,				     // data size
	    0,				     // arg1
	    0,				     // arg2
	    0x78,
	    0x56,
	    0x34,
	    0x12 // transfer handle = 0x12345678
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	size_t msg_len = requestMsg.size();

	uint32_t transfer_handle = 0;
	auto rc = decode_get_inventory_information_req(request, msg_len,
						       &transfer_handle);

	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(0x12345678, le32toh(transfer_handle));
}

TEST(getInventoryInformation, testGoodEncodeResponse)
{
	std::vector<uint8_t> property_record1{
	    0x01,	 // property ID
	    NvU16,	 // data type
	    0x02,  0x00, // data length=2
	    0x34,  0x12	 // NvU16=0x1234
	};

	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
	    property_record1.size());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint32_t next_transfer_handle = 0x12345678;

	uint8_t *inventory_information = property_record1.data();
	uint32_t inventory_information_len = property_record1.size();
	uint16_t data_size =
	    sizeof(next_transfer_handle) + inventory_information_len;

	auto rc = encode_get_inventory_information_resp(
	    0, NSM_SUCCESS, next_transfer_handle, inventory_information,
	    inventory_information_len, response);

	struct nsm_get_inventory_information_resp *resp =
	    reinterpret_cast<struct nsm_get_inventory_information_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_INVENTORY_INFORMATION, resp->command);
	EXPECT_EQ(data_size, le16toh(resp->data_size));
	EXPECT_EQ(next_transfer_handle, le32toh(resp->next_transfer_handle));
}

TEST(getInventoryInformation, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x7e, // message type( VDM-PCI)
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_INVENTORY_INFORMATION,   // command
	    0,				     // completion code
	    10, 0,			     // data size
	    0x78, 0x56, 0x34, 0x12,	     // next transfer handle
					     // property record1
	    PCIE_VENDOR_ID,		     // property id
	    NvU16,			     // data type
	    2, 0,			     // data length
	    0xde, 0x10			     // PCI VID
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	size_t msg_len = responseMsg.size();

	uint8_t cc = 0;
	uint16_t data_size = 0;
	uint32_t next_transfer_handle = 0;

	uint8_t inventory_information[6];

	auto rc = decode_get_inventory_information_resp(
	    response, msg_len, &cc, &data_size, &next_transfer_handle,
	    inventory_information);

	uint8_t property_id = *((uint8_t *)inventory_information);
	uint8_t property_type = *((uint8_t *)inventory_information + 1);
	uint16_t property_length =
	    le16toh(*((uint16_t *)(inventory_information + 2)));
	uint16_t pic_vid = le16toh(*((uint16_t *)(inventory_information + 4)));

	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(0x12345678, le32toh(next_transfer_handle));
	EXPECT_EQ(property_id, PCIE_VENDOR_ID);
	EXPECT_EQ(property_type, NvU16);
	EXPECT_EQ(property_length, 2);
	EXPECT_EQ(pic_vid, 0x10de);
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

	EXPECT_EQ(rc, NSM_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_TEMPERATURE_READING, req->command);
	EXPECT_EQ(2, req->data_size);
	EXPECT_EQ(sensor_id, req->arg1);
	EXPECT_EQ(0, req->arg2);
}

TEST(getTemperature, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x7e, // message type( VDM-PCI)
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_TEMPERATURE_READING,     // command
	    2,				     // data size
	    1,				     // arg1=sensor_id
	    0				     // arg2
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	uint8_t sensor_id = 0;
	auto rc =
	    decode_get_temperature_reading_req(request, msg_len, &sensor_id);

	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(sensor_id, 1);
}

TEST(encode_get_temperature_reading_resp, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	real32_t temperature_reading = 12.34;

	auto rc = encode_get_temperature_reading_resp(
	    0, NSM_SUCCESS, temperature_reading, response);

	struct nsm_get_temperature_reading_resp *resp =
	    reinterpret_cast<struct nsm_get_temperature_reading_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_TEMPERATURE_READING, resp->command);
	EXPECT_EQ(sizeof(resp->reading), le16toh(resp->data_size));

	uint32_t data = le32toh(resp->reading);
	real32_t reading = 0;
	memcpy(&reading, &data, 4);
	EXPECT_EQ(temperature_reading, reading);
}

TEST(decode_get_temperature_reading_resp, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x7e, // message type( VDM-PCI)
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL, // NVIDIA_MSG_TYPE
	    NSM_GET_TEMPERATURE_READING,     // command
	    0,				     // completion code
	    4,
	    0, // data size
	    0xa4,
	    0x70,
	    0x45,
	    0x41 // temperature reading=12.34
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = 0;
	real32_t temperature_reading = 0;

	auto rc = decode_get_temperature_reading_resp(response, msg_len, &cc,
						      &temperature_reading);

	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_FLOAT_EQ(temperature_reading, 12.34);
}

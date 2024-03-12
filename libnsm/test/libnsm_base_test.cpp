#include "base.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::ElementsAre;

TEST(PackNSMMessage, BadPathTest)
{
	struct nsm_header_info hdr;
	struct nsm_header_info *hdr_ptr = NULL;

	struct nsm_msg_hdr msg {
	};

	// header information pointer is NULL
	auto rc = pack_nsm_header(hdr_ptr, &msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// message pointer is NULL
	rc = pack_nsm_header(&hdr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// header information pointer and message pointer is NULL
	rc = pack_nsm_header(hdr_ptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// Instance ID out of range
	hdr.nsm_msg_type = NSM_REQUEST;
	hdr.instance_id = 32;
	rc = pack_nsm_header(&hdr, &msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(UnpackNSMMessage, BadPathTest)
{
	struct nsm_header_info hdr;

	// message pointer is NULL
	auto rc = unpack_nsm_header(nullptr, &hdr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	struct nsm_msg_hdr msg {
	};

	// hdr pointer is NULL
	rc = unpack_nsm_header(&msg, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(ping, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_common_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_ping_req(0, request);

	struct nsm_common_req *req =
	    reinterpret_cast<struct nsm_common_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_PING, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(ping, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t instance_id = 0x12;
	uint16_t reason_code = 0;
	auto rc = encode_ping_resp(instance_id, reason_code, response);

	struct nsm_common_resp *resp =
	    reinterpret_cast<struct nsm_common_resp *>(response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(instance_id, response->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_PING, resp->command);
	EXPECT_EQ(0, le16toh(resp->data_size));
}

TEST(ping, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x00, // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_PING,				  // command
	    0,					  // completion code
	    0,
	    0 // data size
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	auto rc = decode_ping_resp(response, msg_len, &cc, &reason_code);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(getSupportedNvidiaMessageTypes, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_common_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_get_supported_nvidia_message_types_req(0, request);

	struct nsm_common_req *req =
	    reinterpret_cast<struct nsm_common_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SUPPORTED_NVIDIA_MESSAGE_TYPES, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(getSupportedNvidiaMessageTypes, testGoodEncodeResponse)
{

	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_supported_nvidia_message_types_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t instance_id = 0x12;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	std::vector<uint8_t> types{0xf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				   0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				   0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				   0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

	auto rc = encode_get_supported_nvidia_message_types_resp(
	    instance_id, cc, reason_code, (bitfield8_t *)types.data(),
	    response);

	struct nsm_get_supported_nvidia_message_types_resp *resp =
	    reinterpret_cast<
		struct nsm_get_supported_nvidia_message_types_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(instance_id, response->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SUPPORTED_NVIDIA_MESSAGE_TYPES, resp->hdr.command);
	EXPECT_EQ(32, le16toh(resp->hdr.data_size));

	uint8_t responseTypes[SUPPORTED_MSG_TYPE_DATA_SIZE];
	memcpy(responseTypes, resp->supported_nvidia_message_types,
	       SUPPORTED_MSG_TYPE_DATA_SIZE);
	EXPECT_THAT(responseTypes,
		    ElementsAre(0xf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0));
}

TEST(getSupportedNvidiaMessageTypes, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x00, // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_SUPPORTED_NVIDIA_MESSAGE_TYPES,	  // command
	    0,					  // completion code
	    32,
	    0, // data size
	    0xf,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0 // types
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint8_t responseTypes[SUPPORTED_MSG_TYPE_DATA_SIZE];

	auto rc = decode_get_supported_nvidia_message_types_resp(
	    response, msg_len, &cc, &reason_code, (bitfield8_t *)responseTypes);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_THAT(responseTypes,
		    ElementsAre(0xf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0));
}

TEST(getSupportedCommandCodes, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 0x12;
	uint8_t msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;
	auto rc = encode_get_supported_command_codes_req(instance_id, msg_type,
							 request);

	struct nsm_get_supported_command_codes_req *req =
	    reinterpret_cast<struct nsm_get_supported_command_codes_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(instance_id, request->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SUPPORTED_COMMAND_CODES, req->hdr.command);
	EXPECT_EQ(1, req->hdr.data_size);
	EXPECT_EQ(msg_type, req->nvidia_message_type);
}

TEST(getSupportedCommandCodes, testGoodEncodeResponse)
{

	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t instance_id = 0x12;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	std::vector<uint8_t> command_codes{
	    0xf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

	auto rc = encode_get_supported_command_codes_resp(
	    instance_id, cc, reason_code, (bitfield8_t *)command_codes.data(),
	    response);

	struct nsm_get_supported_command_codes_resp *resp =
	    reinterpret_cast<struct nsm_get_supported_command_codes_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(instance_id, response->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SUPPORTED_COMMAND_CODES, resp->hdr.command);
	EXPECT_EQ(32, le16toh(resp->hdr.data_size));

	uint8_t responseCodes[SUPPORTED_COMMAND_CODE_DATA_SIZE];
	memcpy(responseCodes, resp->supported_command_codes,
	       SUPPORTED_COMMAND_CODE_DATA_SIZE);
	EXPECT_THAT(responseCodes,
		    ElementsAre(0xf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0));
}

TEST(getSupportedCommandCodes, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x00, // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_SUPPORTED_COMMAND_CODES,	  // command
	    0,					  // completion code
	    32,
	    0, // data size
	    0xf,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0 // codes
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint8_t responseCodes[SUPPORTED_COMMAND_CODE_DATA_SIZE];

	auto rc = decode_get_supported_command_codes_resp(
	    response, msg_len, &cc, &reason_code, (bitfield8_t *)responseCodes);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_THAT(responseCodes,
		    ElementsAre(0xf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0));
}

TEST(queryDeviceIdentification, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 0x12;
	auto rc =
	    encode_nsm_query_device_identification_req(instance_id, request);

	struct nsm_common_req *req =
	    reinterpret_cast<struct nsm_common_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(instance_id, request->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_DEVICE_IDENTIFICATION, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(queryDeviceIdentification, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t instance_id = 0x12;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint8_t device_identification = NSM_DEV_ID_GPU;
	uint8_t device_instance = 1;

	auto rc = encode_query_device_identification_resp(
	    instance_id, cc, reason_code, device_identification,
	    device_instance, response);

	struct nsm_query_device_identification_resp *resp =
	    reinterpret_cast<struct nsm_query_device_identification_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(instance_id, response->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_DEVICE_IDENTIFICATION, resp->hdr.command);
	EXPECT_EQ(2, le16toh(resp->hdr.data_size));
	EXPECT_EQ(device_identification, resp->device_identification);
	EXPECT_EQ(device_instance, resp->instance_id);
}

TEST(queryDeviceIdentification, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x00, // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_QUERY_DEVICE_IDENTIFICATION,	  // command
	    0,					  // completion code
	    2,
	    0,		    // data size
	    NSM_DEV_ID_GPU, // device _identification
	    1		    // instance_id
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint8_t device_identification = 0;
	uint8_t device_instance = 0;

	auto rc = decode_query_device_identification_resp(
	    response, msg_len, &cc, &reason_code, &device_identification,
	    &device_instance);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(NSM_DEV_ID_GPU, device_identification);
	EXPECT_EQ(1, device_instance);
}

TEST(encodeReasonCode, testGoodEncodeReasonCode)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_non_success_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;

	auto rc = encode_reason_code(cc, reasonCode, NSM_PING, response);

	struct nsm_common_non_success_resp *resp =
	    reinterpret_cast<struct nsm_common_non_success_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_ERROR, resp->completion_code);
	EXPECT_EQ(NSM_PING, resp->command);
	EXPECT_EQ(ERR_NULL, le16toh(resp->reason_code));
}

TEST(encodeReasonCode, testBadEncodeReasonCode)
{
	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;

	auto rc = encode_reason_code(cc, reasonCode, NSM_PING, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(decodeReasonCodeCC, testGoodDecodeReasonCode)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x00, // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_QUERY_DEVICE_IDENTIFICATION,	  // command
	    0x01, // completion code != NSM_SUCCESS
	    0x00, // reason code
	    0x00};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reason_code = ERR_NULL;

	auto rc =
	    decode_reason_code_and_cc(response, msg_len, &cc, &reason_code);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_ERROR);
	EXPECT_EQ(reason_code, 0x0000);
}

TEST(decodeReasonCodeCC, testGoodDecodeCompletionCode)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x00, // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_QUERY_DEVICE_IDENTIFICATION,	  // command
	    0x00, // completion code = NSM_SUCCESS
	    0x00, // reason code
	    0x02};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reason_code = ERR_NULL;

	auto rc =
	    decode_reason_code_and_cc(response, msg_len, &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reason_code, ERR_NULL);
}

TEST(decodeReasonCode, testBadDecodeReasonCode)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x00, // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_QUERY_DEVICE_IDENTIFICATION,	  // command
	    0x01,				  // completion code
	    0x00,				  // reason code
	    0x00};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;

	auto rc =
	    decode_reason_code_and_cc(nullptr, msg_len, &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc =
	    decode_reason_code_and_cc(response, msg_len, nullptr, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_reason_code_and_cc(response, msg_len, &cc, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_reason_code_and_cc(
	    response, msg_len - 2, &cc,
	    &reason_code); // sending msg len less then expected
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
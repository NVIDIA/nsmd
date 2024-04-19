#include "base.h"
#include "pci-links.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

TEST(queryScalarGroupTelemetryV1, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_query_scalar_group_telemetry_v1_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t device_id = 0;
	uint8_t group_index = 0;

	auto rc = encode_query_scalar_group_telemetry_v1_req(
	    0, device_id, group_index, request);

	struct nsm_query_scalar_group_telemetry_v1_req *req =
	    (struct nsm_query_scalar_group_telemetry_v1_req *)request->payload;

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, req->hdr.command);
	EXPECT_EQ(2 * sizeof(uint8_t), req->hdr.data_size);
	EXPECT_EQ(device_id, req->device_id);
	EXPECT_EQ(group_index, req->group_index);
}

TEST(queryScalarGroupTelemetryV1, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x80,	       // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    2,					 // data size
	    0,					 // device_id
	    0					 // group_index
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	size_t msg_len = requestMsg.size();

	uint8_t device_id = 0;
	uint8_t group_index = 1;
	auto rc = decode_query_scalar_group_telemetry_v1_req(
	    request, msg_len, &device_id, &group_index);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, device_id);
	EXPECT_EQ(0, group_index);
}

TEST(queryScalarGroupTelemetryV1Group0, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	struct nsm_query_scalar_group_telemetry_group_0 data;
	data.pci_vendor_id = 3;
	data.pci_device_id = 3;
	data.pci_subsystem_vendor_id = 3;
	data.pci_subsystem_device_id = 3;
	struct nsm_query_scalar_group_telemetry_group_0 data_test = data;

	auto rc = encode_query_scalar_group_telemetry_v1_group0_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);

	struct nsm_query_scalar_group_telemetry_v1_group_0_resp *resp =
	    reinterpret_cast<
		struct nsm_query_scalar_group_telemetry_v1_group_0_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, resp->hdr.command);
	EXPECT_EQ(sizeof(nsm_query_scalar_group_telemetry_group_0),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(data_test.pci_vendor_id, le32toh(data.pci_vendor_id));
	EXPECT_EQ(data_test.pci_device_id, le32toh(data.pci_device_id));
	EXPECT_EQ(data_test.pci_subsystem_vendor_id,
		  le32toh(data.pci_subsystem_vendor_id));
	EXPECT_EQ(data_test.pci_subsystem_vendor_id,
		  le32toh(data.pci_subsystem_vendor_id));
}

TEST(queryScalarGroupTelemetryV1Group0, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    16,
	    0 // data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_0 *>(
		data_byte.data());
	auto data_test = data;
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	auto rc = decode_query_scalar_group_telemetry_v1_group0_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_size, 16);
	EXPECT_EQ(le32toh(data_test->pci_vendor_id), data->pci_vendor_id);
	EXPECT_EQ(le32toh(data_test->pci_device_id), data->pci_device_id);
	EXPECT_EQ(le32toh(data_test->pci_subsystem_vendor_id),
		  data->pci_subsystem_vendor_id);
	EXPECT_EQ(le32toh(data_test->pci_subsystem_device_id),
		  data->pci_subsystem_device_id);
}

TEST(queryScalarGroupTelemetryV1Group0, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    15, // incorrect data size
	    0	// data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_0 *>(
		data_byte.data());
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_query_scalar_group_telemetry_v1_group0_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group0_resp(
	    response, msg_len, NULL, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group0_resp(
	    response, msg_len, &cc, NULL, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group0_resp(
	    response, msg_len - data_byte.size(), &cc, &data_size, &reason_code,
	    data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_scalar_group_telemetry_v1_group0_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(queryScalarGroupTelemetryV1Group1, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	struct nsm_query_scalar_group_telemetry_group_1 data;
	data.negotiated_link_speed = 3;
	data.negotiated_link_width = 3;
	data.target_link_speed = 3;
	data.max_link_speed = 3;
	data.max_link_width = 3;
	struct nsm_query_scalar_group_telemetry_group_1 data_test = data;

	auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);

	struct nsm_query_scalar_group_telemetry_v1_group_1_resp *resp =
	    reinterpret_cast<
		struct nsm_query_scalar_group_telemetry_v1_group_1_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, resp->hdr.command);
	EXPECT_EQ(sizeof(nsm_query_scalar_group_telemetry_group_1),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(data_test.negotiated_link_speed,
		  le32toh(data.negotiated_link_speed));
}

TEST(queryScalarGroupTelemetryV1Group1, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x09, 0x08, 0x07,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    20,
	    0 // data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_1 *>(
		data_byte.data());
	auto data_test = data;
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	auto rc = decode_query_scalar_group_telemetry_v1_group1_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_size, 20);
	EXPECT_EQ(le32toh(data_test->negotiated_link_speed),
		  data->negotiated_link_speed);
}

TEST(queryScalarGroupTelemetryV1Group1, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x09, 0x08, 0x07,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    14, // incorrect data size
	    0	// data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_1 *>(
		data_byte.data());
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_query_scalar_group_telemetry_v1_group1_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group1_resp(
	    response, msg_len, NULL, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group1_resp(
	    response, msg_len, &cc, NULL, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group1_resp(
	    response, msg_len - data_byte.size(), &cc, &data_size, &reason_code,
	    data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_scalar_group_telemetry_v1_group1_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(queryScalarGroupTelemetryV1Group2, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_scalar_group_telemetry_v1_group_2_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	struct nsm_query_scalar_group_telemetry_group_2 data;
	data.non_fatal_errors = 1111;
	data.fatal_errors = 2222;
	data.unsupported_request_count = 3333;
	data.correctable_errors = 4444;

	struct nsm_query_scalar_group_telemetry_group_2 data_test = data;
	auto rc = encode_query_scalar_group_telemetry_v1_group2_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);

	struct nsm_query_scalar_group_telemetry_v1_group_2_resp *resp =
	    reinterpret_cast<
		struct nsm_query_scalar_group_telemetry_v1_group_2_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, resp->hdr.command);
	EXPECT_EQ(sizeof(nsm_query_scalar_group_telemetry_group_2),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(data_test.correctable_errors,
		  le32toh(data.correctable_errors));
}

TEST(queryScalarGroupTelemetryV1Group2, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    16,
	    0 // data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_2 *>(
		data_byte.data());
	auto data_test = data;
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	auto rc = decode_query_scalar_group_telemetry_v1_group2_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_size, 16);
	EXPECT_EQ(le32toh(data_test->correctable_errors),
		  data->correctable_errors);
}

TEST(queryScalarGroupTelemetryV1Group2, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    14, // incorrect data size
	    0	// data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_2 *>(
		data_byte.data());
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_query_scalar_group_telemetry_v1_group2_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group2_resp(
	    response, msg_len, NULL, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group2_resp(
	    response, msg_len, &cc, NULL, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group2_resp(
	    response, msg_len - data_byte.size(), &cc, &data_size, &reason_code,
	    data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_scalar_group_telemetry_v1_group2_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(queryScalarGroupTelemetryV1Group3, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_scalar_group_telemetry_v1_group_3_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint16_t reason_code = ERR_NULL;
	struct nsm_query_scalar_group_telemetry_group_3 data;
	data.L0ToRecoveryCount = 8769;
	struct nsm_query_scalar_group_telemetry_group_3 data_test = data;
	auto rc = encode_query_scalar_group_telemetry_v1_group3_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);

	struct nsm_query_scalar_group_telemetry_v1_group_3_resp *resp =
	    reinterpret_cast<
		struct nsm_query_scalar_group_telemetry_v1_group_3_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, resp->hdr.command);
	EXPECT_EQ(sizeof(nsm_query_scalar_group_telemetry_group_3),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(data_test.L0ToRecoveryCount, le32toh(data.L0ToRecoveryCount));
}

TEST(queryScalarGroupTelemetryV1Group3, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01,
	    0x00,
	    0x00,
	    0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    4,
	    0 // data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_3 *>(
		data_byte.data());
	auto data_test = data;
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	auto rc = decode_query_scalar_group_telemetry_v1_group3_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_size, 4);
	EXPECT_EQ(le32toh(data_test->L0ToRecoveryCount),
		  data->L0ToRecoveryCount);
}

TEST(queryScalarGroupTelemetryV1Group3, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01,
	    0x00,
	    0x00,
	    0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    3, // wrong data size
	    0  // data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_3 *>(
		data_byte.data());

	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_query_scalar_group_telemetry_v1_group3_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group3_resp(
	    response, msg_len, NULL, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group3_resp(
	    response, msg_len, &cc, NULL, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group3_resp(
	    response, msg_len - data_byte.size(), &cc, &data_size, &reason_code,
	    data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_scalar_group_telemetry_v1_group3_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(queryScalarGroupTelemetryV1Group4, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_scalar_group_telemetry_v1_group_4_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint16_t reason_code = ERR_NULL;

	struct nsm_query_scalar_group_telemetry_group_4 data;
	data.recv_err_cnt = 100;
	data.NAK_recv_cnt = 200;
	data.NAK_sent_cnt = 300;
	data.bad_TLP_cnt = 400;
	data.replay_rollover_cnt = 500;
	data.FC_timeout_err_cnt = 600;
	data.replay_cnt = 700;

	struct nsm_query_scalar_group_telemetry_group_4 data_test = data;
	auto rc = encode_query_scalar_group_telemetry_v1_group4_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);

	struct nsm_query_scalar_group_telemetry_v1_group_4_resp *resp =
	    reinterpret_cast<
		struct nsm_query_scalar_group_telemetry_v1_group_4_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, resp->hdr.command);
	EXPECT_EQ(sizeof(nsm_query_scalar_group_telemetry_group_4),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(data_test.recv_err_cnt, le32toh(data.recv_err_cnt));
}

TEST(queryScalarGroupTelemetryV1Group4, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    28,
	    0 // data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_4 *>(
		data_byte.data());
	auto data_test = data;
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	auto rc = decode_query_scalar_group_telemetry_v1_group4_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);

	EXPECT_EQ(data_size, 28);
	EXPECT_EQ(le32toh(data_test->bad_TLP_cnt), data->bad_TLP_cnt);
}

TEST(queryScalarGroupTelemetryV1Group4, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    27, // wrong data size
	    0	// data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_4 *>(
		data_byte.data());
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_query_scalar_group_telemetry_v1_group4_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group4_resp(
	    response, msg_len, NULL, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group4_resp(
	    response, msg_len, &cc, NULL, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group4_resp(
	    response, msg_len - data_byte.size(), &cc, &data_size, &reason_code,
	    data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_scalar_group_telemetry_v1_group4_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(queryScalarGroupTelemetryV1Group5, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_scalar_group_telemetry_v1_group_5_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint16_t reason_code = ERR_NULL;
	struct nsm_query_scalar_group_telemetry_group_5 data;
	data.PCIeTXBytes = 8769000;
	data.PCIeRXBytes = 876654;

	struct nsm_query_scalar_group_telemetry_group_5 data_test = data;
	auto rc = encode_query_scalar_group_telemetry_v1_group5_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);

	struct nsm_query_scalar_group_telemetry_v1_group_5_resp *resp =
	    reinterpret_cast<
		struct nsm_query_scalar_group_telemetry_v1_group_5_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, resp->hdr.command);
	EXPECT_EQ(sizeof(nsm_query_scalar_group_telemetry_group_5),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(data_test.PCIeTXBytes, le32toh(data.PCIeTXBytes));
}

TEST(queryScalarGroupTelemetryV1Group5, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    8,
	    0 // data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_5 *>(
		data_byte.data());
	auto data_test = data;
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	auto rc = decode_query_scalar_group_telemetry_v1_group5_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_size, 8);
	EXPECT_EQ(le32toh(data_test->PCIeRXBytes), data->PCIeRXBytes);
}

TEST(queryScalarGroupTelemetryV1Group5, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    7,
	    0 // data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_5 *>(
		data_byte.data());
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_query_scalar_group_telemetry_v1_group5_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group5_resp(
	    response, msg_len, NULL, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group5_resp(
	    response, msg_len, &cc, NULL, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group5_resp(
	    response, msg_len - data_byte.size(), &cc, &data_size, &reason_code,
	    data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_scalar_group_telemetry_v1_group5_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(queryScalarGroupTelemetryV1Group6, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_query_scalar_group_telemetry_v1_group_6_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	struct nsm_query_scalar_group_telemetry_group_6 data;
	data.invalid_flit_counter = 3;
	data.ltssm_state = 3;
	struct nsm_query_scalar_group_telemetry_group_6 data_test = data;

	auto rc = encode_query_scalar_group_telemetry_v1_group6_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);

	struct nsm_query_scalar_group_telemetry_v1_group_6_resp *resp =
	    reinterpret_cast<
		struct nsm_query_scalar_group_telemetry_v1_group_6_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_PCI_LINK, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, resp->hdr.command);
	EXPECT_EQ(sizeof(nsm_query_scalar_group_telemetry_group_6),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(data_test.invalid_flit_counter,
		  le32toh(data.invalid_flit_counter));
	EXPECT_EQ(data_test.ltssm_state, le32toh(data.ltssm_state));
}

TEST(queryScalarGroupTelemetryV1Group6, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    8,
	    0 // data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_6 *>(
		data_byte.data());
	auto data_test = data;
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	auto rc = decode_query_scalar_group_telemetry_v1_group6_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_size, 8);
	EXPECT_EQ(le32toh(data_test->invalid_flit_counter),
		  data->invalid_flit_counter);
	EXPECT_EQ(le32toh(data_test->ltssm_state), data->ltssm_state);
}

TEST(queryScalarGroupTelemetryV1Group6, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x09, 0x08, 0x07,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PCI_LINK, // NVIDIA_MSG_TYPE
	    NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, // command
	    0,					 // completion code
	    0,
	    0,
	    7, // incorrect data size
	    0  // data size
	};
	auto data =
	    reinterpret_cast<nsm_query_scalar_group_telemetry_group_6 *>(
		data_byte.data());
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_query_scalar_group_telemetry_v1_group6_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group6_resp(
	    response, msg_len, NULL, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group6_resp(
	    response, msg_len, &cc, NULL, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_query_scalar_group_telemetry_v1_group6_resp(
	    response, msg_len - data_byte.size(), &cc, &data_size, &reason_code,
	    data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_query_scalar_group_telemetry_v1_group6_resp(
	    response, msg_len, &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
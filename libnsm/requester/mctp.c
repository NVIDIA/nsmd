/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
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




#include "mctp.h"
#include "base.h"

#include "stdio.h"
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

nsm_requester_rc_t nsm_open()
{
	int fd = -1;
	int rc = -1;

	fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (-1 == fd) {
		return fd;
	}

	const char path[] = "\0mctp-mux";
	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	memcpy(addr.sun_path, path, sizeof(path) - 1);
	rc = connect(fd, (struct sockaddr *)&addr,
		     sizeof(path) + sizeof(addr.sun_family) - 1);
	if (-1 == rc) {
		close(fd);
		return -1;
	}
	uint8_t mctpMsgType = MCTP_MSG_TYPE_PCI_VDM;
	rc = write(fd, &mctpMsgType, sizeof(mctpMsgType));
	if (-1 == rc) {
		close(fd);
		return -1;
	}

	return fd;
}

static nsm_requester_rc_t mctp_recv(mctp_eid_t eid, int mctp_fd,
				    uint8_t **nsm_resp_msg,
				    size_t *resp_msg_len)
{
	uint8_t msgTag = 0;
	uint8_t mctpMsgType = MCTP_MSG_TYPE_PCI_VDM;
	ssize_t min_len = sizeof(msgTag) + sizeof(eid) + sizeof(mctpMsgType) +
			  sizeof(struct nsm_msg_hdr);
	ssize_t length = recv(mctp_fd, NULL, 0, MSG_PEEK | MSG_TRUNC);
	if (length <= 0) {
		return NSM_REQUESTER_RECV_FAIL;
	} else if (length < min_len) {
		/* read and discard */
		uint8_t buf[length];
		(void)recv(mctp_fd, buf, length, 0);
		return NSM_REQUESTER_INVALID_RECV_LEN;
	} else {
		struct iovec iov[2];
		size_t mctp_prefix_len =
		    sizeof(msgTag) + sizeof(eid) + sizeof(mctpMsgType);
		uint8_t mctp_prefix[mctp_prefix_len];
		size_t nsm_len = length - mctp_prefix_len;
		iov[0].iov_len = mctp_prefix_len;
		iov[0].iov_base = mctp_prefix;
		*nsm_resp_msg = malloc(nsm_len);
		iov[1].iov_len = nsm_len;
		iov[1].iov_base = *nsm_resp_msg;
		struct msghdr msg = {0};
		msg.msg_iov = iov;
		msg.msg_iovlen = sizeof(iov) / sizeof(iov[0]);
		ssize_t bytes = recvmsg(mctp_fd, &msg, 0);

		if (length != bytes) {
			free(*nsm_resp_msg);
			return NSM_REQUESTER_INVALID_RECV_LEN;
		}

		if ((mctp_prefix[1] != eid) ||
		    (mctp_prefix[2] != mctpMsgType)) {
			free(*nsm_resp_msg);
			return NSM_REQUESTER_NOT_NSM_MSG;
		}
		*resp_msg_len = nsm_len;
		return NSM_REQUESTER_SUCCESS;
	}
}

nsm_requester_rc_t nsm_recv_any(mctp_eid_t eid, int mctp_fd,
				uint8_t **nsm_resp_msg, size_t *resp_msg_len)
{
	nsm_requester_rc_t rc =
	    mctp_recv(eid, mctp_fd, nsm_resp_msg, resp_msg_len);
	if (rc != NSM_REQUESTER_SUCCESS) {
		return rc;
	}

	struct nsm_msg_hdr *hdr = (struct nsm_msg_hdr *)(*nsm_resp_msg);
	if (hdr->request != 0 || hdr->datagram != 0) {
		free(*nsm_resp_msg);
		return NSM_REQUESTER_NOT_RESP_MSG;
	}

	uint8_t cc = 0;
	if (*resp_msg_len < (sizeof(struct nsm_msg_hdr) + sizeof(cc))) {
		free(*nsm_resp_msg);
		return NSM_REQUESTER_RESP_MSG_TOO_SMALL;
	}

	return NSM_REQUESTER_SUCCESS;
}

nsm_requester_rc_t nsm_recv(mctp_eid_t eid, int mctp_fd, uint8_t instance_id,
			    uint8_t **nsm_resp_msg, size_t *resp_msg_len)
{
	nsm_requester_rc_t rc =
	    nsm_recv_any(eid, mctp_fd, nsm_resp_msg, resp_msg_len);
	if (rc != NSM_REQUESTER_SUCCESS) {
		return rc;
	}

	struct nsm_msg_hdr *hdr = (struct nsm_msg_hdr *)(*nsm_resp_msg);
	if (hdr->instance_id != instance_id) {
		free(*nsm_resp_msg);
		return NSM_REQUESTER_INSTANCE_ID_MISMATCH;
	}

	return NSM_REQUESTER_SUCCESS;
}

nsm_requester_rc_t nsm_send_recv(mctp_eid_t eid, int mctp_fd,
				 const uint8_t *nsm_req_msg, size_t req_msg_len,
				 uint8_t **nsm_resp_msg, size_t *resp_msg_len)
{
	struct nsm_msg_hdr *hdr = (struct nsm_msg_hdr *)nsm_req_msg;

	if (hdr->request != 1 || hdr->datagram != 0) {
		return NSM_REQUESTER_NOT_REQ_MSG;
	}

	nsm_requester_rc_t rc =
	    nsm_send(eid, mctp_fd, nsm_req_msg, req_msg_len);
	if (rc != NSM_REQUESTER_SUCCESS) {
		return rc;
	}

	while (1) {
		rc = nsm_recv(eid, mctp_fd, hdr->instance_id, nsm_resp_msg,
			      resp_msg_len);
		if (rc == NSM_REQUESTER_SUCCESS) {
			break;
		}
	}

	return rc;
}

nsm_requester_rc_t nsm_send(mctp_eid_t eid, int mctp_fd,
			    const uint8_t *nsm_req_msg, size_t req_msg_len)
{
	uint8_t hdr[3] = {MCTP_MSG_TAG_REQ, eid, MCTP_MSG_TYPE_PCI_VDM};

	struct iovec iov[2];
	iov[0].iov_base = hdr;
	iov[0].iov_len = sizeof(hdr);
	iov[1].iov_base = (uint8_t *)nsm_req_msg;
	iov[1].iov_len = req_msg_len;

	struct msghdr msg = {0};
	msg.msg_iov = iov;
	msg.msg_iovlen = sizeof(iov) / sizeof(iov[0]);

	ssize_t rc = sendmsg(mctp_fd, &msg, 0);
	if (rc == -1) {
		return NSM_REQUESTER_SEND_FAIL;
	}
	return NSM_REQUESTER_SUCCESS;
}

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

#ifndef MCTP_H
#define MCTP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libmctp-externals.h>
#include <stddef.h>
#include <stdint.h>

#define MCTP_DEMUX_PREFIX 3 // tag + eid + mctp message type

typedef uint8_t mctp_eid_t;
#define MCTP_MSG_TYPE_PCI_VDM 0x7E
#define MCTP_MSG_TAG_REQ (MCTP_TAG_NSM | 1 << 3)

typedef enum nsm_requester_error_codes {
	NSM_REQUESTER_SUCCESS = 0,
	NSM_REQUESTER_OPEN_FAIL = -1,
	NSM_REQUESTER_NOT_NSM_MSG = -2,
	NSM_REQUESTER_NOT_RESP_MSG = -3,
	NSM_REQUESTER_NOT_REQ_MSG = -4,
	NSM_REQUESTER_RESP_MSG_TOO_SMALL = -5,
	NSM_REQUESTER_INSTANCE_ID_MISMATCH = -6,
	NSM_REQUESTER_SEND_FAIL = -7,
	NSM_REQUESTER_RECV_FAIL = -8,
	NSM_REQUESTER_INVALID_RECV_LEN = -9,
	NSM_REQUESTER_RECV_TIMEOUT = -10,
	NSM_REQUESTER_EID_MISMATCH = -11,
} nsm_requester_rc_t;

/**
 * @brief Connect to the MCTP socket and provide an fd to it. The fd can be
 *        used to pass as input to other APIs below, or can be polled.
 *
 * @return fd on success, nsm_requester_rc_t on error (errno may be set)
 */
nsm_requester_rc_t nsm_open();

/**
 * @brief Send a NSM request message. Wait for corresponding response message,
 *        which once received, is returned to the caller.
 *
 * @param[in] eid - destination MCTP eid
 * @param[in] mctp_fd - MCTP socket fd
 * @param[in] nsm_req_msg - caller owned pointer to NSM request msg
 * @param[in] req_msg_len - size of request msg
 * @param[out] nsm_resp_msg - *nsm_resp_msg will point to response msg,
 *             this function allocates memory, caller to free(*nsm_resp_msg) on
 *             success.
 * @param[out] resp_msg_len - caller owned pointer that will be made point to
 *             the size of the response msg.
 *
 * @return nsm_requester_rc_t (errno may be set)
 */
nsm_requester_rc_t nsm_send_recv(mctp_eid_t eid, int mctp_fd,
				 const uint8_t *nsm_req_msg, size_t req_msg_len,
				 uint8_t **nsm_resp_msg, size_t *resp_msg_len);

/**
 * @brief Send a NSM request message, don't wait for response. Essentially an
 *        async API. A user of this would typically have added the MCTP fd to an
 *        event loop for polling. Once there's data available, the user would
 *        invoke nsm_recv().
 *
 * @param[in] eid - destination MCTP eid
 * @param[in] mctp_fd - MCTP socket fd
 * @param[in] nsm_req_msg - caller owned pointer to NSM request msg
 * @param[in] req_msg_len - size of NSM request msg
 *
 * @return nsm_requester_rc_t (errno may be set)
 */
nsm_requester_rc_t nsm_send(mctp_eid_t eid, int mctp_fd,
			    const uint8_t *nsm_req_msg, size_t req_msg_len);

/**
 * @brief Read MCTP socket. If there's data available, return success only if
 *        data is a NSM response message that matches eid and instance_id.
 *
 * @param[in] eid - destination MCTP eid
 * @param[in] mctp_fd - MCTP socket fd
 * @param[in] instance_id - NSM instance id of previously sent NSM request msg
 * @param[out] nsm_resp_msg - *nsm_resp_msg will point to NSM response msg,
 *             this function allocates memory, caller to free(*nsm_resp_msg) on
 *             success.
 * @param[out] resp_msg_len - caller owned pointer that will be made point to
 *             the size of the NSM response msg.
 *
 * @return nsm_requester_rc_t (errno may be set). failure is returned even
 *         when data was read, but didn't match eid or instance_id.
 */
nsm_requester_rc_t nsm_recv(mctp_eid_t eid, int mctp_fd, uint8_t instance_id,
			    uint8_t **nsm_resp_msg, size_t *resp_msg_len);

/**
 * @brief Read MCTP socket. If there's data available, return success only if
 *        data is a NSM response message.
 *
 * @param[in] eid - destination MCTP eid
 * @param[in] mctp_fd - MCTP socket fd
 * @param[out] nsm_resp_msg - *nsm_resp_msg will point to NSM response msg,
 *             this function allocates memory, caller to free(*nsm_resp_msg) on
 *             success.
 * @param[out] resp_msg_len - caller owned pointer that will be made point to
 *             the size of the NSM response msg.
 *
 * @return nsm_requester_rc_t (errno may be set). failure is returned even
 *         when data was read, but wasn't a NSM response message
 */
nsm_requester_rc_t nsm_recv_any(mctp_eid_t eid, int mctp_fd,
				uint8_t **nsm_resp_msg, size_t *resp_msg_len);

#ifdef __cplusplus
}
#endif

#endif /* MCTP_H */

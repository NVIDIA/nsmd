// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#ifndef LIBNSM_FPGA_CONTROLLER_H
#define LIBNSM_FPGA_CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* FPGA MMIO payload codec: the 32-byte records of the mixed controller ABI. */

#define NSM_FPGA_RECORD_SIZE 32
#define NSM_FPGA_PROFILE_IBC2 0x32434249u
#define NSM_FPGA_MAX_SEQUENCE 0x7fffffffu
/* Reason code of completion 0x01 when the FPGA has no session for the frame */
#define NSM_FPGA_REASON_NO_SESSION 0xF002u

/* Data opcodes carried in header byte 4 */
enum nsm_fpga_opcode {
	NSM_FPGA_OP_READ32 = 0x01,
	NSM_FPGA_OP_WRITE32 = 0x02,
	NSM_FPGA_OP_BYTE = 0x20,
	NSM_FPGA_OP_XFER_BASE = 0x40, /* bounded I2C transaction family */
};

/* Session opcodes carried in control byte 4 */
enum nsm_fpga_control_opcode {
	NSM_FPGA_CTRL_ACTIVATE = 0x10,
	NSM_FPGA_CTRL_ACQUIRE = 0x11,
	NSM_FPGA_CTRL_RENEW = 0x12,
	NSM_FPGA_CTRL_RELEASE = 0x13,
	NSM_FPGA_CTRL_RECOVER = 0x14,
};

enum nsm_fpga_control_result {
	NSM_FPGA_CTRL_OK = 0,
	NSM_FPGA_CTRL_INVALID = 1,
	NSM_FPGA_CTRL_BUSY = 2,
	NSM_FPGA_CTRL_STALE = 3,
	NSM_FPGA_CTRL_UNSAFE = 4,
	NSM_FPGA_CTRL_DENIED = 5,
	NSM_FPGA_CTRL_FENCED = 6,
	NSM_FPGA_CTRL_EXHAUSTED = 7,
};

enum nsm_fpga_result {
	NSM_FPGA_RESULT_EMPTY = 0,
	NSM_FPGA_RESULT_PENDING = 1,
	NSM_FPGA_RESULT_SUCCESS = 2,
	NSM_FPGA_RESULT_TARGET_NACK = 3,
	NSM_FPGA_RESULT_FAULT = 4,
};

enum nsm_fpga_owner {
	NSM_FPGA_OWNER_NONE = 0,
	NSM_FPGA_OWNER_NATIVE = 1,
	NSM_FPGA_OWNER_NSM = 2,
};

/* Completion flag bits in data completion byte 5 */
#define NSM_FPGA_FLAG_OCCUPIED 0x01
#define NSM_FPGA_FLAG_PENDING 0x02
#define NSM_FPGA_FLAG_QUIESCENT 0x04
#define NSM_FPGA_FLAG_FAULTED 0x08
#define NSM_FPGA_FLAG_READ_VALID 0x10
#define NSM_FPGA_FLAG_BUS_HELD 0x20
#define NSM_FPGA_FLAG_BUS_RELEASED 0x40

/* Grant plus per-record sequence; every nonzero field is required. */
struct nsm_fpga_identity {
	uint64_t epoch;
	uint32_t session;
	uint32_t generation;
	uint32_t sequence;
};

struct nsm_fpga_control_completion {
	struct nsm_fpga_identity identity;
	uint8_t opcode;
	uint8_t result;
	uint8_t owner;
	bool occupied;
	bool pending;
	uint32_t argument;
};

struct nsm_fpga_word_completion {
	struct nsm_fpga_identity identity;
	uint8_t result;
	uint8_t flags;
	uint8_t owner;
	uint8_t opcode;
	uint32_t data;
	uint8_t fault_response;
	uint8_t fault_reason;
};

/* Classification of a completion read against the outstanding request */
enum nsm_fpga_match {
	NSM_FPGA_MATCH_SUCCESS = 0,
	NSM_FPGA_MATCH_PENDING = 1,
	NSM_FPGA_MATCH_FOREIGN = 2,
	NSM_FPGA_MATCH_EMPTY = 3,
	NSM_FPGA_MATCH_FAULT = 4,
	NSM_FPGA_MATCH_REJECTED = 5,
};

/** @brief Validate a grant identity: nonzero fields, sequence within range.
 *  @return true when the identity may appear in a record
 */
bool nsm_fpga_identity_valid(const struct nsm_fpga_identity *identity);

/** @brief Encode a session control record.
 *  @param[in] identity - ACQUIRE needs sequence 1, RENEW and RELEASE above 1
 *  @param[in] opcode - nsm_fpga_control_opcode
 *  @param[in] argument - RELEASE carries the last verified data sequence
 *  @param[out] record - 32 bytes
 *  @return nsm_sw_codes
 */
int nsm_fpga_encode_control(const struct nsm_fpga_identity *identity,
			    uint8_t opcode, uint32_t argument, uint8_t *record);

/** @brief Decode the control completion read after a control record.
 *  @return nsm_sw_codes, NSM_SW_ERROR_DATA when the record is malformed
 */
int nsm_fpga_decode_control_completion(
    const uint8_t *record, struct nsm_fpga_control_completion *completion);

/** @brief Classify a control completion against the request it answers. */
enum nsm_fpga_match
nsm_fpga_match_control(const struct nsm_fpga_control_completion *completion,
		       const struct nsm_fpga_identity *identity, uint8_t opcode,
		       uint32_t argument);

/** @brief Decode the completion read after a word header record.
 *  @return nsm_sw_codes, NSM_SW_ERROR_DATA when the envelope is malformed
 */
int nsm_fpga_decode_word_completion(
    const uint8_t *record, struct nsm_fpga_word_completion *completion);

/** @brief Classify a word completion against the request it answers. */
enum nsm_fpga_match
nsm_fpga_match_word(const struct nsm_fpga_word_completion *completion,
		    const struct nsm_fpga_identity *identity, uint8_t opcode);

/* Authority status flag bits in byte 5 of the status record */
#define NSM_FPGA_AUTH_INITIALIZED 0x01
#define NSM_FPGA_AUTH_AVAILABLE 0x02
#define NSM_FPGA_AUTH_QUARANTINED 0x04
#define NSM_FPGA_AUTH_GRANT_VALID 0x08
#define NSM_FPGA_AUTH_RING_ARMED 0x10
#define NSM_FPGA_AUTH_QUIESCENT 0x20
#define NSM_FPGA_AUTH_RELEASED 0x40

/* Read-only view of the bank authority: activated epoch, last grant, owner. */
struct nsm_fpga_authority_status {
	uint64_t epoch;
	uint32_t session;
	uint32_t generation;
	uint8_t owner;
	uint8_t flags;
};

/* Management session state of a bank, classified from its authority status.
   UNREACHABLE is for a status record that could not be read or decoded. */
enum nsm_fpga_session_state {
	NSM_FPGA_SESSION_UNREACHABLE = 0,
	NSM_FPGA_SESSION_UNINITIALIZED = 1,
	NSM_FPGA_SESSION_READY = 2,
	NSM_FPGA_SESSION_OWNED_BY_BMC = 3,
	NSM_FPGA_SESSION_OWNED_BY_HOST = 4,
	NSM_FPGA_SESSION_QUARANTINED = 5,
};

/** @brief Classify the authority status flags, owner and epoch. */
enum nsm_fpga_session_state
nsm_fpga_session_classify(const struct nsm_fpga_authority_status *status);

/** @brief Decode the authority status record read at the status address.
 *  @return nsm_sw_codes, NSM_SW_ERROR_DATA when the record is malformed
 */
int nsm_fpga_decode_authority_status(const uint8_t *record,
				     struct nsm_fpga_authority_status *status);

/** @brief Short name of a control result for logs. */
const char *nsm_fpga_control_result_name(uint8_t result);

/** @brief True when a new session may be acquired on this bank.
 *
 *  Requires an activated epoch, the bank available and not quarantined.
 *  nsmd activates only a bank nobody activated since its controller came
 *  up, and recovers only its own lapsed lease.
 */
bool nsm_fpga_authority_acquirable(
    const struct nsm_fpga_authority_status *status);

/** @brief True when the bank holds nsmd's own lapsed lease: quarantined,
 *  quiescent and released with nsmd as the last owner. Only such a bank may
 *  be recovered by nsmd; a held, faulted or host owned bank never is.
 */
bool nsm_fpga_authority_recoverable(
    const struct nsm_fpga_authority_status *status);

/** @brief True when the bank has never been activated since its controller
 *  came up: initialized clear, not quarantined, no grant, no owner and a zero
 *  epoch. Only such a bank may be activated by nsmd; a bank another party
 *  holds or already activated never is.
 */
bool nsm_fpga_authority_activatable(
    const struct nsm_fpga_authority_status *status);

/** @brief Grant for the next ACQUIRE: the activated epoch, a nonzero
 *  session chosen by the caller and the previous generation plus one.
 *  @return nsm_sw_codes, NSM_SW_ERROR_DATA when the bank is not acquirable
 */
int nsm_fpga_grant_from_authority(
    const struct nsm_fpga_authority_status *status, uint32_t session,
    uint64_t *epoch, uint32_t *generation);

/* NSM window of one controller map: where each record is written or read */
struct nsm_fpga_window {
	uint32_t configuration_type;
	uint64_t control_write;
	uint64_t control_read;
	uint64_t data_write;
	uint64_t completion_read;
	uint64_t status;
	uint64_t compact; /* the word record, one SET carries the whole op */
};

/* Session grant: the activated epoch, a nonzero session, generation + 1 */
struct nsm_fpga_grant {
	uint64_t epoch;
	uint32_t session;
	uint32_t generation;
};

enum nsm_fpga_step_kind {
	NSM_FPGA_STEP_WRITE = 0,
	NSM_FPGA_STEP_READ = 1,
};

/* One MMIO exchange: SET of a record or GET of a 32-byte record */
struct nsm_fpga_step {
	uint8_t kind;
	uint64_t address;
	uint8_t record[NSM_FPGA_RECORD_SIZE];
};

#define NSM_FPGA_PLAN_CONTROL_STEPS 2

/** @brief Identity a plan stamps on its records: grant plus sequence */
struct nsm_fpga_identity
nsm_fpga_plan_identity(const struct nsm_fpga_grant *grant, uint32_t sequence);

/** @brief Frames of one session control: SET record, GET receipt.
 *  @param[in] opcode - ACQUIRE, RENEW or RELEASE
 *  @param[in] control_sequence - 1 for the first control of a session
 *  @param[in] argument - 0, or the last data sequence for RELEASE
 *  @param[out] steps - NSM_FPGA_PLAN_CONTROL_STEPS entries
 *  @return nsm_sw_codes
 */
int nsm_fpga_plan_control(const struct nsm_fpga_window *window,
			  const struct nsm_fpga_grant *grant, uint8_t opcode,
			  uint32_t control_sequence, uint32_t argument,
			  struct nsm_fpga_step *steps);

#define NSM_FPGA_PLAN_COMPACT_STEPS 2
#define NSM_FPGA_COMPACT_RECORD_VERSION 1

/** @brief Encode the compact word record: identity at 0, station, opcode,
 *  byte mask and record version at 20 to 23, 32-bit address at 24, data at 28.
 *  @param[in] mask - 15 for a read; a write clears the bytes it disables
 *  @param[in] data - write data, zero for a read
 *  @return nsm_sw_codes
 */
int nsm_fpga_encode_word_compact(const struct nsm_fpga_identity *identity,
				 uint8_t station, uint8_t opcode, uint8_t mask,
				 uint64_t address, uint32_t data,
				 uint8_t *record);

/** @brief Frames of one word op: SET compact record, GET result.
 *  @param[in] data_sequence - 1 for the first word of a session
 *  @param[in] opcode - NSM_FPGA_OP_READ32 or NSM_FPGA_OP_WRITE32
 *  @param[out] steps - NSM_FPGA_PLAN_COMPACT_STEPS entries
 *  @return nsm_sw_codes, NSM_SW_ERROR_DATA when the map has no word window
 */
int nsm_fpga_plan_word_compact(const struct nsm_fpga_window *window,
			       const struct nsm_fpga_grant *grant,
			       uint32_t data_sequence, uint8_t station,
			       uint8_t opcode, uint64_t address, uint32_t value,
			       struct nsm_fpga_step *steps);

#define NSM_FPGA_PLAN_BYTE_STEPS 2

/** @brief True when an I2C byte argument word is well formed.
 *  address 0x08 to 0x77, bits 7 and 20 to 31 zero, write data zero on a read,
 *  read_nack only on a read, and a read with stop must also NACK.
 */
bool nsm_fpga_i2c_argument_valid(uint32_t argument);

/** @brief Encode the header record of an I2C byte op: opcode BYTE, mask 0, the
 *  argument word at bytes 28 to 31.
 *  @param[out] record - 32 bytes
 */
int nsm_fpga_encode_byte_header(const struct nsm_fpga_identity *identity,
				uint8_t station, uint32_t argument,
				uint8_t *record);

/** @brief True for a bounded transaction opcode: 0x40 | writes << 3 | reads
 *  with writes 0..3, reads 0..4 and at least one of them. */
bool nsm_fpga_xfer_opcode_valid(uint8_t opcode);

/** @brief Header record of a bounded I2C transaction. */
int nsm_fpga_encode_xfer_header(const struct nsm_fpga_identity *identity,
				uint8_t station, uint8_t opcode,
				uint32_t argument, uint8_t *record);

/** @brief Plan a bounded transaction: header write, completion read. */
int nsm_fpga_plan_i2c_xfer(const struct nsm_fpga_window *window,
			   const struct nsm_fpga_grant *grant,
			   uint32_t data_sequence, uint8_t station,
			   uint8_t opcode, uint32_t argument,
			   struct nsm_fpga_step *steps);

/** @brief Frames of one I2C byte op: SET header to the data write window, GET.
 *  @param[in] data_sequence - previous data sequence plus one
 *  @param[out] steps - NSM_FPGA_PLAN_BYTE_STEPS entries
 */
int nsm_fpga_plan_i2c_byte(const struct nsm_fpga_window *window,
			   const struct nsm_fpga_grant *grant,
			   uint32_t data_sequence, uint8_t station,
			   uint32_t argument, struct nsm_fpga_step *steps);

/** @brief Frames of an authority request, ACTIVATE or RECOVER: a control
 *  record that carries only the fresh epoch, then the receipt read. The epoch
 *  must exceed the bank's.
 */
int nsm_fpga_plan_authority(const struct nsm_fpga_window *window,
			    uint8_t opcode, uint64_t epoch,
			    struct nsm_fpga_step *steps);

/** @brief nsm_fpga_plan_authority for RECOVER */
int nsm_fpga_plan_recover(const struct nsm_fpga_window *window, uint64_t epoch,
			  struct nsm_fpga_step *steps);

/** @brief GET of the authority status record */
int nsm_fpga_plan_status_read(const struct nsm_fpga_window *window,
			      struct nsm_fpga_step *step);

/* What a GET reply means for the request it answers */
enum nsm_fpga_reply {
	NSM_FPGA_REPLY_DONE = 0,
	NSM_FPGA_REPLY_PENDING = 1,
	NSM_FPGA_REPLY_REJECTED = 2,
	NSM_FPGA_REPLY_FAULT = 3,
	NSM_FPGA_REPLY_FOREIGN = 4,
	NSM_FPGA_REPLY_MALFORMED = 5,
};

/** @brief Class of a control receipt; result set when REJECTED.
 *  An empty receipt counts as PENDING: the record is not processed yet.
 */
enum nsm_fpga_reply
nsm_fpga_classify_control(const uint8_t *record,
			  const struct nsm_fpga_identity *identity,
			  uint8_t opcode, uint32_t argument, uint8_t *result);

/** @brief Class of an authority receipt (ACTIVATE or RECOVER) for the epoch
 *  sent: the owner is not checked, the bank has none after it; result set
 *  when REJECTED. */
enum nsm_fpga_reply nsm_fpga_classify_authority(const uint8_t *record,
						uint64_t epoch, uint8_t opcode,
						uint8_t *result);

/** @brief Class of a word completion; data set when DONE, reason on FAULT */
enum nsm_fpga_reply
nsm_fpga_classify_word(const uint8_t *record,
		       const struct nsm_fpga_identity *identity, uint8_t opcode,
		       uint32_t *data, uint8_t *fault_reason);

/** @brief Class of an I2C byte completion against the submitted argument.
 *  DONE on success or a terminal target NACK (nack set, data unavailable);
 *  data_byte set on a successful read; PENDING or FOREIGN means read again.
 *  Success needs quiescent, not faulted, read valid matching the read bit, and
 *  the bus released when stop was set or held when it was not.
 */
enum nsm_fpga_reply
nsm_fpga_classify_i2c_byte(const uint8_t *record,
			   const struct nsm_fpga_identity *identity,
			   uint32_t argument, uint8_t *data_byte, bool *nack);

/** @brief Classify the completion of a bounded transaction: DONE with the
 *  read bytes in data (little-endian) or with nack set when the target
 *  NACKed and the bus is released, FAULT with the reason, else as a byte. */
enum nsm_fpga_reply nsm_fpga_classify_i2c_xfer(
    const uint8_t *record, const struct nsm_fpga_identity *identity,
    uint8_t opcode, uint32_t *data, bool *nack, uint8_t *fault_reason);

/** @brief Short name of a reply class for logs */
const char *nsm_fpga_reply_name(enum nsm_fpga_reply reply);

#ifdef __cplusplus
}
#endif
#endif

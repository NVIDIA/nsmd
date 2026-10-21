// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#include "fpga-controller.h"

#include "base.h"

#include <string.h>

/* Byte offsets inside a record, by record layout */
enum record_layout {
	/* header, control, byte and transaction records */
	REC_PROFILE = 0,
	REC_OPCODE = 4,
	REC_STATION = 5,
	REC_MASK = 6,
	REC_IDENTITY = 8,
	REC_ARGUMENT = 28,
	/* compact word record */
	COMPACT_IDENTITY = 0,
	COMPACT_STATION = 20,
	COMPACT_OPCODE = 21,
	COMPACT_MASK = 22,
	COMPACT_VERSION = 23,
	COMPACT_ADDRESS = 24,
	COMPACT_DATA = 28,
	/* control completion */
	CTRL_RESULT = 5,
	CTRL_OWNER = 6,
	CTRL_STATE = 7,
	/* word completion */
	WORD_RESULT = 4,
	WORD_FLAGS = 5,
	WORD_OWNER = 6,
	WORD_OPCODE = 7,
	WORD_DATA = 28,
	WORD_FAULT_RESPONSE = 28,
	WORD_FAULT_REASON = 29,
	/* authority status record */
	STATUS_VERSION = 4,
	STATUS_FLAGS = 5,
	STATUS_OWNER = 6,
	STATUS_RESERVED = 7,
	STATUS_EPOCH = 8,
	STATUS_SESSION = 16,
	STATUS_GENERATION = 20,
	STATUS_ZERO = 24,
};

/* Control completion state byte bits */
#define CTRL_STATE_OCCUPIED 0x01u
#define CTRL_STATE_PENDING 0x02u
#define CTRL_STATE_KNOWN (CTRL_STATE_OCCUPIED | CTRL_STATE_PENDING)
/* Bit 7 of a flags byte is reserved and reads zero */
#define FLAGS_RESERVED 0x80u
#define STATUS_RECORD_VERSION 1
#define PROFILE_BYTES sizeof(uint32_t)

static void put_le(uint8_t *record, size_t offset, uint64_t value, size_t bytes)
{
	for (size_t i = 0; i < bytes; ++i) {
		record[offset + i] = (uint8_t)(value >> (8 * i));
	}
}

static uint64_t get_le(const uint8_t *record, size_t offset, size_t bytes)
{
	uint64_t value = 0;
	for (size_t i = 0; i < bytes; ++i) {
		value |= (uint64_t)record[offset + i] << (8 * i);
	}
	return value;
}

bool nsm_fpga_identity_valid(const struct nsm_fpga_identity *identity)
{
	return identity != NULL && identity->epoch != 0 &&
	       identity->session != 0 && identity->generation != 0 &&
	       identity->sequence != 0 &&
	       identity->sequence <= NSM_FPGA_MAX_SEQUENCE;
}

static void put_identity(uint8_t *record, size_t offset,
			 const struct nsm_fpga_identity *identity)
{
	put_le(record, offset, identity->epoch, sizeof(identity->epoch));
	offset += sizeof(identity->epoch);
	put_le(record, offset, identity->session, sizeof(identity->session));
	offset += sizeof(identity->session);
	put_le(record, offset, identity->generation,
	       sizeof(identity->generation));
	offset += sizeof(identity->generation);
	put_le(record, offset, identity->sequence, sizeof(identity->sequence));
}

static void get_identity(const uint8_t *record, size_t offset,
			 struct nsm_fpga_identity *identity)
{
	identity->epoch = get_le(record, offset, sizeof(identity->epoch));
	offset += sizeof(identity->epoch);
	identity->session =
	    (uint32_t)get_le(record, offset, sizeof(identity->session));
	offset += sizeof(identity->session);
	identity->generation =
	    (uint32_t)get_le(record, offset, sizeof(identity->generation));
	offset += sizeof(identity->generation);
	identity->sequence =
	    (uint32_t)get_le(record, offset, sizeof(identity->sequence));
}

static bool same_identity(const struct nsm_fpga_identity *a,
			  const struct nsm_fpga_identity *b)
{
	return a->epoch == b->epoch && a->session == b->session &&
	       a->generation == b->generation && a->sequence == b->sequence;
}

static bool control_request_valid(const struct nsm_fpga_identity *identity,
				  uint8_t opcode, uint32_t argument)
{
	if (identity == NULL) {
		return false;
	}
	if (opcode == NSM_FPGA_CTRL_ACTIVATE ||
	    opcode == NSM_FPGA_CTRL_RECOVER) {
		/* an authority request carries the epoch and nothing else */
		return identity->epoch != 0 && identity->session == 0 &&
		       identity->generation == 0 && identity->sequence == 0 &&
		       argument == 0;
	}
	if (!nsm_fpga_identity_valid(identity)) {
		return false;
	}
	switch (opcode) {
	case NSM_FPGA_CTRL_ACQUIRE:
		return identity->sequence == 1 && argument == 0;
	case NSM_FPGA_CTRL_RENEW:
		return identity->sequence > 1 && argument == 0;
	case NSM_FPGA_CTRL_RELEASE:
		return identity->sequence > 1 &&
		       argument <= NSM_FPGA_MAX_SEQUENCE;
	default:
		return false;
	}
}

int nsm_fpga_encode_control(const struct nsm_fpga_identity *identity,
			    uint8_t opcode, uint32_t argument, uint8_t *record)
{
	if (record == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	if (!control_request_valid(identity, opcode, argument)) {
		return NSM_SW_ERROR_DATA;
	}
	memset(record, 0, NSM_FPGA_RECORD_SIZE);
	put_le(record, REC_PROFILE, NSM_FPGA_PROFILE_IBC2, PROFILE_BYTES);
	record[REC_OPCODE] = opcode;
	put_identity(record, REC_IDENTITY, identity);
	put_le(record, REC_ARGUMENT, argument, sizeof(argument));
	return NSM_SW_SUCCESS;
}

int nsm_fpga_decode_control_completion(
    const uint8_t *record, struct nsm_fpga_control_completion *completion)
{
	if (record == NULL || completion == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	if (get_le(record, REC_PROFILE, PROFILE_BYTES) !=
		NSM_FPGA_PROFILE_IBC2 ||
	    record[CTRL_RESULT] > NSM_FPGA_CTRL_EXHAUSTED ||
	    (record[CTRL_STATE] & ~CTRL_STATE_KNOWN) != 0) {
		return NSM_SW_ERROR_DATA;
	}
	memset(completion, 0, sizeof(*completion));
	completion->opcode = record[REC_OPCODE];
	completion->result = record[CTRL_RESULT];
	completion->owner = record[CTRL_OWNER];
	completion->occupied = (record[CTRL_STATE] & CTRL_STATE_OCCUPIED) != 0;
	completion->pending = (record[CTRL_STATE] & CTRL_STATE_PENDING) != 0;
	get_identity(record, REC_IDENTITY, &completion->identity);
	completion->argument = (uint32_t)get_le(record, REC_ARGUMENT,
						sizeof(completion->argument));
	if (!completion->occupied) {
		/* an empty record carries nothing after the profile */
		for (size_t i = PROFILE_BYTES; i < NSM_FPGA_RECORD_SIZE; ++i) {
			if (record[i] != 0) {
				return NSM_SW_ERROR_DATA;
			}
		}
		return NSM_SW_SUCCESS;
	}
	/* an authority receipt (ACTIVATE, RECOVER) may name no owner */
	const bool authority = completion->opcode == NSM_FPGA_CTRL_ACTIVATE ||
			       completion->opcode == NSM_FPGA_CTRL_RECOVER;
	if ((completion->owner != NSM_FPGA_OWNER_NATIVE &&
	     completion->owner != NSM_FPGA_OWNER_NSM &&
	     !(authority && completion->owner == NSM_FPGA_OWNER_NONE)) ||
	    (!authority && completion->opcode != NSM_FPGA_CTRL_ACQUIRE &&
	     completion->opcode != NSM_FPGA_CTRL_RENEW &&
	     completion->opcode != NSM_FPGA_CTRL_RELEASE) ||
	    (completion->pending && completion->result != NSM_FPGA_CTRL_OK)) {
		return NSM_SW_ERROR_DATA;
	}
	return NSM_SW_SUCCESS;
}

enum nsm_fpga_match
nsm_fpga_match_control(const struct nsm_fpga_control_completion *completion,
		       const struct nsm_fpga_identity *identity, uint8_t opcode,
		       uint32_t argument)
{
	if (completion == NULL || identity == NULL) {
		return NSM_FPGA_MATCH_REJECTED;
	}
	if (!completion->occupied) {
		return NSM_FPGA_MATCH_EMPTY;
	}
	if (completion->owner != NSM_FPGA_OWNER_NSM ||
	    completion->opcode != opcode || completion->argument != argument ||
	    !same_identity(&completion->identity, identity)) {
		return NSM_FPGA_MATCH_FOREIGN;
	}
	if (completion->pending) {
		return NSM_FPGA_MATCH_PENDING;
	}
	return completion->result == NSM_FPGA_CTRL_OK ? NSM_FPGA_MATCH_SUCCESS
						      : NSM_FPGA_MATCH_REJECTED;
}

int nsm_fpga_decode_word_completion(const uint8_t *record,
				    struct nsm_fpga_word_completion *completion)
{
	if (record == NULL || completion == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	if (get_le(record, REC_PROFILE, PROFILE_BYTES) !=
		NSM_FPGA_PROFILE_IBC2 ||
	    record[WORD_RESULT] > NSM_FPGA_RESULT_FAULT ||
	    (record[WORD_FLAGS] & FLAGS_RESERVED) != 0) {
		return NSM_SW_ERROR_DATA;
	}
	memset(completion, 0, sizeof(*completion));
	completion->result = record[WORD_RESULT];
	completion->flags = record[WORD_FLAGS];
	completion->owner = record[WORD_OWNER];
	completion->opcode = record[WORD_OPCODE];
	get_identity(record, REC_IDENTITY, &completion->identity);
	completion->data =
	    (uint32_t)get_le(record, WORD_DATA, sizeof(completion->data));
	completion->fault_response = record[WORD_FAULT_RESPONSE];
	completion->fault_reason = record[WORD_FAULT_REASON];
	if (completion->result == NSM_FPGA_RESULT_EMPTY) {
		/* an empty record carries nothing after the profile */
		for (size_t i = PROFILE_BYTES; i < NSM_FPGA_RECORD_SIZE; ++i) {
			if (record[i] != 0) {
				return NSM_SW_ERROR_DATA;
			}
		}
		return NSM_SW_SUCCESS;
	}
	if (!nsm_fpga_identity_valid(&completion->identity) ||
	    (completion->owner != NSM_FPGA_OWNER_NATIVE &&
	     completion->owner != NSM_FPGA_OWNER_NSM) ||
	    (completion->opcode != NSM_FPGA_OP_READ32 &&
	     completion->opcode != NSM_FPGA_OP_WRITE32 &&
	     completion->opcode != NSM_FPGA_OP_BYTE &&
	     !nsm_fpga_xfer_opcode_valid(completion->opcode)) ||
	    (completion->flags & NSM_FPGA_FLAG_OCCUPIED) == 0) {
		return NSM_SW_ERROR_DATA;
	}
	return NSM_SW_SUCCESS;
}

enum nsm_fpga_match
nsm_fpga_match_word(const struct nsm_fpga_word_completion *completion,
		    const struct nsm_fpga_identity *identity, uint8_t opcode)
{
	if (completion == NULL || identity == NULL) {
		return NSM_FPGA_MATCH_REJECTED;
	}
	if (completion->result == NSM_FPGA_RESULT_EMPTY) {
		return NSM_FPGA_MATCH_EMPTY;
	}
	if (completion->owner != NSM_FPGA_OWNER_NSM ||
	    completion->opcode != opcode ||
	    !same_identity(&completion->identity, identity)) {
		return NSM_FPGA_MATCH_FOREIGN;
	}
	const uint8_t flags = completion->flags;
	if ((flags & (NSM_FPGA_FLAG_BUS_HELD | NSM_FPGA_FLAG_BUS_RELEASED)) ==
		(NSM_FPGA_FLAG_BUS_HELD | NSM_FPGA_FLAG_BUS_RELEASED) ||
	    ((flags & NSM_FPGA_FLAG_PENDING) != 0 &&
	     (flags & (NSM_FPGA_FLAG_QUIESCENT | NSM_FPGA_FLAG_READ_VALID)) !=
		 0)) {
		return NSM_FPGA_MATCH_REJECTED;
	}
	switch (completion->result) {
	case NSM_FPGA_RESULT_PENDING:
		if ((flags & (NSM_FPGA_FLAG_PENDING | NSM_FPGA_FLAG_FAULTED)) ==
			NSM_FPGA_FLAG_PENDING &&
		    completion->data == 0) {
			return NSM_FPGA_MATCH_PENDING;
		}
		return NSM_FPGA_MATCH_REJECTED;
	case NSM_FPGA_RESULT_SUCCESS: {
		const bool reading = opcode == NSM_FPGA_OP_READ32;
		const bool read_valid = (flags & NSM_FPGA_FLAG_READ_VALID) != 0;
		if ((flags & (NSM_FPGA_FLAG_PENDING | NSM_FPGA_FLAG_QUIESCENT |
			      NSM_FPGA_FLAG_FAULTED)) ==
			NSM_FPGA_FLAG_QUIESCENT &&
		    read_valid == reading &&
		    (reading || completion->data == 0)) {
			return NSM_FPGA_MATCH_SUCCESS;
		}
		return NSM_FPGA_MATCH_REJECTED;
	}
	case NSM_FPGA_RESULT_FAULT:
		if ((flags &
		     (NSM_FPGA_FLAG_FAULTED | NSM_FPGA_FLAG_READ_VALID)) ==
			NSM_FPGA_FLAG_FAULTED &&
		    (completion->data >> 16) == 0 &&
		    completion->fault_reason >= 1 &&
		    completion->fault_reason <= 8) {
			return NSM_FPGA_MATCH_FAULT;
		}
		return NSM_FPGA_MATCH_REJECTED;
	default:
		return NSM_FPGA_MATCH_REJECTED;
	}
}

int nsm_fpga_decode_authority_status(const uint8_t *record,
				     struct nsm_fpga_authority_status *status)
{
	if (record == NULL || status == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	/* the reserved byte and the last eight bytes read zero */
	if (get_le(record, REC_PROFILE, PROFILE_BYTES) !=
		NSM_FPGA_PROFILE_IBC2 ||
	    record[STATUS_VERSION] != STATUS_RECORD_VERSION ||
	    (record[STATUS_FLAGS] & FLAGS_RESERVED) != 0 ||
	    record[STATUS_OWNER] > NSM_FPGA_OWNER_NSM ||
	    record[STATUS_RESERVED] != 0 ||
	    get_le(record, STATUS_ZERO, sizeof(uint64_t)) != 0) {
		return NSM_SW_ERROR_DATA;
	}
	memset(status, 0, sizeof(*status));
	status->flags = record[STATUS_FLAGS];
	status->owner = record[STATUS_OWNER];
	status->epoch = get_le(record, STATUS_EPOCH, sizeof(status->epoch));
	status->session =
	    (uint32_t)get_le(record, STATUS_SESSION, sizeof(status->session));
	status->generation = (uint32_t)get_le(record, STATUS_GENERATION,
					      sizeof(status->generation));
	const bool initialized =
	    (status->flags & NSM_FPGA_AUTH_INITIALIZED) != 0;
	if (initialized != (status->epoch != 0)) {
		return NSM_SW_ERROR_DATA;
	}
	return NSM_SW_SUCCESS;
}

bool nsm_fpga_authority_acquirable(
    const struct nsm_fpga_authority_status *status)
{
	if (status == NULL) {
		return false;
	}
	const uint8_t f = status->flags;
	return (f & NSM_FPGA_AUTH_INITIALIZED) != 0 &&
	       (f & NSM_FPGA_AUTH_AVAILABLE) != 0 &&
	       (f & NSM_FPGA_AUTH_QUARANTINED) == 0 && status->epoch != 0;
}

bool nsm_fpga_authority_recoverable(
    const struct nsm_fpga_authority_status *status)
{
	if (status == NULL) {
		return false;
	}
	const uint8_t need = NSM_FPGA_AUTH_QUARANTINED |
			     NSM_FPGA_AUTH_QUIESCENT | NSM_FPGA_AUTH_RELEASED;
	return (status->flags & need) == need &&
	       status->owner == NSM_FPGA_OWNER_NSM;
}

bool nsm_fpga_authority_activatable(
    const struct nsm_fpga_authority_status *status)
{
	if (status == NULL) {
		return false;
	}
	const uint8_t never = NSM_FPGA_AUTH_INITIALIZED |
			      NSM_FPGA_AUTH_QUARANTINED |
			      NSM_FPGA_AUTH_GRANT_VALID;
	return (status->flags & never) == 0 &&
	       status->owner == NSM_FPGA_OWNER_NONE && status->epoch == 0;
}

enum nsm_fpga_session_state
nsm_fpga_session_classify(const struct nsm_fpga_authority_status *status)
{
	if (status == NULL) {
		return NSM_FPGA_SESSION_UNREACHABLE;
	}
	if (status->flags & NSM_FPGA_AUTH_QUARANTINED) {
		return NSM_FPGA_SESSION_QUARANTINED;
	}
	if (status->epoch == 0 ||
	    !(status->flags & NSM_FPGA_AUTH_INITIALIZED)) {
		return NSM_FPGA_SESSION_UNINITIALIZED;
	}
	if ((status->flags & NSM_FPGA_AUTH_GRANT_VALID) ||
	    status->owner == NSM_FPGA_OWNER_NSM) {
		return NSM_FPGA_SESSION_OWNED_BY_BMC;
	}
	if (status->owner == NSM_FPGA_OWNER_NATIVE) {
		return NSM_FPGA_SESSION_OWNED_BY_HOST;
	}
	if (status->flags & NSM_FPGA_AUTH_AVAILABLE) {
		return NSM_FPGA_SESSION_READY;
	}
	return NSM_FPGA_SESSION_UNINITIALIZED;
}

int nsm_fpga_grant_from_authority(
    const struct nsm_fpga_authority_status *status, uint32_t session,
    uint64_t *epoch, uint32_t *generation)
{
	if (status == NULL || epoch == NULL || generation == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	if (!nsm_fpga_authority_acquirable(status) || session == 0 ||
	    status->generation == UINT32_MAX) {
		return NSM_SW_ERROR_DATA;
	}
	*epoch = status->epoch;
	*generation = status->generation + 1;
	return NSM_SW_SUCCESS;
}

const char *nsm_fpga_control_result_name(uint8_t result)
{
	switch (result) {
	case NSM_FPGA_CTRL_OK:
		return "OK";
	case NSM_FPGA_CTRL_INVALID:
		return "INVALID";
	case NSM_FPGA_CTRL_BUSY:
		return "BUSY";
	case NSM_FPGA_CTRL_STALE:
		return "STALE";
	case NSM_FPGA_CTRL_UNSAFE:
		return "UNSAFE";
	case NSM_FPGA_CTRL_DENIED:
		return "DENIED";
	case NSM_FPGA_CTRL_FENCED:
		return "FENCED";
	case NSM_FPGA_CTRL_EXHAUSTED:
		return "EXHAUSTED";
	default:
		return "UNKNOWN";
	}
}

struct nsm_fpga_identity
nsm_fpga_plan_identity(const struct nsm_fpga_grant *grant, uint32_t sequence)
{
	struct nsm_fpga_identity identity = {0, 0, 0, 0};
	if (grant != NULL) {
		identity.epoch = grant->epoch;
		identity.session = grant->session;
		identity.generation = grant->generation;
		identity.sequence = sequence;
	}
	return identity;
}

static void read_step(struct nsm_fpga_step *step, uint64_t address)
{
	memset(step, 0, sizeof(*step));
	step->kind = NSM_FPGA_STEP_READ;
	step->address = address;
}

int nsm_fpga_plan_control(const struct nsm_fpga_window *window,
			  const struct nsm_fpga_grant *grant, uint8_t opcode,
			  uint32_t control_sequence, uint32_t argument,
			  struct nsm_fpga_step *steps)
{
	if (window == NULL || grant == NULL || steps == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	const struct nsm_fpga_identity identity =
	    nsm_fpga_plan_identity(grant, control_sequence);
	memset(steps, 0, NSM_FPGA_PLAN_CONTROL_STEPS * sizeof(*steps));
	steps[0].kind = NSM_FPGA_STEP_WRITE;
	steps[0].address = window->control_write;
	int rc = nsm_fpga_encode_control(&identity, opcode, argument,
					 steps[0].record);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}
	read_step(&steps[1], window->control_read);
	return NSM_SW_SUCCESS;
}

int nsm_fpga_plan_authority(const struct nsm_fpga_window *window,
			    uint8_t opcode, uint64_t epoch,
			    struct nsm_fpga_step *steps)
{
	if (window == NULL || steps == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	if (opcode != NSM_FPGA_CTRL_ACTIVATE &&
	    opcode != NSM_FPGA_CTRL_RECOVER) {
		return NSM_SW_ERROR_DATA;
	}
	const struct nsm_fpga_identity identity = {epoch, 0, 0, 0};
	memset(steps, 0, NSM_FPGA_PLAN_CONTROL_STEPS * sizeof(*steps));
	steps[0].kind = NSM_FPGA_STEP_WRITE;
	steps[0].address = window->control_write;
	int rc = nsm_fpga_encode_control(&identity, opcode, 0, steps[0].record);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}
	read_step(&steps[1], window->control_read);
	return NSM_SW_SUCCESS;
}

int nsm_fpga_plan_recover(const struct nsm_fpga_window *window, uint64_t epoch,
			  struct nsm_fpga_step *steps)
{
	return nsm_fpga_plan_authority(window, NSM_FPGA_CTRL_RECOVER, epoch,
				       steps);
}

int nsm_fpga_encode_word_compact(const struct nsm_fpga_identity *identity,
				 uint8_t station, uint8_t opcode, uint8_t mask,
				 uint64_t address, uint32_t data,
				 uint8_t *record)
{
	if (record == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	if (!nsm_fpga_identity_valid(identity) || (address & 3) != 0 ||
	    address > UINT32_MAX) {
		return NSM_SW_ERROR_DATA;
	}
	if (opcode == NSM_FPGA_OP_READ32) {
		if (mask != 0x0f || data != 0) {
			return NSM_SW_ERROR_DATA;
		}
	} else if (opcode == NSM_FPGA_OP_WRITE32) {
		if (mask == 0 || mask > 0x0f) {
			return NSM_SW_ERROR_DATA;
		}
		for (unsigned i = 0; i < 4; ++i) {
			/* a byte the mask disables must be sent as zero */
			if ((mask & (1u << i)) == 0 &&
			    ((data >> (8 * i)) & 0xff) != 0) {
				return NSM_SW_ERROR_DATA;
			}
		}
	} else {
		return NSM_SW_ERROR_DATA;
	}
	memset(record, 0, NSM_FPGA_RECORD_SIZE);
	put_identity(record, COMPACT_IDENTITY, identity);
	record[COMPACT_STATION] = station;
	record[COMPACT_OPCODE] = opcode;
	record[COMPACT_MASK] = mask;
	record[COMPACT_VERSION] = NSM_FPGA_COMPACT_RECORD_VERSION;
	put_le(record, COMPACT_ADDRESS, address, sizeof(uint32_t));
	put_le(record, COMPACT_DATA, data, sizeof(data));
	return NSM_SW_SUCCESS;
}

int nsm_fpga_plan_word_compact(const struct nsm_fpga_window *window,
			       const struct nsm_fpga_grant *grant,
			       uint32_t data_sequence, uint8_t station,
			       uint8_t opcode, uint64_t address, uint32_t value,
			       struct nsm_fpga_step *steps)
{
	if (window == NULL || grant == NULL || steps == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	if (window->compact == 0) {
		return NSM_SW_ERROR_DATA;
	}
	const struct nsm_fpga_identity identity =
	    nsm_fpga_plan_identity(grant, data_sequence);
	memset(steps, 0, NSM_FPGA_PLAN_COMPACT_STEPS * sizeof(*steps));
	steps[0].kind = NSM_FPGA_STEP_WRITE;
	steps[0].address = window->compact;
	int rc = nsm_fpga_encode_word_compact(
	    &identity, station, opcode, 0x0f, address,
	    opcode == NSM_FPGA_OP_WRITE32 ? value : 0, steps[0].record);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}
	read_step(&steps[1], window->completion_read);
	return NSM_SW_SUCCESS;
}

bool nsm_fpga_i2c_argument_valid(uint32_t argument)
{
	const uint8_t address = (uint8_t)(argument & 0x7f);
	const uint8_t data = (uint8_t)((argument >> 8) & 0xff);
	const bool read = ((argument >> 16) & 1) != 0;
	const bool stop = ((argument >> 18) & 1) != 0;
	const bool read_nack = ((argument >> 19) & 1) != 0;
	if ((argument & 0x80) != 0 || (argument >> 20) != 0) {
		return false;
	}
	if (address < 0x08 || address > 0x77) {
		return false;
	}
	if (read) {
		if (data != 0 || (stop && !read_nack)) {
			return false;
		}
	} else if (read_nack) {
		return false;
	}
	return true;
}

int nsm_fpga_encode_byte_header(const struct nsm_fpga_identity *identity,
				uint8_t station, uint32_t argument,
				uint8_t *record)
{
	if (record == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	if (!nsm_fpga_identity_valid(identity) ||
	    !nsm_fpga_i2c_argument_valid(argument)) {
		return NSM_SW_ERROR_DATA;
	}
	memset(record, 0, NSM_FPGA_RECORD_SIZE);
	put_le(record, REC_PROFILE, NSM_FPGA_PROFILE_IBC2, PROFILE_BYTES);
	record[REC_OPCODE] = NSM_FPGA_OP_BYTE;
	record[REC_STATION] = station;
	record[REC_MASK] = 0;
	put_identity(record, REC_IDENTITY, identity);
	put_le(record, REC_ARGUMENT, argument, sizeof(argument));
	return NSM_SW_SUCCESS;
}

int nsm_fpga_plan_i2c_byte(const struct nsm_fpga_window *window,
			   const struct nsm_fpga_grant *grant,
			   uint32_t data_sequence, uint8_t station,
			   uint32_t argument, struct nsm_fpga_step *steps)
{
	if (window == NULL || grant == NULL || steps == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	const struct nsm_fpga_identity identity =
	    nsm_fpga_plan_identity(grant, data_sequence);
	memset(steps, 0, NSM_FPGA_PLAN_BYTE_STEPS * sizeof(*steps));
	steps[0].kind = NSM_FPGA_STEP_WRITE;
	steps[0].address = window->data_write;
	int rc = nsm_fpga_encode_byte_header(&identity, station, argument,
					     steps[0].record);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}
	read_step(&steps[1], window->completion_read);
	return NSM_SW_SUCCESS;
}

bool nsm_fpga_xfer_opcode_valid(uint8_t opcode)
{
	const uint8_t writes = (opcode >> 3) & 0x3;
	const uint8_t reads = opcode & 0x7;
	return (opcode >> 5) == 2 && reads <= 4 && (opcode & 0x1f) != 0 &&
	       (writes + reads) > 0;
}

static bool xfer_argument_valid(uint8_t opcode, uint32_t argument)
{
	const uint8_t address = (uint8_t)(argument & 0x7f);
	const uint8_t writes = (opcode >> 3) & 0x3;
	if ((argument & 0x80) != 0 || address < 0x08 || address > 0x77) {
		return false;
	}
	/* no write bytes beyond the count the opcode announces */
	return writes == 3 || (argument >> (8 + 8 * writes)) == 0;
}

int nsm_fpga_encode_xfer_header(const struct nsm_fpga_identity *identity,
				uint8_t station, uint8_t opcode,
				uint32_t argument, uint8_t *record)
{
	if (record == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	if (!nsm_fpga_identity_valid(identity) ||
	    !nsm_fpga_xfer_opcode_valid(opcode) ||
	    !xfer_argument_valid(opcode, argument)) {
		return NSM_SW_ERROR_DATA;
	}
	memset(record, 0, NSM_FPGA_RECORD_SIZE);
	put_le(record, REC_PROFILE, NSM_FPGA_PROFILE_IBC2, PROFILE_BYTES);
	record[REC_OPCODE] = opcode;
	record[REC_STATION] = station;
	record[REC_MASK] = 0;
	put_identity(record, REC_IDENTITY, identity);
	put_le(record, REC_ARGUMENT, argument, sizeof(argument));
	return NSM_SW_SUCCESS;
}

int nsm_fpga_plan_i2c_xfer(const struct nsm_fpga_window *window,
			   const struct nsm_fpga_grant *grant,
			   uint32_t data_sequence, uint8_t station,
			   uint8_t opcode, uint32_t argument,
			   struct nsm_fpga_step *steps)
{
	if (window == NULL || grant == NULL || steps == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	const struct nsm_fpga_identity identity =
	    nsm_fpga_plan_identity(grant, data_sequence);
	memset(steps, 0, NSM_FPGA_PLAN_BYTE_STEPS * sizeof(*steps));
	steps[0].kind = NSM_FPGA_STEP_WRITE;
	steps[0].address = window->data_write;
	int rc = nsm_fpga_encode_xfer_header(&identity, station, opcode,
					     argument, steps[0].record);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}
	read_step(&steps[1], window->completion_read);
	return NSM_SW_SUCCESS;
}

enum nsm_fpga_reply nsm_fpga_classify_i2c_xfer(
    const uint8_t *record, const struct nsm_fpga_identity *identity,
    uint8_t opcode, uint32_t *data, bool *nack, uint8_t *fault_reason)
{
	struct nsm_fpga_word_completion c;
	if (record == NULL || identity == NULL || nack == NULL ||
	    nsm_fpga_decode_word_completion(record, &c) != NSM_SW_SUCCESS) {
		return NSM_FPGA_REPLY_MALFORMED;
	}
	*nack = false;
	if (c.result == NSM_FPGA_RESULT_EMPTY) {
		return NSM_FPGA_REPLY_PENDING;
	}
	if (c.owner != NSM_FPGA_OWNER_NSM || c.opcode != opcode ||
	    !same_identity(&c.identity, identity)) {
		return NSM_FPGA_REPLY_FOREIGN;
	}
	const uint8_t reads = opcode & 0x7;
	const uint8_t released = NSM_FPGA_FLAG_OCCUPIED |
				 NSM_FPGA_FLAG_QUIESCENT |
				 NSM_FPGA_FLAG_BUS_RELEASED;
	switch (c.result) {
	case NSM_FPGA_RESULT_PENDING:
		if (c.flags ==
			(NSM_FPGA_FLAG_OCCUPIED | NSM_FPGA_FLAG_PENDING) &&
		    c.data == 0) {
			return NSM_FPGA_REPLY_PENDING;
		}
		return NSM_FPGA_REPLY_REJECTED;
	case NSM_FPGA_RESULT_SUCCESS:
		if (c.flags != (reads ? (released | NSM_FPGA_FLAG_READ_VALID)
				      : released) ||
		    (reads < 4 && (c.data >> (8 * reads)) != 0)) {
			return NSM_FPGA_REPLY_REJECTED;
		}
		if (data != NULL) {
			*data = c.data;
		}
		return NSM_FPGA_REPLY_DONE;
	case NSM_FPGA_RESULT_TARGET_NACK:
		/* the controller ended the transaction and released the bus */
		if (c.flags != released || (c.data & 0xff) != 1) {
			return NSM_FPGA_REPLY_REJECTED;
		}
		*nack = true;
		return NSM_FPGA_REPLY_DONE;
	case NSM_FPGA_RESULT_FAULT:
		if ((c.flags & NSM_FPGA_FLAG_FAULTED) == 0 ||
		    ((c.data & 0xff) != 2 && (c.data & 0xff) != 3)) {
			return NSM_FPGA_REPLY_REJECTED;
		}
		if (fault_reason != NULL) {
			*fault_reason = (uint8_t)(c.data & 0xff);
		}
		return NSM_FPGA_REPLY_FAULT;
	default:
		return NSM_FPGA_REPLY_REJECTED;
	}
}

int nsm_fpga_plan_status_read(const struct nsm_fpga_window *window,
			      struct nsm_fpga_step *step)
{
	if (window == NULL || step == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	read_step(step, window->status);
	return NSM_SW_SUCCESS;
}

static enum nsm_fpga_reply reply_of(enum nsm_fpga_match match)
{
	switch (match) {
	case NSM_FPGA_MATCH_SUCCESS:
		return NSM_FPGA_REPLY_DONE;
	case NSM_FPGA_MATCH_PENDING:
	case NSM_FPGA_MATCH_EMPTY:
		return NSM_FPGA_REPLY_PENDING;
	case NSM_FPGA_MATCH_REJECTED:
		return NSM_FPGA_REPLY_REJECTED;
	case NSM_FPGA_MATCH_FAULT:
		return NSM_FPGA_REPLY_FAULT;
	case NSM_FPGA_MATCH_FOREIGN:
	default:
		return NSM_FPGA_REPLY_FOREIGN;
	}
}

enum nsm_fpga_reply
nsm_fpga_classify_control(const uint8_t *record,
			  const struct nsm_fpga_identity *identity,
			  uint8_t opcode, uint32_t argument, uint8_t *result)
{
	struct nsm_fpga_control_completion completion;
	if (record == NULL || identity == NULL ||
	    nsm_fpga_decode_control_completion(record, &completion) !=
		NSM_SW_SUCCESS) {
		return NSM_FPGA_REPLY_MALFORMED;
	}
	const enum nsm_fpga_reply reply = reply_of(
	    nsm_fpga_match_control(&completion, identity, opcode, argument));
	if (result != NULL && reply == NSM_FPGA_REPLY_REJECTED) {
		*result = completion.result;
	}
	return reply;
}

enum nsm_fpga_reply nsm_fpga_classify_authority(const uint8_t *record,
						uint64_t epoch, uint8_t opcode,
						uint8_t *result)
{
	struct nsm_fpga_control_completion completion;
	if (record == NULL || nsm_fpga_decode_control_completion(
				  record, &completion) != NSM_SW_SUCCESS) {
		return NSM_FPGA_REPLY_MALFORMED;
	}
	if (!completion.occupied) {
		return NSM_FPGA_REPLY_PENDING;
	}
	if (completion.opcode != opcode || completion.identity.epoch != epoch ||
	    completion.argument != 0) {
		return NSM_FPGA_REPLY_FOREIGN;
	}
	if (completion.pending) {
		return NSM_FPGA_REPLY_PENDING;
	}
	if (completion.result != NSM_FPGA_CTRL_OK) {
		if (result != NULL) {
			*result = completion.result;
		}
		return NSM_FPGA_REPLY_REJECTED;
	}
	return NSM_FPGA_REPLY_DONE;
}

enum nsm_fpga_reply
nsm_fpga_classify_word(const uint8_t *record,
		       const struct nsm_fpga_identity *identity, uint8_t opcode,
		       uint32_t *data, uint8_t *fault_reason)
{
	struct nsm_fpga_word_completion completion;
	if (record == NULL || identity == NULL ||
	    nsm_fpga_decode_word_completion(record, &completion) !=
		NSM_SW_SUCCESS) {
		return NSM_FPGA_REPLY_MALFORMED;
	}
	const enum nsm_fpga_reply reply =
	    reply_of(nsm_fpga_match_word(&completion, identity, opcode));
	if (data != NULL && reply == NSM_FPGA_REPLY_DONE) {
		*data = completion.data;
	}
	if (fault_reason != NULL && reply == NSM_FPGA_REPLY_FAULT) {
		*fault_reason = completion.fault_reason;
	}
	return reply;
}

enum nsm_fpga_reply
nsm_fpga_classify_i2c_byte(const uint8_t *record,
			   const struct nsm_fpga_identity *identity,
			   uint32_t argument, uint8_t *data_byte, bool *nack)
{
	struct nsm_fpga_word_completion c;
	if (record == NULL || identity == NULL || nack == NULL ||
	    nsm_fpga_decode_word_completion(record, &c) != NSM_SW_SUCCESS) {
		return NSM_FPGA_REPLY_MALFORMED;
	}
	*nack = false;
	if (c.result == NSM_FPGA_RESULT_EMPTY) {
		return NSM_FPGA_REPLY_PENDING;
	}
	if (c.owner != NSM_FPGA_OWNER_NSM || c.opcode != NSM_FPGA_OP_BYTE ||
	    !same_identity(&c.identity, identity)) {
		return NSM_FPGA_REPLY_FOREIGN;
	}
	const uint8_t flags = c.flags;
	const bool read = ((argument >> 16) & 1) != 0;
	const bool quiescent = (flags & NSM_FPGA_FLAG_QUIESCENT) != 0;
	const bool faulted = (flags & NSM_FPGA_FLAG_FAULTED) != 0;
	const bool read_valid = (flags & NSM_FPGA_FLAG_READ_VALID) != 0;
	const bool bus_held = (flags & NSM_FPGA_FLAG_BUS_HELD) != 0;
	const bool bus_released = (flags & NSM_FPGA_FLAG_BUS_RELEASED) != 0;
	switch (c.result) {
	case NSM_FPGA_RESULT_PENDING:
		return NSM_FPGA_REPLY_PENDING;
	case NSM_FPGA_RESULT_TARGET_NACK:
		if (!quiescent || faulted || (bus_released && bus_held)) {
			return NSM_FPGA_REPLY_REJECTED;
		}
		*nack = true;
		return NSM_FPGA_REPLY_DONE;
	case NSM_FPGA_RESULT_SUCCESS:
		if (!quiescent || faulted || read_valid != read ||
		    (bus_released && bus_held)) {
			return NSM_FPGA_REPLY_REJECTED;
		}
		if (data_byte != NULL) {
			*data_byte = read ? (uint8_t)(c.data & 0xff) : 0;
		}
		return NSM_FPGA_REPLY_DONE;
	case NSM_FPGA_RESULT_FAULT:
		return NSM_FPGA_REPLY_FAULT;
	default:
		return NSM_FPGA_REPLY_REJECTED;
	}
}

const char *nsm_fpga_reply_name(enum nsm_fpga_reply reply)
{
	switch (reply) {
	case NSM_FPGA_REPLY_DONE:
		return "done";
	case NSM_FPGA_REPLY_PENDING:
		return "pending";
	case NSM_FPGA_REPLY_REJECTED:
		return "rejected";
	case NSM_FPGA_REPLY_FAULT:
		return "fault";
	case NSM_FPGA_REPLY_FOREIGN:
		return "foreign";
	case NSM_FPGA_REPLY_MALFORMED:
		return "malformed";
	}
	return "unknown";
}

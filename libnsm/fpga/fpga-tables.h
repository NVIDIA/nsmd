// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors
#ifndef LIBNSM_FPGA_TABLES_H
#define LIBNSM_FPGA_TABLES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fpga-controller.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Map data generated from the FPGA controller map: window, stations, rules. */

enum nsm_fpga_station_kind {
	NSM_FPGA_STATION_I2C_BYTE = 0,
	NSM_FPGA_STATION_WORD = 1,
	NSM_FPGA_STATION_SDM = 2, /* SDM mailbox: strict per-word policy */
};

/* One operation target of the map: selector, kind and allowed byte window. */
struct nsm_fpga_station {
	uint8_t selector;
	uint8_t kind;
	uint64_t first_byte;
	uint64_t last_byte;
};

/* How a rule publishes: a state object, a numeric Sensor.Value, a 64-bit
   identifier string (one-time inventory read), or a vector of numbers. */
enum nsm_fpga_rule_kind {
	NSM_FPGA_RULE_STATE = 0,
	NSM_FPGA_RULE_NUMERIC = 1,
	NSM_FPGA_RULE_IDENTITY = 2,
	NSM_FPGA_RULE_ARRAY = 3,
};

/* The OperationalStatus a state rule or an ended group publishes */
enum nsm_fpga_state {
	NSM_FPGA_STATE_ENABLED = 0,
	NSM_FPGA_STATE_STARTING = 1,
	NSM_FPGA_STATE_STANDBY_OFFLINE = 2,
	NSM_FPGA_STATE_ABSENT = 3,
	NSM_FPGA_STATE_UNAVAILABLE_OFFLINE = 4,
	NSM_FPGA_STATE_FAULT = 5,
};

enum nsm_fpga_health {
	NSM_FPGA_HEALTH_OK = 0,
	NSM_FPGA_HEALTH_WARNING = 1,
	NSM_FPGA_HEALTH_CRITICAL = 2,
};

/* One line of a state map: the first entry whose (word & mask) == value
   gives the state and health; a mask of 0 matches every word. */
struct nsm_fpga_state_entry {
	uint32_t mask;
	uint32_t value;
	uint8_t state;	/* nsm_fpga_state */
	uint8_t health; /* nsm_fpga_health */
};

/* The vector property an ARRAY rule publishes */
enum nsm_fpga_vector {
	NSM_FPGA_VECTOR_NONE = 0,
	NSM_FPGA_VECTOR_TX_POWER = 1, /* PortOpticalModuleMetrics TX power */
	NSM_FPGA_VECTOR_RX_POWER = 2, /* PortOpticalModuleMetrics RX power */
	NSM_FPGA_VECTOR_TX_FAULT = 3, /* OpticalLaneStatus TxFault */
	NSM_FPGA_VECTOR_RX_LOS = 4,   /* OpticalLaneStatus RxLossOfSignal */
};

/* Encoding of a numeric field in the capture buffer. FIXED is a bit field of
   a little-endian word: bit_shift, bit_width and is_signed of the spec. */
enum nsm_fpga_format {
	NSM_FPGA_FORMAT_FIXED = 0,
	NSM_FPGA_FORMAT_U16BE = 1,
	NSM_FPGA_FORMAT_S16BE = 2,
};

/* Which bytes a compare or decode reads: none, the item's own captures, or
   the group buffer filled by the group's setup rows. */
enum nsm_fpga_buffer {
	NSM_FPGA_BUF_NONE = 0,
	NSM_FPGA_BUF_ITEM = 1,
	NSM_FPGA_BUF_GROUP = 2,
	NSM_FPGA_BUF_WORD = 3, /* the word the last read returned; rows only */
};

enum nsm_fpga_compare_op {
	NSM_FPGA_CMP_EQ = 0,
	NSM_FPGA_CMP_GE = 1,
	NSM_FPGA_CMP_LE = 2,
};

/* ((bytes[offset..] as LE word & mask) op value). On run_if and valid_if a
   buffer of NONE means no condition; on expect a mask of 0 means no check and
   the compared word is the one just read. */
struct nsm_fpga_compare {
	uint8_t buffer;
	uint8_t op;
	uint8_t width; /* bytes read from the buffer, 1 to 4 */
	size_t offset;
	uint32_t mask;
	uint32_t value;
};

struct nsm_fpga_numeric_spec {
	const char *sensor_type;
	uint8_t format;
	uint8_t buffer; /* nsm_fpga_buffer the field is read from */
	size_t offset;
	uint8_t bit_shift;
	uint8_t bit_width;
	bool is_signed;
	double scale;
	double bias;
	double min_value;
	double max_value;
	/* ARRAY rules: element i starts stride_bits * i bits after offset */
	size_t element_count;
	size_t stride_bits;
	uint8_t vector; /* nsm_fpga_vector the elements are published on */
};

/* One CSR word access: READ32 or WRITE32 of a 32-bit register. */
struct nsm_fpga_csr_op {
	uint64_t address;
	uint32_t value;
};

/* I2C byte flags, the argument word bits 16 to 19 */
#define NSM_FPGA_I2C_READ 0x1u
#define NSM_FPGA_I2C_START 0x2u
#define NSM_FPGA_I2C_STOP 0x4u
#define NSM_FPGA_I2C_READ_NACK 0x8u

/* One I2C byte on an I2C station: a register read is a START write of the
   register followed by READ bytes, the last with STOP and READ_NACK. */
struct nsm_fpga_i2c_op {
	uint8_t dev_addr;
	uint8_t data;
	uint8_t flags;
};

/* POP: read status_address while (word & poll_mask) != 0, each time reading
   and discarding data_address; bounded by limit. */
struct nsm_fpga_pop_op {
	uint64_t status_address;
	uint64_t data_address;
};

/* One bounded I2C transaction: START, up to three write bytes, repeated
   START, up to four read bytes with NACK and STOP. captured is a bit mask of
   the write bytes taken from the group buffer, in order from captured_offset */
struct nsm_fpga_xfer_op {
	uint8_t dev_addr;
	uint8_t write[3];
	uint8_t write_count;
	uint8_t read_count;
	uint8_t captured;
	uint8_t captured_offset;
};

/* WRITE_CAPTURED: write bytes taken from the group buffer at offset; a word
   station writes one word at address, an I2C station one byte to dev_addr. */
struct nsm_fpga_captured_op {
	uint64_t address;
	uint8_t dev_addr;
	uint8_t flags;
	size_t offset;
};

/* nsmd procedure ops, above the wire opcodes; the executor runs them.
   ACK_CLEAR writes the last polled word back with poll_mask cleared. CHECK
   sends nothing: it evaluates expect on the buffers and applies
   on_expect_fail. */
#define NSM_FPGA_OP_ACK_CLEAR 0x80
#define NSM_FPGA_OP_POP 0x81
#define NSM_FPGA_OP_WRITE_CAPTURED 0x82
#define NSM_FPGA_OP_CHECK 0x83
#define NSM_FPGA_OP_I2C_XFER 0x84

/* AVMM no-response sentinel: a CSR read of an absent or unpowered slice returns
   this word, so decode it as no reading rather than real status bits */
#define NSM_FPGA_WORD_UNAVAILABLE 0xDEADC0DEu

/* How a read is repeated until its poll condition on poll_mask holds */
enum nsm_fpga_poll {
	NSM_FPGA_POLL_NONE = 0, /* read once */
	NSM_FPGA_POLL_SET = 1,	/* until (value & poll_mask) == poll_mask */
	NSM_FPGA_POLL_ANY = 2,	/* until (value & poll_mask) != 0 */
};

/* A guard row skipped while its station is clean, or the row leaving it so */
enum nsm_fpga_clean {
	NSM_FPGA_CLEAN_NONE = 0,
	NSM_FPGA_UNLESS_CLEAN = 1, /* skip while the station is clean */
	NSM_FPGA_LEAVES_CLEAN =
	    2, /* done in a good run, the station is clean */
};

/* What a failed expect does: fail this item, or end the group and publish
   group_state and group_health on every item of the group that has rows. */
enum nsm_fpga_on_fail {
	NSM_FPGA_FAIL_ITEM = 0,
	NSM_FPGA_END_GROUP = 1,
};

/* One command of a sequence; op picks the union member. */
struct nsm_fpga_command {
	uint8_t op;
	uint8_t station;
	uint8_t poll;	  /* nsm_fpga_poll */
	uint8_t capture;  /* nsm_fpga_buffer the read value is appended to */
	uint8_t remember; /* the word read is what ACK_CLEAR writes back */
	uint8_t clean;	  /* nsm_fpga_clean */
	uint32_t poll_mask;
	union {
		struct nsm_fpga_csr_op csr;
		struct nsm_fpga_i2c_op i2c;
		struct nsm_fpga_xfer_op xfer;
		struct nsm_fpga_pop_op pop;
		struct nsm_fpga_captured_op captured;
	} u;
	struct nsm_fpga_compare expect;
	uint8_t on_expect_fail; /* nsm_fpga_on_fail */
	uint8_t group_state;	/* nsm_fpga_state for END_GROUP */
	uint8_t group_health;	/* nsm_fpga_health for END_GROUP */
	uint32_t period_us;	/* poll period, 0 = default */
	uint32_t limit;		/* poll or pop bound, 0 = default */
	struct nsm_fpga_compare run_if;
};

/* Where an identity field is published */
enum nsm_fpga_asset_field {
	NSM_FPGA_ASSET_SERIAL = 0,
	NSM_FPGA_ASSET_MODEL = 1,
	NSM_FPGA_ASSET_PART = 2,
	NSM_FPGA_ASSET_MANUFACTURER = 3,
	NSM_FPGA_ASSET_REVISION = 4, /* the Revision decorator's Version */
};

/* How the bytes of an identity field read as text */
enum nsm_fpga_identity_format {
	NSM_FPGA_IDENTITY_HEX_LE =
	    0, /* one little-endian number, 0x prefixed */
	NSM_FPGA_IDENTITY_HEX_BE =
	    1, /* hex digits, first byte first, 0x prefixed */
	NSM_FPGA_IDENTITY_ASCII =
	    2, /* printable text, trailing spaces dropped */
};

/* One identity field: length bytes at offset of a buffer, published as text
   on the asset property; HEX_LE takes at most 8 bytes. */
struct nsm_fpga_identity_field {
	uint8_t asset;	/* nsm_fpga_asset_field */
	uint8_t format; /* nsm_fpga_identity_format */
	uint8_t buffer; /* nsm_fpga_buffer the bytes are read from */
	size_t offset;
	size_t length;
};

/* Longest identity text, the 0x prefix and the terminator included */
#define NSM_FPGA_IDENTITY_TEXT_MAX 64

/* One telemetry item: the name suffix an entity-manager entry carries. */
struct nsm_fpga_rule {
	const char *name;
	const struct nsm_fpga_command *commands; /* run in series */
	size_t command_count;
	/* run on every exit once the first write row was sent */
	const struct nsm_fpga_command *cleanup;
	size_t cleanup_count;
	uint8_t kind;
	struct nsm_fpga_numeric_spec numeric;
	const char *group; /* telemetry group: its items read in one round */
	/* STATE rules: the map from the read word to state and health */
	const struct nsm_fpga_state_entry *states;
	size_t state_count;
	struct nsm_fpga_compare valid_if; /* publish only when it holds */
	/* IDENTITY rules: the fields published; none means the serial number
	   as the first eight captured bytes in HEX_LE */
	const struct nsm_fpga_identity_field *identity;
	size_t identity_count;
	/* a compare on the group buffer; when it fails the item sends no row
	   and publishes unavailable this round */
	struct nsm_fpga_compare run_if;
};

/* One telemetry group: setup rows run once per round before its items and
   cleanup rows after the last. An IDENTITY item of a group runs until it
   has a reading and again after a round that ended the group as absent. */
struct nsm_fpga_group {
	const char *name;
	const struct nsm_fpga_command *setup;
	size_t setup_count;
	const struct nsm_fpga_command *cleanup;
	size_t cleanup_count;
};

/* The rules and groups of one telemetry part, generated from its rows file */
struct nsm_fpga_part {
	const struct nsm_fpga_rule *rules;
	size_t rule_count;
	const struct nsm_fpga_group *groups;
	size_t group_count;
};

/* Everything known about one FPGA build: window, stations and its parts */
struct nsm_fpga_map {
	struct nsm_fpga_window window;
	const struct nsm_fpga_station *stations;
	size_t station_count;
	const struct nsm_fpga_part *const *parts;
	size_t part_count;
};

/** @brief The P0 map, generated from its rows file. */
const struct nsm_fpga_map *nsm_fpga_map_p0(void);

/** @brief Station by selector, NULL when the map has none. */
const struct nsm_fpga_station *
nsm_fpga_station_by_selector(const struct nsm_fpga_map *map, uint8_t selector);

/** @brief Rule whose name ends the sensor name after a '_' or at 0. */
const struct nsm_fpga_rule *nsm_fpga_find_rule(const struct nsm_fpga_map *map,
					       const char *name);

/** @brief Group by name, NULL when the map defines none for it. */
const struct nsm_fpga_group *nsm_fpga_find_group(const struct nsm_fpga_map *map,
						 const char *name);

/** @brief The I2C byte argument word of an op: address, data and flags. */
uint32_t nsm_fpga_i2c_argument(const struct nsm_fpga_i2c_op *op);

/** @brief Wire opcode of a bounded transaction: 0x40 | writes << 3 | reads. */
uint8_t nsm_fpga_xfer_opcode(const struct nsm_fpga_xfer_op *op);

/** @brief Argument word of a bounded transaction: target then write bytes. */
uint32_t nsm_fpga_xfer_argument(uint8_t dev_addr, const uint8_t *write,
				size_t count);

/** @brief True when every row of the list addresses a legal target: a word
 *  op inside its word station window, a byte op on an I2C station, a
 *  procedure op on its station kind. */
bool nsm_fpga_commands_valid(const struct nsm_fpga_map *map,
			     const struct nsm_fpga_command *commands,
			     size_t count);

/** @brief True when the rule's rows and cleanup rows are valid; a rule
 *  with no rows is valid when its value comes from the group buffer. */
bool nsm_fpga_rule_valid(const struct nsm_fpga_map *map,
			 const struct nsm_fpga_rule *rule);

/** @brief The word a state decode tests: the first word of the item buffer,
 *  or for a rule with no rows the word at numeric.offset of the group buffer.
 *  False when the buffer is too short. */
bool nsm_fpga_rule_word(const struct nsm_fpga_rule *rule, const uint8_t *item,
			size_t item_len, const uint8_t *group, size_t group_len,
			uint32_t *word);

/** @brief Evaluate a compare against a buffer, or against value when the
 *  compare names no buffer. A compare that cannot read its bytes is false. */
bool nsm_fpga_compare_holds(const struct nsm_fpga_compare *compare,
			    const uint8_t *item, size_t item_len,
			    const uint8_t *group, size_t group_len,
			    uint32_t value);

/** @brief True when the poll condition on value holds for poll and mask. */
bool nsm_fpga_poll_satisfied(uint8_t poll, uint32_t poll_mask, uint32_t value);

/** @brief Decode the numeric field of spec from the buffers; false when the
 *  field does not fit, is not finite or lies outside min and max. */
bool nsm_fpga_decode_numeric(const struct nsm_fpga_numeric_spec *spec,
			     const uint8_t *item, size_t item_len,
			     const uint8_t *group, size_t group_len,
			     double *value);

/** @brief The identity fields of a rule, the default serial field when the
 *  rule names none; count returned through count. */
const struct nsm_fpga_identity_field *
nsm_fpga_rule_identity(const struct nsm_fpga_rule *rule, size_t *count);

/** @brief Render an identity field from the buffers into text; false when
 *  the bytes do not fit the buffer or text does not fit out. */
bool nsm_fpga_decode_identity(const struct nsm_fpga_identity_field *field,
			      const uint8_t *item, size_t item_len,
			      const uint8_t *group, size_t group_len, char *out,
			      size_t out_size);

/** @brief Decode element index of an ARRAY spec, same rules as numeric. A
 *  stride under eight bits walks the bits of a FIXED field, so bit_width 1
 *  with stride 1 reads a flag vector. */
bool nsm_fpga_decode_element(const struct nsm_fpga_numeric_spec *spec,
			     size_t index, const uint8_t *item, size_t item_len,
			     const uint8_t *group, size_t group_len,
			     double *value);

/** @brief True when a word op on the SDM mailbox station is allowed by its
 *  policy: reads only at offsets 8, 20, 24, 32; writes at offset 0 a length-1
 *  header for code 0x19 or 0x18, at offset 4 the value 1 or an argument-less
 *  header for code 0x10, 0x12 or 0x13; every write mask 0xf. Anything else
 *  faults the station and fences the grant, so it must never be sent. */
bool nsm_fpga_sdm_word_legal(uint8_t op, uint64_t address, uint32_t value,
			     uint8_t mask);

/** @brief True when a planned word op is legal for its station in this map:
 *  the station exists, the mask is 0xf, and either the address is aligned
 *  inside the station window (plain word station) or it satisfies the SDM
 *  mailbox policy. A planned word that fails this would fault the station, so
 *  the caller must log it and never publish it. */
bool nsm_fpga_word_legal(const struct nsm_fpga_map *map, uint8_t station,
			 uint8_t op, uint64_t address, uint32_t value,
			 uint8_t mask);

/** @brief State and health of word under the rule's state map; false when
 *  no entry matches, so the sensor publishes unavailable with a warning. */
bool nsm_fpga_decode_state(const struct nsm_fpga_rule *rule, uint32_t word,
			   uint8_t *state, uint8_t *health);

#ifdef __cplusplus
}
#endif
#endif

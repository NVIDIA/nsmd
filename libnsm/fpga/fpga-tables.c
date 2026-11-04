// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors
#include "fpga-tables.h"

#include <math.h>
#include <string.h>

/* Generic operations on an FPGA map: lookups, row validation, compares and the
 * numeric and state decodes. */
const struct nsm_fpga_station *
nsm_fpga_station_by_selector(const struct nsm_fpga_map *map, uint8_t selector)
{
	if (map == NULL) {
		return NULL;
	}
	for (size_t i = 0; i < map->station_count; ++i) {
		if (map->stations[i].selector == selector) {
			return &map->stations[i];
		}
	}
	return NULL;
}

/* True when suffix ends name at a '_' boundary or is the whole name */
static bool name_has_suffix(const char *name, const char *suffix)
{
	const size_t length = strlen(name);
	const size_t suffix_length = strlen(suffix);
	if (length < suffix_length) {
		return false;
	}
	const size_t start = length - suffix_length;
	if (strcmp(name + start, suffix) != 0) {
		return false;
	}
	return start == 0 || name[start - 1] == '_';
}

const struct nsm_fpga_rule *nsm_fpga_find_rule(const struct nsm_fpga_map *map,
					       const char *name)
{
	if (map == NULL || name == NULL) {
		return NULL;
	}
	for (size_t p = 0; p < map->part_count; ++p) {
		const struct nsm_fpga_part *part = map->parts[p];
		for (size_t i = 0; i < part->rule_count; ++i) {
			if (name_has_suffix(name, part->rules[i].name)) {
				return &part->rules[i];
			}
		}
	}
	return NULL;
}

const struct nsm_fpga_group *nsm_fpga_find_group(const struct nsm_fpga_map *map,
						 const char *name)
{
	if (map == NULL || name == NULL) {
		return NULL;
	}
	for (size_t p = 0; p < map->part_count; ++p) {
		const struct nsm_fpga_part *part = map->parts[p];
		for (size_t i = 0; i < part->group_count; ++i) {
			if (strcmp(part->groups[i].name, name) == 0) {
				return &part->groups[i];
			}
		}
	}
	return NULL;
}

uint32_t nsm_fpga_i2c_argument(const struct nsm_fpga_i2c_op *op)
{
	if (op == NULL) {
		return 0;
	}
	return (uint32_t)(op->dev_addr & 0x7f) | ((uint32_t)op->data << 8) |
	       ((uint32_t)(op->flags & 0xf) << 16);
}

uint8_t nsm_fpga_xfer_opcode(const struct nsm_fpga_xfer_op *op)
{
	if (op == NULL) {
		return 0;
	}
	return (uint8_t)(0x40 | ((op->write_count & 0x3) << 3) |
			 (op->read_count & 0x7));
}

uint32_t nsm_fpga_xfer_argument(uint8_t dev_addr, const uint8_t *write,
				size_t count)
{
	uint32_t argument = dev_addr & 0x7f;
	for (size_t i = 0; i < count && i < 3; ++i) {
		argument |= (uint32_t)write[i] << (8 * (i + 1));
	}
	return argument;
}

static bool word_in_station(const struct nsm_fpga_station *station,
			    uint64_t address)
{
	return (address & 3) == 0 && address >= station->first_byte &&
	       address <= station->last_byte &&
	       station->last_byte - address >= 3;
}

static bool compare_well_formed(const struct nsm_fpga_compare *compare)
{
	if (compare->buffer == NSM_FPGA_BUF_NONE) {
		return true;
	}
	if (compare->buffer == NSM_FPGA_BUF_WORD) {
		return compare->op <= NSM_FPGA_CMP_LE;
	}
	return compare->buffer <= NSM_FPGA_BUF_GROUP &&
	       compare->op <= NSM_FPGA_CMP_LE && compare->width >= 1 &&
	       compare->width <= 4;
}

bool nsm_fpga_commands_valid(const struct nsm_fpga_map *map,
			     const struct nsm_fpga_command *commands,
			     size_t count)
{
	if (map == NULL || (count > 0 && commands == NULL)) {
		return false;
	}
	for (size_t i = 0; i < count; ++i) {
		const struct nsm_fpga_command *cmd = &commands[i];
		const struct nsm_fpga_station *station =
		    nsm_fpga_station_by_selector(map, cmd->station);
		if (station == NULL || cmd->poll > NSM_FPGA_POLL_ANY ||
		    cmd->capture > NSM_FPGA_BUF_GROUP ||
		    cmd->on_expect_fail > NSM_FPGA_END_GROUP ||
		    cmd->group_state > NSM_FPGA_STATE_FAULT ||
		    cmd->group_health > NSM_FPGA_HEALTH_CRITICAL ||
		    !compare_well_formed(&cmd->run_if) ||
		    (cmd->expect.buffer != NSM_FPGA_BUF_NONE &&
		     !compare_well_formed(&cmd->expect))) {
			return false;
		}
		if (cmd->op == NSM_FPGA_OP_CHECK) {
			/* a check reads a buffer, never the wire */
			if (cmd->expect.buffer == NSM_FPGA_BUF_NONE) {
				return false;
			}
			continue;
		}
		const bool sdm = station->kind == NSM_FPGA_STATION_SDM;
		const bool word = station->kind == NSM_FPGA_STATION_WORD;
		switch (cmd->op) {
		case NSM_FPGA_OP_READ32:
		case NSM_FPGA_OP_WRITE32:
		case NSM_FPGA_OP_ACK_CLEAR:
			if (sdm) {
				/* ACK_CLEAR is sent as a write */
				if (!nsm_fpga_sdm_word_legal(
					cmd->op == NSM_FPGA_OP_READ32
					    ? NSM_FPGA_OP_READ32
					    : NSM_FPGA_OP_WRITE32,
					cmd->u.csr.address, cmd->u.csr.value,
					0xf)) {
					return false;
				}
			} else if (!word || !word_in_station(
						station, cmd->u.csr.address)) {
				return false;
			}
			break;
		case NSM_FPGA_OP_POP:
			if (sdm) {
				if (!nsm_fpga_sdm_word_legal(
					NSM_FPGA_OP_READ32,
					cmd->u.pop.status_address, 0, 0xf) ||
				    !nsm_fpga_sdm_word_legal(
					NSM_FPGA_OP_READ32,
					cmd->u.pop.data_address, 0, 0xf)) {
					return false;
				}
			} else if (!word ||
				   !word_in_station(
				       station, cmd->u.pop.status_address) ||
				   !word_in_station(station,
						    cmd->u.pop.data_address)) {
				return false;
			}
			if (cmd->poll_mask == 0) {
				return false;
			}
			break;
		case NSM_FPGA_OP_WRITE_CAPTURED:
			if (word) {
				if (!word_in_station(station,
						     cmd->u.captured.address)) {
					return false;
				}
			} else if (station->kind != NSM_FPGA_STATION_I2C_BYTE) {
				return false;
			}
			break;
		case NSM_FPGA_OP_BYTE:
			if (station->kind != NSM_FPGA_STATION_I2C_BYTE ||
			    !nsm_fpga_i2c_argument_valid(
				nsm_fpga_i2c_argument(&cmd->u.i2c))) {
				return false;
			}
			break;
		case NSM_FPGA_OP_I2C_XFER: {
			const struct nsm_fpga_xfer_op *x = &cmd->u.xfer;
			if (station->kind != NSM_FPGA_STATION_I2C_BYTE ||
			    x->dev_addr < 0x08 || x->dev_addr > 0x77 ||
			    x->write_count > 3 || x->read_count > 4 ||
			    x->write_count + x->read_count == 0 ||
			    x->captured >= (1u << x->write_count)) {
				return false;
			}
			break;
		}
		default:
			return false;
		}
	}
	return true;
}

static bool identity_well_formed(const struct nsm_fpga_identity_field *field)
{
	if (field->asset > NSM_FPGA_ASSET_REVISION ||
	    field->format > NSM_FPGA_IDENTITY_ASCII || field->length == 0 ||
	    (field->buffer != NSM_FPGA_BUF_ITEM &&
	     field->buffer != NSM_FPGA_BUF_GROUP)) {
		return false;
	}
	if (field->format == NSM_FPGA_IDENTITY_HEX_LE) {
		return field->length <= 8;
	}
	if (field->format == NSM_FPGA_IDENTITY_ASCII) {
		return field->length < NSM_FPGA_IDENTITY_TEXT_MAX;
	}
	/* the hex text, its 0x prefix and the terminator must fit */
	return field->length <= (NSM_FPGA_IDENTITY_TEXT_MAX - 3) / 2;
}

static bool identity_fields_valid(const struct nsm_fpga_rule *rule)
{
	if (rule->kind != NSM_FPGA_RULE_IDENTITY) {
		return rule->identity_count == 0;
	}
	size_t count = 0;
	const struct nsm_fpga_identity_field *fields =
	    nsm_fpga_rule_identity(rule, &count);
	if (fields == NULL || count == 0) {
		return false;
	}
	for (size_t i = 0; i < count; ++i) {
		if (!identity_well_formed(&fields[i])) {
			return false;
		}
	}
	return true;
}

/* An ARRAY spec names its elements and its vector property; a stride under
   a byte walks FIXED bits */
static bool array_well_formed(const struct nsm_fpga_rule *rule)
{
	const struct nsm_fpga_numeric_spec *spec = &rule->numeric;
	if (rule->kind != NSM_FPGA_RULE_ARRAY) {
		return spec->vector == NSM_FPGA_VECTOR_NONE;
	}
	if (spec->element_count == 0 || spec->stride_bits == 0 ||
	    spec->vector == NSM_FPGA_VECTOR_NONE ||
	    spec->vector > NSM_FPGA_VECTOR_RX_LOS) {
		return false;
	}
	return (spec->stride_bits % 8) == 0 ||
	       spec->format == NSM_FPGA_FORMAT_FIXED;
}

/* A STATE rule has a state map; no other kind does */
static bool states_well_formed(const struct nsm_fpga_rule *rule)
{
	if (rule->kind != NSM_FPGA_RULE_STATE) {
		return rule->state_count == 0;
	}
	if (rule->states == NULL || rule->state_count == 0) {
		return false;
	}
	for (size_t i = 0; i < rule->state_count; ++i) {
		if (rule->states[i].state > NSM_FPGA_STATE_FAULT ||
		    rule->states[i].health > NSM_FPGA_HEALTH_CRITICAL) {
			return false;
		}
	}
	return true;
}

bool nsm_fpga_rule_valid(const struct nsm_fpga_map *map,
			 const struct nsm_fpga_rule *rule)
{
	if (map == NULL || rule == NULL) {
		return false;
	}
	if (!identity_fields_valid(rule) || !array_well_formed(rule) ||
	    !states_well_formed(rule) || !compare_well_formed(&rule->run_if) ||
	    (rule->run_if.buffer != NSM_FPGA_BUF_NONE &&
	     rule->run_if.buffer != NSM_FPGA_BUF_GROUP)) {
		return false;
	}
	if (rule->command_count == 0 || rule->commands == NULL) {
		/* no rows: the value must come from the group buffer */
		return rule->command_count == 0 && rule->cleanup_count == 0 &&
		       rule->numeric.buffer == NSM_FPGA_BUF_GROUP &&
		       compare_well_formed(&rule->valid_if);
	}
	if (!compare_well_formed(&rule->valid_if)) {
		return false;
	}
	return nsm_fpga_commands_valid(map, rule->commands,
				       rule->command_count) &&
	       nsm_fpga_commands_valid(map, rule->cleanup, rule->cleanup_count);
}

bool nsm_fpga_rule_word(const struct nsm_fpga_rule *rule, const uint8_t *item,
			size_t item_len, const uint8_t *group, size_t group_len,
			uint32_t *word)
{
	if (rule == NULL || word == NULL) {
		return false;
	}
	const uint8_t *buffer = item;
	size_t length = item_len;
	size_t offset = 0;
	if (rule->command_count == 0) {
		buffer = group;
		length = group_len;
		offset = rule->numeric.offset;
	}
	if (buffer == NULL || offset >= length) {
		return false;
	}
	const size_t width = length - offset < 4 ? length - offset : 4;
	uint32_t value = 0;
	for (size_t i = 0; i < width; ++i) {
		value |= (uint32_t)buffer[offset + i] << (8 * i);
	}
	*word = value;
	return true;
}

/* Little-endian word of width bytes at offset, false when it does not fit */
static bool read_word(const uint8_t *buffer, size_t length, size_t offset,
		      size_t width, uint64_t *out)
{
	if (buffer == NULL || width == 0 || width > 8 || offset > length ||
	    width > length - offset) {
		return false;
	}
	uint64_t value = 0;
	for (size_t i = 0; i < width; ++i) {
		value |= (uint64_t)buffer[offset + i] << (8 * i);
	}
	*out = value;
	return true;
}

static bool select_buffer(uint8_t which, const uint8_t *item, size_t item_len,
			  const uint8_t *group, size_t group_len,
			  const uint8_t **buffer, size_t *length)
{
	if (which == NSM_FPGA_BUF_ITEM) {
		*buffer = item;
		*length = item_len;
		return true;
	}
	if (which == NSM_FPGA_BUF_GROUP) {
		*buffer = group;
		*length = group_len;
		return true;
	}
	return false;
}

bool nsm_fpga_compare_holds(const struct nsm_fpga_compare *compare,
			    const uint8_t *item, size_t item_len,
			    const uint8_t *group, size_t group_len,
			    uint32_t value)
{
	if (compare == NULL) {
		return false;
	}
	uint64_t word = value;
	if (compare->buffer != NSM_FPGA_BUF_NONE &&
	    compare->buffer != NSM_FPGA_BUF_WORD) {
		const uint8_t *buffer = NULL;
		size_t length = 0;
		if (!select_buffer(compare->buffer, item, item_len, group,
				   group_len, &buffer, &length) ||
		    !read_word(buffer, length, compare->offset, compare->width,
			       &word)) {
			return false;
		}
	}
	const uint32_t masked = (uint32_t)word & compare->mask;
	switch (compare->op) {
	case NSM_FPGA_CMP_EQ:
		return masked == compare->value;
	case NSM_FPGA_CMP_GE:
		return masked >= compare->value;
	case NSM_FPGA_CMP_LE:
		return masked <= compare->value;
	default:
		return false;
	}
}

bool nsm_fpga_poll_satisfied(uint8_t poll, uint32_t poll_mask, uint32_t value)
{
	switch (poll) {
	case NSM_FPGA_POLL_NONE:
		return true;
	case NSM_FPGA_POLL_SET:
		return (value & poll_mask) == poll_mask;
	case NSM_FPGA_POLL_ANY:
		return (value & poll_mask) != 0;
	default:
		return false;
	}
}

static size_t format_bytes(uint8_t format)
{
	switch (format) {
	case NSM_FPGA_FORMAT_U16BE:
	case NSM_FPGA_FORMAT_S16BE:
		return 2;
	case NSM_FPGA_FORMAT_FIXED:
		return 4;
	default:
		return 0;
	}
}

static int64_t sign_extend(uint64_t raw, unsigned bits)
{
	if (bits == 0 || bits >= 64) {
		return (int64_t)raw;
	}
	const uint64_t sign = (uint64_t)1 << (bits - 1);
	return (int64_t)((raw ^ sign) - sign);
}

/* Decode the field at byte offset of the spec's buffer into value; a FIXED
   field starts bit_shift bits into the word read there */
static bool decode_at(const struct nsm_fpga_numeric_spec *spec, size_t offset,
		      unsigned bit_shift, const uint8_t *item, size_t item_len,
		      const uint8_t *group, size_t group_len, double *value)
{
	const uint8_t *buffer = NULL;
	size_t length = 0;
	size_t width = format_bytes(spec->format);
	uint64_t raw = 0;
	if (spec->format == NSM_FPGA_FORMAT_FIXED) {
		if (spec->bit_width == 0 || spec->bit_width > 32 ||
		    bit_shift + spec->bit_width > 32) {
			return false;
		}
		/* only the bytes the field spans need to be in the buffer */
		width = (bit_shift + spec->bit_width + 7) / 8;
	}
	if (width == 0 ||
	    !select_buffer(spec->buffer, item, item_len, group, group_len,
			   &buffer, &length) ||
	    !read_word(buffer, length, offset, width, &raw)) {
		return false;
	}
	double decoded = 0.0;
	switch (spec->format) {
	case NSM_FPGA_FORMAT_U16BE:
		decoded = (double)(((raw & 0xff) << 8) | ((raw >> 8) & 0xff));
		break;
	case NSM_FPGA_FORMAT_S16BE:
		decoded = (double)sign_extend(
		    ((raw & 0xff) << 8) | ((raw >> 8) & 0xff), 16);
		break;
	case NSM_FPGA_FORMAT_FIXED: {
		const uint64_t field =
		    (raw >> bit_shift) & (((uint64_t)1 << spec->bit_width) - 1);
		decoded = spec->is_signed
			      ? (double)sign_extend(field, spec->bit_width)
			      : (double)field;
		break;
	}
	default:
		return false;
	}
	decoded = decoded * spec->scale + spec->bias;
	if (!isfinite(decoded) ||
	    (!isnan(spec->min_value) && decoded < spec->min_value) ||
	    (!isnan(spec->max_value) && decoded > spec->max_value)) {
		return false;
	}
	*value = decoded;
	return true;
}

bool nsm_fpga_decode_numeric(const struct nsm_fpga_numeric_spec *spec,
			     const uint8_t *item, size_t item_len,
			     const uint8_t *group, size_t group_len,
			     double *value)
{
	if (spec == NULL || value == NULL) {
		return false;
	}
	return decode_at(spec, spec->offset, spec->bit_shift, item, item_len,
			 group, group_len, value);
}

/* Without fields an identity rule publishes its first eight captured bytes */
static const struct nsm_fpga_identity_field default_identity[] = {
    {.asset = NSM_FPGA_ASSET_SERIAL,
     .format = NSM_FPGA_IDENTITY_HEX_LE,
     .buffer = NSM_FPGA_BUF_ITEM,
     .offset = 0,
     .length = 8}};

const struct nsm_fpga_identity_field *
nsm_fpga_rule_identity(const struct nsm_fpga_rule *rule, size_t *count)
{
	if (rule == NULL || count == NULL) {
		return NULL;
	}
	if (rule->identity == NULL || rule->identity_count == 0) {
		*count = 1;
		return default_identity;
	}
	*count = rule->identity_count;
	return rule->identity;
}

bool nsm_fpga_decode_identity(const struct nsm_fpga_identity_field *field,
			      const uint8_t *item, size_t item_len,
			      const uint8_t *group, size_t group_len, char *out,
			      size_t out_size)
{
	if (field == NULL || out == NULL || out_size == 0 ||
	    !identity_well_formed(field)) {
		return false;
	}
	const uint8_t *buffer = NULL;
	size_t length = 0;
	if (!select_buffer(field->buffer, item, item_len, group, group_len,
			   &buffer, &length) ||
	    buffer == NULL || field->offset > length ||
	    field->length > length - field->offset) {
		return false;
	}
	/* well formed bounds length, so the text sizes below cannot wrap */
	const uint8_t *bytes = buffer + field->offset;
	static const char digits[] = "0123456789abcdef";
	size_t n = 0;
	if (field->format == NSM_FPGA_IDENTITY_ASCII) {
		if (field->length >= out_size) {
			return false;
		}
		for (size_t i = 0; i < field->length; ++i) {
			/* the text ends at the first byte that is not printable
			 */
			if (bytes[i] < 0x20 || bytes[i] > 0x7e) {
				break;
			}
			out[n++] = (char)bytes[i];
		}
		while (n > 0 && out[n - 1] == ' ') {
			n--;
		}
		out[n] = '\0';
		return true;
	}
	if (out_size < field->length * 2 + 3) {
		return false;
	}
	out[n++] = '0';
	out[n++] = 'x';
	for (size_t i = 0; i < field->length; ++i) {
		/* HEX_LE prints the number, so its last byte comes first */
		const uint8_t b = field->format == NSM_FPGA_IDENTITY_HEX_LE
				      ? bytes[field->length - 1 - i]
				      : bytes[i];
		out[n++] = digits[b >> 4];
		out[n++] = digits[b & 0xf];
	}
	out[n] = '\0';
	return true;
}

bool nsm_fpga_decode_element(const struct nsm_fpga_numeric_spec *spec,
			     size_t index, const uint8_t *item, size_t item_len,
			     const uint8_t *group, size_t group_len,
			     double *value)
{
	if (spec == NULL || value == NULL || index >= spec->element_count) {
		return false;
	}
	if ((spec->stride_bits % 8) != 0 &&
	    spec->format != NSM_FPGA_FORMAT_FIXED) {
		return false;
	}
	/* the element's first bit, counted from the spec's byte offset */
	if (spec->stride_bits != 0 &&
	    index > (SIZE_MAX - spec->bit_shift) / spec->stride_bits) {
		return false;
	}
	const size_t bit = spec->bit_shift + index * spec->stride_bits;
	if (spec->offset > SIZE_MAX - bit / 8) {
		return false;
	}
	return decode_at(spec, spec->offset + bit / 8, (unsigned)(bit % 8),
			 item, item_len, group, group_len, value);
}

bool nsm_fpga_sdm_word_legal(uint8_t op, uint64_t address, uint32_t value,
			     uint8_t mask)
{
	if (op == NSM_FPGA_OP_READ32) {
		/* reads only at offsets 8, 20, 24, 32 */
		return address == 8 || address == 20 || address == 24 ||
		       address == 32;
	}
	if (op != NSM_FPGA_OP_WRITE32) {
		return false;
	}
	if (mask != 0xf) {
		return false;
	}
	const uint32_t code = value & 0x7ff;	       /* bits 0 to 10 */
	const uint32_t length = (value >> 12) & 0x7ff; /* bits 12 to 22 */
	if (address == 0) {
		/* header: code 0x19 or 0x18, length 1, reserved bits clear */
		if (code != 0x19 && code != 0x18) {
			return false;
		}
		if (length != 1) {
			return false;
		}
		return (value & ((1u << 11) | (1u << 23) | 0xf0000000u)) == 0;
	}
	if (address == 4) {
		/* the argument after a header, or an argument-less header */
		if (value == 0x00000001u) {
			return true;
		}
		if (length != 0) {
			return false;
		}
		return code == 0x10 || code == 0x12 || code == 0x13;
	}
	return false;
}

bool nsm_fpga_word_legal(const struct nsm_fpga_map *map, uint8_t station,
			 uint8_t op, uint64_t address, uint32_t value,
			 uint8_t mask)
{
	const struct nsm_fpga_station *target =
	    nsm_fpga_station_by_selector(map, station);
	if (target == NULL) {
		return false;
	}
	if (op != NSM_FPGA_OP_READ32 && op != NSM_FPGA_OP_WRITE32 &&
	    op != NSM_FPGA_OP_ACK_CLEAR) {
		return false;
	}
	if (target->kind == NSM_FPGA_STATION_SDM) {
		/* ACK_CLEAR is a masked write of the last polled word */
		const uint8_t word_op = op == NSM_FPGA_OP_READ32
					    ? NSM_FPGA_OP_READ32
					    : NSM_FPGA_OP_WRITE32;
		return nsm_fpga_sdm_word_legal(word_op, address, value, mask);
	}
	if (target->kind != NSM_FPGA_STATION_WORD) {
		return false;
	}
	if (mask != 0xf) {
		return false;
	}
	return word_in_station(target, address);
}

bool nsm_fpga_decode_state(const struct nsm_fpga_rule *rule, uint32_t word,
			   uint8_t *state, uint8_t *health)
{
	if (rule == NULL || state == NULL || health == NULL) {
		return false;
	}
	for (size_t i = 0; i < rule->state_count; ++i) {
		const struct nsm_fpga_state_entry *entry = &rule->states[i];
		if ((word & entry->mask) == entry->value) {
			*state = entry->state;
			*health = entry->health;
			return true;
		}
	}
	return false;
}

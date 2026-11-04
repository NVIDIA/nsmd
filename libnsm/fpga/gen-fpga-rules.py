#!/usr/bin/env python3
# Generate the C tables of an FPGA telemetry map or part from its rows file.
import argparse
import ast
import operator
import os
import sys

# The arithmetic a rows file may use in a number
BINARY_OPS = {
    ast.Add: operator.add,
    ast.Sub: operator.sub,
    ast.Mult: operator.mul,
    ast.Div: operator.truediv,
    ast.FloorDiv: operator.floordiv,
    ast.Mod: operator.mod,
    ast.LShift: operator.lshift,
    ast.RShift: operator.rshift,
    ast.BitOr: operator.or_,
    ast.BitAnd: operator.and_,
    ast.BitXor: operator.xor,
}
UNARY_OPS = {ast.USub: operator.neg, ast.UAdd: operator.pos,
             ast.Invert: operator.invert}

KINDS = {
    "state": "NSM_FPGA_RULE_STATE",
    "numeric": "NSM_FPGA_RULE_NUMERIC",
    "identity": "NSM_FPGA_RULE_IDENTITY",
    "array": "NSM_FPGA_RULE_ARRAY",
}
STATION_KINDS = {
    "i2c": "NSM_FPGA_STATION_I2C_BYTE",
    "word": "NSM_FPGA_STATION_WORD",
    "sdm": "NSM_FPGA_STATION_SDM",
}
BUFFERS = {"item": "NSM_FPGA_BUF_ITEM", "group": "NSM_FPGA_BUF_GROUP",
           "word": "NSM_FPGA_BUF_WORD"}
COMPARE_OPS = {
    "==": "NSM_FPGA_CMP_EQ",
    ">=": "NSM_FPGA_CMP_GE",
    "<=": "NSM_FPGA_CMP_LE",
}
POLLS = {
    "set": "NSM_FPGA_POLL_SET",
    "any": "NSM_FPGA_POLL_ANY",
}
STATES = {
    "enabled": "NSM_FPGA_STATE_ENABLED",
    "starting": "NSM_FPGA_STATE_STARTING",
    "standby": "NSM_FPGA_STATE_STANDBY_OFFLINE",
    "absent": "NSM_FPGA_STATE_ABSENT",
    "unavailable": "NSM_FPGA_STATE_UNAVAILABLE_OFFLINE",
    "fault": "NSM_FPGA_STATE_FAULT",
}
HEALTHS = {
    "ok": "NSM_FPGA_HEALTH_OK",
    "warning": "NSM_FPGA_HEALTH_WARNING",
    "critical": "NSM_FPGA_HEALTH_CRITICAL",
}
FORMATS = {
    "u16be": "NSM_FPGA_FORMAT_U16BE",
    "s16be": "NSM_FPGA_FORMAT_S16BE",
    "fixed": "NSM_FPGA_FORMAT_FIXED",
}
ASSETS = {
    "serial": "NSM_FPGA_ASSET_SERIAL",
    "model": "NSM_FPGA_ASSET_MODEL",
    "part": "NSM_FPGA_ASSET_PART",
    "manufacturer": "NSM_FPGA_ASSET_MANUFACTURER",
    "revision": "NSM_FPGA_ASSET_REVISION",
}
TEXT_FORMATS = {
    "hex_le": "NSM_FPGA_IDENTITY_HEX_LE",
    "hex_be": "NSM_FPGA_IDENTITY_HEX_BE",
    "ascii": "NSM_FPGA_IDENTITY_ASCII",
}
VECTORS = {
    "tx_power": "NSM_FPGA_VECTOR_TX_POWER",
    "rx_power": "NSM_FPGA_VECTOR_RX_POWER",
    "tx_fault": "NSM_FPGA_VECTOR_TX_FAULT",
    "rx_los": "NSM_FPGA_VECTOR_RX_LOS",
}
I2C_FLAGS = {"start": 0x2, "stop": 0x4, "nack": 0x8}
ROW_OPS = (
    "read32", "write32", "ack_clear", "pop", "write", "read", "reads",
    "write_captured", "check", "xfer",
)


class RowsError(Exception):
    pass


def hexs(value):
    return hex(value) if value > 9 else str(value)


class Compare:
    def __init__(self):
        self.buffer = None
        self.op = "=="
        self.width = 0
        self.offset = 0
        self.mask = 0
        self.value = 0

    def empty(self):
        return self.buffer is None and self.mask == 0

    def emit(self):
        parts = []
        if self.buffer:
            parts.append(".buffer = " + BUFFERS[self.buffer])
        if self.op != "==":
            parts.append(".op = " + COMPARE_OPS[self.op])
        if self.width:
            parts.append(".width = %d" % self.width)
        if self.offset:
            parts.append(".offset = %d" % self.offset)
        parts.append(".mask = " + hexs(self.mask))
        parts.append(".value = " + hexs(self.value))
        return "{" + ", ".join(parts) + "}"


class Row:
    def __init__(self):
        self.op = None
        self.station = 0
        self.poll = None
        self.poll_mask = 0
        self.capture = None
        self.remember = False
        self.clean = None
        self.union = None
        self.expect = Compare()
        self.end_group = None
        self.period = 0
        self.limit = 0
        self.run_if = Compare()

    def emit(self):
        parts = [".op = " + self.op, ".station = %d" % self.station]
        if self.poll:
            parts.append(".poll = " + POLLS[self.poll])
        if self.capture:
            parts.append(".capture = " + BUFFERS[self.capture])
        if self.remember:
            parts.append(".remember = 1")
        if self.clean:
            parts.append(".clean = " + self.clean)
        if self.poll_mask:
            parts.append(".poll_mask = " + hexs(self.poll_mask))
        if self.union:
            parts.append(".u = " + self.union)
        if not self.expect.empty():
            parts.append(".expect = " + self.expect.emit())
        if self.end_group:
            state, health = self.end_group
            parts.append(".on_expect_fail = NSM_FPGA_END_GROUP")
            parts.append(".group_state = " + STATES[state])
            parts.append(".group_health = " + HEALTHS[health])
        if self.period:
            parts.append(".period_us = %d" % self.period)
        if self.limit:
            parts.append(".limit = %d" % self.limit)
        if not self.run_if.empty():
            parts.append(".run_if = " + self.run_if.emit())
        return "{" + ", ".join(parts) + "}"


class Rule:
    def __init__(self, name):
        self.name = name
        self.kind = None
        self.sensor_type = None
        self.vector = None
        self.group = None
        self.decode = None
        self.states = []
        self.fields = []
        self.valid_if = Compare()
        self.run_if = Compare()
        self.rows = []
        self.cleanup = []


class Group:
    def __init__(self, name):
        self.name = name
        self.setup = []
        self.cleanup = []


class Parser:
    def __init__(self, text, directory="."):
        self.lines = text.splitlines()
        self.directory = directory
        self.consts = {}
        self.seqs = {}
        self.stations = []
        self.selectors = {}
        self.rules = []
        self.groups = []
        self.map_name = None
        self.part_name = None
        self.parts = []
        self.window = None
        self.line_no = 0

    def fail(self, message):
        raise RowsError("line %d: %s" % (self.line_no, message))

    def tokens(self, line):
        line = line.split("#", 1)[0].strip()
        return line.split() if line else []

    def value(self, token, env):
        for name, replacement in env.items():
            token = token.replace("$" + name, replacement)
        if token.startswith("$"):
            self.fail("unknown parameter " + token)
        try:
            return self.evaluate(ast.parse(token, mode="eval").body)
        except (SyntaxError, ValueError, KeyError, ZeroDivisionError,
                TypeError):
            self.fail("cannot evaluate " + token)

    def evaluate(self, node):
        if isinstance(node, ast.Constant) and isinstance(node.value,
                                                         (int, float)):
            return node.value
        if isinstance(node, ast.Name):
            return self.consts[node.id]
        if isinstance(node, ast.UnaryOp) and type(node.op) in UNARY_OPS:
            return UNARY_OPS[type(node.op)](self.evaluate(node.operand))
        if isinstance(node, ast.BinOp) and type(node.op) in BINARY_OPS:
            return BINARY_OPS[type(node.op)](self.evaluate(node.left),
                                             self.evaluate(node.right))
        raise ValueError("unsupported expression")

    def integer(self, token, env):
        result = self.value(token, env)
        if isinstance(result, float) and result.is_integer():
            result = int(result)
        if not isinstance(result, int):
            self.fail("not an integer: " + token)
        return result

    def word(self, token, env):
        for name, replacement in env.items():
            token = token.replace("$" + name, replacement)
        return token

    def station(self, token, env):
        name = self.word(token, env)
        if name in self.selectors:
            return self.selectors[name]
        self.fail("unknown station " + name)

    def include(self, path):
        full = os.path.join(self.directory, path)
        try:
            with open(full) as handle:
                text = handle.read()
        except OSError:
            self.fail("cannot read " + path)
        other = Parser(text, os.path.dirname(full)).parse()
        self.consts.update(other.consts)
        self.selectors.update(other.selectors)

    def compare(self, toks, i, env):
        cmp = Compare()
        if toks[i] == "word":
            cmp.buffer = toks[i]
            i += 1
        elif toks[i] in BUFFERS:
            cmp.buffer = toks[i]
            cmp.offset = self.integer(toks[i + 1], env)
            cmp.width = 1
            i += 2
            if toks[i] == "w":
                cmp.width = self.integer(toks[i + 1], env)
                i += 2
        if toks[i] != "&":
            self.fail("compare expects '&' before the mask")
        cmp.mask = self.integer(toks[i + 1], env)
        if toks[i + 2] not in COMPARE_OPS:
            self.fail("compare expects ==, >= or <=")
        cmp.op = toks[i + 2]
        cmp.value = self.integer(toks[i + 3], env)
        return cmp, i + 4

    def parse(self):
        block = None
        section = None
        body = []
        for raw in self.lines:
            self.line_no += 1
            toks = self.tokens(raw)
            if not toks:
                continue
            head = toks[0]
            if block is None:
                if head == "const":
                    self.consts[toks[1]] = self.value(toks[2], {})
                elif head == "station":
                    if self.part_name:
                        self.fail("stations belong in the map file")
                    selector = self.integer(toks[2], {})
                    self.selectors[toks[1]] = selector
                    self.stations.append((
                        selector, STATION_KINDS[toks[3]],
                        self.integer(toks[4], {}), self.integer(toks[5], {})))
                elif head == "include":
                    self.include(toks[1])
                elif head == "seq":
                    block = ("seq", toks[1], toks[2:])
                    body = []
                elif head == "rule":
                    if self.map_name:
                        self.fail("rules belong in a part file")
                    block = ("rule", Rule(toks[1]))
                    section = "rows"
                elif head == "group":
                    if self.map_name:
                        self.fail("groups belong in a part file")
                    block = ("group", Group(toks[1]))
                    section = "setup"
                elif head == "map":
                    if self.part_name:
                        self.fail("a part file has no map")
                    block = ("map",)
                    self.map_name = toks[1]
                elif head == "part":
                    if self.map_name or self.part_name:
                        self.fail("one part per file")
                    self.part_name = toks[1]
                else:
                    self.fail("unexpected " + head)
                continue
            if head == "end":
                if block[0] == "seq":
                    self.seqs[block[1]] = (block[2], body)
                elif block[0] == "rule":
                    self.rules.append(block[1])
                elif block[0] == "group":
                    self.groups.append(block[1])
                block = None
                continue
            if block[0] == "seq":
                body.append((self.line_no, toks))
            elif block[0] == "map":
                if head == "window":
                    self.window = [self.integer(t, {}) for t in toks[1:]]
                    if len(self.window) != 7:
                        self.fail("window takes type and six addresses")
                elif head == "part":
                    self.parts.append(toks[1])
                else:
                    self.fail("unexpected " + head + " in map")
            else:
                section = self.line(block[1], section, toks, {}, None)
        if block is not None:
            self.fail("block not closed")
        if self.map_name is None and self.part_name is None:
            self.fail("neither map nor part")
        if self.map_name and self.window is None:
            self.fail("map without window")
        return self

    def line(self, target, section, toks, env, gate):
        head = toks[0]
        if head in ("rows", "cleanup", "setup"):
            return head
        if head == "use":
            return self.use(target, section, toks, env, gate)
        if head in ROW_OPS:
            rows = self.row(toks, env)
            for row in rows:
                if gate is not None:
                    if not row.run_if.empty():
                        self.fail("a row inside a gated use cannot carry if")
                    row.run_if = gate
            self.rows_of(target, section).extend(rows)
            return section
        if isinstance(target, Group):
            self.fail("groups hold rows only")
        self.attribute(target, toks, env)
        return section

    def rows_of(self, target, section):
        if isinstance(target, Group):
            return target.setup if section == "setup" else target.cleanup
        return target.rows if section == "rows" else target.cleanup

    def use(self, target, section, toks, env, gate):
        if toks[1] not in self.seqs:
            self.fail("unknown seq " + toks[1])
        params, body = self.seqs[toks[1]]
        args = toks[2:]
        inner_gate = gate
        if "if" in args:
            at = args.index("if")
            inner_gate, _ = self.compare(args, at + 1, env)
            args = args[:at]
        if len(args) != len(params):
            self.fail("seq %s takes %d arguments" % (toks[1], len(params)))
        inner = dict(env)
        for name, arg in zip(params, args):
            inner[name] = self.word(arg, env)
        saved = self.line_no
        for line_no, body_toks in body:
            self.line_no = line_no
            section = self.line(target, section, body_toks, inner, inner_gate)
        self.line_no = saved
        return section

    def attribute(self, rule, toks, env):
        head = toks[0]
        if head == "kind":
            rule.kind = toks[1]
            if toks[1] == "numeric":
                rule.sensor_type = self.word(toks[2], env)
            elif toks[1] == "array":
                rule.vector = self.word(toks[2], env)
            elif toks[1] not in KINDS:
                self.fail("unknown kind " + toks[1])
        elif head == "group":
            rule.group = self.word(toks[1], env)
        elif head == "word":
            rule.decode = {"buffer": toks[1],
                           "offset": self.integer(toks[2], env)}
        elif head == "decode":
            rule.decode = self.decode(toks, env)
        elif head == "state":
            if toks[1] == "default":
                rule.states.append((0, 0, toks[2], toks[3]))
            else:
                rule.states.append((self.integer(toks[1], env),
                                    self.integer(toks[2], env), toks[3],
                                    toks[4]))
        elif head == "field":
            rule.fields.append((toks[1], toks[2], toks[3],
                                self.integer(toks[4], env),
                                self.integer(toks[5], env)))
        elif head == "valid_if":
            rule.valid_if, _ = self.compare(toks, 1, env)
        elif head == "run_if":
            rule.run_if, _ = self.compare(toks, 1, env)
        else:
            self.fail("unexpected " + head)

    def decode(self, toks, env):
        spec = {"format": toks[1], "buffer": toks[2],
                "offset": self.integer(toks[3], env), "scale": 1.0}
        i = 4
        while i < len(toks):
            key = toks[i]
            if key == "signed":
                spec["signed"] = True
                i += 1
            elif key == "range":
                spec["min"] = float(self.value(toks[i + 1], env))
                spec["max"] = float(self.value(toks[i + 2], env))
                i += 3
            elif key in ("scale", "bias"):
                spec[key] = float(self.value(toks[i + 1], env))
                i += 2
            elif key in ("shift", "width", "count", "stride"):
                spec[key] = self.integer(toks[i + 1], env)
                i += 2
            else:
                self.fail("unexpected " + key + " in decode")
        return spec

    def xfer(self, row, toks, i, env):
        device = self.integer(toks[i], env)
        i += 1
        writes = []
        captured = 0
        reads = 0
        while i < len(toks) and toks[i] in ("w", "r", "wc"):
            key = toks[i]
            i += 1
            if key == "r":
                reads = self.integer(toks[i], env)
                i += 1
            elif key == "wc":
                captured |= 1 << len(writes)
                writes.append("group:%d" % self.integer(toks[i], env))
                i += 1
            else:
                while i < len(toks) and toks[i] not in (
                        "r", "wc", "capture", "remember", "expect", "if",
                        "else", "poll", "period", "limit", "unless_clean",
                        "leaves_clean"):
                    writes.append(hexs(self.integer(toks[i], env)))
                    i += 1
        if len(writes) > 3 or not (writes or reads):
            self.fail("xfer takes up to three write bytes and some bytes")
        if reads > 4:
            # a long register read: one pointer byte, four bytes per step
            if len(writes) != 1 or captured:
                self.fail("a read over four bytes needs one pointer byte")
            pointer = self.integer(writes[0], {})
            rows = []
            for offset in range(0, reads, 4):
                part = Row()
                part.station = row.station
                part.op = "NSM_FPGA_OP_I2C_XFER"
                part.union = self.xfer_union(
                    device, [hexs(pointer + offset)], 0,
                    min(4, reads - offset))
                rows.append(part)
            return rows, i
        row.op = "NSM_FPGA_OP_I2C_XFER"
        row.union = self.xfer_union(device, writes, captured, reads)
        return [row], i

    def xfer_union(self, device, writes, captured, reads):
        offset = 0
        taken = 0
        values = []
        for value in writes:
            if value.startswith("group:"):
                at = int(value.split(":")[1])
                if taken == 0:
                    offset = at
                elif at != offset + taken:
                    self.fail("captured write bytes must be consecutive")
                taken += 1
                values.append("0")
            else:
                values.append(value)
        values += ["0"] * (3 - len(values))
        return ("{.xfer = {%s, {%s}, %d, %d, %d, %d}}" % (
            hexs(device), ", ".join(values), len(writes), reads, captured,
            offset))

    def row(self, toks, env):
        row = Row()
        head = toks[0]
        row.station = self.station(toks[1], env)
        i = 2
        rows = [row]
        if head in ("read32", "write32", "ack_clear"):
            address = self.integer(toks[i], env)
            i += 1
            value = 0
            if head == "write32":
                value = self.integer(toks[i], env)
                i += 1
            if head == "ack_clear":
                row.poll_mask = self.integer(toks[i], env)
                i += 1
            row.op = {"read32": "NSM_FPGA_OP_READ32",
                      "write32": "NSM_FPGA_OP_WRITE32",
                      "ack_clear": "NSM_FPGA_OP_ACK_CLEAR"}[head]
            row.union = "{.csr = {%s, %s}}" % (hexs(address), hexs(value))
        elif head == "pop":
            row.op = "NSM_FPGA_OP_POP"
            status = self.integer(toks[i], env)
            row.poll_mask = self.integer(toks[i + 1], env)
            data = self.integer(toks[i + 2], env)
            row.union = "{.pop = {%s, %s}}" % (hexs(status), hexs(data))
            i += 3
        elif head in ("write", "read", "reads"):
            device = self.integer(toks[i], env)
            i += 1
            data = 0
            count = 1
            if head == "write":
                data = self.integer(toks[i], env)
                i += 1
            if head == "reads":
                count = self.integer(toks[i], env)
                i += 1
            flags = 0x1 if head != "write" else 0
            while i < len(toks) and toks[i] in I2C_FLAGS:
                flags |= I2C_FLAGS[toks[i]]
                i += 1
            if head == "reads":
                rows = []
                for n in range(count):
                    part = Row()
                    part.station = row.station
                    part.op = "NSM_FPGA_OP_BYTE"
                    f = 0x1
                    if n == 0:
                        f |= 0x2
                    if n == count - 1:
                        f |= 0x4 | 0x8
                    part.union = "{.i2c = {%s, 0, %s}}" % (hexs(device),
                                                          hexs(f))
                    rows.append(part)
            else:
                row.op = "NSM_FPGA_OP_BYTE"
                row.union = "{.i2c = {%s, %s, %s}}" % (hexs(device),
                                                       hexs(data), hexs(flags))
        elif head == "write_captured":
            row.op = "NSM_FPGA_OP_WRITE_CAPTURED"
            if toks[i] == "word":
                address = self.integer(toks[i + 1], env)
                offset = self.integer(toks[i + 2], env)
                row.union = "{.captured = {.address = %s, .offset = %d}}" % (
                    hexs(address), offset)
                i += 3
            else:
                device = self.integer(toks[i + 1], env)
                offset = self.integer(toks[i + 2], env)
                i += 3
                flags = 0
                while i < len(toks) and toks[i] in I2C_FLAGS:
                    flags |= I2C_FLAGS[toks[i]]
                    i += 1
                row.union = ("{.captured = {.dev_addr = %s, .flags = %s, "
                             ".offset = %d}}" % (hexs(device), hexs(flags),
                                                 offset))
        elif head == "check":
            row.op = "NSM_FPGA_OP_CHECK"
            row.expect, i = self.compare(toks, i, env)
        elif head == "xfer":
            rows, i = self.xfer(row, toks, i, env)
        # An expanded read replaces the row: refuse modifiers it would lose
        expanded = row not in rows
        while i < len(toks):
            key = toks[i]
            if expanded and key in ("poll", "remember", "expect", "else",
                                    "period", "limit", "unless_clean",
                                    "leaves_clean"):
                self.fail(key + " is not supported on a read over four bytes")
            if key == "poll":
                row.poll = toks[i + 1]
                row.poll_mask = self.integer(toks[i + 2], env)
                i += 3
            elif key == "capture":
                for part in rows:
                    part.capture = toks[i + 1]
                i += 2
            elif key == "remember":
                row.remember = True
                i += 1
            elif key in ("unless_clean", "leaves_clean"):
                row.clean = "NSM_FPGA_" + key.upper()
                i += 1
            elif key == "expect":
                row.expect, i = self.compare(toks, i + 1, env)
            elif key == "else":
                if toks[i + 1] != "end_group":
                    self.fail("unknown else action " + toks[i + 1])
                row.end_group = (toks[i + 2], toks[i + 3])
                i += 4
            elif key == "if":
                gate, i = self.compare(toks, i + 1, env)
                for part in rows:
                    part.run_if = gate
            elif key == "period":
                row.period = self.integer(toks[i + 1], env)
                i += 2
            elif key == "limit":
                row.limit = self.integer(toks[i + 1], env)
                i += 2
            else:
                self.fail("unexpected " + key + " in row")
        return rows


class Emitter:
    def __init__(self, parsed, source_name):
        self.p = parsed
        self.source = source_name
        self.out = []
        self.arrays = {}

    def array(self, kind, body, name):
        key = (kind, tuple(body))
        if key in self.arrays:
            return self.arrays[key]
        self.arrays[key] = name
        self.out.append("static const %s %s[] = {" % (kind, name))
        for item in body:
            self.out.append("    " + item + ",")
        self.out.append("};")
        self.out.append("")
        return name

    def count(self, name):
        return "sizeof(%s) / sizeof(%s[0])" % (name, name)

    def rule(self, rule):
        name = rule.name
        parts = ['.name = "%s"' % name]
        if rule.rows:
            rows = self.array("struct nsm_fpga_command",
                              [r.emit() for r in rule.rows], "rows_" + name)
            parts.append(".commands = " + rows)
            parts.append(".command_count = " + self.count(rows))
        if rule.cleanup:
            end = self.array("struct nsm_fpga_command",
                             [r.emit() for r in rule.cleanup], "end_" + name)
            parts.append(".cleanup = " + end)
            parts.append(".cleanup_count = " + self.count(end))
        parts.append(".kind = " + KINDS[rule.kind])
        numeric = self.numeric(rule)
        if numeric:
            parts.append(".numeric = {" + ", ".join(numeric) + "}")
        if rule.group:
            parts.append('.group = "%s"' % rule.group)
        if rule.states:
            states = self.array("struct nsm_fpga_state_entry", [
                "{%s, %s, %s, %s}" % (hexs(m), hexs(v), STATES[s], HEALTHS[h])
                for m, v, s, h in rule.states], "states_" + name)
            parts.append(".states = " + states)
            parts.append(".state_count = " + self.count(states))
        if not rule.valid_if.empty():
            parts.append(".valid_if = " + rule.valid_if.emit())
        if rule.fields:
            fields = self.array("struct nsm_fpga_identity_field", [
                "{.asset = %s, .format = %s, .buffer = %s, .offset = %d, "
                ".length = %d}" % (ASSETS[a], TEXT_FORMATS[f], BUFFERS[b], o,
                                   n) for a, f, b, o, n in rule.fields],
                "fields_" + name)
            parts.append(".identity = " + fields)
            parts.append(".identity_count = " + self.count(fields))
        if not rule.run_if.empty():
            parts.append(".run_if = " + rule.run_if.emit())
        return "{" + ",\n     ".join(parts) + "}"

    def numeric(self, rule):
        parts = []
        if rule.sensor_type:
            parts.append('.sensor_type = "%s"' % rule.sensor_type)
        spec = rule.decode
        if spec:
            if "format" in spec:
                parts.append(".format = " + FORMATS[spec["format"]])
            parts.append(".buffer = " + BUFFERS[spec["buffer"]])
            if spec["offset"]:
                parts.append(".offset = %d" % spec["offset"])
            if spec.get("shift"):
                parts.append(".bit_shift = %d" % spec["shift"])
            if spec.get("width"):
                parts.append(".bit_width = %d" % spec["width"])
            if spec.get("signed"):
                parts.append(".is_signed = true")
            if "scale" in spec:
                parts.append(".scale = %r" % spec["scale"])
            if spec.get("bias"):
                parts.append(".bias = %r" % spec["bias"])
            if "min" in spec:
                parts.append(".min_value = %r" % spec["min"])
                parts.append(".max_value = %r" % spec["max"])
            elif "format" in spec:
                parts.append(".min_value = NAN")
                parts.append(".max_value = NAN")
            if spec.get("count"):
                parts.append(".element_count = %d" % spec["count"])
            if spec.get("stride"):
                parts.append(".stride_bits = %d" % spec["stride"])
        if rule.vector:
            parts.append(".vector = " + VECTORS[rule.vector])
        return parts

    def header(self):
        self.out.append("// SPDX-License-Identifier: Apache-2.0")
        self.out.append("// SPDX-FileCopyrightText: Copyright OpenBMC Authors")
        self.out.append("/* clang-format off */")
        self.out.append("/* Generated from %s by gen-fpga-rules.py, do not "
                        "edit. */" % self.source)
        self.out.append('#include "fpga-tables.h"')
        self.out.append("")
        self.out.append("#include <math.h>")
        self.out.append("")

    def emit(self):
        self.header()
        if self.p.map_name:
            self.emit_map()
        else:
            self.emit_part()
        return "\n".join(self.out) + "\n"

    def emit_map(self):
        p = self.p
        name = p.map_name
        self.out.append("static const struct nsm_fpga_station stations_%s[] = {"
                        % name)
        for selector, kind, first, last in p.stations:
            self.out.append("    {%d, %s, %s, %s}," % (
                selector, kind, hexs(first), hexs(last)))
        self.out.append("};")
        self.out.append("")
        for part in p.parts:
            self.out.append("extern const struct nsm_fpga_part nsm_fpga_part_%s;"
                            % part)
        if p.parts:
            self.out.append("")
            self.out.append("static const struct nsm_fpga_part *const "
                            "parts_%s[] = {" % name)
            for part in p.parts:
                self.out.append("    &nsm_fpga_part_%s," % part)
            self.out.append("};")
            self.out.append("")
        w = p.window
        self.out.append("static const struct nsm_fpga_map map_%s = {" % name)
        self.out.append("    .window = {%d, %s, %s, %s, %s, %s, %s},"
                        % (w[0], hexs(w[1]), hexs(w[2]), hexs(w[3]),
                           hexs(w[4]), hexs(w[5]), hexs(w[6])))
        self.out.append("    .stations = stations_%s," % name)
        self.out.append("    .station_count = %s," % self.count(
            "stations_" + name))
        if p.parts:
            self.out.append("    .parts = parts_%s," % name)
            self.out.append("    .part_count = %s," % self.count(
                "parts_" + name))
        self.out.append("};")
        self.out.append("")
        self.out.append("const struct nsm_fpga_map *nsm_fpga_map_%s(void)"
                        % name)
        self.out.append("{")
        self.out.append("\treturn &map_%s;" % name)
        self.out.append("}")

    def emit_part(self):
        p = self.p
        name = p.part_name
        rules = [self.rule(r) for r in p.rules]
        if rules:
            self.out.append("static const struct nsm_fpga_rule rules_%s[] = {"
                            % name)
            for text in rules:
                self.out.append("    " + text + ",")
            self.out.append("};")
            self.out.append("")
        groups = []
        for group in p.groups:
            parts = ['.name = "%s"' % group.name]
            if group.setup:
                setup = self.array("struct nsm_fpga_command",
                                   [r.emit() for r in group.setup],
                                   "setup_" + group.name)
                parts.append(".setup = " + setup)
                parts.append(".setup_count = " + self.count(setup))
            if group.cleanup:
                end = self.array("struct nsm_fpga_command",
                                 [r.emit() for r in group.cleanup],
                                 "cleanup_" + group.name)
                parts.append(".cleanup = " + end)
                parts.append(".cleanup_count = " + self.count(end))
            groups.append("{" + ",\n     ".join(parts) + "}")
        if groups:
            self.out.append("static const struct nsm_fpga_group groups_%s[] = {"
                            % name)
            for text in groups:
                self.out.append("    " + text + ",")
            self.out.append("};")
            self.out.append("")
        symbol = "nsm_fpga_part_" + name
        self.out.append("extern const struct nsm_fpga_part %s;" % symbol)
        self.out.append("const struct nsm_fpga_part %s = {" % symbol)
        if rules:
            self.out.append("    .rules = rules_%s," % name)
            self.out.append("    .rule_count = %s," % self.count(
                "rules_" + name))
        if groups:
            self.out.append("    .groups = groups_%s," % name)
            self.out.append("    .group_count = %s," % self.count(
                "groups_" + name))
        self.out.append("};")


def main():
    parser = argparse.ArgumentParser(
        description="Generate the C tables of a map or part from its rows file")
    parser.add_argument("rows", help="the rows file")
    parser.add_argument("output", help="the C file to write")
    parser.add_argument("--check", action="store_true",
                        help="only report whether the C file is up to date")
    args = parser.parse_args()
    with open(args.rows) as handle:
        text = handle.read()
    try:
        parsed = Parser(text, os.path.dirname(args.rows)).parse()
    except RowsError as error:
        print("%s: %s" % (args.rows, error), file=sys.stderr)
        return 2
    source = args.rows.rsplit("/", 1)[-1]
    generated = Emitter(parsed, source).emit()
    if args.check:
        with open(args.output) as handle:
            current = handle.read()
        if current != generated:
            print("%s is out of date: run gen-fpga-rules.py %s %s" % (
                args.output, args.rows, args.output), file=sys.stderr)
            return 1
        return 0
    with open(args.output, "w") as handle:
        handle.write(generated)
    return 0


if __name__ == "__main__":
    sys.exit(main())

# FPGA telemetry map

FPGA telemetry is data driven. A rows file lists, for every telemetry item,
the FPGA commands to send, how to wait on them, which bytes to keep and how to
turn them into a reading. libnsm runs those rows one MMIO frame at a time and
nsmd only sends the frames and publishes the result. Adding telemetry is a
rows change, not an engine change.

## Files

- `fpga-rules-lp30.rows` - the LP30 map: window, stations and the list of
  parts.
- `fpga-rules-lp30-<part>.rows` - one part per telemetry group: its rules
  and groups. These are the files to edit.
- `gen-fpga-rules.py` - turns a rows file into its C tables. Run
  `python3 gen-fpga-rules.py <name>.rows <name>.c` after editing; the test
  `libnsm_fpga_generated_<name>` fails when the C file is stale.
- `fpga-rules-lp30*.c` - generated, never edited by hand.
- `fpga-tables.h` / `.c` - the table model (stations, rows, compares, rule
  kinds, state maps, numeric and identity decodes) and its validator.
- `fpga-run.h` / `.c` - the row runner: hands nsmd the next frame, takes the
  reply back, applies poll, expect, capture and run_if, fences on a lost grant.
- `fpga-controller.h` / `.c` - the record codec and the reply classifier.
- nsmd side: `nsmFpgaController` (one session per FPGA, the send and reply
  loop), `nsmFpgaSensors` (publish per rule kind), `nsmFpgaStatus` and
  `nsmFpgaDiscoveryCheck`.

## Rows file

Lines are tokens separated by spaces; `#` starts a comment. Numbers may be
decimal, hex or an expression over `const` names (`IOSSM_DATA+4`,
`0xa0|($reg<<8)`). Blocks end with `end`.

```
map p0
  window <type> <control_write> <control_read> <data_write> <completion_read> <status> <word>
  part <name>                     # one line per part file, in link order
end
const NAME value
station <name> <selector> i2c|word|sdm <first_byte> <last_byte>
include <rows file>               # take its consts and station names
```

A part file starts with `part <name>`, includes the map file for its station
names and consts, and holds sequences, rules and groups. Rows name their
station; the map file alone binds a name to the selector the FPGA build uses,
so a build that renumbers its stations is a map file change only.

Rows, one FPGA command each:

```
read32  <station> <address>        [poll set|any <mask>] [capture item|group] [remember] [expect & <mask> <op> <value> [else end_group <state> <health>]] [if <compare>] [period <us>] [limit <n>] [unless_clean]
write32 <station> <address> <value> [if <compare>]
ack_clear <station> <address> <mask> [if <compare>] [unless_clean|leaves_clean]   # write the last polled or remembered word back with mask cleared
pop     <station> <status_address> <mask> <data_address> limit <n> [unless_clean|leaves_clean]   # drain a FIFO
write   <station> <device> <byte> [start] [stop] [if <compare>]       # one I2C byte
read    <station> <device> [start] [stop] [nack] [capture ...] [expect ...] [if ...]
reads   <station> <device> <count> [capture item|group]   # a register's bytes: start first, stop and nack last
write_captured <station> word <address> <offset> | byte <device> <offset> [start] [stop]
check   <station> <compare> else end_group <state> <health>   # compares a buffer, sends nothing
xfer    <station> <device> [w <byte>...] [wc <offset>] [r <count>] [capture ...] [expect ...] [if ...]
```

`xfer` is one bounded I2C transaction: up to three write bytes (`wc` takes
one from the group buffer at the offset), then up to four read bytes with the
NACK and STOP. A read over four bytes after one pointer byte expands into
four-byte steps. Use it on controllers that advertise bounded transactions;
`write`, `read` and `reads` send one byte per command.
A compare is `item|group <offset> [w <width>] & <mask> == | >= | <= <value>`, or on a row `word & <mask> <op> <value>` against the word the last read returned;
on `expect` without a buffer it tests the value just read. A row marked
`leaves_clean` that completes in an item that did not fail leaves its station
clean until any row touches that station again or the session changes; rows
marked `unless_clean` are skipped while the station is clean, so a guard that
clears a stale response runs only when something may have left one. `<state>` is
enabled, starting, standby, absent, unavailable or fault; `<health>` is ok,
warning or critical.

Sequences hold lines that repeat; `$name` is replaced by the argument, and an
`if` on the `use` line gates every row of the sequence:

```
seq hub_ready st
  read32 $st IOSSM_RESPONSE_STATUS poll set IOSSM_RESPONSE_READY
end
  use hub_ready 28
  use bank_read 1 $reg $count if group 8 & 0x3 >= 1
```

Rules and groups:

```
rule <Name>                       # the suffix an entity-manager Name ends with
  kind state | numeric <sensor_type> | identity | array tx_power|rx_power|tx_fault|rx_los
  group <Group>
  state <mask> <value> <state> <health>     # state rules: first match wins
  state default <state> <health>
  word group <offset>             # a row-less state rule reads the group buffer
  decode <format> item|group <offset> [shift n] [width n] [signed] [scale x] [bias x] [range lo hi] [count n stride bits]
  field serial|model|part|manufacturer|revision hex_le|hex_be|ascii item|group <offset> <length>
  valid_if <compare>              # publish only when it holds
  run_if <compare>                # skip the item this round when it fails
  <rows>
  cleanup
  <rows run on every exit once a write went out>
end

group <Group>
  setup
  <rows run once per round before the items>
  cleanup
  <rows run after the last item>
end
```

Formats: fixed (a bit field of a little-endian word: shift, width, signed),
u16be, s16be.

## To add a telemetry item

1. Write its rows in the part file of its group, or start a new part file
   and list it in `fpga-rules-lp30.rows`: the words or bytes to send, the
   reads to repeat until a condition holds, the reads to capture, the cleanup
   that leaves the station clean. Reuse a `seq` when one fits.
2. Give the rule its kind and decode: a state map, a numeric spec, identity
   fields or an array spec. Items that share a bus setup and a refresh rate
   go in one group; its setup rows fill the group buffer that row-less items,
   `run_if` and `check` read. A device read at two rates gets two groups with
   the same setup, one per rate, so every round reads its whole group.
3. Regenerate the C files, list a new part's C file in `libnsm/meson.build`
   and its rows file in the test list, and add an entity-manager entry whose
   Name ends with the rule name.

Every row is checked against the station policy before it is sent, so a wrong
row is a logged rule bug, never a faulted station.

## When the engine also changes

Extend the engine, as a separate change, only when an item needs a new row
op, a new numeric format or a new D-Bus property. Once it exists, every later
item that uses it is a rows change again.

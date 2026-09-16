# DO command diagnostics

## Wire contract audit (2026-09-16)

The backend command path was compared with the supplied
`.refs/unitlab-firmware` snapshot. No channel-address or frame-layout mismatch
was found for valid DO commands. This does not establish which firmware is
installed on a device or confirm physical relay operation.

Ownership:

- `frontend/src/pages/devices/components/DeviceChannelsList.vue` sends the
  channel's zero-based `index`. The default label is `CH${index + 1}`.
- `backend/app/ws/actions/manual_command_admission.py` validates the device,
  workspace, channel ID and index before admitting a command.
- `backend/app/services/command_queue_service.py` encodes the logical index
  unchanged and publishes to `<unit_id>/c` through the outbound queue.
- Firmware `src/io/do/DoManager.cpp` maps logical channels to physical outputs
  using `src/io/do/DoConfig.h`. The application must not repeat this mapping.

Protocol v1 uses a 15-byte header: mode:u8, version:u8, packet ID:u16,
timestamp:u64, flags:u8, payload length:u16. Multibyte values are big endian.

| Command | Mode | Payload |
| --- | --- | --- |
| Single | `0x20` | logical channel:u8, value:u8 |
| All | `0x21` | logical channel mask:u32 |
| Pair | `0x22` | channel A:u8, channel B:u8, state:u8 (bit 0=A, bit 1=B) |
| Pulse | `0x23` | logical channel:u8, value:u8, duration ms:u16 |

## CH5 reference

For the default NCV7240 configuration in the supplied firmware:

| Layer | CH5 target |
| --- | --- |
| UI default label | CH5 |
| WebSocket and wire channel | 4 |
| Single-command ON / OFF payload | `04 01` / `04 00` |
| Logical mask bit | `0x00000010` |
| Physical backend index | 8 |
| Firmware mapping description | terminal 5, relay 9 |
| Driver | second NCV7240, local output index 0 (datasheet OUT1) |
| Chip select | `DO_HSPI_CS1`, GPIO18 |

Do not confuse CH5 with physical relay 5: the latter maps to logical index 2
(default UI label CH3). Custom channel names can differ from default labels.
The legacy FastGPIO build has a different mapping.

The NCV7240 datasheet supplied at
`.refs/unitlab-firmware/docs/hardware/NCV7240-D.pdf`, table 4 (page 26), agrees
with the driver's 16-bit MSB-first encoding: ON=`10`, OFF=`11`, OUT1 in bits
1:0. With the other outputs OFF, CH5 ON requires `FF FE` on the second chip;
CH5 OFF requires `FF FF`. These values are a passive logic-analyzer reference,
not instructions to bypass command admission or directly drive hardware.

## Validation and limits

- `backend/tests/services/test_do_command_wire_contract.py` exercises the actual
  queue encoder against independent golden vectors: ON/OFF for all 32 logical
  channels, bulk mask bit 31, all pair states, and pulse byte order.
- Focused backend run: 82 tests passed, including command identity and manual
  command admission tests.
- The supplied firmware native DO-manager suite passed with its fake backend.
- A local temporary cross-language harness fed 64 actual backend frames into
  the firmware C++ packet parser, decoder and DO manager. All 32 channels
  ON/OFF reached the expected physical mock output and returned a matching ACK.
- The real SPI driver and electrical outputs were not exercised by those mock
  tests. The audit itself did not change pin mapping, firmware or wire format.
  Backend retry behavior was subsequently updated as described below.

Repeat the backend checks from `backend/`:

```bash
.venv/bin/python -m pytest -q tests/services/test_do_command_wire_contract.py tests/services/test_command_queue_identity.py tests/ws/test_manual_command_admission.py
```

## Isolating a physical CH5 failure

### Shared retry policy after missing ACK

An unconfirmed hardware command blocks the channel for 60 seconds from its
creation. After that interval the operator can request a new action without
manually editing the database or performing a separate recovery action.
The same policy applies to manual ON/OFF, pair/all commands, pulses, AO,
sequences and signal test runs, including their optimized allocation lookup.
It applies across workspaces and to historical records already in the database.

The exception is based on `execution_status` being `unknown` or `timeout`,
not the operation type or owner. It covers missing-ACK records in `created`,
`queued`, `unknown`, `recovery_required` and `publish_failed` states, including
records left by an interrupted backend or runner. A newer unresolved command
starts its own interval. Device availability and channel leases remain checked;
expiration of an old record does not release an active operation's lease.

Historical records are preserved. Expiration does not mean success and does
not automatically retry any operation or resume a failed test. A new operation
must pass the normal admission flow. Confirmed responses with unresolved
recovery failures (for example, ACK received but readback failed) are not
classified as missing ACK and retain their recovery requirements.

The outbound worker checks every tracked command before MQTT publish. Expired,
timed-out or otherwise terminal tracked commands are discarded from the Redis
outbound queue, including test commands and pulses. A database failure leaves
the entry pending and prevents publication. The check cannot recall a packet
already sent to the broker/device. Untracked state queries keep their existing
transport behavior.

Deploy the backend API, runners and MQTT outbound worker from the same release.
No database migration or firmware change is required. The interval uses the
persisted creation timestamp and backend UTC time; changing the host clock can
change the elapsed interval.

Focused tests: `tests/services/test_hardware_command_retry.py` covers the
60-second boundary across operation types and owners, history preservation,
the allocation repository's correlated SQL query, queued command expiry,
Redis acknowledgement and database failure. Existing admission tests cover
channel ownership. On a simulator, verify that a missing-ACK attempt is blocked
before 60 seconds and that a newly requested manual operation or test can proceed
afterward. Verify that restarting an outbound worker does not replay expired
queued operations.

### Evidence to collect

During an operator-authorized test on a safe setup, correlate one UI command:

1. Backend queued-command log: correct unit, `SET_SINGLE_BIT`, payload ending
   `0401` for ON or `0400` for OFF. Queued does not prove MQTT delivery.
2. Device firmware log: `Channel 5 -> physical output 9 ON` (or `OFF`). This
   establishes receipt and the mapping used by the running firmware.
3. Driver diagnostic log: physical `ch=8`. Debug transitions may be absent in
   release logging. Capture any short/thermal or pending-timeout diagnostics.
4. Note whether UI remains OFF, reports pending/fault, or becomes ON while the
   physical output remains unchanged. Record the flashed firmware/build target.

If CH5 stays OFF and an error toast appears, capture its exact text first.
`Hardware command rejected: ...` is emitted for backend admission/publish
failures; by itself it is not evidence of an electrical fault:

| Reason | Meaning |
| --- | --- |
| `hardware_recovery_required` | Earlier unresolved command blocks this channel; for a missing ACK, retry after the 60-second interval described above |
| `channel_lease_busy` | Another operation owns the channel; the new command is not sent |
| `channel_scope_invalid` | Channel ID/index/device/type validation failed |
| `device_offline` | Backend considers the target unavailable |
| `publish_failed` | Manual command processing raised an exception; inspect the command record to determine how far it progressed |

Also inspect `hardware_command_result.reason` in the WebSocket event:
`device_negative_ack`, `hardware_ack_timeout`, and `hardware_readback_timeout`
identify device rejection, missing acknowledgement and missing confirmed state,
respectively. They can accompany `delivery: queued` in the current implementation.
Do not delete command evidence or clear recovery locks to test a theory.

If the command reaches the driver but the load does not switch, inspect CS1,
SPI data, driver supply/enable and output/load wiring against the actual board
schematic. A driver-enabled state and a successful command ACK do not prove
relay contact movement. The supplied driver explicitly reports driver state,
not measured contact feedback.

Do not change the channel mapping merely to make a different relay switch.
The supplied CH5 database record (`unknown` / `timeout`) explains the permanent
block in the previous implementation. This release removes that permanent
missing-ACK block; the original cause of the missing ACK remains unconfirmed.

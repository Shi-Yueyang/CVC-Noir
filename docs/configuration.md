## Configuration Guide (`simu_config.json`)

The `simu_config.json` file is the central configuration file for the Win300C simulator. It controls how the simulation environment operates, what external applications it launches, logging behavior, simulated hardware state, communication sessions, and data storage.

### 1. Global Settings
- `cycle_ms`: The main execution cycle duration of the simulator in milliseconds.
  ```json
  "cycle_ms": 200
  ```

### 2. Launcher (`launcher`)
An array of external applications to start automatically when the simulator runs.
- `enabled`: (Optional) If `false`, this application will not be launched.
- `target`: Path to the executable. An absolute path is recommended. A bare name without a directory component (e.g. `"notepad"`) is resolved via the Windows executable search order (system directory, Windows directory, and `PATH`); an `.exe` extension is assumed when none is given.
- `args`: (Optional) Command-line arguments passed to the executable. The resulting command line is `"<target>" <args>`. Defaults to empty (no arguments).
- `if_running`: Behavior if the app is already running. Options: `"restart"` (kill and start new), `"skip"` (leave it as is), `"launch_new"` (launch a new instance) .
- `auto_close`: If `true`, the simulator will kill this process when the simulator itself closes.
- `delay_ms`: (Optional) Delay in milliseconds to wait before launching this entry. Useful for staggering startup order. Must be a non-negative integer; defaults to `0` (launch immediately).
- `working_dir`: (Optional) Working directory for the launched process, passed as-is to the OS. If omitted, defaults to the directory containing `target`. If set to an empty string (or when omitted and `target` has no directory component), the launched process inherits the simulator's current working directory.
- `console`: (Optional) Console disposition for the launched process. Options: `"new"` (default) — give the process its own new console window (`CREATE_NEW_CONSOLE`); `"inherit"` — share the simulator's console, no new window; `"none"` — no console window (`CREATE_NO_WINDOW`), output goes to an invisible buffer. An invalid value skips the entry with a warning. This setting only affects console-subsystem apps; GUI apps are unaffected.
```json
"launcher": [
    {
        "enabled": true,
        "target": "C:\\dev\\tacn_env_20260120\\TSRS_SIM\\TSRS_SIM.exe",
        "args": "--config sim",
        "working_dir": "C:\\dev\\tacn_env_20260120\\TSRS_SIM",
        "delay_ms": 500,
        "if_running": "restart",
        "auto_close": true
    }
]
```

### 3. Logging (`log`)
Configures the logging system outputs (console, file) and log levels per module.
- **`default_console` & `default_file`**:
  - `enabled`: `true`/`false`.
  - `level`: Minimum severity to log (`trace`, `debug`, `info`, `warn`, `error`, `critical`).
  - `flush_level`: Severity level that triggers immediate file flushing.
  - `pattern`: The formatting string for log lines.
- **`modules`**: Override logging configurations for specific software modules (e.g., `simulation`, `037_tsrs_1`).
- **Hot reload**: The `log` section is reloaded at runtime when `simu_config.json` is modified (detected via file mtime polling once per cycle, after `SyncOutput`). On a valid edit the new logging config is applied live (console/file levels, patterns, per-module overrides, sink enable/disable) without restarting the simulator and without creating new log files for the current run. On a malformed edit a warning is logged with the parse error and the previous config is retained unchanged. If a valid edit cannot be applied (e.g. an unwritable log `directory`), a warning is logged with the failure reason and the previous config and loggers are rolled back intact, so logging continues on the old config; a subsequent valid edit reloads normally.
```json
"log": {
    "default_console": {
        "enabled": false,
        "level": "info",
        "use_color": true,
        "pattern": "[%H:%M:%S.%e] [%^%l%$] [%n] %v"
    }
}
```

### 4. Board Status (`board_status`)
Simulates the status of hardware boards within the system. You can define the primary and friend boards and their health status.
- `type`: Type of the board (e.g., `MPB`, `GWB`, `VVB`, `VIB`, `VOB`, `BTM`, `AIOB`).
- `boards`: Array of board objects defining their `id`, `friend_id` (the redundant counterpart), and `status` (`good`, `bad`).
- `id` encoding: Identifies a board's physical location.
  - For all board types except `BTM`: `id = ((rack - 1) << 4) + slot`, where `rack` is the 1-based rack number and `slot` is the slot position within that rack (upper nibble = rack, lower nibble = slot).
  - For `BTM`: fixed values are used instead of the formula — `BTM-A` is `15` and `BTM-B` is `16`.
  - `friend_id` follows the same encoding and points at the redundant counterpart board.
```json
"board_status": [
    {
        "type": "MPB",
        "boards": [
            { "id": 3, "friend_id": 12, "status": "good" },
            { "id": 12, "friend_id": 3, "status": "good" }
        ]
    }
]
```

### 5. Communication Sessions (`sessions`)
Defines the network and serial communication channels used by the simulator. The file separates sessions by their functional category.

**Common Session Properties:**
Most sessions share these root-level keys:
- `name`: Identifier for the session.
- `id`: Numeric or hex string identifier.
- `is_skip`: (Optional) If `true`, the simulator will intercept or skip this session's communication.
- `connection`: Object defining the transport layer (see below).

**Connection Options (`connection` object):**
The `connection` block is completely independent of the session type. Any connection type can be used in any session. There are five supported connection types: `serial`, `udp`, `tcp_server`, `tcp_client`, and `dummy`.

#### 1. Serial Connection
Used for serial port (COM) communication.
- `type`: Must be `"serial"`.
- `port`: The COM port name (e.g., `"COM33"`).
- `baud_rate`: Speed of the connection (e.g., `115200`).
- `data_bits`: Data bits (usually `8`).
- `stop_bits`: Stop bits (usually `1`).
- `parity`: Parity check (`0` for None).
- `rts_cts`: (Optional) Hardware flow control (`0` for off).
- `read_total_timeout_ms`: (Optional) Read timeout in milliseconds.

*Example:*
```json
"connection": {
    "type": "serial",
    "port": "COM33",
    "baud_rate": 115200,
    "data_bits": 8,
    "stop_bits": 1,
    "parity": 0,
    "rts_cts": 0,
    "read_total_timeout_ms": 10
}
```

#### 2. UDP Connection
Used for connectionless network communication.
- `type`: Must be `"udp"`.
- `local_ip`: IP address to bind locally (e.g., `"127.0.0.1"`).
- `local_port`: Local port to listen on.
- `default_peer_ip`: (Optional) Default destination IP address for outgoing packets.
- `default_peer_port`: (Optional) Default destination port.

*Example:*
```json
"connection": {
    "type": "udp",
    "local_ip": "127.0.0.1",
    "local_port": 2334,
    "default_peer_ip": "127.0.0.1",
    "default_peer_port": 2333
}
```

#### 3. TCP Server Connection
Used to host a TCP server that external clients can connect to.
- `type`: Must be `"tcp_server"`.
- `local_ip`: IP address to bind the server to.
- `local_port`: Port to listen for incoming connections.
- `receive_framing`: (Optional, default `"none"`) Decodes received byte stream into message boundaries. Supported values:
    - `"none"`: Raw byte stream, no framing.
  - `"dmi"`: DMI framing (magic `0x55 0xAA`, single-byte length at offset 3).
  - `"2-byte-len-big"`: 2-byte length prefix framing, big-endian length field.
  - `"2-byte-len-little"`: 2-byte length prefix framing, little-endian length field.
  - `"ndjson"`: Newline-delimited JSON. Receive: one line per `receive()`, delivered without the trailing `\n` (a trailing `\r` is also stripped); empty lines are skipped; lines that do not fit the receive buffer are dropped and the stream resyncs on the next line. Send: a `\n` is appended unless the payload already ends with one.
- `send_framing`: (Optional, default `"none"`) Auto-prepends framing header to outgoing data. Same values as `receive_framing`.

*Example:*
```json
"connection": {
    "type": "tcp_client",
    "local_ip": "0.0.0.0",
    "local_port": 6000,
    "receive_framing": "2-byte-len-big",
    "send_framing": "2-byte-len-big",
    "local_ip": "127.0.0.1",
    "local_port": 8080
}
```

#### 4. TCP Client Connection
Used to connect as a client to an external TCP server.
- `type`: Must be `"tcp_client"`.
- `remote_ip`: IP address of the server to connect to.
- `remote_port`: Port of the server to connect to.
- `local_ip`: (Optional) Local IP address to bind before connecting.
- `local_port`: (Optional) Local port to bind before connecting. `0` lets the OS choose.
- `receive_framing`: (Optional, default `"none"`) Decodes received byte stream into message boundaries. Supported values:
    - `"none"`: Raw byte stream, no framing.
  - `"dmi"`: DMI framing (magic `0x55 0xAA`, single-byte length at offset 3).
  - `"2-byte-len-big"`: 2-byte length prefix framing, big-endian length field.
  - `"2-byte-len-little"`: 2-byte length prefix framing, little-endian length field.
  - `"ndjson"`: Newline-delimited JSON. Receive: one line per `receive()`, delivered without the trailing `\n` (a trailing `\r` is also stripped); empty lines are skipped; lines that do not fit the receive buffer are dropped and the stream resyncs on the next line. Send: a `\n` is appended unless the payload already ends with one.
- `send_framing`: (Optional, default `"none"`) Auto-prepends framing header to outgoing data. Same values as `receive_framing`.

*Example:*
```json
"connection": {
    "type": "tcp_client",
    "remote_ip": "192.168.1.100",
    "remote_port": 5000,
    "local_ip": "0.0.0.0",
    "local_port": 6000,
    "receive_framing": "2-byte-len-big",
    "send_framing": "2-byte-len-big"
}
```

#### 5. Dummy Connection
Used for simulator-only scenarios where a session should receive one fixed payload without creating any real transport.
- `type`: Must be `"dummy"`.
- `received_data`: Array of byte values to return from the first `receive()` call after open. Integer and numeric-string items in the range `0`-`255` are accepted.

Behavior:
- The first successful `receive()` after `open()` returns the configured payload.
- Later `receive()` calls return `would_block` until the connection is closed and reopened.
- `send()` accepts data and discards it.

*Example:*
```json
"connection": {
  "type": "dummy",
  "received_data": [1, 2, 3, 4]
}
```

#### `ext_sessions`
Used for external hardware like GNSS receivers (typically Serial).
- **Special Keys:**
  - `prefix_zero_bytes`: Number of zero bytes to prepend to outgoing messages.
  - `idle_payload`: Array of bytes (integers) to continuously send when the connection is idle (e.g., `[85, 170, 85, 170, 0, 0]`).

#### `raw_sessions`, `maint_sessions`, `snmp_sessions`
Standard UDP or Serial sessions for raw packet streams (IPQuery, LKJ), maintenance (JRU), or SNMP links (TAU). They use the standard `name`, `id`, and `connection` blocks.

#### `rsspi_sessions`
Used for specific application links like DMI and ATO. They use the standard `name`, `id`, and `connection` blocks.

#### `safety037_sessions`
Used for safety-critical connections like RBC and TSRS. Supports advanced timing logic and dynamic peer resolution.
- **Special Keys:**
  - `recv_no_data_send_lost_ms`: Timeout (in ms) to declare a connection lost if no data is received. `-1` disables it.
  - `connect_success_delay_ms`: Artificial delay before confirming a successful connection.
  - `disconnect_failure_delay_ms`: Artificial delay before tearing down a failed connection.
  - `dynamic_peer`: Object to dynamically resolve peer addresses. Contains `enabled` (boolean) and `peer_mappings` (array of objects mapping incoming `from` IP/port to local `to` IP/port).

#### `other_asw_sessions`
Used to control internal software messaging behavior (e.g., between ATP and ATO). They typically use the standard `name`, `id`, and `is_skip` properties without requiring a `connection` block.

#### `a_train_session`
Single-instance (not an array) category for A-train related links, like `pxi_session`. The section is optional; entries use the `name` property and an optional `connection` block (any connection type). There is no `id` field. Each cycle the session drains the connection (received content is discarded, logged at trace level under module `a_train`) and drains pending VOB output messages. Each VOB message is sent as one NDJSON `atp_command` line. The temporary hardcoded identity is `train_id: "TRAIN001"`, `cab_id: 1`; `atp_signal` contains one bit per VOB port index, with missing indices set to `0`, and an underscore is inserted after every five bits.
```
{"type":"atp_command","train_id":"TRAIN001","cab_id":1,"atp_signal":"0100..."}\n
```

*Example:*
```json
"a_train_session": {
    "name": "a-train",
    "connection": {
        "type": "tcp_server",
        "local_ip": "127.0.0.1",
        "local_port": 19022
    }
}
```

#### `pxi_session`
Specialized session mapping hardware I/O lines to network packets for PXI test benches. This session also supports the standard session fields (`name`, `id`) and maintains a `connection` internally that the simulator opens.
- **Special Keys:**
  - `local_ip`, `local_port`, `peer_ip`, `peer_port`: Direct UDP connection details at the root level.
  - `io_mapping`: Describes how individual bits are translated between internal signals and PXI frames, using three directional sub-objects:
    - `to_pxi`: **Object or array** of mapping rules. Each rule maps internal bits into the outgoing frame sent to PXI hardware. The rule to use is selected by matching the rule's `length` field against `vob_data.Length`. `total_length` is the padded byte length of the UDP packet to send and may differ from `length`. If `length` is omitted in a rule, it defaults to that rule's `total_length`. If no rule matches, a warning is logged and the VOB message is skipped. The legacy single-object form is still accepted and treated as a one-rule array.
    - `from_pxi`: **Array** of mapping rules. Each rule supports `total_length`, `high_value`, and `entries`. When a packet is received, the simulator selects the first rule whose `total_length` exactly matches the incoming packet byte length and applies its entries. If no rule matches, a warning is logged and the `from_pxi` mapping is skipped for that cycle; any configured `from_internal` mapping is still published.
    - `from_internal`: Maps internal bits that should be exposed back internally (loopback or derived signals). Note: only `entries` is supported here; `total_length` and `high_value` are ignored for `from_internal`.

    Each `to_pxi` rule and each element of `from_pxi` supports:
    - `total_length`: Integer. For `to_pxi`, the padded byte length of the outgoing UDP packet. For `from_pxi`, the total bit-length of the destination vector produced by this mapping (i.e., the size of the target bitfield). Ensure it is large enough to hold the highest `to` index referenced in `entries`.
    - `length`: (Optional, `to_pxi` only) Integer. The value of `vob_data.Length` that selects this rule. If omitted, it defaults to the rule's `total_length`.
    - `high_value`: Byte. Integer (0–255) or hex string (e.g., `"0x11"`) parsed as a single byte used as the default/reset value for the destination vector before applying `entries`. If omitted, destination bits default low.
    - `entries`: Array of mapping rules. Each rule supports the following keys:
      - `from`: Integer. Source bit index to read.
      - `to`: Integer. Destination bit index to write.
      - `or`: (Optional) Integer. When present, the destination bit is the logical OR of `from` and `or` sources.

    Notes and recommendations:
    - Bit indexing is zero-based for both `from` and `to`.
    - `to` must be within `[0, total_length - 1]`. Define `total_length` large enough to cover the maximum `to` index.
    - If multiple `to_pxi` rules have the same `length`, the first matching rule is used.
    - If multiple `from_pxi` rules have the same `total_length`, the first matching rule is used.
    - If multiple rules target the same `to` index, their effects are combined by the underlying implementation. Prefer unique `to` indices for clarity, or explicitly use the `or` key to describe intended merges.
    - Unmapped destination bits retain the default state derived from `high_value` (or 0 if `high_value` is not set).

    Example: Sending selected internal bits to PXI (to_pxi)
    ```json
    "io_mapping": {
      "to_pxi": [
        {
          "length": 17,
          "total_length": 35,
          "high_value": "0x11",
          "entries": [
            { "from": 0,  "or": 17, "to": 2 },
            { "from": 1,  "or": 18, "to": 3 },
            { "from": 2,  "or": 19, "to": 4 },
            { "from": 3,  "or": 20, "to": 5 },
            { "from": 4,               "to": 6 },
            { "from": 7,               "to": 7 },
            { "from": 6,               "to": 8 }
          ]
        },
        {
          "length": 20,
          "total_length": 40,
          "high_value": "0x22",
          "entries": [
            { "from": 0, "to": 10 },
            { "from": 1, "to": 11 }
          ]
        }
      ]
    }
    ```

    Example: Receiving PXI bits and mapping to internal (from_pxi) and loopback (from_internal)
    ```json
    "io_mapping": {
      "from_pxi": [
        {
          "total_length": 28,
          "high_value": "0x11",
          "entries": [
            { "from": 6,  "to": 2 },
            { "from": 7,  "to": 3 },
            { "from": 8,  "to": 4 },
            { "from": 9,  "to": 5 }
          ]
        },
        {
          "total_length": 32,
          "high_value": "0x22",
          "entries": [
            { "from": 10, "to": 6 },
            { "from": 11, "to": 7 }
          ]
        }
      ],
      "from_internal": {
        "entries": [
          { "from": 0,  "or": 17, "to": 0 },
          { "from": 1,  "or": 18, "to": 1 }
        ]
      }
    }
    ```

- **`port_names`** (Optional): Human-readable labels for I/O port indices, used only for trace-level logging. It does not affect mapping behavior.
  - An object with two optional sub-objects: `input` (labels for VIB input ports) and `output` (labels for VOB output ports).
  - Keys are string-encoded port indices (e.g., `"2"`); values are the display names.
  - Partial naming is supported: only the ports you care about need to be listed. Ports not present in `port_names` fall back to their numeric index in logs.
  - Omitting `port_names`, `input`, or `output` entirely causes all ports in that direction to be shown by index. No warnings are emitted for missing entries.
  - Invalid keys (non-numeric or negative) and non-string values are skipped with a warning log.

    Example:
    ```json
    "port_names": {
      "input": {
        "2": "door_status",
        "3": "speed_feedback"
      },
      "output": {
        "2": "door_command",
        "3": "speed_setpoint"
      }
    }
    ```

    At trace level, the PXI session logs the post-mapping I/O state per cycle:
    ```
    in: HIGH[door_status,4] LOW[0,1,speed_feedback,5]
    out: HIGH[door_command] LOW[2,4,5,6,7,8]
    ```
    `HIGH` = `PortValue == 1`; `LOW` = any other value. Port indices are sorted ascending within each group.

- **`trace_unnamed_ports`** (Optional, boolean, default `true`): Controls whether ports without an entry in `port_names` are shown in trace logs.
  - `true` (default): unnamed ports are shown by their numeric index, preserving full visibility. Matches the behavior when the field is omitted.
  - `false`: only ports that have a name in `port_names` are shown; unnamed ports are omitted from both the `HIGH[...]` and `LOW[...]` groups. A group containing only unnamed ports renders as `HIGH[]` / `LOW[]`, which is indistinguishable from an empty group.
  - This field does not affect mapping behavior and only applies to trace-level logging.

    Example:
    ```json
    "trace_unnamed_ports": false
    ```

#### `pxi_motion_session`
Direct UDP mapping for PXI motion control. This session is also opened via an internal `connection` object constructed from the provided addressing fields.
- **Special Keys:**
  - `local_ip`, `local_port`, `peer_ip`, `peer_port`: Direct UDP addressing.

### 6. Data Storage & PDA (`data`)
Configures application data arrays and the paths for the Personal Digital Assistant (PDA) file storage.
- `application_data`: Core application binary and config files.
- `nrnw`: NVRAM storage (1st type PDA data: single read/write, double read/write).
- `araw`: Dataplug storage (2nd type PDA data: single read/write, double read/write).
- `arnw`: NVRAM storage (4th type PDA data: single read-only, double read/write).
- `flash`: Flash storage (3rd type PDA data). An array supporting up to 7 entries (`id` 0-6).
- **Simulated operation delays (`read_ms` / `write_ms`)**: Each PDA entry (`nrnw`/`araw`/`arnw`, and every `flash` array entry) accepts optional `read_ms` and `write_ms` fields — the simulated duration in milliseconds for read/write operations on that storage. The new key aliases `pda1`/`pda2`/`pda3`/`pda4` are accepted in place of `nrnw`/`araw`/`arnw`/`flash` and carry the same fields.
  - Default is `0` (operation completes immediately, matching legacy behavior). Missing section, missing entry, or missing field all mean `0`.
  - Invalid values (non-integer, negative) are ignored with a warning and default to `0`.
  - With a delay configured, the read/write API performs its file operation immediately but returns `PENDING`; `API_GetXXXStatus()` (and for types without a status poll, a repeat call of the read API) keeps returning `PENDING` until the configured milliseconds have elapsed (monotonic wall clock, independent of `cycle_ms`), then reports `SUCCEED`.
  - `application_data` is startup-only data and has no delays.
  - 4th type writes go to the dataplug file but use `arnw`/`pda4`'s `write_ms`.
```json
"data": {
    "nrnw": {
        "file": "Plug/nvram.bin",
        "read_ms": 400,
        "write_ms": 200
    },
    "araw": {
        "file": "Plug/dataplug.bin"
    },
    "arnw": {
        "file": "Plug/arnw.bin",
        "read_ms": 400
    },
    "flash": [
        { "id": 0, "file": "Plug/db.dat", "read_ms": 1000, "write_ms": 1500 },
        { "id": 1, "file": "Plug/db2.dat", "read_ms": 1000, "write_ms": 1500 }
    ]
}
```
# Radio protocols

This firmware speaks **TCI** (Transceiver Control Interface) over
WebSocket. That's the only supported protocol; the earlier Kenwood
`IF;` over TCP path from the ESP8266 sketches has been removed.

## TCI WebSocket

TCI is a text‑over‑WebSocket protocol developed by Expert
Electronics (the SunSDR / MB1 / ExpertSDR3 vendor). Each line is a
semicolon‑terminated message of the form `command:arg1,arg2,...;`.
The firmware connects to a single TCI server endpoint per radio and
subscribes to a small subset of the event stream:

| TCI event | Library hook | What this firmware does |
| --- | --- | --- |
| `vfo:RIG,VFO,FREQ;` | `attach_vfo_event` | If `VFO == 0` (main dial), decode the band and drive the BCD bank. |
| `modulation:RIG,MODE;` | `attach_modulation_event` | Push the mode string (USB / LSB / CW / DIGU…) to the LCD. |
| `trx:RIG,STATE;` | `attach_trx_event` | Push the TX/RX state to the LCD's state column. |
| `tune:RIG,STATE;` | `attach_tune_event` | Latch the per‑BPF tune flag; force the bank to bypass while engaged. |

Everything else (rx_smeter, drive, agc_gain, audio_samplerate, …) is
parsed by the TCI library but ignored. The library's "Unhandled
message!" Serial.printf is gated behind `TCI_LOG_UNHANDLED`
(default 0) so the console stays quiet; a separate
`[TCI raw tune-like]` diagnostic tap prints any incoming frame
whose payload contains the substring `tune`, useful when validating
a new server's tune behaviour.

### Connection lifecycle

1. `g_radio1.set_host(...)` / `set_port(...)` / `set_iaru_region(...)`
   pick up the config.
2. `g_radio1.attach_*` registers the four event callbacks above.
3. `g_radio1.connect()` opens the WebSocket and spawns two FreeRTOS
   tasks inside the library: `ws_reader_task` (reads frames into a
   ring buffer) and `ws_event_task` (drains the buffer and calls
   `parse_message`, which fires the registered callbacks).
4. On WebSocket drop the library reconnects on its own. The
   firmware's 250 ms failsafe in `loop()` independently forces the
   bank to bypass during the outage.

### Default ports / regions

| Field | Default | Notes |
| --- | --- | --- |
| Host | `192.168.1.20` (R1), `192.168.1.21` (R2) | Pre‑filled at first boot, change via portal. |
| Port | `50001` | ExpertSDR3 / SunSDR TCI server default. |
| IARU region | `1` (Europe / Africa) | Affects band‑edge limits inside the library; not used by `BcdBandPlan`. |

## Server modes

Picked automatically from the saved config:

### DUAL

Radio 1 and Radio 2 have different `host:port` pairs. The firmware
opens two TCI clients, each filters on `senderRig=0 / senderVfo=0`
so only main‑RX VFO‑A events drive the BPF. Use this when you have
two separate radios feeding two filters.

### SHARED

Radio 1 and Radio 2 resolve to the same `host:port`. The firmware
opens **one** TCI client and demuxes by `senderRig`: events with
`senderRig=0` drive BPF 1, `senderRig=1` drive BPF 2. Use this when
a single dual‑receiver radio (SunSDR2 PRO, MB1) feeds both filters.

Detection is in `isSharedTciConfig()` — case‑insensitive host match
+ numeric port match.

## Verified servers

| Server / radio | Mode tested | VFO | Mode | TRX | Tune | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| **ExpertSDR3 + SunSDR2 PRO** | SHARED | ✓ | ✓ | ✓ | ✓ | Full event stream. RX‑1 = `rig=0`, RX‑2 = `rig=1`. |
| **ExpertSDR3 + Colibri DDC** | DUAL  | ✓ | ✓ | — | — | RX‑only receiver — no TX/TUNE events ever. |
| **AetherSDR + TCI**          | DUAL  | ✓ | ✓ | ✓ | **✗** | TCI server doesn't forward tune state. Use the manual bypass path (web `/bypass`, serial `bypassN on/off`) before pressing TUNE on the radio. |

## Verifying a new server

To check whether a new TCI server emits the events this firmware
needs, watch the serial console at 115200 baud while exercising the
radio:

| Action on radio | Expected serial output |
| --- | --- |
| Tune the main VFO to 14.250 MHz | `[R1] 20m @ 14250000 Hz (bcd=5 inh=0 tune=0)` |
| Change mode USB → CW | (LCD updates; no per‑event log by default) |
| Key the mic / footswitch | (LCD state column flips to `TX`) |
| Press TUNE | `[R1] tune evt rig=0 state=1` then `[BPF1] TUNE change -> ON (forcing bypass)` then `[R1] bypass @ … (bcd=<warc> inh=1 tune=1)` |

If a TUNE press produces nothing on serial, build a quick
verification flash with `-DTCI_LOG_UNHANDLED=1` to dump every
unparsed TCI frame, OR rely on the always‑on
`[TCI raw tune-like]` tap to catch any frame whose payload contains
the substring `tune`. Silence on both means the server simply
doesn't surface tune events — drop back to the manual bypass path.

## Adding another protocol

The architecture isn't deliberately hostile to other protocols, but
the API surface was simplified down to TCI when the ESP8266 sketches
were removed. To re‑add (e.g.) Kenwood `IF;` over TCP for a non‑TCI
radio:

1. Add a `radio1_protocol` byte to the `Config` struct
   ([Config.h](../ESP32_SO2R_TCI/Config.h)) and bump the schema
   version.
2. Introduce a `RadioInterface` abstraction with `TciRadio` and
   `KenwoodTcpRadio` implementations; have each invoke the same
   `applyBand(idx, hz)`.
3. Surface a protocol picker in the web portal
   ([WebPortal.h](../ESP32_SO2R_TCI/WebPortal.h)).

None of `BcdBandPlan` / `LcdDisplay` / the bypass policy needs to
change — they're already protocol‑agnostic.

# Radio protocols

This firmware needs one thing from a radio: a way to read the **current
transmit frequency in Hz** over the network. Two approaches are supported
(one fully, one as a planned protocol slot).

## 1. Kenwood `IF;` over TCP — protocol 0 (default, implemented)

The classic Kenwood CAT command `IF;` returns a 37‑byte response of the form

```
IFfffffffffff*****+yyyyrxitmdpbb01 ;
  ^^^^^^^^^^^                       (11-digit frequency in Hz, position 2..12)
```

The firmware:

1. Opens a TCP socket to `radio_host:radio_port`.
2. Sends `IF;` every `poll_interval_ms` (default 300 ms).
3. Reads until the next `;`. If the reply starts with `IF`, extracts
   characters 2..13 and parses as a long.
4. Hands the frequency to `BandPlan::bandFor(frequencyHz)`.

Anything else (`PS`, `AI`, garbage) is dropped.

### Servers known to speak this

- **Thetis** (FlexRadio 6000‑series): built‑in CAT TCP server, default
  port **13013**. Reference sketch was developed against this.
- **Node‑Red flows** that bridge a radio's CAT to a TCP listener: common port
  **7355**.
- **hamlib `rigctld`** with the `--tcp` option: speaks rigctl but a small
  Node‑Red node can wrap it; for direct support add protocol 2 later.

### Failure modes

- **TCP closed:** reconnect with bounded backoff (1 → 2 → 4 → 8 s). Outputs
  driven to bypass during the outage.
- **No `IF` reply for > 5 s:** outputs forced to bypass and the socket is
  dropped & reopened.
- **Parse error:** ignored; next poll retries.

## 2. TCI WebSocket — protocol 1 (reserved, not yet implemented)

TCI (Transceiver Control Interface, by Expert Electronics) is a JSON‑over‑
WebSocket protocol used by SunSDR, MB1, and ExpertSDR2/3. The interesting
event for this firmware is

```
vfo:0,0,14250000;
```

— receiver 0, VFO 0, frequency in Hz.

Two implementation options:

- **Native client on ESP‑12** using `arduinoWebSockets` (Markus Sattler).
  Adds a non‑trivial dependency and ~30 KB flash; manageable but adds
  surface area. Protocol slot `1` is reserved for this.
- **Bridge through Node‑Red** (recommended today). Node‑Red has a `tci-client`
  contrib node; pipe `vfo` updates into the existing CAT TCP flow on port
  7355 and the firmware sees them via protocol 0. Zero extra firmware code.

The config schema has `radio_protocol` already, so adding the native TCI
client later is a `RadioInterface.h` change and a build flag — nothing in the
config layout has to move.

## Why "TCP server per radio" is enough

The user's premise is that each radio either exposes a TCP server itself or
sits behind a host that does. The firmware deliberately knows nothing about
the underlying radio model, USB CAT, CI‑V, BCD, etc. — that translation is
delegated to whatever is running the TCP server. This is what makes the
"swap radios" property cheap: the firmware just points at a different host.

For radios with no native TCP server, the canonical bridges are:

- Thetis on a PC (FlexRadio).
- Node‑Red on a Raspberry Pi (any radio with a USB CAT cable).
- `rigctld -t 4532` (hamlib) — wrap with a small TCP/IF; adapter, or add
  protocol 2 to the firmware.

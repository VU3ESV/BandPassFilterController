# BandPassFilterController

ESP12 (ESP8266) firmware that drives an HF contest band‑pass filter (BPF) by
reading the operating frequency of an attached radio over the network.

Two filter models are targeted, one firmware build per device:

- **5B4AGN TXBPF** — high‑power contest filter discussed at
  <https://groups.io/g/TXBPF>. Six switched filter sections (160/80/40/20/15/10 m)
  with internal relays. Code lives in [5B4AGN/](5B4AGN/).
- **Hamation MBF‑100 BandPasser II** — six‑band contest BPF documented at
  <https://www.hamation.com/Bandpasser.html>. Each band is engaged by applying
  +5–12 V to a rear‑panel control pin; with no input it falls back to bypass.
  Code lives in [Hamation/](Hamation/).

## Objectives

1. **Read radio frequency over the network.** Each ESP12 connects to a TCP
   endpoint that exposes the radio (Kenwood/Thetis `IF;` CAT over TCP today,
   TCI WebSocket bridged via Node‑Red / rigctld; see
   [docs/RADIO_PROTOCOLS.md](docs/RADIO_PROTOCOLS.md)).
2. **Map frequency → contest band.** Only the six contest bands (160/80/40/20/15/10 m)
   engage a filter section. WARC bands (30/17/12 m), 60 m, 6 m, and out‑of‑band
   readings drop the controller into **bypass** (all band outputs de‑asserted).
3. **One controller per filter, one filter per radio, fully swappable.** The
   association between radio and filter is just configuration (IP + port +
   protocol). Swapping radios is a web‑UI change, no recompile.
4. **Configurable in the field.** The ESP12 hosts a web portal for SSID, radio
   endpoint, protocol, and hostname. Config persists in EEPROM. First boot (or
   reset hold) starts a SoftAP captive portal.
5. **Safe defaults.** All outputs LOW on boot, on disconnect, and on parse
   failure — both filters interpret "no input" as bypass, which keeps RF
   flowing without damaging a filter section.

## Layout

```
.
├── CLAUDE.md                     ← this file
├── README.md                     ← public README
├── docs/
│   ├── ARCHITECTURE.md           ← firmware module layout, runtime model
│   ├── HARDWARE.md               ← pinout, relay board, level shifting
│   ├── CONFIGURATION.md          ← web portal, EEPROM layout, factory reset
│   └── RADIO_PROTOCOLS.md        ← Kenwood IF;, TCI, bridging notes
├── 5B4AGN/                       ← Arduino sketch for 5B4AGN TXBPF
│   ├── 5B4AGN.ino
│   ├── Config.h
│   ├── BandPlan.h
│   ├── RadioInterface.h
│   └── WebPortal.h
├── Hamation/                     ← Arduino sketch for Hamation BandPasser II
│   ├── Hamation.ino
│   ├── Config.h
│   ├── BandPlan.h
│   ├── RadioInterface.h
│   └── WebPortal.h
└── .support/
    └── ESP12TCPClientV6.ino      ← original reference sketch (pins, IF; flow)
```

Both sketches share the same module shape; the differences are cosmetic
(default hostname, on‑screen label) plus the comments documenting how each
filter interprets the 6 band‑select outputs. See
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the rationale behind keeping
them as two self‑contained sketches rather than a shared library.

## Build & flash (quick)

- Arduino IDE → install **ESP8266** board package (3.1.x).
- Tools → Board → *Generic ESP8266 Module* or *NodeMCU 1.0*.
- Open `5B4AGN/5B4AGN.ino` (or `Hamation/Hamation.ino`) and upload.
- First boot: ESP12 raises `BPF-Setup-XXXX` SoftAP; join it, browse to
  `http://192.168.4.1/`, set WiFi + radio endpoint, save, reboot.

See [docs/CONFIGURATION.md](docs/CONFIGURATION.md) for details and the
factory‑reset procedure.

## Reference implementation

`.support/ESP12TCPClientV6.ino` is the original sketch this project is derived
from. It establishes the GPIO usage for the LC Technology `ESP12F_Relay_X8`
8‑relay carrier and the Kenwood `IF;` TCP polling pattern. Two things the new
firmware corrects:

1. Its band mapping bundles 30 m with 40 m and 17 m with 20 m — wrong for a
   contest BPF where WARC bands must route to bypass.
2. Its `Relay1..Relay8` macros are numbered **reverse** of the board's
   silkscreen (the reference's `Relay1 = GPIO5` is *Relay 8* on the PCB).
   The new firmware uses the silkscreen labels and deliberately skips
   Relays 1 (GPIO16) and 6 (GPIO0) because both pulse ON briefly at every
   power‑up. See [docs/HARDWARE.md](docs/HARDWARE.md) for the full pin
   table and sources.

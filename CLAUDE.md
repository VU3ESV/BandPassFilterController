# BandPassFilterController

ESP32 firmware that drives **two** HF contest band‑pass filters at the
same time by reading the operating frequency of one or two SDR radios
over the network using the **TCI WebSocket** protocol. One ESP32 dev
board with an 8‑relay carrier emits two independent Yaesu‑style 4‑bit
BCD band‑data buses — one per BPF.

A single firmware variant lives in [ESP32_SO2R_TCI/](ESP32_SO2R_TCI/);
the earlier ESP8266 single‑radio / Kenwood `IF;` sketches have been
removed.

## Target filters

- **5B4AGN TXBPF** — high‑power contest filter, six switched sections
  (160/80/40/20/15/10 m); discussed at <https://groups.io/g/TXBPF>.
- **Hamation MBF‑100 BandPasser II** — six‑band contest BPF documented
  at <https://www.hamation.com/Bandpasser.html>. Each band is engaged
  by applying +5–12 V on a rear‑panel control pin; with no input it
  falls back to its built‑in bypass relay. **Important quirk**:
  Hamation's decoder reads BCD `0000` as **160 m**, not bypass — so
  the firmware never emits `0000` for inhibit. See "Bypass policy"
  below.

Either filter can be on either bank; the firmware doesn't care.

## Radio support

- **TCI WebSocket** — native protocol for SunSDR / Expert Electronics
  (ExpertSDR3 default port `50001`). Two operating modes, picked
  automatically from the saved config:

  | Mode | Trigger | Behaviour |
  | --- | --- | --- |
  | **DUAL**   | Radio 1 and Radio 2 have different `host:port` pairs | Two TCI clients opened; each filter follows its own radio's RX‑1 VFO A. |
  | **SHARED** | Radio 1 and Radio 2 share the same `host:port` | One TCI client; `rig=0` events drive BPF 1, `rig=1` events drive BPF 2. Use this when a dual‑receiver radio (e.g. SunSDR2 PRO) feeds both filters. |

- Tested against SunSDR2 PRO, ExpertSDR3, and AetherSDR. The
  ExpertSDR3 / SunSDR2 PRO TCI server emits the full event stream
  (vfo, modulation, trx, tune). AetherSDR's TCI implementation does
  not forward tune state, so its bank uses the manual bypass path
  (see below).

The TCI library is
[IW7DMH's v1.0.1](https://iw7dmh.jimdofree.com/sunsdr2-pages/tci-esp32s-arduino-libraries/);
the compiled copy lives next to the sketch
([ESP32_SO2R_TCI/TCI.h](ESP32_SO2R_TCI/TCI.h),
[ESP32_SO2R_TCI/TCI.cpp](ESP32_SO2R_TCI/TCI.cpp),
[ESP32_SO2R_TCI/RTX.h](ESP32_SO2R_TCI/RTX.h),
[ESP32_SO2R_TCI/RTX.cpp](ESP32_SO2R_TCI/RTX.cpp))
with two local patches: quoted `RTX.h` include for in‑sketch
compilation, and `Unhandled message!` Serial.printf gated behind
`TCI_LOG_UNHANDLED` (default 0). A separate `[TCI raw tune-like]`
diagnostic tap surfaces any incoming TCI frame containing the
substring "tune" — quiet way to confirm a new server's tune
behaviour.

## BCD band plan — full Yaesu Table 5

| Band | BCD (DCBA) | dec |
| --- | --- | --- |
| 160 m | 0001 | 1 |
| 80 m | 0010 | 2 |
| 40 m | 0011 | 3 |
| 30 m | 0100 | 4 |
| 20 m | 0101 | 5 |
| 17 m | 0110 | 6 |
| 15 m | 0111 | 7 |
| 12 m | 1000 | 8 |
| 10 m | 1001 | 9 |
| 6 m  | 1010 | 10 |

60 m / OOB / disconnect / tune emit a **nearest‑WARC** bypass code
(see below), never `0000`.

## Bypass policy

The firmware forces bypass on a bank in any of these states:

- The radio's **TUNE** button is engaged (TCI `tune:RIG,true;`).
- Manual bypass requested via web (`POST /bypass?bpf=N&on=1`) or
  serial (`bypassN on`) — used when the TCI server doesn't emit tune
  events (AetherSDR).
- The TCI link drops or WiFi disconnects.
- The decoded frequency is on 60 m or out‑of‑band.

In any of those cases, instead of `0000` the bank emits the
**nearest WARC code** to the last known frequency:

| Last freq | Bypass code |
| --- | --- |
| < 14 MHz   | 30 m → 0100 (4) |
| 14–21 MHz  | 17 m → 0110 (6) |
| > 21 MHz   | 12 m → 1000 (8) |
| Unknown    | 30 m → 0100 (4) |

Rationale: the Hamation decodes `0000` as 160 m, exactly the opposite
of bypass and dangerous if the ATU is sweeping at high power on a
different band. A WARC code triggers Hamation's built‑in bypass relay
(no matching section). 5B4AGN behaves the same way on unrecognised
codes — all section relays released = bypass. Single safe policy for
both filters.

## Layout

```
.
├── CLAUDE.md                     ← this file
├── README.md                     ← public README
├── docs/                         ← long-form reference; current
│   ├── ARCHITECTURE.md             with the shipping firmware
│   ├── HARDWARE.md                 (ESP32 + TCI + dual/shared mode
│   ├── CONFIGURATION.md            + WARC bypass policy)
│   └── RADIO_PROTOCOLS.md
├── ESP32_SO2R_TCI/               ← the firmware
│   ├── ESP32_SO2R_TCI.ino        ← main sketch (TCI handlers, wiring,
│   │                               failsafe, manual bypass plumbing)
│   ├── Config.h                  ← EEPROM schema, magic, CRC32
│   ├── BcdBandPlan.h             ← Yaesu Table 5 + nearest-WARC bypass
│   ├── WebPortal.h               ← /, /save, /status, /bypass, /reboot
│   ├── LcdDisplay.h              ← 16x2 I2C LCD, link indicator, delta
│   │                               redraw at 150 ms / 400 kHz I2C
│   ├── TCI.h / TCI.cpp           ← bundled IW7DMH TCI client
│   ├── RTX.h / RTX.cpp             (locally patched — see notes above)
│   └── README.md
```

## Hardware

- One **ESP32 dev board** (generic profile works).
- One **8‑relay carrier**, **active‑LOW** inputs (HIGH = relay
  de‑energised). All 8 relays consumed: 4 per BPF for the BCD bus.
  No inhibit line — bypass is "all 4 lines HIGH" on the affected
  bank (or the WARC override above).
- Pin map (BPF 1 matches the GPIOs used by the original MQTT
  reference build so existing 5B4AGN wiring is preserved):

  | Function | BPF 1 GPIO | BPF 2 GPIO |
  | --- | --- | --- |
  | BCD A | 16 | 33 |
  | BCD B | 17 | 32 |
  | BCD C | 18 | 27 |
  | BCD D | 19 | 26 |

- Optional **16×2 I²C LCD** (PCF8574 backpack at `0x27`, override in
  `setup()` if `0x3F`). SDA = GPIO 21, SCL = GPIO 22. Layout:

  ```
  col:    0123456789012345
  row 0:  ' 14.2500 USB RX'   ← BPF 1
  row 1:  '* 7.1000 LSB TU'   ← BPF 2: link DOWN ('*') OR tune engaged ('TU')
  ```

  Col 0: `' '` = TCI up, `'*'` = TCI/WiFi down → forced bypass.
  Last column shows `TU` while tune (radio or manual) is engaged,
  `TX`/`RX` otherwise, `--` when the link is down.

## Configure

First boot: ESP32 raises `BPF-Setup-XXXXXX` SoftAP (XXXXXX = lower 24
bits of eFuse MAC). Join, browse `http://192.168.4.1/`, fill WiFi +
both radio endpoints + hostname (`SO2R-BPF` by default), save, reboot.
On station WiFi the portal is reachable at `http://<hostname>.local/`
(mDNS) or the DHCP‑assigned IP printed on serial at 115200. The
ESP32 re‑applies its hostname on `ARDUINO_EVENT_WIFI_STA_START` so
routers / IP scanners see `SO2R-BPF`, not the default `esp32-<mac>`.

Config persists to a 384‑byte EEPROM page (magic `0xBF50C0DE`,
version 2, CRC32). On magic/version/CRC mismatch the controller drops
back to AP‑portal mode.

Web routes:

| Route | Method | Purpose |
| --- | --- | --- |
| `/` | GET | HTML config form + manual‑bypass buttons |
| `/save` | POST | Persist form, return "Saved" |
| `/status` | GET | JSON: mode / wifi / R1+R2 connected, last band, tuning |
| `/bypass` | POST | `bpf=1\|2&on=0\|1` — manual force‑bypass |
| `/reboot` | POST | Soft reboot |
| `/factory_reset` | POST | Zero EEPROM (`confirm=YES` required) |

Serial commands at 115200: `status`, `reset`,
`bypass{1,2} {on,off}`.

## Build & flash

```bash
arduino-cli core install esp32:esp32
arduino-cli lib install WebSockets "LiquidCrystal I2C"
# First flash MUST be over USB (writes the partition table):
arduino-cli compile \
  --fqbn esp32:esp32:esp32:UploadSpeed=115200,PartitionScheme=min_spiffs \
  --upload --port /dev/cu.usbserial-XXXX ESP32_SO2R_TCI
# Subsequent flashes can use ArduinoOTA over Wi-Fi:
arduino-cli compile \
  --fqbn esp32:esp32:esp32:UploadSpeed=115200,PartitionScheme=min_spiffs \
  --upload --port SO2R-BPF.local ESP32_SO2R_TCI
```

- `PartitionScheme=min_spiffs` is required: each OTA slot becomes
  1.9 MB instead of the default 1.25 MB so the firmware fits with
  headroom for growth. The first USB flash writes the partition
  table itself; once that's in place, OTA can take over.
- `UploadSpeed=115200` works around CH340 noise at the 921600
  default.
- `WebSockets` is a transitive dep of TCI; `LiquidCrystal I2C`
  claims AVR‑only but works on ESP32 (Wire is portable — expect a
  harmless warning at compile time).
- On OTA start the firmware forces both BPFs to bypass (`onTuneChange(0, true);`
  `onTuneChange(1, true);`) so an in-flight ATU tune sweep can't
  hot‑switch a filter section while the binary is being rewritten.
  OTA defaults to no password — set `kOtaPassword` in
  `ESP32_SO2R_TCI.ino` and reflash over USB once to enable.

## Defaults / safety

- All outputs HIGH on boot (= bank de‑energised on the active‑LOW
  carrier) before WiFi starts.
- On WiFi drop, TCI drop, parse failure, or boot before first VFO
  event: bank is forced to bypass via WARC code. Filter sees either
  its built‑in bypass (Hamation) or all‑sections‑released (5B4AGN).
- On radio TUNE engaged: bank flips to bypass for the duration; the
  cached last‑known frequency is reused on release to restore the
  live band.
- Manual bypass (web/serial) latches the same flag a TCI tune event
  would; the LCD shows `TU` so a forgotten engagement is visible.

## Historical context

Two earlier sketches shaped the current firmware (no longer in the
repo, kept here for the design record):

- The original ESP8266 IF;-CAT reference sketch — gave us the
  Kenwood `IF;` over TCP polling pattern and the LC Technology
  `ESP12F_Relay_X8` carrier pinout. Superseded when the firmware
  moved to ESP32 + TCI.
- An ESP32 MQTT switch build — gave us the BPF 1 BCD pin map
  (16/17/18/19), the FreeRTOS LCD task pattern, and the active‑LOW
  relay convention. Differences vs the current firmware: MQTT
  replaced with TCI; inhibit lines dropped (all 8 relays consumed
  by two BCD banks); shared‑server mode added; tune auto‑bypass
  and the WARC bypass policy are new.

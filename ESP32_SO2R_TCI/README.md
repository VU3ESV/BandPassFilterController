# ESP32 SO2R / TCI variant

One ESP32 dev board → two TCI WebSocket connections → two Yaesu‑style BCD
band‑data buses → drives **two band‑pass filters at once** (5B4AGN TXBPF
and Hamation BandPasser II, or any other BCD‑input filter).

Each filter is dedicated to one radio: Radio 1 drives BPF 1, Radio 2 drives
BPF 2. No SO2R "BPF follows the RX radio" swap logic — see the MQTT
reference at [`../.support/ESP32MQTTSwitchV2.ino`](../.support/ESP32MQTTSwitchV2.ino)
for that mode if needed.

## Differences vs the ESP8266 sketches

| | `5B4AGN/`, `Hamation/` (ESP8266) | `ESP32_SO2R_TCI/` (this) |
| --- | --- | --- |
| MCU | ESP‑12F (ESP8266EX) | ESP32 (any dev board) |
| Radios | one per controller | **two per controller** |
| Protocol | Kenwood `IF;` over TCP | **TCI WebSocket** (native) |
| Filter | one (1‑of‑6 relay outputs) | **two** (4‑bit BCD per filter + inhibit) |
| Logic level | active‑HIGH 1‑of‑6 | **active‑LOW Yaesu BCD** |
| Config | web portal + EEPROM | hardcoded constants (first cut) |

## Hardware

ESP32 dev board (any) + an 8‑relay carrier wired up with active‑LOW inputs.
Pin map (matches [`../.support/ESP32MQTTSwitchV2.ino`](../.support/ESP32MQTTSwitchV2.ino)
for BPF 1; BPF 2 reuses the freed R1_Tx / R2_Tx / relay8 plus two spare
GPIOs):

| Function | BPF 1 GPIO | BPF 2 GPIO |
| --- | --- | --- |
| BCD A | 16 | 27 |
| BCD B | 17 | 32 |
| BCD C | 18 | 33 |
| BCD D | 19 | 25 |
| INHIBIT | 26 | 14 |

All outputs are **active‑LOW** — driven HIGH at boot to keep the relays
de‑energised. INHIBIT LOW = filter goes to bypass.

### Yaesu BCD codes used

```
Band   BCD (DCBA)   Inhibit
160 m  0001 (1)     HIGH (not inhibited)
80  m  0010 (2)     HIGH
40  m  0011 (3)     HIGH
20  m  0101 (5)     HIGH
15  m  0111 (7)     HIGH
10  m  1001 (9)     HIGH
WARC / 60 m / 6 m / OOB / disconnect:  all HIGH, INHIBIT LOW
```

## Dependencies

| Library | Source | Purpose |
| --- | --- | --- |
| `TCI` by IW7DMH v1.0.1 | bundled at [`../.support/TCI-2/`](../.support/TCI-2/) — see also <https://iw7dmh.jimdofree.com/sunsdr2-pages/tci-esp32s-arduino-libraries/> | TCI WebSocket client |
| `WebSockets` by Markus Sattler | install via Arduino Library Manager (`arduino-cli lib install WebSockets`) | required transitively by `TCI` |

### Install the bundled TCI library

```bash
# from the repo root
cp -R .support/TCI-2 ~/Documents/Arduino/libraries/TCI
# or symlink instead so future updates stay in sync:
# ln -s "$(pwd)/.support/TCI-2" ~/Documents/Arduino/libraries/TCI
```

Then install the WebSockets dep:

```bash
arduino-cli lib install WebSockets
```

## Configure

**Two ways** — pick whichever you prefer.

### Option A: web portal (recommended)

Identical UX to the ESP8266 sketches' portal but with **two** radio
sections.

1. On first boot (or after a factory reset), the controller raises a
   SoftAP named `BPF-Setup-XXXXXX` (`XXXXXX` = lower 24 bits of the ESP32
   eFuse MAC). Join it from a phone / laptop.
2. Browse to `http://192.168.4.1/`.
3. Fill in: WiFi SSID + password, hostname, Radio 1 (host/port/IARU
   region), Radio 2 (host/port/IARU region). Save → reboot.
4. Once on station WiFi, the portal is reachable at
   `http://<hostname>.local/` (mDNS) or the DHCP-assigned IP printed to
   serial at 115200.

Routes:

| Route | Method | Purpose |
| --- | --- | --- |
| `/` | GET | HTML form |
| `/save` | POST | URL-encoded form, writes EEPROM, returns "Saved" |
| `/status` | GET | JSON: filter / WiFi / R1 + R2 connected + last band / uptime |
| `/reboot` | POST | Soft reboot |
| `/factory_reset` | POST | Zero EEPROM (requires `confirm=YES`) |

Config is persisted to flash (EEPROM emulation, 384-byte page with magic
`0xBF50C0DE`, version 2, CRC32-checksummed). On magic / version / CRC
mismatch the controller drops back to AP-portal mode.

### Option B: hardcoded defaults

If you'd rather skip the portal entirely, edit `defaults()` in
[Config.h](Config.h) to set the values you want and let the firmware
fall back to them when the EEPROM is blank. Defaults today:

- Hostname: `bpf-so2r`
- Radio 1: `192.168.1.20:50001`, IARU region 1
- Radio 2: `192.168.1.21:50001`, IARU region 1

Default TCI port for ExpertSDR3 / SunSDR is **50001**. IARU region 1 / 2 / 3
sets the band edges (default 1 = Europe/Africa).

## Build / flash

```bash
arduino-cli core install esp32:esp32     # once
arduino-cli compile --fqbn esp32:esp32:esp32 ESP32_SO2R_TCI
arduino-cli upload --fqbn esp32:esp32:esp32 --port /dev/cu.usbserial-XXXX ESP32_SO2R_TCI
```

(Pick whichever ESP32 board profile matches your dev board — `esp32` is the
generic profile and works for nearly every variant.)

## Status / debug

Serial console at 115200 baud:

```
BandPassFilterController :: ESP32 SO2R / TCI
[wifi] connecting to 'PiHackerNet_Mesh'...
[wifi] up, ip=192.168.86.55
[R1] TCI connected
[R2] TCI connected
[R1] 20m @ 14250000 Hz (bcd=5 inh=0)
[R2] 40m @ 7080000 Hz (bcd=3 inh=0)
```

If either WiFi or a TCI link drops, the affected bank is forced to
INHIBIT (bypass) until the link comes back.

## What's not in this build

- **No LCD.** The reference MQTT sketch drives a 16×2 I²C LCD; left out
  here to keep the build small. Trivial to add back via the
  `LiquidCrystal_I2C` library.
- **No SO2R swap mode.** Each BPF is dedicated to one radio. Add a
  build‑time `#define BPF_SO2R_SWAP` and read each TCI's `getTrx()` /
  `getTxEnable()` state if you want the reference's
  "BPF follows the RX radio" behaviour later.

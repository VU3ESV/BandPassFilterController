# ESP32 SO2R / TCI variant

One ESP32 dev board → up to two TCI WebSocket connections → two
Yaesu‑style BCD band‑data buses → drives **two band‑pass filters at
once** (5B4AGN TXBPF, Hamation BandPasser II, or any other BCD‑input
filter).

Each BPF is dedicated to one receiver:

- **DUAL server mode** — two separate TCI servers (one per radio).
  Each filter follows its own radio's main RX VFO A.
- **SHARED server mode** — one TCI server feeds both filters. Useful
  for a dual‑receiver radio (e.g. SunSDR2 PRO) feeding two BPFs:
  RX‑1 drives BPF 1, RX‑2 drives BPF 2. Detected automatically when
  Radio 1 and Radio 2 have identical host/port in config.

No SO2R "BPF follows the RX radio" swap logic — see the MQTT reference
at [`../.support/ESP32MQTTSwitchV2.ino`](../.support/ESP32MQTTSwitchV2.ino)
for that mode if needed.

## Differences vs the ESP8266 sketches

| | `5B4AGN/`, `Hamation/` (ESP8266) | `ESP32_SO2R_TCI/` (this) |
| --- | --- | --- |
| MCU | ESP‑12F (ESP8266EX) | ESP32 (any dev board) |
| Radios | one per controller | **one or two per controller** |
| Protocol | Kenwood `IF;` over TCP | **TCI WebSocket** (native) |
| Filter | one (1‑of‑6 relay outputs) | **two** (4‑bit BCD per filter) |
| Logic level | active‑HIGH 1‑of‑6 | **active‑LOW Yaesu BCD** |
| Config | web portal + EEPROM | web portal + EEPROM |

## Hardware

One ESP32 dev board + one 8‑relay carrier (active‑LOW inputs). All 8
relays are used: 4 per filter for the two 4‑bit BCD buses. **No
inhibit line** in this build — the bypass state is "all four BCD
lines HIGH" on the affected bank, which is what both 5B4AGN and
Hamation see on a WARC band, on 6 m, or when the TCI link drops.

Pin map (BPF 1 matches [`../.support/ESP32MQTTSwitchV2.ino`](../.support/ESP32MQTTSwitchV2.ino)
verbatim because the 5B4AGN side is already wired):

| Function | BPF 1 GPIO | BPF 2 GPIO |
| --- | --- | --- |
| BCD A | 16 | 33 |
| BCD B | 17 | 32 |
| BCD C | 18 | 27 |
| BCD D | 19 | 26 |

All outputs are **active‑LOW** — driven HIGH at boot to keep the relays
de‑energised.

### Yaesu BCD codes used

Standard Yaesu "Table 5" band‑select assignment — all 10 HF/6 m
amateur bands get a unique code:

```
Band   BCD (DCBA)  decimal
160 m  0001        1
80  m  0010        2
40  m  0011        3
30  m  0100        4
20  m  0101        5
17  m  0110        6
15  m  0111        7
12  m  1000        8
10  m  1001        9
6   m  1010        10

60 m / OOB / disconnect / tune:  emits a nearest-WARC code (4, 6, or 8)
                                  to force the BPF into bypass — see below.
```

Note: the 5B4AGN TXBPF and Hamation BandPasser II only have internal
sections for the six contest bands (160/80/40/20/15/10 m). On
30/17/12/6 m the standard code is still emitted on the BCD bus, but
the BPF itself will bypass — its own decode table only recognises the
six contest codes.

#### Bypass uses a WARC code, not 0000

The Hamation BandPasser II decodes BCD `0000` as **160 m** — exactly
the opposite of bypass, and dangerous when the radio is mid-tune at
high power on a different band. To work around this, the firmware
never emits `0000` for bypass states. Instead, it picks the WARC band
code nearest the radio's last known frequency:

| Last known freq | Bypass code emitted |
| --- | --- |
| < 14 MHz (160/80/40 m)   | `0100` = 30 m (4) |
| 14–21 MHz (20 m)         | `0110` = 17 m (6) |
| > 21 MHz (15/10/6 m)     | `1000` = 12 m (8) |
| Unknown (link down/boot) | `0100` = 30 m (4) |

Hamation has no internal 30/17/12 m sections, so seeing a WARC code
trips its built-in bypass relay. 5B4AGN behaves the same way (no
matching contest section ⇒ all section relays released ⇒ bypass), so
this single policy is safe on both filters. Bypass states covered:
TUNE pressed, OOB, 60 m, TCI link down, WiFi down.

## Dependencies

| Library | Source | Purpose |
| --- | --- | --- |
| `TCI` by IW7DMH v1.0.1 | **bundled** — `TCI.h/.cpp` + `RTX.h/.cpp` live next to the .ino, gated unhandled‑message print | TCI WebSocket client |
| `WebSockets` by Markus Sattler | install via Arduino Library Manager (`arduino-cli lib install WebSockets`) | required transitively by `TCI` |
| `LiquidCrystal I2C` by Frank de Brabander | install via Arduino Library Manager (`arduino-cli lib install "LiquidCrystal I2C"`) | 16×2 I²C LCD driver. (The library's `library.properties` claims AVR‑only; it works on ESP32 anyway — `Wire.h` underneath is portable. Expect a harmless "may be incompatible" warning at compile time.) |

The TCI source files in this folder are a copy of
[`../.support/TCI-2/src/`](../.support/TCI-2/src/) with two small local
patches: the `<RTX.h>` angle‑bracket include is rewritten to the quoted
form so the files compile in‑place as sketch source, and the noisy
`Unhandled message!` print in `TCI::parse_message()` is wrapped in
`#if TCI_LOG_UNHANDLED` (default off). To re‑enable the diagnostic
print, add `-DTCI_LOG_UNHANDLED=1` to the build flags.

```bash
arduino-cli lib install WebSockets "LiquidCrystal I2C"
```

## Configure

**Two ways** — pick whichever you prefer.

### Option A: web portal (recommended)

Identical UX to the ESP8266 sketches' portal but with **two** radio
sections.

1. On first boot (or after a factory reset), the controller raises a
   SoftAP named `BPF-Setup-XXXXXX` (`XXXXXX` = lower 24 bits of the
   ESP32 eFuse MAC). Join it from a phone / laptop.
2. Browse to `http://192.168.4.1/`.
3. Fill in: WiFi SSID + password, hostname, Radio 1 (host/port/IARU
   region), Radio 2 (host/port/IARU region). Save → reboot.
4. Once on station WiFi, the portal is reachable at
   `http://<hostname>.local/` (mDNS) or the DHCP‑assigned IP printed to
   serial at 115200.

**Shared server tip:** to feed both BPFs from a single dual‑receiver
radio (SunSDR2 PRO etc.), enter the **same host and port** in Radio 1
and Radio 2. The firmware detects the match, opens one TCI client,
and routes RX‑1 (rig 0) to BPF 1 and RX‑2 (rig 1) to BPF 2. The portal
shows a hint about this under the Radio 2 section.

Routes:

| Route | Method | Purpose |
| --- | --- | --- |
| `/` | GET | HTML form |
| `/save` | POST | URL‑encoded form, writes EEPROM, returns "Saved" |
| `/status` | GET | JSON: filter / mode / WiFi / R1 + R2 connected + last band / uptime |
| `/reboot` | POST | Soft reboot |
| `/factory_reset` | POST | Zero EEPROM (requires `confirm=YES`) |

Config is persisted to flash (EEPROM emulation, 384‑byte page with
magic `0xBF50C0DE`, version 2, CRC32‑checksummed). On magic / version
/ CRC mismatch the controller drops back to AP‑portal mode.

### Option B: hardcoded defaults

If you'd rather skip the portal entirely, edit `defaults()` in
[Config.h](Config.h) to set the values you want and let the firmware
fall back to them when the EEPROM is blank. Defaults today:

- Hostname: `SO2R-BPF`
- Radio 1: `192.168.1.20:50001`, IARU region 1
- Radio 2: `192.168.1.21:50001`, IARU region 1

Default TCI port for ExpertSDR3 / SunSDR is **50001**. IARU region 1 / 2
/ 3 sets the band edges (default 1 = Europe/Africa).

## Build / flash

```bash
arduino-cli core install esp32:esp32     # once
arduino-cli lib install WebSockets "LiquidCrystal I2C"
arduino-cli compile --fqbn esp32:esp32:esp32:UploadSpeed=115200 \
  --upload --port /dev/cu.usbserial-XXXX ESP32_SO2R_TCI
```

(Pick whichever ESP32 board profile matches your dev board — `esp32`
is the generic profile and works for nearly every variant. Forcing
`UploadSpeed=115200` works around CH340 noise at the default 921600.)

## Status / debug

Serial console at 115200 baud:

```
BandPassFilterController :: ESP32 SO2R / TCI
[wifi] STA connecting to 'HomeMesh'.........
[wifi] STA up, ip=192.168.86.55
[mdns] http://SO2R-BPF.local/
[tci] shared server mode -> 192.168.1.20:50001 (BPF1=rig0, BPF2=rig1)
[R1] TCI conn event
[R1] 20m @ 14250000 Hz (bcd=5 inh=0)
[R2] 40m @ 7080000 Hz (bcd=3 inh=0)
```

If either WiFi or a TCI link drops, the affected bank is forced to
bypass (all BCD lines released HIGH) until the link comes back, and
column 0 of the affected LCD row changes from a space to `*`.

## 16×2 I²C LCD

Connect a standard PCF8574‑backed 16×2 LCD to the ESP32's I²C pins
(`SDA` = GPIO 21, `SCL` = GPIO 22 on most dev boards) and 5 V / GND.
The LCD comes up at 0x27 by default; some modules ship at 0x3F —
change the `g_lcd.begin()` argument in `setup()` if yours is 0x3F.

Layout (16 columns):

```
col:    0123456789012345
Row 0:  ' 14.2500 USB RX'      <- BPF 1: link OK, 20m USB, receiving
Row 1:  '* 7.1000 LSB --'      <- BPF 2: TCI link DOWN (asterisk),
                                  stale freq, dashes for state
```

- **Col 0**: link indicator. `' '` = TCI connected (and WiFi up);
  `'*'` = TCI down → that BPF is in bypass.
- **Col 1–8**: frequency in MHz, four decimals (10 Hz resolution).
- **Col 10–12**: mode reported by TCI (USB, LSB, CW, DIGU, ...).
- **Col 14–15**: `TU` while the radio's TUNE button is engaged (the
  BPF is forced to bypass for the duration), `TX` while transmitting,
  `RX` otherwise, `--` when the link is down.

The LCD refresh task runs on core 1 at 150 ms cadence and pushes only
the columns that changed since the last frame; the I²C bus is clocked
at 400 kHz. Together this keeps a frequency tick on the dial visible
on the LCD with no perceptible lag.

## What's not in this build

- **No SO2R swap mode.** Each BPF is dedicated to one radio /
  receiver. Add a build‑time `#define BPF_SO2R_SWAP` and read each
  TCI's `getTrx()` / `getTxEnable()` state if you want the reference's
  "BPF follows the RX radio" behaviour later.
- **No verbose TCI dump.** The bundled TCI library gates its
  "Unhandled message!" diagnostic behind `TCI_LOG_UNHANDLED`; set it
  to `1` at build time to flood the console with every TCI frame the
  firmware ignores.

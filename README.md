# BandPassFilterController

ESP32 firmware that drives **two HF contest band‑pass filters at once**
from one or two SDR radios over the **TCI WebSocket** protocol.
One ESP32 dev board on an 8‑relay carrier emits two independent
Yaesu‑style 4‑bit BCD band‑data buses — one per BPF.

| Filter (BCD‑input) | Notes |
| --- | --- |
| **5B4AGN TXBPF** ([group](https://groups.io/g/TXBPF)) | High‑power contest filter, six switched sections (160/80/40/20/15/10 m). |
| **Hamation MBF‑100 BandPasser II** ([product](https://www.hamation.com/Bandpasser.html)) | Six‑band contest BPF, +5–12 V per‑band on rear control connector. Built‑in bypass relay on unrecognised codes. |

Single sketch: [`ESP32_SO2R_TCI/`](ESP32_SO2R_TCI/).

## Features

- **One or two TCI radios.** Auto‑detected from config:
  - **Dual mode** — Radio 1 and Radio 2 are different servers (one TCI
    client per filter, each following its radio's RX‑1 VFO A).
  - **Shared mode** — Radio 1 and Radio 2 share the same `host:port`
    (one TCI client; rig 0 → BPF 1, rig 1 → BPF 2). Right wiring for a
    dual‑receiver radio like SunSDR2 PRO feeding both filters.
- **Standard Yaesu BCD encoding** for all 10 HF/6 m bands
  (160/80/40/30/20/17/15/12/10/6 m, codes 1–10). 60 m and OOB drop to
  bypass.
- **Auto‑bypass during ATU tune.** When the radio's TCI server emits
  `tune:RIG,true;`, the affected bank is forced to bypass for the
  tune cycle, then restored on release. Saves the filter from being
  hit with a wide tuning sweep at high power.
- **Manual bypass path** for radios whose TCI server doesn't emit
  tune (AetherSDR, etc.) — clickable buttons on the web portal plus
  serial commands.
- **Hamation‑safe bypass.** Hamation reads BCD `0000` as 160 m, not
  bypass. So the firmware never emits `0000` for any bypass state —
  it picks a nearest‑WARC code (30/17/12 m) that triggers Hamation's
  built‑in bypass relay. Harmless on 5B4AGN (no matching section ⇒
  all section relays released).
- **Failsafe.** Bank is forced to bypass on boot, WiFi drop, TCI
  link drop, parse failure, or out‑of‑band frequency.
- **16×2 I²C LCD** showing per‑radio frequency, mode, TX/RX/TU state,
  and a link indicator in column 0 (`' '` = up, `'*'` = down). 150 ms
  redraw, 400 kHz I²C, delta‑only column writes — frequency ticks
  show up with no perceptible lag.
- **Web portal** for WiFi credentials, both radio endpoints,
  hostname; config persisted to EEPROM with CRC32, captive‑portal
  fallback on first boot or CRC mismatch.
- **mDNS** at `<hostname>.local/` (default `SO2R-BPF`). Hostname is
  re‑applied on `ARDUINO_EVENT_WIFI_STA_START` so DHCP sees it
  reliably.

## Quick start

1. Wire one ESP32 dev board to an active‑LOW 8‑relay carrier per the
   pin map in [`ESP32_SO2R_TCI/README.md`](ESP32_SO2R_TCI/README.md)
   (BPF 1 = GPIO 16/17/18/19, BPF 2 = GPIO 33/32/27/26). Optional 16×2
   I²C LCD on GPIO 21/22.
2. Install deps:
   ```bash
   arduino-cli core install esp32:esp32
   arduino-cli lib install WebSockets "LiquidCrystal I2C"
   ```
3. Flash:
   ```bash
   arduino-cli compile --fqbn esp32:esp32:esp32:UploadSpeed=115200 \
     --upload --port /dev/cu.usbserial-XXXX ESP32_SO2R_TCI
   ```
   (`UploadSpeed=115200` avoids CH340 noise at 921600.)
4. First boot raises `BPF-Setup-XXXXXX` SoftAP at `192.168.4.1`.
   Configure WiFi + both radio endpoints (default TCI port is `50001`)
   + hostname, save, reboot. Then reach the portal at
   `http://SO2R-BPF.local/`.

The TCI library (IW7DMH v1.0.1) is bundled inside the sketch folder —
no `~/Documents/Arduino/libraries/` install needed.

## Manual bypass (for non‑tune‑emitting radios)

- Web portal root has a "Manual bypass" fieldset with four buttons.
- Serial at 115200: `bypass1 on`, `bypass1 off`, `bypass2 on`,
  `bypass2 off`.
- HTTP: `curl -X POST 'http://SO2R-BPF.local/bypass?bpf=1&on=1'`

All three latch the same internal flag the TCI tune event would,
so the LCD shows `TU` and the WARC bypass code is emitted while
engaged.

## Status

Released. Tested end‑to‑end against:
- **SunSDR2 PRO + ExpertSDR3** (shared mode; dual‑receiver feeding
  both filters via single TCI server).
- **AetherSDR + TCI** (dual mode; manual bypass since AetherSDR's
  TCI doesn't emit tune events).

See [`CLAUDE.md`](CLAUDE.md) for the full architecture rationale and
[`ESP32_SO2R_TCI/README.md`](ESP32_SO2R_TCI/README.md) for the
sketch‑level reference (pins, bypass policy, web routes, dependency
list). The `docs/` folder contains the legacy ESP8266‑era notes and
is no longer authoritative.

## License

MIT. See [LICENSE](LICENSE).

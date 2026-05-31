# Hardware

## Bill of materials

| Item | Qty | Notes |
| --- | --- | --- |
| ESP32 dev board (generic) | 1 | Any DOIT / DevKit‑C style board with on‑board USB. ESP32‑WROOM‑32 is what this firmware is tested against. |
| 8‑channel relay carrier, **active‑LOW**, 5 V | 1 | Optoisolated input is preferred. Examples: SongHe / Tongling 8‑channel boards; the cheap "8 Channel 5V Relay Module" sold under many names. |
| 16×2 I²C LCD, PCF8574 backpack | 1 (optional) | Default I²C address `0x27`; some modules ship `0x3F`. |
| Jumper wire, ribbon cable | as needed | For the BCD bus to each BPF and power |
| 5 V supply | 1 | Powers ESP32 dev board via USB *and* the relay carrier's `VCC`. ~250 mA peak with all 8 relays active. |

The earlier ESP8266 LC Technology `ESP12F_Relay_X8` board (active‑HIGH,
no on‑board USB) is **no longer the target** — the firmware now runs on
a generic ESP32 dev board with the relay carrier as a separate module.

## Pin map

The BCD pins for BPF 1 (16/17/18/19) match
[`../.support/ESP32MQTTSwitchV2.ino`](../.support/ESP32MQTTSwitchV2.ino)
deliberately, so the existing 5B4AGN wiring from the reference MQTT
build carries over. BPF 2 uses the remaining four output GPIOs.

| Function | BPF 1 GPIO | BPF 2 GPIO | Boot behaviour |
| --- | --- | --- | --- |
| BCD A (LSB) | 16 | 33 | clean |
| BCD B | 17 | 32 | clean |
| BCD C | 18 | 27 | clean |
| BCD D (MSB) | 19 | 26 | clean |
| I²C SDA | 21 | — | clean (LCD) |
| I²C SCL | 22 | — | clean (LCD) |

All eight BCD outputs are **active‑LOW** — driven HIGH at boot (in
`setupBank()`) to keep the relays de‑energised. None of these GPIOs
have strapping conflicts at reset, so there's no boot‑time glitching
to engineer around (unlike the ESP8266 GPIO0 / GPIO15 / GPIO16
gotchas on the LC Tech ESP12F board).

GPIO 0 is left untouched (used by the on‑board USB‑serial chip for
flash mode). GPIO 1 / 3 are the on‑board UART0 (Serial console).
GPIO 2 (on‑board LED on many dev boards) is free.

## BCD bus → BPF

Each BPF expects a 4‑bit BCD bus on its rear‑panel control connector.
Wire the GPIO outputs (post‑relay if your carrier passes through;
direct if you're tapping the GPIOs straight) to the corresponding
A/B/C/D inputs:

```
        ESP32 ──┐
                │ active-LOW: pin pulled LOW = bit set
                ▼
   GPIO 16 ── relay 1 NO ── BPF 1 BCD A
   GPIO 17 ── relay 2 NO ── BPF 1 BCD B
   GPIO 18 ── relay 3 NO ── BPF 1 BCD C
   GPIO 19 ── relay 4 NO ── BPF 1 BCD D
   GPIO 33 ── relay 5 NO ── BPF 2 BCD A
   GPIO 32 ── relay 6 NO ── BPF 2 BCD B
   GPIO 27 ── relay 7 NO ── BPF 2 BCD C
   GPIO 26 ── relay 8 NO ── BPF 2 BCD D
   GND    ── relay coil ground ── BPF control ground (common)
```

Use the relay contacts to switch the BPF's +5 V (Hamation) or
+12 V band line, and tie the relay coils to the ESP32's GPIO via
the carrier's optoisolated input header.

If your BPF's BCD inputs are 3.3 V‑tolerant CMOS (some custom builds
are), you can skip the relay carrier entirely and run GPIOs straight
to the BCD pins. The active‑LOW convention then needs an inverter or
a software flip; today the firmware emits active‑LOW because the
target carrier is active‑LOW.

### 5B4AGN TXBPF

The TXBPF reads a 4‑bit BCD code (Yaesu Table 5) on its rear‑panel
control connector. A matching code engages the corresponding filter
section; an unmatched code (WARC, 6 m, or `0000`) releases all six
section relays — the filter passes through its bypass path.

Wire the 4 relay contacts of BPF 1's bank to the TXBPF control PCB's
BCD A/B/C/D inputs, with common ground. Verify against your specific
TXBPF build (community variations exist) on the
[TXBPF groups.io community][txbpf] (free membership required) before
applying RF.

### Hamation MBF‑100 BandPasser II

Per the [Hamation product page][hamation], the BandPasser II's rear
control connector accepts a 4‑bit BCD code. The decoder reads the
code and engages one of the six contest sections; unrecognised codes
(WARC 30/17/12 m, 6 m) **engage Hamation's built‑in bypass relay**.

There is one critical quirk: **BCD `0000` is decoded as 160 m**, not
bypass. So the firmware never emits `0000` on a bank — it picks a
WARC code instead (see "Bypass policy" in
[ARCHITECTURE.md](ARCHITECTURE.md)). With WARC codes Hamation safely
bypasses regardless of which BPF is wired on which bank.

Power Hamation from its own +12 V supply (~100 mA); share ground
with the ESP32 / relay carrier so the relay contacts can switch the
BCD bus reference.

## I²C LCD wiring

```
   ESP32 GPIO 21 (SDA) ── LCD backpack SDA
   ESP32 GPIO 22 (SCL) ── LCD backpack SCL
   ESP32 5V            ── LCD backpack VCC
   ESP32 GND           ── LCD backpack GND
```

The I²C clock is bumped to 400 kHz in `LcdDisplay::begin()` so a
full‑row redraw is fast enough to keep up with frequency ticks.

If your module's I²C address is `0x3F` instead of `0x27`, change the
call in `setup()`:

```cpp
bpf::g_lcd.begin(0x3F);
```

A quick way to find out: install the `i2cdetect` example from the
`Wire` library or any address scanner and watch the serial output.

## Power

- ESP32 dev board: powered from USB (typical) or from a regulated
  5 V source on `VIN` / `5V`. Current draw is ~120 mA average.
- Relay carrier: powered from the same 5 V rail. ~70 mA per
  energised coil; with 4 of 8 relays on at any band, peak is around
  280 mA + the ESP32, so a 500 mA supply is comfortable.
- BPF supplies are independent (each filter has its own ±12 V or
  similar). Common ground all three.

## Programming the board

ESP32 dev boards have an on‑board USB‑serial chip (CP2102 or CH340)
and auto‑reset circuitry — no jumpering, no buttons. Plug the USB
cable in and:

```bash
arduino-cli core install esp32:esp32
arduino-cli lib install WebSockets "LiquidCrystal I2C"
arduino-cli compile --fqbn esp32:esp32:esp32:UploadSpeed=115200 \
  --upload --port /dev/cu.usbserial-XXXX ESP32_SO2R_TCI
```

`UploadSpeed=115200` is intentional — the default 921600 introduces
sync errors on cheap CH340 clones. The compile + flash takes about
80 seconds on a stock Mac.

### macOS USB‑serial caveats (verified 2026‑05)

- **CH340 / CH341 clones**: install the WCH driver from
  <https://www.wch.cn/downloads/CH34XSER_MAC_ZIP.html>. The native
  macOS driver works for some board variants but glitches at high
  baud rates. Always force `UploadSpeed=115200`.
- **CP2102**: native macOS driver works out of the box; ports
  appear as `/dev/cu.usbserial-0001` or similar.
- **Prolific PL2303 (VID 0x067B, 0x2192)**: the 0x2192 variant
  triggers `termios EINVAL` on macOS during `_set_port_baudrate`.
  Switch to a CP2102 or FTDI cable if you hit it.
- Devices like the **SunSDR E‑Coder** front panel enumerate as
  Prolific too — `ls /dev/cu.*` can show unrelated USB‑serial ports.
  Check `ioreg -p IOUSB | grep -i product` to identify which one is
  your ESP32 dev board before flashing.

### When the upload says "port busy"

Close every serial monitor (`screen`, `cu`, `pyserial`, Arduino IDE
Serial Monitor) before retrying. macOS won't grant exclusive access
to a port that another process has open.

## Quick sanity check before applying RF

1. Power the board from USB only — no RF, no BPFs connected.
2. Watch serial at 115200. You should see:
   ```
   BandPassFilterController :: ESP32 SO2R / TCI
   [wifi] AP 'BPF-Setup-A1B2C3' at 192.168.4.1
   ```
   (or, after configuration, `[wifi] STA up, ip=…`).
3. Join the AP, browse to `192.168.4.1`, fill the form, save, reboot.
4. Once on station WiFi, point your radio's TCI server at it and
   tune to 160 m. Expect:
   - LCD row 0 (or 1, depending on which radio) shows `' 1.8500 LSB RX'`.
   - Serial: `[R1] 160m @ 1850000 Hz (bcd=1 inh=0 tune=0)`.
   - On the BPF 1 bank: GPIO 16 LOW, 17/18/19 HIGH.
5. Step through each band; for WARC bands (30/17/12 m) expect:
   - LCD shows the freq normally.
   - Bank emits the WARC code itself (e.g. `bcd=4 inh=0` on 30 m).
   - On 5B4AGN / Hamation, the filter bypasses (no matching section).
6. Press TUNE on the radio (if its TCI server emits tune events).
   Expect LCD state column to flip to `TU`, serial:
   `[BPF1] TUNE change -> ON (forcing bypass)` then
   `[R1] bypass @ … (bcd=<warc> inh=1 tune=1)`. Release TUNE — the
   bank snaps back to the live band.
7. *Only then* connect RF.

## References

**TCI library**

- [IW7DMH TCI for ESP32 / Arduino — v1.0.1][iw7dmh]
  — upstream library, bundled into the sketch as `TCI.h/.cpp` +
  `RTX.h/.cpp` with local patches (see
  [`../ESP32_SO2R_TCI/README.md`](../ESP32_SO2R_TCI/README.md)).
- [Expert Electronics TCI protocol specification][tci-spec]
  — protocol reference for ExpertSDR3 / SunSDR / MB1.

**Filters**

- [Hamation MBF‑100 BandPasser II product page][hamation] — six
  contest bands, +5–12 V per band on rear control connector.
  Critical decoder quirk: `0000` = 160 m (not bypass).
- [TXBPF groups.io community][txbpf] — discussion / schematics
  for the 5B4AGN TX band‑pass filter. **Gated**: a free groups.io
  membership is required to read the archive.

**ESP32 reference sketch**

- [`../.support/ESP32MQTTSwitchV2.ino`](../.support/ESP32MQTTSwitchV2.ino)
  — earlier MQTT‑driven ESP32 build that the BCD pin map for BPF 1
  and the FreeRTOS LCD task pattern came from. Differences vs the
  current firmware: MQTT replaced with TCI; inhibit lines dropped
  (all 8 relays used by two BCD banks); shared‑server mode added;
  tune auto‑bypass and WARC‑bypass policy are new.

**Legacy — ESP8266 era (not the current target)**

- [`../.support/ESP12TCPClientV6.ino`](../.support/ESP12TCPClientV6.ino)
  — original ESP8266 IF; CAT reference sketch. Not used by the
  current firmware but kept for historical context.

[iw7dmh]: https://iw7dmh.jimdofree.com/sunsdr2-pages/tci-esp32s-arduino-libraries/
[tci-spec]: https://eesdr.com/en/manuals-en/eesdr3-en/tci-protocol-en
[hamation]: https://www.hamation.com/Bandpasser.html
[txbpf]: https://groups.io/g/TXBPF

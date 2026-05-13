# Hardware

## Target board: LC Technology `ESP12F_Relay_X8`

The 8‑channel ESP‑12F WiFi relay board sold under many names (LC Technology,
"eWeLink ESP8266 8‑Channel", "ESP12F_Relay_X8", DC 5 V or 7–28 V input,
active‑HIGH relays). The product page the user originally pointed to is
[Fruugo — "8‑Channel ESP8266 Wireless WiFi Relay Module ESP‑12F"][fruugo];
the listing's description is sparse and partially incorrect, so the
authoritative pinout below is taken from the Tasmota and ESPHome device
databases — see the [References](#references) section at the end of this
document.

> The original `.support/ESP12TCPClientV6.ino` reference sketch numbers the
> `Relay1..Relay8` macros in **reverse** order from the board silkscreen
> (Relay 1 in the code is the relay labelled *Relay 8* on the PCB). The
> GPIOs themselves are correct — it's the labelling that's flipped. This
> firmware uses the silkscreen labels.

### Canonical Relay → GPIO map (silkscreen)

The table below is the consensus mapping across the
[Tasmota `ESP12F_Relay_X8` template][tasmota] and the
[ESPHome `esp-12f-relay-x8` device page][esphome], cross‑checked against
Werner Rothschopf's hands‑on [browser‑switch write‑up][rothschopf].

| Silkscreen | GPIO | Boot behaviour | Used by this firmware |
| --- | --- | --- | --- |
| Relay 1 | **GPIO16** | **Pulses ON at boot** — GPIO16 floats high briefly before `setup()` drives it low ([noted by Tasmota][tasmota]) | **No** (skipped — see below) |
| Relay 2 | GPIO14 | clean | **Yes — 160 m** |
| Relay 3 | GPIO12 | clean | **Yes — 80 m** |
| Relay 4 | GPIO13 | clean | **Yes — 40 m** |
| Relay 5 | GPIO15 | board pulls LOW (required boot‑strap); stays OFF at boot | **Yes — 20 m** |
| Relay 6 | **GPIO0**  | **Pulses ON at boot** — GPIO0 must be HIGH for normal boot; the carrier holds it HIGH → relay energised until firmware drives it LOW | **No** (skipped — see below) |
| Relay 7 | GPIO4  | clean | **Yes — 15 m** |
| Relay 8 | GPIO5  | clean | **Yes — 10 m** |

All relays on this board are **active‑HIGH** (relay coil energised when the
GPIO is driven HIGH). Contacts are typically 10 A @ 250 VAC / 10 A @ 30 VDC
(rating depends on the relay variant fitted; check yours before pushing
mains).

### Why we skip Relay 1 and Relay 6

On power‑up the ESP8266 needs **GPIO0 HIGH** (to boot from flash) and
**GPIO15 LOW** (boot‑strap), and **GPIO16** has no internal pull and tends
to float HIGH for a few hundred milliseconds before the firmware initialises
it. On this board:

- GPIO16 → Relay 1: energises briefly at every reboot. Documented behaviour;
  Tasmota notes it explicitly.
- GPIO0  → Relay 6: HIGH at boot is *required*, so Relay 6 is energised from
  power‑up until the firmware in `setup()` drives it LOW.

For a hot station that is a problem — a brief 160 m or 10 m relay pulse
could feed an in‑transmit RF path into the wrong filter section. By
assigning band lines to Relays **2, 3, 4, 5, 7, 8** only, none of the six
contest bands glitches at boot. Relays 1 and 6 are left unused; their
boot‑time pulses are harmless because nothing is wired to them.

### Firmware ↔ band ↔ relay table

| Band | GPIO (firmware constant) | Silkscreen relay | Wire band line here |
| --- | --- | --- | --- |
| 160 m | GPIO14 (`kPin160`) | **Relay 2** | NO/COM of Relay 2 |
| 80 m  | GPIO12 (`kPin80`)  | **Relay 3** | NO/COM of Relay 3 |
| 40 m  | GPIO13 (`kPin40`)  | **Relay 4** | NO/COM of Relay 4 |
| 20 m  | GPIO15 (`kPin20`)  | **Relay 5** | NO/COM of Relay 5 |
| 15 m  | GPIO4  (`kPin15`)  | **Relay 7** | NO/COM of Relay 7 |
| 10 m  | GPIO5  (`kPin10`)  | **Relay 8** | NO/COM of Relay 8 |

## Wiring to the filter

### 5B4AGN TXBPF

The TXBPF design exposes one band‑select line per filter section. Wire the
NO contact of each of the six relays above to the matching TXBPF band‑select
input, with a common ground between the carrier and the TXBPF control PCB.

When all six relays are de‑energised the TXBPF should sit in its all‑off /
bypass state. **Verify against your specific TXBPF build** by cross‑checking
with the [TXBPF groups.io community][txbpf] (gated; membership required) —
community builds vary; check the schematic before applying RF.

### Hamation MBF‑100 BandPasser II

Per the [Hamation product page][hamation], the BandPasser II expects
**+5 V to +12 V** on a per‑band input on its rear‑panel control connector
and "automatically enters Bypass mode … when no power is applied or no
filters are selected." The relay contacts on this carrier are the right
interface:

```
  +12 V ──┬────────────────────────────────────────────────────────
          │
          ├── Relay 2 NO ── 160 m band input
          ├── Relay 3 NO ── 80 m  band input
          ├── Relay 4 NO ── 40 m  band input
          ├── Relay 5 NO ── 20 m  band input
          ├── Relay 7 NO ── 15 m  band input
          └── Relay 8 NO ── 10 m  band input
  GND  ──── BandPasser II ground (common)
```

With all six relays de‑energised, no band input is fed and the BandPasser II
auto‑bypasses. That matches the firmware's bypass behaviour.

If you prefer a solid‑state interface, a ULN2003A / ULN2803A open‑collector
sink can replace the relay contacts; tie its inputs to the same GPIOs and
its outputs (commoned with +12 V via the BandPasser II inputs) to the band
lines.

## Power

The carrier accepts 5 V at the screw terminal or 7–28 V at the wider input
(internal buck to 5 V, then LDO to 3.3 V for the ESP‑12F). Current draw is
typically < 100 mA idle plus ~70 mA per energised relay coil.

The Hamation needs its own 12–14 V supply at ~100 mA — share ground with the
carrier so the relay contacts can switch the Hamation's +12 V rail.

## Programming the board

The carrier brings out the ESP‑12F flash header. To flash:

1. Hold the on‑board **PROG** button (or short GPIO0 to GND).
2. Press / release **RESET**.
3. Release **PROG**.
4. Upload at 115200 baud, *Generic ESP8266 Module* (or *NodeMCU 1.0*).

Power the board from USB‑to‑serial during flashing; do not feed it from the
high‑voltage input at the same time.

## Optional additions

- **Status LED.** Spare Relay 7 / Relay 8 outputs can drive an LED. The
  firmware currently uses Relay 7 (15 m) and Relay 8 (10 m); if you want a
  status LED, drop one of the unused bands or wire an LED off the ESP‑12F's
  on‑board LED pad (GPIO2).
- **Factory‑reset jumper.** A momentary switch from any unused GPIO to GND
  can be sampled in `setup()`; not enabled by default to avoid surprise
  behaviour. Pick a pin not used as a relay drive.

## Quick sanity check before applying RF

1. Power the board with USB only (no RF connected).
2. Watch the relay LEDs through `setup()`. Relays 1 and 6 may flash; Relays
   2/3/4/5/7/8 must stay **OFF**.
3. Once the firmware is up, tune the radio to 160 m → Relay 2 should
   energise on its own. Step through each band and verify the matching relay
   activates. Tune to a WARC band → all relays must drop.
4. *Only then* connect the filter's RF and band‑select wiring.

## References

Sources used to verify the pinout, boot‑time behaviour, and filter control
interfaces in this document.

**Carrier board — LC Technology `ESP12F_Relay_X8`**

- [Fruugo product listing — "8‑Channel ESP8266 Wireless WiFi Relay Module ESP‑12F"][fruugo]
  — original product page that prompted this project. Description is sparse
  and partially inaccurate; cited here only as the storefront link.
- [Tasmota — `ESP12F_Relay_X8` template][tasmota] — authoritative
  Relay→GPIO mapping; flags the GPIO16 / Relay 1 boot pulse.
- [ESPHome — `esp-12f-relay-x8` device][esphome] — cross‑check of the same
  pinout in an independent firmware project.
- [Werner Rothschopf — "ESP8266 ESP12F Relay X8 board to switch pins with browser"][rothschopf]
  — hands‑on confirmation of pinout, programming sequence, and active‑HIGH
  relay logic.

**ESP8266 boot strapping**

The boot‑mode requirements (GPIO0 HIGH, GPIO15 LOW, GPIO2 HIGH at reset)
are part of the ESP8266 hardware spec; see Espressif's
[ESP8266 hardware design guidelines][esp-hw] §2.4 for the canonical
strap‑pin table.

**Filters**

- [Hamation MBF‑100 BandPasser II product page][hamation] — six contest
  bands (160/80/40/20/15/10), +5–12 V per‑band rear‑panel control,
  auto‑bypass with no input applied.
- [TXBPF groups.io community][txbpf] — discussion / variants of the 5B4AGN
  TX band‑pass filter. **Gated**: a free groups.io membership is required to
  read the message archive and uploaded schematics.

[fruugo]: https://www.fruugonorge.com/8-channel-esp8266-wireless-wifi-relay-module-esp-12f-development-board-dc-5v7-28v-e-welink-app-remo/p-350285115-763787021?language=en
[tasmota]: https://templates.blakadder.com/ESP12F_Relay_X8.html
[esphome]: https://devices.esphome.io/devices/esp-12f-relay-x8/
[rothschopf]: https://werner.rothschopf.net/microcontroller/202108_esp8266_esp12f_relay_x8_en.htm
[esp-hw]: https://www.espressif.com/sites/default/files/documentation/esp8266_hardware_design_guidelines_en.pdf
[hamation]: https://www.hamation.com/Bandpasser.html
[txbpf]: https://groups.io/g/TXBPF

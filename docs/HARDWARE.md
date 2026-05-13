# Hardware

## Target board

ESP‑12E or ESP‑12F module on a generic 8‑relay carrier board (the same board
the reference sketch `.support/ESP12TCPClientV6.ino` targets). 5 V supply,
opto‑isolated active‑HIGH relays.

## Pin assignment (both sketches)

| Function | Net | GPIO | Notes |
| --- | --- | --- | --- |
| Band 160 m | Relay 1 | GPIO5  | active HIGH |
| Band 80 m  | Relay 2 | GPIO4  | |
| Band 40 m  | Relay 3 | GPIO0  | boot‑sensitive; pulled HIGH on carrier |
| Band 20 m  | Relay 4 | GPIO15 | boot‑sensitive; pulled LOW on carrier |
| Band 15 m  | Relay 5 | GPIO13 | |
| Band 10 m  | Relay 6 | GPIO12 | |
| Spare / status LED | Relay 7 | GPIO14 | wired but unused by default |
| Spare / PTT‑inhibit | Relay 8 | GPIO16 | wired but unused by default; no interrupts |

The mapping intentionally matches the reference sketch so the same carrier
board can be reused without rework. **Differences from the reference firmware**:
the reference shares Relay 3 between 30 m and 40 m and Relay 4 between 17 m
and 20 m. In a contest BPF that is incorrect — the WARC bands must route to
bypass — so this firmware only asserts a band line for the six contest bands.

## Wiring to the filter

### 5B4AGN TXBPF

The TXBPF design exposes one band‑select line per filter section. Connect the
six relay outputs of the carrier (Relay 1 … Relay 6 above) to the band‑select
inputs of the TXBPF controller PCB, plus a common ground.

When all six lines are de‑asserted the TXBPF should be in its all‑off /
bypass state. **Verify against your specific TXBPF build** — community builds
vary; check the schematic before applying RF.

### Hamation MBF‑100 BandPasser II

The BandPasser II expects **+5 V to +12 V** on a per‑band input on its rear‑
panel control connector. The ESP‑12 puts out 3.3 V logic which is too low for
this directly, so the 8‑relay carrier's relay contacts are the right
interface:

```
  +12 V ──┬────────────────────────────────────────────────────────
          │
          ├── relay 1 NO ── 160 m band input
          ├── relay 2 NO ── 80 m  band input
          ├── relay 3 NO ── 40 m  band input
          ├── relay 4 NO ── 20 m  band input
          ├── relay 5 NO ── 15 m  band input
          └── relay 6 NO ── 10 m  band input
  GND  ──── BandPasser II ground (common)
```

With all relays de‑energised, no band input is fed and the BandPasser II
auto‑bypasses. That matches the firmware's bypass behaviour.

If you prefer a solid‑state interface, a ULN2003A / ULN2803A open‑collector
sink can replace the relay contacts; tie its inputs to the same GPIOs and its
outputs (commoned with +12 V via the BandPasser II inputs) to the band lines.

## Boot‑mode caveats

GPIO0 and GPIO15 are sampled at power‑up to decide flash vs. run mode. The
8‑relay carrier board has the required pull resistors so that with all relay
inputs idle the chip boots into normal run mode. Two consequences:

- Never wire the GPIO0 / GPIO15 lines to anything that pulls them in the
  opposite direction during boot.
- `setup()` drives all six relay GPIOs to `LOW` **before** `WiFi.begin()`, so
  the carrier's idle state is held during boot.

## Power

The carrier accepts 5 V at the barrel jack and runs an LDO to 3.3 V for the
ESP‑12. The Hamation needs its own 12–14 V supply at ~100 mA — share ground
with the carrier so the relay contacts can switch the Hamation's +12 V rail.

## Optional additions

- **Status LED on GPIO14.** Solid = WiFi+TCP up, slow blink = WiFi only,
  fast blink = AP/portal mode. The firmware leaves the GPIO free; wiring an
  LED through the existing Relay 7 channel is enough.
- **Factory‑reset jumper.** Tie a momentary switch from GPIO16 to GND. The
  firmware samples this pin on boot; if held LOW for 3 s, it wipes EEPROM and
  re‑enters captive portal mode. (Hook the read in `setup()`; not enabled by
  default to avoid surprise behaviour on existing carriers.)

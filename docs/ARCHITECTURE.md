# Architecture

## Runtime overview

```
                                       +-------------------+
   Radio  ─ USB/CAT/TCI ─►  Thetis  ──►│ TCP server :13013 │ ◄── ESP-12 ──► BPF
   (any)                   /Node-Red   │  speaks IF;…      │     (this fw)
                           /rigctld    +-------------------+
```

One ESP‑12 controls **one** filter and watches **one** radio. The pairing
between radio and filter is configuration (host / port / protocol stored in
EEPROM), so the two are freely swappable from the web portal without a
recompile.

The firmware is a cooperative single‑threaded loop that does three things:

1. **WiFi + radio link.** Maintain WiFi association; maintain TCP session to
   the configured radio endpoint; poll `IF;` every ~300 ms (matches the
   reference sketch).
2. **Band decode → outputs.** Parse the 11‑digit frequency out of the `IF`
   reply, map it to one of the six contest bands or to `BAND_BYPASS`, and
   drive the 6 band‑select GPIOs in a 1‑of‑6 / all‑off pattern.
3. **Config + web UI.** Serve a tiny HTTP UI (root form + `/save` POST +
   `/status` JSON + `/reboot`) so the user can change SSID / radio endpoint /
   hostname without re‑flashing.

A watchdog timer is fed by `loop()`; any blocking call longer than ~2 s is
considered a bug.

## Module layout

Each sketch folder (`5B4AGN/`, `Hamation/`) contains:

| File | Responsibility |
| --- | --- |
| `<Name>.ino` | `setup()` / `loop()` — wires the modules together. Nothing else. |
| `Config.h` | `struct Config`, defaults, magic+CRC EEPROM read/write, factory reset. |
| `BandPlan.h` | Frequency → `Band` enum; `Band` → 6‑bit relay pattern; pin map. |
| `RadioInterface.h` | TCP client. Connect/reconnect, send `IF;`, parse reply, surface `long frequencyHz()`. |
| `WebPortal.h` | `ESP8266WebServer` routes for `/`, `/save`, `/status`, `/reboot`, `/factory_reset`. |

Header‑only modules are deliberate — each sketch ships as a self‑contained
folder so a user can open it in the Arduino IDE and hit Upload. No external
non‑library files to track.

### Why two sketches instead of one shared library?

- Arduino IDE expects one `.ino` per sketch folder; libraries are installed
  separately and create friction for first‑time users (this firmware is
  expected to be flashed by hams who have used the Arduino IDE before but are
  not C++ developers).
- The two filters differ in trivial ways at the firmware level (defaults,
  comments). Duplicating ~50 lines of glue is cheaper than maintaining a
  shared library + two thin wrappers.

If a third filter is added later and the duplication starts to bite, extract
`BandPlan` and `RadioInterface` into a library under `libraries/BPFCore/` and
have both sketches `#include <BPFCore.h>`.

## Threading / concurrency

ESP8266 Arduino is single‑threaded with the WiFi/TCP stack running in the
background SDK task. `loop()` must yield (or stay short) so the SDK can run.
The polling cadence is `client.print("IF;")` + ≤ 250 ms wait for the response
+ web server handle. We never `delay()` for more than 50 ms in one go; longer
waits are converted to a `millis()` timer.

## State machine

```
        ┌──────────────┐
        │   BOOT       │
        └──────┬───────┘
               │ EEPROM valid?
       no ─────┤            ├── yes
               ▼            ▼
       ┌──────────────┐ ┌──────────────┐
       │ AP / PORTAL  │ │ STA + CONNECT│
       └──────┬───────┘ └──────┬───────┘
              │ save+reboot    │ TCP up?
              └────────────────┤
                               ▼
                       ┌──────────────┐
                       │  POLLING     │  ←─ on disconnect → outputs LOW,
                       └──────────────┘     reconnect with backoff
```

`POLLING` is the steady state. On any of `WiFi.disconnected`, `!client.connected()`,
or stale `IF;` reply (no fresh frequency in > 5 s), the firmware forces all
outputs LOW (= bypass) and attempts to reconnect with a bounded exponential
backoff (1 s → 2 s → 4 s → 8 s, capped).

## Safety properties

- **Bypass on uncertainty.** Both filters interpret all‑outputs‑LOW as bypass.
  The firmware aggressively returns to that state on disconnect, invalid
  frequency, or any unexpected condition. RF keeps flowing; no filter section
  is hot‑switched into an inappropriate band.
- **Boot‑mode safe pins.** Pin assignment matches the reference 8‑relay ESP‑12
  carrier, which has the required pull resistors on `GPIO0`/`GPIO15` for boot.
  All outputs are explicitly driven LOW in `setup()` before WiFi starts.
- **No PTT control here.** This firmware does not key the radio and does not
  inhibit PTT. Anyone retrofitting a PTT‑inhibit interlock should use one of
  the spare GPIOs (`GPIO14` / `GPIO16`) and ensure the relay is driven open
  on boot.

# Architecture

## Runtime overview

```
   Radio 1 ──TCI WS──┐
                     ├──► ESP32 ──► 4-bit BCD bank 1 ──► BPF 1
   Radio 2 ──TCI WS──┘                4-bit BCD bank 2 ──► BPF 2
                     (or same server for both, in SHARED mode)
                                │
                                ├──► 16x2 I2C LCD
                                └──► HTTP portal (config + manual bypass)
```

One ESP32 dev board on an active‑LOW 8‑relay carrier drives two
contest band‑pass filters. Each filter consumes 4 relays (one Yaesu
4‑bit BCD bus). No inhibit line — bypass is signalled by a WARC band
code on the BCD bus (see [Bypass policy](#bypass-policy) below).

Two TCI server modes, chosen automatically at startup:

| Mode | Config trigger | Behaviour |
| --- | --- | --- |
| **DUAL** | Radio 1 and Radio 2 have different `host:port` pairs | Two TCI clients opened; each filter follows its own radio's RX‑1 VFO A. |
| **SHARED** | Radio 1 and Radio 2 share the same `host:port` | One TCI client; `senderRig=0` events drive BPF 1, `senderRig=1` events drive BPF 2. Right wiring when a dual‑receiver radio (e.g. SunSDR2 PRO) feeds both filters. |

The detection is case‑insensitive host string match + numeric port
match, computed in `isSharedTciConfig()` at boot.

## Module layout (`ESP32_SO2R_TCI/`)

| File | Responsibility |
| --- | --- |
| `ESP32_SO2R_TCI.ino` | `setup()` / `loop()`, globals, all TCI event handlers (dual + shared), `applyBand()` + bypass policy, WiFi mode bring‑up, failsafe + LCD link‑status polling. |
| `Config.h` | `struct Config` (two radio endpoints), magic/version/CRC32 EEPROM read/write, factory reset. Schema version `2`. |
| `BcdBandPlan.h` | `bcdFor(hz)` → Yaesu Table 5 code; `nearestWarcBypass(hz)` → safe bypass code; `BcdBank` GPIO struct + `driveBank()` (active‑LOW). |
| `WebPortal.h` | `WebServer` routes `/`, `/save`, `/status`, `/bypass`, `/reboot`, `/factory_reset`. Callbacks back to the .ino for status JSON and manual bypass. |
| `LcdDisplay.h` | 16×2 I²C LCD wrapper. FreeRTOS task pinned to core 1, mutex‑protected cached state, delta‑only column writes at 150 ms / 400 kHz I²C. |
| `TCI.h` / `TCI.cpp` | Bundled IW7DMH TCI WebSocket client. Locally patched (quoted `RTX.h` include; `Unhandled message!` log gated; tune‑substring diagnostic tap). |
| `RTX.h` / `RTX.cpp` | TCI library's per‑rig state container (VFO/mode/TRX/tune/etc.). Two instances per TCI client. |
| `README.md` | Sketch‑level reference: pins, deps, web routes, bypass policy. |

Everything except `TCI.cpp` / `RTX.cpp` is header‑only — so opening
the sketch folder in the Arduino IDE and hitting Upload Just Works
without library hunting.

## Concurrency model

ESP32 Arduino runs on FreeRTOS. Tasks live on two cores:

| Core | Task | Owners |
| --- | --- | --- |
| 0 (PRO_CPU) | Arduino `loop()`, WiFi, lwIP, TCI WebSocket reader/event tasks (created by the TCI library), HTTP server | Everything network‑facing |
| 1 (APP_CPU) | `BpfLcd` FreeRTOS task | LCD redraw only |

The LCD task reads cached state behind a mutex and pushes only the
columns that changed; the WebSocket‑event task on core 0 calls
`g_lcd.setFreq()` / `setMode()` / `setTx()` / `setTune()` /
`setLink()`, which take the same mutex and update cached state. No
LCD I/O happens from event handlers — that would block the WebSocket
task on the I²C bus.

`loop()` itself is short: WebServer tick + DNS tick (AP mode only)
+ 250 ms failsafe check + 10 ms yield. The WiFi/TCI stacks run as
SDK tasks underneath; nothing in `loop()` waits on them.

## Event flow — a TCI VFO update

1. Server sends `vfo:0,0,14250000;` over the WebSocket.
2. TCI library's `ws_reader_task` (core 0) enqueues the frame.
3. TCI library's `ws_event_task` (core 0) dispatches it: parses,
   updates `rtx[0]`, calls `do_vfo_event(0, 0)`.
4. `onRadio1Vfo(0, 0)` (or `onSharedVfo(0, 0)` in shared mode)
   filters: only `senderRig=0`, `senderVfo=0` (main RX, VFO A) is
   acted on.
5. `applyBand(0, 14250000)` runs:
   - Decoded band = 20 m (code 5).
   - Tune flag clear, inhibit false → uses the decoded code.
   - Only writes the BCD bank if the code changed.
   - Logs to serial: `[R1] 20m @ 14250000 Hz (bcd=5 inh=0 tune=0)`.
   - Pushes new freq to the LCD via `g_lcd.setFreq(0, hz)`.

The single‑band‑change guard avoids burning I²C / GPIO writes on
sub‑band frequency ticks.

## Bypass policy

`applyBand()` forces the bank to bypass whenever any of these hold:

- `g_tune1` (or `g_tune2`) is true — set by TCI tune event handler
  OR by manual bypass (web `POST /bypass`, serial `bypassN on`).
- The decoded band is 60 m / out‑of‑band (inhibit flag from
  `bcdFor()`).
- TCI link or WiFi has dropped (caught in `loop()` 250 ms failsafe
  check, which calls `applyBand(idx, 0)` to nudge the policy).

Bypass never emits BCD `0000` — instead it emits the "nearest WARC"
code from the cached last frequency:

| Last freq | Bypass code |
| --- | --- |
| < 14 MHz | 30 m (4) |
| 14–21 MHz | 17 m (6) |
| > 21 MHz | 12 m (8) |
| Unknown | 30 m (4) |

Reason: the Hamation BandPasser II decodes `0000` as **160 m**, not
bypass — emitting `0000` during ATU tune would hot‑switch the 160 m
filter section. A WARC code (no matching contest section) triggers
Hamation's built‑in bypass relay; the 5B4AGN behaves identically (no
matching internal section ⇒ all section relays released). Single
policy works on both filters.

## State machine

```
        ┌──────────────┐
        │   BOOT       │ Drive both BCD banks IDLE (HIGH).
        │              │ LCD splash "BPF SO2R / TCI / starting..."
        └──────┬───────┘
               │ EEPROM valid?
       no ─────┤            ├── yes
               ▼            ▼
       ┌──────────────┐ ┌──────────────┐
       │ AP / PORTAL  │ │ STA + CONNECT│ ARDUINO_EVENT_WIFI_STA_START
       │ DNS hijack=0 │ │ + setHostname│   re-applies hostname so DHCP
       └──────┬───────┘ └──────┬───────┘   advertises SO2R-BPF.
              │                │ wifi up?
              │                ▼
              │        ┌──────────────┐
              │        │ DETECT MODE  │ shared / dual TCI?
              │        └──────┬───────┘
              │               ▼
              │        ┌──────────────┐
              │        │   POLLING    │ ─── TCI events → applyBand
              │        └──────────────┘     250 ms failsafe → bypass
              │                              on WiFi / TCI drop
              ▼
       portal handles /save → reboot
```

`POLLING` is the steady state. The 250 ms failsafe is purely
defensive — the TCI library auto‑reconnects on WebSocket drop with
its own backoff (handled by the library's `ws_reader_task`).

## Safety properties

- **Bypass on uncertainty.** Tune, WiFi drop, TCI drop, OOB, 60 m,
  parse failure → bank is forced to bypass via WARC code. Filter
  sees either its built‑in bypass (Hamation) or all‑sections‑released
  (5B4AGN). RF keeps flowing through the filter shell.
- **Boot‑safe pins.** ESP32 GPIOs used for the BCD banks (16/17/18/19
  and 33/32/27/26) are clean at reset — no strapping conflicts. All
  outputs are explicitly driven HIGH (idle / relay off) in `setup()`
  before WiFi starts.
- **LCD non‑blocking.** Redraw runs on core 1; if the I²C bus stalls,
  the WebSocket task on core 0 keeps consuming TCI events. The LCD
  shows stale state at worst, never blocks band switching.
- **Hostname survives DHCP.** Set in `WiFi.setHostname()` once, then
  re‑applied from an `ARDUINO_EVENT_WIFI_STA_START` event handler so
  it reaches lwIP before DHCP DISCOVER. Without this, routers see
  the default `esp32-<mac>` because of a race in the ESP32 Arduino
  core.
- **No PTT interlock.** The firmware does not key the radio or
  inhibit PTT. Anyone wiring an interlock should use one of the
  free GPIOs and ensure the relay defaults to OPEN at boot.

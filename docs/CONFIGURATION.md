# Configuration

The ESP32 stores its operational config in EEPROM (emulated in flash).
All configuration is editable from the on‑device web portal — no
recompile is needed to point the controller at different radios or
rename the host.

## EEPROM layout (schema v2)

A single packed struct with a magic prefix and CRC32 suffix:

```
offset  size  field
0       4     magic     = 0xBF50C0DE   ("BPF config")
4       2     version   = 2            (two radio endpoints)
6       32    wifi_ssid                (null-terminated)
38      64    wifi_pass                (null-terminated)
102     32    hostname                 (default "SO2R-BPF")
134     48    radio1_host
182     2     radio1_port
184     1     radio1_iaru              (1 | 2 | 3)
185     48    radio2_host
233     2     radio2_port
235     1     radio2_iaru
236     4     crc32                    (over bytes 0..235)
```

Total: 240 bytes used out of a 384‑byte allocation
(`EEPROM.begin(384)`).

If the magic is missing, the version is unknown, or the CRC fails,
the firmware loads defaults and starts in AP / portal mode.

Schema version `2` is distinct from version `1` of the removed
ESP8266 single‑radio sketches — flashing this firmware onto an
ESP32 that previously held some other config will fail validation
and fall back to defaults, which is the safe behaviour.

## Web portal routes

Served by the on‑device `WebServer` on port 80.

| Route | Method | Auth | Purpose |
| --- | --- | --- | --- |
| `/` | GET | admin | HTML form, prefilled with current config + manual‑bypass buttons + backup/restore. |
| `/save` | POST | admin | URL‑encoded form. Updates the struct, writes EEPROM, returns "Saved". Optional `admin_pass` sets/replaces the admin password. |
| `/status` | GET | – | JSON: filter / mode / wifi / R1+R2 connected, last band, tuning, sensors, history, uptime. |
| `/config` | GET | – | JSON: stored config (Wi-Fi password omitted) with `Content-Disposition` for download. |
| `/discover` | GET | – | JSON: device identity (vendor, product, version, build, hostname, IP, `auth_required`, ports, endpoint paths). Companion to the `_bpf-so2r._tcp` mDNS service — see [Discovery](#discovery) below. |
| `/history` | POST | admin | `clear=YES` → wipe the band-change ring buffer in RAM. |
| `/live` | GET | – | Standalone WebSocket-driven live status page (subscribes to port 81). |
| `/bypass` | POST | admin | `bpf=1\|2&on=0\|1`. Latches manual bypass on the named BPF (same effect as a TCI tune event). |
| `/reboot` | POST | admin | Soft reboot via `ESP.restart()`. |
| `/factory_reset` | POST | admin | Zeroes EEPROM and the admin password (`confirm=YES` required); next boot lands in portal. |

### Admin password

Since v0.6.0 one admin password (user `admin`) protects the portal
page, every route marked *admin* above, and ArduinoOTA uploads.

- **Set / change:** type it into the *Admin password* field and Save
  (max 64 characters; blank = keep the current one). It takes effect
  immediately for both the portal and OTA.
- **Scheme:** HTTP **Digest** (realm `BandPassFilterController`), so
  the password isn't sent in clear over plain HTTP. Basic credentials
  are refused (v0.6.1+), even when the password is right. Browsers prompt
  for it; `curl --digest -u admin:<password> ...`; the companion app
  stores it in the macOS Keychain.
- **Storage:** NVS namespace `bpf-auth` — separate from the EEPROM
  config page, so the schema stays at v2 and an OTA upgrade from
  v0.5.x keeps the saved settings. Never rendered back into the form
  or included in `/config`.
- **No password set** (fresh flash, factory reset, `auth clear`):
  every route and OTA are open, as before v0.6.0, and the portal shows
  a red warning banner. `GET /discover` reports `auth_required`.
- **Lost it:** type `auth clear` on the USB serial console.
- Read-only routes (`/status`, `/config`, `/discover`, `/live`) and
  the port‑81 WebSocket stay open so discovery and status views keep
  working without credentials.

### Form layout

The root page has five fieldsets:

1. **WiFi** — SSID, password, hostname (mDNS name).
2. **Radio 1 (drives BPF 1)** — host, port, IARU region.
3. **Radio 2 (drives BPF 2)** — host, port, IARU region.
4. **Admin password** — write-only; blank keeps the current one.
5. **Manual bypass** — four buttons (`BPF 1 ON / OFF`, `BPF 2 ON / OFF`).

A tip below Radio 2 reminds the user that entering the same host:port
in both radio fieldsets is the supported way to feed both filters
from a single dual‑receiver TCI server (shared mode).

### Status JSON

`GET /status` returns something like:

```json
{
  "filter": "ESP32 SO2R / TCI",
  "mode": "shared",
  "ap_mode": false,
  "wifi": "up",
  "ip": "192.168.86.55",
  "rssi": -56,
  "r1": { "connected": true,  "freq_hz": 14250000, "band": "20m",    "tuning": false },
  "r2": { "connected": true,  "freq_hz": 7080000,  "band": "40m",    "tuning": true  },
  "uptime_s": 1843
}
```

`mode` is `"shared"` if Radio 1 and Radio 2 resolve to the same TCI
server, `"dual"` otherwise. `tuning: true` means the firmware is
holding that BPF in bypass — either because the TCI server emitted
`tune:RIG,true;` or because the manual bypass path was triggered.

## Reaching the portal

| State | Address |
| --- | --- |
| STA mode (steady state) | `http://<hostname>.local/` via mDNS, e.g. `http://SO2R-BPF.local/`. Or the DHCP‑assigned IP printed on serial at 115200. |
| AP / portal mode | Open AP named `BPF-Setup-XXXXXX` (lower 24 bits of the eFuse MAC). Portal at `http://192.168.4.1/`. |

## Discovery

Two complementary mechanisms for letting a client app / shack PC
find every BPF controller on the LAN automatically:

### mDNS service (preferred)

Once on station Wi-Fi the device advertises a custom service
`_bpf-so2r._tcp.local.` on port 80, alongside the standard
`_http._tcp` registration. Any zeroconf-aware client can enumerate
controllers without hard-coding hostnames.

TXT records (identification only — for *state*, fetch `/discover` or
`/status`, which are cheap and live):

| Key | Example | Notes |
| --- | --- | --- |
| `vendor`  | `VU3ESV`                    | Project owner |
| `product` | `BandPassFilterController`  | Project name |
| `version` | `0.5.0`                     | Firmware version |
| `build`   | `Jan  1 2026 12:34:56`      | Build timestamp |
| `host`    | `SO2R-BPF`                  | Configured hostname |
| `bpf`     | `2`                         | Number of BPFs |
| `path`    | `/discover`                 | HTTP path to fetch metadata |
| `ws_live` | `81`                        | Live WebSocket port |

Browse from the shell:

```bash
# macOS
dns-sd -B _bpf-so2r._tcp local.
dns-sd -L SO2R-BPF _bpf-so2r._tcp local.   # detail one entry
# Linux
avahi-browse -r _bpf-so2r._tcp
```

From code: Apple Bonjour, Java JmDNS, Python `zeroconf`, Go `mdns`
all browse the same service type the same way.

### `GET /discover` (HTTP)

After mDNS has found a candidate — or any time the client already
has the IP (manual entry, DHCP-leases page, etc.) — `GET /discover`
returns a small, static, machine-readable identity record:

```json
{
  "service": "bpf-so2r",
  "vendor":  "VU3ESV",
  "product": "BandPassFilterController",
  "version": "0.6.0",
  "build":   "Jan  1 2026 12:34:56",
  "hostname": "SO2R-BPF",
  "ip":       "192.168.86.39",
  "bpf_count": 2,
  "auth_required": true,
  "ports":     { "http": 80, "ws_live": 81, "ota": 3232 },
  "endpoints": {
    "portal": "/", "status": "/status", "config": "/config",
    "discover": "/discover", "live": "/live", "bypass": "/bypass",
    "history_clear": "/history?clear=YES"
  }
}
```

No live state — for that the client calls `/status` (or opens the
WebSocket on the advertised `ws_live` port). Keeping `/discover`
static and cheap means a discovery sweep across many devices is
fast and cacheable.

The ESP32 re‑applies `WiFi.setHostname()` inside an
`ARDUINO_EVENT_WIFI_STA_START` event handler so DHCP DISCOVER carries
the configured hostname instead of the default `esp32-<mac>` — this
is the most reliable way around a known race in the Arduino ESP32
core. After a portal save the router should pick up `SO2R-BPF` on
the next lease.

## Swapping radios / changing mode

To repoint a filter at a different radio:

1. Browse to `http://SO2R-BPF.local/`.
2. Update host / port for Radio 1 or Radio 2 (and IARU region if
   needed).
3. Click **Save** → click **Reboot now**.

To switch from dual to shared mode (single dual‑receiver radio feeds
both BPFs):

1. Set Radio 1 and Radio 2 fields to the **same** host:port.
2. Save → reboot.
3. Verify on serial: `[tci] shared server mode -> <host>:<port> (BPF1=rig0, BPF2=rig1)`.

No firmware change. No recompile.

## Manual bypass

For radios whose TCI server doesn't forward tune state (AetherSDR,
partial TCI implementations) the firmware accepts an external
"act like the radio is tuning" signal. Three equivalent paths:

| Method | Trigger |
| --- | --- |
| Web | Click `BPF 1 bypass ON` (or `BPF 2 ...`) button on the root page. |
| HTTP | `curl -X POST 'http://SO2R-BPF.local/bypass?bpf=1&on=1'` |
| Serial (115200) | `bypass1 on`, `bypass1 off`, `bypass2 on`, `bypass2 off` |

All three call the same internal handler, latch the same flag a real
TCI tune event would set, and surface as `TU` in the LCD's state
column. Press `OFF` (or POST `on=0`, or serial `bypassN off`) when
the ATU finishes.

## Serial console

At 115200 baud the firmware accepts a small command set:

| Command | Effect |
| --- | --- |
| `status` | Prints the same JSON as `GET /status`. |
| `reset` | Zero EEPROM, clear the admin password and restart. |
| `auth clear` | Clear only the admin password and restart (lost-password recovery). |
| `bypass1 on` / `bypass1 off` | Manual bypass on BPF 1. |
| `bypass2 on` / `bypass2 off` | Manual bypass on BPF 2. |

Unrecognised input is ignored.

## Factory reset

Three options:

- **Web**: `POST /factory_reset` with `confirm=YES` (needs the admin
  password once one is set). The root form's "Factory reset" button
  submits this with a JS confirm dialog.
- **Serial**: type `reset` on the 115200 console.
- **Power off + wait + power on**: not a reset; just a reboot. Use
  one of the above for an actual EEPROM wipe.

After a reset the device boots into AP / portal mode with the
defaults below.

## Defaults

```
hostname        = "SO2R-BPF"
radio1_host     = "192.168.1.20"
radio1_port     = 50001            (ExpertSDR3 / SunSDR TCI default)
radio1_iaru     = 1                (Europe / Africa band edges)
radio2_host     = "192.168.1.21"
radio2_port     = 50001
radio2_iaru     = 1
```

Adjust them in `defaults()` inside [`ESP32_SO2R_TCI/Config.h`](../ESP32_SO2R_TCI/Config.h)
if you'd rather not use the portal on first boot.

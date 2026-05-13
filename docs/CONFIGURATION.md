# Configuration

The ESP‑12 stores its operational config in EEPROM (emulated in flash). All
configuration is editable from the on‑device web portal — no recompile is
needed to point a controller at a different radio.

## EEPROM layout

A single packed struct, prefixed with a magic word and suffixed with a CRC32:

```
offset  size  field
0       4     magic   = 0xBF50C0DE   ("BPF config")
4       2     version = 1
6       32    wifi_ssid               (null-terminated)
38      64    wifi_pass               (null-terminated)
102     32    hostname                (null-terminated, e.g. "bpf-5b4agn")
134     48    radio_host              (IP or DNS, null-terminated)
182     2     radio_port              (uint16, big-endian)
184     1     radio_protocol          (0 = Kenwood IF;/CAT TCP, 1 = TCI WS, reserved)
185     2     poll_interval_ms        (uint16, default 300)
187     4     crc32                   (over bytes 0..186)
```

Total: 191 bytes (rounded to 256 for `EEPROM.begin(256)`).

If the magic is missing, the version is unknown, or the CRC fails, the
firmware loads defaults and starts in AP / portal mode.

## Web portal routes

Served by the on‑device `ESP8266WebServer` on port 80.

| Route | Method | Purpose |
| --- | --- | --- |
| `/` | GET | HTML form, prefilled with current config. |
| `/save` | POST | URL‑encoded form. Validates, writes EEPROM, returns 200 + "Saved, reboot". |
| `/status` | GET | JSON: `{wifi, ip, rssi, radio_connected, last_freq_hz, last_band, uptime_s}`. |
| `/reboot` | POST | Soft reboot via `ESP.restart()`. |
| `/factory_reset` | POST | Zeroes EEPROM magic; next boot lands in portal. Confirmation required. |

In **STA mode** the portal is reachable at `http://<hostname>.local` (mDNS)
or the DHCP‑assigned IP, which is printed to serial at 115200.

In **AP / portal mode** the ESP starts an open AP named `BPF-Setup-<chipid>`
(e.g. `BPF-Setup-A1B2C3`) and serves the same routes at `http://192.168.4.1/`.
A captive‑portal DNS hijack is **not** implemented — connect to the AP and
browse to `192.168.4.1` manually.

## Swapping radios

To repoint a filter at a different radio:

1. Browse to `http://<hostname>.local/`.
2. Update **Radio host** and/or **Radio port** (and protocol, if different).
3. Click **Save**. The ESP writes EEPROM and prompts for reboot.
4. After reboot, the controller polls the new endpoint and starts tracking
   frequency immediately.

No firmware change. Both filters (5B4AGN and Hamation) use the same config
schema, so a 5B4AGN controller built for radio A can be moved to watch radio
B simply by changing the host field.

## Factory reset

Three options:

- **Web:** `POST /factory_reset` with `?confirm=YES` (the form on `/` has a
  button that submits this).
- **Serial:** type `reset` on the 115200 serial console (handled by the main
  loop's serial command parser).
- **Jumper (optional):** see `docs/HARDWARE.md` for wiring a hold‑to‑reset
  button on GPIO16.

After a reset the device boots into AP / portal mode with defaults.

## Defaults

```
hostname       = "bpf-5b4agn"      (5B4AGN sketch)
                 "bpf-hamation"    (Hamation sketch)
radio_host     = "192.168.1.10"
radio_port     = 13013             (Thetis default; Node-Red flow uses 7355)
radio_protocol = 0                 (Kenwood IF;/CAT over TCP)
poll_interval  = 300 ms
```

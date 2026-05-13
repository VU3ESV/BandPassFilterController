# BandPassFilterController

ESP8266 / ESP‑12 firmware that automatically selects the right contest band on
an HF band‑pass filter by polling an attached radio's frequency over the
network.

Two filter models are supported, one firmware build per device:

| Filter | Bands | Control input | Sketch |
| --- | --- | --- | --- |
| **5B4AGN TXBPF** ([group](https://groups.io/g/TXBPF)) | 160 / 80 / 40 / 20 / 15 / 10 m | Internal band‑select relays | [`5B4AGN/`](5B4AGN/) |
| **Hamation MBF‑100 BandPasser II** ([product](https://www.hamation.com/Bandpasser.html)) | 160 / 80 / 40 / 20 / 15 / 10 m | +5–12 V per‑band on rear control connector | [`Hamation/`](Hamation/) |

## Features

- Reads frequency from a TCP endpoint speaking Kenwood `IF;` CAT (Thetis,
  hamlib `rigctld`, Node‑Red bridge, TS‑2000 emulators, etc.).
- Engages the matching contest band; drops to **bypass** on WARC / 60 m / 6 m /
  out‑of‑band / disconnect / parse failure.
- Each ESP12 talks to one radio; swapping radios is a config change (no
  recompile).
- On‑device web portal for WiFi credentials, radio host/port, protocol, and
  hostname; config persisted to EEPROM.
- mDNS (`bpf-5b4agn.local` / `bpf-hamation.local`) for easy access.
- Captive portal on first boot.

## Quick start

1. Wire your ESP‑12 relay board / control buffer per
   [docs/HARDWARE.md](docs/HARDWARE.md).
2. Open `5B4AGN/5B4AGN.ino` or `Hamation/Hamation.ino` in the Arduino IDE
   (ESP8266 core 3.1+, *Generic ESP8266 Module*).
3. Flash. First boot brings up `BPF-Setup-XXXX` AP at `192.168.4.1`. Configure
   and reboot.

See [`CLAUDE.md`](CLAUDE.md) for the full project rationale and
[`docs/`](docs/) for the architecture, hardware, configuration, and radio
protocol notes.

## Status

Pre‑release. Tested against the reference 8‑relay ESP‑12 carrier and a Thetis
TCP `IF;` server. Not yet validated end‑to‑end on physical 5B4AGN or Hamation
hardware — wiring details in `docs/HARDWARE.md` should be treated as a
starting point, not a guarantee.

## License

MIT. See [LICENSE](LICENSE).

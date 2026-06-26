# sensowash_serial — ESPHome external component

Controls a **Duravit SensoWash Starck F-Plus** (serial generation) shower-toilet over BLE from a cheap
ESP32-S3 board, exposing it to Home Assistant as native ESPHome entities. 

## Hardware

- Generic **ESP32-S3-DevKitC-1** (N8R2 or N16R8), USB-C
- No GPIO wiring required.

- I have used N16R8 so tweak the partition size if you use the 8MB variant.

## Install

### Option A — pull it into your own ESPHome config (recommended)

This repo is laid out as a remote ESPHome external component. Add it to your device YAML and
ESPHome fetches it at compile time — no manual file copying:

```yaml
external_components:
  - source: github://darrenweake/esphome-sensowash@v0.27
    components: [sensowash_serial, web_dashboard]
    refresh: never      # pin the tag for reproducible builds
```

Then wire up the platforms (`sensowash_serial:`, plus the `button`/`switch`/`select`/`sensor`/
`binary_sensor`/`text_sensor` blocks and your `ble_client`). See [`config.yaml`](config.yaml) for a
complete working example you can copy from.

> Drop the `web_dashboard` entry from `components:` if you only want the Home Assistant entities
> and not the standalone tablet dashboard.

### Option B — clone and build the bundled config

Clone this repo and build the included [`config.yaml`](config.yaml) directly — it uses
`external_components: { source: { type: local, path: components } }`, so it compiles the component
straight from the checkout. Follow the step-by-step setup below.

## Layout

```
components/sensowash_serial/
├── __init__.py            # hub: schema, codegen, ble_client wiring, connect/disconnect triggers
├── sensowash_serial.{h,cpp}  # handshake state machine, frame build/parse, command queue, parsers
├── button/                # lid toggle, nozzle clean, holiday mode, descaling (diagnostic)
├── switch/                # auto lid open/close, feedback tones, energy saving
├── select/                # night light, dryer, seat temperature
├── sensor/                # descaling remaining (days)
├── binary_sensor/         # seat occupied (occupancy)
└── text_sensor/           # serial no., SW/HW version, descaling state, fault codes
```

## Setup — step by step

### 1. Install ESPHome and add your secrets

```bash
pip install esphome
```

Copy `secrets.yaml.example` → `secrets.yaml` (it's git-ignored — your real values never get
committed) and fill in:

| key | what to put |
|---|---|
| `wifi_ssid` / `wifi_password` | your **2.4 GHz** Wi-Fi |
| `toilet_mac` | your toilet's BLE MAC — find it with a phone BLE scanner (nRF Connect, LightBlue) |
| `ota_password` | any password you choose; needed for future wireless updates |
| `handshake_key` | your toilet's 8-hex-char key **if you know it**. If you don't, leave it for now and follow step 4. |

### 2. Flash the board (first time must be over USB)

Plug the ESP32-S3 into USB, then:

```bash
esphome run config.yaml --device COM3      # /dev/ttyUSB0 on Linux/Mac
```

`esphome run` flashes the board and then **streams its logs — keep that window open**, it's your
main feedback during pairing.

### 3. Open the dashboard

When the log shows the board has joined Wi-Fi, browse to **http://sensowash-bridge.local/**.
You'll see the control panel. The status bar at the bottom shows **"Bluetooth not linked"** — that's
expected until it pairs with the toilet.

### 4. Pair with the toilet

- **If you entered a `handshake_key` you already know:** there's nothing to do. Within a few seconds
  the status bar flips to **"Bluetooth linked"** and the Serial Number / Days-to-descale fields fill
  in. **Pairing done.**

- **If you DON'T know your key**, the toilet will only hand it over while it's in *pairing mode* —
  exactly like the official Duravit app. **There is no "pair" button on the dashboard**; you trigger
  it on the toilet itself:
  1. In `config.yaml`, **comment out** the `handshake_key: !secret handshake_key` line (so no key is
     set), and reflash. *(Don't set it to `00000000` — that's treated as a real key and skips pairing.)*
  2. With the dashboard open and showing **"Bluetooth not linked"**, **press the Bluetooth button on
     the toilet.**
  3. **Wait for the dashboard's status to change to "Bluetooth linked"** and the device info (Serial
     Number, Days to descale) to appear — **that's your success signal.** In the log window you'll
     also see `Learned handshake key XXXXXXXX` / `Persisted handshake key`.
  4. Copy that 8-character key into `secrets.yaml` as `handshake_key`, re-enable the `config.yaml`
     line, and reflash. This makes the pairing permanent (the auto-saved copy is lost if the flash is
     ever wiped).

### 5. Use it

Control everything from the dashboard, or add the board to Home Assistant — it appears automatically
over the ESPHome API. From now on you can update it wirelessly with `esphome run config.yaml`
(it uses the `ota_password` from your secrets).

> You can point the ESPHome dashboard / HA add-on at this folder as a local external component
> instead of using the CLI.

## Handshake key — how it works

Pairing (above) revolves around a 4-byte key (8 hex chars). The technical details:

- The key is **deterministic per toilet** and never changes once learned.
- **Key configured** (`handshake_key` in `secrets.yaml`): on connect the bridge writes it directly
  and is ready in well under a second.
- **Key absent** (line commented out): the bridge writes a `00000000` challenge and waits. The
  toilet only discloses its real key once you put it in pairing mode (its Bluetooth button); the
  firmware then logs `Learned handshake key …` and **persists it to the ESP's flash (NVS)**, so
  later boots skip the wait. Setting the key to `00000000` does **not** enrol — it's treated as a
  (wrong) real key.
- The NVS copy is wiped by a partition change or `esphome clean`, so keep the learned value in
  `secrets.yaml` for a permanent record.
- ⚠️ There is **no UI prompt** for any of this — watch the dashboard's Bluetooth status and/or the
  ESPHome logs (see Setup step 4).

## Entities

| Platform | Entity | Opcode / source |
|---|---|---|
| button | Lid Toggle | `0x09` |
| button | Nozzle Self-Clean / Manual Clean | `0x60` / `0x61` |
| button | Holiday Mode / Tank Drain (EN1717) | `0x62 [01]` / `0x62 [00]` |
| button | Start Descaling | `0x63` |
| switch | Auto Lid Open / Close | `0x43` / `0x44`, state from func config byte1 bit0/1 |
| switch | Feedback Tones | `0x40`, state from func config byte0 bit0 |
| switch | Energy Saving | `0x47`, optimistic tracking (see note) |
| select | Night Light (Off/On/Auto) | `0x41 [0/1/2]` |
| select | Dryer (Off/Low/Warm/Hot) | Off→`0x01`, else `0x04 [1/2/3]` |
| select | Seat Temperature (Off/Low/Warm/Hot) | `0x25 [0-3]` |
| sensor | Descaling Remaining (d) | `0x66` response (2-byte BE) |
| binary_sensor | Seat Occupied (occupancy) | toilet state `0x53` byte0 bit2 |
| text_sensor | Serial / SW ver / HW ver | `0x59` / `0x57` / `0x5B` |
| text_sensor | Descaling State / Fault Codes | `0x65` / `0x55` |

### Notes

- **Energy Saving** is tracked optimistically: writing it ON with no schedule entries makes the
  next `0x51` readback report OFF, so the firmware trusts the local state after sending the
  command (seeded once from byte3 on first connect).
- **Dryer** has no speed control in this protocol — only temperature + stop.
- **Hardware Version** is not always answered by this toilet's firmware; the entity stays unknown
  if no response arrives.
- All command writes are write-without-response, paced ~130 ms apart through an outbound queue.

## Protocol summary

Frame: `[0x55][0x05][len][opcode][payload...][checksum]`, `len = payload+3`,
`checksum = (len + opcode + Σpayload) & 0xFF`. Golden values: stop `55 05 03 01 04`,
eco flush `55 05 03 0C 0F`, full flush `55 05 03 0B 0E`.

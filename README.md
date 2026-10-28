# SmartPhset · ESP32-CAM

LAN video firmware for the **AI-Thinker ESP32-CAM with OV2640 and PSRAM**.
The board streams JPEG/MJPEG; Python runs YOLO and Next.js presents results.
The sensors/relays use the separate [smart-things](https://github.com/smart-phset/smart-things) repository.

```text
ESP32-CAM :81/stream → private Python session worker :8002 → authenticated Next proxy → browser
```

## Source and hardware

| File | Purpose |
|---|---|
| `web-cam.ino` | Primary Arduino sketch wrapper, matching directory name |
| `CameraWebServer.ino` | Camera, Wi-Fi and sensor initialization |
| `board_config.h` | Selects `CAMERA_MODEL_AI_THINKER`, flash brightness 48 |
| `camera_pins.h` | Board pin definitions |
| `app_httpd.cpp`, `camera_index.h` | HTTP handlers and embedded UI |
| `partitions.csv` | Existing custom partition layout |
| `config.example.h` | Safe Wi-Fi template; copy privately to ignored `config.h` |

Required: board and camera ribbon, stable board-rated supply, common ground,
data-capable cable and USB-to-serial programmer if the carrier lacks one. Use
**3.3 V UART logic**; confirm power-pin ratings on the actual carrier/adapter.
Use an adequate external supply when the programmer cannot power camera/flash
reliably. Do not attach fan/pump relays to the camera pins.

Verified definitions in the selected `CAMERA_MODEL_AI_THINKER` block:

| Signal | GPIO |
|---|---|
| PWDN / RESET | 32 / -1 (no reset pin) |
| XCLK | 0 |
| SCCB SDA / SCL | 26 / 27 |
| D0–D7 (`Y2`–`Y9`) | 5, 18, 19, 21, 36, 39, 34, 35 |
| VSYNC / HREF / PCLK | 25 / 23 / 22 |
| Flash LED | 4 |

These are firmware assignments, not physical wiring acceptance. Other pin blocks
in `camera_pins.h` are not the selected hardware. Runtime defaults are JPEG VGA
640×480, quality 10/two PSRAM buffers/latest-frame grab with PSRAM; fallback is
quality 12/one DRAM buffer. Brightness +1, contrast +1, saturation 0, automatic
white balance/exposure/gain. Flash PWM is 5 kHz, 8 bit, default 48/255.

## Toolchain and private configuration

Inspected tools: Arduino CLI 1.5.1, Espressif **esp32:esp32 3.3.12**.
Camera/Wi-Fi/HTTP libraries ship with that core. The `arduino:esp32` core is a
different package. Arduino IDE can use the same Espressif core, ESP32 Dev Module,
PSRAM enabled, DIO flash and the supplied custom partition table; CLI below is
the reproducible build profile.

```bash
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.12 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
# From web-cam/:
cp config.example.h config.h
```

Edit `WIFI_SSID` and `WIFI_PASSWORD` privately in `config.h`. Use a reachable
2.4 GHz network, without client isolation between worker and camera. Do not
commit credentials or compiled binaries: firmware embeds the credentials.
This firmware uses `config.h`, not dotenv; an `.env` file has no effect.

## Compile, upload and serial monitoring

Keep the sketch directory named `web-cam`; Arduino compiles both `.ino` files.

```bash
arduino-cli board list
arduino-cli compile \
  --fqbn esp32:esp32:esp32:PSRAM=enabled,PartitionScheme=custom,FlashMode=dio,UploadSpeed=115200 \
  --build-path /tmp/smartphset-camera-build .
```

Flashing is a separate manual action. With power disconnected, cross adapter TX
with board RX and adapter RX with board TX, and connect common ground. Confirm
labels/voltage on the actual hardware. Ground GPIO0, power/reset into download
mode and close serial monitors. Replace `/dev/ttyUSB1` with the discovered port:

```bash
arduino-cli upload \
  --fqbn esp32:esp32:esp32:PSRAM=enabled,PartitionScheme=custom,FlashMode=dio,UploadSpeed=115200 \
  --port /dev/ttyUSB1 --input-dir /tmp/smartphset-camera-build .
# Remove GPIO0-to-GND with power off, then power/reset for normal boot.
arduino-cli monitor --port /dev/ttyUSB1 --config baudrate=115200
```

Windows uses a discovered `COMx` port and a writable Windows build directory;
run the same CLI options on one line in PowerShell. Install the appropriate
USB-UART driver and ensure Linux serial-group permissions where applicable.

## Endpoints and integration

Serial output reports the DHCP address. On the private camera network:

| URL | Purpose |
|---|---|
| `http://CAMERA_LAN_IP/` | Embedded camera UI (port 80) |
| `http://CAMERA_LAN_IP/status` | Camera status |
| `http://CAMERA_LAN_IP/capture` | JPEG snapshot |
| `http://CAMERA_LAN_IP:81/stream` | MJPEG stream |
| `/control`, `/bmp`, `/xclk`, `/reg`, `/greg`, `/pll`, `/resolution` on port 80 | Implemented camera controls/debug handlers |

These handlers have no application authentication or TLS. **Never expose them
to the public Internet.** Test a snapshot from the worker's LAN host first:

```bash
curl --fail --max-time 10 http://CAMERA_LAN_IP/capture -o /tmp/smartphset-camera-check.jpg
```

Then configure the existing [Python worker](https://github.com/smart-phset/phset-ai):

```bash
export SMARTPHSET_CAMERA_SOURCES='{"esp32-cam":"http://CAMERA_LAN_IP:81/stream"}'
python session_service.py
```

Spring: `SMARTPHSET_CAMERA_REGISTRY=esp32-cam=Grow room`, plus private
`SMARTPHSET_CAMERA_API_KEY`. Next uses the same registry key server-side and a
separate matching Python `SMARTPHSET_AI_SERVICE_KEY`. IDs must match exactly;
Spring stores IDs/labels only, never camera URLs. Start Camera opens shared
capture; Start AI Detection enables inference. Multiple viewers share a single
worker, avoiding multiple raw-stream owners. Do not run legacy AI simultaneously
against the same camera. See the [frontend camera guide](https://github.com/smart-phset/phset-client/blob/feat/grow-room-monitoring/docs/dynamic-camera-inference.md).

## Verification and troubleshooting

Compile success verifies code/toolchain compatibility, not camera hardware.
Manually confirm boot logs, PSRAM YES, camera initialization, Wi-Fi address,
moving frames and camera ID in the frontend. There is no automated device test suite.
Run `git diff --check` before release.

- Init failure: check selected board, seated ribbon (power off first), power and PSRAM configuration.
- Brownouts/flash errors: stable supply, shorter cable, common ground; verify board ratings. Do not disable brownout protection to hide power faults.
- Upload timeout: correct port, GPIO0 grounded/reset, crossed UART and no monitor holding the port; remove jumper afterward.
- Wi-Fi unavailable: private credentials, 2.4 GHz, signal, router DHCP and no client isolation. Inspect serial rather than guessing the IP.
- Snapshot works but stream fails: use port 81 `/stream`; avoid competing viewers and confirm network/firewall access from Python's host.
- AI source unavailable/404: compare exact registry and worker IDs and current DHCP address. AWS loopback cannot reach a laptop worker or private camera LAN.

Keep `config.h` ignored. Do not publish firmware artifacts containing Wi-Fi secrets.

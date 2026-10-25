# SmartPhset ESP32-CAM

The raw live-video source for SmartPhset AI. This firmware runs on an AI-Thinker
ESP32-CAM; inference runs on the computer, not on the camera.

```text
ESP32-CAM
    ↓
http://<CAMERA_IP>:81/stream
    ↓
SmartPhset AI / OpenCV / YOLO
    ↓
annotated MJPEG (Python :8001/stream.mjpg)
    ↓
Next.js frontend
```

[AI repository](https://github.com/smart-phset/phset-ai/tree/feat/live-camera) ·
[Frontend](https://github.com/smart-phset/phset-client/tree/feat/grow-room-monitoring) ·
[Spring backend](https://github.com/smart-phset/phset-api) ·
[Regular ESP32 MQTT firmware](https://github.com/smart-phset/smart-things).
The regular ESP32 remains a separate device and repository.

## Hardware and preserved camera profile

- AI-Thinker ESP32-CAM, OV2640 camera, PSRAM, built-in flash LED on GPIO 4.
- JPEG VGA **640×480**. With PSRAM: JPEG quality **10**, **2 frame buffers**, PSRAM
  framebuffer storage and **CAMERA_GRAB_LATEST**.
- Without detected PSRAM: quality 12, one DRAM buffer, when-empty grab. This is a
  fallback; the intended board/build should report PSRAM YES.
- Brightness **+1**, contrast **+1**, saturation **0**, automatic white balance
  (AWB and AWB gain), automatic exposure (AEC) and gain (AGC) enabled.
- Camera UI, `/capture` JPEG snapshot and port **81** MJPEG stream retained.
- Flash PWM: **5 kHz**, **8-bit duty**, conservative SmartPhset default **48/255**.

`board_config.h` selects `CAMERA_MODEL_AI_THINKER`, includes `camera_pins.h` and
sets `SMARTPHSET_FLASH_BRIGHTNESS`. `app_httpd.cpp` implements HTTP/stream/flash;
`camera_index.h` contains the required embedded web UI. Keep `partitions.csv`:
the commands below select the existing custom partition layout.

## Requirements

Both platforms need Git, Arduino CLI, the Espressif ESP32 Arduino core, a
USB-TTL/serial programmer if your ESP32-CAM carrier has no built-in USB programmer,
a data-capable USB cable and a stable board power supply. Use **3.3 V UART logic**;
check your board/adapter power pin labels and connect a common ground.

The inspected toolchain is **Arduino CLI 1.5.1**, **esp32:esp32 core 3.3.12**.
Camera, Wi-Fi and HTTP libraries are bundled with that core; no extra Arduino
libraries are needed. The `arduino:esp32` core is a different package.

Linux: install Git and Arduino CLI on PATH; ensure your user has access to the
serial device through your distribution's serial group/udev configuration.
Windows: install Git for Windows, Arduino CLI on PATH and the USB-UART driver
appropriate to your programmer. Use native PowerShell; WSL is not required.
Reopen the terminal after PATH/driver installation.

Check `arduino-cli version`, `arduino-cli core list`, and
`arduino-cli board details --fqbn esp32:esp32:esp32`. The installed core supports
`PSRAM=enabled`, `PartitionScheme=custom`, `FlashMode=dio` and `UploadSpeed=115200`.

## Clone and install the core

The following commands work in Bash and PowerShell:

```text
git clone https://github.com/smart-phset/web-cam.git
cd web-cam
git switch main
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.12 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

Keep the clone directory named **web-cam**. Arduino requires a primary `.ino`
matching its directory name, so the small `web-cam.ino` entry file is included.
The actual implementation stays in **CameraWebServer.ino**; Arduino compiles both
files together. Without the entry file, compiling `.` in web-cam fails with
“main file missing from sketch.”

## Private Wi-Fi configuration

Copy the template **once**, then replace the two placeholders privately:

```cpp
#pragma once
#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
```

Use your reachable 2.4 GHz Wi-Fi network. `config.h` is private and **must never be
committed**. `config.example.h` is the committed safe template. The sketch includes
config.h and calls `WiFi.begin(WIFI_SSID, WIFI_PASSWORD)`; it does not print these
credentials to Serial. Do not overwrite an existing configured config.h.

Linux Bash:

```bash
cp config.example.h config.h
# Edit config.h with your preferred editor.
chmod 600 config.h
```

Windows PowerShell:

```powershell
Copy-Item config.example.h config.h
notepad config.h
```

## Compile and upload — Linux

Run from the web-cam clone. Build output goes to a writable temporary directory;
firmware binaries contain Wi-Fi settings and must also remain private.

```bash
arduino-cli board list
arduino-cli compile \
  --fqbn esp32:esp32:esp32:PSRAM=enabled,PartitionScheme=custom,FlashMode=dio,UploadSpeed=115200 \
  --build-path /tmp/smartphset-camera-build \
  .
```

Discover your actual port with `arduino-cli board list`; `/dev/ttyUSB1` is only an
example and can change. Enter flashing mode as described below, close any serial
monitor, then upload the compiled artifacts:

```bash
arduino-cli upload \
  --fqbn esp32:esp32:esp32:PSRAM=enabled,PartitionScheme=custom,FlashMode=dio,UploadSpeed=115200 \
  --port /dev/ttyUSB1 \
  --input-dir /tmp/smartphset-camera-build \
  .
```

After restoring normal boot mode:

```bash
arduino-cli monitor --port /dev/ttyUSB1 --config baudrate=115200
```

Recompile after source/config changes before uploading; upload does not compile.
Temporary build output may disappear after reboot. No upload is needed merely to
migrate the already-running firmware into this repository.

## Compile and upload — Windows PowerShell

Run from the web-cam clone. Use `.build` rather than a Linux `/tmp` path:

```powershell
arduino-cli board list
arduino-cli compile `
  --fqbn "esp32:esp32:esp32:PSRAM=enabled,PartitionScheme=custom,FlashMode=dio,UploadSpeed=115200" `
  --build-path .build `
  .
```

PowerShell backticks must be the last character on their lines (no trailing
spaces). Find the correct **COM port** from board list or Device Manager → Ports;
it may be COM3, COM4, COM5, etc. Do not assume a fixed port. An “unknown” board
label can still have a usable serial port with the explicit FQBN above.

Enter flashing mode and close the serial monitor before upload. Replace COM3
below with the discovered COMx:

```powershell
arduino-cli upload `
  --fqbn "esp32:esp32:esp32:PSRAM=enabled,PartitionScheme=custom,FlashMode=dio,UploadSpeed=115200" `
  --port COM3 `
  --input-dir .build `
  .
```

After restoring normal boot mode:

```powershell
arduino-cli monitor --port COM3 --config baudrate=115200
```

The stable upload speed is **115200** on both platforms. `.build` is ignored;
do not commit/share its credential-bearing firmware artifacts.

## Manual flashing mode

1. Disconnect power before changing wiring. Connect adapter TX → camera U0R/RX,
   adapter RX → camera U0T/TX, common ground and appropriate stable board power.
2. Connect **GPIO0 → GND**. Apply power and press **RST** (or power-cycle).
   This selects the serial bootloader, rather than the running camera firmware.
3. Close serial monitors and run the upload command with the correct port.
4. After successful upload, **disconnect GPIO0 from GND** (power off for wiring
   changes), restore power and press **RST again**. Firmware now boots normally.
   Leaving GPIO0 grounded prevents normal camera startup.
5. Open the serial monitor at **115200 baud**. Press RST if needed to see startup.

## Startup output and camera URLs

With the intended PSRAM board/profile, expect these lines among the diagnostics:

```text
SmartPhset camera profile: AI detection
Resolution: VGA 640x480
JPEG quality: 10
PSRAM: YES
Frame buffers: 2
Grab mode: latest
Flash LED GPIO: 4
Flash stream brightness: 48/255 (5 kHz, 8-bit)
WiFi connected
IP address: <ESP32_CAM_IP>
```

The firmware also prints framebuffer/sensor details and Web UI, Capture and
Stream URLs. Read the assigned IP from Serial; the router's DHCP/device list is
another option. Replace `<ESP32_CAM_IP>` below with that address:

| Endpoint | URL |
|---|---|
| Camera web UI (HTTP port 80) | `http://<ESP32_CAM_IP>/` |
| Raw MJPEG (port 81) | `http://<ESP32_CAM_IP>:81/stream` |
| JPEG snapshot | `http://<ESP32_CAM_IP>/capture` |

`/capture` is registered in app_httpd.cpp on the main HTTP server. The stream
server increments the default port 80 to 81. Test these in a browser before
starting AI. A moving raw stream verifies camera/network operation, not model quality.

## Networking

The ESP32-CAM and AI computer must be on a network where they can reach each
other. Guest Wi-Fi/client isolation may block traffic even on the same SSID.
The camera receives its own IP via DHCP; that address can change after reconnects.

Unlike the regular ESP32 MQTT firmware, this camera **does not need MQTT_HOST** or
the computer's MQTT IP. The computer connects to the **camera's** address:
`http://<ESP32_CAM_IP>:81/stream`. Do not substitute localhost or the laptop IP.
Keep the unauthenticated camera HTTP interface on a trusted local network.

## SmartPhset AI integration

Prepare the AI repository/venv using its README (Python 3.11, requirements.txt,
models/best.pt). Current live work is on `feat/live-camera`:

```text
git clone https://github.com/smart-phset/phset-ai.git
git -C phset-ai switch feat/live-camera
```

From a parent directory containing the AI clone, run the following. If your
existing clone is named SmartPhset-AI, use that directory instead of phset-ai.
Replace the camera IP and private ingestion key. Spring/PostgreSQL should already
be running for metadata publishing; the AI key must match Spring AI_INGEST_KEY.

Linux Bash:

```bash
cd phset-ai
export SMARTPHSET_AI_INGEST_KEY="YOUR_PRIVATE_AI_KEY"
.venv/bin/python live_camera.py \
  --source 'http://YOUR_ESP32_CAM_IP:81/stream' \
  --model-provider local \
  --device cpu \
  --conf 0.4 \
  --fps 5 \
  --publish-every 5 \
  --backend-url http://localhost:9090 \
  --serve \
  --host 127.0.0.1 \
  --port 8001
```

Windows PowerShell (direct venv executable; activation is not required):

```powershell
cd phset-ai
$env:SMARTPHSET_AI_INGEST_KEY = "YOUR_PRIVATE_AI_KEY"
.\.venv\Scripts\python.exe live_camera.py `
  --source 'http://YOUR_ESP32_CAM_IP:81/stream' `
  --model-provider local `
  --device cpu `
  --conf 0.4 `
  --fps 5 `
  --publish-every 5 `
  --backend-url http://localhost:9090 `
  --serve `
  --host 127.0.0.1 `
  --port 8001
```

No Qt setting is needed in serving mode. `QT_QPA_PLATFORM=xcb` is Linux/X11-specific;
**do not set it on Windows**.

Python turns the raw camera stream into annotated MJPEG. Verify
http://127.0.0.1:8001/health and http://127.0.0.1:8001/stream.mjpg. Next.js uses the
server-only `SMARTPHSET_AI_LIVE_BASE_URL=http://127.0.0.1:8001`; after signing in,
`/scan` → Live Detection displays the proxied annotated stream. The Python/Next.js
loopback addresses assume those services run on the same computer.

Separately, Python publishes detection JSON with camera ID **esp32-cam** to Spring,
which stores metadata in PostgreSQL. Set frontend SMARTPHSET_AI_CAMERA_ID=esp32-cam
for Overview/history. At migration time, `/api/ai/live/detection` still explicitly
selects webcam-0; that panel's metadata may not describe the ESP32 video. Verify
ESP32 results via Overview/history or
`http://localhost:9090/api/ai/detections/latest?camera=esp32-cam`.

## Flash, lighting and power

GPIO **4** uses **5 kHz, 8-bit PWM**. The default **48/255** is deliberately below
full brightness to limit heat/power draw. The LED stays off at boot/idle and lights
during streaming/capture. The web led_intensity control can override duty (0–255).
Flash updates are synchronized across HTTP tasks and use a separate timer/channel
from the camera clock. Preserve this behavior when editing app_httpd.cpp.

Weak USB-TTL power can cause brownout, reboot loops, camera initialization failure
and “Unable to verify flash chip connection.” Use a stable power supply and common
ground; check adapter logic/power levels. Higher flash duty can worsen instability.
External diffuse lighting often gives better AI input than increasing flash duty.

## Troubleshooting

1. **Failed to connect to ESP32:** GPIO0 → GND, press RST, retry upload. Confirm
   correct port and crossed TX/RX wiring. Remove GPIO0 jumper/reset after upload.
2. **Unable to verify flash chip connection:** use upload speed 115200; check
   supply, USB-TTL adapter and data cable; shorten wires; close the serial monitor.
3. **Serial port busy:** the Arduino Serial Monitor or another arduino-cli monitor
   is often still open. Stop it with Ctrl+C/close its window before retrying.
   Linux: `fuser -v /dev/ttyUSB1` identifies users of that device; replace the port.
   Windows: close Arduino IDE/terminal serial sessions, check Device Manager →
   Ports and unplug/reconnect the adapter if necessary. In PowerShell,
   `[System.IO.Ports.SerialPort]::GetPortNames()` lists ports but does not identify
   their owners. If ownership remains unclear, use Process Explorer's handle
   search for the serial device. Get-NetTCPConnection cannot diagnose COM ownership;
   do not kill unrelated processes based on TCP ports.
4. **Garbage serial output:** select 115200 baud, verify the correct COM/tty device
   and press RST. Check wiring and stable power if corruption continues.
5. **Dark stream:** verify streaming/capture activates flash and led_intensity is
   not zero; improve external lighting. Current tuning is brightness +1/contrast +1.
6. **Laptop camera AI works better:** compare lighting, focus and framing; keep
   VGA, avoid flash glare and confirm raw-stream stability. Camera optics and
   scene differences affect detections; a successful stream is not AI validation.
7. **PSRAM NO / camera init failure:** confirm AI_THINKER, PSRAM-enabled FQBN,
   camera ribbon seating and power. Keep the custom partitions.csv with the build.
8. **Stream unreachable:** read the current IP from Serial; use port 81 `/stream`,
   verify common reachable network and normal boot mode. It is not an MQTT issue.
9. **Sketch main file missing:** keep the clone folder named web-cam and retain
   web-cam.ino; the implementation remains CameraWebServer.ino.

## Validation and physical acceptance

The sanitized repository copy is compiled with the PSRAM/custom-partition/DIO
configuration above using the installed core. Run `git diff --check` for source
changes. Windows commands are provided for native PowerShell; this migration's
compile validation runs on Linux, not Windows.

The camera has already been physically tested as an ESP32-CAM video source.
The **newest lighting tuning still requires further physical acceptance**.
Repository migration does not require another physical upload; compilation does
not establish lighting effectiveness, power stability or AI detection quality.

## Git workflow and security

Never commit config.h, Wi-Fi credentials, API keys, build output or firmware
binaries. `.gitignore` covers private config, binaries, `.build`, build/cache and
editor directories. A compiled firmware can contain credentials even when source
configuration is ignored. Do not print or paste credentials into Serial/logs/docs.

Before staging, privately search candidate files for your actual SSID/password
and other secrets without printing their values. Verify:

```text
git status --ignored
git ls-files config.h
```

The status should show config.h ignored; ls-files must print nothing for it.
Review intended changes and staged content before committing:

```text
git switch main
git pull --ff-only
git switch -c feat/my-camera-change
# Edit, compile and review.
git diff --check
git status
git add CameraWebServer.ino README.md
# Add any other intended safe files explicitly.
git diff --cached
git commit -m "Describe the camera change"
git push -u origin feat/my-camera-change
```

Open a Pull Request. Never force-add config.h or use force push without explicit
team agreement. If a secret was committed, rotate it; deleting it in a later
commit does not revoke it.

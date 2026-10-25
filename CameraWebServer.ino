#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>

// ===========================
// Select camera model in board_config.h
// ===========================
#include "board_config.h"

// ===========================
// Private Wi-Fi settings; copy config.example.h to config.h
// ===========================
#include "config.h"

void startCameraServer();
void setupLedFlash();

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

#if defined(LED_GPIO_NUM)
  // Hold the flash off even if camera initialization fails.
  pinMode(LED_GPIO_NUM, OUTPUT);
  digitalWrite(LED_GPIO_NUM, LOW);
#endif

  const bool hasPSRAM = psramFound();
  camera_config_t config = {};

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;

  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;

  // =========================================================
  // Camera configuration for stable live streaming
  // =========================================================
  config.pixel_format = PIXFORMAT_JPEG;

  // Start at VGA instead of UXGA
  // VGA = 640 x 480
  config.frame_size = FRAMESIZE_VGA;

  config.jpeg_quality = 12;
  config.fb_count = 1;

  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_DRAM;

  // If PSRAM is available, use it for smoother streaming
  if (hasPSRAM) {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.fb_location = CAMERA_FB_IN_PSRAM;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // =========================================================
  // Camera initialization
  // =========================================================
  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {
    Serial.printf(
      "Camera init failed with error 0x%x\n",
      err
    );
    return;
  }

  // =========================================================
  // Camera sensor configuration
  // =========================================================
  sensor_t *s = esp_camera_sensor_get();
  if (!s) {
    Serial.println("Camera sensor unavailable");
    return;
  }

  // Initial sensors are flipped vertically
  // and colors can be oversaturated.
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }

  // IMPORTANT:
  // Keep runtime resolution at VGA.
  // The old code forced QVGA (320x240).
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(
      s,
      FRAMESIZE_VGA
    );
  }

  // Conservative AI image profile; the normal web controls remain available.
  int sensorResult = 0;
  sensorResult |= s->set_brightness(s, 1);
  sensorResult |= s->set_contrast(s, 1);
  sensorResult |= s->set_saturation(s, 0);
  sensorResult |= s->set_whitebal(s, 1);
  sensorResult |= s->set_awb_gain(s, 1);
  sensorResult |= s->set_wb_mode(s, 0);  // automatic white balance
  sensorResult |= s->set_exposure_ctrl(s, 1);
  sensorResult |= s->set_gain_ctrl(s, 1);
  if (sensorResult != 0) {
    Serial.println("Warning: some AI sensor settings were not applied");
  }

  Serial.println("SmartPhset camera profile: AI detection");
  Serial.println("Resolution: VGA 640x480");
  Serial.printf("JPEG quality: %d\n", s->status.quality);
  Serial.printf("PSRAM: %s\n", hasPSRAM ? "YES" : "NO");
  Serial.printf("Frame buffers: %u\n", (unsigned)config.fb_count);
  Serial.printf("Grab mode: %s\n", config.grab_mode == CAMERA_GRAB_LATEST ? "latest" : "when empty");
  Serial.printf("Framebuffer: %s\n", hasPSRAM ? "PSRAM" : "DRAM");
  Serial.printf("Sensor: brightness=%d contrast=%d saturation=%d AWB=%u AEC=%u AGC=%u\n",
                s->status.brightness, s->status.contrast, s->status.saturation,
                s->status.awb, s->status.aec, s->status.agc);

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

  // =========================================================
  // Flash LED
  // =========================================================
#if defined(LED_GPIO_NUM)
  setupLedFlash();
#endif

  // =========================================================
  // WiFi
  // =========================================================
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  WiFi.setSleep(false);

  Serial.print(
    "WiFi connecting"
  );

  while (
    WiFi.status() != WL_CONNECTED
  ) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println(
    "WiFi connected"
  );

  Serial.print(
    "IP address: "
  );

  Serial.println(
    WiFi.localIP()
  );

  // =========================================================
  // Start HTTP servers
  // =========================================================
  startCameraServer();

  Serial.println();
  Serial.println(
    "Camera server started"
  );

  Serial.print(
    "Web UI: http://"
  );

  Serial.println(
    WiFi.localIP()
  );

  Serial.print(
    "Capture: http://"
  );

  Serial.print(
    WiFi.localIP()
  );

  Serial.println(
    "/capture"
  );

  Serial.print(
    "Stream: http://"
  );

  Serial.print(
    WiFi.localIP()
  );

  Serial.println(
    ":81/stream"
  );
}

void loop() {
  // Everything is handled by the HTTP server tasks.
  delay(10000);
}
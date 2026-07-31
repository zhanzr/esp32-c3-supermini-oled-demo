# OV3660 Camera Frame Test (ESP32-S3 N8R8 OV3660 Board)

Uses the Espressif `esp32-camera` managed component to initialize the OV3660
over the CSI interface, capture a single RGB565 frame, and log its dimensions
plus a few sampled pixel values. It proves the full camera pipeline
(SCCB + XCLK + CSI data lines) works before building the streaming server.

## Hardware connection

Same pinout as the ESP32S3-EYE reference (`CAMERA_MODEL_ESP32S3_EYE` in
Arduino `camera_pins.h`, which the vendor's CameraWebServer sketch uses).

| Signal | GPIO | Signal | GPIO |
|--------|------|--------|------|
| SIOD   | 4    | XCLK   | 15   |
| SIOC   | 5    | CSI_PCLK | 13 |
| CSI_VSYNC | 6 | CSI_HREF | 7  |
| CSI_Y9 (D7) | 16 | CSI_Y5 (D3) | 10 |
| CSI_Y8 (D6) | 17 | CSI_Y4 (D2) | 8  |
| CSI_Y7 (D5) | 18 | CSI_Y3 (D1) | 9  |
| CSI_Y6 (D4) | 12 | CSI_Y2 (D0) | 11 |

There is **no PWDN and no RESET** wire to the sensor (`pin_pwdn = -1`,
`pin_reset = -1`); the driver performs a software reset over SCCB.

## Component & config

- `main/idf_component.yml`: `espressif/esp32-camera: "^2.1.7"` (declares
  `ESP-IDF >= 5.1`, builds cleanly against IDF v6.0.2).
- PSRAM is required for the frame buffer:
  `CONFIG_ESP32S3_SPIRAM_SUPPORT=y`, `CONFIG_SPIRAM=y`,
  `CONFIG_SPIRAM_MODE_OCT=y` (N8R8 = octal 8 MB PSRAM), `CONFIG_SPIRAM_SPEED_80M=y`.
- `xclk_freq_hz = 20 MHz` (works; the vendor firmware uses 15 MHz — either is fine).
- `PIXFORMAT_RGB565`, `FRAMESIZE_QVGA` (320x240), `fb_count = 1`,
  `fb_location = CAMERA_FB_IN_PSRAM`, `CAMERA_GRAB_WHEN_EMPTY`.

## Key driver facts

- Sensor detected via `SCCB_Read16(0x300A)<<8 | SCCB_Read16(0x300B)` == `0x3660`.
- The ov3660 driver only populates `id.PID`; `id.VER`/`id.MIDH`/`id.MIDL`
  stay 0 (our manual SCCB test read real `VER=0x60`/`MID=0x2200` from the
  registers, so those fields are just not filled in by the driver).

## Output

```
I (799) cam_hal: cam init ok
I (799) sccb-ng: pin_sda 4 pin_scl 5
I (819) camera: Camera PID=0x3660 VER=0x00 MIDL=0x00 MIDH=0x00
I (819) camera: Detected OV3660 camera
I (819) camera: Detected camera at address=0x3c
I (1129) cam_hal: Allocating 153600 Byte frame buffer in PSRAM
I (1169) camtest: Camera init OK
I (1169) camtest: Sensor: pid=0x3660 ver=0x0 mid=0x0000 slv=0x3C
I (1529) camtest: Captured frame: 320x240 format=0 len=153600
I (1529) camtest: center   (160,120): RGB565=0xB2A6 R=22 G=21 B=6
I (1529) camtest: top-left (0,0): RGB565=0x4E75 R=9 G=51 B=21
I (1529) camtest: top-right (319,0): RGB565=0x484B R=9 G=2 B=11
I (1529) camtest: bot-left (0,239): RGB565=0xD09E R=26 G=4 B=30
I (1529) camtest: bot-right (319,239): RGB565=0x4B64 R=9 G=27 B=4
```

`format=0` is `PIXFORMAT_RGB565`; `len = 320*240*2 = 153600` bytes is exactly
correct. The varying R/G/B values across the five samples show the CSI lines
deliver real image data.

## Reference

The vendor package (`D:\board_database\main-esp32-s3-cam2`) confirms this
board is an ESP32S3-EYE-pin-compatible design: its `CameraWebServer.ino` uses
`CAMERA_MODEL_ESP32S3_EYE`, the same GPIO map, and a hardcoded STA connection
(`ssid="HiwonderESP"`, `password="hiwonder"`) with the standard esp32-camera
MJPEG web server.

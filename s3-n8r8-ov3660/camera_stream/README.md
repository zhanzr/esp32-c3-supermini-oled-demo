# OV3660 MJPEG Stream Server (ESP32-S3 N8R8 OV3660 Board)

STA WiFi web server that streams the OV3660 as MJPEG and exposes a control
page (resolution / quality / pixel format / rotation) with a live FPS readout.
Built on the `camera_test` frame-capture foundation.

## Hardware connection

Same ESP32S3-EYE pinout as `camera_test` (see its README); no PWDN/RESET.

| Signal | GPIO | Signal | GPIO |
|--------|------|--------|------|
| SIOD   | 4    | XCLK   | 15   |
| SIOC   | 5    | CSI_PCLK | 13 |
| CSI_VSYNC | 6 | CSI_HREF | 7  |
| CSI_Y9 (D7) | 16 | CSI_Y5 (D3) | 10 |
| CSI_Y8 (D6) | 17 | CSI_Y4 (D2) | 8  |
| CSI_Y7 (D5) | 18 | CSI_Y3 (D1) | 9  |
| CSI_Y6 (D4) | 12 | CSI_Y2 (D0) | 11 |

## Two httpd instances (why two ports)

`esp_http_server` runs one select() loop per task, so a forever-looping
`/stream` handler would block every other route on the same server (observed:
`/status` FPS froze, `/control` was ignored while streaming). The fix is two
server instances, the same pattern as esp32-camera's `app_httpd.cpp`:

- **Port 80** (main): `/` page, `/favicon.ico`, `/capture`, `/status`, `/control`.
- **Port 81** (stream): `/stream` only, MJPEG `multipart/x-mixed-replace`.

### ctrl_port gotcha (reboot loop)

Both instances default to `ctrl_port = 32768` (`ESP_HTTPD_DEF_CTRL_PORT` in
`esp_http_server.h`): each server binds a UDP control socket to
`127.0.0.1:32768` to wake its own select() loop. The second `httpd_start()`
then fails with `error in creating ctrl socket (112)` = `EADDRINUSE`, which
was wrapped in `ESP_ERROR_CHECK` -> abort -> reboot loop. The stream server
must use a distinct ctrl port:

```c
httpd_config_t stream_cfg = HTTPD_DEFAULT_CONFIG();
stream_cfg.server_port = STREAM_SERVER_PORT;   /* 81 */
stream_cfg.ctrl_port   = STREAM_SERVER_PORT + 32768;
```

## Web UI (single HTML page, served from `/`)

- **Start / Stop / Grab** — mutually exclusive buttons. Start = `GET /stream`
  on port 81 (long-poll MJPEG); Grab = single `GET /capture`.
- **Mode** dropdown: Stream or Static.
- **FPS** — `GET /status` polled every second while streaming:
  `{"streaming":1,"fps":13.4,"w":320,"h":240,"format":4,"quality":12}`.
- **Apply controls** (`GET /control`): `res` (QQVGA..QXGA), `quality` (0-63),
  `pixfmt` (JPEG | RGB565), `rot` (0/90/180/270 via vflip/hmirror),
  `detect` (1/0, stream overlay).
  Note: 90/270 rotate in place; width/height are not swapped in `/status`.
- **Classify** button (`GET /classify`) — one-frame HSV color + shape
  classification, returns JSON:
  `{"count":1,"dets":[{"color":"green","shape":"triangle","area":471,"bbox":[22,0,98,42],...}]}`.
  Results are also drawn on the live stream when "Draw boxes" (detect) is on.
- Non-JPEG capture/stream is JPEG-compressed on the fly (`frame2jpg`, q80).

## Classifier (`classifier.c`)

Classic computer vision, no training:

- **Color**: RGB565 -> HSV (H 0-360, S/V 0-100) per pixel; 6 buckets
  (red/orange/yellow/green/blue/purple) with `S>=40, V>=40` gating.
- **Shape**: connected components (BFS flood fill, downscaled grid) per color;
  bounding-box fill ratio (`area/(w*h)`) + aspect ratio picks
  `bar`/`rect`/`circle`/`triangle`. Ideal values: circle 0.79, triangle 0.5,
  axis-aligned square/rect 1.0.
- Thresholds live at the top of `classifier.c` (`MIN_BLOB_FRAC`, HSV bounds,
  fill-ratio cutoffs) — tune them for your lighting/objects.
- Stream detection path (`stream_detect_frame`) decodes JPEG -> RGB565, runs
  the classifier, draws boxes, re-encodes (~4-6 FPS at QVGA).

The ML upgrade (TFLite Micro CNN, dataset prep, training) is documented in
`ML_TFLITE.md`.

## Component & config

- `main/idf_component.yml`: `espressif/esp32-camera: "^2.1.7"`.
- Octal PSRAM (frame buffer + JPEG compressor heap) — same sdkconfig.defaults
  as `camera_test`: `CONFIG_SPIRAM_MODE_OCT=y`, `CONFIG_SPIRAM_SPEED_80M=y`,
  `CONFIG_ESP32S3_SPIRAM_SUPPORT=y`.
- `xclk_freq_hz = 20 MHz` (vendor uses 15 MHz; 20 MHz gives PCLK 10 MHz /
  SYSCLK 50 MHz, ~16 FPS QVGA JPEG on this setup; 15 MHz -> ~12 FPS).
- Camera default: JPEG QVGA quality 12, `fb_count = 1`, `CAMERA_GRAB_WHEN_EMPTY`.
- WiFi: STA via `wifi_config.h` (same SSID/pass as the `wifi_con_test` project;
  `.example` has placeholders). Vendor firmware uses `HiwonderESP`/`hiwonder`.

## Verified output (2026-07-31)

```
I (4881) httpd: Main server started on port 80
I (4881) httpd: Stream server started on port 81
GET /status      -> 200 {"streaming":1,"fps":13.4,...,"format":4,"quality":12}
GET /capture     -> 200 image/jpeg (or frame2jpg-converted)
GET /stream:81   -> 200 multipart/x-mixed-replace, FPS stays live
GET /control?res=VGA -> 200, status then reports 640x480
```

FPS ~13 (QVGA JPEG); ~5-6 when streaming RGB565 (on-the-fly compression).
Device IP is printed to the console on connect.

Bottom line: QVGA for classic CV, VGA-JPEG + 96×96 int8 for TFLite. Both fit comfortably in the N8R8.

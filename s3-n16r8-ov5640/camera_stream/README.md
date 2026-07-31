# OV5640 MJPEG Stream Server (ESP32-S3 N16R8 OV5640 Board)

STA WiFi web server that streams the OV5640 as MJPEG and exposes a control
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
- **Apply controls** (`GET /control`): `res` (QQVGA..5MP), `quality` (0-63),
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
- `xclk_freq_hz = 20 MHz` (the official vendor kit value — do not overclock).
- Camera default: JPEG QVGA quality 12, `fb_count = 2`,
  `CAMERA_GRAB_LATEST` — matching the vendor kit (`Sketch_07.1_CameraWebServer`:
  `fb_count = 2`, `grab_mode = CAMERA_GRAB_LATEST` when PSRAM is present).
  The old `fb_count = 1` + `GRAB_WHEN_EMPTY` held the single frame buffer while
  the TCP send was blocked, so a WiFi stall starved the camera and collapsed
  FPS to 1-3; double-buffering keeps the camera filling a fresh buffer during
  TX stalls and `GRAB_LATEST` returns instantly after one.
- WiFi: `esp_wifi_set_ps(WIFI_PS_NONE)` before `esp_wifi_start()` — modem-sleep
  power save (default `WIFI_PS_MIN_MODEM`) woke the radio periodically and
  caused hundreds-of-ms TX stalls (same fix the vendor uses via
  `WiFi.setSleep(false)`).
- WiFi: STA via `wifi_config.h` (same SSID/pass as the `wifi_con_test` project;
  `.example` has placeholders). Vendor kit firmware uses `TP-LINK_D68D`.

## Verified output (2026-08-01)

```
I (4372) camera_stream: Got IP: 192.168.5.214
I (4372) camera_stream: AP rssi=-56 phy=11n ch=8
I (5112) ov5640: Set PLL: ... SYSCLK: 45000000 Hz, PCLK: 11250000 Hz
I (5252) camera_stream: Sensor: pid=0x5640 ver=0x0 slv=0x3C
I (5252) httpd: Main server started on port 80
I (5252) httpd: Stream server started on port 81
GET /status      -> 200 {"streaming":1,"fps":22.2,...,"format":4,"quality":12}
GET /capture     -> 200 image/jpeg (or frame2jpg-converted)
GET /stream:81   -> 200 multipart/x-mixed-replace, FPS stays live
GET /control?res=QSXGA -> 200, status then reports 2560x1920, quality 30
```

### Streaming stability fix (fb_count=2 + GRAB_LATEST + WIFI_PS_NONE)

Before (fb_count=1, GRAB_WHEN_EMPTY, modem-sleep on): QVGA stream was
intermittently 1-3 FPS. Diagnostics showed the camera was always healthy
(`grab_max` ~85 ms constant ≈ 11.5 FPS) while `send_max` (TCP TX) spiked to
0.3 / 1.5 / 5.9 s — the FPS collapses correlated 1:1 with WiFi TX stalls, not
the sensor. Applied the three changes above, A/B-verified on the same AP:

| Board | ANT | 60 s QVGA stream | gaps >300 ms | worst gap |
|-------|-----|------------------|--------------|-----------|
| S3-N8R8-OV3660 (reference) | inserted | 27.7 FPS (1664 frames) | 0 | 70 ms |
| S3-N16R8-OV5640 | PCB | 22.2 FPS (1331 frames) | 0 | 90 ms |

Server-side diag confirms no stalls: `fps=23.1 grab_max=40ms send_max=55ms
slow_grab=0 slow_send=0` (OV5640) and `fps=28.8 grab_max=32ms send_max=37ms`
(OV3660). Residual FPS difference between boards is the sensor PLL (OV3660
PCLK 10 MHz / SYSCLK 50 MHz vs OV5640 PCLK 11.25 MHz / SYSCLK 45 MHz) plus the
OV3660 board's better inserted antenna vs the OV5640 board's PCB antenna —
not a software issue. The OV5640 maxes ~22 FPS at QVGA (PCLK-limited).

Device IP is printed to the console on connect.

Resolution options now extend to **QSXGA (2560x1920) and 5MP (2592x1944)**
(the OV5640's maximum), via the `res` control. Note the OV5640's higher
resolutions are only usable as JPEG; RGB565 at QXGA+ exceeds PSRAM budget.

The OV5640's 5MP-class internal JPEG encoder at low quality values (e.g. the
default quality 12) exceeds the driver's 4 s frame-grab timeout, so QSXGA/5MP
stall. The `/control` handler therefore forces `quality >= 30` when those
sizes are selected, which keeps a single frame encode under ~2 s. Still
capture (`/capture`) at QSXGA/5MP yields ~0.7 MB JPEG frames.

Verified 2026-08-01 (streaming FPS / JPEG, JPEG mode, after stability fix):

| Resolution | Stream FPS | Capture |
|-----------|-----------|---------|
| QVGA      | ~22        | ~20 KB   |
| VGA       | ~22        | ~32 KB   |
| SVGA      | ~14        | ~44 KB   |
| XGA       | ~9.5       | ~52 KB   |
| HD        | ~7         | ~126 KB  |
| UXGA      | ~1.7       | ~477 KB  |
| QXGA      | ~1.2       | ~825 KB  |
| QSXGA     | ~1.2       | ~378 KB  |
| 5MP       | ~1.2       | ~695 KB  |

At HD and below the stream is now stall-free (zero gaps >300 ms) and limited by
the sensor PLL (PCLK 11.25 MHz) / TCP bandwidth, not WiFi stalls. At UXGA+ each
frame encode approaches/exceeds the 4 s stream window, so FPS drops to ~1-2
even though the link sustains ~0.5-0.6 MB/s with no errors; use `/capture` for
the top sizes. The driver caps the sensor at `FRAMESIZE_QSXGA` (requests for
`5MP` are clamped to QSXGA by the esp32-camera sensor table).

Bottom line: QVGA for classic CV, VGA-JPEG + 96×96 int8 for TFLite,
5MP-JPEG for stills. All fit comfortably in the N16R8.

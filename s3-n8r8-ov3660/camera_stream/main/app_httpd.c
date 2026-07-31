#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_http_server.h"
#include "esp_camera.h"
#include "app_httpd.h"

static const char *TAG = "httpd";

/* The MJPEG stream loops forever inside the httpd handler, and esp_http_server
 * processes all connections in a single task. So the stream must run on its
 * own httpd instance (port 81) or /status and /control stall while streaming.
 */
#define MAIN_SERVER_PORT   80
#define STREAM_SERVER_PORT 81

#define PART_BOUNDARY   "123456789000000000000987654321"
#define STREAM_CONTENT_TYPE "multipart/x-mixed-replace;boundary=" PART_BOUNDARY
#define STREAM_BOUNDARY "\r\n--" PART_BOUNDARY "\r\n"
#define STREAM_PART     "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n"

static volatile int g_streaming = 0;
static float g_fps = 0.0f;
static int g_quality = 12;

static const struct {
    const char *name;
    framesize_t size;
} g_res_map[] = {
    { "QQVGA", FRAMESIZE_QQVGA },
    { "QVGA",  FRAMESIZE_QVGA  },
    { "VGA",   FRAMESIZE_VGA   },
    { "SVGA",  FRAMESIZE_SVGA  },
    { "XGA",   FRAMESIZE_XGA   },
    { "HD",    FRAMESIZE_HD    },
    { "SXGA",  FRAMESIZE_SXGA  },
    { "UXGA",  FRAMESIZE_UXGA  },
    { "QXGA",  FRAMESIZE_QXGA  },
    { NULL, 0 },
};

/* -------------------------------------------------------------------------
 * /  -> control page
 * ---------------------------------------------------------------------- */
static const char g_index_html[] =
"<!DOCTYPE html>\n"
"<html><head><meta charset=\"utf-8\">\n"
"<title>OV3660 Stream</title>\n"
"<style>\n"
"body{font-family:Segoe UI,Arial,sans-serif;background:#121212;color:#eee;margin:16px}\n"
".card{background:#1e1e1e;border:1px solid #333;border-radius:8px;padding:12px;margin-bottom:12px}\n"
"select,button,input{background:#2a2a2a;color:#eee;border:1px solid #555;border-radius:4px;padding:6px 8px;margin:4px}\n"
"button{cursor:pointer}\n"
"h2,h3{margin:4px 0}\n"
"#view{width:100%;max-width:960px;background:#000;border:1px solid #333;border-radius:8px;min-height:240px}\n"
"#view img{width:100%;display:block}\n"
"#stream{display:none}\n"
"#still{display:none}\n"
"</style></head><body>\n"
"<h2>ESP32-S3 N8R8 OV3660</h2>\n"
"<div class=\"card\">\n"
"<label>Mode:\n"
"<select id=\"mode\"><option value=\"stream\" selected>Stream</option><option value=\"static\">Static</option></select>\n"
"</label>\n"
"<button id=\"btnStart\">Start</button>\n"
"<button id=\"btnStop\" style=\"display:none\">Stop</button>\n"
"<button id=\"btnGrab\" style=\"display:none\">Grab</button>\n"
"<span>FPS: <b id=\"fps\">-</b></span>\n"
"<span>Status: <b id=\"stat\">idle</b></span>\n"
"</div>\n"
"<div class=\"card\">\n"
"<label>Resolution:\n"
"<select id=\"res\">\n"
"<option value=\"QQVGA\">QQVGA 160x120</option>\n"
"<option value=\"QVGA\" selected>QVGA 320x240</option>\n"
"<option value=\"VGA\">VGA 640x480</option>\n"
"<option value=\"SVGA\">SVGA 800x600</option>\n"
"<option value=\"XGA\">XGA 1024x768</option>\n"
"<option value=\"HD\">HD 1280x720</option>\n"
"<option value=\"SXGA\">SXGA 1280x1024</option>\n"
"<option value=\"UXGA\">UXGA 1600x1200</option>\n"
"<option value=\"QXGA\">QXGA 2048x1536</option>\n"
"</select></label>\n"
"<label>Rotation:\n"
"<select id=\"rot\"><option value=\"0\" selected>0 deg</option><option value=\"90\">90 deg</option><option value=\"180\">180 deg</option><option value=\"270\">270 deg</option></select>\n"
"</label>\n"
"<label>Quality: <input type=\"range\" id=\"q\" min=\"4\" max=\"63\" value=\"12\"> <b id=\"qval\">12</b></label>\n"
"<label>Format:\n"
"<select id=\"pix\"><option value=\"JPEG\" selected>JPEG</option><option value=\"RGB565\">RGB565</option></select>\n"
"</label>\n"
"<button id=\"btnApply\">Apply</button>\n"
"</div>\n"
"<div id=\"view\"><img id=\"stream\"><img id=\"still\"></div>\n"
"<script>\n"
"var m=document.getElementById('mode'),s=document.getElementById('stream'),\n"
"    st=document.getElementById('still'),bs=document.getElementById('btnStart'),\n"
"    bp=document.getElementById('btnStop'),bg=document.getElementById('btnGrab'),\n"
"    f=document.getElementById('fps'),t=document.getElementById('stat'),\n"
"    r=document.getElementById('res'),q=document.getElementById('q'),\n"
"    qv=document.getElementById('qval'),p=document.getElementById('pix'),\n"
"    rot=document.getElementById('rot');\n"
"var streaming=false;\n"
"var STREAM_URL='http://'+location.hostname+':81/stream';\n"
"function show(el){el.style.display='inline-block';}\n"
"function hide(el){el.style.display='none';}\n"
"function showimg(el){el.style.display='block';}\n"
"function hideimg(el){el.style.display='none';}\n"
"function refreshButtons(){\n"
"  if(m.value!=='stream'){hide(bs);hide(bp);show(bg);return;}\n"
"  hide(bg);\n"
"  if(streaming){show(bp);hide(bs);}else{show(bs);hide(bp);}\n"
"}\n"
"function startStream(){\n"
"  streaming=true;t.textContent='starting';showimg(s);hideimg(st);s.src=STREAM_URL;refreshButtons();\n"
"}\n"
"function stopStream(){streaming=false;s.removeAttribute('src');t.textContent='stopped';refreshButtons();}\n"
"function grab(){st.src='/capture?ts='+Date.now();t.textContent='grabbed';}\n"
"function setMode(){\n"
"  if(m.value==='stream'){hideimg(st);showimg(s);}\n"
"  else{hideimg(s);showimg(st);stopStream();}\n"
"  refreshButtons();\n"
"}\n"
"function applyCfg(){\n"
"  fetch('/control?res='+encodeURIComponent(r.value)+'&quality='+q.value+'&pixfmt='+encodeURIComponent(p.value)+'&rot='+encodeURIComponent(rot.value))\n"
"    .then(function(){t.textContent='applied';});\n"
"  if(streaming){stopStream();startStream();}\n"
"}\n"
"bs.onclick=function(){if(m.value!=='stream'){m.value='stream';setMode();}startStream();};\n"
"bp.onclick=function(){stopStream();};\n"
"bg.onclick=grab;\n"
"q.oninput=function(){qv.textContent=q.value;};\n"
"m.onchange=setMode;\n"
"rot.onchange=function(){applyCfg();};\n"
"setMode();\n"
"setInterval(function(){\n"
"  fetch('/status').then(function(x){return x.json();}).then(function(j){\n"
"    if(j.streaming){f.textContent=j.fps.toFixed(1);t.textContent='streaming '+j.w+'x'+j.h+' '+(j.format===0?'RGB565':'JPEG');}\n"
"    else f.textContent='-';\n"
"  }).catch(function(){});\n"
"},1000);\n"
"</script></body></html>\n";

static esp_err_t handler_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, g_index_html, strlen(g_index_html));
}

/* -------------------------------------------------------------------------
 * /favicon.ico -> quiet 204 (browsers always ask for it)
 * ---------------------------------------------------------------------- */
static esp_err_t handler_favicon(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_sendstr(req, "");
}

/* -------------------------------------------------------------------------
 * /stream (own server on port 81) -> MJPEG multipart stream
 * ---------------------------------------------------------------------- */
static esp_err_t handler_stream(httpd_req_t *req)
{
    static int64_t last_fps_ts = 0;
    static uint32_t frames = 0;

    httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    g_streaming = 1;
    g_fps = 0.0f;
    last_fps_ts = 0;
    frames = 0;

    esp_err_t res = ESP_OK;
    bool first = true;
    while (true) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            break;
        }
        if (first) {
            /* First frame after stream start can be missing the JPEG SOI
             * marker (cam_hal NO-SOI). Discard it, the next one is clean. */
            first = false;
            esp_camera_fb_return(fb);
            continue;
        }

        uint8_t *jpg_buf = NULL;
        size_t jpg_len = 0;
        if (fb->format != PIXFORMAT_JPEG) {
            if (!frame2jpg(fb, 80, &jpg_buf, &jpg_len)) {
                ESP_LOGE(TAG, "JPEG compression failed");
                esp_camera_fb_return(fb);
                break;
            }
        } else {
            jpg_buf = fb->buf;
            jpg_len = fb->len;
        }

        char part_buf[64];
        int hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, (unsigned)jpg_len);
        if (hlen < 0 || hlen >= (int)sizeof(part_buf)) {
            ESP_LOGE(TAG, "Part header truncated");
        } else {
            res = httpd_resp_send_chunk(req, part_buf, (size_t)hlen);
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char *)jpg_buf, jpg_len);
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
        }

        if (fb->format != PIXFORMAT_JPEG) {
            free(jpg_buf);
        }
        esp_camera_fb_return(fb);
        if (res != ESP_OK) {
            break;   /* client closed the connection (Stop/reload) */
        }

        frames++;
        int64_t now = esp_timer_get_time();
        if (last_fps_ts == 0) {
            last_fps_ts = now;
        }
        if (now - last_fps_ts >= 1000000) {
            g_fps = (float)frames * 1000000.0f / (float)(now - last_fps_ts);
            frames = 0;
            last_fps_ts = now;
        }
    }

    g_streaming = 0;
    g_fps = 0.0f;
    return ESP_OK;   /* suppress "uri handler execution failed" on client disconnect */
}

/* -------------------------------------------------------------------------
 * /capture -> single JPEG image
 * ---------------------------------------------------------------------- */
static esp_err_t handler_capture(httpd_req_t *req)
{
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");

    esp_err_t res = ESP_OK;
    if (fb->format == PIXFORMAT_JPEG) {
        res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    } else {
        uint8_t *jpg = NULL;
        size_t jlen = 0;
        if (frame2jpg(fb, 80, &jpg, &jlen)) {
            res = httpd_resp_send(req, (const char *)jpg, jlen);
            free(jpg);
        } else {
            httpd_resp_send_500(req);
            res = ESP_FAIL;
        }
    }

    esp_camera_fb_return(fb);
    return res;
}

/* -------------------------------------------------------------------------
 * /status -> JSON with streaming flag, fps, resolution, format
 * ---------------------------------------------------------------------- */
static esp_err_t handler_status(httpd_req_t *req)
{
    sensor_t *s = esp_camera_sensor_get();
    int w = 320, h = 240;
    int fmt = PIXFORMAT_JPEG;
    if (s) {
        fmt = s->pixformat;
        if (s->status.framesize < FRAMESIZE_INVALID) {
            w = resolution[s->status.framesize].width;
            h = resolution[s->status.framesize].height;
        }
    }

    char buf[160];
    int len = snprintf(buf, sizeof(buf),
                       "{\"streaming\":%d,\"fps\":%.1f,\"w\":%d,\"h\":%d,\"format\":%d,\"quality\":%d}",
                       (int)g_streaming, (double)g_fps, w, h, fmt, g_quality);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, len);
}

/* -------------------------------------------------------------------------
 * /control?res=...&quality=...&pixfmt=...&rot=...  (GET, applied immediately)
 * ---------------------------------------------------------------------- */
static esp_err_t handler_control(httpd_req_t *req)
{
    sensor_t *s = esp_camera_sensor_get();
    if (!s) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    size_t qlen = httpd_req_get_url_query_len(req);
    if (qlen == 0) {
        httpd_resp_sendstr(req, "OK");
        return ESP_OK;
    }
    if (qlen > 255) {
        qlen = 255;
    }
    char query[256];
    httpd_req_get_url_query_str(req, query, qlen + 1);

    char val[64];
    if (httpd_query_key_value(query, "res", val, sizeof(val)) == ESP_OK) {
        for (int i = 0; g_res_map[i].name; i++) {
            if (strcmp(g_res_map[i].name, val) == 0) {
                ESP_LOGI(TAG, "Set framesize: %s", val);
                s->set_framesize(s, g_res_map[i].size);
                break;
            }
        }
    }
    if (httpd_query_key_value(query, "quality", val, sizeof(val)) == ESP_OK) {
        int q = atoi(val);
        if (q < 0) q = 0;
        if (q > 63) q = 63;
        g_quality = q;
        ESP_LOGI(TAG, "Set quality: %d", q);
        s->set_quality(s, q);
    }
    if (httpd_query_key_value(query, "pixfmt", val, sizeof(val)) == ESP_OK) {
        if (strcmp(val, "RGB565") == 0) {
            ESP_LOGI(TAG, "Set pixformat: RGB565");
            s->set_pixformat(s, PIXFORMAT_RGB565);
        } else {
            ESP_LOGI(TAG, "Set pixformat: JPEG");
            s->set_pixformat(s, PIXFORMAT_JPEG);
        }
    }
    if (httpd_query_key_value(query, "rot", val, sizeof(val)) == ESP_OK) {
        int rot = atoi(val);
        int vflip = 0, hmirror = 0;
        switch (rot) {
            case 90:  vflip = 1; hmirror = 0; break;
            case 180: vflip = 1; hmirror = 1; break;
            case 270: vflip = 0; hmirror = 1; break;
            default:  rot = 0;  break;
        }
        ESP_LOGI(TAG, "Set rotation: %d deg (vflip=%d hmirror=%d)", rot, vflip, hmirror);
        s->set_vflip(s, vflip);
        s->set_hmirror(s, hmirror);
    }

    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

static esp_err_t register_uri(httpd_handle_t server, const char *uri,
                              esp_err_t (*handler)(httpd_req_t *))
{
    httpd_uri_t u = { .uri = uri, .method = HTTP_GET, .handler = handler, .user_ctx = NULL };
    return httpd_register_uri_handler(server, &u);
}

/* -------------------------------------------------------------------------
 * Server bootstrap (two httpd instances: main + stream)
 * ---------------------------------------------------------------------- */
esp_err_t start_camera_server(void)
{
    /* Main server (port 80): page, capture, status, control. */
    httpd_config_t main_cfg = HTTPD_DEFAULT_CONFIG();
    main_cfg.server_port = MAIN_SERVER_PORT;
    main_cfg.lru_purge_enable = true;
    main_cfg.stack_size = 8192;

    httpd_handle_t main_server = NULL;
    esp_err_t err = httpd_start(&main_server, &main_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start main server: %s", esp_err_to_name(err));
        return err;
    }
    register_uri(main_server, "/", handler_root);
    register_uri(main_server, "/favicon.ico", handler_favicon);
    register_uri(main_server, "/capture", handler_capture);
    register_uri(main_server, "/status", handler_status);
    register_uri(main_server, "/control", handler_control);
    ESP_LOGI(TAG, "Main server started on port %d", MAIN_SERVER_PORT);

    /* Stream server (port 81): MJPEG only, so it can block forever without
     * stalling /status and /control on the main server. */
    httpd_config_t stream_cfg = HTTPD_DEFAULT_CONFIG();
    stream_cfg.server_port = STREAM_SERVER_PORT;
    stream_cfg.stack_size = 16384;
    /* Both instances default to ctrl_port 32768 (a UDP socket bound to
     * 127.0.0.1), so the second httpd_start fails with EADDRINUSE (112).
     * Give the stream server its own ctrl port. */
    stream_cfg.ctrl_port = STREAM_SERVER_PORT + 32768;

    httpd_handle_t stream_server = NULL;
    err = httpd_start(&stream_server, &stream_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start stream server: %s", esp_err_to_name(err));
        return err;
    }
    register_uri(stream_server, "/stream", handler_stream);
    ESP_LOGI(TAG, "Stream server started on port %d", STREAM_SERVER_PORT);

    return ESP_OK;
}

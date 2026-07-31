#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_camera.h"

static const char *TAG = "camtest";

/* S3-N8R8 OV3660 board wiring */
#define CAM_PIN_PWDN   -1
#define CAM_PIN_RESET  -1
#define CAM_PIN_XCLK   15
#define CAM_PIN_SIOD   4
#define CAM_PIN_SIOC   5
#define CAM_PIN_D7     16   /* Y9 */
#define CAM_PIN_D6     17   /* Y8 */
#define CAM_PIN_D5     18   /* Y7 */
#define CAM_PIN_D4     12   /* Y6 */
#define CAM_PIN_D3     10   /* Y5 */
#define CAM_PIN_D2     8    /* Y4 */
#define CAM_PIN_D1     9    /* Y3 */
#define CAM_PIN_D0     11   /* Y2 */
#define CAM_PIN_VSYNC  6
#define CAM_PIN_HREF   7
#define CAM_PIN_PCLK   13

static uint16_t get_pixel_rgb565(const camera_fb_t *fb, uint32_t x, uint32_t y)
{
    if (fb->format != PIXFORMAT_RGB565 || fb->width == 0) {
        return 0;
    }
    size_t off = ((size_t)y * fb->width + x) * 2;
    if (off + 1 >= fb->len) {
        return 0;
    }
    return (uint16_t)(fb->buf[off] | (fb->buf[off + 1] << 8));
}

static void dump_pixel(const char *label, const camera_fb_t *fb, uint32_t x, uint32_t y)
{
    uint16_t p = get_pixel_rgb565(fb, x, y);
    ESP_LOGI(TAG, "%s (%u,%u): RGB565=0x%04X R=%u G=%u B=%u",
             label, x, y, p, (p >> 11) & 0x1F, (p >> 5) & 0x3F, p & 0x1F);
}

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  ESP32-S3 N8R8 OV3660 Camera Test");
    ESP_LOGI(TAG, "  XCLK=GPIO15 SIOD=GPIO4 SIOC=GPIO5");
    ESP_LOGI(TAG, "  PCLK=GPIO13 VSYNC=GPIO6 HREF=GPIO7");
    ESP_LOGI(TAG, "========================================");

    camera_config_t camera_config = {
        .pin_pwdn     = CAM_PIN_PWDN,
        .pin_reset    = CAM_PIN_RESET,
        .pin_xclk     = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,

        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href  = CAM_PIN_HREF,
        .pin_pclk  = CAM_PIN_PCLK,

        .xclk_freq_hz = 20000000,
        .ledc_timer   = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_RGB565,
        .frame_size   = FRAMESIZE_QVGA,
        .jpeg_quality = 12,
        .fb_count     = 1,
        .fb_location  = CAMERA_FB_IN_PSRAM,
        .grab_mode    = CAMERA_GRAB_WHEN_EMPTY,
    };

    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: 0x%X (%s)", err, esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "Camera init OK");

    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        ESP_LOGI(TAG, "Sensor: pid=0x%X ver=0x%X mid=0x%02X%02X slv=0x%02X",
                 s->id.PID, s->id.VER, s->id.MIDH, s->id.MIDL, s->slv_addr);
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (fb == NULL) {
        ESP_LOGE(TAG, "Camera capture failed");
        return;
    }

    ESP_LOGI(TAG, "Captured frame: %ux%u format=%u len=%u",
             fb->width, fb->height, fb->format, (unsigned)fb->len);

    if (fb->format == PIXFORMAT_RGB565) {
        dump_pixel("center  ", fb, fb->width / 2, fb->height / 2);
        dump_pixel("top-left", fb, 0, 0);
        dump_pixel("top-right", fb, fb->width - 1, 0);
        dump_pixel("bot-left", fb, 0, fb->height - 1);
        dump_pixel("bot-right", fb, fb->width - 1, fb->height - 1);
    }

    esp_camera_fb_return(fb);
    ESP_LOGI(TAG, "Done");
}

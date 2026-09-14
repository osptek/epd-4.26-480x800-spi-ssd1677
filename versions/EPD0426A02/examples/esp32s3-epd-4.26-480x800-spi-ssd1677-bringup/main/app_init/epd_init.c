#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"

#include "epd_init.h"
#include "epd_ssd1677.h"
#include "epd_text.h"
#include "font32x32_bold.h"
#include "demo_1_4g.h"
#include "demo_2_4g.h"
#include "demo_3_4g.h"
#include "bw_1_bw.h"
#include "bw_2_bw.h"
#include "gray16_1_4g.h"
#include "gray16_2_4g.h"

static const char *TAG = "epd_init";

esp_err_t app_epd_init(void)
{
    ESP_RETURN_ON_ERROR(epd_ssd1677_init(), TAG, "epd init failed");

    ESP_LOGI(TAG, "white");
    ESP_RETURN_ON_ERROR(epd_ssd1677_fill(3), TAG, "fill white failed");
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "black");
    ESP_RETURN_ON_ERROR(epd_ssd1677_fill(0), TAG, "fill black failed");
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "checkerboard");
    ESP_RETURN_ON_ERROR(epd_ssd1677_show_checkerboard(), TAG, "checkerboard failed");
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "stripes");
    ESP_RETURN_ON_ERROR(epd_ssd1677_show_stripes(), TAG, "stripes failed");
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "partial animation");
    ESP_RETURN_ON_ERROR(epd_ssd1677_show_partial_demo(), TAG, "partial demo failed");
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_RETURN_ON_ERROR(epd_ssd1677_power_off(), TAG, "powerOff failed");
    ESP_LOGI(TAG, "EPD demo done");
    return ESP_OK;
}

esp_err_t app_epd_partial_demo(void)
{
    ESP_RETURN_ON_ERROR(epd_ssd1677_init(), TAG, "epd init failed");

    ESP_LOGI(TAG, "=== partial refresh demo ===");
    ESP_RETURN_ON_ERROR(epd_ssd1677_show_partial_demo(), TAG, "partial demo failed");

    ESP_RETURN_ON_ERROR(epd_ssd1677_power_off(), TAG, "powerOff failed");
    ESP_LOGI(TAG, "partial demo done");
    return ESP_OK;
}

esp_err_t app_epd_image_demo(void)
{
    ESP_RETURN_ON_ERROR(epd_ssd1677_init(), TAG, "epd init failed");

    ESP_LOGI(TAG, "=== B/W partial text demo ===");
    ESP_RETURN_ON_ERROR(epd_ssd1677_clear(0xFF), TAG, "white baseline failed");
    ESP_RETURN_ON_ERROR(
        epd_ssd1677_show_text_partial(
            "ABCDEFGHIJKL\n"
            "MNOPQRSTUVWX\n"
            "YZ\n"
            "abcdefghijkl\n"
            "mnopqrstuvwx\n"
            "yz\n"
            "0123456789\n"
            " +-!?:.,/%",
            16, 16, EPD_TEXT_SCALE_DEFAULT),
        TAG, "show partial text failed");

    ESP_LOGI(TAG, "image demo done");
    return ESP_OK;
}

typedef struct {
    const char *text;
} boot_line_t;

static const boot_line_t BOOT_LINES[] = {
    {"OSPTEK BOOT v1.0"},
    {"----------------"},
    {"cpu up"},
    {"heap ok"},
    {"init SPI2..."},
    {"bus 4MHz"},
    {"gpio ok"},
    {"reset panel"},
    {"wait busy"},
    {"SSD1677 init"},
    {"lut loaded"},
    {"GDEQ0426T82"},
    {"480x800"},
    {"rotation 3"},
    {"partial on"},
    {"b/w mode"},
    {"frame sync"},
    {"drv ready"},
    {"load fonts"},
    {"term ok"},
    {"sys ready"},
};

#define BOOT_LOGO_COLS    12
#define BOOT_LOGO_MARGIN  ((EPD_WIDTH - BOOT_LOGO_COLS * EPD_FONT_W) / 2)
#define BOOT_TERM_X       0
#define BOOT_TERM_Y       8
#define BOOT_TERM_LINE_GAP 4
#define DEMO_SWITCH_PAUSE_MS  6000

esp_err_t app_epd_boot_demo(void)
{
    const uint8_t scale = EPD_TEXT_SCALE_DEFAULT;
    const char *logo = "   OSPTEK   ";
    const int16_t logo_line_w = (int16_t)(BOOT_LOGO_COLS * EPD_FONT_W * scale);
    const int16_t term_line_step = (int16_t)(EPD_FONT_H * scale + BOOT_TERM_LINE_GAP);

    ESP_RETURN_ON_ERROR(epd_ssd1677_init(), TAG, "epd init failed");

    ESP_LOGI(TAG, "=== boot: grey4 grey16 -> OSPTEK -> terminal -> slideshow ===");

    ESP_LOGI(TAG, "show grey4_levels");
    ESP_RETURN_ON_ERROR(epd_ssd1677_show_grey4_levels(), TAG, "grey4 failed");
    vTaskDelay(pdMS_TO_TICKS(DEMO_SWITCH_PAUSE_MS));

    ESP_LOGI(TAG, "show grey16_stripes");
    ESP_RETURN_ON_ERROR(epd_ssd1677_show_grey16_stripes(), TAG, "grey16 failed");
    vTaskDelay(pdMS_TO_TICKS(DEMO_SWITCH_PAUSE_MS));

    ESP_RETURN_ON_ERROR(epd_ssd1677_clear(0xFF), TAG, "white baseline failed");

    int16_t lh = (int16_t)(EPD_FONT_H * scale);
    int16_t cy = (int16_t)((EPD_HEIGHT - lh) / 2);

    ESP_LOGI(TAG, "show logo");
    ESP_RETURN_ON_ERROR(epd_ssd1677_show_text_partial(logo, BOOT_LOGO_MARGIN, cy, scale), TAG, "logo failed");
    vTaskDelay(pdMS_TO_TICKS(2500));

    ESP_LOGI(TAG, "erase logo");
    ESP_RETURN_ON_ERROR(
        epd_text_clear_partial(BOOT_LOGO_MARGIN, (int16_t)(cy - 8),
                               logo_line_w, (int16_t)(lh + 16)),
        TAG, "erase logo failed");
    vTaskDelay(pdMS_TO_TICKS(400));

    int16_t y = BOOT_TERM_Y;
    for (size_t i = 0; i < sizeof(BOOT_LINES) / sizeof(BOOT_LINES[0]); i++) {
        const char *line = BOOT_LINES[i].text;
        ESP_LOGI(TAG, "boot: %s", line);
        ESP_RETURN_ON_ERROR(
            epd_ssd1677_show_text_partial_term(line, BOOT_TERM_X, y, scale),
            TAG, "boot line failed");
        y = (int16_t)(y + term_line_step);
        if (y + EPD_FONT_H > EPD_HEIGHT) {
            break;
        }
    }

    ESP_LOGI(TAG, "boot demo done");
    return app_epd_slideshow_loop();
}

typedef enum {
    SLIDE_BW,
    SLIDE_4G,
    SLIDE_GREY4,
    SLIDE_GREY16,
} slide_kind_t;

typedef struct {
    slide_kind_t kind;
    const uint8_t *data;
    const char *name;
} slide_t;

static const slide_t SLIDES[] = {
    { SLIDE_BW,     bw_1_bw,        "bw_1" },
    { SLIDE_BW,     bw_2_bw,        "bw_2" },
    { SLIDE_4G,     demo_1_4g_4g,   "demo_1" },
    { SLIDE_4G,     demo_2_4g_4g,   "demo_2" },
    { SLIDE_4G,     demo_3_4g_4g,   "demo_3" },
    { SLIDE_4G,     gray16_1_4g_4g, "gray16_1" },
    { SLIDE_4G,     gray16_2_4g_4g, "gray16_2" },
};

esp_err_t app_epd_slideshow_loop(void)
{
    ESP_LOGI(TAG, "=== slideshow loop (bw_1 bw_2 demo_1..3 gray16_1 gray16_2) ===");

    uint32_t round = 0;
    while (1) {
        round++;
        ESP_LOGI(TAG, "slideshow round %lu", (unsigned long)round);
        for (size_t i = 0; i < sizeof(SLIDES) / sizeof(SLIDES[0]); i++) {
            int64_t t0 = esp_timer_get_time();
            ESP_LOGI(TAG, "show %s (start)", SLIDES[i].name);
            if (SLIDES[i].kind == SLIDE_BW) {
                ESP_RETURN_ON_ERROR(
                    epd_ssd1677_show_image_bw(SLIDES[i].data),
                    TAG, "show bw image failed");
            } else if (SLIDES[i].kind == SLIDE_4G) {
                ESP_RETURN_ON_ERROR(
                    epd_ssd1677_show_image_4g(SLIDES[i].data),
                    TAG, "show 4g image failed");
            } else if (SLIDES[i].kind == SLIDE_GREY4) {
                ESP_RETURN_ON_ERROR(
                    epd_ssd1677_show_grey4_levels(),
                    TAG, "show grey4 failed");
            } else {
                ESP_RETURN_ON_ERROR(
                    epd_ssd1677_show_grey16_stripes(),
                    TAG, "show grey16 failed");
            }
            ESP_LOGI(TAG, "%s done: %lld ms",
                     SLIDES[i].name, (long long)((esp_timer_get_time() - t0) / 1000));
            vTaskDelay(pdMS_TO_TICKS(DEMO_SWITCH_PAUSE_MS));
        }
    }

    return ESP_OK;
}

esp_err_t app_epd_bw_demo_loop(void)
{
    return app_epd_slideshow_loop();
}

esp_err_t app_epd_image_slideshow_loop(void)
{
    ESP_LOGI(TAG, "=== 4G image slideshow loop ===");

    uint32_t round = 0;
    while (1) {
        round++;
        ESP_LOGI(TAG, "slideshow round %lu", (unsigned long)round);
        for (size_t i = 2; i < sizeof(SLIDES) / sizeof(SLIDES[0]); i++) {
            if (SLIDES[i].kind != SLIDE_4G) {
                continue;
            }
            ESP_LOGI(TAG, "show %s", SLIDES[i].name);
            ESP_RETURN_ON_ERROR(
                epd_ssd1677_show_image_4g(SLIDES[i].data),
                TAG, "show image failed");
            vTaskDelay(pdMS_TO_TICKS(DEMO_SWITCH_PAUSE_MS));
        }
    }

    return ESP_OK;
}

esp_err_t app_epd_refresh_loop(void)
{
    ESP_RETURN_ON_ERROR(epd_ssd1677_init(), TAG, "epd init failed");

    ESP_LOGI(TAG, "Enter full-screen refresh loop (white/black), measure VCC on serial markers");
    vTaskDelay(pdMS_TO_TICKS(1000));

    uint32_t cycle = 0;
    while (1) {
        cycle++;
        ESP_LOGW(TAG, ">>> cycle %lu: REFRESH WHITE START <<<", (unsigned long)cycle);
        ESP_RETURN_ON_ERROR(epd_ssd1677_clear(0xFF), TAG, "clear white failed");
        ESP_LOGW(TAG, ">>> cycle %lu: REFRESH WHITE DONE <<<", (unsigned long)cycle);
        vTaskDelay(pdMS_TO_TICKS(3000));

        ESP_LOGW(TAG, ">>> cycle %lu: REFRESH BLACK START <<<", (unsigned long)cycle);
        ESP_RETURN_ON_ERROR(epd_ssd1677_clear(0x00), TAG, "clear black failed");
        ESP_LOGW(TAG, ">>> cycle %lu: REFRESH BLACK DONE <<<", (unsigned long)cycle);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }

    return ESP_OK;
}

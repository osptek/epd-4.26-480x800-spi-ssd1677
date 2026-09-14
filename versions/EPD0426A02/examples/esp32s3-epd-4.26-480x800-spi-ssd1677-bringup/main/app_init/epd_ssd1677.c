#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_rom_gpio.h"
#include "esp_timer.h"

#include "board_pins.h"
#include "epd_ssd1677.h"
#include "epd_text.h"

static const char *TAG = "epd_ssd1677";

#define EPD_SPI_HOST            SPI2_HOST
#define EPD_SPI_FREQ_HZ         (8 * 1000 * 1000)
#define EPD_BUSY_ACTIVE_LEVEL   1   /* GxEPD2: HIGH = busy, idle = 0 */
#define EPD_BUSY_TIMEOUT_US     (15 * 1000 * 1000)
#define EPD_RESET_MS            10
#define EPD_POWER_ON_MS         100
#define EPD_POWER_OFF_MS        150
#define EPD_GREY_REFRESH_MS     4000
#define EPD_FULL_REFRESH_MS     1800
#define EPD_PARTIAL_REFRESH_MS  510
#define EPD_BUFFER_BYTES        ((EPD_NATIVE_WIDTH / 4) * EPD_PAGE_HEIGHT)

typedef enum {
    EPD_REFRESH_FULL,
    EPD_REFRESH_GREY,
    EPD_REFRESH_FAST,
    EPD_REFRESH_FORCED_FULL,
} epd_refresh_mode_t;

static spi_device_handle_t s_spi;
static bool s_init_display_done;
static bool s_init_4g_done;
static bool s_power_is_on;
static bool s_hibernating;
static bool s_initial_write = true;
static bool s_initial_refresh = true;
static epd_refresh_mode_t s_refresh_mode = EPD_REFRESH_FULL;

/* 4G LUT from reference/4灰波表.txt (alternate to GxEPD2 default) */
static const uint8_t s_lut_4g[] = {
    0x80, 0x48, 0x4A, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x48, 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x88, 0x48, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xA8, 0x48, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x07, 0x1E, 0x1C, 0x02, 0x00,
    0x05, 0x01, 0x05, 0x01, 0x02,
    0x08, 0x02, 0x01, 0x04, 0x04,
    0x00, 0x02, 0x00, 0x02, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01,
    0x22, 0x22, 0x22, 0x22, 0x22,
    0x17, 0x41, 0xA8, 0x32, 0x30,
    0x00, 0x00,
};

static void epd_cs_set(int level)
{
    gpio_set_level(PIN_EPD_CS, level);
}

static void epd_dc_set(int level)
{
    gpio_set_level(PIN_EPD_DC, level);
}

static esp_err_t epd_spi_tx(const uint8_t *data, size_t len)
{
    if (len == 0) {
        return ESP_OK;
    }
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    esp_err_t err = spi_device_polling_transmit(s_spi, &t);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI tx failed: %s", esp_err_to_name(err));
    }
    return err;
}

static void epd_write_command(uint8_t cmd)
{
    epd_dc_set(0);
    epd_cs_set(0);
    epd_spi_tx(&cmd, 1);
    epd_cs_set(1);
    epd_dc_set(1);
}

static void epd_write_data(uint8_t data)
{
    epd_cs_set(0);
    epd_spi_tx(&data, 1);
    epd_cs_set(1);
}

static void epd_write_data_bulk(const uint8_t *data, size_t len)
{
    epd_cs_set(0);
    epd_spi_tx(data, len);
    epd_cs_set(1);
}

static void epd_write_data_fill(uint8_t value, size_t len)
{
    epd_dc_set(1);
    epd_cs_set(0);
    uint8_t chunk[512];
    while (len > 0) {
        size_t n = len > sizeof(chunk) ? sizeof(chunk) : len;
        memset(chunk, value, n);
        epd_spi_tx(chunk, n);
        len -= n;
    }
    epd_cs_set(1);
}

static void epd_wait_busy(const char *comment, uint16_t busy_time_ms)
{
    vTaskDelay(pdMS_TO_TICKS(1));

    int64_t start = esp_timer_get_time();
    int64_t busy_time_us = (int64_t)busy_time_ms * 1000;
    bool saw_busy = false;
    while (1) {
        int level = gpio_get_level(PIN_EPD_BUSY);
        if (level == EPD_BUSY_ACTIVE_LEVEL) {
            saw_busy = true;
        } else if (saw_busy) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
        int64_t elapsed = esp_timer_get_time() - start;
        if (saw_busy && elapsed > EPD_BUSY_TIMEOUT_US) {
            ESP_LOGW(TAG, "%s: BUSY wait timeout, level=%d",
                     comment ? comment : "epd", level);
            break;
        }
        if (!saw_busy && elapsed >= busy_time_us) {
            break;
        }
    }

    int64_t elapsed_ms = (esp_timer_get_time() - start) / 1000;
    if (!saw_busy) {
        ESP_LOGW(TAG, "%s: BUSY never asserted (panel may not be receiving SPI)", comment ? comment : "epd");
    }
    ESP_LOGI(TAG, "%s: waited %lld ms (BUSY=%d, saw_busy=%d)",
             comment ? comment : "epd", elapsed_ms,
             gpio_get_level(PIN_EPD_BUSY), saw_busy);
}

static void epd_reset_panel(void)
{
    gpio_set_level(PIN_EPD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_EPD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(EPD_RESET_MS));
    gpio_set_level(PIN_EPD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(EPD_RESET_MS));
    s_hibernating = false;
}

static void epd_set_partial_ram_area(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    y = EPD_NATIVE_HEIGHT - y - h;

    epd_write_command(0x11);
    epd_write_data(0x01);

    epd_write_command(0x44);
    epd_write_data(x % 256);
    epd_write_data(x / 256);
    epd_write_data((x + w - 1) % 256);
    epd_write_data((x + w - 1) / 256);

    epd_write_command(0x45);
    epd_write_data((y + h - 1) % 256);
    epd_write_data((y + h - 1) / 256);
    epd_write_data(y % 256);
    epd_write_data(y / 256);

    epd_write_command(0x4e);
    epd_write_data(x % 256);
    epd_write_data(x / 256);

    epd_write_command(0x4f);
    epd_write_data((y + h - 1) % 256);
    epd_write_data((y + h - 1) / 256);
}

static void epd_write_screen_buffer(uint8_t command, uint8_t value)
{
    epd_set_partial_ram_area(0, 0, EPD_NATIVE_WIDTH, EPD_NATIVE_HEIGHT);
    epd_write_command(command);
    epd_write_data_fill(value, (size_t)EPD_NATIVE_WIDTH * EPD_NATIVE_HEIGHT / 8);
}

static void epd_write_screen_buffer_4g(uint8_t command, uint8_t value)
{
    epd_set_partial_ram_area(0, 0, EPD_NATIVE_WIDTH, EPD_NATIVE_HEIGHT);
    epd_write_command(command);
    epd_write_data_fill(value, EPD_IMAGE_4G_BYTES);
}

static void epd_power_on(void)
{
    if (!s_power_is_on) {
        epd_write_command(0x22);
        epd_write_data(0xc0);
        epd_write_command(0x20);
        epd_wait_busy("_PowerOn", EPD_POWER_ON_MS);
    }
    s_power_is_on = true;
}

static void epd_power_off_internal(void)
{
    if (s_power_is_on) {
        epd_write_command(0x22);
        epd_write_data(0x83);
        epd_write_command(0x20);
        epd_wait_busy("_PowerOff", EPD_POWER_OFF_MS);
    }
    s_power_is_on = false;
}

static void epd_init_display(void)
{
    if (s_hibernating) {
        epd_reset_panel();
    }

    vTaskDelay(pdMS_TO_TICKS(10));
    epd_write_command(0x12);
    vTaskDelay(pdMS_TO_TICKS(10));

    epd_write_command(0x0C);
    epd_write_data(0xAE);
    epd_write_data(0xC7);
    epd_write_data(0xC3);
    epd_write_data(0xC0);
    epd_write_data(0x80);

    epd_write_command(0x01);
    epd_write_data((EPD_NATIVE_HEIGHT - 1) % 256);
    epd_write_data((EPD_NATIVE_HEIGHT - 1) / 256);
    epd_write_data(0x02);

    epd_write_command(0x3C);
    epd_write_data(0x01);

    epd_write_command(0x18);
    epd_write_data(0x80);

    epd_set_partial_ram_area(0, 0, EPD_NATIVE_WIDTH, EPD_NATIVE_HEIGHT);
    s_init_display_done = true;
    s_init_4g_done = false;
    s_refresh_mode = EPD_REFRESH_FULL;
}

static void epd_init_4g(void)
{
    if (s_hibernating) {
        epd_reset_panel();
    }

    vTaskDelay(pdMS_TO_TICKS(10));
    epd_write_command(0x12);
    vTaskDelay(pdMS_TO_TICKS(10));

    epd_write_command(0x0C);
    epd_write_data(0xAE);
    epd_write_data(0xC7);
    epd_write_data(0xC3);
    epd_write_data(0xC0);
    epd_write_data(0x80);

    epd_write_command(0x01);
    epd_write_data((EPD_NATIVE_HEIGHT - 1) % 256);
    epd_write_data((EPD_NATIVE_HEIGHT - 1) / 256);
    epd_write_data(0x02);

    epd_write_command(0x3C);
    epd_write_data(0x00);

    epd_write_command(0x18);
    epd_write_data(0x80);

    epd_set_partial_ram_area(0, 0, EPD_NATIVE_WIDTH, EPD_NATIVE_HEIGHT);

    epd_write_command(0x32);
    epd_write_data_bulk(s_lut_4g, sizeof(s_lut_4g));

    epd_write_command(0x03);
    epd_write_data(s_lut_4g[105]);
    epd_write_command(0x04);
    epd_write_data(s_lut_4g[106]);
    epd_write_data(s_lut_4g[107]);
    epd_write_data(s_lut_4g[108]);
    epd_write_command(0x2C);
    epd_write_data(s_lut_4g[109]);

    epd_write_screen_buffer_4g(0x24, 0x00);
    epd_write_screen_buffer_4g(0x26, 0x00);

    s_initial_write = false;
    s_init_display_done = false;
    s_init_4g_done = true;
    s_refresh_mode = EPD_REFRESH_GREY;
}

static void epd_update_full_mode(bool fast)
{
    if (!s_power_is_on) {
        epd_power_on();
    }

    epd_set_partial_ram_area(0, 0, EPD_NATIVE_WIDTH, EPD_NATIVE_HEIGHT);

    epd_write_command(0x21);
    epd_write_data(0x40);
    epd_write_data(0x00);

    if (fast) {
        epd_write_command(0x1A);
        epd_write_data(0x5A);
        epd_write_command(0x22);
        epd_write_data(0xd7);
    } else {
        epd_write_command(0x22);
        epd_write_data(0xf7);
    }

    epd_write_command(0x20);
    epd_wait_busy(fast ? "_Update_Full" : "_Update_FullSlow", EPD_FULL_REFRESH_MS);
    s_power_is_on = false;
}

static void epd_update_full(void)
{
    epd_update_full_mode(true);
}

static void epd_update_4g(void)
{
    if (!s_power_is_on) {
        epd_power_on();
    }

    epd_set_partial_ram_area(0, 0, EPD_NATIVE_WIDTH, EPD_NATIVE_HEIGHT);

    epd_write_command(0x21);
    epd_write_data(0x00);
    epd_write_data(0x00);
    epd_write_command(0x22);
    epd_write_data(0xc7);
    epd_write_command(0x20);
    epd_wait_busy("_Update_4G", EPD_GREY_REFRESH_MS);
    s_power_is_on = false;
}

static void epd_init_part(void)
{
    epd_init_display();
    epd_power_on();
    s_refresh_mode = EPD_REFRESH_FAST;
}

static void epd_update_part(void)
{
    if (!s_power_is_on) {
        epd_power_on();
    }

    epd_write_command(0x21);
    epd_write_data(0x00);
    epd_write_data(0x00);
    epd_write_command(0x22);
    epd_write_data(0xfc);
    epd_write_command(0x20);
    epd_wait_busy("_Update_Part", EPD_PARTIAL_REFRESH_MS);
    s_power_is_on = true;
}

static void epd_refresh(bool partial_update_mode);

static bool epd_clip_area(int16_t x, int16_t y, int16_t w, int16_t h,
                          int16_t *x1, int16_t *y1, int16_t *w1, int16_t *h1)
{
    int16_t w_clip = x < 0 ? (int16_t)(w + x) : w;
    int16_t h_clip = y < 0 ? (int16_t)(h + y) : h;
    int16_t x_clip = x < 0 ? 0 : x;
    int16_t y_clip = y < 0 ? 0 : y;

    w_clip = (x_clip + w_clip < (int16_t)EPD_NATIVE_WIDTH) ? w_clip : (int16_t)EPD_NATIVE_WIDTH - x_clip;
    h_clip = (y_clip + h_clip < (int16_t)EPD_NATIVE_HEIGHT) ? h_clip : (int16_t)EPD_NATIVE_HEIGHT - y_clip;
    if (w_clip <= 0 || h_clip <= 0) {
        return false;
    }

    w_clip += x_clip % 8;
    if (w_clip % 8 > 0) {
        w_clip = (int16_t)(w_clip + 8 - (w_clip % 8));
    }
    x_clip -= x_clip % 8;

    *x1 = x_clip;
    *y1 = y_clip;
    *w1 = w_clip;
    *h1 = h_clip;
    return true;
}

static void epd_refresh_area(int16_t x, int16_t y, int16_t w, int16_t h)
{
    if (s_initial_refresh) {
        epd_refresh(false);
        return;
    }

    if (s_refresh_mode == EPD_REFRESH_FORCED_FULL) {
        epd_refresh(false);
        return;
    }

    int16_t x1, y1, w1, h1;
    if (!epd_clip_area(x, y, w, h, &x1, &y1, &w1, &h1)) {
        return;
    }

    if (s_refresh_mode == EPD_REFRESH_FULL) {
        epd_init_part();
    }

    epd_set_partial_ram_area((uint16_t)x1, (uint16_t)y1, (uint16_t)w1, (uint16_t)h1);
    if (s_refresh_mode == EPD_REFRESH_GREY) {
        epd_update_4g();
    } else {
        epd_update_part();
    }
}

static void epd_refresh(bool partial_update_mode)
{
    if (partial_update_mode) {
        epd_refresh_area(0, 0, EPD_NATIVE_WIDTH, EPD_NATIVE_HEIGHT);
        return;
    }

    if (s_refresh_mode == EPD_REFRESH_FAST) {
        epd_init_display();
        epd_power_on();
        s_refresh_mode = EPD_REFRESH_FULL;
    } else if (!s_power_is_on) {
        epd_power_on();
    }

    if (s_refresh_mode == EPD_REFRESH_GREY) {
        epd_update_4g();
    } else {
        epd_update_full();
    }
    s_initial_refresh = false;
}

static void epd_ensure_bw_mode(void)
{
    if (!s_init_display_done || s_init_4g_done) {
        epd_init_display();
    }
}

static void epd_write_bw_fill(uint8_t command, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t value)
{
    int16_t x1, y1, w1, h1;
    if (!epd_clip_area(x, y, w, h, &x1, &y1, &w1, &h1)) {
        return;
    }

    epd_ensure_bw_mode();
    epd_set_partial_ram_area((uint16_t)x1, (uint16_t)y1, (uint16_t)w1, (uint16_t)h1);
    epd_write_command(command);
    epd_write_data_fill(value, (size_t)w1 * h1 / 8);
}

/* Port of GxEPD2 _writeImage(), 1bpp */
static void epd_write_image_bw(uint8_t command, const uint8_t *bitmap,
                               int16_t x, int16_t y, int16_t w, int16_t h)
{
    int16_t wb = (w + 7) / 8;
    x -= x % 8;
    w = (int16_t)(wb * 8);

    int16_t x1 = x < 0 ? 0 : x;
    int16_t y1 = y < 0 ? 0 : y;
    uint16_t w1 = x + w < (int16_t)EPD_NATIVE_WIDTH ? (uint16_t)w : (uint16_t)(EPD_NATIVE_WIDTH - x);
    uint16_t h1 = y + h < (int16_t)EPD_NATIVE_HEIGHT ? (uint16_t)h : (uint16_t)(EPD_NATIVE_HEIGHT - y);
    int16_t dx = x1 - x;
    int16_t dy = y1 - y;
    w1 = (uint16_t)(w1 - dx);
    h1 = (uint16_t)(h1 - dy);
    if ((w1 <= 0) || (h1 <= 0)) {
        return;
    }

    epd_ensure_bw_mode();
    if (s_initial_write) {
        epd_write_screen_buffer(0x24, 0xFF);
        s_initial_write = false;
    }

    epd_set_partial_ram_area(x1, y1, w1, h1);
    epd_write_command(command);
    epd_dc_set(1);
    epd_cs_set(0);
    for (uint16_t i = 0; i < h1; i++) {
        for (uint16_t j = 0; j < w1 / 8; j++) {
            uint32_t idx = j + (uint32_t)(dx / 8) + (uint32_t)(i + dy) * (uint32_t)wb;
            uint8_t data = bitmap[idx];
            epd_spi_tx(&data, 1);
        }
        if ((i & 0x0F) == 0) {
            vTaskDelay(1);
        }
    }
    epd_cs_set(1);
}

static void epd_partial_sync_fill(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t value)
{
    epd_write_bw_fill(0x26, x, y, w, h, value);
    epd_write_bw_fill(0x24, x, y, w, h, value);
}

static void epd_partial_draw_fill(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t value)
{
    epd_write_bw_fill(0x24, x, y, w, h, value);
}

/* Encode one plane (0x26 previous or 0x24 current) and stream over SPI */
static void epd_write_image_4g_plane(uint8_t command, const uint8_t *bitmap,
                                     int16_t dx, int16_t dy, int16_t wb,
                                     uint16_t w1, uint16_t h1, bool invert_grey)
{
    const uint8_t bpp = 2;
    const uint16_t ppb = 4;
    const uint8_t mask = 0xC0;
    const uint8_t grey1 = 0x80;
    const uint16_t row_bytes = w1 / 8;
    uint8_t row_buf[EPD_NATIVE_WIDTH / 8];

    epd_write_command(command);
    epd_dc_set(1);
    epd_cs_set(0);
    for (uint16_t i = 0; i < h1; i++) {
        uint16_t out_idx = 0;
        for (uint16_t j = 0; j < w1 / ppb; j += bpp) {
            uint8_t out_byte = 0;
            for (uint16_t k = 0; k < bpp; k++) {
                uint32_t idx = j + k + dx / ppb + (uint32_t)(i + dy) * wb;
                uint8_t in_byte = bitmap[idx];
                for (uint16_t n = 0; n < ppb; n++) {
                    out_byte <<= 1;
                    uint8_t nibble = in_byte & mask;
                    if (nibble == mask) {
                        out_byte |= 0x01;
                    } else if (nibble == 0x00) {
                        out_byte |= 0x00;
                    } else if (nibble >= grey1) {
                        out_byte |= invert_grey ? 0x00 : 0x01;
                    } else {
                        out_byte |= invert_grey ? 0x01 : 0x00;
                    }
                    in_byte <<= bpp;
                }
            }
            row_buf[out_idx++] = (uint8_t)~out_byte;
        }
        epd_spi_tx(row_buf, row_bytes);
        if ((i & 0x0F) == 0) {
            vTaskDelay(1);
        }
    }
    epd_cs_set(1);
}

/* Port of GxEPD2 writeImage_4G(), bpp=2 */
static void epd_write_image_4g(const uint8_t *bitmap, int16_t x, int16_t y, int16_t w, int16_t h)
{
    const uint16_t ppb = 4;

    int16_t wbc = (w + 7) / 8;
    x -= x % 8;
    w = wbc * 8;
    int16_t wb = (w + ppb - 1) / ppb;

    int16_t x1 = x < 0 ? 0 : x;
    int16_t y1 = y < 0 ? 0 : y;
    uint16_t w1 = x + w < (int16_t)EPD_NATIVE_WIDTH ? w : EPD_NATIVE_WIDTH - x;
    uint16_t h1 = y + h < (int16_t)EPD_NATIVE_HEIGHT ? h : EPD_NATIVE_HEIGHT - y;
    int16_t dx = x1 - x;
    int16_t dy = y1 - y;
    w1 -= dx;
    h1 -= dy;
    if ((w1 <= 0) || (h1 <= 0)) {
        return;
    }

    if (!s_init_4g_done) {
        epd_init_4g();
    }

    epd_set_partial_ram_area(x1, y1, w1, h1);
    epd_write_image_4g_plane(0x26, bitmap, dx, dy, wb, w1, h1, false);
    epd_write_image_4g_plane(0x24, bitmap, dx, dy, wb, w1, h1, true);
}

static void epd_buffer_fill_grey(uint8_t *buf, uint8_t brb)
{
    memset(buf, (uint8_t)(brb * 0x55), EPD_BUFFER_BYTES);
}

static void epd_buffer_set_pixel(uint8_t *buf, int16_t x, int16_t y, uint8_t brb)
{
    if (x < 0 || x >= EPD_NATIVE_WIDTH || y < 0 || y >= EPD_PAGE_HEIGHT) {
        return;
    }
    uint16_t i = x / 4 + y * (EPD_NATIVE_WIDTH / 4);
    buf[i] = (uint8_t)(buf[i] & (uint8_t)(0xFF ^ (3 << (2 * (3 - x % 4)))));
    buf[i] = (uint8_t)(buf[i] | (uint8_t)((brb & 0x03) << (2 * (3 - x % 4))));
}

static void epd_buffer_fill_rect(uint8_t *buf, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t brb)
{
    for (int16_t j = 0; j < h; j++) {
        for (int16_t i = 0; i < w; i++) {
            epd_buffer_set_pixel(buf, x + i, y + j, brb);
        }
    }
}

static void epd_page_clip_fill_rect(uint8_t *buf, int page, int page_h,
                                    int16_t x, int16_t y_global, int16_t w, int16_t h, uint8_t brb)
{
    int page_y0 = page * page_h;
    int page_y1 = page_y0 + page_h;
    int clip_y0 = y_global > page_y0 ? y_global : page_y0;
    int clip_y1 = (y_global + h < page_y1) ? y_global + h : page_y1;
    if (clip_y0 >= clip_y1) {
        return;
    }
    epd_buffer_fill_rect(buf, x, clip_y0 - page_y0, w, clip_y1 - clip_y0, brb);
}

static uint8_t s_solid_brb;

static void epd_draw_solid_page(uint8_t *buf, int page, int page_h)
{
    (void)page;
    (void)page_h;
    epd_buffer_fill_grey(buf, s_solid_brb);
}

static void epd_draw_checkerboard_page(uint8_t *buf, int page, int page_h)
{
    const int cell = 40;
    const int rows = (EPD_NATIVE_HEIGHT + cell - 1) / cell;
    const int cols = (EPD_NATIVE_WIDTH + cell - 1) / cell;

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            uint8_t brb = ((row + col) % 2 == 0) ? 0 : 3;
            int x = col * cell;
            int y = row * cell;
            int w = (x + cell > EPD_NATIVE_WIDTH) ? EPD_NATIVE_WIDTH - x : cell;
            int h = (y + cell > EPD_NATIVE_HEIGHT) ? EPD_NATIVE_HEIGHT - y : cell;
            epd_page_clip_fill_rect(buf, page, page_h, x, y, w, h, brb);
        }
    }
}

static void epd_draw_stripes_page(uint8_t *buf, int page, int page_h)
{
    const int stripe = 40;

    for (int x = 0; x < EPD_NATIVE_WIDTH; x += stripe) {
        uint8_t brb = ((x / stripe) % 2 == 0) ? 0 : 3;
        int w = (x + stripe > EPD_NATIVE_WIDTH) ? EPD_NATIVE_WIDTH - x : stripe;
        epd_page_clip_fill_rect(buf, page, page_h, x, 0, w, EPD_NATIVE_HEIGHT, brb);
    }
}

static esp_err_t epd_draw_paged_4g(void (*draw_page)(uint8_t *buf, int page, int page_h))
{
    uint8_t *buf = malloc(EPD_BUFFER_BYTES);
    if (!buf) {
        return ESP_ERR_NO_MEM;
    }

    for (int page = 0; page < 2; page++) {
        epd_buffer_fill_grey(buf, 3);
        if (draw_page) {
            draw_page(buf, page, EPD_PAGE_HEIGHT);
        }
        int16_t y = page * EPD_PAGE_HEIGHT;
        int16_t h = (page == 1) ? (EPD_NATIVE_HEIGHT - EPD_PAGE_HEIGHT) : EPD_PAGE_HEIGHT;
        epd_write_image_4g(buf, 0, y, EPD_NATIVE_WIDTH, h);
    }

    free(buf);
    epd_refresh(false);
    return ESP_OK;
}

esp_err_t epd_ssd1677_init(void)
{
    ESP_LOGI(TAG, "Init SPI/GPIO (same as display.init + selectSPI)");

    const gpio_num_t pins[] = {
        PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_CS, PIN_EPD_BUSY, PIN_EPD_SCK, PIN_EPD_MOSI,
    };
    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
        esp_rom_gpio_pad_select_gpio(pins[i]);
    }

    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << PIN_EPD_CS) | (1ULL << PIN_EPD_DC) | (1ULL << PIN_EPD_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&out_cfg), TAG, "gpio output config failed");

    gpio_config_t in_cfg = {
        .pin_bit_mask = (1ULL << PIN_EPD_BUSY),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&in_cfg), TAG, "gpio input config failed");

    epd_cs_set(1);
    epd_dc_set(1);
    epd_reset_panel();

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_EPD_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_EPD_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = (EPD_NATIVE_WIDTH / 8) * EPD_PAGE_HEIGHT,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(EPD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO), TAG, "spi bus init failed");

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = EPD_SPI_FREQ_HZ,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 1,
    };
    ESP_RETURN_ON_ERROR(spi_bus_add_device(EPD_SPI_HOST, &dev_cfg, &s_spi), TAG, "spi add device failed");

    s_init_display_done = false;
    s_init_4g_done = false;
    s_power_is_on = false;
    s_hibernating = false;
    s_initial_write = true;
    s_initial_refresh = true;
    s_refresh_mode = EPD_REFRESH_FULL;

    ESP_LOGI(TAG, "BUSY idle level = %d", gpio_get_level(PIN_EPD_BUSY));
    return ESP_OK;
}

esp_err_t epd_ssd1677_clear(uint8_t value)
{
    ESP_LOGI(TAG, "clearScreen(0x%02x)", value);

    if (!s_init_display_done) {
        epd_init_display();
    }

    epd_write_screen_buffer(0x26, value);
    epd_write_screen_buffer(0x24, value);
    s_refresh_mode = EPD_REFRESH_FULL;
    epd_refresh(false);
    s_initial_write = false;
    s_initial_refresh = false;
    return ESP_OK;
}

esp_err_t epd_ssd1677_purge_ghost(void)
{
    ESP_LOGI(TAG, "purge partial ghost (B/W solid black -> white, full screen)");

    ESP_RETURN_ON_ERROR(epd_ssd1677_show_solid_bw(0x00), TAG, "purge black failed");
    ESP_RETURN_ON_ERROR(epd_ssd1677_show_solid_bw(0xFF), TAG, "purge white failed");

    s_init_4g_done = false;
    return ESP_OK;
}

esp_err_t epd_ssd1677_clear_4g(uint8_t brb)
{
    const uint8_t pat = (uint8_t)((brb & 0x03) * 0x55);

    ESP_LOGI(TAG, "clear_4g level %u (4G full refresh)", brb & 0x03);

    if (!s_init_4g_done) {
        epd_init_4g();
    }

    epd_write_screen_buffer_4g(0x26, pat);
    epd_write_screen_buffer_4g(0x24, pat);

    s_refresh_mode = EPD_REFRESH_GREY;
    epd_update_4g();

    s_initial_write = false;
    s_initial_refresh = false;
    return ESP_OK;
}

esp_err_t epd_ssd1677_fill(uint8_t brb)
{
    ESP_LOGI(TAG, "fill grey level %u (4G)", brb);
    s_solid_brb = brb & 0x03;
    return epd_draw_paged_4g(epd_draw_solid_page);
}

esp_err_t epd_ssd1677_show_checkerboard(void)
{
    ESP_LOGI(TAG, "checkerboard pattern");
    return epd_draw_paged_4g(epd_draw_checkerboard_page);
}

esp_err_t epd_ssd1677_show_stripes(void)
{
    ESP_LOGI(TAG, "vertical stripes");
    return epd_draw_paged_4g(epd_draw_stripes_page);
}

esp_err_t epd_ssd1677_show_image_4g(const uint8_t *bitmap)
{
    if (!bitmap) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "show 4G image (%dx%d)", EPD_NATIVE_WIDTH, EPD_NATIVE_HEIGHT);

    epd_write_image_4g(bitmap, 0, 0, EPD_NATIVE_WIDTH, EPD_PAGE_HEIGHT);
    epd_write_image_4g(bitmap + EPD_BUFFER_BYTES, 0, EPD_PAGE_HEIGHT, EPD_NATIVE_WIDTH, EPD_PAGE_HEIGHT);

    epd_refresh(false);
    s_initial_write = false;
    s_initial_refresh = false;
    return ESP_OK;
}

esp_err_t epd_ssd1677_show_image_bw(const uint8_t *bitmap)
{
    if (!bitmap) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "show B/W image (%dx%d)", EPD_NATIVE_WIDTH, EPD_NATIVE_HEIGHT);

    epd_ensure_bw_mode();
    epd_write_image_bw(0x26, bitmap, 0, 0, EPD_NATIVE_WIDTH, EPD_NATIVE_HEIGHT);
    epd_write_image_bw(0x24, bitmap, 0, 0, EPD_NATIVE_WIDTH, EPD_NATIVE_HEIGHT);

    s_refresh_mode = EPD_REFRESH_FULL;
    epd_refresh(false);
    s_initial_write = false;
    s_initial_refresh = false;
    return ESP_OK;
}

esp_err_t epd_ssd1677_show_solid_bw(uint8_t value)
{
    uint8_t *buf = malloc(EPD_IMAGE_BW_BYTES);
    if (!buf) {
        return ESP_ERR_NO_MEM;
    }

    memset(buf, value, EPD_IMAGE_BW_BYTES);
    esp_err_t err = epd_ssd1677_show_image_bw(buf);
    free(buf);
    return err;
}

static esp_err_t epd_show_framebuffer_4g(uint8_t *fb)
{
    esp_err_t err = epd_ssd1677_show_image_4g(fb);
    free(fb);
    return err;
}

/* Bayer 4x4 ordered dither thresholds (0..15). */
static const uint8_t s_bayer4x4[16] = {
    0, 8, 2, 10,
    12, 4, 14, 6,
    3, 11, 1, 9,
    15, 7, 13, 5,
};

static void epd_fb_set_pixel_4g(uint8_t *fb, int16_t x, int16_t y, uint8_t brb)
{
    uint32_t i = (uint32_t)(x / 4) + (uint32_t)y * (EPD_NATIVE_WIDTH / 4);
    uint8_t shift = (uint8_t)(2 * (3 - x % 4));
    fb[i] = (uint8_t)(fb[i] & (uint8_t)~(3u << shift));
    fb[i] = (uint8_t)(fb[i] | (uint8_t)((brb & 0x03) << shift));
}

static void epd_fb_fill_band(uint8_t *fb, int16_t y, int16_t h, uint8_t brb)
{
    const uint8_t pat = (uint8_t)(brb * 0x55);
    const size_t row_bytes = EPD_NATIVE_WIDTH / 4;

    for (int16_t row = 0; row < h; row++) {
        memset(&fb[(size_t)(y + row) * row_bytes], pat, row_bytes);
    }
}

static uint8_t epd_dither_grey4_bayer(int16_t x, int16_t y, float target)
{
    if (target <= 0.0f) {
        return 0;
    }
    if (target >= 3.0f) {
        return 3;
    }

    int base = (int)target;
    float frac = target - (float)base;
    uint8_t th = s_bayer4x4[(y & 3) * 4 + (x & 3)];
    if (frac > (float)th / 16.0f) {
        base++;
    }
    if (base > 3) {
        base = 3;
    }
    return (uint8_t)base;
}

esp_err_t epd_ssd1677_show_grey4_levels(void)
{
    const int bands = 4;
    const int band_h = EPD_NATIVE_HEIGHT / bands;
    static const uint8_t levels[4] = {3, 2, 1, 0};

    uint8_t *fb = malloc(EPD_IMAGE_4G_BYTES);
    if (!fb) {
        return ESP_ERR_NO_MEM;
    }

    for (int b = 0; b < bands; b++) {
        int16_t y0 = (int16_t)(b * band_h);
        int16_t h = (b == bands - 1) ? (int16_t)(EPD_NATIVE_HEIGHT - y0) : (int16_t)band_h;
        epd_fb_fill_band(fb, y0, h, levels[b]);
    }

    ESP_LOGI(TAG, "show 4-grey level bands (4G full refresh)");
    return epd_show_framebuffer_4g(fb);
}

esp_err_t epd_ssd1677_show_grey16_stripes(void)
{
    const int stripes = 16;
    const int stripe_w = EPD_NATIVE_WIDTH / stripes;

    uint8_t *fb = malloc(EPD_IMAGE_4G_BYTES);
    if (!fb) {
        return ESP_ERR_NO_MEM;
    }

    memset(fb, 0xFF, EPD_IMAGE_4G_BYTES);

    for (int s = 0; s < stripes; s++) {
        float target = (float)s * 3.0f / (float)(stripes - 1);
        int16_t x0 = (int16_t)(s * stripe_w);
        int16_t x1 = (s == stripes - 1) ? EPD_NATIVE_WIDTH : (int16_t)((s + 1) * stripe_w);

        for (int16_t y = 0; y < EPD_NATIVE_HEIGHT; y++) {
            for (int16_t x = x0; x < x1; x++) {
                uint8_t level = epd_dither_grey4_bayer(x, y, target);
                epd_fb_set_pixel_4g(fb, x, y, level);
            }
        }
    }

    ESP_LOGI(TAG, "show 16-grey dither stripes (4G, %d vertical bands)", stripes);
    return epd_show_framebuffer_4g(fb);
}

esp_err_t epd_ssd1677_show_text_4g(const char *text, int16_t x, int16_t y, uint8_t grey, uint8_t scale)
{
    if (!text) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t *fb = malloc(EPD_IMAGE_4G_BYTES);
    if (!fb) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = epd_text_compose(fb, EPD_IMAGE_4G_BYTES, NULL, text, x, y, grey, scale);
    if (err != ESP_OK) {
        free(fb);
        ESP_RETURN_ON_ERROR(err, TAG, "compose text failed");
    }
    return epd_show_framebuffer_4g(fb);
}

esp_err_t epd_ssd1677_show_image_with_text_4g(const uint8_t *bg, const char *text,
                                                  int16_t x, int16_t y, uint8_t grey, uint8_t scale)
{
    if (!bg || !text) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t *fb = malloc(EPD_IMAGE_4G_BYTES);
    if (!fb) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = epd_text_compose(fb, EPD_IMAGE_4G_BYTES, bg, text, x, y, grey, scale);
    if (err != ESP_OK) {
        free(fb);
        ESP_RETURN_ON_ERROR(err, TAG, "compose image+text failed");
    }
    return epd_show_framebuffer_4g(fb);
}

esp_err_t epd_ssd1677_show_text_partial(const char *text, int16_t x, int16_t y, uint8_t scale)
{
    return epd_text_show_partial(text, x, y, scale);
}

esp_err_t epd_ssd1677_show_text_partial_term(const char *text, int16_t x, int16_t y, uint8_t scale)
{
    return epd_text_show_partial_term(text, x, y, scale);
}

esp_err_t epd_ssd1677_show_char_partial_term(char c, int16_t x, int16_t y, uint8_t scale)
{
    return epd_text_show_char_partial_term(c, x, y, scale);
}

esp_err_t epd_ssd1677_refresh_area(int16_t x, int16_t y, int16_t w, int16_t h)
{
    epd_refresh_area(x, y, w, h);
    return ESP_OK;
}

esp_err_t epd_ssd1677_write_bw_fill(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t value)
{
    if (s_refresh_mode == EPD_REFRESH_GREY) {
        epd_write_bw_fill(0x26, x, y, w, h, value);
    }
    epd_write_bw_fill(0x24, x, y, w, h, value);
    return ESP_OK;
}

esp_err_t epd_ssd1677_write_bw_fill_again(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t value)
{
    epd_partial_sync_fill(x, y, w, h, value);
    return ESP_OK;
}

esp_err_t epd_ssd1677_write_image_bw(const uint8_t *bitmap, int16_t x, int16_t y, int16_t w, int16_t h)
{
    if (!bitmap) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_refresh_mode == EPD_REFRESH_GREY) {
        epd_write_image_bw(0x26, bitmap, x, y, w, h);
    }
    epd_write_image_bw(0x24, bitmap, x, y, w, h);
    return ESP_OK;
}

esp_err_t epd_ssd1677_write_image_bw_again(const uint8_t *bitmap, int16_t x, int16_t y, int16_t w, int16_t h)
{
    if (!bitmap) {
        return ESP_ERR_INVALID_ARG;
    }
    epd_write_image_bw(0x26, bitmap, x, y, w, h);
    epd_write_image_bw(0x24, bitmap, x, y, w, h);
    return ESP_OK;
}

esp_err_t epd_ssd1677_show_partial_demo(void)
{
    const int16_t margin = 60;
    const int16_t sq_big = 72;
    const int16_t sq_small = 40;
    const int16_t band_y = 36;
    const int16_t band_h = 10;
    const int frames = 50;

    ESP_LOGI(TAG, "partial demo: white baseline");
    ESP_RETURN_ON_ERROR(epd_ssd1677_clear(0xFF), TAG, "baseline white failed");

    ESP_LOGI(TAG, "partial demo: draw title band");
    epd_partial_draw_fill(margin, band_y, (int16_t)(EPD_NATIVE_WIDTH - 2 * margin), band_h, 0x00);
    epd_refresh_area(margin, band_y, (int16_t)(EPD_NATIVE_WIDTH - 2 * margin), band_h);
    epd_partial_sync_fill(margin, band_y, (int16_t)(EPD_NATIVE_WIDTH - 2 * margin), band_h, 0x00);

    int16_t x_big = margin;
    int16_t x_small = (int16_t)(EPD_NATIVE_WIDTH - margin - sq_small);
    const int16_t y_big = (int16_t)(EPD_NATIVE_HEIGHT / 2 - sq_big / 2 - 20);
    const int16_t y_small = (int16_t)(EPD_NATIVE_HEIGHT / 2 - sq_small / 2 + 30);
    int16_t dx_big = 20;
    int16_t dx_small = -20;
    const int16_t x_max_big = (int16_t)(EPD_NATIVE_WIDTH - margin - sq_big);
    const int16_t x_max_small = (int16_t)(EPD_NATIVE_WIDTH - margin - sq_small);

    ESP_LOGI(TAG, "partial demo: dual squares %d frames", frames);
    for (int i = 0; i < frames; i++) {
        int16_t old_x_big = x_big;
        int16_t old_x_small = x_small;

        x_big = (int16_t)(x_big + dx_big);
        x_small = (int16_t)(x_small + dx_small);
        if (x_big <= margin || x_big >= x_max_big) {
            dx_big = (int16_t)-dx_big;
            x_big = (int16_t)(x_big + dx_big);
        }
        if (x_small <= margin || x_small >= x_max_small) {
            dx_small = (int16_t)-dx_small;
            x_small = (int16_t)(x_small + dx_small);
        }

        epd_partial_draw_fill(old_x_big, y_big, sq_big, sq_big, 0xFF);
        epd_partial_draw_fill(old_x_small, y_small, sq_small, sq_small, 0xFF);
        epd_partial_draw_fill(x_big, y_big, sq_big, sq_big, 0x00);
        epd_partial_draw_fill(x_small, y_small, sq_small, sq_small, 0x00);

        int16_t rx = old_x_big;
        if (x_big < rx) {
            rx = x_big;
        }
        if (old_x_small < rx) {
            rx = old_x_small;
        }
        if (x_small < rx) {
            rx = x_small;
        }

        int16_t rx_end = (int16_t)(old_x_big + sq_big);
        if (x_big + sq_big > rx_end) {
            rx_end = (int16_t)(x_big + sq_big);
        }
        if (old_x_small + sq_small > rx_end) {
            rx_end = (int16_t)(old_x_small + sq_small);
        }
        if (x_small + sq_small > rx_end) {
            rx_end = (int16_t)(x_small + sq_small);
        }

        int16_t ry = y_big;
        if (y_small < ry) {
            ry = y_small;
        }
        int16_t ry_end = (int16_t)(y_big + sq_big);
        if (y_small + sq_small > ry_end) {
            ry_end = (int16_t)(y_small + sq_small);
        }

        epd_refresh_area(rx, ry, (int16_t)(rx_end - rx), (int16_t)(ry_end - ry));

        epd_partial_sync_fill(old_x_big, y_big, sq_big, sq_big, 0xFF);
        epd_partial_sync_fill(old_x_small, y_small, sq_small, sq_small, 0xFF);
        epd_partial_sync_fill(x_big, y_big, sq_big, sq_big, 0x00);
        epd_partial_sync_fill(x_small, y_small, sq_small, sq_small, 0x00);

        vTaskDelay(pdMS_TO_TICKS(80));
    }

    ESP_LOGI(TAG, "partial demo: hold final frame");
    vTaskDelay(pdMS_TO_TICKS(3000));
    return ESP_OK;
}

esp_err_t epd_ssd1677_power_off(void)
{
    epd_power_off_internal();
    return ESP_OK;
}

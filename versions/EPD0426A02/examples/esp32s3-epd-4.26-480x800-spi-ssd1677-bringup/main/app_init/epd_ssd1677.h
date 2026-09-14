#pragma once

#include <stdint.h>
#include "esp_err.h"

#define EPD_NATIVE_WIDTH   800
#define EPD_NATIVE_HEIGHT  480

/* Portrait logical size (rotation 3, 480x800 upright on GDEQ0426T82). */
#define EPD_WIDTH   480
#define EPD_HEIGHT  800
#define EPD_PAGE_HEIGHT (EPD_NATIVE_HEIGHT / 2)
#define EPD_IMAGE_4G_BYTES ((EPD_NATIVE_WIDTH * EPD_NATIVE_HEIGHT) / 4)

#define EPD_IMAGE_BW_BYTES ((EPD_NATIVE_WIDTH * EPD_NATIVE_HEIGHT) / 8)

esp_err_t epd_ssd1677_init(void);
esp_err_t epd_ssd1677_clear(uint8_t value);
esp_err_t epd_ssd1677_clear_4g(uint8_t brb);
esp_err_t epd_ssd1677_purge_ghost(void);
esp_err_t epd_ssd1677_fill(uint8_t brb);
esp_err_t epd_ssd1677_show_checkerboard(void);
esp_err_t epd_ssd1677_show_stripes(void);
esp_err_t epd_ssd1677_show_image_4g(const uint8_t *bitmap);
esp_err_t epd_ssd1677_show_image_bw(const uint8_t *bitmap);
esp_err_t epd_ssd1677_show_solid_bw(uint8_t value);
esp_err_t epd_ssd1677_show_grey4_levels(void);
esp_err_t epd_ssd1677_show_grey16_stripes(void);
esp_err_t epd_ssd1677_show_text_4g(const char *text, int16_t x, int16_t y, uint8_t grey, uint8_t scale);
esp_err_t epd_ssd1677_show_text_partial(const char *text, int16_t x, int16_t y, uint8_t scale);
esp_err_t epd_ssd1677_show_text_partial_term(const char *text, int16_t x, int16_t y, uint8_t scale);
esp_err_t epd_ssd1677_show_char_partial_term(char c, int16_t x, int16_t y, uint8_t scale);
esp_err_t epd_ssd1677_show_image_with_text_4g(const uint8_t *bg, const char *text,
                                                  int16_t x, int16_t y, uint8_t grey, uint8_t scale);
esp_err_t epd_ssd1677_refresh_area(int16_t x, int16_t y, int16_t w, int16_t h);
esp_err_t epd_ssd1677_write_bw_fill(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t value);
esp_err_t epd_ssd1677_write_bw_fill_again(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t value);
esp_err_t epd_ssd1677_write_image_bw(const uint8_t *bitmap, int16_t x, int16_t y, int16_t w, int16_t h);
esp_err_t epd_ssd1677_write_image_bw_again(const uint8_t *bitmap, int16_t x, int16_t y, int16_t w, int16_t h);
esp_err_t epd_ssd1677_show_partial_demo(void);
esp_err_t epd_ssd1677_power_off(void);

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#define EPD_TEXT_SCALE_DEFAULT  1

void epd_fb_fill_grey(uint8_t *fb, uint8_t grey);
void epd_fb_copy_image(uint8_t *fb, const uint8_t *src);
void epd_fb_draw_char(uint8_t *fb, int16_t x, int16_t y, char c, uint8_t grey, uint8_t scale);
void epd_fb_draw_string(uint8_t *fb, int16_t x, int16_t y, const char *text, uint8_t grey, uint8_t scale);

esp_err_t epd_text_compose(uint8_t *fb, size_t fb_len, const uint8_t *bg,
                           const char *text, int16_t x, int16_t y,
                           uint8_t grey, uint8_t scale);

void epd_text_measure(const char *text, uint8_t scale, int16_t *out_w, int16_t *out_h);
esp_err_t epd_text_show_partial(const char *text, int16_t x, int16_t y, uint8_t scale);
esp_err_t epd_text_show_partial_term(const char *text, int16_t x, int16_t y, uint8_t scale);
esp_err_t epd_text_show_char_partial_term(char c, int16_t x, int16_t y, uint8_t scale);
int16_t epd_text_term_advance(uint8_t scale);
esp_err_t epd_text_clear_partial(int16_t x, int16_t y, int16_t w, int16_t h);

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "epd_ssd1677.h"
#include "epd_text.h"
#include "font32x32_bold.h"

typedef enum {
    EPD_TEXT_FONT_LARGE,
} epd_text_font_id_t;

typedef struct {
    epd_text_font_id_t id;
    uint8_t w;
    uint8_t h;
    uint8_t spacing;
    uint8_t advance;   /* 0: use w + spacing */
    uint8_t line_gap;
} epd_text_font_t;

static const epd_text_font_t s_font_large = {
    .id = EPD_TEXT_FONT_LARGE,
    .w = EPD_FONT_W,
    .h = EPD_FONT_H,
    .spacing = EPD_FONT_SPACING,
    .advance = 0,
    .line_gap = 0,
};

/* Same 32x32 glyphs as logo, tighter horizontal pitch for terminal output. */
static const epd_text_font_t s_font_term = {
    .id = EPD_TEXT_FONT_LARGE,
    .w = EPD_FONT_W,
    .h = EPD_FONT_H,
    .spacing = 0,
    .advance = 18,
    .line_gap = 8,
};

static const uint32_t *epd_font_glyph(const epd_text_font_t *font, uint8_t code)
{
    if (!font || code >= 128) {
        return NULL;
    }
    switch (font->id) {
    case EPD_TEXT_FONT_LARGE:
        return epd_font_bold[code];
    default:
        return NULL;
    }
}

static int16_t epd_font_advance_for(const epd_text_font_t *font, uint8_t scale)
{
    if (font->advance > 0) {
        return (int16_t)(font->advance * scale);
    }
    return (int16_t)(font->w * scale + font->spacing);
}

static int16_t epd_font_line_step_for(const epd_text_font_t *font, uint8_t scale)
{
    return (int16_t)(font->h * scale + font->line_gap);
}

static int16_t epd_font_advance(uint8_t scale)
{
    return epd_font_advance_for(&s_font_large, scale);
}

static int16_t epd_font_line_step(uint8_t scale)
{
    return epd_font_line_step_for(&s_font_large, scale);
}

static void epd_font_draw_glyph_4g(uint8_t *fb, int16_t x, int16_t y, char c,
                                   uint8_t grey, uint8_t scale)
{
    uint8_t code = (uint8_t)c;
    if (code >= 128 || scale == 0) {
        return;
    }

    const uint32_t *glyph = epd_font_bold[code];
    for (uint8_t row = 0; row < EPD_FONT_H; row++) {
        uint32_t bits = glyph[row];
        for (uint8_t col = 0; col < EPD_FONT_W; col++) {
            if (bits & (0x80000000u >> col)) {
                if (scale == 1) {
                    uint16_t i = (uint16_t)((x + col) / 4 + (y + row) * (EPD_NATIVE_WIDTH / 4));
                    uint8_t shift = (uint8_t)(2 * (3 - ((x + col) % 4)));
                    fb[i] = (uint8_t)(fb[i] & (uint8_t)(0xFF ^ (3u << shift)));
                    fb[i] = (uint8_t)(fb[i] | (uint8_t)((grey & 0x03) << shift));
                } else {
                    for (uint8_t sy = 0; sy < scale; sy++) {
                        for (uint8_t sx = 0; sx < scale; sx++) {
                            int16_t px = (int16_t)(x + col * scale + sx);
                            int16_t py = (int16_t)(y + row * scale + sy);
                            if (px < 0 || px >= EPD_NATIVE_WIDTH || py < 0 || py >= EPD_NATIVE_HEIGHT) {
                                continue;
                            }
                            uint16_t i = (uint16_t)(px / 4 + py * (EPD_NATIVE_WIDTH / 4));
                            uint8_t shift = (uint8_t)(2 * (3 - (px % 4)));
                            fb[i] = (uint8_t)(fb[i] & (uint8_t)(0xFF ^ (3u << shift)));
                            fb[i] = (uint8_t)(fb[i] | (uint8_t)((grey & 0x03) << shift));
                        }
                    }
                }
            }
        }
    }
}

static void epd_font_draw_glyph_bw_ex(uint8_t *buf, int16_t buf_w, int16_t x, int16_t y,
                                      const epd_text_font_t *font, char c, uint8_t scale)
{
    uint8_t code = (uint8_t)c;
    if (code >= 128 || scale == 0 || !buf || !font) {
        return;
    }

    const uint32_t *glyph = epd_font_glyph(font, code);
    if (!glyph) {
        return;
    }

    int16_t row_bytes = (int16_t)((buf_w + 7) / 8);

    for (uint8_t row = 0; row < font->h; row++) {
        uint32_t bits = glyph[row];
        for (uint8_t col = 0; col < font->w; col++) {
            if (bits & (0x80000000u >> col)) {
                if (scale == 1) {
                    int16_t px = (int16_t)(x + col);
                    int16_t py = (int16_t)(y + row);
                    if (px >= 0 && px < buf_w && py >= 0) {
                        uint16_t idx = (uint16_t)(px / 8 + py * row_bytes);
                        uint8_t mask = (uint8_t)(0x80u >> (px % 8));
                        buf[idx] &= (uint8_t)~mask;
                    }
                } else {
                    for (uint8_t sy = 0; sy < scale; sy++) {
                        for (uint8_t sx = 0; sx < scale; sx++) {
                            int16_t px = (int16_t)(x + col * scale + sx);
                            int16_t py = (int16_t)(y + row * scale + sy);
                            if (px >= 0 && px < buf_w && py >= 0) {
                                uint16_t idx = (uint16_t)(px / 8 + py * row_bytes);
                                uint8_t mask = (uint8_t)(0x80u >> (px % 8));
                                buf[idx] &= (uint8_t)~mask;
                            }
                        }
                    }
                }
            }
        }
    }
}

static void epd_font_draw_string_bw_ex(uint8_t *buf, int16_t buf_w, int16_t x, int16_t y,
                                       const char *text, uint8_t scale, const epd_text_font_t *font)
{
    if (!buf || !text || scale == 0 || !font) {
        return;
    }

    int16_t cursor_x = x;
    const int16_t advance = epd_font_advance_for(font, scale);
    const int16_t line_step = epd_font_line_step_for(font, scale);

    for (const char *p = text; *p != '\0'; p++) {
        if (*p == '\n') {
            cursor_x = x;
            y = (int16_t)(y + line_step);
            continue;
        }
        epd_font_draw_glyph_bw_ex(buf, buf_w, cursor_x, y, font, *p, scale);
        cursor_x = (int16_t)(cursor_x + advance);
    }
}

static void epd_font_draw_string_4g(uint8_t *fb, int16_t x, int16_t y,
                                    const char *text, uint8_t grey, uint8_t scale)
{
    if (!fb || !text || scale == 0) {
        return;
    }

    int16_t cursor_x = x;
    const int16_t advance = epd_font_advance(scale);
    const int16_t line_step = epd_font_line_step(scale);

    for (const char *p = text; *p != '\0'; p++) {
        if (*p == '\n') {
            cursor_x = x;
            y = (int16_t)(y + line_step);
            continue;
        }
        epd_font_draw_glyph_4g(fb, cursor_x, y, *p, grey, scale);
        cursor_x = (int16_t)(cursor_x + advance);
    }
}

void epd_fb_fill_grey(uint8_t *fb, uint8_t grey)
{
    memset(fb, (uint8_t)((grey & 0x03) * 0x55), EPD_IMAGE_4G_BYTES);
}

void epd_fb_copy_image(uint8_t *fb, const uint8_t *src)
{
    if (fb && src) {
        memcpy(fb, src, EPD_IMAGE_4G_BYTES);
    }
}

void epd_fb_draw_char(uint8_t *fb, int16_t x, int16_t y, char c, uint8_t grey, uint8_t scale)
{
    epd_font_draw_glyph_4g(fb, x, y, c, grey, scale);
}

void epd_fb_draw_string(uint8_t *fb, int16_t x, int16_t y, const char *text, uint8_t grey, uint8_t scale)
{
    epd_font_draw_string_4g(fb, x, y, text, grey, scale);
}

esp_err_t epd_text_compose(uint8_t *fb, size_t fb_len, const uint8_t *bg,
                           const char *text, int16_t x, int16_t y,
                           uint8_t grey, uint8_t scale)
{
    if (!fb || fb_len < EPD_IMAGE_4G_BYTES || !text) {
        return ESP_ERR_INVALID_ARG;
    }
    if (scale == 0 || scale > 4) {
        return ESP_ERR_INVALID_ARG;
    }

    if (bg) {
        epd_fb_copy_image(fb, bg);
    } else {
        epd_fb_fill_grey(fb, 3);
    }

    epd_font_draw_string_4g(fb, x, y, text, grey, scale);
    return ESP_OK;
}

static void epd_text_measure_font(const char *text, uint8_t scale, const epd_text_font_t *font,
                                  int16_t *out_w, int16_t *out_h)
{
    int16_t max_w = 0;
    int16_t line_w = 0;
    int16_t lines = 1;

    if (!text || scale == 0 || !font) {
        if (out_w) {
            *out_w = 0;
        }
        if (out_h) {
            *out_h = 0;
        }
        return;
    }

    const int16_t advance = epd_font_advance_for(font, scale);
    const int16_t glyph_w = (int16_t)(font->w * scale);
    const int16_t line_step = epd_font_line_step_for(font, scale);
    int16_t line_chars = 0;

    for (const char *p = text; *p != '\0'; p++) {
        if (*p == '\n') {
            if (line_chars > 0) {
                line_w = (int16_t)((line_chars - 1) * advance + glyph_w);
            }
            if (line_w > max_w) {
                max_w = line_w;
            }
            line_w = 0;
            line_chars = 0;
            lines++;
            continue;
        }
        line_chars++;
    }
    if (line_chars > 0) {
        line_w = (int16_t)((line_chars - 1) * advance + glyph_w);
    }
    if (line_w > max_w) {
        max_w = line_w;
    }

    if (out_w) {
        *out_w = max_w;
    }
    if (out_h) {
        *out_h = (int16_t)(lines * line_step - font->line_gap);
    }
}

void epd_text_measure(const char *text, uint8_t scale, int16_t *out_w, int16_t *out_h)
{
    epd_text_measure_font(text, scale, &s_font_large, out_w, out_h);
}

static bool epd_bw_pixel_is_ink(const uint8_t *buf, int16_t buf_w, int16_t x, int16_t y)
{
    int16_t row_bytes = (int16_t)((buf_w + 7) / 8);
    uint16_t idx = (uint16_t)(x / 8 + y * row_bytes);
    uint8_t mask = (uint8_t)(0x80u >> (x % 8));
    return (buf[idx] & mask) == 0;
}

static void epd_bw_pixel_set_ink(uint8_t *buf, int16_t buf_w, int16_t x, int16_t y)
{
    int16_t row_bytes = (int16_t)((buf_w + 7) / 8);
    uint16_t idx = (uint16_t)(x / 8 + y * row_bytes);
    uint8_t mask = (uint8_t)(0x80u >> (x % 8));
    buf[idx] &= (uint8_t)~mask;
}

/* GxEPD2 rotation 3: logical portrait (lx,ly) -> native (ly, 480-lx-1). */
static void epd_bw_rotate_r3(const uint8_t *src, int16_t src_w, int16_t src_h,
                             uint8_t *dst, int16_t dst_w, int16_t dst_h)
{
    for (int16_t sy = 0; sy < src_h; sy++) {
        for (int16_t sx = 0; sx < src_w; sx++) {
            if (epd_bw_pixel_is_ink(src, src_w, sx, sy)) {
                int16_t dx = sy;
                int16_t dy = (int16_t)(src_w - sx - 1);
                if (dx >= 0 && dx < dst_w && dy >= 0 && dy < dst_h) {
                    epd_bw_pixel_set_ink(dst, dst_w, dx, dy);
                }
            }
        }
    }
}

static void epd_logical_rect_to_native(int16_t lx, int16_t ly, int16_t lw, int16_t lh,
                                       int16_t *nx, int16_t *ny, int16_t *nw, int16_t *nh)
{
    *nx = ly;
    *ny = (int16_t)(EPD_NATIVE_HEIGHT - lx - lw);
    *nw = lh;
    *nh = lw;
}

static void epd_bw_align_area(int16_t x, int16_t y, int16_t w, int16_t h,
                              int16_t *out_x, int16_t *out_y, int16_t *out_w, int16_t *out_h)
{
    int16_t ax = (int16_t)(x - (x % 8));
    int16_t aw = (int16_t)(w + (x - ax));
    if (aw % 8 != 0) {
        aw = (int16_t)(aw + 8 - (aw % 8));
    }
    *out_x = ax;
    *out_y = y;
    *out_w = aw;
    *out_h = h;
}

static esp_err_t epd_text_show_partial_font(const char *text, int16_t x, int16_t y, uint8_t scale,
                                            const epd_text_font_t *font)
{
    if (!text || !font) {
        return ESP_ERR_INVALID_ARG;
    }
    if (scale == 0 || scale > 4) {
        return ESP_ERR_INVALID_ARG;
    }

    int16_t text_w = 0;
    int16_t text_h = 0;
    epd_text_measure_font(text, scale, font, &text_w, &text_h);
    if (text_w <= 0 || text_h <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int16_t rx, ry, rw, rh;
    epd_bw_align_area(x, y, text_w, text_h, &rx, &ry, &rw, &rh);
    if (rx + rw > EPD_WIDTH) {
        rw = (int16_t)(EPD_WIDTH - rx);
    }
    if (ry + rh > EPD_HEIGHT) {
        rh = (int16_t)(EPD_HEIGHT - ry);
    }
    if (rw <= 0 || rh <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t logical_bytes = (size_t)((rw + 7) / 8) * (size_t)rh;
    uint8_t *logical_bitmap = malloc(logical_bytes);
    if (!logical_bitmap) {
        return ESP_ERR_NO_MEM;
    }
    memset(logical_bitmap, 0xFF, logical_bytes);

    epd_font_draw_string_bw_ex(logical_bitmap, rw, (int16_t)(x - rx), (int16_t)(y - ry),
                               text, scale, font);

    int16_t nx, ny, nw, nh;
    epd_logical_rect_to_native(rx, ry, rw, rh, &nx, &ny, &nw, &nh);

    size_t native_bytes = (size_t)((nw + 7) / 8) * (size_t)nh;
    uint8_t *native_bitmap = malloc(native_bytes);
    if (!native_bitmap) {
        free(logical_bitmap);
        return ESP_ERR_NO_MEM;
    }
    memset(native_bitmap, 0xFF, native_bytes);
    epd_bw_rotate_r3(logical_bitmap, rw, rh, native_bitmap, nw, nh);

    esp_err_t err = epd_ssd1677_write_image_bw(native_bitmap, nx, ny, nw, nh);
    if (err == ESP_OK) {
        err = epd_ssd1677_refresh_area(nx, ny, nw, nh);
    }
    if (err == ESP_OK) {
        err = epd_ssd1677_write_image_bw_again(native_bitmap, nx, ny, nw, nh);
    }

    free(native_bitmap);
    free(logical_bitmap);
    return err;
}

esp_err_t epd_text_show_partial(const char *text, int16_t x, int16_t y, uint8_t scale)
{
    return epd_text_show_partial_font(text, x, y, scale, &s_font_large);
}

esp_err_t epd_text_show_partial_term(const char *text, int16_t x, int16_t y, uint8_t scale)
{
    return epd_text_show_partial_font(text, x, y, scale, &s_font_term);
}

int16_t epd_text_term_advance(uint8_t scale)
{
    return epd_font_advance_for(&s_font_term, scale);
}

static esp_err_t epd_text_show_char_partial_font(char c, int16_t x, int16_t y, uint8_t scale,
                                                 const epd_text_font_t *font)
{
    if (!font || scale == 0 || scale > 4) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((uint8_t)c >= 128) {
        return ESP_ERR_INVALID_ARG;
    }

    const int16_t text_w = (int16_t)(font->w * scale);
    const int16_t text_h = (int16_t)(font->h * scale);
    if (text_w <= 0 || text_h <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int16_t rx, ry, rw, rh;
    epd_bw_align_area(x, y, text_w, text_h, &rx, &ry, &rw, &rh);
    if (rx + rw > EPD_WIDTH) {
        rw = (int16_t)(EPD_WIDTH - rx);
    }
    if (ry + rh > EPD_HEIGHT) {
        rh = (int16_t)(EPD_HEIGHT - ry);
    }
    if (rw <= 0 || rh <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t logical_bytes = (size_t)((rw + 7) / 8) * (size_t)rh;
    uint8_t *logical_bitmap = malloc(logical_bytes);
    if (!logical_bitmap) {
        return ESP_ERR_NO_MEM;
    }
    memset(logical_bitmap, 0xFF, logical_bytes);

    epd_font_draw_glyph_bw_ex(logical_bitmap, rw, (int16_t)(x - rx), (int16_t)(y - ry),
                              font, c, scale);

    int16_t nx, ny, nw, nh;
    epd_logical_rect_to_native(rx, ry, rw, rh, &nx, &ny, &nw, &nh);

    size_t native_bytes = (size_t)((nw + 7) / 8) * (size_t)nh;
    uint8_t *native_bitmap = malloc(native_bytes);
    if (!native_bitmap) {
        free(logical_bitmap);
        return ESP_ERR_NO_MEM;
    }
    memset(native_bitmap, 0xFF, native_bytes);
    epd_bw_rotate_r3(logical_bitmap, rw, rh, native_bitmap, nw, nh);

    esp_err_t err = epd_ssd1677_write_image_bw(native_bitmap, nx, ny, nw, nh);
    if (err == ESP_OK) {
        err = epd_ssd1677_refresh_area(nx, ny, nw, nh);
    }
    if (err == ESP_OK) {
        err = epd_ssd1677_write_image_bw_again(native_bitmap, nx, ny, nw, nh);
    }

    free(native_bitmap);
    free(logical_bitmap);
    return err;
}

esp_err_t epd_text_show_char_partial_term(char c, int16_t x, int16_t y, uint8_t scale)
{
    return epd_text_show_char_partial_font(c, x, y, scale, &s_font_term);
}

esp_err_t epd_text_clear_partial(int16_t x, int16_t y, int16_t w, int16_t h)
{
    if (w <= 0 || h <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int16_t nx, ny, nw, nh;
    epd_logical_rect_to_native(x, y, w, h, &nx, &ny, &nw, &nh);

    esp_err_t err = epd_ssd1677_write_bw_fill(nx, ny, nw, nh, 0xFF);
    if (err == ESP_OK) {
        err = epd_ssd1677_refresh_area(nx, ny, nw, nh);
    }
    if (err == ESP_OK) {
        err = epd_ssd1677_write_bw_fill_again(nx, ny, nw, nh, 0xFF);
    }
    return err;
}

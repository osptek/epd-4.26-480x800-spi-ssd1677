#pragma once

#include "esp_err.h"

esp_err_t app_epd_init(void);
esp_err_t app_epd_partial_demo(void);
esp_err_t app_epd_image_demo(void);
esp_err_t app_epd_boot_demo(void);
esp_err_t app_epd_image_slideshow_loop(void);
esp_err_t app_epd_slideshow_loop(void);
esp_err_t app_epd_bw_demo_loop(void);
esp_err_t app_epd_refresh_loop(void);

/*
 * SPDX-FileCopyrightText: Copyright 2026 OSPTEK
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * https://github.com/osptek
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "app_init.h"
#include "epd_init.h"

static const char *TAG = "app_main";

static void epd_bringup_task(void *arg)
{
    (void)arg;
    esp_err_t err = app_epd_boot_demo();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "EPD boot demo failed: %s", esp_err_to_name(err));
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "GDEQ0426T82 boot + image slideshow");
    xTaskCreate(epd_bringup_task, "epd_bringup", 12288, NULL, 5, NULL);
}

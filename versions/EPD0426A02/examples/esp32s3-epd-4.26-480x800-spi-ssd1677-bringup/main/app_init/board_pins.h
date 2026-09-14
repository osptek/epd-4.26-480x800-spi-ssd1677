#pragma once

#include "driver/gpio.h"

/* GDEQ0426T82 (4.26" 800x480) on ESP32-S3 */
#define PIN_EPD_BUSY    (GPIO_NUM_4)
#define PIN_EPD_DC      (GPIO_NUM_6)
#define PIN_EPD_CS      (GPIO_NUM_7)
#define PIN_EPD_SCK     (GPIO_NUM_15)
#define PIN_EPD_MOSI    (GPIO_NUM_16)
#define PIN_EPD_RST     (GPIO_NUM_5)

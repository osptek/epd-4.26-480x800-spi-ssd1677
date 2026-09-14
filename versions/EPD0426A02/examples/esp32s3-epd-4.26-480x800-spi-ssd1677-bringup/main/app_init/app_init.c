#include "esp_check.h"

#include "app_init.h"
#include "epd_init.h"

esp_err_t app_init(void)
{
    ESP_RETURN_ON_ERROR(app_epd_init(), "app_init", "epd bringup failed");
    return ESP_OK;
}

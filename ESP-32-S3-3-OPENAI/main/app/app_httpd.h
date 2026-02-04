#pragma once

#include "esp_err.h"

/**
 * @brief Initialize HTTP REST API server for dashboard
 */
esp_err_t app_httpd_init(void);

/**
 * @brief Stop HTTP server
 */
void app_httpd_stop(void);

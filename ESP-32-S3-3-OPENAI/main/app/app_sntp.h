#pragma once

#include "esp_err.h"

/**
 * @brief Initialize SNTP and set timezone to IST
 */
void app_sntp_init(void);

/**
 * @brief Get the formatted time string (e.g. "10:30 AM")
 * 
 * @param buf Buffer to store string
 * @param max_len Size of buffer
 * @return true if time is set (synced), false if not
 */
bool app_sntp_get_time_str(char *buf, size_t max_len);

/**
 * @brief Get the date string (e.g. "Mon, Jan 01")
 */
bool app_sntp_get_date_str(char *buf, size_t max_len);

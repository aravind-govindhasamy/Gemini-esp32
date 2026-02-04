#pragma once

#include "esp_err.h"
#include <stdbool.h>

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

/**
 * @brief Set 24hr or 12hr time format
 *
 * @param is_24hr true for 24hr, false for 12hr (AM/PM)
 */
void app_sntp_set_24hr_mode(bool is_24hr);

/**
 * @brief Get current time format mode
 *
 * @return true if 24hr mode, false if 12hr mode
 */
bool app_sntp_get_24hr_mode(void);

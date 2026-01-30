/*
 * Compatibility layer for ESP-BOX-3 to support legacy BSP functions
 */

#pragma once

#include "esp_err.h"
#include "bsp/esp-bsp.h"
#include "esp_codec_dev.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the board (legacy wrapper)
 */
esp_err_t bsp_board_init(void);

/**
 * @brief Read from I2S (legacy wrapper)
 */
esp_err_t bsp_i2s_read(void *dest, size_t size, size_t *bytes_read, TickType_t timeout);

/**
 * @brief Write to I2S (legacy wrapper)
 */
esp_err_t bsp_i2s_write(void *src, size_t size, size_t *bytes_written, TickType_t timeout);

/**
 * @brief Set codec sample rate (legacy wrapper)
 */
esp_err_t bsp_codec_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch);

/**
 * @brief Mute/unmute codec (legacy wrapper)
 */
esp_err_t bsp_codec_mute_set(bool mute);

/**
 * @brief Set codec volume (legacy wrapper)
 */
esp_err_t bsp_codec_volume_set(int volume, int *v);

#ifdef __cplusplus
}
#endif

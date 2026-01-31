/*
 * Wrapper for PicoTTS English synthesis on ESP32-S3
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize PicoTTS engine
 * @return ESP_OK on success
 */
esp_err_t app_tts_init(void);

/**
 * @brief Add text to the TTS queue and start speaking
 * @param text The text to synthesize
 * @return ESP_OK on success
 */
esp_err_t app_tts_speak(const char *text);

/**
 * @brief Stop current speech synthesis
 * @return ESP_OK on success
 */
esp_err_t app_tts_stop(void);

/**
 * @brief Check if TTS is currently speaking
 * @return true if speaking
 */
bool app_tts_is_playing(void);

/**
 * @brief Wait for TTS to finish speaking
 * @param timeout_ms Max time to wait
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if timed out
 */
esp_err_t app_tts_wait_done(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

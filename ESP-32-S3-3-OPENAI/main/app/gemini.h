#ifndef GEMINI_H
#define GEMINI_H

#include "esp_err.h"

/**
 * @brief Initialize Gemini API client
 * 
 * @param api_key The Gemini API key
 * @return esp_err_t ESP_OK if success
 */
esp_err_t gemini_init(const char *api_key);

/**
 * @brief Send a text query to Gemini and get a response
 * 
 * @param query The text to send
 * @return char* The response text (caller must free), or NULL on error
 */
char* gemini_query(const char *query);

/**
 * @brief Send audio data to Gemini and get a text response
 * 
 * @param audio Binary audio data (PCM/WAV)
 * @param len Length of audio data
 * @return char* The transcribed/responded text (caller must free)
 */
char* gemini_audio_query(uint8_t *audio, size_t len);

#endif // GEMINI_H

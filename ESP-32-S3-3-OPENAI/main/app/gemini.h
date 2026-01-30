#ifndef GEMINI_H
#define GEMINI_H

#include <stddef.h>
#include "esp_err.h"

typedef struct {
    char *text;
    uint8_t *audio;
    size_t audio_len;
} gemini_response_t;

/**
 * @brief Initialize Gemini API client
 * 
 * @param api_key The Gemini API key
 * @return esp_err_t ESP_OK if success
 */
esp_err_t gemini_init(const char *api_key);

/**
 * @brief Send text data to Gemini and get text + audio response
 * 
 * @param text The user's transcribed text
 * @return gemini_response_t* Struct containing text and audio (caller must use gemini_response_free)
 */
gemini_response_t* gemini_text_query(const char *text);

/**
 * @brief Send audio data to Gemini and get text + audio response (LEGACY - DO NOT USE FOR INPUT)
 * 
 * @param audio Binary audio data (PCM/WAV)
 * @param len Length of audio data
 * @return gemini_response_t* Struct containing text and audio (caller must use gemini_response_free)
 */
gemini_response_t* gemini_audio_query(uint8_t *audio, size_t len);

/**
 * @brief Free a Gemini response struct
 * 
 * @param res Response pointer to free
 */
void gemini_response_free(gemini_response_t *res);

#endif // GEMINI_H

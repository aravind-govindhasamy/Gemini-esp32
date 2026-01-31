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
 * @param name The user's name
 * @param age The user's age
 * @return gemini_response_t* Struct containing text and audio (caller must use gemini_response_free)
 */
gemini_response_t* gemini_text_query(const char *text, const char *name, int age);

/**
 * @brief Send audio data to Gemini and get text + audio response
 * 
 * @param audio Binary audio data (WAV format)
 * @param len Length of audio data
 * @param name User's name
 * @param age User's age
 * @return gemini_response_t* Struct containing text and audio
 */
gemini_response_t* gemini_audio_query(uint8_t *audio, size_t len, const char *name, int age);

/**
 * @brief Simple STT query using generateContent
 * 
 * @param audio Binary audio data (WAV)
 * @param len Length of audio data
 * @return char* Transcribed text (caller must free)
 */
char* gemini_stt_query(uint8_t *audio, size_t len);

/**
 * @brief Free a Gemini response struct
 * 
 * @param res Response pointer to free
 */
void gemini_response_free(gemini_response_t *res);

#endif // GEMINI_H

#pragma once

#include "esp_err.h"
#include "gemini.h" // reuse response struct

/**
 * @brief Initialize the OpenAI client with an API key
 */
void openai_init(const char *api_key);

/**
 * @brief Generate speech using OpenAI TTS API
 * 
 * @param text The text to synthesize
 * @return gemini_response_t* containing audio data (NULL if failed)
 */
gemini_response_t* openai_tts_query(const char *text);

#ifndef MAIN_H
#define MAIN_H

#include "esp_err.h"

/**
 * @brief Trigger a Gemini interaction using a text prompt
 * 
 * @param prompt The text prompt (transcribed locally or from command)
 * @return esp_err_t ESP_OK if success
 */
esp_err_t gemini_speech_bot_trigger(const char *prompt);

#endif // MAIN_H

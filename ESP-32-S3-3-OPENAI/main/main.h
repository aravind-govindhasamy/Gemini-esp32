#ifndef MAIN_H
#define MAIN_H

#include "esp_err.h"
#include "gemini.h"

/**
 * @brief Trigger a Gemini interaction using a text prompt
 * 
 * @param prompt The text prompt (transcribed locally or from command)
 * @return esp_err_t ESP_OK if success
 */
esp_err_t gemini_audio_bot_trigger(uint8_t *audio, size_t len, nlu_result_t *pre_nlu_res);
esp_err_t gemini_speech_bot_trigger(const char *prompt);

void live_stt_callback(const char *text, bool is_final);

// Hardware Control
void hw_light_set(bool on, uint32_t color);
float hw_get_sensor_temp(void);
#endif // MAIN_H

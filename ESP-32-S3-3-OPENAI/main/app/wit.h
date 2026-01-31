#pragma once

#include "esp_err.h"
#include "gemini.h" // reuse response struct

typedef struct wit_nlu_result {
    char *text;
    char *intent;
    float intent_conf;
    // Add entities/traits as needed in the future
} wit_nlu_result_t;

/**
 * @brief Initialize Wit.ai client
 */
void wit_init(const char *token);

/**
 * @brief Callback for live transcription updates
 * @param text The current partial or final transcription
 * @param is_final True if this is the final transcription
 */
typedef void (*wit_stt_callback_t)(const char *text, bool is_final);

/**
 * @brief Send audio to Wit.ai and get structured NLU result (Blocking/Legacy)
 */
wit_nlu_result_t* wit_stt_query(uint8_t *pcm_audio, size_t len);

/**
 * @brief Start a live STT stream
 * @param cb Callback for intermediate results
 */
esp_err_t wit_stt_stream_start(wit_stt_callback_t cb);

/**
 * @brief Feed raw PCM audio to the active stream
 */
esp_err_t wit_stt_stream_feed(uint8_t *pcm_audio, size_t len);

/**
 * @brief Stop the active stream and get final NLU result
 */
wit_nlu_result_t* wit_stt_stream_stop(void);

/**
 * @brief Free a Wit NLU result
 */
void wit_nlu_result_free(wit_nlu_result_t *res);

/**
 * @brief Generate speech using Wit.ai TTS
 * 
 * @param text The text to synthesize
 * @return gemini_response_t* containing audio data (NULL if failed)
 */
gemini_response_t* wit_tts_query(const char *text);

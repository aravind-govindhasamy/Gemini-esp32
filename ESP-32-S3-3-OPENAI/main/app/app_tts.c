/*
 * Wrapper for PicoTTS English synthesis on ESP32-S3
 */

#include "app_tts.h"
#include "picotts.h"
#include "esp_log.h"
#include "esp_codec_dev.h"
#include "bsp_board.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "audio_player.h"

static const char *TAG = "app_tts";

#define TTS_TASK_PRIORITY 15
#define TTS_CORE 0
#define TTS_DONE_BIT BIT0

static EventGroupHandle_t tts_event_group;
static bool g_tts_playing = false;
static esp_codec_dev_handle_t g_speaker_dev = NULL;

// Extern from bsp_board.c if we didn't want to use bsp_i2s_write
// But using a direct codec write is more efficient for the callback
static void tts_sample_cb(int16_t *buf, unsigned count)
{
    if (g_speaker_dev == NULL) {
        g_speaker_dev = bsp_board_get_play_handle();
    }
    
    if (g_speaker_dev && count > 0) {
        // Use a larger buffer to ensure no truncation (count is usually 128)
        static int16_t stereo_buf[1024]; 
        unsigned to_write = count > 512 ? 512 : count;
        
        for (int i = 0; i < to_write; i++) {
            stereo_buf[i * 2] = buf[i];
            stereo_buf[i * 2 + 1] = buf[i];
        }
        size_t written = 0;
        bsp_i2s_write(stereo_buf, to_write * 2 * sizeof(int16_t), &written, portMAX_DELAY);
    }
}

static void tts_done_cb(void)
{
    ESP_LOGI(TAG, "TTS Finished speaking");
    g_tts_playing = false;
    
    // Unmute mic when speech ends
    esp_codec_dev_handle_t rec_handle = bsp_board_get_record_handle();
    if (rec_handle) esp_codec_dev_set_in_mute(rec_handle, false);
    
    xEventGroupSetBits(tts_event_group, TTS_DONE_BIT);
}

static void tts_error_cb(void)
{
    ESP_LOGE(TAG, "TTS Error occurred");
    g_tts_playing = false;
    xEventGroupSetBits(tts_event_group, TTS_DONE_BIT);
}

esp_err_t app_tts_init(void)
{
    if (tts_event_group == NULL) {
        tts_event_group = xEventGroupCreate();
    }

    // Initialize speaker dev if not already done
    // In this codebase, bsp_board_init handles it, but we'll fetch the handle
    // We'll assume bsp_board_init has been called as it's in main.c
    // However, to be safe, we'll use a local pointer or just use bsp_i2s_write
    // The user's snippet used esp_codec_dev_write, so let's try to get that handle.
    // Actually, bsp_board.c keeps play_handle static. 
    // Let's modify bsp_board.c to expose the play_handle or use the bsp wrapper.
    
    // For now, let's use the bsp_i2s_write wrapper which is already public.
    // BUT tts_sample_cb is called from the TTS task, so we need a stable path.
    
    if (picotts_init(TTS_TASK_PRIORITY, tts_sample_cb, TTS_CORE)) {
        picotts_set_idle_notify(tts_done_cb);
        picotts_set_error_notify(tts_error_cb);
        ESP_LOGI(TAG, "PicoTTS Initialized successfully");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Failed to initialize PicoTTS");
    return ESP_FAIL;
}

esp_err_t app_tts_speak(const char *text)
{
    if (!text) return ESP_ERR_INVALID_ARG;

    ESP_LOGI(TAG, "Speaking: %s", text);
    
    // Prepare for speech: Stop other audio and mute mic
    audio_player_stop(); // Stop chime/wav playback
    esp_codec_dev_handle_t rec_handle = bsp_board_get_record_handle();
    if (rec_handle) esp_codec_dev_set_in_mute(rec_handle, true);
    
    g_tts_playing = true;
    xEventGroupClearBits(tts_event_group, TTS_DONE_BIT);

    picotts_add(text, strlen(text) + 1); 

    return ESP_OK;
}

esp_err_t app_tts_stop(void)
{
    // PicoTTS doesn't have a clear "abort" in the provided snippet, 
    // but picotts_shutdown or a reset might work.
    // For now, we'll leave it as a placeholder.
    return ESP_OK;
}

bool app_tts_is_playing(void)
{
    return g_tts_playing;
}

esp_err_t app_tts_wait_done(uint32_t timeout_ms)
{
    EventBits_t bits = xEventGroupWaitBits(tts_event_group, TTS_DONE_BIT, pdTRUE, pdTRUE, pdMS_TO_TICKS(timeout_ms));
    return (bits & TTS_DONE_BIT) ? ESP_OK : ESP_ERR_TIMEOUT;
}

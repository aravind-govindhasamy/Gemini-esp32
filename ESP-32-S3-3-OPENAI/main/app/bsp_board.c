/*
 * Compatibility layer for ESP-BOX-3 to support legacy BSP functions
 */

#include "bsp_board.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "bsp_compat";

static esp_codec_dev_handle_t play_handle = NULL;
static esp_codec_dev_handle_t record_handle = NULL;

esp_err_t bsp_board_init(void)
{
    esp_err_t ret = bsp_audio_init(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize audio BSP");
        return ret;
    }
    
    play_handle = bsp_audio_codec_speaker_init();
    record_handle = bsp_audio_codec_microphone_init();
    
    if (play_handle == NULL || record_handle == NULL) {
        ESP_LOGE(TAG, "Failed to initialize codec devices");
        return ESP_FAIL;
    }

    // Open handles with default settings
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = 16000,
        .bits_per_sample = 16,
        .channel = 2,
    };
    esp_codec_dev_open(play_handle, &fs);
    esp_codec_dev_open(record_handle, &fs);
    
    // Boost microphone sensitivity (for ES7210)
    esp_codec_dev_set_in_gain(record_handle, 30.0);
    esp_codec_dev_set_in_mute(record_handle, false);
    
    return ESP_OK;
}

esp_err_t bsp_i2s_read(void *dest, size_t size, size_t *bytes_read, TickType_t timeout)
{
    if (record_handle == NULL) {
        vTaskDelay(pdMS_TO_TICKS(10));
        return ESP_ERR_INVALID_STATE;
    }
    int ret = esp_codec_dev_read(record_handle, dest, size);
    if (ret == ESP_CODEC_DEV_OK) {
        if (bytes_read) *bytes_read = size;
        return ESP_OK;
    }
    // If it fails, don't spin too fast to avoid watchdog
    vTaskDelay(pdMS_TO_TICKS(5));
    return ESP_FAIL;
}

esp_err_t bsp_i2s_write(void *src, size_t size, size_t *bytes_written, TickType_t timeout)
{
    if (play_handle == NULL) {
        vTaskDelay(pdMS_TO_TICKS(10));
        return ESP_ERR_INVALID_STATE;
    }
    int ret = esp_codec_dev_write(play_handle, src, size);
    if (ret == ESP_CODEC_DEV_OK) {
        if (bytes_written) *bytes_written = size;
        return ESP_OK;
    }
    vTaskDelay(pdMS_TO_TICKS(5));
    return ESP_FAIL;
}

esp_err_t bsp_codec_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    if (play_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = rate,
        .bits_per_sample = bits_cfg,
        .channel = (ch == I2S_SLOT_MODE_STEREO) ? 2 : 1,
    };
    
    // We must close and re-open to change FS in some codec drivers, 
    // but esp_codec_dev_open usually handles it.
    return esp_codec_dev_open(play_handle, &fs) == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
}

esp_err_t bsp_codec_mute_set(bool mute)
{
    if (play_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_codec_dev_set_out_mute(play_handle, mute) == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
}

esp_err_t bsp_codec_volume_set(int volume, int *v)
{
    if (play_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    int ret = esp_codec_dev_set_out_vol(play_handle, volume);
    if (ret == ESP_CODEC_DEV_OK) {
        if (v) *v = volume;
        return ESP_OK;
    }
    return ESP_FAIL;
}

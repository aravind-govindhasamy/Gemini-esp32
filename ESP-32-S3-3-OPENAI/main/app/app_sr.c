/*
 * SPDX-FileCopyrightText: 2015-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "app_sr.h"
#include "esp_mn_speech_commands.h"
#include "esp_process_sdkconfig.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_models.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_iface.h"
#include "esp_mn_iface.h"
#include "model_path.h"
#include "bsp/esp-bsp.h"
#include "bsp_board.h"
#include "app_audio.h"
#include "app_wifi.h"
#include <math.h>

static const char *TAG = "app_sr";

static esp_afe_sr_iface_t *afe_handle = NULL;
static srmodel_list_t *models = NULL;
static bool manul_detect_flag = false;

sr_data_t *g_sr_data = NULL;

#define I2S_CHANNEL_NUM      2

extern bool record_flag;
extern uint32_t record_total_len;

static void audio_feed_task(void *arg)
{
    ESP_LOGI(TAG, "Feed Task");
    size_t bytes_read = 0;
    esp_afe_sr_data_t *afe_data = (esp_afe_sr_data_t *) arg;
    int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
    int feed_channel = 3;
    ESP_LOGI(TAG, "audio_chunksize=%d, feed_channel=%d", audio_chunksize, feed_channel);

    /* Allocate audio buffer and check for result */
    int16_t *audio_buffer = heap_caps_malloc(audio_chunksize * sizeof(int16_t) * feed_channel, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    assert(audio_buffer);
    g_sr_data->afe_in_buffer = audio_buffer;

    while (true) {
        if (g_sr_data->event_group && xEventGroupGetBits(g_sr_data->event_group)) {
            xEventGroupSetBits(g_sr_data->event_group, FEED_DELETED);
            vTaskDelete(NULL);
        }

        /* Read audio data from I2S bus */
        bsp_i2s_read((char *)audio_buffer, audio_chunksize * I2S_CHANNEL_NUM * sizeof(int16_t), &bytes_read, portMAX_DELAY);

        /* Channel Adjust */
        for (int  i = audio_chunksize - 1; i >= 0; i--) {
            audio_buffer[i * 3 + 2] = 0;
            audio_buffer[i * 3 + 1] = audio_buffer[i * 2 + 1];
            audio_buffer[i * 3 + 0] = audio_buffer[i * 2 + 0];
        }

        /* Feed samples of an audio stream to the AFE_SR */
        afe_handle->feed(afe_data, audio_buffer);
        
        // Diagnostic: Check if we have any signal (RMS)
        static int rms_count = 0;
        if (rms_count++ % 30 == 0) {
            float rms = 0;
            for (int i=0; i<audio_chunksize; i++) {
                float sample = (float)audio_buffer[i*3];
                rms += sample * sample;
            }
            rms = sqrtf(rms / audio_chunksize);
            ESP_LOGI(TAG, "Audio In RMS: %.2f (Mic1: %d)", rms, audio_buffer[0]);
        }

        audio_record_save(audio_buffer, audio_chunksize);
    }
}

static void audio_detect_task(void *arg)
{
    ESP_LOGI(TAG, "Detection task");
    static afe_vad_state_t local_state;
    static uint8_t frame_keep = 0;

    bool detect_flag = false;
    esp_afe_sr_data_t *afe_data = arg;

    while (true) {
        if (NEED_DELETE && xEventGroupGetBits(g_sr_data->event_group)) {
            xEventGroupSetBits(g_sr_data->event_group, DETECT_DELETED);
            vTaskDelete(g_sr_data->handle_task);
            vTaskDelete(NULL);
        }
        afe_fetch_result_t *res = afe_handle->fetch(afe_data);
        if (!res || res->ret_value == ESP_FAIL) {
            continue;
        }
        if (res->wakeup_state == WAKENET_DETECTED) {
            ESP_LOGI(TAG,  "wakeword detected");
            sr_result_t result = {
                .wakenet_mode = WAKENET_DETECTED,
                .state = ESP_MN_STATE_DETECTING,
                .command_id = 0,
            };
            xQueueSend(g_sr_data->result_que, &result, 0);
        } else if (res->wakeup_state == WAKENET_CHANNEL_VERIFIED || manul_detect_flag) {
            detect_flag = true;
            if (manul_detect_flag) {
                manul_detect_flag = false;
                sr_result_t result = {
                    .wakenet_mode = WAKENET_DETECTED,
                    .state = ESP_MN_STATE_DETECTING,
                    .command_id = 0,
                };
                xQueueSend(g_sr_data->result_que, &result, 0);
            }
            frame_keep = 0;
            g_sr_data->afe_handle->disable_wakenet(afe_data);
            ESP_LOGI(TAG,  "AFE_FETCH_CHANNEL_VERIFIED, channel index: %d\n", res->trigger_channel_id);
        }

        if (true == detect_flag) {
            if (g_sr_data->multinet == NULL || g_sr_data->model_data == NULL) {
                ESP_LOGE(TAG, "Multinet not initialized!");
                detect_flag = false;
                g_sr_data->afe_handle->enable_wakenet(afe_data);
                continue;
            }
            esp_mn_state_t mn_state = g_sr_data->multinet->detect(g_sr_data->model_data, res->data);

            if (ESP_MN_STATE_DETECTED == mn_state) {
                esp_mn_results_t *mn_res = g_sr_data->multinet->get_results(g_sr_data->model_data);
                for (int i = 0; i < mn_res->num; i++) {
                    ESP_LOGI(TAG, "ID: %d, Phrase ID: %d, Prob: %f", mn_res->command_id[i], mn_res->phrase_id[i], mn_res->prob[i]);
                }

                sr_result_t result = {
                    .wakenet_mode = WAKENET_NO_DETECT,
                    .state = mn_state,
                    .command_id = mn_res->command_id[0],
                };
                xQueueSend(g_sr_data->result_que, &result, 0);
                g_sr_data->afe_handle->enable_wakenet(afe_data);
                detect_flag = false;
                continue;
            } else if (ESP_MN_STATE_TIMEOUT == mn_state) {
                ESP_LOGW(TAG, "Multinet internal timeout");
                sr_result_t result = {
                    .wakenet_mode = WAKENET_NO_DETECT,
                    .state = ESP_MN_STATE_TIMEOUT,
                    .command_id = 0,
                };
                xQueueSend(g_sr_data->result_que, &result, 0);
                g_sr_data->afe_handle->enable_wakenet(afe_data);
                detect_flag = false;
                continue;
            }

            // Periodic log to see if it's actually detecting
            static int detect_log_count = 0;
            if (detect_log_count++ % 100 == 0) {
                ESP_LOGI(TAG, "MN Detecting... (VAD: %d)", res->vad_state);
            }

            if (local_state != res->vad_state) {
                local_state = res->vad_state;
                frame_keep = 0;
            } else {
                frame_keep++;
            }

            if ((100 == frame_keep) && (AFE_VAD_SILENCE == res->vad_state)) {
                ESP_LOGI(TAG, "Silence detected (1.6s) - Triggering Timeout");
                sr_result_t result = {
                    .wakenet_mode = WAKENET_NO_DETECT,
                    .state = ESP_MN_STATE_TIMEOUT,
                    .command_id = 0,
                };
                xQueueSend(g_sr_data->result_que, &result, 0);
                g_sr_data->afe_handle->enable_wakenet(afe_data);
                detect_flag = false;
                continue;
            }
        }
    }
    /* Task never returns */
    vTaskDelete(NULL);
}

esp_err_t app_sr_set_language(sr_language_t new_lang)
{
    ESP_RETURN_ON_FALSE(NULL != g_sr_data, ESP_ERR_INVALID_STATE, TAG, "SR is not running");

    if (new_lang == g_sr_data->lang) {
        ESP_LOGW(TAG, "nothing to do");
        return ESP_OK;
    } else {
        g_sr_data->lang = new_lang;
    }
    ESP_LOGI(TAG, "Set language %s", SR_LANG_EN == g_sr_data->lang ? "EN" : "CN");
    if (g_sr_data->model_data) {
        g_sr_data->multinet->destroy(g_sr_data->model_data);
    }
    char *wn_name = esp_srmodel_filter(models, ESP_WN_PREFIX, "");
    ESP_LOGI(TAG, "load wakenet:%s", wn_name);
    g_sr_data->afe_handle->set_wakenet(g_sr_data->afe_data, wn_name);
    return ESP_OK;
}

esp_err_t app_sr_start(bool record_en)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(NULL == g_sr_data, ESP_ERR_INVALID_STATE, TAG, "SR already running");

    g_sr_data = heap_caps_calloc(1, sizeof(sr_data_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(NULL != g_sr_data, ESP_ERR_NO_MEM, TAG, "Failed create sr data");

    g_sr_data->result_que = xQueueCreate(3, sizeof(sr_result_t));
    ESP_GOTO_ON_FALSE(NULL != g_sr_data->result_que, ESP_ERR_NO_MEM, err, TAG, "Failed create result queue");

    g_sr_data->event_group = xEventGroupCreate();
    ESP_GOTO_ON_FALSE(NULL != g_sr_data->event_group, ESP_ERR_NO_MEM, err, TAG, "Failed create event_group");

    BaseType_t ret_val;
    models = esp_srmodel_init("model");
    afe_handle = (esp_afe_sr_iface_t *)&ESP_AFE_SR_HANDLE;
    afe_config_t afe_config = AFE_CONFIG_DEFAULT();

    afe_config.wakenet_model_name = esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);
    afe_config.aec_init = false;

    esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(&afe_config);
    g_sr_data->afe_handle = afe_handle;
    g_sr_data->afe_data = afe_data;

    g_sr_data->lang = SR_LANG_MAX;
    ret = app_sr_set_language(SR_LANG_EN);
    ESP_GOTO_ON_FALSE(ESP_OK == ret, ESP_FAIL, err, TAG,  "Failed to set language");

    // Add local commands for kid-friendly interaction (esp-sr 1.3.3 API)
    char *mn_name = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
    ESP_LOGI(TAG, "Load multinet: %s", mn_name);
    
    esp_mn_iface_t *multinet = esp_mn_handle_from_name(mn_name);
    if (multinet == NULL) {
        ESP_LOGE(TAG, "Failed to get multinet interface for %s", mn_name);
        goto err;
    }

    ESP_LOGI(TAG, "Memory Before Multinet Init - Internal: %d, SPIRAM: %d", 
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL), 
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
             
    model_iface_data_t *model_data = multinet->create(mn_name, 6000); // 6s timeout
    if (model_data == NULL) {
        ESP_LOGE(TAG, "Failed to create multinet model data (Out of memory?)");
        goto err;
    }

    ESP_LOGI(TAG, "Memory After Multinet Init - Internal: %d, SPIRAM: %d", 
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL), 
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    
    esp_err_t mn_ret = esp_mn_commands_alloc(multinet, model_data);
    if (mn_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to allocate mn commands: %d", mn_ret);
        goto err;
    }

    ESP_LOGI(TAG, "Adding commands to Multinet 6...");
    ret = esp_mn_commands_add(1, "TELL ME A JOKE");
    ret |= esp_mn_commands_add(2, "SING A SONG");
    ret |= esp_mn_commands_add(3, "WHAT IS THE ALPHABET");
    ret |= esp_mn_commands_add(4, "WHO ARE YOU");
    ret |= esp_mn_commands_add(5, "I LOVE YOU");
    ret |= esp_mn_commands_add(6, "STOP");
    ret |= esp_mn_commands_add(7, "CLOSE");
    ret |= esp_mn_commands_add(8, "HI BRO");
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add regular commands!");
    } else {
        ESP_LOGI(TAG, "8 phrases registered successfully (including Hi bro).");
    }

    g_sr_data->multinet = multinet;
    g_sr_data->model_data = model_data;

    ret_val = xTaskCreatePinnedToCore(&audio_feed_task, "Feed Task", 8 * 1024, (void *)afe_data, 15, &g_sr_data->feed_task, 0);
    ESP_GOTO_ON_FALSE(pdPASS == ret_val, ESP_FAIL, err, TAG,  "Failed create audio feed task");

    ret_val = xTaskCreatePinnedToCore(&audio_detect_task, "Detect Task", 12 * 1024, (void *)afe_data, 10, &g_sr_data->detect_task, 1);
    ESP_GOTO_ON_FALSE(pdPASS == ret_val, ESP_FAIL, err, TAG,  "Failed create audio detect task");

    ret_val = xTaskCreatePinnedToCore(&sr_handler_task, "SR Handler Task", 8 * 1024, NULL, 5, &g_sr_data->handle_task, 0);
    ESP_GOTO_ON_FALSE(pdPASS == ret_val, ESP_FAIL, err, TAG,  "Failed create audio handler task");

    audio_record_init();

    return ESP_OK;
err:
    app_sr_stop();
    return ret;
}

esp_err_t app_sr_stop(void)
{
    ESP_RETURN_ON_FALSE(NULL != g_sr_data, ESP_ERR_INVALID_STATE, TAG, "SR is not running");
    xEventGroupSetBits(g_sr_data->event_group, NEED_DELETE);
    xEventGroupWaitBits(g_sr_data->event_group, NEED_DELETE | FEED_DELETED | DETECT_DELETED | HANDLE_DELETED, 1, 1, portMAX_DELAY);

    if (g_sr_data->result_que) {
        vQueueDelete(g_sr_data->result_que);
        g_sr_data->result_que = NULL;
    }

    if (g_sr_data->event_group) {
        vEventGroupDelete(g_sr_data->event_group);
        g_sr_data->event_group = NULL;
    }

    if (g_sr_data->fp) {
        fclose(g_sr_data->fp);
        g_sr_data->fp = NULL;
    }

    if (g_sr_data->model_data) {
        g_sr_data->multinet->destroy(g_sr_data->model_data);
    }

    if (g_sr_data->afe_data) {
        g_sr_data->afe_handle->destroy(g_sr_data->afe_data);
    }

    if (g_sr_data->afe_in_buffer) {
        heap_caps_free(g_sr_data->afe_in_buffer);
    }

    if (g_sr_data->afe_out_buffer) {
        heap_caps_free(g_sr_data->afe_out_buffer);
    }

    heap_caps_free(g_sr_data);
    g_sr_data = NULL;
    return ESP_OK;
}

esp_err_t app_sr_get_result(sr_result_t *result, TickType_t xTicksToWait)
{
    ESP_RETURN_ON_FALSE(NULL != g_sr_data, ESP_ERR_INVALID_STATE, TAG, "SR is not running");
    xQueueReceive(g_sr_data->result_que, result, xTicksToWait);
    return ESP_OK;
}

esp_err_t app_sr_start_once(void)
{
    ESP_RETURN_ON_FALSE(NULL != g_sr_data, ESP_ERR_INVALID_STATE, TAG, "SR is not running");
    manul_detect_flag = true;
    return ESP_OK;
}

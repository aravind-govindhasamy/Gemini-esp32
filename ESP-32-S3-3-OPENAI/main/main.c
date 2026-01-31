/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "app_ui_ctrl.h"
#ifndef CHATGPT_DEMO_VERSION
#define CHATGPT_DEMO_VERSION_MAJOR 1
#endif
#include "audio_player.h"
#include "app_sr.h"
#include "bsp/esp-bsp.h"
#include "bsp_board.h"
#include "app_audio.h"
#include "app_wifi.h"
#include "settings.h"
#include "gemini.h"

#define SCROLL_START_DELAY_S            (1.5)
#define LISTEN_SPEAK_PANEL_DELAY_MS     2000
#define SERVER_ERROR                    "server_error"
#define SORRY_CANNOT_UNDERSTAND         "Sorry, I can't understand."

#include "main.h"

static const char *TAG = "app_main";
static sys_param_t *sys_param = NULL;

/* 
 * Gemini Voice Bot Interaction Loop
 */
esp_err_t gemini_speech_bot_trigger(const char *prompt)
{
    esp_err_t ret = ESP_OK;
    gemini_response_t *response = NULL;

    ui_ctrl_show_panel(UI_CTRL_PANEL_GET, 0);

    ESP_LOGI(TAG, "Querying Gemini with prompt: %s", prompt);
    gemini_init(sys_param->gemini_key);
    // Pass persistent user info
    response = gemini_text_query(prompt, sys_param->user_name, sys_param->user_age);

    if (NULL == response || NULL == response->text) {
        ret = ESP_ERR_INVALID_RESPONSE;
        ui_ctrl_label_show_text(UI_CTRL_LABEL_LISTEN_SPEAK, SORRY_CANNOT_UNDERSTAND);
        ui_ctrl_show_panel(UI_CTRL_PANEL_SLEEP, LISTEN_SPEAK_PANEL_DELAY_MS);
        ESP_LOGE(TAG, "[gemini_text_query]: invalid response");
        goto err;
    }

    // UI display success
    ui_ctrl_label_show_text(UI_CTRL_LABEL_REPLY_QUESTION, prompt); 
    ui_ctrl_label_show_text(UI_CTRL_LABEL_LISTEN_SPEAK, response->text);
    ui_ctrl_label_show_text(UI_CTRL_LABEL_REPLY_CONTENT, response->text);
    ui_ctrl_show_panel(UI_CTRL_PANEL_REPLY, 0);

    // Play Voice Output if available
    if (response->audio && response->audio_len > 0) {
        FILE *fp = fopen("/spiffs/response.wav", "wb");
        if (fp) {
            fwrite(response->audio, 1, response->audio_len, fp);
            fclose(fp);
            
            fp = fopen("/spiffs/response.wav", "rb");
            if (fp) {
                ESP_LOGI(TAG, "Playing Gemini Voice Output (%zu bytes)", response->audio_len);
                audio_player_play(fp);
                ui_ctrl_reply_set_audio_start_flag(true);
                
                // Wait for audio to finish playing
                while (audio_player_get_state() == AUDIO_PLAYER_STATE_PLAYING) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                ui_ctrl_reply_set_audio_end_flag(true);
            }
        }
    } else {
        // No audio, wait a bit for scrolling
        vTaskDelay(pdMS_TO_TICKS(1000));
        ui_ctrl_reply_set_audio_start_flag(true);
        ui_ctrl_reply_set_audio_end_flag(true);
    }
    
    // STICKY UI: Do NOT transition to sleep automatically!
    // The screen will now stay on UI_CTRL_PANEL_REPLY until touched or "Close" is said.
    // ui_ctrl_show_panel(UI_CTRL_PANEL_SLEEP, 10000); // REMOVED

err:
    if (response) gemini_response_free(response);
    return ret;
}

esp_err_t gemini_audio_bot_trigger(uint8_t *audio, size_t len)
{
    esp_err_t ret = ESP_OK;
    char *transcription = NULL;
    gemini_response_t *response = NULL;

    ui_ctrl_show_panel(UI_CTRL_PANEL_GET, 0);
    ui_ctrl_label_show_text(UI_CTRL_LABEL_LISTEN_SPEAK, "Transcribing...");

    // Step 1: Transcribe (The Ears)
    transcription = gemini_stt_query(audio, len);
    if (NULL == transcription) {
        ESP_LOGE(TAG, "STT returned NULL");
        ui_ctrl_label_show_text(UI_CTRL_LABEL_LISTEN_SPEAK, SORRY_CANNOT_UNDERSTAND);
        ui_ctrl_show_panel(UI_CTRL_PANEL_SLEEP, LISTEN_SPEAK_PANEL_DELAY_MS);
        return ESP_ERR_INVALID_RESPONSE;
    }

    ESP_LOGI(TAG, "Recognized: %s", transcription);
    ui_ctrl_label_show_text(UI_CTRL_LABEL_LISTEN_SPEAK, transcription);
    ui_ctrl_label_show_text(UI_CTRL_LABEL_REPLY_QUESTION, transcription);

    // Step 2: Think (The Brain)
    ESP_LOGI(TAG, "Querying Brain for: %s", transcription);
    response = gemini_text_query(transcription, sys_param->user_name, sys_param->user_age);
    free(transcription);

    if (NULL == response || NULL == response->text) {
        ret = ESP_ERR_INVALID_RESPONSE;
        ui_ctrl_label_show_text(UI_CTRL_LABEL_LISTEN_SPEAK, SORRY_CANNOT_UNDERSTAND);
        ui_ctrl_show_panel(UI_CTRL_PANEL_SLEEP, LISTEN_SPEAK_PANEL_DELAY_MS);
        goto err;
    }

    // UI display success
    ui_ctrl_label_show_text(UI_CTRL_LABEL_REPLY_CONTENT, response->text);
    ui_ctrl_show_panel(UI_CTRL_PANEL_REPLY, 0);

    // Play Voice Output if available
    if (response->audio && response->audio_len > 0) {
        FILE *fp = fopen("/spiffs/response.wav", "wb");
        if (fp) {
            fwrite(response->audio, 1, response->audio_len, fp);
            fclose(fp);
            fp = fopen("/spiffs/response.wav", "rb");
            if (fp) {
                ESP_LOGI(TAG, "Playing Gemini Voice Output (%zu bytes)", response->audio_len);
                audio_player_play(fp);
                ui_ctrl_reply_set_audio_start_flag(true);
                while (audio_player_get_state() == AUDIO_PLAYER_STATE_PLAYING) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                ui_ctrl_reply_set_audio_end_flag(true);
            }
        }
    } else {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ui_ctrl_reply_set_audio_start_flag(true);
        ui_ctrl_reply_set_audio_end_flag(true);
    }

err:
    if (response) gemini_response_free(response);
    return ret;
}

/* play audio function */

static void audio_play_finish_cb(void)
{
    ESP_LOGI(TAG, "replay audio end");
    if (ui_ctrl_reply_get_audio_start_flag()) {
        ui_ctrl_reply_set_audio_end_flag(true);
    }
}

void app_main()
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(settings_read_parameter_from_nvs());
    sys_param = settings_get_parameter();

    bsp_spiffs_mount();
    bsp_i2c_init();

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
    };
    bsp_display_start_with_config(&cfg);
    bsp_board_init();

    ESP_LOGI(TAG, "Display LVGL demo");
    bsp_display_backlight_on();
    ui_ctrl_init();
    app_network_start();

    ESP_LOGI(TAG, "User Persona: %s, Age: %d", sys_param->user_name, sys_param->user_age);
    gemini_init(sys_param->gemini_key);

    ESP_LOGI(TAG, "speech recognition start");
    app_sr_start(false);
    audio_register_play_finish_cb(audio_play_finish_cb);

    while (true) {
        ESP_LOGI(TAG, "Memory - Internal: %d, SPIRAM: %d",
                 heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
                 heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        vTaskDelay(pdMS_TO_TICKS(5 * 1000));
    }
}

/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <string.h>
#include <time.h>
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
#include "wit.h"

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
    wit_init(sys_param->wit_token);
    
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

    // Use Wit.ai for High-Quality TTS (User Preferred)
    gemini_response_t *voice_response = wit_tts_query(response->text);
    if (voice_response && voice_response->audio) {
        FILE *fp = fopen("/spiffs/response.mp3", "wb");
        if (fp) {
            fwrite(voice_response->audio, 1, voice_response->audio_len, fp);
            fclose(fp);
            
            fp = fopen("/spiffs/response.mp3", "rb");
            if (fp) {
                ESP_LOGI(TAG, "Playing Wit Voice Output (%zu bytes)", voice_response->audio_len);
                audio_player_play(fp);
                ui_ctrl_reply_set_audio_start_flag(true);
                while (audio_player_get_state() == AUDIO_PLAYER_STATE_PLAYING) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                ui_ctrl_reply_set_audio_end_flag(true);
            }
        }
        gemini_response_free(voice_response);
    }

    // STICKY UI: Do NOT transition to sleep automatically!
    // The screen will now stay on UI_CTRL_PANEL_REPLY until touched or "Close" is said.

err:
    if (response) gemini_response_free(response);
    return ret;
}


#include "driver/gpio.h"

void hw_light_set(bool on, uint32_t color) {
    ESP_LOGI(TAG, "Hardware: Light %s (Color: %06lX)", on ? "ON" : "OFF", color);
    // On ESP-BOX-3, GPIO 2 controls the internal LED. 
    // For now, we'll use a simple ON/OFF logic.
    gpio_reset_pin(GPIO_NUM_2);
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_2, on ? 1 : 0);
}

float hw_get_sensor_temp(void) {
    // Dummy sensor read for now
    return 24.5f;
}

void live_stt_callback(const char *text, bool is_final) {
    if (text) {
        ESP_LOGI(TAG, "Live STT: %s %s", text, is_final ? "[FINAL]" : "");
        ui_ctrl_label_show_text(UI_CTRL_LABEL_LISTEN_SPEAK, (char*)text);
    }
}

esp_err_t gemini_audio_bot_trigger_internal(uint8_t *audio, size_t len, wit_nlu_result_t *pre_nlu_res);

esp_err_t gemini_audio_bot_trigger(uint8_t *audio, size_t len, wit_nlu_result_t *pre_nlu_res)
{
    esp_err_t ret = ESP_OK;
    wit_nlu_result_t *nlu_res = pre_nlu_res;
    gemini_response_t *response = NULL;

    ui_ctrl_show_panel(UI_CTRL_PANEL_GET, 0);
    
    // Step 1: Transcribe & Understand (only if not already done by stream)
    // If we have a result from the stream but it wasn't marked "is_final",
    // we prefer to re-transcribe the full local recording for accuracy.
    if (NULL == nlu_res || !nlu_res->is_final) {
        if (nlu_res) {
            ESP_LOGI(TAG, "Streaming result incomplete, falling back to full cloud STT");
            // Note: we don't free nlu_res yet if it came from pre_nlu_res, 
            // but in that case trigger() wasn't responsible for it. 
            // Actually, we should probably free it if we're replacing it.
            if (pre_nlu_res) wit_nlu_result_free(pre_nlu_res);
            nlu_res = NULL;
            pre_nlu_res = NULL; // Mark that we're now owning the new result
        }
        ui_ctrl_label_show_text(UI_CTRL_LABEL_LISTEN_SPEAK, "Transcribing...");
        nlu_res = wit_stt_query(audio, len);
        if (NULL == nlu_res || NULL == nlu_res->text) {
            ESP_LOGE(TAG, "Wit STT/NLU returned NULL");
            ui_ctrl_label_show_text(UI_CTRL_LABEL_LISTEN_SPEAK, SORRY_CANNOT_UNDERSTAND);
            ui_ctrl_show_panel(UI_CTRL_PANEL_SLEEP, LISTEN_SPEAK_PANEL_DELAY_MS);
            if (nlu_res) wit_nlu_result_free(nlu_res);
            return ESP_ERR_INVALID_RESPONSE;
        }
    }

    ESP_LOGI(TAG, "Recognized: %s", nlu_res->text);
    if (nlu_res->intent) {
        ESP_LOGI(TAG, "Intent: %s (conf: %.2f)", nlu_res->intent, nlu_res->intent_conf);
    }
    ui_ctrl_label_show_text(UI_CTRL_LABEL_LISTEN_SPEAK, nlu_res->text);
    ui_ctrl_label_show_text(UI_CTRL_LABEL_REPLY_QUESTION, nlu_res->text);

    // Step 2: Intent Routing (Local vs Brain)
    bool handled_locally = false;
    char local_reply[256];
    
    // 2.1: Robust Intent Detection (NLU + Keyword Fallback)
    const char *text = nlu_res->text ? nlu_res->text : "";
    bool is_light_on = (nlu_res->intent && strcmp(nlu_res->intent, "wit/light_on") == 0) || 
                       (strcasestr(text, "light") && strcasestr(text, "on"));
    bool is_light_off = (nlu_res->intent && strcmp(nlu_res->intent, "wit/light_off") == 0) || 
                        (strcasestr(text, "light") && strcasestr(text, "off"));
    bool is_sensor = (nlu_res->intent && strcmp(nlu_res->intent, "wit/get_sensor") == 0) ||
                     (strcasestr(text, "temperature") || strcasestr(text, "sensor"));
    bool is_time = (nlu_res->intent && strcmp(nlu_res->intent, "wit/get_time") == 0) ||
                   (strcasestr(text, "time") && strcasestr(text, "what"));

    if (is_time) {
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(local_reply, sizeof(local_reply), "It is currently %H:%M.", &timeinfo);
        handled_locally = true;
    } else if (nlu_res->intent && strcmp(nlu_res->intent, "wit/greetings") == 0) {
        snprintf(local_reply, sizeof(local_reply), "Hello %s! How can I help you today?", sys_param->user_name);
        handled_locally = true;
    } else if (is_light_on) {
        hw_light_set(true, 0xFFFFFF);
        snprintf(local_reply, sizeof(local_reply), "Sure! Turning the light on for you.");
        handled_locally = true;
    } else if (is_light_off) {
        hw_light_set(false, 0);
        snprintf(local_reply, sizeof(local_reply), "Off it goes. Light is now off.");
        handled_locally = true;
    } else if (is_sensor) {
        float temp = hw_get_sensor_temp();
        snprintf(local_reply, sizeof(local_reply), "The room temperature is currently %.1f degrees.", temp);
        handled_locally = true;
    }

    if (handled_locally) {
        ESP_LOGI(TAG, "Intent handled locally: %s", local_reply);
        ui_ctrl_label_show_text(UI_CTRL_LABEL_LISTEN_SPEAK, local_reply);
        ui_ctrl_label_show_text(UI_CTRL_LABEL_REPLY_CONTENT, local_reply);
        ui_ctrl_show_panel(UI_CTRL_PANEL_REPLY, 0);
        
        gemini_response_t *voice_res = wit_tts_query(local_reply);
        if (voice_res && voice_res->audio) {
            FILE *fp = fopen("/spiffs/response.mp3", "wb");
            if (fp) {
                fwrite(voice_res->audio, 1, voice_res->audio_len, fp);
                fclose(fp);
                fp = fopen("/spiffs/response.mp3", "rb");
                if (fp) {
                    audio_player_play(fp);
                    ui_ctrl_reply_set_audio_start_flag(true);
                    while (audio_player_get_state() == AUDIO_PLAYER_STATE_PLAYING) {
                        vTaskDelay(pdMS_TO_TICKS(100));
                    }
                    ui_ctrl_reply_set_audio_end_flag(true);
                }
            }
            gemini_response_free(voice_res);
        }
        goto cleanup;
    }

    // Step 2: Think (The Brain - Fallback to Gemini)
    ESP_LOGI(TAG, "Querying Brain for: %s", nlu_res->text);
    gemini_init(sys_param->gemini_key);
    wit_init(sys_param->wit_token);
    response = gemini_text_query(nlu_res->text, sys_param->user_name, sys_param->user_age);

    if (NULL == response || NULL == response->text) {
        ret = ESP_ERR_INVALID_RESPONSE;
        ui_ctrl_label_show_text(UI_CTRL_LABEL_LISTEN_SPEAK, SORRY_CANNOT_UNDERSTAND);
        ui_ctrl_show_panel(UI_CTRL_PANEL_SLEEP, LISTEN_SPEAK_PANEL_DELAY_MS);
        goto cleanup;
    }

    // UI display success
    ui_ctrl_label_show_text(UI_CTRL_LABEL_LISTEN_SPEAK, response->text);
    ui_ctrl_label_show_text(UI_CTRL_LABEL_REPLY_CONTENT, response->text);
    ui_ctrl_show_panel(UI_CTRL_PANEL_REPLY, 0);

    // Step 3: High Quality Voice (Wit.ai TTS)
    gemini_response_t *voice_res = wit_tts_query(response->text);
    if (voice_res && voice_res->audio) {
        FILE *fp = fopen("/spiffs/response.mp3", "wb");
        if (fp) {
            fwrite(voice_res->audio, 1, voice_res->audio_len, fp);
            fclose(fp);
            fp = fopen("/spiffs/response.mp3", "rb");
            if (fp) {
                ESP_LOGI(TAG, "Playing Wit Voice Output (%zu bytes)", voice_res->audio_len);
                audio_player_play(fp);
                ui_ctrl_reply_set_audio_start_flag(true);
                while (audio_player_get_state() == AUDIO_PLAYER_STATE_PLAYING) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                ui_ctrl_reply_set_audio_end_flag(true);
            }
        }
        gemini_response_free(voice_res);
    }

cleanup:
    // Only free if we created it here (i.e. if pre_nlu_res was NULL)
    if (nlu_res && pre_nlu_res == NULL) wit_nlu_result_free(nlu_res);
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
    wit_init(sys_param->wit_token);

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

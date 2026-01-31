/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <string.h>
#include <time.h>
#include <inttypes.h>
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
#include "app_tts.h"
#include "app_ui_ctrl.h"

#define SCROLL_START_DELAY_S            (1.5)
#define LISTEN_SPEAK_PANEL_DELAY_MS     2000
#define SERVER_ERROR                    "server_error"
#define SORRY_CANNOT_UNDERSTAND         "Sorry, I can't understand."

#include "main.h"
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

    ESP_LOGI(TAG, "User Message: %s", prompt);
    // Pass persistent user info
    response = gemini_text_query(prompt, sys_param->user_name, sys_param->user_age);

    if (response && response->text) {
        ESP_LOGI(TAG, "Gemini Response: %s", response->text);
    }

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

    // Use Local PicoTTS for High-Speed English (User Preferred)
    app_tts_speak(response->text);
    ui_ctrl_reply_set_audio_start_flag(true);
    app_tts_wait_done(60000); // Increased to 60s for long AI responses
    ui_ctrl_reply_set_audio_end_flag(true);
    // UI Sleep is now handled by the scroll timer in app_ui_ctrl.c for better timing
    // ui_ctrl_show_panel(UI_CTRL_PANEL_SLEEP, LISTEN_SPEAK_PANEL_DELAY_MS);

    /* 
    // Legacy Wit.ai TTS (Removed for PicoTTS)
    gemini_response_t *voice_response = wit_tts_query(response->text);
    ...
    */

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

esp_err_t gemini_audio_bot_trigger(uint8_t *audio, size_t len, nlu_result_t *pre_nlu_res)
{
    esp_err_t ret = ESP_OK;
    gemini_response_t *response = NULL;

    ui_ctrl_show_panel(UI_CTRL_PANEL_GET, 0);
    ui_ctrl_label_show_text(UI_CTRL_LABEL_LISTEN_SPEAK, "Thinking...");

    ESP_LOGI(TAG, "Triggering Audio Brain Query (%zu bytes)", len);
    gemini_init(sys_param->gemini_key);
    
    // Send audio directly to Gemini (Universal STT + Brain)
    response = gemini_audio_query(audio, len, sys_param->user_name, sys_param->user_age);

    if (NULL == response || NULL == response->text) {
        ret = ESP_ERR_INVALID_RESPONSE;
        ui_ctrl_label_show_text(UI_CTRL_LABEL_LISTEN_SPEAK, SORRY_CANNOT_UNDERSTAND);
        ui_ctrl_show_panel(UI_CTRL_PANEL_SLEEP, LISTEN_SPEAK_PANEL_DELAY_MS);
        ESP_LOGE(TAG, "[gemini_audio_query]: invalid response");
        goto err;
    }

    ESP_LOGI(TAG, "Gemini Response: %s", response->text);

    // UI display success
    ui_ctrl_label_show_text(UI_CTRL_LABEL_REPLY_QUESTION, "CIRCUIT DIGEST"); 
    ui_ctrl_label_show_text(UI_CTRL_LABEL_LISTEN_SPEAK, response->text);
    ui_ctrl_label_show_text(UI_CTRL_LABEL_REPLY_CONTENT, response->text);
    ui_ctrl_show_panel(UI_CTRL_PANEL_REPLY, 0);

    // Use Local PicoTTS
    app_tts_speak(response->text);
    ui_ctrl_reply_set_audio_start_flag(true);
    app_tts_wait_done(60000); 
    ui_ctrl_reply_set_audio_end_flag(true);
    // UI Sleep is now handled by the scroll timer in app_ui_ctrl.c

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

    ESP_LOGI(TAG, "User Persona: %s, Age: %" PRId32, sys_param->user_name, sys_param->user_age);
    gemini_init(sys_param->gemini_key);
    // wit_init(sys_param->wit_token); // Removed Wit.ai dependency

    ESP_LOGI(TAG, "speech recognition start");
    app_sr_start(false);
    audio_register_play_finish_cb(audio_play_finish_cb);

    // Initialize TTS
    app_tts_init();
    bsp_codec_volume_set(70, NULL);
    
    while (true) {
        ESP_LOGI(TAG, "Memory - Internal: %d, SPIRAM: %d",
                 heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
                 heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        vTaskDelay(pdMS_TO_TICKS(5 * 1000));
    }
}

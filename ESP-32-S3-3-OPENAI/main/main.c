#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#ifndef CHATGPT_DEMO_VERSION_MAJOR
#define CHATGPT_DEMO_VERSION_MAJOR 1
#endif

#include "app_audio.h"
#include "app_http_client.h"
#include "app_sensor.h"
#include "app_sntp.h"
#include "app_sr.h"
#include "app_ui_ctrl.h"
#include "app_wifi.h"
#include "audio_player.h"
#include "bsp/esp-bsp.h"
#include "bsp_board.h"
#include "esp_netif.h"
#include "settings.h"
#include "ui.h"

#define LISTEN_SPEAK_PANEL_DELAY_MS 2000

#include "main.h"

void ui_update_task(void *pvParameters);

static const char *TAG = "app_main";
static sys_param_t *sys_param = NULL;

// Simplified - Just show message on display (no Gemini, no TTS)
esp_err_t display_message(const char *title, const char *message) {
  ui_ctrl_label_show_text(UI_CTRL_LABEL_REPLY_QUESTION, title);
  ui_ctrl_label_show_text(UI_CTRL_LABEL_LISTEN_SPEAK, message);
  ui_ctrl_label_show_text(UI_CTRL_LABEL_REPLY_CONTENT, message);
  ui_ctrl_show_panel(UI_CTRL_PANEL_REPLY, 0);
  return ESP_OK;
}

// Called when voice wake word detected
esp_err_t on_voice_wake(void) {
  ESP_LOGI(TAG, "Voice wake detected!");
  ui_ctrl_show_panel(UI_CTRL_PANEL_LISTEN, 0);
  ui_ctrl_label_show_text(UI_CTRL_LABEL_LISTEN_SPEAK, "Listening...");
  return ESP_OK;
}

// Called when audio recording complete - just display, no Gemini
esp_err_t gemini_audio_bot_trigger(uint8_t *audio, size_t len,
                                   void *pre_nlu_res) {
  ESP_LOGI(TAG, "Audio recorded (%u bytes) - Use Streamlit for AI query",
           (unsigned int)len);
  // Placeholder - Use Streamlit for AI query

  ui_ctrl_show_panel(UI_CTRL_PANEL_GET, 0);
  ui_ctrl_label_show_text(
      UI_CTRL_LABEL_LISTEN_SPEAK,
      "Audio captured. Use Streamlit dashboard for AI query.");

  // Go back to sleep after delay
  ui_ctrl_show_panel(UI_CTRL_PANEL_SLEEP, LISTEN_SPEAK_PANEL_DELAY_MS * 2);

  return ESP_OK;
}

// Called for text queries - just display, no Gemini
esp_err_t gemini_speech_bot_trigger(const char *prompt) {
  ESP_LOGI(TAG, "Text query: %s - Use Streamlit for AI query", prompt);

  ui_ctrl_show_panel(UI_CTRL_PANEL_GET, 0);
  ui_ctrl_label_show_text(UI_CTRL_LABEL_LISTEN_SPEAK,
                          "Use Streamlit dashboard for AI queries.");
  ui_ctrl_show_panel(UI_CTRL_PANEL_SLEEP, LISTEN_SPEAK_PANEL_DELAY_MS);

  return ESP_OK;
}

#include "driver/gpio.h"
#include "driver/i2c.h"

void hw_light_set(bool on, uint32_t color) {
  ESP_LOGI(TAG, "Hardware: Light %s (Color: %06X)", on ? "ON" : "OFF",
           (unsigned int)color);
  gpio_reset_pin(GPIO_NUM_2);
  gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
  gpio_set_level(GPIO_NUM_2, on ? 1 : 0);
}

float hw_get_sensor_temp(void) { return 24.5f; }

void live_stt_callback(const char *text, bool is_final) {
  if (text) {
    ESP_LOGI(TAG, "Live STT: %s %s", text, is_final ? "[FINAL]" : "");
    ui_ctrl_label_show_text(UI_CTRL_LABEL_LISTEN_SPEAK, (char *)text);
  }
}

static void audio_play_finish_cb(void) {
  ESP_LOGI(TAG, "Audio playback complete");
  if (ui_ctrl_reply_get_audio_start_flag()) {
    ui_ctrl_reply_set_audio_end_flag(true);
  }
}

void app_main() {
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  ESP_ERROR_CHECK(settings_read_parameter_from_nvs());
  sys_param = settings_get_parameter();

  bsp_spiffs_mount();
  bsp_i2c_init();

  // Harden I2C stability after BSP initialization
  i2c_config_t i2c_conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = BSP_I2C_SDA,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_io_num = BSP_I2C_SCL,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = 50000, // Reduced to 50kHz for maximum reliability
  };
  i2c_param_config(BSP_I2C_NUM, &i2c_conf);
  i2c_set_timeout(BSP_I2C_NUM, 0xFFFFF); // Maximum hardware timeout

  // Enable internal pull-ups as an extra safety measure
  gpio_set_pull_mode(BSP_I2C_SDA, GPIO_PULLUP_ONLY);
  gpio_set_pull_mode(BSP_I2C_SCL, GPIO_PULLUP_ONLY);

  bsp_display_cfg_t cfg = {
      .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
  };
  bsp_display_start_with_config(&cfg);
  bsp_board_init();

  ESP_LOGI(TAG, "Display LVGL demo");
  bsp_display_backlight_on();
  ui_ctrl_init();
  app_network_start();

  // Init Sensors, Time, and HTTP Client (Push data to Hub)
  app_sensor_init();
  app_sntp_init();
  app_http_client_start();

  ESP_LOGI(TAG, "User Persona: %s, Age: %d", sys_param->user_name,
           (int)sys_param->user_age);
  ESP_LOGI(TAG, "WiFi SSID: %s", sys_param->ssid);

  // NO Gemini init - handled by Streamlit
  // NO TTS init - removed

  ESP_LOGI(TAG, "Speech recognition start");
  app_sr_start(false);
  audio_register_play_finish_cb(audio_play_finish_cb);

  bsp_codec_volume_set(70, NULL);

  xTaskCreatePinnedToCore(&ui_update_task, "ui_update_task", 4096, NULL, 5,
                          NULL, 1);

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(10 * 1000));
  }
}

// Global UI update task
void ui_update_task(void *pvParameters) {
  char time_str[32];
  char date_str[32];
  float temp, hum;

  while (1) {
    // 1. Update Time/Date
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(time_str, sizeof(time_str), "%H:%M", &timeinfo);
    strftime(date_str, sizeof(date_str), "%a, %b %d", &timeinfo);

    // 2. Update Sensors
    app_sensor_get_values(&temp, &hum);

    // 3. Update UI Labels (Safely with Lock)
    if (bsp_display_lock(0)) {
      if (ui_LabelTime)
        lv_label_set_text(ui_LabelTime, time_str);
      if (ui_LabelDate)
        lv_label_set_text(ui_LabelDate, date_str);

      char sensor_str[32];
      snprintf(sensor_str, sizeof(sensor_str), "%.1f°C", temp);
      if (ui_LabelTempValue)
        lv_label_set_text(ui_LabelTempValue, sensor_str);

      snprintf(sensor_str, sizeof(sensor_str), "%.0f%%", hum);
      if (ui_LabelHumValue)
        lv_label_set_text(ui_LabelHumValue, sensor_str);
      bsp_display_unlock();
    }

    vTaskDelay(pdMS_TO_TICKS(2000)); // Reduced frequency to 2 seconds
  }
}

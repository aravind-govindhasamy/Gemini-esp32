#include "app_sensor.h"
#include "app_wifi.h"
#include "cJSON.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "app_http_client";

static void http_push_task(void *pvParameters) {
  sys_param_t *param = settings_get_parameter();

  while (1) {
    if (wifi_connected_already() != WIFI_STATUS_CONNECTED_OK) {
      ESP_LOGW(TAG, "Waiting for WiFi connection...");
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    float temp, hum, lux;
    bool presence;

    app_sensor_get_values(&temp, &hum);
    lux = app_sensor_get_lux();
    presence = app_sensor_get_presence();

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "temp", temp);
    cJSON_AddNumberToObject(root, "hum", hum);
    cJSON_AddNumberToObject(root, "lux", lux);
    cJSON_AddBoolToObject(root, "presence", presence);

    char *json_str = cJSON_PrintUnformatted(root);

    // Use HUB_IP from settings or hardcoded for now
    // Assuming user will set HUB_IP in their .env/settings
    char url[128];
    snprintf(url, sizeof(url), "http://%s:8000/update", param->hub_ip);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_str, strlen(json_str));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
      ESP_LOGI(TAG, "✅ HTTP POST Status = %d, content_length = %lld",
               esp_http_client_get_status_code(client),
               esp_http_client_get_content_length(client));
    } else {
      ESP_LOGE(TAG, "❌ HTTP POST request failed to %s: %s", url,
               esp_err_to_name(err));
      if (err == ESP_ERR_HTTP_CONNECT) {
        ESP_LOGW(TAG, "Target Hub is unreachable. Check firewall/port 8000.");
      }
    }

    esp_http_client_cleanup(client);
    cJSON_Delete(root);
    free(json_str);

    vTaskDelay(pdMS_TO_TICKS(60000)); // Push every 60 seconds
  }
}

void app_http_client_start(void) {
  xTaskCreatePinnedToCore(&http_push_task, "http_push_task", 8192, NULL, 5,
                          NULL, 1);
}

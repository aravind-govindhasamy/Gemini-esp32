#include "app_sensor.h"
#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "app_http_client";

static void http_push_task(void *pvParameters) {
  char post_data[256];
  sys_param_t *param = settings_get_parameter();

  while (1) {
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
    snprintf(url, sizeof(url), "http://192.168.32.10:8000/update");

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_str, strlen(json_str));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
      ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %lld",
               esp_http_client_get_status_code(client),
               esp_http_client_get_content_length(client));
    } else {
      ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    cJSON_Delete(root);
    free(json_str);

    vTaskDelay(pdMS_TO_TICKS(5000)); // Push every 5 seconds
  }
}

void app_http_client_start(void) {
  xTaskCreate(http_push_task, "http_push_task", 8192, NULL, 5, NULL);
}

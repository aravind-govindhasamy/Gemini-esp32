#include "app_httpd.h"
#include "../settings/settings.h"
#include "app_sensor.h"
#include "app_sntp.h"
#include "app_wifi.h"
#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "app_httpd";
static httpd_handle_t server = NULL;

// GET /status
static esp_err_t status_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "Request: GET /status");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_sendstr(req, "{\"status\":\"online\",\"version\":\"1.0\"}");
  return ESP_OK;
}

// GET /sensors
static esp_err_t sensors_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "Request: GET /sensors");
  float temp, hum;
  app_sensor_get_values(&temp, &hum);
  float lux = app_sensor_get_lux();
  bool presence = app_sensor_get_presence();

  cJSON *root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "temp", temp);
  cJSON_AddNumberToObject(root, "hum", hum);
  cJSON_AddNumberToObject(root, "lux", lux);
  cJSON_AddBoolToObject(root, "presence", presence);

  char *json = cJSON_PrintUnformatted(root);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_sendstr(req, json);

  free(json);
  cJSON_Delete(root);
  return ESP_OK;
}

// POST /voice/activate
static esp_err_t voice_activate_handler(httpd_req_t *req) {
  extern void EventPanelSleepClickCb(void *e);
  EventPanelSleepClickCb(NULL);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_sendstr(req, "{\"success\":true}");
  return ESP_OK;
}

// POST /settings/time
static esp_err_t settings_time_handler(httpd_req_t *req) {
  char buf[100];
  int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
  if (ret <= 0)
    return ESP_FAIL;
  buf[ret] = '\0';

  cJSON *root = cJSON_Parse(buf);
  if (root) {
    cJSON *fmt = cJSON_GetObjectItem(root, "format_24hr");
    if (fmt)
      app_sntp_set_24hr_mode(cJSON_IsTrue(fmt));
    cJSON_Delete(root);
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_sendstr(req, "{\"success\":true}");
  return ESP_OK;
}

// POST /settings/wifi
static esp_err_t settings_wifi_handler(httpd_req_t *req) {
  char buf[256];
  int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
  if (ret <= 0)
    return ESP_FAIL;
  buf[ret] = '\0';

  cJSON *root = cJSON_Parse(buf);
  if (root) {
    sys_param_t *param = settings_get_parameter();
    cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
    cJSON *pass = cJSON_GetObjectItem(root, "password");
    if (ssid && ssid->valuestring) {
      strncpy(param->ssid, ssid->valuestring, 31);
      param->ssid[31] = '\0';
    }
    if (pass && pass->valuestring) {
      strncpy(param->password, pass->valuestring, 63);
      param->password[63] = '\0';
    }
    settings_write_parameter_to_nvs();
    cJSON_Delete(root);
    ESP_LOGI(TAG, "WiFi credentials updated");
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_sendstr(req, "{\"success\":true}");
  return ESP_OK;
}

// POST /settings/apikey
static esp_err_t settings_apikey_handler(httpd_req_t *req) {
  char buf[256];
  int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
  if (ret <= 0)
    return ESP_FAIL;
  buf[ret] = '\0';

  cJSON *root = cJSON_Parse(buf);
  if (root) {
    sys_param_t *param = settings_get_parameter();
    cJSON *key = cJSON_GetObjectItem(root, "key");
    if (key && key->valuestring) {
      strncpy(param->gemini_key, key->valuestring, 127);
      param->gemini_key[127] = '\0';
    }
    settings_write_parameter_to_nvs();
    cJSON_Delete(root);
    ESP_LOGI(TAG, "Gemini API key updated");
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_sendstr(req, "{\"success\":true}");
  return ESP_OK;
}

// CORS preflight
static esp_err_t cors_handler(httpd_req_t *req) {
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

esp_err_t app_httpd_init(void) {
  ESP_LOGI(TAG, "Starting HTTP server on port 80...");
  // 2. I2C Bus check
  // bsp_i2c_init() is already called in main.c, so the driver is installed.
  // We just need to ensure the pins 8/18 are used.
  ESP_LOGI(TAG, "Sensors using I2C_NUM_0 (Pins 8/18)");
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 16;
  config.stack_size = 8192; // Enough for JSON/CORS
  config.task_priority = 5;
  config.lru_purge_enable = true;
  config.max_open_sockets = 4; // LWIP limit is low on ESP-BOX-3

  if (httpd_start(&server, &config) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start HTTP server!");
    return ESP_FAIL;
  }

  httpd_uri_t uris[] = {
      {"/status", HTTP_GET, status_handler, NULL},
      {"/sensors", HTTP_GET, sensors_handler, NULL},
      {"/voice/activate", HTTP_POST, voice_activate_handler, NULL},
      {"/settings/time", HTTP_POST, settings_time_handler, NULL},
      {"/settings/wifi", HTTP_POST, settings_wifi_handler, NULL},
      {"/settings/apikey", HTTP_POST, settings_apikey_handler, NULL},
      {"/status", HTTP_OPTIONS, cors_handler, NULL},
      {"/sensors", HTTP_OPTIONS, cors_handler, NULL},
      {"/settings/wifi", HTTP_OPTIONS, cors_handler, NULL},
      {"/settings/apikey", HTTP_OPTIONS, cors_handler, NULL},
  };

  for (int i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
    httpd_register_uri_handler(server, &uris[i]);
  }

  ESP_LOGI(TAG, "HTTP server started");
  return ESP_OK;
}

void app_httpd_stop(void) {
  if (server) {
    httpd_stop(server);
    server = NULL;
  }
}

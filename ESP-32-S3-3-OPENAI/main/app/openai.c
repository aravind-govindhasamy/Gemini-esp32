#include <string.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "openai.h"

static const char *TAG = "openai_client";
static char g_api_key[165] = {0};

void openai_init(const char *api_key) {
    if (api_key) {
        strncpy(g_api_key, api_key, sizeof(g_api_key) - 1);
    }
}

gemini_response_t* openai_tts_query(const char *text) {
    if (strlen(g_api_key) == 0 || !text) {
        ESP_LOGE(TAG, "OpenAI key not set or empty text");
        return NULL;
    }

    ESP_LOGI(TAG, "Querying OpenAI TTS for text: %s", text);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", "tts-1");
    cJSON_AddStringToObject(root, "input", text);
    cJSON_AddStringToObject(root, "voice", "shimmer");
    cJSON_AddStringToObject(root, "response_format", "mp3");

    char *post_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    esp_http_client_config_t config = {
        .url = "https://api.openai.com/v1/audio/speech",
        .method = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 30000,
        .buffer_size = 10240,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_http_client_set_header(client, "Content-Type", "application/json");
    
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", g_api_key);
    esp_http_client_set_header(client, "Authorization", auth_header);

    gemini_response_t *res = NULL;
    esp_err_t err = esp_http_client_open(client, strlen(post_data));
    if (err == ESP_OK) {
        esp_http_client_fetch_headers(client);
        int status = esp_http_client_get_status_code(client);
        
        if (status == 200) {
            int content_len = esp_http_client_get_content_length(client);
            if (content_len > 0) {
                res = (gemini_response_t*)calloc(1, sizeof(gemini_response_t));
                res->audio = (uint8_t*)malloc(content_len);
                res->audio_len = content_len;
                
                int total_read = 0;
                while (total_read < content_len) {
                    int read = esp_http_client_read(client, (char*)res->audio + total_read, content_len - total_read);
                    if (read <= 0) break;
                    total_read += read;
                }
                ESP_LOGI(TAG, "OpenAI TTS Success: Received %d bytes", total_read);
            }
        } else {
            char error_buf[512];
            esp_http_client_read(client, error_buf, sizeof(error_buf)-1);
            ESP_LOGE(TAG, "OpenAI TTS Failed (%d): %s", status, error_buf);
        }
    }
    
    esp_http_client_cleanup(client);
    free(post_data);
    return res;
}

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "gemini.h"
#include "mbedtls/base64.h"

static const char *TAG = "gemini_client";
static char *g_api_key = NULL;

esp_err_t gemini_init(const char *api_key) {
    if (g_api_key) free(g_api_key);
    
    // Trim potential whitespace
    const char *start = api_key;
    while (*start == ' ') start++;
    char *trimmed = strdup(start);
    char *end = trimmed + strlen(trimmed) - 1;
    while (end > trimmed && (*end == ' ' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }
    g_api_key = trimmed;
    ESP_LOGI(TAG, "Gemini initialized");
    return ESP_OK;
}

char* gemini_audio_query(uint8_t *audio, size_t len) {
    if (!g_api_key || !audio) return NULL;

    size_t total_len = len + 44; // Including WAV header

    // 1. Base64 encode the audio
    size_t out_len = 0;
    mbedtls_base64_encode(NULL, 0, &out_len, audio, total_len);
    char *b64_audio = malloc(out_len + 1);
    if (!b64_audio) return NULL;
    mbedtls_base64_encode((uint8_t*)b64_audio, out_len, &out_len, audio, total_len);
    b64_audio[out_len] = '\0';

    // 2. Build Multimodal JSON
    cJSON *root = cJSON_CreateObject();
    cJSON *contents = cJSON_CreateArray();
    cJSON *content = cJSON_CreateObject();
    cJSON *parts = cJSON_CreateArray();
    
    cJSON *part_text = cJSON_CreateObject();
    cJSON_AddStringToObject(part_text, "text", "You are a friendly companion for a child. Listen and reply briefly.");
    cJSON_AddItemToArray(parts, part_text);

    cJSON *part_audio = cJSON_CreateObject();
    cJSON *inline_data = cJSON_CreateObject();
    cJSON_AddStringToObject(inline_data, "mime_type", "audio/wav");
    cJSON_AddStringToObject(inline_data, "data", b64_audio);
    cJSON_AddItemToObject(part_audio, "inline_data", inline_data);
    cJSON_AddItemToArray(parts, part_audio);

    cJSON_AddItemToObject(content, "parts", parts);
    cJSON_AddItemToArray(contents, content);
    cJSON_AddItemToObject(root, "contents", contents);

    char *post_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    free(b64_audio);

    // 3. Send HTTP Request
    char url[256];
    // Using v1beta for gemini-2.5-flash (which shows usage in your dashboard)
    snprintf(url, sizeof(url), "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=%s", g_api_key);
    
    ESP_LOGI(TAG, "Querying Gemini (gemini-2.5-flash)...");

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 30000, 
        .buffer_size = 4096,
        .buffer_size_tx = 4096,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");

    esp_err_t err = esp_http_client_open(client, strlen(post_data));
    char *result_text = NULL;
    if (err == ESP_OK) {
        esp_http_client_write(client, post_data, strlen(post_data));
        esp_http_client_fetch_headers(client);
        int status = esp_http_client_get_status_code(client);
        
        char *response_buffer = malloc(8192);
        int read_len = esp_http_client_read(client, response_buffer, 8191);
        if (read_len > 0) {
            response_buffer[read_len] = '\0';
            ESP_LOGI(TAG, "HTTP Status: %d, Response: %s", status, response_buffer);

            if (status == 200) {
                cJSON *resp_root = cJSON_Parse(response_buffer);
                if (resp_root) {
                    cJSON *candidates = cJSON_GetObjectItem(resp_root, "candidates");
                    if (cJSON_IsArray(candidates) && cJSON_GetArraySize(candidates) > 0) {
                        cJSON *first_candidate = cJSON_GetArrayItem(candidates, 0);
                        cJSON *content_obj = cJSON_GetObjectItem(first_candidate, "content");
                        if (content_obj) {
                            cJSON *parts_arr = cJSON_GetObjectItem(content_obj, "parts");
                            if (cJSON_IsArray(parts_arr) && cJSON_GetArraySize(parts_arr) > 0) {
                                cJSON *first_part = cJSON_GetArrayItem(parts_arr, 0);
                                cJSON *text_obj = cJSON_GetObjectItem(first_part, "text");
                                if (cJSON_IsString(text_obj)) {
                                    result_text = strdup(text_obj->valuestring);
                                }
                            }
                        }
                    }
                    cJSON_Delete(resp_root);
                }
            }
        } else {
             ESP_LOGE(TAG, "No body received. HTTP Status: %d", status);
        }
        free(response_buffer);
    } else {
        ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
    }

    free(post_data);
    esp_http_client_cleanup(client);
    return result_text;
}

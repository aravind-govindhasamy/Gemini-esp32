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

void gemini_response_free(gemini_response_t *res) {
    if (!res) return;
    if (res->text) free(res->text);
    if (res->audio) free(res->audio);
    free(res);
}

static gemini_response_t* gemini_query_interactions(cJSON *contents_item) {
    if (!g_api_key || !contents_item) return NULL;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", "gemini-3-flash-preview");
    cJSON *input = cJSON_AddArrayToObject(root, "input");
    cJSON *turn = cJSON_CreateObject();
    cJSON_AddStringToObject(turn, "role", "user");
    cJSON_AddItemToObject(turn, "content", contents_item);
    cJSON_AddItemToArray(input, turn);

    cJSON *modalities = cJSON_AddArrayToObject(root, "response_modalities");
    cJSON_AddItemToArray(modalities, cJSON_CreateString("text"));
    cJSON_AddItemToArray(modalities, cJSON_CreateString("audio"));

    char *post_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    char url[256];
    snprintf(url, sizeof(url), "https://generativelanguage.googleapis.com/v1beta/interactions?key=%s", g_api_key);
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 45000, 
        .buffer_size = 10240,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");

    esp_err_t err = esp_http_client_open(client, strlen(post_data));
    gemini_response_t *res = NULL;
    if (err == ESP_OK) {
        esp_http_client_write(client, post_data, strlen(post_data));
        esp_http_client_fetch_headers(client);
        int status = esp_http_client_get_status_code(client);
        
        char *response_buffer = malloc(256 * 1024);
        int total_received = 0;
        int read_len;
        while ((read_len = esp_http_client_read(client, response_buffer + total_received, 2048)) > 0) {
            total_received += read_len;
            if (total_received > 255 * 1024) break;
        }
        
        if (total_received > 0) {
            response_buffer[total_received] = '\0';
            if (status == 200 || status == 201) {
                cJSON *resp_root = cJSON_Parse(response_buffer);
                if (resp_root) {
                    cJSON *outputs = cJSON_GetObjectItem(resp_root, "outputs");
                    if (cJSON_IsArray(outputs)) {
                        res = calloc(1, sizeof(gemini_response_t));
                        for (int i = 0; i < cJSON_GetArraySize(outputs); i++) {
                            cJSON *output = cJSON_GetArrayItem(outputs, i);
                            cJSON *type = cJSON_GetObjectItem(output, "type");
                            if (cJSON_IsString(type)) {
                                if (strcmp(type->valuestring, "text") == 0) {
                                    cJSON *text = cJSON_GetObjectItem(output, "text");
                                    if (cJSON_IsString(text)) res->text = strdup(text->valuestring);
                                } else if (strcmp(type->valuestring, "audio") == 0) {
                                    cJSON *audio_field = cJSON_GetObjectItem(output, "audio");
                                    if (audio_field && cJSON_IsString(audio_field)) {
                                        size_t b64_len = strlen(audio_field->valuestring);
                                        res->audio = malloc(b64_len);
                                        mbedtls_base64_decode(res->audio, b64_len, &res->audio_len, (uint8_t*)audio_field->valuestring, b64_len);
                                    } else if (audio_field && cJSON_IsObject(audio_field)) {
                                        cJSON *b64_data = cJSON_GetObjectItem(audio_field, "data");
                                        if (cJSON_IsString(b64_data)) {
                                            size_t b64_len = strlen(b64_data->valuestring);
                                            res->audio = malloc(b64_len);
                                            mbedtls_base64_decode(res->audio, b64_len, &res->audio_len, (uint8_t*)b64_data->valuestring, b64_len);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    cJSON_Delete(resp_root);
                }
            } else {
                ESP_LOGE(TAG, "Gemini Error %d: %s", status, response_buffer);
            }
        }
        free(response_buffer);
    }
    free(post_data);
    esp_http_client_cleanup(client);
    return res;
}

gemini_response_t* gemini_text_query(const char *text) {
    ESP_LOGI(TAG, "Querying Gemini with TEXT...");
    cJSON *content = cJSON_CreateArray();
    cJSON *part_sys = cJSON_CreateObject();
    cJSON_AddStringToObject(part_sys, "type", "text");
    cJSON_AddStringToObject(part_sys, "text", "You are a friendly companion for a child. Be fun and safe. Listen and reply briefly.");
    cJSON_AddItemToArray(content, part_sys);
    
    cJSON *part_user = cJSON_CreateObject();
    cJSON_AddStringToObject(part_user, "type", "text");
    cJSON_AddStringToObject(part_user, "text", text);
    cJSON_AddItemToArray(content, part_user);
    
    return gemini_query_interactions(content);
}

gemini_response_t* gemini_audio_query(uint8_t *audio, size_t len) {
    // Redundant as per user constraint
    return NULL;
}

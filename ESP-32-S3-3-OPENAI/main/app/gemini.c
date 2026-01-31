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

static gemini_response_t* gemini_query_brain(cJSON *contents) {
    if (!g_api_key || !contents) return NULL;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "contents", contents);
    
    // Request audio and text output (Standard for Gemini 2.0 Flash / Pro)
    cJSON *gen_cfg = cJSON_AddObjectToObject(root, "generationConfig");
    
    // Add Thinking Config to reduce latency (Gemini 3 Flash specific)
    cJSON *think_cfg = cJSON_AddObjectToObject(gen_cfg, "thinkingConfig");
    cJSON_AddStringToObject(think_cfg, "thinkingLevel", "low");

    cJSON *modalities = cJSON_AddArrayToObject(gen_cfg, "response_modalities");
    cJSON_AddItemToArray(modalities, cJSON_CreateString("text"));
    cJSON_AddItemToArray(modalities, cJSON_CreateString("audio"));

    char *post_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    char url[256];
    // Use the model provided by the user in their script
    snprintf(url, sizeof(url), "https://generativelanguage.googleapis.com/v1beta/models/gemini-3-flash-preview:generateContent?key=%s", g_api_key);
    
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
        
        // Response can be large if audio is included
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
                    // Standard generateContent response parsing
                    cJSON *candidates = cJSON_GetObjectItem(resp_root, "candidates");
                    if (cJSON_IsArray(candidates) && cJSON_GetArraySize(candidates) > 0) {
                        cJSON *candidate = cJSON_GetArrayItem(candidates, 0);
                        cJSON *content = cJSON_GetObjectItem(candidate, "content");
                        cJSON *parts = cJSON_GetObjectItem(content, "parts");
                        
                        if (cJSON_IsArray(parts)) {
                            res = calloc(1, sizeof(gemini_response_t));
                            for (int i = 0; i < cJSON_GetArraySize(parts); i++) {
                                cJSON *part = cJSON_GetArrayItem(parts, i);
                                
                                // Text field
                                cJSON *text = cJSON_GetObjectItem(part, "text");
                                if (cJSON_IsString(text)) {
                                    if (res->text) {
                                        // Append if multiple text parts
                                        char *new_text = malloc(strlen(res->text) + strlen(text->valuestring) + 1);
                                        strcpy(new_text, res->text);
                                        strcat(new_text, text->valuestring);
                                        free(res->text);
                                        res->text = new_text;
                                    } else {
                                        res->text = strdup(text->valuestring);
                                    }
                                }
                                
                                // Audio field (inline_data)
                                cJSON *inline_data = cJSON_GetObjectItem(part, "inline_data");
                                if (inline_data) {
                                    cJSON *mime = cJSON_GetObjectItem(inline_data, "mime_type");
                                    cJSON *data = cJSON_GetObjectItem(inline_data, "data");
                                    if (cJSON_IsString(data)) {
                                        size_t b64_len = strlen(data->valuestring);
                                        res->audio = (uint8_t*)malloc(b64_len);
                                        mbedtls_base64_decode(res->audio, b64_len, &res->audio_len, (uint8_t*)data->valuestring, b64_len);
                                        ESP_LOGI(TAG, "Decoded audio part (%zu bytes, %s)", res->audio_len, mime ? mime->valuestring : "unknown");
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

gemini_response_t* gemini_text_query(const char *text, const char *name, int age) {
    if (!text || strlen(text) == 0) {
        ESP_LOGW(TAG, "Empty text query, skipping Gemini call.");
        return NULL;
    }
    ESP_LOGI(TAG, "Querying Brain (Text)...");
    
    cJSON *contents = cJSON_CreateArray();
    cJSON *turn = cJSON_CreateObject();
    cJSON_AddStringToObject(turn, "role", "user");
    cJSON *parts = cJSON_AddArrayToObject(turn, "parts");
    
    // Part 1: System Persona
    cJSON *p_sys = cJSON_CreateObject();
    char sys_prompt[512];
    snprintf(sys_prompt, sizeof(sys_prompt), 
        "You are a friendly companion for a child. Be fun and safe. Listen and reply briefly. "
        "The child's name is %s and they are %d years old. Tailor your language to be age-appropriate.",
        name ? name : "Friend", age > 0 ? age : 7);
    cJSON_AddStringToObject(p_sys, "text", sys_prompt);
    cJSON_AddItemToArray(parts, p_sys);
    
    // Part 2: User Input
    cJSON *p_user = cJSON_CreateObject();
    cJSON_AddStringToObject(p_user, "text", text);
    cJSON_AddItemToArray(parts, p_user);
    
    cJSON_AddItemToArray(contents, turn);
    
    return gemini_query_brain(contents);
}

gemini_response_t* gemini_audio_query(uint8_t *audio, size_t len, const char *name, int age) {
    if (!audio || len == 0) return NULL;
    
    ESP_LOGI(TAG, "Querying Brain (Audio: %zu bytes)...", len);
    
    cJSON *contents = cJSON_CreateArray();
    cJSON *turn = cJSON_CreateObject();
    cJSON_AddStringToObject(turn, "role", "user");
    cJSON *parts = cJSON_AddArrayToObject(turn, "parts");
    
    // Part 1: System Persona
    cJSON *p_sys = cJSON_CreateObject();
    char sys_prompt[512];
    snprintf(sys_prompt, sizeof(sys_prompt), 
        "You are a friendly companion for a child. Be fun and safe. Listen to the audio and reply briefly. "
        "The child's name is %s and they are %d years old. Tailor your language to be age-appropriate.",
        name ? name : "Friend", age > 0 ? age : 7);
    cJSON_AddStringToObject(p_sys, "text", sys_prompt);
    cJSON_AddItemToArray(parts, p_sys);
    
    // Part 2: Multimodal Audio (inline_data)
    cJSON *p_audio = cJSON_CreateObject();
    cJSON *inline_data = cJSON_AddObjectToObject(p_audio, "inline_data");
    cJSON_AddStringToObject(inline_data, "mime_type", "audio/wav"); 
    
    size_t b64_len = 0;
    mbedtls_base64_encode(NULL, 0, &b64_len, audio, len);
    char *b64_data = (char*)malloc(b64_len + 1);
    mbedtls_base64_encode((uint8_t*)b64_data, b64_len + 1, &b64_len, audio, len);
    b64_data[b64_len] = '\0';
    
    cJSON_AddStringToObject(inline_data, "data", b64_data);
    free(b64_data);
    cJSON_AddItemToArray(parts, p_audio);
    
    cJSON_AddItemToArray(contents, turn);
    
    return gemini_query_brain(contents);
}


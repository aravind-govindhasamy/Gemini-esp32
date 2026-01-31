#include <string.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "wit.h"

#define KEY_SIZE 165

typedef struct {
    char *buf;
    int size;
} response_collector_t;

static const char *TAG = "wit_client";
static char g_token[KEY_SIZE] = {0};
static esp_http_client_handle_t g_stream_client = NULL;
static bool g_stream_ready = false;
static wit_stt_callback_t g_stt_cb = NULL;
static response_collector_t g_stream_collector = {0};

void wit_init(const char *token) {
    if (token && strlen(token) > 0) {
        strncpy(g_token, token, sizeof(g_token) - 1);
        ESP_LOGI(TAG, "Wit initialized (token starts with %.4s...)", g_token);
    } else {
        ESP_LOGE(TAG, "Wit init failed: empty token");
    }
}

static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    response_collector_t *collector = (response_collector_t *)evt->user_data;
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_DATA:
            if (collector && evt->data_len > 0) {
                collector->buf = realloc(collector->buf, collector->size + evt->data_len + 1);
                memcpy(collector->buf + collector->size, evt->data, evt->data_len);
                collector->size += evt->data_len;
                collector->buf[collector->size] = '\0';
                
                // Live STT: Try to parse partial JSON objects from the stream
                // Wit.ai sends multiple JSON objects back-to-back.
                if (g_stt_cb) {
                    // Find the START of the LAST JSON object in the buffer
                    char *last_brace = strrchr(collector->buf, '{');
                    if (last_brace) {
                        cJSON *root = cJSON_Parse(last_brace);
                        if (root) {
                            cJSON *text = cJSON_GetObjectItem(root, "text");
                            cJSON *type = cJSON_GetObjectItem(root, "type");
                            bool is_final = (type && strcmp(type->valuestring, "FINAL_TRANSCRIPTION") == 0);
                            if (cJSON_IsString(text) && strlen(text->valuestring) > 0) {
                                g_stt_cb(text->valuestring, is_final || (type && strstr(type->valuestring, "FINAL")));
                            }
                            cJSON_Delete(root);
                        }
                    }
                }
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

wit_nlu_result_t* wit_stt_query(uint8_t *pcm_audio, size_t len) {
    if (strlen(g_token) == 0 || !pcm_audio) {
        ESP_LOGE(TAG, "Wit token not set or no audio");
        return NULL;
    }

    ESP_LOGI(TAG, "Querying Wit.ai STT (%zu bytes)", len);

    response_collector_t collector = { .buf = NULL, .size = 0 };

    esp_http_client_config_t config = {
        .url = "https://api.wit.ai/speech?v=20260131",
        .method = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 30000,
        .buffer_size = 10240,
        .event_handler = _http_event_handler,
        .user_data = &collector,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "audio/raw;encoding=signed-integer;bits=16;rate=16000;endian=little");
    
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", g_token);
    esp_http_client_set_header(client, "Authorization", auth_header);
    
    // Set post field for raw audio
    esp_http_client_set_post_field(client, (const char*)pcm_audio, (int)len);

    ESP_LOGI(TAG, "Sending STT request...");

    wit_nlu_result_t *res = NULL;
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        if (status == 200 && collector.buf) {
            ESP_LOGI(TAG, "Response JSON: %s", collector.buf);
            cJSON *root = cJSON_Parse(collector.buf);
            if (root) {
                res = (wit_nlu_result_t*)calloc(1, sizeof(wit_nlu_result_t));
                
                cJSON *text = cJSON_GetObjectItem(root, "text");
                if (cJSON_IsString(text)) {
                    res->text = strdup(text->valuestring);
                }
                
                cJSON *intents = cJSON_GetObjectItem(root, "intents");
                if (cJSON_IsArray(intents) && cJSON_GetArraySize(intents) > 0) {
                    cJSON *intent_obj = cJSON_GetArrayItem(intents, 0);
                    cJSON *name = cJSON_GetObjectItem(intent_obj, "name");
                    cJSON *conf = cJSON_GetObjectItem(intent_obj, "confidence");
                    if (cJSON_IsString(name)) {
                        res->intent = strdup(name->valuestring);
                        res->intent_conf = (float)conf->valuedouble;
                        ESP_LOGI(TAG, "Detected Intent: %s (conf: %.2f)", res->intent, res->intent_conf);
                    }
                }
                cJSON_Delete(root);
            }
        } else {
            ESP_LOGE(TAG, "Wit STT failed with HTTP status %d, size %d", status, collector.size);
        }
    } else {
        ESP_LOGE(TAG, "Wit STT HTTP perform failed: %s", esp_err_to_name(err));
    }
    
    if (collector.buf) free(collector.buf);
    esp_http_client_cleanup(client);
    return res;
}

void wit_nlu_result_free(wit_nlu_result_t *res) {
    if (res) {
        if (res->text) free(res->text);
        if (res->intent) free(res->intent);
        free(res);
    }
}

esp_err_t wit_stt_stream_start(wit_stt_callback_t cb) {
    if (g_stream_client) return ESP_ERR_INVALID_STATE;
    g_stt_cb = cb;
    g_stream_ready = false;
    memset(&g_stream_collector, 0, sizeof(response_collector_t));

    esp_http_client_config_t config = {
        .url = "https://api.wit.ai/speech?v=20260131",
        .method = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 30000,
        .buffer_size = 4096, // Smaller buffer for more frequent chunks
        .event_handler = _http_event_handler,
        .user_data = &g_stream_collector,
    };
    g_stream_client = esp_http_client_init(&config);
    esp_http_client_set_header(g_stream_client, "Content-Type", "audio/raw;encoding=signed-integer;bits=16;rate=16000;endian=little");
    esp_http_client_set_header(g_stream_client, "Transfer-Encoding", "chunked");
    
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", g_token);
    esp_http_client_set_header(g_stream_client, "Authorization", auth_header);

    esp_err_t err = esp_http_client_open(g_stream_client, -1); // -1 for chunked
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open stream: %s", esp_err_to_name(err));
        esp_http_client_cleanup(g_stream_client);
        g_stream_client = NULL;
        return err;
    }
    g_stream_ready = true;
    ESP_LOGI(TAG, "STT Stream Opened");
    return ESP_OK;
}

esp_err_t wit_stt_stream_feed(uint8_t *pcm_audio, size_t len) {
    if (!g_stream_client || !g_stream_ready) return ESP_ERR_INVALID_STATE;
    
    int written = esp_http_client_write(g_stream_client, (const char*)pcm_audio, (int)len);
    if (written < 0) {
        // Only log error once if we lose connection
        static uint32_t last_fail = 0;
        if (esp_log_timestamp() - last_fail > 1000) {
            ESP_LOGE(TAG, "Stream feed failed (Connection Reset?)");
            last_fail = esp_log_timestamp();
        }
        g_stream_ready = false;
        return ESP_FAIL;
    }
    
    // Periodically poll for incoming data (don't block the audio task too much)
    static uint32_t last_poll = 0;
    if (esp_log_timestamp() - last_poll > 250) {
        esp_http_client_fetch_headers(g_stream_client);
        last_poll = esp_log_timestamp();
    }
    
    return ESP_OK;
}

wit_nlu_result_t* wit_stt_stream_stop(void) {
    if (!g_stream_client) return NULL;
    g_stream_ready = false;
    
    // Close the chunked request by sending final zero-length chunk
    esp_http_client_fetch_headers(g_stream_client); // Final check for data
    
    wit_nlu_result_t *res = NULL;
    
    // Wit.ai streaming results are messy. We need to find the LAST valid JSON object
    // in the collector buffer that contains "FINAL_UNDERSTANDING" or similar.
    if (g_stream_collector.buf) {
        char *ptr = g_stream_collector.buf;
        while (ptr) {
            char *next_brace = strstr(ptr, "{");
            if (!next_brace) break;
            
            cJSON *root = cJSON_Parse(next_brace);
            if (root) {
                cJSON *type = cJSON_GetObjectItem(root, "type");
                if (type && strstr(type->valuestring, "FINAL")) {
                    // This is likely our winner.
                    if (!res) res = (wit_nlu_result_t*)calloc(1, sizeof(wit_nlu_result_t));
                    
                    cJSON *text = cJSON_GetObjectItem(root, "text");
                    if (cJSON_IsString(text)) {
                        if (res->text) free(res->text);
                        res->text = strdup(text->valuestring);
                    }
                    
                    cJSON *intents = cJSON_GetObjectItem(root, "intents");
                    if (cJSON_IsArray(intents) && cJSON_GetArraySize(intents) > 0) {
                        cJSON *intent_obj = cJSON_GetArrayItem(intents, 0);
                        cJSON *name = cJSON_GetObjectItem(intent_obj, "name");
                        cJSON *conf = cJSON_GetObjectItem(intent_obj, "confidence");
                        if (cJSON_IsString(name)) {
                            if (res->intent) free(res->intent);
                            res->intent = strdup(name->valuestring);
                            res->intent_conf = (float)conf->valuedouble;
                        }
                    }
                }
                cJSON_Delete(root);
            }
            ptr = next_brace + 1; // Move past this brace to find the next object
        }
    }

    if (g_stream_collector.buf) free(g_stream_collector.buf);
    esp_http_client_cleanup(g_stream_client);
    g_stream_client = NULL;
    g_stt_cb = NULL;
    memset(&g_stream_collector, 0, sizeof(response_collector_t));
    
    return res;
}

gemini_response_t* wit_tts_query(const char *text) {
    if (strlen(g_token) == 0 || !text) {
        ESP_LOGE(TAG, "Wit token not set or empty text");
        return NULL;
    }

    ESP_LOGI(TAG, "Querying Wit.ai TTS for: %s", text);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "q", text);
    cJSON_AddStringToObject(root, "voice", "Rebecca"); // Default good voice
    char *post_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    ESP_LOGI(TAG, "Request URL: https://api.wit.ai/synthesize?v=20260131");
    ESP_LOGI(TAG, "Request Body: %s", post_data);

    response_collector_t collector = { .buf = NULL, .size = 0 };

    esp_http_client_config_t config = {
        .url = "https://api.wit.ai/synthesize?v=20260131",
        .method = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 30000,
        .buffer_size = 10240,
        .event_handler = _http_event_handler,
        .user_data = &collector,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Accept", "audio/mpeg");
    
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", g_token);
    esp_http_client_set_header(client, "Authorization", auth_header);

    gemini_response_t *res = NULL;
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        if (status == 200 && collector.buf) {
            res = (gemini_response_t*)calloc(1, sizeof(gemini_response_t));
            res->audio = (uint8_t*)collector.buf;
            res->audio_len = collector.size;
            // Prevent freeing the buffer in this function since it's now owned by res
            collector.buf = NULL; 
            ESP_LOGI(TAG, "Wit TTS Success: %d bytes", res->audio_len);
        } else {
            ESP_LOGE(TAG, "Wit TTS failed with HTTP status %d", status);
        }
    } else {
        ESP_LOGE(TAG, "Wit TTS HTTP perform failed: %s", esp_err_to_name(err));
    }
    
    if (collector.buf) free(collector.buf);
    esp_http_client_cleanup(client);
    free(post_data);
    return res;
}

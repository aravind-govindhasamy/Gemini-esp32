#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "wit.h"

#define KEY_SIZE 165
#define STT_RING_BUFFER_SIZE (64 * 1024)

typedef struct {
    char *buf;
    int size;
} response_collector_t;

static const char *TAG = "wit_client";
static char g_token[KEY_SIZE] = {0};
static esp_http_client_handle_t g_stream_client = NULL;
static bool g_stream_ready = false;
static bool g_stream_active = false;
static wit_stt_callback_t g_stt_cb = NULL;
static response_collector_t g_stream_collector = {0};
static RingbufHandle_t g_stream_rb = NULL;
static TaskHandle_t g_stream_task_handle = NULL;

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

static wit_nlu_result_t* _parse_wit_response(const char *buf) {
    if (!buf) return NULL;
    wit_nlu_result_t *res = NULL;
    char *ptr = (char*)buf;
    
    // Wit.ai sends multiple JSON objects back-to-back.
    // We iterate through them and keep the most "final" or latest one.
    while (ptr) {
        char *next_brace = strstr(ptr, "{");
        if (!next_brace) break;
        
        cJSON *root = cJSON_Parse(next_brace);
        if (root) {
            cJSON *type = cJSON_GetObjectItem(root, "type");
            cJSON *text = cJSON_GetObjectItem(root, "text");
            bool is_final_msg = (type && strstr(type->valuestring, "FINAL"));
            
            // We prioritize FINAL_UNDERSTANDING or FINAL_TRANSCRIPTION.
            // If we already have a final result, we only replace it if this one is also final or better.
            if (!res || is_final_msg || !res->is_final) {
                if (!res) res = (wit_nlu_result_t*)calloc(1, sizeof(wit_nlu_result_t));
                
                if (cJSON_IsString(text)) {
                    if (res->text) free(res->text);
                    res->text = strdup(text->valuestring);
                }
                
                if (type && strstr(type->valuestring, "FINAL")) res->is_final = true;

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
        ptr = next_brace + 1;
    }
    return res;
}

wit_nlu_result_t* wit_stt_query(uint8_t *pcm_audio, size_t len) {
    if (strlen(g_token) == 0 || !pcm_audio) {
        ESP_LOGE(TAG, "Wit token not set or no audio");
        return NULL;
    }
    ESP_LOGI(TAG, "Querying Wit.ai STT (%zu bytes)", len);
    
    response_collector_t collector = {0};
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
    
    esp_http_client_set_post_field(client, (const char *)pcm_audio, len);
    
    ESP_LOGI(TAG, "Sending STT request...");
    esp_err_t err = esp_http_client_perform(client);
    
    wit_nlu_result_t *res = NULL;
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        if (status == 200 && collector.buf) {
            ESP_LOGI(TAG, "Response JSON: %s", collector.buf);
            res = _parse_wit_response(collector.buf);
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

static void _stt_stream_task(void *pv) {
    ESP_LOGI(TAG, "STT Background Task Started");
    
    esp_http_client_config_t config = {
        .url = "https://api.wit.ai/speech?v=20260131",
        .method = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
        .buffer_size = 10240, // Increased buffer for stability
        .event_handler = _http_event_handler,
        .user_data = &g_stream_collector,
    };
    
    g_stream_client = esp_http_client_init(&config);
    esp_http_client_set_header(g_stream_client, "Content-Type", "audio/raw;encoding=signed-integer;bits=16;rate=16000;endian=little");
    esp_http_client_set_header(g_stream_client, "Transfer-Encoding", "chunked");
    
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", g_token);
    esp_http_client_set_header(g_stream_client, "Authorization", auth_header);

    esp_err_t err = esp_http_client_open(g_stream_client, -1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open STT stream: %s", esp_err_to_name(err));
        g_stream_ready = false;
        g_stream_active = false;
        esp_http_client_cleanup(g_stream_client);
        g_stream_client = NULL;
        vTaskDelete(NULL);
        return;
    }

    g_stream_ready = true;
    bool headers_fetched = false;
    uint32_t bytes_sent = 0;
    
    while (g_stream_active) {
        size_t item_size;
        // Wait up to 50ms for audio data
        uint8_t *item = xRingbufferReceiveUpTo(g_stream_rb, &item_size, pdMS_TO_TICKS(50), 2048);
        
        if (item) {
            int written = esp_http_client_write(g_stream_client, (const char*)item, (int)item_size);
            vRingbufferReturnItem(g_stream_rb, item);
            
            if (written < 0) {
                ESP_LOGW(TAG, "Stream write failed (possibly closed by server), headers_fetched=%d", headers_fetched);
                break;
            }
            bytes_sent += written;
            
            // Try to get response headers after sending some initial data
            if (!headers_fetched && bytes_sent > 4096) {
                if (esp_http_client_fetch_headers(g_stream_client) >= 0) {
                    headers_fetched = true;
                    ESP_LOGI(TAG, "STT Stream Connected & Headers Received");
                    // Use small timeout for subsequent duplex reads
                    esp_http_client_set_timeout_ms(g_stream_client, 100);
                }
            }
        }
        
        // After headers are fetched, poll for partial transcriptions from Wit.ai
        if (headers_fetched) {
            char read_buf[512];
            int n = esp_http_client_read(g_stream_client, read_buf, sizeof(read_buf));
            if (n < 0 && n != -ESP_ERR_HTTP_EAGAIN) {
                ESP_LOGW(TAG, "Stream read error: %d, ending duplex", n);
                break;
            }
            
            // Check if the collector now contains a "FINAL" message to stop writing early
            if (g_stream_collector.buf && strstr(g_stream_collector.buf, "FINAL_UNDERSTANDING")) {
                ESP_LOGI(TAG, "Final understanding received, closing write stream");
                break;
            }
        }

        // Slow down the loop slightly if no data to prevent CPU starvation
        if (!item) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    
    // Close chunked stream properly
    if (g_stream_client) {
        esp_http_client_write(g_stream_client, NULL, 0); 
        if (!headers_fetched) esp_http_client_fetch_headers(g_stream_client);
    }
    
    // Final read flush to ensure we get the last understanding object
    esp_http_client_set_timeout_ms(g_stream_client, 2000);
    char final_buf[512];
    while(esp_http_client_read(g_stream_client, final_buf, sizeof(final_buf)) > 0);

    ESP_LOGI(TAG, "STT Stream Task Finishing");
    g_stream_ready = false;
    g_stream_active = false;
    vTaskDelete(NULL);
    g_stream_task_handle = NULL;
}

esp_err_t wit_stt_stream_start(wit_stt_callback_t cb) {
    if (g_stream_active) return ESP_ERR_INVALID_STATE;
    
    g_stt_cb = cb;
    g_stream_active = true;
    g_stream_ready = false;
    
    if (g_stream_collector.buf) {
        free(g_stream_collector.buf);
        g_stream_collector.buf = NULL;
    }
    g_stream_collector.size = 0;

    if (!g_stream_rb) {
        g_stream_rb = xRingbufferCreate(STT_RING_BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);
    } else {
        // Clear any leftover data
        size_t size;
        void *item;
        while ((item = xRingbufferReceive(g_stream_rb, &size, 0))) {
            vRingbufferReturnItem(g_stream_rb, item);
        }
    }

    xTaskCreatePinnedToCore(_stt_stream_task, "wit_stt_task", 12288, NULL, 5, &g_stream_task_handle, 1);
    
    // Wait a short bit for task to start and headers to be sent? 
    // Or just let feed handle it.
    return ESP_OK;
}

esp_err_t wit_stt_stream_feed(uint8_t *pcm_audio, size_t len) {
    if (!g_stream_active || !g_stream_rb) return ESP_ERR_INVALID_STATE;
    
    BaseType_t ret = xRingbufferSend(g_stream_rb, pcm_audio, len, 0);
    if (ret != pdTRUE) {
        // Log sparingly
        static uint32_t last_log = 0;
        if (esp_log_timestamp() - last_log > 2000) {
            ESP_LOGW(TAG, "STT Ringbuffer full, dropping audio chunk");
            last_log = esp_log_timestamp();
        }
        return ESP_FAIL;
    }
    return ESP_OK;
}

wit_nlu_result_t* wit_stt_stream_stop(void) {
    if (!g_stream_active) return NULL;
    
    // Signal task to stop reading and finish the request
    g_stream_active = false;
    
    // Wait for task to finish (max 5 seconds)
    int timeout = 50;
    while (g_stream_task_handle && timeout-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    wit_nlu_result_t *res = NULL;
    if (g_stream_collector.buf) {
        res = _parse_wit_response(g_stream_collector.buf);
        free(g_stream_collector.buf);
        g_stream_collector.buf = NULL;
    }
    g_stream_collector.size = 0;

    if (g_stream_client) {
        esp_http_client_cleanup(g_stream_client);
        g_stream_client = NULL;
    }
    
    g_stt_cb = NULL;
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

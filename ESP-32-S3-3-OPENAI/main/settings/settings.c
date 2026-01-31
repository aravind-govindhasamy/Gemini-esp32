/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_check.h"
#include "bsp/esp-bsp.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "settings.h"
#include "esp_ota_ops.h"

esp_err_t settings_write_parameter_to_nvs(void);
static const char *TAG = "settings";
const char *uf2_nvs_partition = "nvs";
const char *uf2_nvs_namespace = "configuration";
static nvs_handle_t my_handle;
static sys_param_t g_sys_param = {0};

esp_err_t settings_factory_reset(void)
{
    const esp_partition_t *update_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    ESP_LOGI(TAG, "Switch to partition UF2");
    esp_ota_set_boot_partition(update_partition);
    esp_restart();
    return ESP_OK;
}

esp_err_t settings_read_parameter_from_nvs(void)
{
    // Initialize with empty strings
    memset(&g_sys_param, 0, sizeof(sys_param_t));

    /* --- LOCAL CREDENTIALS OVERRIDE --- */
    /* You can hardcode your credentials here if NVS is not working */
    strncpy(g_sys_param.ssid, "", SSID_SIZE);
    strncpy(g_sys_param.password, "", PASSWORD_SIZE);
    strncpy(g_sys_param.gemini_key, "", KEY_SIZE);
    strncpy(g_sys_param.wit_token, "", KEY_SIZE);
    strncpy(g_sys_param.user_name, "Friend", 32);
    g_sys_param.user_age = 7;
    /* ---------------------------------- */

    esp_err_t ret = nvs_open_from_partition(uf2_nvs_partition, uf2_nvs_namespace, NVS_READONLY, &my_handle);
    if (ESP_ERR_NVS_NOT_FOUND == ret) {
        ESP_LOGW(TAG, "NVS Partition not found, using local/empty parameters");
        return ESP_OK; 
    }

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "nvs open failed (0x%x)", ret);
        return ESP_OK;
    }

    size_t len = 0;

    // Read SSID
    len = sizeof(g_sys_param.ssid);
    if (nvs_get_str(my_handle, "ssid", g_sys_param.ssid, &len) != ESP_OK) {
        ESP_LOGW(TAG, "No SSID in NVS");
    }

    // Read password
    len = sizeof(g_sys_param.password);
    if (nvs_get_str(my_handle, "password", g_sys_param.password, &len) != ESP_OK) {
        ESP_LOGW(TAG, "No Password in NVS");
    }

    // Read Gemini key
    len = sizeof(g_sys_param.gemini_key);
    if (nvs_get_str(my_handle, "Gemini_key", g_sys_param.gemini_key, &len) != ESP_OK) {
        ESP_LOGW(TAG, "No Gemini key in NVS");
    }

    // Read Wit token
    len = sizeof(g_sys_param.wit_token);
    if (nvs_get_str(my_handle, "wit_token", g_sys_param.wit_token, &len) != ESP_OK) {
        ESP_LOGW(TAG, "No Wit token in NVS");
    }

    // Read User Name
    len = sizeof(g_sys_param.user_name);
    if (nvs_get_str(my_handle, "user_name", g_sys_param.user_name, &len) != ESP_OK) {
        ESP_LOGW(TAG, "No User Name in NVS");
    }

    // Read User Age
    if (nvs_get_i32(my_handle, "user_age", &g_sys_param.user_age) != ESP_OK) {
        ESP_LOGW(TAG, "No User Age in NVS");
    }

    nvs_close(my_handle);

    ESP_LOGI(TAG, "stored ssid:%s", g_sys_param.ssid);
    return ESP_OK;
}

sys_param_t *settings_get_parameter(void)
{
    return &g_sys_param;
}

esp_err_t settings_write_parameter_to_nvs(void)
{
    esp_err_t ret = nvs_open_from_partition(uf2_nvs_partition, uf2_nvs_namespace, NVS_READWRITE, &my_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "nvs open failed (0x%x)", ret);
        return ESP_FAIL;
    }

    nvs_set_str(my_handle, "user_name", g_sys_param.user_name);
    nvs_set_str(my_handle, "wit_token", g_sys_param.wit_token);
    nvs_set_i32(my_handle, "user_age", g_sys_param.user_age);
    
    ret = nvs_commit(my_handle);
    nvs_close(my_handle);
    return ret;
}

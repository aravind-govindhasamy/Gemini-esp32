/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include "esp_err.h"

#define SSID_SIZE 32
#define PASSWORD_SIZE 64
#define KEY_SIZE 165

typedef struct {
    char ssid[SSID_SIZE];             /* SSID of target AP. */
    char password[PASSWORD_SIZE];     /* Password of target AP. */
    char gemini_key[KEY_SIZE];        /* Gemini key. */
    char wit_token[KEY_SIZE];         /* Wit.ai token. */
    char user_name[32];               /* User's name */
    int32_t user_age;                 /* User's age */
} sys_param_t;

esp_err_t settings_factory_reset(void);
esp_err_t settings_read_parameter_from_nvs(void);
esp_err_t settings_write_parameter_to_nvs(void);
sys_param_t *settings_get_parameter(void);

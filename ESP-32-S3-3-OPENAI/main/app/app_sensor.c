#include "app_sensor.h"
#include "bsp/esp-bsp.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "app_sensor";

static uint8_t sensor_addr = 0; // 0 means not found
static bool is_aht20 = false;
static bool sensor_initialized = false;

esp_err_t app_sensor_init(void)
{
    // I2C should already be initialized by bsp_board_init()
    esp_err_t ret = ESP_OK;
    
    ESP_LOGI(TAG, "Scanning I2C Bus...");
    for (int i = 0x01; i < 0x7F; i++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (i << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        if (i2c_master_cmd_begin(BSP_I2C_NUM, cmd, pdMS_TO_TICKS(10)) == ESP_OK) {
            ESP_LOGI(TAG, "Found device at 0x%02X", i);
            if (i == 0x38) {
                sensor_addr = 0x38;
                is_aht20 = true;
                ESP_LOGI(TAG, "Detected AHT20");
            } else if (i == 0x44) {
                sensor_addr = 0x44;
                is_aht20 = false; // SHT3x
                ESP_LOGI(TAG, "Detected SHT3x");
            }
        }
        i2c_cmd_link_delete(cmd);
    }

    if (sensor_addr == 0) {
        ESP_LOGE(TAG, "No compatible sensor found!");
        return ESP_ERR_NOT_FOUND;
    }

    if (is_aht20) {
        // Init AHT20
        vTaskDelay(pdMS_TO_TICKS(40));
        uint8_t read_data[1];
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (sensor_addr << 1) | I2C_MASTER_READ, true);
        i2c_master_read(cmd, read_data, 1, I2C_MASTER_LAST_NACK);
        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(BSP_I2C_NUM, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);

        // Check if calibrated (bit 3)
        if (ret == ESP_OK && (read_data[0] & 0x08) == 0) {
             ESP_LOGI(TAG, "Calibrating AHT20...");
             cmd = i2c_cmd_link_create();
             i2c_master_start(cmd);
             i2c_master_write_byte(cmd, (sensor_addr << 1) | I2C_MASTER_WRITE, true);
             i2c_master_write_byte(cmd, 0xBE, true);
             i2c_master_write_byte(cmd, 0x08, true);
             i2c_master_write_byte(cmd, 0x00, true);
             i2c_master_stop(cmd);
             i2c_master_cmd_begin(BSP_I2C_NUM, cmd, pdMS_TO_TICKS(100));
             i2c_cmd_link_delete(cmd);
             vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    sensor_initialized = true;
    return ESP_OK;
}

esp_err_t app_sensor_get_values(float *temp, float *hum)
{
    if (!sensor_initialized) {
        if (app_sensor_init() != ESP_OK) return ESP_FAIL;
    }

    esp_err_t ret = ESP_FAIL;

    if (is_aht20) {
        // AHT20 Read
        uint8_t cmd_data[3] = {0xAC, 0x33, 0x00};
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (sensor_addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write(cmd, cmd_data, 3, true);
        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(BSP_I2C_NUM, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);
        
        if (ret != ESP_OK) return ret;
        vTaskDelay(pdMS_TO_TICKS(80));

        uint8_t data[6];
        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (sensor_addr << 1) | I2C_MASTER_READ, true);
        i2c_master_read(cmd, data, 6, I2C_MASTER_LAST_NACK);
        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(BSP_I2C_NUM, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK) {
            uint32_t raw_hum = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
            uint32_t raw_temp = ((uint32_t)(data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];
            if (hum) *hum = (float)raw_hum * 100.0f / 1048576.0f;
            if (temp) *temp = ((float)raw_temp * 200.0f / 1048576.0f) - 50.0f;
        }

    } else {
        // SHT3x Read
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (sensor_addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, 0x2C, true); // High repeatability
        i2c_master_write_byte(cmd, 0x06, true);
        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(BSP_I2C_NUM, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);

        if (ret != ESP_OK) return ret;
        vTaskDelay(pdMS_TO_TICKS(20));

        uint8_t data[6];
        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (sensor_addr << 1) | I2C_MASTER_READ, true);
        i2c_master_read(cmd, data, 6, I2C_MASTER_LAST_NACK);
        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(BSP_I2C_NUM, cmd, pdMS_TO_TICKS(100));
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK) {
            // SHT3x: Temp = -45 + 175 * raw / 65535, Hum = 100 * raw / 65535
            uint16_t raw_temp = (data[0] << 8) | data[1];
            uint16_t raw_hum = (data[3] << 8) | data[4];
            if (temp) *temp = -45.0f + 175.0f * (float)raw_temp / 65535.0f;
            if (hum) *hum = 100.0f * (float)raw_hum / 65535.0f;
        }
    }
    
    return ret;
}

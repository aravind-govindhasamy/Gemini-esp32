#include "app_sensor.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "app_sensor";
static SemaphoreHandle_t i2c_mux = NULL;

// Dock I2C (Now using I2C_NUM_0 to avoid conflict with BSP on I2C_NUM_1)
#define DOCK_I2C_SDA         41
#define DOCK_I2C_SCL         40
#define DOCK_I2C_NUM         I2C_NUM_0

// Presence Sensor
#define PRESENCE_GPIO        21

static uint8_t aht20_addr = 0x38;
static bool aht20_found = false;

esp_err_t app_sensor_init(void)
{
    if (i2c_mux == NULL) {
        i2c_mux = xSemaphoreCreateMutex();
    }

    // 1. Presence GPIO init
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PRESENCE_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // 2. Dock I2C (AHT20)
    // Using I2C_NUM_0 which is FREE on ESP-BOX-3 (BSP uses I2C_NUM_1)
    i2c_config_t conf0 = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = DOCK_I2C_SDA,
        .scl_io_num = DOCK_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    
    xSemaphoreTake(i2c_mux, portMAX_DELAY);
    esp_err_t err = i2c_param_config(DOCK_I2C_NUM, &conf0);
    if (err == ESP_OK) {
        i2c_driver_install(DOCK_I2C_NUM, conf0.mode, 0, 0, 0);
        ESP_LOGI(TAG, "I2C_NUM_0 initialized for sensors (41/40)");
    }

    // Initial probe AHT20
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (aht20_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    if (i2c_master_cmd_begin(DOCK_I2C_NUM, cmd, pdMS_TO_TICKS(50)) == ESP_OK) {
        aht20_found = true;
        ESP_LOGI(TAG, "AHT20 found at 0x38 on I2C_NUM_0");
    } else {
        ESP_LOGW(TAG, "AHT20 NOT found on I2C_NUM_0");
    }
    i2c_cmd_link_delete(cmd);
    xSemaphoreGive(i2c_mux);

    return ESP_OK;
}

esp_err_t app_sensor_get_values(float *temp, float *hum)
{
    if (!aht20_found) return ESP_ERR_NOT_FOUND;
    if (xSemaphoreTake(i2c_mux, pdMS_TO_TICKS(500)) != pdTRUE) return ESP_ERR_TIMEOUT;

    // Trigger
    uint8_t measure_cmd[] = {0xAC, 0x33, 0x00};
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (aht20_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, measure_cmd, 3, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(DOCK_I2C_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    vTaskDelay(pdMS_TO_TICKS(80));

    uint8_t data[6];
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (aht20_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(DOCK_I2C_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) {
        uint32_t raw_hum = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
        uint32_t raw_temp = ((uint32_t)(data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];
        if (hum) *hum = (float)raw_hum * 100.0f / 1048576.0f;
        if (temp) *temp = ((float)raw_temp * 200.0f / 1048576.0f) - 50.0f;
    }

    xSemaphoreGive(i2c_mux);
    return ret;
}

bool app_sensor_get_presence(void)
{
    return gpio_get_level(PRESENCE_GPIO) == 0;
}

float app_sensor_get_lux(void)
{
    return 0.0f;
}

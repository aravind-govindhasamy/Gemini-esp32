#include "app_sensor.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "app_sensor";
static SemaphoreHandle_t i2c_mux = NULL;

// Internal I2C Bus (BOX-3)
#define DOCK_I2C_SDA 8
#define DOCK_I2C_SCL 18
#define DOCK_I2C_NUM I2C_NUM_0

// Presence Sensor
#define PRESENCE_GPIO 21

static uint8_t sht4x_addr = 0x44;
static uint8_t bh1750_addr = 0x23;
static bool sht4x_found = false;
static bool bh1750_found = false;

esp_err_t app_sensor_init(void) {
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

  // 2. I2C Bus check
  // bsp_i2c_init() is already called in main.c, so the driver is installed.
  // We just need to ensure the pins 8/18 are used.
  ESP_LOGI(TAG, "Sensors using I2C_NUM_0 (Pins 8/18)");

  // Initial probe SHT4x
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (sht4x_addr << 1) | I2C_MASTER_WRITE, true);
  i2c_master_stop(cmd);
  if (i2c_master_cmd_begin(DOCK_I2C_NUM, cmd, pdMS_TO_TICKS(50)) == ESP_OK) {
    sht4x_found = true;
    ESP_LOGI(TAG, "SHT4x found at 0x44 on I2C_NUM_0");
  } else {
    ESP_LOGW(TAG, "SHT4x NOT found on I2C_NUM_0");
  }
  i2c_cmd_link_delete(cmd);

  // Initial probe BH1750
  cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (bh1750_addr << 1) | I2C_MASTER_WRITE, true);
  i2c_master_stop(cmd);
  if (i2c_master_cmd_begin(DOCK_I2C_NUM, cmd, pdMS_TO_TICKS(50)) == ESP_OK) {
    bh1750_found = true;
    ESP_LOGI(TAG, "BH1750 found at 0x23 on I2C_NUM_0");
  }
  i2c_cmd_link_delete(cmd);
  xSemaphoreGive(i2c_mux);

  return ESP_OK;
}

esp_err_t app_sensor_get_values(float *temp, float *hum) {
  if (!sht4x_found)
    return ESP_ERR_NOT_FOUND;
  if (xSemaphoreTake(i2c_mux, pdMS_TO_TICKS(500)) != pdTRUE)
    return ESP_ERR_TIMEOUT;

  // SHT4x measurement
  uint8_t measure_cmd = 0xFD; // High precision
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (sht4x_addr << 1) | I2C_MASTER_WRITE, true);
  i2c_master_write_byte(cmd, measure_cmd, true);
  i2c_master_stop(cmd);
  esp_err_t ret = i2c_master_cmd_begin(DOCK_I2C_NUM, cmd, pdMS_TO_TICKS(100));
  i2c_cmd_link_delete(cmd);

  vTaskDelay(pdMS_TO_TICKS(20));

  uint8_t data[6];
  cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (sht4x_addr << 1) | I2C_MASTER_READ, true);
  i2c_master_read(cmd, data, 6, I2C_MASTER_LAST_NACK);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(DOCK_I2C_NUM, cmd, pdMS_TO_TICKS(100));
  i2c_cmd_link_delete(cmd);

  if (ret == ESP_OK) {
    uint16_t t_ticks = (uint16_t)data[0] << 8 | data[1];
    uint16_t rh_ticks = (uint16_t)data[3] << 8 | data[4];
    if (temp)
      *temp = -45.0f + 175.0f * (float)t_ticks / 65535.0f;
    if (hum)
      *hum = -6.0f + 125.0f * (float)rh_ticks / 65535.0f;
  }

  xSemaphoreGive(i2c_mux);
  return ret;
}

bool app_sensor_get_presence(void) {
  return gpio_get_level(PRESENCE_GPIO) == 0;
}

float app_sensor_get_lux(void) {
  if (!bh1750_found)
    return 0.0f;
  if (xSemaphoreTake(i2c_mux, pdMS_TO_TICKS(500)) != pdTRUE)
    return 0.0f;

  uint8_t data[2];
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (bh1750_addr << 1) | I2C_MASTER_READ, true);
  i2c_master_read(cmd, data, 2, I2C_MASTER_LAST_NACK);
  i2c_master_stop(cmd);
  esp_err_t ret = i2c_master_cmd_begin(DOCK_I2C_NUM, cmd, pdMS_TO_TICKS(100));
  i2c_cmd_link_delete(cmd);

  float lux = 0.0f;
  if (ret == ESP_OK) {
    lux = (float)((data[0] << 8) | data[1]) / 1.2f;
  }
  xSemaphoreGive(i2c_mux);
  return lux;
}

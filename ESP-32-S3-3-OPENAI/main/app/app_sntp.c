#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>


#include "app_sntp.h"
#include "esp_log.h"
#include "esp_sntp.h"

static const char *TAG = "app_sntp";

static void sntp_sync_time_cb(struct timeval *tv) {
  ESP_LOGI(TAG, "Time Synchronized!");
  setenv("TZ", "IST-5:30", 1);
  tzset();
}

void app_sntp_init(void) {
  ESP_LOGI(TAG, "Initializing SNTP");

  // Initialize SNTP
  esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, "pool.ntp.org");
  esp_sntp_setservername(1, "time.google.com");
  esp_sntp_set_time_sync_notification_cb(sntp_sync_time_cb);
  esp_sntp_init();

  // Set Timezone to Indian Standard Time (IST) immediately
  setenv("TZ", "IST-5:30", 1);
  tzset();
}

static bool mode_24hr = false;

void app_sntp_set_24hr_mode(bool is_24hr) { mode_24hr = is_24hr; }

bool app_sntp_get_24hr_mode(void) { return mode_24hr; }

bool app_sntp_get_time_str(char *buf, size_t max_len) {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);

  // If year is < 2024, likely not synced (epoch start)
  if (timeinfo.tm_year < (2024 - 1900)) {
    return false;
  }

  if (mode_24hr) {
    // Format: "22:30"
    strftime(buf, max_len, "%H:%M", &timeinfo);
  } else {
    // Format: "10:30 PM"
    strftime(buf, max_len, "%I:%M %p", &timeinfo);
  }
  return true;
}

bool app_sntp_get_date_str(char *buf, size_t max_len) {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);

  if (timeinfo.tm_year < (2024 - 1900)) {
    return false;
  }

  // Format: "Mon, Jan 22"
  strftime(buf, max_len, "%a, %b %d", &timeinfo);
  return true;
}

#include <WiFi.h>
#include <time.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// S3-Box Hardware Pins
#define LCD_BL 45

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9342 _panel_instance;
  lgfx::Bus_SPI       _bus_instance;
public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read  = 16000000;
      cfg.pin_sclk = 7;
      cfg.pin_mosi = 6;
      cfg.pin_miso = -1;
      cfg.pin_dc   = 4;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs           = 5;
      cfg.pin_rst          = 48;
      cfg.panel_width      = 320;
      cfg.panel_height     = 240;
      cfg.invert           = true;
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

LGFX tft;
LGFX_Sprite canvas(&tft);

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 19800; // IST (UTC+5:30)
const int   daylightOffset_sec = 0;

void setup() {
  Serial.begin(115200);
  
  // Power on backlight
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  tft.init();
  tft.setRotation(1);
  canvas.createSprite(320, 240);

  Serial.println("Enter WiFi SSID:");
  while(!Serial.available());
  String ssid = Serial.readStringUntil('\n');
  ssid.trim();

  Serial.println("Enter WiFi Password:");
  while(!Serial.available());
  String password = Serial.readStringUntil('\n');
  password.trim();

  WiFi.begin(ssid.c_str(), password.c_str());
  
  canvas.fillScreen(TFT_BLACK);
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextSize(2);
  canvas.setCursor(20, 100);
  canvas.print("Connecting to WiFi...");
  canvas.pushSprite(0, 0);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nConnected!");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void loop() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }

  // Draw Background Gradient
  for(int i=0; i<240; i++) {
    uint16_t color = canvas.color565(0, 0, i/4 + 20); // Deep Blue Gradient
    canvas.drawFastHLine(0, i, 320, color);
  }

  // Draw Clock
  char timeHour[3], timeMin[3], timeSec[3], dateStr[32];
  strftime(timeHour, 3, "%H", &timeinfo);
  strftime(timeMin, 3, "%M", &timeinfo);
  strftime(timeSec, 3, "%S", &timeinfo);
  strftime(dateStr, 32, "%A, %B %d %Y", &timeinfo);

  canvas.setTextDatum(middle_center);
  
  // Hours:Minutes
  canvas.setTextColor(TFT_CYAN);
  canvas.setTextSize(6);
  String timeStr = String(timeHour) + ":" + String(timeMin);
  canvas.drawString(timeStr, 160, 100);

  // Seconds
  canvas.setTextColor(TFT_YELLOW);
  canvas.setTextSize(3);
  canvas.drawString(String(timeSec), 160, 150);

  // Date
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextSize(2);
  canvas.drawString(dateStr, 160, 200);

  // Status Info
  canvas.setTextDatum(top_left);
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_GREEN);
  canvas.drawString("WiFi: " + WiFi.SSID(), 10, 10);
  canvas.drawString("IP: " + WiFi.localIP().toString(), 10, 25);

  canvas.pushSprite(0, 0);
  delay(1000);
}

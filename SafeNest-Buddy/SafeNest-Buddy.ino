#include <Wire.h>
#include <vector>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// S3-Box Internal Hardware Pins (Display)
#define LCD_MOSI      6
#define LCD_MISO      -1
#define LCD_SCLK      7
#define LCD_DC        4
#define LCD_CS        5
#define LCD_RST       48
#define LCD_BL        45

// ESP32-S3 Box Display Configuration (LovyanGFX)
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
      cfg.pin_sclk = LCD_SCLK;
      cfg.pin_mosi = LCD_MOSI;
      cfg.pin_miso = LCD_MISO;
      cfg.pin_dc   = LCD_DC;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs           = LCD_CS;
      cfg.pin_rst          = LCD_RST;
      cfg.panel_width      = 320;
      cfg.panel_height     = 240;
      cfg.invert           = true;
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

LGFX tft;
bool displayInitialized = false;

const char* apiKey = "AIzaSyCwvHSY3MsDosv1mpBf64BXKXY20iMtfdA";
String endpoint = "https://generativelanguage.googleapis.com/v1beta/models/gemini-pro:generateContent?key=" + String(apiKey);
String userName;
int userAge = 7;
String systemPrompt = "You are SafeNest Buddy, a friendly AI companion for a child.";

void displayPrint(String text, bool newLine = true) {
  Serial.print(text);
  if (newLine) Serial.println("");
  if (displayInitialized) {
    tft.setTextColor(TFT_GREEN);
    tft.setTextSize(1);
    tft.print(text);
    if (newLine) tft.print("\n");
  }
}

void drawFace(String state) {
  if (!displayInitialized) return;
  tft.fillScreen(TFT_BLACK);
  int cx = 160; int cy = 100;
  if (state == "Neutral") {
    tft.fillCircle(cx - 50, cy, 20, TFT_WHITE);
    tft.fillCircle(cx + 50, cy, 20, TFT_WHITE);
    tft.fillRect(cx - 30, cy + 60, 60, 5, TFT_WHITE);
  } else if (state == "Thinking") {
    tft.fillCircle(cx - 50, cy, 20, TFT_YELLOW);
    tft.fillCircle(cx + 50, cy, 20, TFT_YELLOW);
    tft.fillRect(cx - 20, cy + 50, 40, 5, TFT_WHITE);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- S3-Box Booting ---");

  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  displayInitialized = true;

  displayPrint("Welcome to SafeNest Buddy!");
  drawFace("Neutral");
  
  displayPrint("WiFi Scan...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
}

void loop() {
  if (Serial.available() > 0) {
    String userQuery = Serial.readStringUntil('\n');
    userQuery.trim();
    if (userQuery.length() > 0) {
       displayPrint("User: " + userQuery);
       drawFace("Thinking");
       delay(1000);
       drawFace("Neutral");
       displayPrint("Gemini: [Audio Disabled]");
    }
  }
}

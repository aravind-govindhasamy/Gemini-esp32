#include <Wire.h>
#include <vector>
#include <mutex>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "Audio.h" // Use the library header
// S3-Box Internal Hardware Pins
#define I2S_DOUT      15
#define I2S_BCLK      17
#define I2S_LRCK      47
#define I2S_MCLK      2
#define I2S_DIN       16

#define ES8311_SDA    8
#define ES8311_SCL    18
#define ES8311_ADDR   0x18
#define ES7243_ADDR   0x13 // ADC for Microphone

Audio audio;

// Generic I2C write for Audio Codecs
void writeReg(uint8_t addr, uint8_t reg, uint8_t data) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(data);
    Wire.endTransmission();
}

void initES8311() {
    Wire.begin(ES8311_SDA, ES8311_SCL, 100000);
    // Reset
    writeReg(ES8311_ADDR, 0x00, 0x80); delay(10); writeReg(ES8311_ADDR, 0x00, 0x00);
    // Power
    writeReg(ES8311_ADDR, 0x0D, 0x01); writeReg(ES8311_ADDR, 0x0E, 0x02);
    // Volume & DAC
    writeReg(ES8311_ADDR, 0x32, 0xBF); writeReg(ES8311_ADDR, 0x14, 0x1A); writeReg(ES8311_ADDR, 0x12, 0x00); 
}

void initES7243() {
    // Basic Mic Init (Enable ADC)
    writeReg(ES7243_ADDR, 0x00, 0x01); 
    writeReg(ES7243_ADDR, 0x06, 0x00); 
    writeReg(ES7243_ADDR, 0x05, 0x1B); 
}

void setupAudio() {
    initES8311();
    initES7243();
    audio.setPinout(I2S_BCLK, I2S_LRCK, I2S_DOUT, I2S_MCLK);
    audio.setVolume(15);
}

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
      cfg.pin_busy         = -1;
      cfg.panel_width      = 320;
      cfg.panel_height     = 240;
      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      cfg.offset_rotation  = 0;
      cfg.dummy_read_pixel = 8;
      cfg.invert           = true; // S3-Box usually needs inversion
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = false;
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

LGFX tft;

const char* apiKey = "";
String endpoint = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=" + String(apiKey);
String userName;
int userAge = 7;
String systemPrompt = "You are an embedded AI voice companion for an ESP32-S3 Box. "
"Your goal is to interact with young users in a safe, calm, age-appropriate, and educational way. "
"Always assume the user is a child. Language: simple words, short sentences, warm tone. "
"Never sound like a teacher. Never ask personal questions or give dangerous advice. "
"AGE adaptation: 4-6 (very simple, playful), 7-9 (curious explanations), 10-12 (deeper explanations), 13-14 (factual but safe). "
"Never mention the age explicitly. Response: spoken-friendly, no long paragraphs, natural pauses.";
bool displayInitialized = false;

void drawFace(String state) {
  if (!displayInitialized) return;
  tft.fillScreen(TFT_BLACK);
  int cx = 160;
  int cy = 100;
  
  if (state == "Listening") {
    // Large curious eyes
    tft.fillCircle(cx - 50, cy, 30, TFT_CYAN);
    tft.fillCircle(cx + 50, cy, 30, TFT_CYAN);
    tft.fillCircle(cx - 50, cy, 10, TFT_BLACK);
    tft.fillCircle(cx + 50, cy, 10, TFT_BLACK);
    tft.drawArc(cx, cy + 60, 40, 42, 0, 180, TFT_WHITE);
  } else if (state == "Thinking") {
    // Swirling/Thinking eyes
    tft.fillCircle(cx - 50, cy, 20, TFT_YELLOW);
    tft.fillCircle(cx + 50, cy, 20, TFT_YELLOW);
    tft.drawCircle(cx - 50, cy, 25, TFT_WHITE);
    tft.drawCircle(cx + 50, cy, 25, TFT_WHITE);
    tft.fillRect(cx - 20, cy + 50, 40, 5, TFT_WHITE);
  } else if (state == "Speaking") {
    // Happy/Talking face
    tft.fillCircle(cx - 50, cy, 25, TFT_MAGENTA);
    tft.fillCircle(cx + 50, cy, 25, TFT_MAGENTA);
    tft.fillEllipse(cx, cy + 60, 50, 20, TFT_WHITE);
  } else {
    // Neutral
    tft.fillCircle(cx - 50, cy, 20, TFT_WHITE);
    tft.fillCircle(cx + 50, cy, 20, TFT_WHITE);
    tft.fillRect(cx - 30, cy + 60, 60, 5, TFT_WHITE);
  }
}

void displayPrint(String text, bool newLine = true) {
  Serial.print(text);
  if (newLine) Serial.println("");
  
  if (displayInitialized) {
    // Print smaller at the bottom
    tft.setTextColor(TFT_GREEN);
    tft.setTextSize(1);
    tft.print(text);
    if (newLine) tft.print("\n");
  }
}


void connectToWiFi(const char* ssid, const char* password) {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(1000);

  WiFi.begin(ssid, password);

  int retries = 0;
  const int maxRetries = 30;
  displayPrint("Connecting to WiFi: " + String(ssid), false);
  while (WiFi.status() != WL_CONNECTED && retries < maxRetries) {
    delay(1000);
    displayPrint(".", false);
    retries++;
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    displayPrint("\nWiFi connected!");
    displayPrint("IP: " + WiFi.localIP().toString());
  } else {
    displayPrint("\nError: Could not connect.");
  }
}

void loadAPIKey() {
  displayPrint("API Key: [Hardcoded]");
}

void saveAP(const char* ssid, const char* password) {
  Serial.println("Saving Access Point information...");
  String apData = String(ssid) + "//" + String(password) + "\n";
  Serial.print("Writing to SavedAPs.txt: ");
  Serial.println(apData);
}

bool loadSavedAP(String &ssid, String &password) {
  Serial.println("Loading saved Access Points from SavedAPs.txt...");
  
  while (!Serial.available());
  String apData = Serial.readStringUntil('\n');
  
  int separatorIndex = apData.indexOf("//");
  if (separatorIndex == -1) return false;

  ssid = apData.substring(0, separatorIndex);
  password = apData.substring(separatorIndex + 2);
  
  ssid.trim();
  password.trim();
  
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- S3-Box Booting ---");

  // Removed LED pins (2 and 5) because they conflict with Audio MCLK and Display CS

  Serial.println("Init Backlight...");
  pinMode(45, OUTPUT);
  digitalWrite(45, HIGH);

  Serial.println("Init Display...");
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  displayInitialized = true;

  setupAudio(); // Initialize ES8311 and I2S

  loadAPIKey();

  displayPrint("Welcome to Gemini Bot!");
  drawFace("Neutral");
  displayPrint("Please enter your name:");
  
  while (!Serial.available());
  userName = Serial.readStringUntil('\n');
  userName.trim();

  displayPrint("Hello, " + userName + "!");

  displayPrint("Please enter your age:");
  while (!Serial.available());
  String ageInput = Serial.readStringUntil('\n');
  ageInput.trim();
  userAge = ageInput.toInt();
  if (userAge <= 0) userAge = 7; // Fallback
  displayPrint("Age set to: " + String(userAge));

  displayPrint("Scanning WiFi...");
  int n = WiFi.scanNetworks();
  displayPrint("Available networks:");
  for (int i = 0; i < n; i++) {
    Serial.println(String(i + 1) + ": " + WiFi.SSID(i));
  }

  while (true) {
    Serial.println("Do you want to use a saved Access Point? (yes/no):");
    while (!Serial.available());
    String choice = Serial.readStringUntil('\n');
    choice.trim();

    if (choice.equalsIgnoreCase("yes")) {
      Serial.println("Please enter the SSID and password in the format 'SSID//password':");
      
      while (!Serial.available());
      String input = Serial.readStringUntil('\n');
      input.trim();

      int separatorIndex = input.indexOf("//");
      if (separatorIndex == -1) {
        Serial.println("Invalid format. Use 'SSID//password'. Try again.");
        continue;
      }

      String ssid = input.substring(0, separatorIndex);
      String password = input.substring(separatorIndex + 2);
      ssid.trim(); 
      password.trim(); 

      connectToWiFi(ssid.c_str(), password.c_str());
      break; 
    } else if (choice.equalsIgnoreCase("no")) {
      Serial.println("Select a WiFi network by entering the corresponding number or SSID followed by the password separated by '//':");

      while (!Serial.available());
      String input = Serial.readStringUntil('\n');
      input.trim();

      int separatorIndex = input.indexOf("//");
      if (separatorIndex == -1) {
        Serial.println("Invalid format. Use 'number//password' or 'SSID//password'. Try again.");
        continue;
      }

      String password = input.substring(separatorIndex + 2);
      password.trim();

      String ssidInput = input.substring(0, separatorIndex);

      int networkIndex = ssidInput.toInt() - 1;
      if (networkIndex >= 0 && networkIndex < n) {
        String ssid = WiFi.SSID(networkIndex);
        Serial.print("You selected: ");
        Serial.println(ssid);
        Serial.print("Connecting to ");
        Serial.println(ssid);
        connectToWiFi(ssid.c_str(), password.c_str());
      } else {
        Serial.print("Connecting to SSID: ");
        Serial.println(ssidInput);
        connectToWiFi(ssidInput.c_str(), password.c_str());
      }

      if (WiFi.status() == WL_CONNECTED) {
        saveAP(ssidInput.c_str(), password.c_str());
        break;
      } else {
        Serial.println("Failed to connect. Please try again.");
      }
    }
  }
}

void loop() {
  audio.loop(); // Handle audio playback
  if (Serial.available() > 0) {
    String userQuery = Serial.readStringUntil('\n');
    userQuery.trim();

    if (userQuery.length() > 0) {
      if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;

        http.setTimeout(15000);
        http.begin(endpoint);
        http.addHeader("Content-Type", "application/json");

        // Build JSON with system instruction
        DynamicJsonDocument requestDoc(8192); // Increased size
        JsonObject systemInstruction = requestDoc.createNestedObject("system_instruction");
        JsonObject systemPart = systemInstruction.createNestedArray("parts").createNestedObject();
        systemPart["text"] = String(systemPrompt) + " USER_NAME: " + userName + ", AGE: " + String(userAge);

        JsonArray contents = requestDoc.createNestedArray("contents");
        JsonObject userContent = contents.createNestedObject();
        userContent["role"] = "user";
        JsonObject userPart = userContent.createNestedArray("parts").createNestedObject();
        userPart["text"] = userQuery;

        String payload;
        serializeJson(requestDoc, payload);

        if (userQuery.length() > 0) {
          drawFace("Thinking");
          displayPrint(userName + ": \"" + userQuery + "\""); 
        }

        int httpResponseCode = http.POST(payload);

        if (httpResponseCode > 0) {
          String response = http.getString();

          DynamicJsonDocument doc(4096);
          DeserializationError error = deserializeJson(doc, response);
          Serial.println("Raw Response: " + response); // Debug: Print raw JSON

          if (!error) {
            const char* text = doc["candidates"][0]["content"]["parts"][0]["text"];
            tft.fillScreen(TFT_BLACK);
            tft.setCursor(0, 0);
            displayPrint(userName + ": \"" + userQuery + "\""); 
            displayPrint("Gemini: ");
            displayPrint(String(text));
            
            drawFace("Speaking");
            
             // Simple TTS (Google Translate unofficial API) - truncated to 100 chars for safety
            String ttsText = String(text);
            if (ttsText.length() > 100) ttsText = ttsText.substring(0, 100);
            ttsText.replace(" ", "%20");
            
            String ttsUrl = "https://translate.google.com/translate_tts?ie=UTF-8&tl=en&client=tw-ob&q=" + ttsText;
            audio.connecttohost(ttsUrl.c_str());
          } else {
            displayPrint("Error parsing JSON: " + String(error.c_str()));
          }
        } else {
          displayPrint("Request error: " + String(httpResponseCode));
          displayPrint("HTTP error: " + String(http.errorToString(httpResponseCode).c_str()));
        }

        http.end();
      } else {
        Serial.println("WiFi connection error");
      }
    }
  }
}

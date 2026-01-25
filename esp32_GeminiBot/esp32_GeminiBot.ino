#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>

const int redLedPin = 2;
const int blueLedPin = 5;

// ESP32-S3 Box Display Configuration
TFT_eSPI tft = TFT_eSPI();

const char* apiKey = "AIzaSyCRnAaw-mn2v_7zJmHkhJWCeAzy_XTF2Vk";
String endpoint = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key=" + String(apiKey);
String userName;
int userAge = 7; // Default age

const char systemPrompt[] PROGMEM = "You are an embedded AI voice companion for an ESP32-S3 Box. "
"Your goal is to interact with young users in a safe, calm, age-appropriate, and educational way. "
"Always assume the user is a child. Language: simple words, short sentences, warm tone. "
"Never sound like a teacher. Never ask personal questions or give dangerous advice. "
"AGE adaptation: 4-6 (very simple, playful), 7-9 (curious explanations), 10-12 (deeper explanations), 13-14 (factual but safe). "
"Never mention the age explicitly. Response: spoken-friendly, no long paragraphs, natural pauses.";

void displayPrint(String text, bool newLine = true) {
  Serial.print(text);
  if (newLine) Serial.println("");
  
  tft.print(text);
  if (newLine) tft.print("\n");
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
  delay(10);

  pinMode(redLedPin, OUTPUT);
  pinMode(blueLedPin, OUTPUT);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);

  // Turn on backlight
  pinMode(45, OUTPUT);
  digitalWrite(45, HIGH);

  digitalWrite(redLedPin, HIGH);
  digitalWrite(blueLedPin, LOW);

  loadAPIKey();

  displayPrint("Welcome to Gemini Bot!");
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
        DynamicJsonDocument requestDoc(4096);
        JsonObject systemInstruction = requestDoc.createNestedObject("system_instruction");
        JsonArray systemParts = systemInstruction.createNestedArray("parts");
        systemParts.add(String(systemPrompt) + " USER_NAME: " + userName + ", AGE: " + String(userAge));

        JsonArray contents = requestDoc.createNestedArray("contents");
        JsonObject userContent = contents.createNestedObject();
        JsonArray userParts = userContent.createNestedArray("parts");
        userParts.add(userQuery);

        String payload;
        serializeJson(requestDoc, payload);

        digitalWrite(blueLedPin, HIGH);

        int httpResponseCode = http.POST(payload);

        if (httpResponseCode > 0) {
          String response = http.getString();

          DynamicJsonDocument doc(4096);
          DeserializationError error = deserializeJson(doc, response);

          if (!error) {
            const char* text = doc["candidates"][0]["content"]["parts"][0]["text"];
            tft.fillScreen(TFT_BLACK);
            tft.setCursor(0, 0);
            displayPrint(userName + ": \"" + userQuery + "\""); 
            displayPrint("Gemini: ");
            displayPrint(String(text));
          } else {
            displayPrint("Error parsing JSON: " + String(error.c_str()));
          }
        } else {
          displayPrint("Request error: " + String(httpResponseCode));
          displayPrint("HTTP error: " + String(http.errorToString(httpResponseCode).c_str()));
        }

        digitalWrite(blueLedPin, LOW);

        http.end();
      } else {
        Serial.println("WiFi connection error");
      }
    }
  }
}

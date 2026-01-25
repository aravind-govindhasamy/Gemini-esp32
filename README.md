# Gemini ESP32-S3 Box Companion 🤖✨

An embedded AI voice companion that brings Google Gemini 2.0 to the ESP32-S3 Box. Designed as an age-aware companion for children, it features a custom display interface and personalized interactions.


## Features

- **Age-Aware Intelligence**: Uses Gemini 2.0 Flash with a custom master prompt tailored for children (4-14 years).
- **S3-Box Integration**: Full support for the built-in 2.4" LCD display (TFT_eSPI) and backlight.
- **Personalized Setup**: Ask for name and age on boot to customize the AI persona.
- **WiFi Management**: Scan for networks and save Access Points via Serial.

## Quick Start

1. **Hardware**: ESP32-S3 Box (or compatible ESP32-S3 with ILI9342 display).
2. **Setup API Key**: Get a free key from [Google AI Studio](https://ai.google.dev/gemini-api/docs/api-key) and place it in the `apiKey` variable in `esp32_GeminiBot.ino`.
3. **Libraries Required**:
    - `ArduinoJson` (v7+)
    - `TFT_eSPI` (Configure `User_Setup.h` for S3-Box)
4. **Flash**: Use the provided [Flashing Guide](FLASHING_GUIDE.md) for detailed steps.

## Project Structure
- `/esp32_GeminiBot`: Core Arduino firmware.
- `/prompts`: Master AI instruction sets and mode profiles.
- `FLASHING_GUIDE.md`: Step-by-step setup for beginners.

## License
MIT License.

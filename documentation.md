# Gemini-ESP32 Voice Bot: Technical Documentation

This document provides a detailed technical overview of the architecture and implementation of the Gemini-powered voice bot on the ESP32-S3 (ESP-BOX-3).

---

## 🏗️ System Architecture

The project follows a hybrid cloud-edge architecture, leveraging specialized engines for sensory input/output and large language models for core reasoning.

- **Sensory Interaction (Wit.ai)**: Handles Speech-to-Text (STT), Text-to-Speech (TTS), and Natural Language Understanding (NLU).
- **Core Reasoning (Gemini 3)**: The **Gemini 3 Flash Preview** model serves as the "brain," processing complex queries that the local intent router cannot handle.
- **Hardware Control (Local)**: A high-performance intent router executes physical commands (LED control, sensor reading) with zero cloud latency.

---

## 🎙️ Audio Pipeline & Communication

The audio pipeline is designed for high-fidelity recording and stable cloud streaming.

### 1. High-Accuracy Audio Capture
- **Format**: 16kHz, 16-bit, **MONO** PCM.
- **Logic**: The bot captures audio from the onboard microphones. By forcing a MONO 16kHz feed in [app_audio.c](file:///c:/Github/circuit-digest/Gemini-esp32/ESP-32-S3-3-OPENAI/main/app/app_audio.c), we ensure the cloud AI receives high-quality voice data.
- **Accuracy Calibration**: Fixed a critical calculation bug where `record_total_len` was treated as bytes instead of samples. The bot now correctly scales the audio payload to its full size (100% of recording vs the previous 50% truncated feed).

### 2. Task-Based STT Streaming
To achieve "Live STT" (seeing words appear as you speak) without crashing the system, we implemented a **Producer-Consumer architecture** in [wit.c](file:///c:/Github/circuit-digest/Gemini-esp32/ESP-32-S3-3-OPENAI/main/app/wit.c):

- **Ring Buffer (64KB)**: The microphone task (Producer) drops raw audio chunks into a thread-safe FreeRTOS Ring Buffer.
- **Background Task (`wit_stt_task`)**: This dedicated task (Consumer) handles the complex SSL handshake and HTTP streaming loop.
- **Duplex Communication**: The background task simultaneously sends audio and polls the server for partial transcriptions.
- **Stability Guard**: Implemented an early-exit mechanism that closes the write stream immediately upon receiving a `FINAL_UNDERSTANDING` result. This prevents `ECONNRESET` errors caused by writing into a socket closed by the server.

---

## 🧠 Intelligence & Intent Routing

The bot uses a tiered approach to understand and act on user commands.

### 1. Local Intent Router
Before querying the cloud "brain," the bot checks the transcription against a local suite of **Smart Home Intents**:

| Command Keyword | Action | Hardware Detail |
| :--- | :--- | :--- |
| "light on", "led on" | Toggles the LED ON | GPIO 2 High |
| "light off", "led off" | Toggles the LED OFF | GPIO 2 Low |
| "temperature", "sensor" | Reads internal temperature | BSP Sensor API |

### 2. Full-Sentence Recognition
Wit.ai often returns multiple JSON objects (partial vs. final). Our custom parser ([_parse_wit_response](file:///c:/Github/circuit-digest/Gemini-esp32/ESP-32-S3-3-OPENAI/main/app/wit.c#77-124)) iterates through all received objects and prioritizes the `FINAL_UNDERSTANDING` message. This ensures the bot acts on your **entire sentence**, not just the first few words recognized.

### 3. Gemini 3 Failover
If no local hardware intent is matched, the bot transparently passes the full transcription to the **Gemini 3 Flash Preview** model. The model's response is then synthesized into speech using the premium "Rebecca" voice.

---

## 🛠️ Stability & Performance

Significant effort was invested into making the bot "Robotic-Grade" stable:

- **SSL Resilience**: The HTTP client is configured with specific mbedtls stack sizes (12KB) and timeouts (15s) to survive network jitters.
- **Memory Management**: The system aggressively frees NLU results and temporary JSON buffers to maintain ~100KB of internal heap during active conversation.
- **NVS Settings**: Configuration (SSID, Gemini Key, Wit Token) is stored in a dedicated non-volatile partition for easy field updates.

---

## 📝 Configuration (NVS Keys)

The bot requires the following keys in NVS to function:

- `wit_token`: Your Wit.ai Server Access Token.
- `gemini_key`: Your Google Gemini API Key.
- `ssid` / `password`: Your local Wi-Fi credentials.

---

*Documentation compiled on 2026-01-31 by Antigravity AI.*

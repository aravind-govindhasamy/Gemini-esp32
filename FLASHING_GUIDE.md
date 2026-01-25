# ⚡ Quick Flashing Guide — ESP32-S3 Box

Since I can't physically plug into your USB port, you'll need to use the **Arduino IDE** to flash the firmware. Here is the easiest way to do it:

## 1. Install Required Library

You need the **ArduinoJson** library to handle Gemini's responses.

1. In Arduino IDE, go to **Sketch** -> **Include Library** -> **Manage Libraries...**
2. Search for `ArduinoJson` and click **Install** (use the latest version).

## 2. Set Up the Board

1. Go to **File** -> **Preferences**.
2. In "Additional Boards Manager URLs", paste this link:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. Go to **Tools** -> **Board** -> **Boards Manager...**
4. Search for `esp32` and install the board package by **Espressif Systems**.


## ⚠ Troubleshooting "No DFU capable USB device"

If you see this error, it's because the IDE is trying to use the **USB DFU** mode instead of the **Serial** mode.

**The Fix:**
1. Go to **Tools** -> **Upload Mode**.
2. Change it from "USB-OTG CDC" or "DFU" to **UART0 / Hardware Serial**.
3. Ensure **USB CDC On Boot** is still **Enabled**.
4. If it still fails, put the device in **Bootloader Mode**:
   - Hold the **BOOT** button (on the side).
   - Press and release the **RESET** button.
   - Release the **BOOT** button.
   - Now click **Upload** again.

## 3. Configure Your Board Settings

Select the following under the **Tools** menu:
- **Board**: `ESP32S3 Dev Module` (or `ESP32-S3 Box` if available)
- **USB CDC On Boot**: `Enabled` (Crucial for Serial Monitor!)
- **Flash Mode**: `QIO 80MHz`
- **Flash Size**: `16MB` (or matched to your specific box)
- **Partition Scheme**: `Default 4MB with spiffs`
- **User Port**: Select the COM port your box is plugged into.

## 4. Flash and Monitor


1. Click the **Upload** button (the arrow icon ➔).
2. Once it says "Done uploading", open the **Serial Monitor** (**Tools** -> **Serial Monitor**).
3. Set the baud rate to `115200`.
4. **Follow the prompts**: The box will ask for your **Name** and **Age** to start the AI session!

---

> [!IMPORTANT]
> If you get a "Failed to connect" error, try holding the **BOOT** button on the side while clicking Upload!

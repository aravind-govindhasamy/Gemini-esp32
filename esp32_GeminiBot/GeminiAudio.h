#ifndef GEMINI_AUDIO_H
#define GEMINI_AUDIO_H

#include "Audio.h" // Use local file
#include <Wire.h>

// S3-Box Audio Pins
#define I2S_DOUT      15
#define I2S_BCLK      17
#define I2S_LRCK      47

#define ES8311_SDA    8
#define ES8311_SCL    18
#define ES8311_ADDR   0x18

extern Audio audio;

// Basic ES8311 Initialization via I2C
void writeReg(uint8_t reg, uint8_t data) {
    Wire.beginTransmission(ES8311_ADDR);
    Wire.write(reg);
    Wire.write(data);
    Wire.endTransmission();
}

void initES8311() {
    Wire.begin(ES8311_SDA, ES8311_SCL, 100000);
    
    // Reset
    writeReg(0x00, 0x80); 
    delay(10);
    writeReg(0x00, 0x00);
    
    // Power Management
    writeReg(0x0D, 0x01); // Power up analog
    writeReg(0x0E, 0x02); // Power up digital
    
    // Volume config (0x32 is default, max is 0xBF)
    writeReg(0x32, 0xBF); 
    
    // Enable DAC output
    writeReg(0x14, 0x1A); 
    writeReg(0x12, 0x00); // Unmute
    
    Serial.println("ES8311 Initialized");
}

void setupAudio() {
    initES8311();
    
    // Initialize Audio Lib
    audio.setPinout(I2S_BCLK, I2S_LRCK, I2S_DOUT);
    audio.setVolume(15); // 0-21
}

#endif

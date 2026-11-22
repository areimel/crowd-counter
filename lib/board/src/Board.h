#pragma once
#include <Arduino.h>

#include <M5GFX.h>

// Hardware abstraction for the M5Stack Stick-S3. Nothing here
// blocks on USB serial or waits for the speaker.
namespace board {
// Maximum speaker volume tone() will use (75% of 255). M5Stack advises staying
// under ~75% on battery to avoid brownout.
constexpr uint8_t kMaxToneVolume = 191;
constexpr uint8_t BUTTON_COUNT = 2;  // KEY1 (A) and KEY2 (B)

void begin(uint8_t brightness);  // M5.begin, portrait display, brightness 0..255
void update();                   // poll buttons (M5.update)
bool isPressed(uint8_t i);       // false for an out-of-range index
int batteryPercent();            // 0..100, -1 when unknown
bool charging();
int batteryMv();                 // battery millivolts, -1 when unknown
void setBrightness(uint8_t level);  // backlight 0..255; 
// Start a tone and return immediately. volume 0..255 is capped at
// kMaxToneVolume; no-op when the speaker is unavailable or hz/ms/volume is 0.
void tone(uint16_t hz, uint16_t ms, uint8_t volume);
uint32_t rtcEpoch();             // UTC seconds from the RTC; 0 if absent or implausible (< 2024)
bool setRtcEpoch(uint32_t epoch);  // set the RTC to UTC; false if the RTC is absent or the readback differs by more than 2 s
M5GFX& display();                // 135x240 portrait
}  // namespace board

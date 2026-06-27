# DCF77 Clock Receiver

This project implements a self-contained receiver and decoder for [DCF77 long-wave time signals](https://en.wikipedia.org/wiki/DCF77) using Arduino. It reads the demodulated AM signals, decodes the time and date details, validates the signal using parity check, and outputs the information to the Serial interface.

For more information about the DCF77 signal specification and decoding logic, refer to:
- [Wikipedia Article on DCF77](https://en.wikipedia.org/wiki/DCF77)
- [Official PTB DCF77 Information](https://www.ptb.de/cms/en/ptb/fachabteilungen/abt4/fb-44/ag-442/dissemination-of-legal-time/dcf77.html)
- [PTB DCF77 Time Code Detail](https://www.ptb.de/cms/en/ptb/fachabteilungen/abt4/fb-44/ag-442/dissemination-of-legal-time/dcf77/dcf77-time-code.html)
- [Receiving and Decoding the DCF77 Time Signal (ATmega/ATtiny)](https://gabor.heja.hu/blog/2020/12/12/receiving-and-decoding-the-dcf77-time-signal-with-an-atmega-attiny-avr/)
- [Decoding Time Signals with a Microcontroller](https://dk7ih.de/decoding-time-signals-dcf77-etc-with-a-microcontroller/)

All DCF77 decoding and pulse receiving routines are separated into a modular local file pair - `dcf77_decode.h` and `dcf77_decode.cpp`.

## Features
- Modular, decoupled decoding logic.
- Time and date parsing including CET/CEST detection.
- Parity-based error checking.
- **Universal Display Support**: The decoding engine communicates with the display through universal event hooks declared in the header and implemented in the main sketch `DCF77Decode_code.ino` . It makes it easy to adapt for different screen types (LCD, OLED, TFT, etc.).

## Display Event Hooks
To support different screens, the following three hooks are provided:
1. `onDCF77Waiting()`: Called when the receiver is waiting for the start of a new minute boundary.
2. `onDCF77BitReceived(bit, index, bitArray)`: Called each time a new bit is successfully read (0 to 58). This allows you to print receiving progress or display incoming bits (e.g. as a scrolling bit string).
3. `displayOnLCD(time)`: Called to display the final decoded date and time.

An **I2C 1602 LCD** is implemented in the sketch as a reference example, but it is just one choice of display. You can adapt these event hooks to drive any screen of your choice.


## How to Adapt the Sketch for Other Displays

If you decide to swap the default I2C 1602 LCD for another screen type (for example, an **SSD1306 I2C OLED** using the Adafruit library), follow these steps in `DCF77Decode_code.ino`

### Step 1: Update Configuration and Libraries
Replace the LCD libraries and instantiation blocks. For example, for an OLED:
```cpp
// Remove the LiquidCrystal_I2C code and add Adafruit SSD1306 code:
#ifdef USE_OLED
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
Adafruit_SSD1306 display(128, 64, &Wire, -1);
#endif
```

### Step 2: Update Initialization
Update the activation function. For example, for an OLED:
```cpp
void activateLCD() {
#ifdef USE_OLED
  if (!lcdEnabled) {
    if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);
      display.setTextSize(1);
      lcdEnabled = true;
    }
  }
#endif
}
```

### Step 3: Implement Display Event Hooks
Rewrite the hook bodies to match your target screen's drawing API:

```cpp
// 1. Waiting Boundary Hook
void onDCF77Waiting() {
#ifdef USE_OLED
  if (lcdEnabled && !isSynced) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Waiting for a");
    display.println("new minute...");
    display.display();
  }
#endif
}

// 2. Bit Progress Hook (scrolling bit display with live seconds ticking)
void onDCF77BitReceived(uint8_t bit, uint8_t index, const uint8_t* bitArray) {
#ifdef USE_OLED
  if (lcdEnabled) {
    if (isSynced) {
      updateLCDDisplay(&currentTime, index);
    } else {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Receiving bits:");
      char bitsStr[17];
      memset(bitsStr, ' ', 16);
      int startIdx = (index >= 16) ? (index - 15) : 0;
      int len = index - startIdx + 1;
      for (int j = 0; j < len; j++) {
        bitsStr[j] = bitArray[startIdx + j] ? '1' : '0';
      }
      bitsStr[16] = '\0';
      display.println(bitsStr);
      display.display();
    }
  }
#endif
}

// 3. Final Decoded Data Hook
void displayOnLCD(TimeStampDCF77 *time) {
  updateLCDDisplay(time, 0);
}

// Unified Display function to output a centered, consistent date & time layout
void updateLCDDisplay(TimeStampDCF77 *time, int seconds) {
#ifdef USE_OLED
  if (lcdEnabled) {
    display.clearDisplay();
    display.setCursor(0, 0);
    char line1[17];
    char line2[17];
    
    // Line 1: centered date "   DD.MM.YYYY   "
    snprintf(line1, sizeof(line1), "   %02d.%02d.20%02d   ", 
             time->day, time->month, time->year);
             
    // Line 2: consistent time "  HH:MM:SS TZ   "
    snprintf(line2, sizeof(line2), "  %02d:%02d:%02d %-4s ", 
             time->hour, time->minute, seconds, 
             syncError ? "ERR" : (time->CEST ? "CEST" : "CET"));
             
    display.println(line1);
    display.println(line2);
    display.display();
  }
#endif
}
```

---

## Wiring Diagram (Example: Arduino Uno/Nano)
- **DCF77 Receiver module**:
  - `VCC` -> Arduino `5V` / `3.3V` (depending on module specification)
  - `GND` -> Arduino `GND`
  - `Data` -> Arduino Digital Pin `2`
- **I2C Screen (e.g. LCD or OLED)** (Optional):
  - `VCC` -> Arduino `5V` / `3.3V`
  - `GND` -> Arduino `GND`
  - `SDA` -> Arduino SDA Pin (A4 on Uno/Nano)
  - `SCL` -> Arduino SCL Pin (A5 on Uno/Nano)

## Getting Started
1. Open the project in the Arduino IDE.
2. Compile and upload the sketch to your board.
3. Open the Serial Monitor at `115200` baud.

### Enabling the Reference I2C LCD Example
1. Install the `LiquidCrystal_I2C` library in the Arduino IDE.
2. In `DCF77Decode_code.ino`, uncomment:
   ```cpp
   #define USE_LCD
   ```
3. In `DCF77Decode_code.ino`, uncomment the setup line in `loop()`:
   ```cpp
   activateLCD();
   ```

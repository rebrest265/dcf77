/*
  DCF77 Clock Receiver Base Example

  This example demonstrates how to receive demodulated DCF77 signals from a 
  receiver module, decode the time/date data, and print the results to the Serial interface.
  The DCF77 decoding and receiving logic is fully separated into the helper module "dcf77_decode.h".

  For more details about the DCF77 signal specification and decoding logic, refer to:
  - [Wikipedia Article on DCF77](https://en.wikipedia.org/wiki/DCF77)
  - [Official PTB DCF77 Information](https://www.ptb.de/cms/en/ptb/fachabteilungen/abt4/fb-44/ag-442/dissemination-of-legal-time/dcf77.html)
  - [PTB DCF77 Time Code Detail](https://www.ptb.de/cms/en/ptb/fachabteilungen/abt4/fb-44/ag-442/dissemination-of-legal-time/dcf77/dcf77-time-code.html)
  - [Receiving and Decoding the DCF77 Time Signal (ATmega/ATtiny)](https://gabor.heja.hu/blog/2020/12/12/receiving-and-decoding-the-dcf77-time-signal-with-an-atmega-attiny-avr/)
  - [Decoding Time Signals with a Microcontroller](https://dk7ih.de/decoding-time-signals-dcf77-etc-with-a-microcontroller/)

  Optional Display Output:
  This sketch also serves as an example of displaying status and results on an external screen.
  All display-specific configurations and drivers are defined directly in this sketch via
  universal event hooks. An I2C 1602 LCD is implemented as a default example, but you can 
  easily replace it with SSD1306 OLED, Nokia 5110, or other displays by customizing 
  the event hook functions.

  Serial Output Format:
    It is now XX:XX o'clock CET/CEST
    Today is XX.XX.20XX
    Weekday: XX
*/

#include "dcf77_decode.h"

// --- Optional I2C LCD Configuration ---
// Uncomment the line below to enable LiquidCrystal_I2C LCD support.
// Note: Requires installing the "LiquidCrystal_I2C" library.
// #define USE_LCD

#ifdef USE_LCD
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
// Instantiate the LCD. Change the address (commonly 0x27 or 0x3F) to match your screen.
LiquidCrystal_I2C lcd(0x27, 16, 2);
#endif

// Global flag to track whether screen output is active
bool lcdEnabled = false;

// Global flag to track synchronization state
bool isSynced = false;
bool syncError = false;

// --- Global Variables ---
uint8_t bitArray[DCF77_STRING_SIZE]; // Memory location for received bits
TimeStampDCF77 currentTime;          // Decoded time and date
TimeStampDCF77 nextTime;             // Buffered time for the upcoming minute
bool nextTimeValid = false;          // True if nextTime contains a valid decoded time
int ReceiveStatus;                   // DCF77 receiving status
char buffer[80];                     // Cache buffer for formatting output

unsigned long lastSecondMillis = 0;  // For software clock ticking
int currentSeconds = 0;              // Current tracked seconds

// --- Display Driver Helper Functions ---
void activateLCD();
void displayOnLCD(TimeStampDCF77 *time);
void updateLCDDisplay(TimeStampDCF77 *time, int seconds);
void displayErrorOnLCD(const char* errorLine1, const char* errorLine2);
void incrementCurrentTime();
void advanceSoftwareClock();

void advanceSoftwareClock() {
  if (!isSynced) return;
  
  unsigned long now = millis();
  if (now - lastSecondMillis >= 1000) {
    int elapsedSeconds = (now - lastSecondMillis) / 1000;
    lastSecondMillis += elapsedSeconds * 1000;
    
    currentSeconds += elapsedSeconds;
    while (currentSeconds >= 60) {
      currentSeconds -= 60;
      incrementCurrentTime();
      if (nextTimeValid) {
        currentTime = nextTime;
        nextTimeValid = false;
        syncError = false;
      } else {
        syncError = true;
      }
    }
    updateLCDDisplay(&currentTime, currentSeconds);
  }
}

void onDCF77Tick() {
  advanceSoftwareClock();
}

void setup()
{
  Serial.begin(115200);
  delay(7000);        // Wait for the DCF77 module hardware to stabilize
  setupDCF77(2);     // Set MCU digital input pin for DCF77 (Pin 2)
}

void loop() 
{
  // 2. Uncomment the line below to activate LCD output on demand
  // activateLCD();

  advanceSoftwareClock(); // Keep seconds ticking if signal is lost

  ReceiveStatus = receiveDCF77(bitArray, DCF77_STRING_SIZE); // Wait and read a 1-minute frame
  
  if (ReceiveStatus == SUCCESS)
  {
    TimeStampDCF77 tempTime;
    if (decodeDCF77(bitArray, DCF77_STRING_SIZE, &tempTime) == SUCCESS)
    {
      nextTime = tempTime;
      nextTimeValid = true;
      
      // 1. Keep the current outputs to Serial
      snprintf(buffer, sizeof(buffer), "It is now %02d:%02d o'clock %s", 
               tempTime.hour, tempTime.minute, (tempTime.CEST) ? "CEST" : "CET");
      Serial.println(buffer);
      
      snprintf(buffer, sizeof(buffer), "Today is %02d.%02d.20%02d", 
               tempTime.day, tempTime.month, tempTime.year);
      Serial.println(buffer);
      
      snprintf(buffer, sizeof(buffer), "Weekday: %02d\n", tempTime.weekday);
      Serial.println(buffer);
      
      if (tempTime.transmitter_fault != SUCCESS)
      {
        Serial.println("Bad signal, or something wrong with transmitter.");
      }
      else if (tempTime.A1)
      {
        Serial.println("CET/CEST Time change is coming up.");
      }
    }
    else 
    {
      Serial.println("Signal unstable, please readjust antenna.");
      if (isSynced)
      {
        syncError = true;
      }
      else
      {
        displayErrorOnLCD("Decoding Error", "Bad parity/date");
      }
    }
  }
  else if (ReceiveStatus == ERROR_TIMEOUT)
  {
    Serial.println("\nDCF77 signal unstable, please wait or readjust antenna.");
    if (isSynced)
    {
      syncError = true;
    }
    else
    {
      displayErrorOnLCD("Signal Timeout", "Readjust antenna");
    }
  }
}

// Helper to advance time by one minute (with simple rollover approximation)
void incrementCurrentTime()
{
  currentTime.minute++;
  if (currentTime.minute >= 60)
  {
    currentTime.minute = 0;
    currentTime.hour++;
    if (currentTime.hour >= 24)
    {
      currentTime.hour = 0;
      currentTime.day++;
    }
  }
}

// --- Universal Display Event Hook Implementations ---
// Customize these functions to adapt the output for OLEDs, TFTs, or other displays.

// Step 1: Called when starting to wait for a minute boundary
void onDCF77Waiting()
{
#ifdef USE_LCD
  if (lcdEnabled && !isSynced)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Waiting for a");
    lcd.setCursor(0, 1);
    lcd.print("new minute");
  }
#endif
}

// Step 2: Called each time a new bit is successfully read (0 to 58)
// When synced, the bit index equals the current second within the minute.
void onDCF77BitReceived(uint8_t bit, uint8_t index, const uint8_t* bitArray)
{
  if (index == 0) {
    // Deduplicate minute rollover: only apply if the software clock hasn't already rolled over
    if (!isSynced || currentSeconds >= 50) {
      if (nextTimeValid) {
        currentTime = nextTime;
        nextTimeValid = false;
        isSynced = true;
        syncError = false;
      } else if (isSynced) {
        incrementCurrentTime();
        syncError = true;
      }
    }
  }

#ifdef USE_LCD
  if (lcdEnabled)
  {
    if (isSynced)
    {
      currentSeconds = index;
      lastSecondMillis = millis();
      // Show synced time with live seconds counter
      updateLCDDisplay(&currentTime, currentSeconds);
    }
    else
    {
      // Line 1: Receiving DCF77 status
      lcd.setCursor(0, 0);
      lcd.print("Receiving DCF77 "); // Pad to overwrite
      
      // Line 2: scrolling list of bits (show last 16 bits)
      char bitsStr[17];
      memset(bitsStr, ' ', 16); // Fill with space padding
      int startIdx = (index >= 16) ? (index - 15) : 0;
      int len = index - startIdx + 1;
      for (int j = 0; j < len; j++) 
      {
        bitsStr[j] = bitArray[startIdx + j] ? '1' : '0';
      }
      bitsStr[16] = '\0';
      
      lcd.setCursor(0, 1);
      lcd.print(bitsStr);
    }
  }
#endif
}

// Step 3: Called to display the final decoded date and time
void displayOnLCD(TimeStampDCF77 *time) 
{
  updateLCDDisplay(time, 0);
}

// Unified Display function to output a centered, consistent date & time layout
void updateLCDDisplay(TimeStampDCF77 *time, int seconds)
{
#ifdef USE_LCD
  if (lcdEnabled)
  {
    char line1[17];
    char line2[17];
    
    // Line 1: centered date "   DD.MM.YYYY   " (3 spaces + 10 chars + 3 spaces = 16 chars)
    snprintf(line1, sizeof(line1), "   %02d.%02d.20%02d   ", 
             time->day, time->month, time->year);
             
    // Line 2: consistent time "  HH:MM:SS TZ   "
    // %-4s ensures CET, CEST, and ERR are aligned consistently to 4 characters wide
    snprintf(line2, sizeof(line2), "  %02d:%02d:%02d %-4s ", 
             time->hour, time->minute, seconds, 
             syncError ? "ERR" : (time->CEST ? "CEST" : "CET"));
             
    lcd.setCursor(0, 0);
    lcd.print(line1);
    lcd.setCursor(0, 1);
    lcd.print(line2);
  }
#endif
}

// Displays an error message on the LCD when synchronization is lost or fails
void displayErrorOnLCD(const char* errorLine1, const char* errorLine2)
{
#ifdef USE_LCD
  if (lcdEnabled) 
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(errorLine1);
    lcd.setCursor(0, 1);
    lcd.print(errorLine2);
  }
#endif
}

// Initializes the LCD on demand
void activateLCD()
{
#ifdef USE_LCD
  if (!lcdEnabled) 
  {
    lcd.init();
    lcd.backlight();
    lcdEnabled = true;
  }
#endif
}

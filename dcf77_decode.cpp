#include "dcf77_decode.h"

// Private global pin variable
static uint8_t DCF77_PIN = 13;

// Internal function declarations
static int BitScaleDCF77(uint8_t *bitstring, uint8_t length);
static int checkParity(uint8_t *bitArray);

// Sets the digital pin connected to the DCF77 receiver
void setupDCF77(uint8_t pin)
{
  pinMode(pin, INPUT);
  DCF77_PIN = pin;
}

// Receives pulses for a full minute, measuring pulse durations to decode bits
int receiveDCF77(uint8_t* bitArray, uint8_t size) 
{
  unsigned long volatile highDuration;
  bool minuteMarkerDetected = false;
  uint8_t i = 0;

  if (size != DCF77_STRING_SIZE) 
  {
    #ifdef DEBUG_SERIAL
    Serial.println("Error: The passed bitArray size does not match DCF77_STRING_SIZE.");
    #endif
    return ERROR_INVALID_VALUE;
  }
  
  // Trigger event hook: Waiting for a new minute
  onDCF77Waiting();

  // Wait until the current minute is over (detected by lack of pulse for >= 1.5 seconds)
  unsigned long lowStart = millis();
  while (millis() - lowStart < 1500) 
  {
    onDCF77Tick();
    if (digitalRead(DCF77_PIN) == HIGH) 
    {
      // Wait until the pulse ends (goes LOW) to start measuring the LOW period
      unsigned long pulseStart = millis();
      while (digitalRead(DCF77_PIN) == HIGH) 
      {
        onDCF77Tick();
        if (millis() - pulseStart > 1000) 
        {
          break; // Safeguard against stuck HIGH state
        }
      }
      lowStart = millis();
    }
  }

  // Receive and decode the DCF77 pulses for 59 seconds
  for (i = 0; i < DCF77_STRING_SIZE; i++) 
  {
    highDuration = pulseIn(DCF77_PIN, HIGH, TIMEOUT_DURATION);
    onDCF77Tick(); // Keep the software clock ticking!
    
    if (highDuration == 0) 
    {
      #ifdef DEBUG_SERIAL
      Serial.println("\nError: Unexpected DCF77 connection timeout.");
      #endif
      return ERROR_TIMEOUT;
    }

    if (highDuration > BIT_0_DURATION && highDuration <= BIT_1_DURATION) 
    {
      bitArray[i] = 1;
    } 
    else if (highDuration <= BIT_0_DURATION && highDuration > min_BIT_0_DURATION) 
    {
      bitArray[i] = 0;
    }
    else 
    {
      bitArray[i] = 0; // Default fallback for out-of-range pulses
    }
    
    #ifdef DEBUG_SERIAL
    Serial.print(bitArray[i]);
    #endif

    // Trigger event hook: Bit received
    onDCF77BitReceived(bitArray[i], i, bitArray);
  }
  
  return SUCCESS;
}

// Converts a range of bits representing a binary-coded decimal value into an integer
static int BitScaleDCF77(uint8_t *bitstring, uint8_t length) 
{
  static const int weights[] = {1, 2, 4, 8, 10, 20, 40, 80};
  int value = 0;
  static const int weights_len = 8;
  for (int i = 0; i < length && i < weights_len; i++) 
  {
    value += weights[i] * bitstring[i];
  }
  return value;
}

// Checks the parity bits of the received frame to detect transmission errors
static int checkParity(uint8_t *bitArray)
{
  uint8_t minuteParity = 0;
  uint8_t hourParity = 0;
  uint8_t dateParity = 0;

  // Parity for minutes (bits 21 to 27)
  for (uint8_t i = 21; i < 28; ++i)
  {
    minuteParity ^= bitArray[i];
  }

  // Parity for hours (bits 29 to 34)
  for (uint8_t i = 29; i < 35; ++i)
  {
    hourParity ^= bitArray[i];
  }

  // Parity for date (bits 36 to 57)
  for (uint8_t i = 36; i < 58; ++i)
  {
    dateParity ^= bitArray[i];
  }

  // Verify against parity bits at index 28, 35, and 58
  if ((minuteParity != bitArray[28]) || (hourParity != bitArray[35]) || (dateParity != bitArray[58]))
  {
    return ERROR_INVALID_VALUE; // Parity error detected
  }

  return SUCCESS;
}

// Decodes the raw bit array into time structure components
int decodeDCF77(uint8_t *bitArray, uint8_t size, TimeStampDCF77 *time) 
{
  if (size != DCF77_STRING_SIZE) 
  {
    #ifdef DEBUG_SERIAL
    Serial.println("Error: The passed bitArray size does not match DCF77_STRING_SIZE.");
    #endif
    return ERROR_INVALID_VALUE;
  }

  time->hour = BitScaleDCF77(bitArray + 29, 6);
  time->minute = BitScaleDCF77(bitArray + 21, 7);
  time->day = BitScaleDCF77(bitArray + 36, 6);
  time->weekday = BitScaleDCF77(bitArray + 42, 3);
  time->month = BitScaleDCF77(bitArray + 45, 5);
  time->year = BitScaleDCF77(bitArray + 50, 8);
  time->transmitter_fault = BitScaleDCF77(bitArray + 15, 1);
  time->A1 = BitScaleDCF77(bitArray + 16, 1);
  time->CEST = BitScaleDCF77(bitArray + 17, 1);
  time->CET = BitScaleDCF77(bitArray + 18, 1);

  if (checkParity(bitArray) == ERROR_INVALID_VALUE)
  {
    #ifdef DEBUG_SERIAL
    Serial.println("\nError: Parity validation failed in hour, minute, or date.");
    #endif
    return ERROR_INVALID_VALUE;
  }

  // Validate values for date/time plausibility (range checks)
  if (time->minute > 59 || time->hour > 23 ||
      time->day == 0 || time->day > 31 ||
      time->month == 0 || time->month > 12 ||
      time->year > 99 ||
      time->weekday == 0 || time->weekday > 7 ||
      (time->CEST == time->CET))
  {
    #ifdef DEBUG_SERIAL
    Serial.println("\nError: Plausibility validation failed (values out of range or invalid DST status).");
    #endif
    return ERROR_INVALID_VALUE;
  }

  #ifdef DEBUG_SERIAL
  Serial.print("\nTime: ");
  Serial.print(time->hour);
  Serial.print(":");
  Serial.println(time->minute);
  Serial.print("Date: ");
  Serial.print(time->day);
  Serial.print(".");
  Serial.print(time->month);
  Serial.print(".20");
  Serial.println(time->year);
  #endif 

  return SUCCESS;
}

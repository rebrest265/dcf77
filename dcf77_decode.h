#ifndef DCF77_DECODE_H
#define DCF77_DECODE_H

#include <Arduino.h>

// --- Configuration & Constants ---
#define SUCCESS 0
#define ERROR_INVALID_VALUE -1
#define ERROR_TIMEOUT -2

// Comment out DEBUG_SERIAL to disable internal verbose serial debugging
#define DEBUG_SERIAL 

#define BIT_0_DURATION 130000      // Max duration in microseconds for binary 0 (100ms pulse)
#define BIT_1_DURATION 240000      // Max duration in microseconds for binary 1 (200ms pulse)
#define min_BIT_0_DURATION 20000   // Pulses shorter than this are ignored as noise
#define DCF77_STRING_SIZE 59       // DCF77 frame size in bits (1 minute)
#define TIMEOUT_DURATION 1600000   // Timeout for receiving a pulse (1.6s)

// --- Data Types ---
struct TimeStampDCF77 
{
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t weekday;
    uint8_t month;
    uint8_t year;
    uint8_t A1;                 // Impending CET/CEST change announcement bit
    uint8_t CEST;               // CEST active
    uint8_t CET;                // CET active
    int8_t transmitter_fault;   // Transmitter call bit (fault/abnormal status)
};

// --- Universal Display / Status Event Hooks ---
// These functions are implemented in the main sketch (.ino) to update the display hardware.
extern void onDCF77Waiting();
extern void onDCF77BitReceived(uint8_t bit, uint8_t index, const uint8_t* bitArray);
extern void onDCF77Tick();


// --- Function Declarations ---
void setupDCF77(uint8_t pin);
int receiveDCF77(uint8_t* bitArray, uint8_t size);
int decodeDCF77(uint8_t *bitArray, uint8_t size, TimeStampDCF77 *time);

#endif // DCF77_DECODE_H

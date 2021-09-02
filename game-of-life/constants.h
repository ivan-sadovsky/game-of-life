#ifndef CONSTANTS_H
#define CONSTANTS_H

#define LED_PIN 13

#define CURRENT_LIMIT 1000                 // Current limit [mA]; 0 - no limit
#define BRIGHTNESS 140                     // [0-255]

#define LED_COLS 8
#define LED_ROWS 8

#define NUM_LEDS (2*LED_COLS-1)*LED_COLS   // Roughly, every other LED is covered.
                                           // Depends on the particuler wiring. See also get_pixel_index()

#define OPEN_BC 1
#define PERIODIC_BC 2
#define BC OPEN_BC                         // Boundary condition. Can be either OPEN_BC or PERIODIC_BC

#define CHECKSUM_CRC32 1                   // Slow and relatively reliable
#define CHECKSUM_SIMPL_CRC32 2             // Simplified version of CRC32
#define CHECKSUM_ADLER32 3                 // Fast and not very reliable
#define CHECKSUM_TYPE CHECKSUM_SIMPL_CRC32

#define HISTORY_LENGTH 256

#define STEP_PERIOD_MSEC 700               // State changes every STEP_PERIOD_MSEC

#define EFFECT_PERIOD_MSEC 14              // Update time for the effects. Should be 20-100 times smaller than STEP_PERIOD_MSEC

#define FADE_TURN_ON_LENGTH 15             // Out of STEP_PERIOD_MSEC/EFFECT_PERIOD_MSEC
#define FADE_TURN_ON_FLICKER_LENGTH 0
#define FADE_TURN_OFF_LENGTH 10            // Out of STEP_PERIOD_MSEC/EFFECT_PERIOD_MSEC
#define FADE_TO_BLACK_STEP 8

#endif // CONSTANTS_H

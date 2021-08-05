#define LED_PIN 13

#define CURRENT_LIMIT 2000                 // Current limit [mA]; 0 - no limit
#define BRIGHTNESS 250                     // [0-255]

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


#include <FastLED.h>


CRGB leds[NUM_LEDS];
byte state[LED_COLS][LED_ROWS];
byte state_next[LED_COLS][LED_ROWS];

uint32_t step_start_time;
int16_t between_steps_counter;

uint32_t effect_start_time;
uint16_t effect_step_counter;

uint32_t checksum_history[HISTORY_LENGTH];
uint16_t checksum_history_length;
int16_t checksum_history_pos;
int16_t repeating_checksum_history_counter;

byte base_color_red, base_color_green, base_color_blue;
// byte base_color_red_dispersion, base_color_green_dispersion, base_color_blue_dispersion;


uint32_t get_random_seed() {
  // Generates a random seed using 32 values from different analog inputs. 
  // A single measurement of the same analog input often gives the same seed value.
  
  uint32_t seed = 0;
  uint32_t s;
  byte a = 0; // Number of the analog input [0-5]

  for (byte l = 0; l < 32; l++) {
    s = analogRead(a);
    seed ^= s<<l;
    
    a++;
    if (a>5)
      a = 0;
  }

  return seed;  
}

inline uint16_t get_pixel_index(byte x, byte y) {
  // Depends on the particuler wiring. See also NUM_LEDS definition
  if (y%2==0)
    return (2*LED_COLS-1)*y + 2*x;
  else
    return (2*LED_COLS-1)*(y+1) - 2*x - 1;
}

void copy_state(byte from[LED_COLS][LED_ROWS], byte to[LED_COLS][LED_ROWS]) {
  for (byte x = 0; x < LED_COLS; x++)
    for (byte y = 0; y < LED_ROWS; y++)
      to[x][y] = from[x][y];
}

void swap_states(byte state1[LED_COLS][LED_ROWS], byte state2[LED_COLS][LED_ROWS]) {
  byte t;
  for (byte x = 0; x < LED_COLS; x++)
    for (byte y = 0; y < LED_ROWS; y++) {
      t = state1[x][y];
      state1[x][y] = state2[x][y];
      state2[x][y] = t;
    }
}

void init_empty_state(byte state[LED_COLS][LED_ROWS]) {
  for (byte x = 0; x < LED_COLS; x++) 
    for (byte y = 0; y < LED_ROWS; y++)
      state[x][y] = 0;
}

void init_state(byte state[LED_COLS][LED_ROWS]) {
  // TODO: simulate several states for 10--100 steps and choose the most interesting one; this can be done in parallel
  init_empty_state(state);
  
  byte num = random(3*LED_COLS*LED_ROWS/16, LED_COLS*LED_ROWS/2);
  for (byte k = 0; k < num; k++) {
    state[random(0, LED_COLS)][random(0, LED_ROWS)] = 1;
  }
}

bool is_empty_state(byte state[LED_COLS][LED_ROWS]) {
  for (byte x = 0; x < LED_COLS; x++) 
    for (byte y = 0; y < LED_ROWS; y++)
      if (state[x][y]!=0)
        return false;
  return true;
}

byte game_of_life_rule(byte pixel_state, byte neighbours_number) {
    if (pixel_state==0)
      return neighbours_number==3 ? 1 : 0;
    else
      return neighbours_number==2 || neighbours_number==3 ? 1 : 0;
}

#if BC == OPEN_BC
void update_state(byte state[LED_COLS][LED_ROWS], byte state_next[LED_COLS][LED_ROWS]) { // open BC
  byte nn; // neighbours number
  for (byte x = 0; x < LED_COLS; x++) {
    for (byte y = 0; y < LED_ROWS; y++) {
      nn = 0;
      
      if (x > 0) {
        if (y > 0) nn += state[x-1][y-1];
        nn += state[x-1][y ];
        if (y < LED_ROWS-1) nn += state[x-1][y+1];
      }

      if (y > 0) nn += state[x][y-1];
      if (y < LED_ROWS-1) nn += state[x][y+1];
      
      if (x < LED_COLS-1) {
        if (y > 0) nn += state[x+1][y-1];
        nn += state[x+1][y ];
        if (y < LED_ROWS-1) nn += state[x+1][y+1];
      }
      
      state_next[x][y] = game_of_life_rule(state[x][y], nn);
    }
  }
}

#elif BC == PERIODIC_BC
void update_state(byte state[LED_COLS][LED_ROWS], byte state_next[LED_COLS][LED_ROWS]) { // periodic BC
  byte nn; // neighbours number
  byte xm, xp, ym, yp;

  for (byte x = 0; x < LED_COLS; x++) {
    xm = x > 0 ? x-1 : LED_COLS-1;
    xp = x < LED_COLS-1 ? x+1 : 0;
    for (byte y = 0; y < LED_ROWS; y++) {
      ym = y > 0 ? y-1 : LED_ROWS-1;
      yp = y < LED_ROWS-1 ? y+1 : 0;
      
      nn = 0;
      
      nn += state[xm][ym];
      nn += state[xm][y ];
      nn += state[xm][yp];

      nn += state[x ][ym];
      nn += state[x ][yp];
      
      nn += state[xp][ym];
      nn += state[xp][y ];
      nn += state[xp][yp];
      
      state_next[x][y] = game_of_life_rule(state[x][y], nn);
    }
  }
}
#endif

#if CHECKSUM_TYPE == CHECKSUM_CRC32
uint32_t get_state_checksum(byte state[LED_COLS][LED_ROWS]) {
  // CRC32 algorithm
  // Takes about 0.76 ms for 8x8 array
  // TODO: implement for periodic BC
  uint32_t crc = 0xFFFFFFFF;
  for (byte x = 0; x < LED_COLS; x++)
    for (byte y = 0; y < LED_ROWS; y++) {
      crc ^= state[x][y];
      for (byte l = 0; l < 8; l++)
          crc = crc & 1 ? (crc >> 1) ^ 0x82f63b78 : crc >> 1;
    }
  return ~crc;
}

#elif CHECKSUM_TYPE == CHECKSUM_SIMPL_CRC32
uint32_t get_state_checksum(byte state[LED_COLS][LED_ROWS]) {
  // Simplified CRC32 algorithm; should be equivalent to CRC32 for states 0 and 1
  // Takes about 0.088 ms for 8x8 array
  // TODO: implement for periodic BC
  uint32_t crc = 0xFFFFFFFF;
  for (byte x = 0; x < LED_COLS; x++)
    for (byte y = 0; y < LED_ROWS; y++) {
      crc ^= state[x][y];
      crc = crc & 1 ? (crc >> 1) ^ 0x82f63b78 : crc >> 1;
    }
  return ~crc;
}

#elif CHECKSUM_TYPE == CHECKSUM_ADLER32
uint32_t get_state_checksum(byte state[LED_COLS][LED_ROWS]) {
  // Adler-32 algorithm
  // Takes about 4.7 ms for 8x8 array (not very optimized due to % operation at every step)
  // TODO: implement for periodic BC
  uint32_t a = 1, b = 0;
  for (byte x = 0; x < LED_COLS; x++)
    for (byte y = 0; y < LED_ROWS; y++) {
      a = (a + state[x][y]) % 65521;
      b = (b + a) % 65521;
  }
  return (b << 16) | a;
}
#endif

void init_checksum_history() {
  for (uint16_t k = 0; k < HISTORY_LENGTH; k++) 
    checksum_history[k] = 0;
  checksum_history_length = 0;
  checksum_history_pos = -1;
  repeating_checksum_history_counter = 0;
}

void add_to_checksum_history(byte state[LED_COLS][LED_ROWS]) {
  checksum_history_pos++;
  if (checksum_history_pos >= HISTORY_LENGTH)
    checksum_history_pos = 0;
  
  checksum_history[checksum_history_pos] = get_state_checksum(state);

  if (checksum_history_length<HISTORY_LENGTH)
    checksum_history_length++;
}

bool is_checksum_history_repeating(byte state[LED_COLS][LED_ROWS]) {
  int16_t period = -1;
  uint16_t k = checksum_history_pos;
  uint32_t checksum0 = checksum_history[k], checksum;

  for (uint16_t l = 1; l < checksum_history_length; l++) {
    if (k>0)
      k--;
    else
      k = HISTORY_LENGTH-1;

    checksum = checksum_history[k];

    if (checksum == checksum0) {
      period = l;
      break;
    }
  }

  if (period > 0) {
    repeating_checksum_history_counter++;

    int16_t time_to_die;
    if (period==1) {
      if (is_empty_state(state))
        time_to_die = 0;
      else
        time_to_die = 8;
    } else
      time_to_die = 16 * period;

    if (repeating_checksum_history_counter > time_to_die)
      return true;
  }

  return false;
}

void init_color(int16_t intensity = 120, int16_t overall_dispersion = 40) {
  // TODO: add color schemes
  // TODO: add some color dispersion to individual pixels
  int16_t r, g, b, rgb;
  byte scheme = random(1);
  if (scheme == 0) {
    r = random(intensity-overall_dispersion, intensity+overall_dispersion);
    g = random(intensity-overall_dispersion, intensity+overall_dispersion);
    b = 3*intensity - r - g;

    // base_color_red_dispersion = 30;
    // base_color_green_dispersion = 30;
    // base_color_blue_dispersion = 30;
  } 
  // else if (scheme == 1) {
  //   r = 255;
  //   g = random(127);
  //   b = random(31);
  // }

  base_color_red   = r;
  base_color_green = g;
  base_color_blue  = b;
}

// static inline uint32_t rotate_right(uint32_t u, size_t r) {
//     __asm__ ("rorl %%cl, %0" : "+r" (u) : "c" (r));
//     return u;
// }

// static inline uint32_t ror(uint32_t u, size_t r) {
//     ((unsigned)(x) >> (y) | (unsigned)(x) << 32 - (y))
//     return u;
// }
// #define ROR(x,y) ((unsigned)(x) >> (y) | (unsigned)(x) << 32 - (y))

void setup() {
  Serial.begin(9600);

  FastLED.addLeds<WS2811, LED_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  if (CURRENT_LIMIT > 0)
    FastLED.setMaxPowerInVoltsAndMilliamps(5, CURRENT_LIMIT);
  FastLED.setBrightness(BRIGHTNESS);
  fill_solid(leds, NUM_LEDS, CRGB(0, 0, 0));
  FastLED.show();

// randomSeed(0);
// uint32_t t = millis();
// init_empty_state(state);
// init_state(state_next);
// // init_checksum_history();
// // add_to_checksum_history(state_next);
// for (uint16_t k = 0; k < 100; k++) {
//   copy_state(state_next, state);
//   update_state(state, state_next);
//   // add_to_checksum_history(state_next);
// }
// Serial.print(millis()-t);  Serial.print(" ms\n"); 

// a = a >> 7 | a << 32 - 7;  // ror a,7

// uint32_t a = 0xFFFFFFFF-1;
// Serial.print(a, BIN); Serial.print("\n"); 
// for (uint16_t k = 0; k < 32; k++) {
//   a = a >> 7 | a << 32 - 7;
//   Serial.print(a, BIN); Serial.print("\n"); 
// }

// byte s = 2;
// uint32_t a = 0xFFFFFFFF;
// Serial.print("init: "); Serial.print(~a, BIN); Serial.print("\n");
// for (uint16_t k = 0; k < 64; k++) {
//   a ^= s;
//   // a = (a >> 1 | a << 32 - 1);
//   // a ^= ~s;
//   // a = (a >> 3 | a << 32 - 3);
//   // a = (a >> 3 | a << 32 - 3) ^ 0x82f63b78;
//   a = a & 1 ? (a >> 1) ^ 0x82f63b78 : a >> 1;
//   Serial.print(~a, BIN); Serial.print("\n");
// }

// for (byte x = 0; x < LED_COLS; x++)
//   for (byte y = 0; y < LED_ROWS; y++) {
//     init_empty_state(state);
//     state[x][y] = 1;
//     Serial.print(get_state_checksum(state), BIN); Serial.print("\n");
    
// }


  randomSeed(get_random_seed());
  init_empty_state(state);
  init_state(state_next);

  init_checksum_history();
  add_to_checksum_history(state_next);
  
  init_color();

  between_steps_counter = -1;
  effect_step_counter = 0;
  effect_start_time = step_start_time = millis();
}

void loop() {
  uint32_t current_time = millis();
  
  if (current_time - step_start_time >= STEP_PERIOD_MSEC) {
    // Game of life logic.
    // In this part we update state_next only
    copy_state(state_next, state); // takes about 0.04 ms

    update_state(state, state_next); // takes about 0.3 ms

// uint32_t t;
// t = millis();
// for (uint16_t k = 0; k < 1000; k++) {
//     add_to_checksum_history(state_next);
// }
// Serial.print(millis()-t);  Serial.print(" ms\n"); 


    if (between_steps_counter == -1 && is_checksum_history_repeating(state_next))
      between_steps_counter = 0;

    if (between_steps_counter >= 3) {
      randomSeed(get_random_seed());

      init_empty_state(state);
      init_state(state_next);

      init_checksum_history();
      add_to_checksum_history(state_next);
      
      init_color();

      between_steps_counter = -1;
    }
    
    if (between_steps_counter >= 0)
      between_steps_counter++;
    
    effect_step_counter = 0;
    step_start_time = current_time;
  }

  if (current_time - effect_start_time >= EFFECT_PERIOD_MSEC) {      
    // LED strip update with effects
    // 1. Here we should have different state and state_next
    // 2. This part is supposed to be fast
    
    byte s, s_next;
    int16_t r, g, b;

    if (between_steps_counter == -1) {
      // Change from state to state_next
      for (byte x = 0; x < LED_COLS; x++)
        for (byte y = 0; y < LED_ROWS; y++) {
          s = state[x][y];
          s_next = state_next[x][y];

          // r = base_color_red + random(-base_color_red_dispersion/2, base_color_red_dispersion/2)

          if (s==0 && s_next==1) {
            // Pixel turns on
            if (effect_step_counter <= FADE_TURN_ON_LENGTH + FADE_TURN_ON_FLICKER_LENGTH) {
              r = effect_step_counter * base_color_red / FADE_TURN_ON_LENGTH;
              g = effect_step_counter * base_color_green / FADE_TURN_ON_LENGTH;
              b = effect_step_counter * base_color_blue / FADE_TURN_ON_LENGTH;
            } else {
              r = base_color_red;
              g = base_color_green;
              b = base_color_blue;
            }
          
          } else if (s==1 && s_next==0) {
            // Pixel turns off
            if (effect_step_counter <= FADE_TURN_OFF_LENGTH) {
              r = (FADE_TURN_OFF_LENGTH-effect_step_counter) * base_color_red / FADE_TURN_OFF_LENGTH;
              g = (FADE_TURN_OFF_LENGTH-effect_step_counter) * base_color_green / FADE_TURN_OFF_LENGTH;
              b = (FADE_TURN_OFF_LENGTH-effect_step_counter) * base_color_blue / FADE_TURN_OFF_LENGTH;
            } else {
              r = g = b = 0;
            }
          
          } else if (s==1 && s_next==1) {
            // Pixel remains on
            r = base_color_red;
            g = base_color_green;
            b = base_color_blue;

          } else if (s==0 && s_next==0) {
            // Pixel remains off
            r = g = b = 0;
          }

        leds[get_pixel_index(x, y)] = CRGB(r, g, b);
      }

    } else { // between_steps_counter > 0
      // Slow change from state_next to empty state (black)
      fadeToBlackBy(leds, NUM_LEDS, FADE_TO_BLACK_STEP);
    }

    FastLED.show();

    effect_step_counter++;
    effect_start_time = current_time;
  }
}

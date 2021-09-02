#include <FastLED.h>
// #include <Arduino.h>
#include "constants.h"
// #include "checksum.c"
#include "logic.c"


CRGB leds[NUM_LEDS];
byte state[LED_COLS][LED_ROWS];
byte state_next[LED_COLS][LED_ROWS];
byte state_attempt[LED_COLS][LED_ROWS];

uint32_t step_start_time;
int16_t between_steps_counter;

uint32_t effect_start_time;
uint16_t effect_step_counter;

uint32_t checksum_history[HISTORY_LENGTH];
uint16_t checksum_history_length;
int16_t checksum_history_pos;
int16_t repeating_checksum_history_counter;

byte color_red, color_green, color_blue;


uint32_t get_random_seed() {
  // Generates a random seed using 32 values from different analog inputs.
  // A single measurement of the same analog input often gives the same seed value.
  // Uses CRC32 to mix inputs

  uint32_t seed = 0xFFFFFFFF;
  byte a = 0; // Number of the analog input [0-5]
  for (byte k = 0; k < 32; k++) {
    seed ^= analogRead(a);
    for (byte l = 0; l < 8; l++)
        seed = seed & 1 ? (seed >> 1) ^ 0x82f63b78 : seed >> 1;

    if (++a > 5) a = 0;
  }
  return ~seed;
}

inline uint16_t get_pixel_index(byte x, byte y) {
  // Depends on the particuler wiring. See also NUM_LEDS definition
  if (y%2==0)
    return (2*LED_COLS-1)*y + 2*x;
  else
    return (2*LED_COLS-1)*(y+1) - 2*x - 1;
}







void init_nontrivial_state(byte state[LED_COLS][LED_ROWS], byte state_next[LED_COLS][LED_ROWS]) {
  // Generates a random state and makes sure that it will not end up in a few steps
  // A single timestep without history takes ~ 36/100 ms ~ 342/1000 ms ~ 3391/10000 ms
  // A single timestep with history takes ~ 46/100 ms ~ 425/1000 ms ~ 4212/10000 ms
  // Function with 32 attempts and 16 timesteps takes max 320 ms

  for (uint16_t attempt = 0; attempt < 32; attempt++) {
    init_empty_state(state);
    init_random_state(state_attempt);
    copy_state(state_attempt, state_next);
    for (uint16_t t = 0; t < 16; t++) {
      copy_state(state_attempt, state);
      update_state(state, state_attempt);
    }
    if (is_state_nontrivial(state_attempt) && !are_states_identical(state, state_attempt))
      break;
  }
  init_empty_state(state);
  // state_next is already initialized
}

byte game_of_life_rule(const byte pixel_state, const byte neighbours_number) {
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
#endif // BC == PERIODIC_BC


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
  // Takes about 4.7 ms for 8x8 array (not very fast due to % operation at every step)
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
  const uint32_t checksum0 = checksum_history[k];
  uint32_t checksum;

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

void init_color() {
  byte scheme = random(4);
  if (scheme == 0) {
      // Pink
      color_red = 255;
      color_green = random(40);
      color_blue = 114 - color_green;
  } else if (scheme == 1) {
      // Yellow-orange
      color_red = 255 - random(20);
      color_blue = 0;
      color_green = 100 + random(100);
  } else if (scheme == 2) {
      // Green
      color_green = 255 - random(32);
      color_blue = random(32);
      color_red = random(32);
  } else if (scheme == 3) {
      // Blue
      color_blue = 205 - random(32);
      color_green = random(64);
      color_red = 64 + random(80) - color_green;
  }
}

void setup() {
  Serial.begin(9600);

  FastLED.addLeds<WS2811, LED_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  if (CURRENT_LIMIT > 0)
    FastLED.setMaxPowerInVoltsAndMilliamps(5, CURRENT_LIMIT);
  FastLED.setBrightness(BRIGHTNESS);
  fill_solid(leds, NUM_LEDS, CRGB(0, 0, 0));
  FastLED.show();

  randomSeed(get_random_seed());

  // init_empty_state(state);
  // init_random_state(state_next);
  init_nontrivial_state(state, state_next);

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
    copy_state(state_next, state);        // Takes about 0.04 ms
    update_state(state, state_next);      // Takes about 0.3 ms
    add_to_checksum_history(state_next);  // Simplified CRC32 algorithm takes about 0.088 ms

    if (between_steps_counter == -1 && is_checksum_history_repeating(state_next))
      between_steps_counter = 0;

    if (between_steps_counter >= 3) {
      randomSeed(get_random_seed());

      init_nontrivial_state(state, state_next);

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

    // TODO: Move effect to the separate function
    byte s, s_next;
    int16_t r, g, b;

    if (between_steps_counter == -1) {
      // Change from state to state_next
      for (byte x = 0; x < LED_COLS; x++)
        for (byte y = 0; y < LED_ROWS; y++) {
          s = state[x][y];
          s_next = state_next[x][y];

          if (s==0 && s_next==1) {
            // Pixel turns on
            if (effect_step_counter <= FADE_TURN_ON_LENGTH + FADE_TURN_ON_FLICKER_LENGTH) {
              r = effect_step_counter * color_red / FADE_TURN_ON_LENGTH;
              g = effect_step_counter * color_green / FADE_TURN_ON_LENGTH;
              b = effect_step_counter * color_blue / FADE_TURN_ON_LENGTH;
            } else {
              r = color_red;
              g = color_green;
              b = color_blue;
            }

          } else if (s==1 && s_next==0) {
            // Pixel turns off
            if (effect_step_counter <= FADE_TURN_OFF_LENGTH) {
              r = (FADE_TURN_OFF_LENGTH-effect_step_counter) * color_red / FADE_TURN_OFF_LENGTH;
              g = (FADE_TURN_OFF_LENGTH-effect_step_counter) * color_green / FADE_TURN_OFF_LENGTH;
              b = (FADE_TURN_OFF_LENGTH-effect_step_counter) * color_blue / FADE_TURN_OFF_LENGTH;
            } else {
              r = g = b = 0;
            }

          } else if (s==1 && s_next==1) {
            // Pixel remains on
            r = color_red;
            g = color_green;
            b = color_blue;

          } else if (s==0 && s_next==0) {
            // Pixel remains off
            r = g = b = 0;
          }

        leds[get_pixel_index(x, y)] = CRGB(r, g, b);
      }

    } else { // between_steps_counter > 0
      // Slow change from state_next to empty state (black)
      // TODO: For this particular application fadeToBlackBy() sucks. Replace it by the manual one
      fadeToBlackBy(leds, NUM_LEDS, FADE_TO_BLACK_STEP);
    }

    FastLED.show();

    effect_step_counter++;
    effect_start_time = current_time;
  }
}

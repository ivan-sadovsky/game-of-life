#define LED_PIN 13

#define CURRENT_LIMIT 2000    // current limit [mA]; 0 - no limit
#define BRIGHTNESS 250        // [0-255]

#define STEP_PERIOD_MSEC 500  // [msec]
#define FADE_STEP 5

#define OPEN_BC 0
#define PERIODIC_BC 1
#define BC OPEN_BC

#define LED_COLS 8
#define LED_ROWS 8
#define NUM_LEDS (2*LED_COLS-1)*LED_COLS

#define HISTORY_LENGTH 64


#include <FastLED.h>


CRGB leds[NUM_LEDS];
byte state[LED_COLS][LED_ROWS];
byte state_next[LED_COLS][LED_ROWS];

uint32_t step_start_time = 0;

uint32_t checksum_history[HISTORY_LENGTH];
uint16_t checksum_history_length;
int16_t checksum_history_pos;
int16_t repeating_checksum_history_counter;

byte base_color_red=0, 
     base_color_green=0, 
     base_color_blue=0;


uint32_t get_random_seed() {
  // generates a random seed using 32 values 
  // from different analog inputs
  
  uint32_t seed = 0;
  uint32_t s;
  byte a = 0; // number of the analog input [0-5]

  for (byte l = 0; l < 32; l++) {
    s = analogRead(a);
    seed ^= s<<l;
    
    a++;
    if (a>=5)
      a = 0;
  }

  return seed;  
}

inline uint16_t get_pixel_index(byte x, byte y) {
  if (y%2==0)
    return (2*LED_COLS-1)*y + 2*x;
  else
    return (2*LED_COLS-1)*(y+1) - 2*x - 1;
}

void copy_next_state_to_state() {
  for (byte x = 0; x < LED_COLS; x++)
    for (byte y = 0; y < LED_ROWS; y++)
      state[x][y] = state_next[x][y];
}

void swap_next_state_and_state() {
  // TODO: swap pointers instead
  byte t;
  for (byte x = 0; x < LED_COLS; x++)
    for (byte y = 0; y < LED_ROWS; y++) {
      t = state[x][y];
      state[x][y] = state_next[x][y];
      state_next[x][y] = t;
    }
}

void init_state() {
  for (byte x = 0; x < LED_COLS; x++) 
    for (byte y = 0; y < LED_ROWS; y++)
      state[x][y] = 0;
  
  byte num = random(3*LED_COLS*LED_ROWS/16, LED_COLS*LED_ROWS/2);
  for (byte k = 0; k < num; k++) {
    state[random(0, LED_COLS)][random(0, LED_ROWS)] = 1;
  }
}

bool is_empty_state() {
  for (byte x = 0; x < LED_COLS; x++) 
    for (byte y = 0; y < LED_ROWS; y++)
      if (state[x][y]!=0)
        return false;
  return true;
}

#if BC == OPEN_BC
void update_state() { // open BC
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
      
      if (state[x][y]==0)
        state_next[x][y] = (nn==3) ? 1 : 0;
      else
        state_next[x][y] = ((nn==2) || (nn==3)) ? 1 : 0;
    }
  }
  
  // copy_next_state_to_state();
  swap_next_state_and_state();
}

// uint32_t get_state_checksum() {
//   uint32_t sum = 1;
//   byte s;
//   for (byte x = 0; x < LED_COLS; x++)
//     for (byte y = 0; y < LED_ROWS; y++) {
//       s = state[x][y];
//       sum += (x*x + 11*x + 7*y*y + 13*y) * s*s 
//            + (41*x*x + 71*x + 43*y*y + 73*y + 1) * s;
//     }
//   return sum;
// }

// uint32_t get_random_seed() {
//   uint32_t seed = 0;
//   uint32_t s;
//   byte a = 0;

//   for (byte l = 0; l < 32; l++) {
//     s = analogRead(a);
//     seed ^= s<<l;
    
//     if (a>=6)
//       a = 0;
//   }

//   return seed;
// }


// #define CRC32C_POLY 0x82f63b78

/* CRC-32 (Ethernet, ZIP, etc.) polynomial in reversed bit order. */
/* #define POLY 0xedb88320 */

// uint32_t crc32c(uint32_t crc, const unsigned char *buf, size_t len)
// {
//     int k;

//     crc = ~crc;
//     while (len--) {
//         crc ^= *buf++;
//         for (k = 0; k < 8; k++)
//             crc = crc & 1 ? (crc >> 1) ^ CRC32C_POLY : crc >> 1;
//     }
//     return ~crc;
// }

uint32_t get_state_checksum() { // open BC
  // standard crc32 algorithm
  uint32_t crc = 0xFFFFFFFF;
  for (byte x = 0; x < LED_COLS; x++)
    for (byte y = 0; y < LED_ROWS; y++) {
      crc ^= state[x][y];
      for (byte l = 0; l < 8; l++)
          crc = crc & 1 ? (crc >> 1) ^ 0x82f63b78 : crc >> 1;
    }
  return ~crc;
}
#endif

#if BC == PERIODIC_BC
void update_state() { // periodic BC
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
      
      if (state[x][y]==0)
        state_next[x][y] = (nn==3) ? 1 : 0;
      else
        state_next[x][y] = ((nn==2) || (nn==3)) ? 1 : 0;
    }
  }
  
  // copy_next_state_to_state();
  swap_next_state_and_state
}

uint32_t get_state_checksum() { // periodic BC
  // TODO: to be implemented
}
#endif

void init_checksum_history() {
  for (uint16_t k = 0; k < HISTORY_LENGTH; k++) 
    checksum_history[k] = 0;
  checksum_history_length = 0;
  checksum_history_pos = -1;
  repeating_checksum_history_counter = 0;
}

void add_to_checksum_history() {
  checksum_history_pos++;
  if (checksum_history_pos >= HISTORY_LENGTH)
    checksum_history_pos = 0;
      
  checksum_history[checksum_history_pos] = get_state_checksum();

  if (checksum_history_length<HISTORY_LENGTH)
    checksum_history_length++;
}

bool is_checksum_history_repeating() {
  int16_t period = -1;
  uint16_t k = checksum_history_pos;
  uint32_t checksum0 = checksum_history[k], checksum;

// Serial.print("\n\n");
// Serial.print(k); Serial.print("  ");
// Serial.print(checksum0); Serial.print("  ");

  for (uint16_t l = 1; l < checksum_history_length; l++) {
    if (k>0)
      k--;
    else
      k = HISTORY_LENGTH-1;
// Serial.print(k); Serial.print("  ");

    checksum = checksum_history[k];
    // Serial.print(checksum); Serial.print("  ");
// Serial.print(checksum); Serial.print("  "); Serial.print(checksum0); Serial.print("\n");
    if (checksum == checksum0) {
      period = l;
      break;
    }
  }

// Serial.print(checksum0); Serial.print("  "); Serial.print(period); Serial.print("\n");
  if (period > 0) {
    repeating_checksum_history_counter++;

    int16_t time_to_die;
    if (period==1) {
      if (is_empty_state())
        time_to_die = 4;
      else
        time_to_die = 10;
    } else
      time_to_die = 16 * period;

    if (repeating_checksum_history_counter > time_to_die)
      return true;
  }

  return false;
}


void init_color(int16_t intensity = 120, int16_t dispersion = 40) {
  int16_t r, g, b, rgb;
  byte scheme = random(1);
  if (scheme==0) {
    r = random(intensity-dispersion, intensity+dispersion);
    g = random(intensity-dispersion, intensity+dispersion);
    b = 3*intensity - r - g;
  }




//   for (byte l = 0; l < 64; l++) {
//     r = random(96, 160);
//     g = random(96, 160);
//     b = random(96, 160);
//     rgb = r + g + b;
//     // if rgb

//     if (
//       abs(r - base_color_red  ) > 70 ||
//       abs(g - base_color_green) > 70 ||
//       abs(b - base_color_blue ) > 70
//     )
//       break;


// Serial.print("ge0 ");
// Serial.print(intensity * r);  Serial.print("   ");
// Serial.print(intensity * g);  Serial.print("   ");
// Serial.print(intensity * b);  Serial.print("   ");
// Serial.print(rgb);  Serial.print("   ");
// Serial.print("\n");
//     r = 3 * intensity * r / rgb;
//     g = 3 * intensity * g / rgb;
//     b = 3 * intensity * b / rgb;
// Serial.print("gen ");
// Serial.print(r);  Serial.print("   ");
// Serial.print(g);  Serial.print("   ");
// Serial.print(b);  Serial.print("   ");
// Serial.print("\n");

// // Serial.print(abs(r - base_color_red  ));  Serial.print("   ");
// // Serial.print(abs(g - base_color_green));  Serial.print("   ");
// // Serial.print(abs(b - base_color_blue ));  Serial.print("   ");
// // Serial.print("\n");
//     if (
//       abs(r - base_color_red  ) > 70 ||
//       abs(g - base_color_green) > 70 ||
//       abs(b - base_color_blue ) > 70
//     )
//       break;
//   }

  base_color_red   = r;
  base_color_green = g;
  base_color_blue  = b;

// Serial.print("    ");
// Serial.print(base_color_red);  Serial.print("   ");
// Serial.print(base_color_green);  Serial.print("   ");
// Serial.print(base_color_blue);  Serial.print("   ");
// Serial.print("\n\n");
}

// uint32_t get_pixel_color(uint16_t i) {
//   return (((uint32_t)leds[i].r << 16) | ((long)leds[i].g << 8 ) | (long)leds[i].b);
// }

// void fade_to_black() {
//   uint16_t i;
//   for (byte x = 0; x < LED_COLS; x++)
//     for (byte y = 0; y < LED_ROWS; y++) {
//       i = get_pixel_index(x, y);
//       if ((uint32_t)get_pixel_color(i) == 0)
//         continue;
//       leds[i].fadeToBlackBy(FADE_STEP);
//     }
// }

// void fade() {

// }

// // This function modifies 'cur' in place.
// CRGB fadeTowardColor( CRGB& cur, const CRGB& target, uint8_t amount)
// {
//   nblendU8TowardU8( cur.red,   target.red,   amount);
//   nblendU8TowardU8( cur.green, target.green, amount);
//   nblendU8TowardU8( cur.blue,  target.blue,  amount);
//   return cur;
// }

// // Helper function that blends one uint8_t toward another by a given amount
// void nblendU8TowardU8( uint8_t& cur, const uint8_t target, uint8_t amount)
// {
//   if( cur == target) return;
  
//   if( cur < target ) {
//     uint8_t delta = target - cur;
//     delta = scale8_video( delta, amount);
//     cur += delta;
//   } else {
//     uint8_t delta = cur - target;
//     delta = scale8_video( delta, amount);
//     cur -= delta;
//   }
// }

void setup() {
  Serial.begin(9600);

  FastLED.addLeds<WS2811, LED_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  if (CURRENT_LIMIT > 0)
    FastLED.setMaxPowerInVoltsAndMilliamps(5, CURRENT_LIMIT);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.show();

  randomSeed(get_random_seed());
  init_state();
  init_checksum_history();
  init_color();

  step_start_time = millis();
}

void loop() {
  if (millis() - step_start_time >= STEP_PERIOD_MSEC) {
          
    update_state();
    add_to_checksum_history();

    if (is_checksum_history_repeating()) {
      randomSeed(get_random_seed());
      init_state();
      init_checksum_history();
      init_color();
    }
    
    for (byte x = 0; x < LED_COLS; x++)
      for (byte y = 0; y < LED_ROWS; y++) {
        if (state[x][y] == 1)
          leds[get_pixel_index(x, y)] = CRGB(base_color_red, base_color_green, base_color_blue);
        else
          leds[get_pixel_index(x, y)] = CRGB(0, 0, 0);
      }
    
    step_start_time = millis();
    
    FastLED.show();
  }

}


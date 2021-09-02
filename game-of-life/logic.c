#ifndef LOGIC_C
#define LOGIC_C

// #include <Arduino.h>
#include "constants.h"
#include <WMath.cpp>

// typedef unsigned char byte;
// const bool false = 0;
// const bool true = 1;


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

void pseudo_symmetrize_state_in_x(byte state[LED_COLS][LED_ROWS]) {
  for (byte x = 0; x < LED_COLS/2; x++)
    for (byte y = 0; y < LED_ROWS; y++)
      state[LED_COLS-x-1][y] = state[x][y];
}

void pseudo_symmetrize_state_in_y(byte state[LED_COLS][LED_ROWS]) {
  for (byte x = 0; x < LED_COLS; x++)
    for (byte y = 0; y < LED_ROWS/2; y++)
      state[x][LED_ROWS-y-1] = state[x][y];
}

void pseudo_symmetrize_state_in_diag1(byte state[LED_COLS][LED_ROWS]) {
  for (byte x = 0; x < LED_COLS; x++)
    for (byte y = 0; y < x; y++)
      state[x][y] = state[y][x];
}

void pseudo_symmetrize_state_in_diag2(byte state[LED_COLS][LED_ROWS]) {
  for (byte x = 0; x < LED_COLS; x++)
    for (byte y = x; y < LED_ROWS; y++)
      state[x][y] = state[y][x];
}

bool are_states_identical(byte state1[LED_COLS][LED_ROWS], byte state2[LED_COLS][LED_ROWS]) {
  for (byte x = 0; x < LED_COLS; x++)
    for (byte y = 0; y < LED_ROWS; y++)
      if  (state1[x][y] != state2[x][y])
        return false;
  return true;
}

bool is_empty_state(byte state[LED_COLS][LED_ROWS]) {
  for (byte x = 0; x < LED_COLS; x++)
    for (byte y = 0; y < LED_ROWS; y++)
      if (state[x][y]!=0)
        return false;
  return true;
}

uint16_t count_nonzero_pixels(byte state[LED_COLS][LED_ROWS]) {
  uint16_t nnz = 0;
  for (byte x = 0; x < LED_COLS; x++)
    for (byte y = 0; y < LED_ROWS; y++)
      nnz += state[x][y] ? 1 : 0;
  return nnz;
}

bool is_state_nontrivial(byte state[LED_COLS][LED_ROWS]) {
  // Nontrivial state is a state with more than 4 pixels :)
  uint16_t nnz = 0;
  for (byte x = 0; x < LED_COLS; x++)
    for (byte y = 0; y < LED_ROWS; y++) {
      nnz += state[x][y] ? 1 : 0;
      if (nnz > 4)
        return true;
    }
  return false;
}

void init_empty_state(byte state[LED_COLS][LED_ROWS]) {
  // Sets all pixels to zero
  for (byte x = 0; x < LED_COLS; x++)
    for (byte y = 0; y < LED_ROWS; y++)
      state[x][y] = 0;
}

void init_random_state(byte state[LED_COLS][LED_ROWS]) {
  // Generates a random state
  // Sometimes symmetrize it in x, y, xy, diag1, or diag2
  init_empty_state(state);

  byte num = random(3*LED_COLS*LED_ROWS/16, LED_COLS*LED_ROWS/2);
  for (byte k = 0; k < num; k++) {
    state[random(0, LED_COLS)][random(0, LED_ROWS)] = 1;
  }

  uint16_t symm_r = random(64);
  if (symm_r == 0) {
    pseudo_symmetrize_state_in_x(state);
  } else if (symm_r == 1) {
    pseudo_symmetrize_state_in_y(state);
  } else if (symm_r == 2 || symm_r == 3) {
    pseudo_symmetrize_state_in_x(state);
    pseudo_symmetrize_state_in_y(state);
  }
#if LED_COLS == LED_ROWS
  else if (symm_r == 4) {
    pseudo_symmetrize_state_in_diag1(state);
  } else if (symm_r == 5) {
    pseudo_symmetrize_state_in_diag2(state);
  }
#endif
}

#endif // LOGIC_C

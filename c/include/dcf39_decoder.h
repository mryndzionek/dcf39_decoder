#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  int16_t xip;
  int16_t xqp;
  int16_t ya;
  int32_t int1;
  int32_t int2;
  int32_t int3;
  int32_t int4;
  int32_t del0;
  int32_t del1;
  int32_t del2;
  int32_t del3;
  uint16_t decim_idx;
} dcf39_demod_t;

void dcf39_demod_init(dcf39_demod_t* self);
bool dcf39_demod_push(dcf39_demod_t* self, int16_t xi, int16_t xq, bool* bit);

typedef struct {
  uint8_t state;
  uint16_t bits;
  uint16_t idx;
  uint16_t bc;
  uint16_t bit_idx;
  uint16_t byte_count;
  uint8_t data[32];
  uint16_t data_len;
} dcf39_decoder_t;

void dcf39_decoder_init(dcf39_decoder_t* self);
bool dcf39_decoder_push(dcf39_decoder_t* self, bool bit);

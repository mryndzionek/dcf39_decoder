#include "dcf39_decoder.h"

#include <math.h>
#include <stdio.h>

// #define DEBUG_PRINT
#define DCF39_F_SHIFT_HZ (370)
#define SAMPLES_PER_SYMBOL (15)

void dcf39_demod_init(dcf39_demod_t* self) {
  self->xip = 0;
  self->xqp = 0;
  self->ya = 0;
  self->int1 = 0;
  self->int2 = 0;
  self->int3 = 0;
  self->int4 = 0;
  self->del0 = 0;
  self->del1 = 0;
  self->del2 = 0;
  self->del3 = 0;
  self->decim_idx = 0;
}

bool dcf39_demod_push(dcf39_demod_t* self, int16_t xi, int16_t xq, bool* bit) {
  const int32_t i = xi * self->xip + xq * self->xqp;
  const int32_t q = -xi * self->xqp + xq * self->xip;
  const int16_t fm = atan2(q, i) * 2411;
  bool ret = false;

  self->ya += fm - (self->ya / 16);

  self->int1 += self->ya / 16;
  self->int2 += self->int1;
  self->int3 += self->int2;
  self->int4 += self->int3;

  self->decim_idx++;
  if (self->decim_idx == 5) {
    self->decim_idx = 0;
    const int32_t comb1 = self->int4 - self->del0;
    const int32_t comb2 = comb1 - self->del1;
    const int32_t comb3 = comb2 - self->del2;
    const int32_t comb4 = comb3 - self->del3;
    self->del0 = self->int4;
    self->del1 = comb1;
    self->del2 = comb2;
    self->del3 = comb3;
    // printf("%d\n", comb4 >> 10);
    *bit = (comb4 >> 10) < 625;
    ret = true;
  }

  self->xip = xi;
  self->xqp = xq;
  return ret;
}

static void reset(dcf39_decoder_t* self) {
  self->state = 0;
  for (uint16_t i = 0; i < 32; i++) {
    self->data[i] = 0;
  }
  self->byte_count = 0;
}

static bool decode_telegram(dcf39_decoder_t* self, uint8_t byte) {
  if (self->byte_count == 1) {
    if (byte != 0x68) {
#ifdef DEBUG_PRINT
      printf("Wrong start character (expected 0x68, got 0x%02X)\n", byte);
#endif
      reset(self);
      return false;
    }
  } else if (self->byte_count == 3) {
    if (self->data[1] != self->data[2]) {
#ifdef DEBUG_PRINT
      printf(
          "Wrong repeated length field (0x%02X != "
          "0x%02X)\n",
          self->data[1], self->data[2]);
#endif
      reset(self);
      return false;
    }
    self->data_len = self->data[1];
  } else if (self->byte_count == 4) {
    if (byte != 0x68) {
#ifdef DEBUG_PRINT
      printf("Wrong repeated start character (expected 0x68, got 0x%02X)\n",
             byte);
#endif
      reset(self);
      return false;
    }
  } else if (self->byte_count == 5 + self->data_len) {
    const uint8_t crc_exp = byte;
    uint16_t crc = 0;
    for (uint16_t i = 4; i < self->byte_count - 1; i++) {
      crc += self->data[i];
    }
    if (crc_exp != (crc & 0xFF)) {
#ifdef DEBUG_PRINT
      printf("Wrong CRC (expected 0x%02X, got 0x%02X)\n", crc_exp, crc);
#endif
      reset(self);
      return false;
    }
  } else if (self->byte_count == 6 + self->data_len) {
    if (byte != 0x16) {
#ifdef DEBUG_PRINT
      printf("Wrong stop character (expected 0x16, got 0x%02X)\n", byte);
#endif
      reset(self);
      return false;
    } else {
      return true;
    }
  }
  return false;
}

static bool decode_byte(dcf39_decoder_t* self, bool bit, uint8_t* byte) {
  self->bits |= bit << self->bit_idx;
  self->bit_idx += 1;
  if (self->idx == 10 * SAMPLES_PER_SYMBOL) {
    self->bit_idx = 0;
    const uint8_t parity = self->bits >> 8;
    if ((parity & 0b10) == 0) {
#ifdef DEBUG_PRINT
      printf("Wrong stop bit\n");
#endif
      reset(self);
      return false;
    }
    const uint8_t b = self->bits & ~(0b11 << 8);
    uint8_t bc = __builtin_popcount(b);
    if (parity & 0b1) {
      bc += 1;
    }
    if (bc % 2 != 0) {
#ifdef DEBUG_PRINT
      printf("Wrong parity\n");
#endif
      reset(self);
      return false;
    }

    if (self->byte_count >= 32) {
#ifdef DEBUG_PRINT
      printf("Too many bytes\n");
#endif
      reset(self);
      return false;
    }

    self->data[self->byte_count] = b;
    self->state = 0;
    self->byte_count += 1;
    *byte = b;
    return true;
  }
  return false;
}

void dcf39_decoder_init(dcf39_decoder_t* self) {
  self->state = 0;
  self->idx = 0;
  self->bits = 0;
  self->bc = 0;
  self->bit_idx = 0;
  self->byte_count = 0;
  self->data_len = 0;
  for (uint16_t i = 0; i < 32; i++) {
    self->data[i] = 0;
  }
}

bool dcf39_decoder_push(dcf39_decoder_t* self, bool bit) {
  bool ret = false;
  self->idx++;
  if ((self->state == 0) && (bit == 0)) {
    self->state = 1;
    self->idx = 1;
    self->bits = 0;
    self->bc = 0;
  }

  switch (self->state) {
    case 1:
      self->bc += bit;
      if (self->idx == SAMPLES_PER_SYMBOL) {
        if (self->bc < (SAMPLES_PER_SYMBOL / 2)) {
          self->state = 2;
          self->idx = 0;
          self->bc = 0;
        } else {
          self->state = 0;
        }
      }
      break;

    case 2:
      self->bc += bit;
      if ((self->idx % SAMPLES_PER_SYMBOL) == 0) {
        uint8_t byte;
        bool r = decode_byte(self, self->bc > (SAMPLES_PER_SYMBOL / 2), &byte);
        self->bc = 0;
        if (r) {
          r = decode_telegram(self, byte);
          if (r) {
            ret = r;
          }
        }
      }
      break;

    default:
      break;
  }
  return ret;
}

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "dcf39_decoder.h"

#define BUFF_SIZE (1024)

static const float bi[] = {0.06733479193339252, 0.0, -0.735203246, 0.0, 1.0};
static const float bq[] = {0.0, 0.3120175276959214, 0.0, -1.231536616, 0.0,
                           1.0};

static void hilbert(int16_t x, int16_t* xi, int16_t* xq) {
  static int16_t xp[5];
  static float yip[5];
  static float yqp[5];

  float yif = bi[0] * x;
  float yqf = bq[0] * x;

  for (uint16_t i = 0; i < 4; i++) {
    yif += bi[i + 1] * xp[i] - bi[3 - i] * yip[i];
  }

  for (uint16_t i = 0; i < 5; i++) {
    yqf += bq[i + 1] * xp[i] - bq[4 - i] * yqp[i];
  }

  for (int16_t i = 4; i > 0; i--) {
    xp[i] = xp[i - 1];
    yqp[i] = yqp[i - 1];
    yip[i] = yip[i - 1];
  }

  xp[0] = x;
  yip[0] = yif;
  yqp[0] = yqf;

  *xi = yif;
  *xq = yqf;
}

int main(int argc, char* argv[]) {
  int16_t buf[BUFF_SIZE];
  int16_t xi;
  int16_t xq;
  dcf39_demod_t dcf39;
  dcf39_decoder_t dcf39dec;
  bool bit;

  dcf39_demod_init(&dcf39);
  dcf39_decoder_init(&dcf39dec);
  setvbuf(stdout, NULL, _IONBF, 0);

  while (fread(buf, sizeof(int16_t), BUFF_SIZE, stdin) > 0) {
    for (uint16_t i = 0; i < BUFF_SIZE; i++) {
      hilbert(buf[i], &xi, &xq);
      bool ret = dcf39_demod_push(&dcf39, xi, xq, &bit);
      if (ret) {
        ret = dcf39_decoder_push(&dcf39dec, bit);
        if (ret) {
          printf("Data: ");
          for (uint16_t i = 0; i < dcf39dec.byte_count; i++) {
            printf("%02X, ", dcf39dec.data[i]);
          }
          printf(" a1 = %d, a2 = %d, bc = %d\n", dcf39dec.data[5],
                 dcf39dec.data[6], dcf39dec.byte_count);
          if ((dcf39dec.data[5] == 0) && (dcf39dec.data[6] == 0) &&
              (dcf39dec.byte_count == 16)) {
            uint8_t* data = &dcf39dec.data[7];
            const char* const days[] = {"Sun", "Mon", "Tue", "Wed",
                                        "Thu", "Fri", "Sat"};
            printf("Date and Time telegram: ");
            printf("%d:%d:%d %s %s %d.%d.%d\n", data[3] & ~((1 << 7)), data[2],
                   data[1] >> 2, (data[3] & (1 << 7)) != 0 ? "dst" : "",
                   days[data[4] >> 5], data[4] & ~(0b111 << 5), data[5],
                   2000 + data[6]);
          }
          dcf39dec.byte_count = 0;
          dcf39dec.data_len = 0;
        }
      }
    }
  }
}

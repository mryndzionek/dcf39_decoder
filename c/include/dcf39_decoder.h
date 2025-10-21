#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct _dcf39_demod_t dcf39_demod_t;
typedef struct _dcf39_decoder_t dcf39_decoder_t;

dcf39_demod_t* dcf39_demod_create(void);
bool dcf39_demod_push(dcf39_demod_t* self, int16_t xi, int16_t xq, bool* bit);

dcf39_decoder_t* dcf39_decoder_create(void);
bool dcf39_decoder_push(dcf39_decoder_t* self, bool bit);
uint8_t* dcf39_decoder_get_data(dcf39_decoder_t* self, uint16_t* bc);
void dcf39_decoder_reset(dcf39_decoder_t* self);

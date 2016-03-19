/*
 * pcm2wav.h
 *
 *  Created on: Oct 3, 2011
 *      Author: lithium
 */

#ifndef PCM2WAV_H_
#define PCM2WAV_H_

#define MEMORYCACHE  (0x8000L)


void SNDsaveWave(char *file, char *buffer, int32_t size, int32_t speed);
void write_i16_le (void *ptr, int16_t val);
void write_i32_le (void *ptr, int32_t val);
int16_t peek_i16_le (const void *ptr);
uint16_t peek_u16_le (const void *ptr);
int32_t peek_i32_le (const void *ptr);

#endif /* PCM2WAV_H_ */

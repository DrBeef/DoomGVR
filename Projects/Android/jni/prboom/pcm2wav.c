/*
 * pcm2wav.c
 *
 *  Created on: Oct 3, 2011
 *      Author: Clark Scheff
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "pcm2wav.h"

static struct RIFFHEAD
{
	char  riff[4];
	int32_t length;
	char 	wave[4];
} headr;
static struct CHUNK
{
	char name[4];
	int32_t size;
} headc;
static struct WAVEFMT/*format*/
{
	char fmt[4];      /* "fmt " */
	int32_t fmtsize;    /*0x10*/
	int16_t tag;        /*format tag. 1=PCM*/
	int16_t channel;    /*1*/
	int32_t smplrate;
	int32_t bytescnd;   /*average bytes per second*/
	int16_t align;      /*block alignment, in bytes*/
	int16_t nbits;      /*specific to PCM format*/
}headf;
static struct WAVEDATA /*data*/
{
	char data[4];    /* "data" */
	int32_t datasize;
}headw;

void SNDsaveWave(char *file, char *buffer, int32_t size, int32_t speed)
{
  FILE *fp;
  int32_t wsize,sz=0;
  fp = fopen(file, "wb");
  if(fp == NULL)
  {
	  printf("Can't open %s for writing.", file);
	  return;
  }
  /*header*/
  strncpy(headr.riff,"RIFF",4);
  write_i32_le (&headr.length, 4+sizeof(struct WAVEFMT)+sizeof(struct WAVEDATA)+size);
  strncpy(headr.wave,"WAVE",4);
  fwrite(&headr,sizeof(struct RIFFHEAD),1,fp);
  strncpy(headf.fmt, "fmt ",4);
  write_i32_le (&headf.fmtsize,  sizeof(struct WAVEFMT)-8);
  write_i16_le (&headf.tag,      1);
  write_i16_le (&headf.channel,  1);
  write_i32_le (&headf.smplrate, speed);
  write_i32_le (&headf.bytescnd, speed);
  write_i16_le (&headf.align,    1);
  write_i16_le (&headf.nbits,    8);
  fwrite(&headf,sizeof(struct WAVEFMT),1,fp);
  strncpy(headw.data,"data",4);
  write_i32_le (&headw.datasize, size);
  fwrite(&headw,sizeof(struct WAVEDATA),1,fp);
  for(wsize=0;wsize<size;wsize+=sz)
  {
	  sz= (size-wsize>MEMORYCACHE)? MEMORYCACHE:(size-wsize);
	  if(fwrite((buffer+(wsize)),(size_t)sz,1,fp)!=1)
	  {
		  printf("%s: write error!", file);
		  return;
	  }
  }
  fclose(fp);
}

/*
 *	write_i16_le
 *	Write a little-endian 16-bit signed integer to memory area
 *	pointed to by <ptr>.
 */
void write_i16_le (void *ptr, int16_t val)
{
  ((unsigned char *) ptr)[0] = val;
  ((unsigned char *) ptr)[1] = val >> 8;
}


/*
 *	write_i32_le
 *	Write a little-endian 32-bit signed integer to memory area
 *	pointed to by <ptr>.
 */
void write_i32_le (void *ptr, int32_t val)
{
  ((unsigned char *) ptr)[0] = val;
  ((unsigned char *) ptr)[1] = val >> 8;
  ((unsigned char *) ptr)[2] = val >> 16;
  ((unsigned char *) ptr)[3] = val >> 24;
}

/*
 *	peek_i16_le
 *	Read a little-endian 16-bit signed integer from memory area
 *	pointed to by <ptr>.
 */
int16_t peek_i16_le (const void *ptr)
{
  return ((const unsigned char *) ptr)[0]
      | (((const unsigned char *) ptr)[1] << 8);
}


/*
 *	peek_u16_le
 *	Read a little-endian 16-bit unsigned integer from memory area
 *	pointed to by <ptr>.
 */
uint16_t peek_u16_le (const void *ptr)
{
  return ((const unsigned char *) ptr)[0]
      | (((const unsigned char *) ptr)[1] << 8);
}


/*
 *	peek_i32_le
 *	Read a little-endian 32-bit signed integer from memory area
 *	pointed to by <ptr>.
 */
int32_t peek_i32_le (const void *ptr)
{
  return    ((const unsigned char *) ptr)[0]
   | ((uint16_t) ((const unsigned char *) ptr)[1] << 8)
   | ((int32_t) ((const unsigned char *) ptr)[2] << 16)
   | ((int32_t) ((const unsigned char *) ptr)[3] << 24);
}



#include <cstdio>
#include <cstdlib>

#include "twister.h"

#define FREAD(buf,siz,cnt,fil) if ( fread(buf,siz,cnt,fil) != cnt ) exit(-1)

struct TWISTER
{
	unsigned int x;
	unsigned int y;
	unsigned int z;
};

static unsigned int crc=0;

static TWISTER state[3]=
{
	{
		123456789,
		362436069,
		521288629
	},
	{
		123456789,
		362436069,
		521288629
	},
	{
		123456789,
		362436069,
		521288629
	}
};

void dbg_get_twister_state(unsigned int s[6])
{
	s[0]=state[0].x; s[1]=state[0].y; s[2]=state[0].z;
	s[3]=state[2].x; s[4]=state[2].y; s[5]=state[2].z;
}

static TWISTER* twister = state;

void twister_switch(int i)
{
	twister = state+(i&1);
}

int twister_write(FILE* f)
{
	fwrite(&twister->x,4,1,f);
	fwrite(&twister->y,4,1,f);
	fwrite(&twister->z,4,1,f);
	return 12;
}

int twister_read(FILE* f)
{
	FREAD(&twister->x,4,1,f);
	FREAD(&twister->y,4,1,f);
	FREAD(&twister->z,4,1,f);
	return 12;
}

void twister_seed(unsigned int s)
{
	twister->x = s;
	twister->y = 362436069;
	twister->z = 521288629;

	twister_rand();
	twister_rand();
	twister_rand();
}

void crypt_ini()
{
	twister[2].x = 0x09d4eceb ^ twister->z;
	twister[2].y = 0x7b1f5a0a ^ twister->x;
	twister[2].z = 0x5acb246e ^ twister->y;
	crc = 0xef561513;
}

void crypt_enc(int* i)
{
	unsigned int c = crc;

	// update crc before xor
	crc ^= (crc-*i) << 16;
	crc ^= (crc+*i) >> 5;
	crc ^= crc << 1;

	unsigned int t;

	twister[2].x ^= twister[2].x << 16;
	twister[2].x ^= twister[2].x >> 5;
	twister[2].x ^= twister[2].x << 1;

	t = twister[2].x;
	twister[2].x = twister[2].y;
	twister[2].y = twister[2].z;
	twister[2].z = t ^ twister[2].x ^ twister[2].y;

	*i ^= twister[2].z;
	*i ^= c;
}

void crypt_dec(int* i)
{
	unsigned int t;

	twister[2].x ^= twister[2].x << 16;
	twister[2].x ^= twister[2].x >> 5;
	twister[2].x ^= twister[2].x << 1;

	t = twister[2].x;
	twister[2].x = twister[2].y;
	twister[2].y = twister[2].z;
	twister[2].z = t ^ twister[2].x ^ twister[2].y;

	*i ^= twister[2].z;
	*i ^= crc;

	// update crc after xor
	crc ^= (crc-*i) << 16;
	crc ^= (crc+*i) >> 5;
	crc ^= crc << 1;
}

void crypt_enc(unsigned char* i)
{
	unsigned int c = crc;

	// update crc before xor
	crc ^= (crc-*i) << 16;
	crc ^= (crc+*i) >> 5;
	crc ^= crc << 1;

	unsigned int t;

	twister[2].x ^= twister[2].x << 16;
	twister[2].x ^= twister[2].x >> 5;
	twister[2].x ^= twister[2].x << 1;

	t = twister[2].x;
	twister[2].x = twister[2].y;
	twister[2].y = twister[2].z;
	twister[2].z = t ^ twister[2].x ^ twister[2].y;

	*i ^= twister[2].z;
	*i ^= c;
}

void crypt_dec(unsigned char* i)
{
	unsigned int t;

	twister[2].x ^= twister[2].x << 16;
	twister[2].x ^= twister[2].x >> 5;
	twister[2].x ^= twister[2].x << 1;

	t = twister[2].x;
	twister[2].x = twister[2].y;
	twister[2].y = twister[2].z;
	twister[2].z = t ^ twister[2].x ^ twister[2].y;

	*i ^= twister[2].z;
	*i ^= crc;

	// update crc after xor
	crc ^= (crc-*i) << 16;
	crc ^= (crc+*i) >> 5;
	crc ^= crc << 1;
}

unsigned int crypt_chk()
{
	return crc;
}

int twister_rand()
{
	unsigned int t;

	twister->x ^= twister->x << 16;
	twister->x ^= twister->x >> 5;
	twister->x ^= twister->x << 1;

	t = twister->x;
	twister->x = twister->y;
	twister->y = twister->z;
	twister->z = t ^ twister->x ^ twister->y;

	return twister->z & 0x7fffffff;
}

#if 0
static const unsigned long SIZE   = 624;
static const unsigned long PERIOD = 397;
static const unsigned long DIFF   = SIZE-PERIOD;

static unsigned long MT[SIZE];
static unsigned long index = 0;

#define M32(x) (0x80000000 & x) // 32nd Most Significant Bit
#define L31(x) (0x7FFFFFFF & x) // 31 Least Significant Bits
#define ODD(x) (x & 1) // Check if number is odd

#define UNROLL(expr) y = M32(MT[i]) | L31(MT[i+1]); MT[i] = MT[expr] ^ (y>>1) ^ MATRIX[ODD(y)]; ++i;

void twister_seed(unsigned long s)
{
  MT[0] = s;
  index = 0;

  for ( unsigned i=1; i<SIZE; ++i )
    MT[i] = 0x6c078965*(MT[i-1] ^ MT[i-1]>>30) + i;
}

int twister_rand()
{
  if ( !index )
  {
	  static const unsigned long MATRIX[2] = {0, 0x9908b0df};
	  unsigned long y, i=0;

	  while ( i<(DIFF-1) )
	  {
		UNROLL(i+PERIOD);
		UNROLL(i+PERIOD);
	  }

	  UNROLL((i+PERIOD) % SIZE);

	  while ( i<(SIZE-1) )
	  {
		UNROLL(i-DIFF);
		UNROLL(i-DIFF);
		UNROLL(i-DIFF);
		UNROLL(i-DIFF);
		UNROLL(i-DIFF);
		UNROLL(i-DIFF);
		UNROLL(i-DIFF);
		UNROLL(i-DIFF);
		UNROLL(i-DIFF);
		UNROLL(i-DIFF);
		UNROLL(i-DIFF);
	  }

	  y = M32(MT[SIZE-1]) | L31(MT[0]);
	  MT[SIZE-1] = MT[PERIOD-1] ^ (y>>1) ^ MATRIX[ODD(y)];
  }

  unsigned long y = MT[index];

  y ^= y>>11;
  y ^= y<< 7 & 0x9d2c5680;
  y ^= y<<15 & 0xefc60000;
  y ^= y>>18;

  if ( ++index == SIZE )
    index = 0;

  return (int)(y&~(1<<31));
}
#endif

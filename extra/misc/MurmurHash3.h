//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

#ifndef _MURMURHASH3_H_
#define _MURMURHASH3_H_

//-----------------------------------------------------------------------------
// Platform-specific functions and macros

// Microsoft Visual Studio

#if defined(_MSC_VER)

typedef unsigned char uint8_t;
typedef unsigned long uint32_t;
typedef unsigned __int64 uint64_t;

// Other compilers

#else	// defined(_MSC_VER)

#include <stdint.h>

#endif // !defined(_MSC_VER)

//-----------------------------------------------------------------------------

void MurmurHash3_x86_32  ( const void * key, int len, uint32_t seed, void * out );

void MurmurHash3_x86_128 ( const void * key, int len, uint32_t seed, void * out );

void MurmurHash3_x64_128 ( const void * key, int len, uint32_t seed, void * out );

//-----------------------------------------------------------------------------

struct MurmurHash3_x64_128_State {
	uint64_t h1;
	uint64_t h2;
	uint8_t remainder[16];
	int remainder_len;
	int total_len;
};

void MurmurHash3_x64_128_init(uint32_t seed, MurmurHash3_x64_128_State *state);
void MurmurHash3_x64_128_update(const void *key,
				int len,
				MurmurHash3_x64_128_State *state);
void MurmurHash3_x64_128_finalize(MurmurHash3_x64_128_State *state, void *out);

#endif // _MURMURHASH3_H_

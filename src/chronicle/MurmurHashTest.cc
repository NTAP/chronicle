// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

#include "gtest/gtest.h"
#include "MurmurHash3.h"

static const char *data1 = "This is a test buffer";
static const uint64_t data1_r0 = 7864104701578874665llu;
static const uint64_t data1_r1 = 13820044691288142984llu;

static const int bigbuffersize = 4096;
static char bigbuffer[bigbuffersize];
static const uint64_t bigbuffer_r0 = 6660710945702146080llu;
static const uint64_t bigbuffer_r1 = 17051495021520436491llu;

static void
initBigBuffer()
{
    for (int i = 0; i < bigbuffersize; ++i) {
        bigbuffer[i] = i;
    }
}

TEST(MurmurHash3, OriginalGeneratesCorrectValue) {
    uint64_t hash_result[2];
    MurmurHash3_x64_128(data1, strlen(data1), 1, hash_result);
    ASSERT_EQ(data1_r0, hash_result[0]);
    ASSERT_EQ(data1_r1, hash_result[1]);

    initBigBuffer();
    MurmurHash3_x64_128(bigbuffer, bigbuffersize, 1, hash_result);
    ASSERT_EQ(bigbuffer_r0, hash_result[0]);
    ASSERT_EQ(bigbuffer_r1, hash_result[1]);
}

TEST(MurmurHash3, SingleBufferGeneratesCorrectValue) {
    uint64_t hash_result[2];
    MurmurHash3_x64_128_State state;
    MurmurHash3_x64_128_init(1, &state);
    MurmurHash3_x64_128_update(data1, strlen(data1), &state);
    MurmurHash3_x64_128_finalize(&state, hash_result);
    ASSERT_EQ(data1_r0, hash_result[0]);
    ASSERT_EQ(data1_r1, hash_result[1]);

    initBigBuffer();
    MurmurHash3_x64_128_init(1, &state);
    MurmurHash3_x64_128_update(bigbuffer, bigbuffersize, &state);
    MurmurHash3_x64_128_finalize(&state, hash_result);
    ASSERT_EQ(bigbuffer_r0, hash_result[0]);
    ASSERT_EQ(bigbuffer_r1, hash_result[1]);
}

TEST(MurmurHash3, MultiBufferGeneratesCorrectValue) {
    uint64_t hash_result[2];
    MurmurHash3_x64_128_State state;
    MurmurHash3_x64_128_init(1, &state);
    MurmurHash3_x64_128_update(data1, 5, &state);
    MurmurHash3_x64_128_update(&data1[5], 8, &state);
    MurmurHash3_x64_128_update(&data1[13], strlen(data1) - 13, &state);
    MurmurHash3_x64_128_finalize(&state, hash_result);
    ASSERT_EQ(data1_r0, hash_result[0]);
    ASSERT_EQ(data1_r1, hash_result[1]);

    initBigBuffer();
    MurmurHash3_x64_128_init(1, &state);
    
    char *buf = bigbuffer;
    int remaining = bigbuffersize;
    do {
        int bytes = rand() % (remaining + 1);
        MurmurHash3_x64_128_update(buf, bytes, &state);
        remaining -= bytes;
        buf += bytes;
    } while (remaining);
    MurmurHash3_x64_128_finalize(&state, hash_result);
    ASSERT_EQ(bigbuffer_r0, hash_result[0]);
    ASSERT_EQ(bigbuffer_r1, hash_result[1]);
}

// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2014 Netapp, Inc.
 * All rights reserved.
 */

#include "FpeAnonymizer.h"
#include <cassert>
#include <cstring>
#include <endian.h>
#include <fcntl.h>
#include <cmath>
#include <openssl/evp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
//#include <iostream>


static const EVP_MD *MD_ALGORITHM = EVP_sha1();
static const EVP_CIPHER *CIPHER = EVP_aes_128_cbc();
static const unsigned CIPHER_KEYLEN = 16; // AES 16-byte key (128 bits)

FpeAnonymizer::FpeAnonymizer(const std::string &keyfile,
                             const std::string &tweak):
    _cache32(200), _cache64(200)
{
    _tweaklen = tweak.size();
    _tweak = new unsigned char[_tweaklen];
    memcpy(_tweak, tweak.data(), _tweaklen);


    char buf[4096];

    EVP_MD_CTX *ctx = EVP_MD_CTX_create();
    assert(EVP_DigestInit_ex(ctx, MD_ALGORITHM, 0));
    
    int fd = open(keyfile.c_str(), O_RDONLY);
    if (0 > fd) {
        perror(keyfile.c_str());
        abort();
    }
    ssize_t bytes = -1;
    while ((bytes = read(fd, buf, sizeof(buf))) > 0) {
        int rc = EVP_DigestUpdate(ctx, buf, bytes);
        assert(1 == rc);
    }
    assert(bytes >= 0);
    close(fd);

    unsigned keylen = std::max<unsigned>(EVP_MAX_MD_SIZE, CIPHER_KEYLEN);
    _key = new unsigned char[keylen];
    memset(_key, 0, keylen);
    int rc = EVP_DigestFinal_ex(ctx, _key, 0);
    assert(1 == rc);

    EVP_MD_CTX_destroy(ctx);
}

FpeAnonymizer::~FpeAnonymizer()
{
    delete _key;
    delete _tweak;
}

int32_t
FpeAnonymizer::anonymize(int32_t inVal)
{
    int32_t *cval = _cache32.lookup(inVal);
    if (cval)
        return *cval;

    uint64_t u64 = static_cast<uint64_t>(inVal);
    ff1Encrypt(&u64, 4);
    assert(u64 < 1ull<<32);
    int32_t outVal = static_cast<int32_t>(u64);

    _cache32.insert(inVal, outVal);

    return outVal;
}

int64_t
FpeAnonymizer::anonymize(int64_t inVal)
{
    int64_t *cval = _cache64.lookup(inVal);
    if (cval)
        return *cval;

    uint64_t u64 = static_cast<uint64_t>(inVal);
    ff1Encrypt(&u64, 8);
    int64_t outVal = static_cast<int64_t>(u64);

    _cache64.insert(inVal, outVal);

    return outVal;
}

void
FpeAnonymizer::ff1Encrypt(uint64_t *val, unsigned bytes)
{
    assert(bytes == 4 || bytes == 8);
    // using radix = 2, so switch to bits
    const unsigned n = bytes * 8;
    const unsigned u = n/2;
    //const unsigned uBytes = u/8;
    const unsigned v = n - u;
    const unsigned vBytes = v/8;
    uint64_t A = (*val >> v) & ((1llu << u) - 1);
    uint64_t B = *val & ((1llu << v) - 1);
    const unsigned d = ceil(vBytes/4.0) * 4 + 4;
    assert(d == 8); // for 8, 32, 64-bit, it should always be 8
    unsigned char P[16];
    P[0] = 1;
    P[1] = 2;
    P[2] = 1;
    P[3] = 0; P[4] = 0; P[5] = 2; // radix
    P[6] = 10;
    P[7] = u;
    P[8] = 0; P[9] = 0; P[10] = 0; P[11] = n;
    uint32_t be_tlen = htobe32(_tweaklen);
    memcpy(&(P[12]), &be_tlen, 4);

    // prepare fixed portion of Q
    const unsigned qlen = int((_tweaklen + vBytes + 1 + 15) / 16) * 16;
    unsigned char Q[qlen];
    memset(Q, 0, qlen);
    memcpy(Q, _tweak, _tweaklen);
    const unsigned ipos = qlen - vBytes - 1;
    const unsigned char IV[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    for (unsigned i = 0; i < 10; ++i) {
        // part of Q that varies by round
        memcpy(&Q[qlen-vBytes], &B, vBytes);
        Q[ipos] = i;
        // R = prf(P||Q)
        EVP_CIPHER_CTX ctx;
        EVP_CIPHER_CTX_init(&ctx);
        int rc = EVP_EncryptInit_ex(&ctx, CIPHER, 0, _key, IV);
        assert(1 == rc);
        // AES block size is 16 bytes
        assert(16 == EVP_CIPHER_CTX_block_size(&ctx));
        // since block size is > d, we only need 1 block
        unsigned char R[16];
        int rlen;
        rc = EVP_EncryptUpdate(&ctx, R, &rlen, P, 16);
        assert(1 == rc);
        for (unsigned j=0; j < qlen; j += 16) {
            rc = EVP_EncryptUpdate(&ctx, R, &rlen, &(Q[j]), 16);
            assert(1 == rc);
        }
        assert(16 == rlen);
        // Since we're encrypting whole blocks, we don't need to call final.
        //rc = EVP_EncryptFinal_ex(&ctx, R, &rlen);
        //assert(1 == rc);
        EVP_CIPHER_CTX_cleanup(&ctx);

        // First d bytes (8) of R (== S) is what we care about
        uint64_t *yp = reinterpret_cast<uint64_t*>(R);
        uint64_t y = be64toh(*yp);
        assert(u == v);
        uint64_t c = (A + y) % (1llu << u);
        A = B;
        B = c;
    }

    *val = (A << v) | B;
}

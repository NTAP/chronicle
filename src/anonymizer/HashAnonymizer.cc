// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

#include "HashAnonymizer.h"
#include <algorithm>
#include <arpa/inet.h>
#include <cassert>
#include <cstring>
#include <endian.h>
#include <fcntl.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


static const EVP_MD *MD_ALGORITHM = EVP_sha1();

HashAnonymizer::HashAnonymizer(const std::string &keyfile)
{
    unsigned char md_value[EVP_MAX_MD_SIZE];
    char buf[4096];
    _keylen = sizeof(md_value);

    EVP_MD_CTX *ctx = EVP_MD_CTX_create();
    assert(EVP_DigestInit_ex(ctx, MD_ALGORITHM, 0));
    
    int fd = open(keyfile.c_str(), O_RDONLY);
    if (0 > fd) {
        perror(keyfile.c_str());
        abort();
    }
    ssize_t bytes = -1;
    while ((bytes = read(fd, buf, sizeof(buf))) > 0) {
        assert(EVP_DigestUpdate(ctx, buf, bytes));
    }
    assert(bytes >= 0);
    close(fd);

    assert(EVP_DigestFinal_ex(ctx, md_value, &_keylen));
    EVP_MD_CTX_destroy(ctx);

    _key = new unsigned char[_keylen];
    memcpy(_key, md_value, _keylen);
    assert(_keylen > 0);
}

HashAnonymizer::~HashAnonymizer()
{
    delete _key;
}

int8_t
HashAnonymizer::anonymize(int8_t inVal)
{
    return static_cast<int8_t>(anonymize(static_cast<uint8_t>(inVal)));
}

uint8_t
HashAnonymizer::anonymize(uint8_t inVal)
{
    const char *data = reinterpret_cast<const char *>(&inVal);
    std::string outData = anonymize(std::string(data, sizeof(inVal)));
    return outData.data()[0];
}

int16_t
HashAnonymizer::anonymize(int16_t inVal)
{
    return static_cast<int16_t>(anonymize(static_cast<uint16_t>(inVal)));
}

uint16_t
HashAnonymizer::anonymize(uint16_t inVal)
{
    uint16_t netInVal = htons(inVal);
    const char *data = reinterpret_cast<const char *>(&netInVal);
    std::string outData = anonymize(std::string(data, sizeof(inVal)));
    assert(outData.size() >= sizeof(inVal));
    return ntohs(*reinterpret_cast<const uint16_t *>(outData.data()));
}

int32_t
HashAnonymizer::anonymize(int32_t inVal)
{
    return static_cast<int32_t>(anonymize(static_cast<uint32_t>(inVal)));
}

uint32_t
HashAnonymizer::anonymize(uint32_t inVal)
{
    uint32_t netInVal = htonl(inVal);
    const char *data = reinterpret_cast<const char *>(&netInVal);
    std::string outData = anonymize(std::string(data, sizeof(inVal)));
    assert(outData.size() >= sizeof(inVal));
    return ntohl(*reinterpret_cast<const uint32_t *>(outData.data()));
}

int64_t
HashAnonymizer::anonymize(int64_t inVal)
{
    return static_cast<int64_t>(anonymize(static_cast<uint64_t>(inVal)));
}

uint64_t
HashAnonymizer::anonymize(uint64_t inVal)
{
    uint64_t netInVal = htobe64(inVal);
    const char *data = reinterpret_cast<const char *>(&netInVal);
    std::string outData = anonymize(std::string(data, sizeof(inVal)));
    assert(outData.size() >= sizeof(inVal));
    return be64toh(*reinterpret_cast<const uint64_t *>(outData.data()));
}

std::string
HashAnonymizer::anonymize(const std::string &inData)
{
    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned md_len = sizeof(md_value);
    assert(HMAC(MD_ALGORITHM, _key, _keylen,
                reinterpret_cast<const unsigned char *>(inData.data()),
                inData.size(), md_value, &md_len));
    std::string outData(reinterpret_cast<const char *>(md_value), md_len);
    return outData;
}

#include "port_rom_hash.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t state[5];
    uint64_t bitcount;
    uint8_t buffer[64];
    uint8_t buflen;
} Sha1Context;

typedef struct {
    uint32_t state[8];
    uint64_t bitcount;
    uint8_t buffer[64];
    uint8_t buflen;
} Sha256Context;

static uint32_t Sha1Rol(uint32_t value, unsigned int bits) {
    return (value << bits) | (value >> (32u - bits));
}

static void Sha1Init(Sha1Context* context) {
    context->state[0] = 0x67452301u;
    context->state[1] = 0xEFCDAB89u;
    context->state[2] = 0x98BADCFEu;
    context->state[3] = 0x10325476u;
    context->state[4] = 0xC3D2E1F0u;
    context->bitcount = 0;
    context->buflen = 0;
}

static void Sha1Transform(Sha1Context* context, const uint8_t block[64]) {
    uint32_t words[80];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;

    for (int i = 0; i < 16; ++i) {
        words[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) |
                   ((uint32_t)block[i * 4 + 2] << 8) | (uint32_t)block[i * 4 + 3];
    }
    for (int i = 16; i < 80; ++i) {
        words[i] = Sha1Rol(words[i - 3] ^ words[i - 8] ^ words[i - 14] ^ words[i - 16], 1);
    }

    a = context->state[0];
    b = context->state[1];
    c = context->state[2];
    d = context->state[3];
    e = context->state[4];
    for (int i = 0; i < 80; ++i) {
        uint32_t f;
        uint32_t k;
        uint32_t next;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999u;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1u;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCu;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6u;
        }
        next = Sha1Rol(a, 5) + f + e + k + words[i];
        e = d;
        d = c;
        c = Sha1Rol(b, 30);
        b = a;
        a = next;
    }
    context->state[0] += a;
    context->state[1] += b;
    context->state[2] += c;
    context->state[3] += d;
    context->state[4] += e;
}

static void Sha1Update(Sha1Context* context, const uint8_t* data, size_t size) {
    context->bitcount += (uint64_t)size * 8u;
    while (size != 0) {
        const size_t available = sizeof(context->buffer) - context->buflen;
        const size_t take = size < available ? size : available;
        memcpy(context->buffer + context->buflen, data, take);
        context->buflen = (uint8_t)(context->buflen + take);
        data += take;
        size -= take;
        if (context->buflen == sizeof(context->buffer)) {
            Sha1Transform(context, context->buffer);
            context->buflen = 0;
        }
    }
}

static void Sha1Final(Sha1Context* context, uint8_t digest[20]) {
    const uint64_t bitcount = context->bitcount;
    uint8_t value = 0x80;
    uint8_t length[8];

    Sha1Update(context, &value, 1);
    value = 0;
    while (context->buflen != 56) {
        Sha1Update(context, &value, 1);
    }
    for (int i = 0; i < 8; ++i) {
        length[i] = (uint8_t)(bitcount >> (56 - i * 8));
    }
    Sha1Update(context, length, sizeof(length));
    for (int i = 0; i < 5; ++i) {
        digest[i * 4] = (uint8_t)(context->state[i] >> 24);
        digest[i * 4 + 1] = (uint8_t)(context->state[i] >> 16);
        digest[i * 4 + 2] = (uint8_t)(context->state[i] >> 8);
        digest[i * 4 + 3] = (uint8_t)context->state[i];
    }
}

static uint32_t Sha256Ror(uint32_t value, unsigned int bits) {
    return (value >> bits) | (value << (32u - bits));
}

static void Sha256Init(Sha256Context* context) {
    context->state[0] = 0x6A09E667u;
    context->state[1] = 0xBB67AE85u;
    context->state[2] = 0x3C6EF372u;
    context->state[3] = 0xA54FF53Au;
    context->state[4] = 0x510E527Fu;
    context->state[5] = 0x9B05688Cu;
    context->state[6] = 0x1F83D9ABu;
    context->state[7] = 0x5BE0CD19u;
    context->bitcount = 0;
    context->buflen = 0;
}

static void Sha256Transform(Sha256Context* context, const uint8_t block[64]) {
    static const uint32_t constants[64] = {
        0x428A2F98u, 0x71374491u, 0xB5C0FBCFu, 0xE9B5DBA5u, 0x3956C25Bu, 0x59F111F1u, 0x923F82A4u,
        0xAB1C5ED5u, 0xD807AA98u, 0x12835B01u, 0x243185BEu, 0x550C7DC3u, 0x72BE5D74u, 0x80DEB1FEu,
        0x9BDC06A7u, 0xC19BF174u, 0xE49B69C1u, 0xEFBE4786u, 0x0FC19DC6u, 0x240CA1CCu, 0x2DE92C6Fu,
        0x4A7484AAu, 0x5CB0A9DCu, 0x76F988DAu, 0x983E5152u, 0xA831C66Du, 0xB00327C8u, 0xBF597FC7u,
        0xC6E00BF3u, 0xD5A79147u, 0x06CA6351u, 0x14292967u, 0x27B70A85u, 0x2E1B2138u, 0x4D2C6DFCu,
        0x53380D13u, 0x650A7354u, 0x766A0ABBu, 0x81C2C92Eu, 0x92722C85u, 0xA2BFE8A1u, 0xA81A664Bu,
        0xC24B8B70u, 0xC76C51A3u, 0xD192E819u, 0xD6990624u, 0xF40E3585u, 0x106AA070u, 0x19A4C116u,
        0x1E376C08u, 0x2748774Cu, 0x34B0BCB5u, 0x391C0CB3u, 0x4ED8AA4Au, 0x5B9CCA4Fu, 0x682E6FF3u,
        0x748F82EEu, 0x78A5636Fu, 0x84C87814u, 0x8CC70208u, 0x90BEFFFAu, 0xA4506CEBu, 0xBEF9A3F7u,
        0xC67178F2u,
    };
    uint32_t words[64];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
    uint32_t h;

    for (int i = 0; i < 16; ++i) {
        words[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) |
                   ((uint32_t)block[i * 4 + 2] << 8) | (uint32_t)block[i * 4 + 3];
    }
    for (int i = 16; i < 64; ++i) {
        const uint32_t s0 = Sha256Ror(words[i - 15], 7) ^ Sha256Ror(words[i - 15], 18) ^ (words[i - 15] >> 3);
        const uint32_t s1 = Sha256Ror(words[i - 2], 17) ^ Sha256Ror(words[i - 2], 19) ^ (words[i - 2] >> 10);
        words[i] = words[i - 16] + s0 + words[i - 7] + s1;
    }

    a = context->state[0];
    b = context->state[1];
    c = context->state[2];
    d = context->state[3];
    e = context->state[4];
    f = context->state[5];
    g = context->state[6];
    h = context->state[7];
    for (int i = 0; i < 64; ++i) {
        const uint32_t s1 = Sha256Ror(e, 6) ^ Sha256Ror(e, 11) ^ Sha256Ror(e, 25);
        const uint32_t choice = (e & f) ^ ((~e) & g);
        const uint32_t temp1 = h + s1 + choice + constants[i] + words[i];
        const uint32_t s0 = Sha256Ror(a, 2) ^ Sha256Ror(a, 13) ^ Sha256Ror(a, 22);
        const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t temp2 = s0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    context->state[0] += a;
    context->state[1] += b;
    context->state[2] += c;
    context->state[3] += d;
    context->state[4] += e;
    context->state[5] += f;
    context->state[6] += g;
    context->state[7] += h;
}

static void Sha256Update(Sha256Context* context, const uint8_t* data, size_t size) {
    context->bitcount += (uint64_t)size * 8u;
    while (size != 0) {
        const size_t available = sizeof(context->buffer) - context->buflen;
        const size_t take = size < available ? size : available;
        memcpy(context->buffer + context->buflen, data, take);
        context->buflen = (uint8_t)(context->buflen + take);
        data += take;
        size -= take;
        if (context->buflen == sizeof(context->buffer)) {
            Sha256Transform(context, context->buffer);
            context->buflen = 0;
        }
    }
}

static void Sha256Final(Sha256Context* context, uint8_t digest[32]) {
    const uint64_t bitcount = context->bitcount;
    uint8_t value = 0x80;
    uint8_t length[8];

    Sha256Update(context, &value, 1);
    value = 0;
    while (context->buflen != 56) {
        Sha256Update(context, &value, 1);
    }
    for (int i = 0; i < 8; ++i) {
        length[i] = (uint8_t)(bitcount >> (56 - i * 8));
    }
    Sha256Update(context, length, sizeof(length));
    for (int i = 0; i < 8; ++i) {
        digest[i * 4] = (uint8_t)(context->state[i] >> 24);
        digest[i * 4 + 1] = (uint8_t)(context->state[i] >> 16);
        digest[i * 4 + 2] = (uint8_t)(context->state[i] >> 8);
        digest[i * 4 + 3] = (uint8_t)context->state[i];
    }
}

static void DigestToHex(const uint8_t* digest, size_t digestSize, char* output) {
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < digestSize; ++i) {
        output[i * 2] = hex[digest[i] >> 4];
        output[i * 2 + 1] = hex[digest[i] & 0x0F];
    }
    output[digestSize * 2] = '\0';
}

static void FinishHashes(Sha1Context* sha1, Sha256Context* sha256, size_t size, PortRomHashes* out) {
    uint8_t sha1Digest[20];
    uint8_t sha256Digest[32];
    Sha1Final(sha1, sha1Digest);
    Sha256Final(sha256, sha256Digest);
    DigestToHex(sha1Digest, sizeof(sha1Digest), out->sha1);
    DigestToHex(sha256Digest, sizeof(sha256Digest), out->sha256);
    out->size = size;
}

void Port_HashBuffer(const void* data, size_t size, PortRomHashes* out) {
    Sha1Context sha1;
    Sha256Context sha256;
    if (out == NULL) {
        return;
    }
    Sha1Init(&sha1);
    Sha256Init(&sha256);
    if (data != NULL && size != 0) {
        Sha1Update(&sha1, (const uint8_t*)data, size);
        Sha256Update(&sha256, (const uint8_t*)data, size);
    }
    FinishHashes(&sha1, &sha256, size, out);
}

int Port_HashFile(const char* path, PortRomHashes* out) {
    uint8_t buffer[4096];
    size_t total = 0;
    Sha1Context sha1;
    Sha256Context sha256;
    FILE* file;

    if (path == NULL || out == NULL || (file = fopen(path, "rb")) == NULL) {
        return 0;
    }
    Sha1Init(&sha1);
    Sha256Init(&sha256);
    for (;;) {
        const size_t readSize = fread(buffer, 1, sizeof(buffer), file);
        if (readSize != 0) {
            if (total > SIZE_MAX - readSize) {
                fclose(file);
                return 0;
            }
            total += readSize;
            Sha1Update(&sha1, buffer, readSize);
            Sha256Update(&sha256, buffer, readSize);
        }
        if (readSize != sizeof(buffer)) {
            if (ferror(file)) {
                fclose(file);
                return 0;
            }
            break;
        }
    }
    fclose(file);
    FinishHashes(&sha1, &sha256, total, out);
    return 1;
}

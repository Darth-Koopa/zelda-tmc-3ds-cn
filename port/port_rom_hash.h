#pragma once

#include <stddef.h>

#define PORT_SHA1_HEX_LENGTH 40
#define PORT_SHA256_HEX_LENGTH 64

typedef struct {
    char sha1[PORT_SHA1_HEX_LENGTH + 1];
    char sha256[PORT_SHA256_HEX_LENGTH + 1];
    size_t size;
} PortRomHashes;

#ifdef __cplusplus
extern "C" {
#endif

void Port_HashBuffer(const void* data, size_t size, PortRomHashes* out);
int Port_HashFile(const char* path, PortRomHashes* out);

#ifdef __cplusplus
}
#endif

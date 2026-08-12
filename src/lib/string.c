#include <kernel/lib/string.h>

void* memcpy(void* restrict dest, const void* restrict src, size_t len) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    // Copy 8-byte chunks (64-bit alignment)
    size_t qwords = len / 8;
    uint64_t* d64 = (uint64_t*)d;
    const uint64_t* s64 = (const uint64_t*)s;

    for (size_t i = 0; i < qwords; i++) {
        d64[i] = s64[i];
    }

    // Copy remaining trailing bytes (0 to 7 bytes)
    size_t offset = qwords * 8;
    for (size_t i = offset; i < len; i++) {
        d[i] = s[i];
    }

    return dest;
}

void* memset(void* dest, int val, size_t len)
{
    uint8_t* d = (uint8_t*)dest;
    for (size_t i = 0; i < len; i++) {
        d[i] = (uint8_t)val;
    }
    return dest;
}
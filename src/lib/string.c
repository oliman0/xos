#include <stdint.h>
#include <kernel/lib/string.h>

void* memcpy(void* restrict dest, const void* restrict src, size_t len)
{
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    for (size_t i = 0; i < len; i++) {
        d[i] = s[i];
    }

    return dest;
}

void* memset(void* dest, int val, size_t len)
{
    uint8_t* d = (uint8_t*)dest;
    uint8_t v = (uint8_t)val;

    for (size_t i = 0; i < len; i++) {
        d[i] = v;
    }
    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n)
{
    const uint8_t* p1 = (const uint8_t*)s1;
    const uint8_t* p2 = (const uint8_t*)s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] < p2[i]) return -1;
        if (p1[i] > p2[i]) return 1;
    }
    return 0;
}

size_t strlen(const char* str)
{
    size_t len = 0;
    while (str[len]) len++;
    return len;
}
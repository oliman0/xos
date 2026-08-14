#ifndef XOS_STRING_H
#define XOS_STRING_H

#include <stddef.h>
#include <stdint.h>

void* memcpy(void* restrict dest, const void* restrict src, size_t len);

void* memset(void* dest, int val, size_t len);

int memcmp(const void* s1, const void* s2, size_t n);

size_t strlen(const char* str);

#endif
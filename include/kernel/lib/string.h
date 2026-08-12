#ifndef XOS_STRING_H
#define XOS_STRING_H

#include <stddef.h>
#include <stdint.h>

void* memcpy(void* restrict dest, const void* restrict src, size_t len);

void* memset(void* dest, int val, size_t len);

#endif
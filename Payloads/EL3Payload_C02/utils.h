#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

void * memset ( void * ptr, int value, uint32_t num );
int memcmp(void *mem1, void *mem2, uint32_t len);
void *memcpy(void *dst, const void * src, uint32_t n);

void putc(char c);


void delay(uint32_t ctr);

#endif // UTILS_H
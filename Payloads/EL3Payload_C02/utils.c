
#include <stdint.h>

#define SMN_FLASH_ADDR  0x27000000
#define SPI_PRINTF_ADDR ((uint32_t*)(SMN_FLASH_ADDR+0x21000))


void * memset ( void * ptr, int value, uint32_t num ){
  uint8_t *p =(uint8_t*)ptr;
  while (num--){
    *p++ = value;
  }
  return ptr;
}

int memcmp(void *mem1, void *mem2, uint32_t len){
  int ret = 0;

  uint8_t *m1 = (uint8_t*)mem1;
  uint8_t *m2 = (uint8_t*)mem2;

  while (!ret && len--){
    ret = *m2++ - *m1++;
  }

  return ret;
}

void *memcpy(void *dst, const void * src, uint32_t n){
  uint8_t *d =(uint8_t*)dst;
  const uint8_t *s =(const uint8_t*)src;
  for (uint32_t i = 0; i<n; i++){
    d[i] = s[i];
  }
  return dst;  
}


__attribute__((naked)) void delay(uint32_t ctr){
  asm(
  "cmp r0, #0\n"
  "beq .done\n"
  ".loop:\n"
  "subs r0, r0, #1\n"
  "bne .loop\n"
  ".done:\n"
  "bx lr\n"
  );
}

void putc(char c){
  *SPI_PRINTF_ADDR = c;
  delay(0x2000);
  if (c == '\n'){
    delay(0x8000);
  }
}

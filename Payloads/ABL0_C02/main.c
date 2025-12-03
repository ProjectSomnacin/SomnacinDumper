#include <stdint.h>

#pragma mark code
void c_main(uint32_t arg){
  void (*postcode)(uint32_t code);
  postcode = (void(*)(uint32_t))arg;
  postcode(0x61626364); //hello

  uint8_t *flashMapped = (uint8_t*) (0x27000000+0x10000);
  uint32_t *someaddr = (uint32_t*)(flashMapped);

  uint32_t dst = ((uint32_t)&c_main) + 0x2000;
  dst &=~0xFFF;
  uint32_t *dstAddr = (uint32_t*)dst;
  postcode(someaddr[0]);

  for (int i = 0; i < 0x8000; i++){
    dstAddr[i] = someaddr[1+i];
  }
  void (*newpayload)(void) = (void(*)(void))(((uint32_t)dstAddr)|1);

  postcode(dstAddr[0]);
  postcode(0x13371337); //start
  newpayload();

  postcode(0x91929394); //goodbye
}
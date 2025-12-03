
#include <pico.h>
#include <stdio.h>

#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <hardware/clocks.h>


#include "macros.h"

#include "postcode.h"
#include "spihack.h"

#include "payload_abl0.h"
#include "payload_el3.h"

#pragma mark pin config
// LAD[0-3]
#define PIN_LPC_BUS_BASE  4
#define PIN_LPC_LCLK      8
#define PIN_LPC_LFRAME    9

#define PIN_SPI_MOSI          10
#define PIN_SPI_CLK           12
#define PIN_SPI_CS_MAINBOARD  13
#define PIN_SPI_CS_CHIP       17
#define PIN_SPI_MISO          18

#define TARGET_RESET_PIN (16u)


#pragma mark amd hack config
#ifndef ABL0_START
#define ABL0_START  0x962d00
#endif

#ifndef ABL0_SIZE
#define ABL0_SIZE   0x440
#endif

#ifndef SECUREOS_START
#define SECUREOS_START  0x99e200
#endif

#ifndef SECUREOS_SIZE
#define SECUREOS_SIZE   0x122f0
#endif


#ifndef X86_BIOS_ADDR1
#define X86_BIOS_ADDR1 (0xe02000+0x83B00)
#endif

#ifndef X86_BIOS_ADDR2
#define X86_BIOS_ADDR2 (0xe02000+0x1FD270)
#endif

#define MAGIC_INJECTION_ADDR 0x10000
#define SPI_PRINTF_ADDR 0x21000
#define SPI_DUMP_ADDR   0x20000

#pragma mark static global vars
static int gArmedPayload = 0;

#pragma mark code defs
void toggle_inject_payload();


#pragma mark code
void injectDoneCallback(){
  toggle_inject_payload();
  switch (gArmedPayload){
  // case 0:
  //   info("Done inject ABL0");
  //   break;

  // case 1:
  //   info("Done inject EL3");
  //   break;

  default:
    info("Done inject!");
    break;
  }
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

void spiWriteCallback(uint32_t addr, uint32_t data){
  // if ((addr >> 24) == 3){
  //   addr &= 0xFFFFFF;
  //   if (addr < 0x1000){
  //     printf("R: 0x%08x - 0x%08x\n",addr,data);
  //   }
    
  //   return;
  // }
  
  if ((addr >> 24) != 2) return;
  addr &= 0xFFFFFF;
  if (addr == SPI_PRINTF_ADDR){
    putchar(data);
  }else if (addr == SPI_DUMP_ADDR){
    char *d = (char*)&data;
    printf("%02x%02x%02x%02x",d[0],d[1],d[2],d[3]);
  } else if (addr == 0x20004){
    gpio_set_dir(15, GPIO_OUT);
    gpio_put(15, 0);
    delay(1);
    gpio_put(15, 1);
    // gpio_set_dir(15, GPIO_IN);
//    gpio_disable_pulls(15);
  }else{
    printf("SPI: 0x%08x - 0x%08x\n",addr,data);
  }  
}

void init_pins(){
  gpio_init(TARGET_RESET_PIN);
  gpio_set_dir(TARGET_RESET_PIN, GPIO_OUT);
  gpio_put(TARGET_RESET_PIN, 0);

  gpio_set_function(15, GPIO_FUNC_SIO);
}

void reset_mainboard(){
  debug("reset mainboard");
  gpio_put(TARGET_RESET_PIN, 1);
  sleep_ms(100);
  gpio_put(TARGET_RESET_PIN, 0);
}

void toggle_inject_payload(){
  if (gArmedPayload == 0){
    spihack_inject_payload(ABL0_START+0xFC, ABL0_START+0xFC+(sizeof(payload_abl0)&~3), payload_abl0, sizeof(payload_abl0));
  }else{
    spihack_inject_payload(MAGIC_INJECTION_ADDR, MAGIC_INJECTION_ADDR+(sizeof(payload_el3)&~3), payload_el3, sizeof(payload_el3));
  }
  gArmedPayload = !gArmedPayload;
}

void core2(void){
  while (1){
    postcode_task();
    spihack_task();
  }
}

int main(){
  int err = 0;
  int res = 0;
  
  set_sys_clock_khz(198e3, true);

  stdio_init_all();
  // Wait for a while. Otherwise, USB CDC doesn't print all printfs.
  sleep_ms(2000);

  init_pins();
  info("Hello from PicoModchip");

  multicore_launch_core1(core2);

  _Static_assert(PIN_SPI_CLK+1 == PIN_SPI_CS_MAINBOARD, "CLK,MAINBOARD_CS should be consequitive PINS!");
  _Static_assert(PIN_SPI_MOSI+2 == PIN_SPI_CLK, "MOSI,<UNK>,CLK should be consequitive PINS!");
  
  cretassure(!(res = spihack_init(PIN_SPI_MISO,PIN_SPI_MOSI,PIN_SPI_CLK,PIN_SPI_CS_MAINBOARD,PIN_SPI_CS_CHIP)),"spihack_init failed with res=%d",res);
  spihack_register_injectdone_callback(injectDoneCallback);
  spihack_register_spiwrite_callback(spiWriteCallback);
  debug("spihack init ok");

  cretassure(!(res = postcode_init(PIN_LPC_BUS_BASE, PIN_LPC_LFRAME, PIN_LPC_LCLK)),"postcode_init failed with res=%d",res);
  debug("postcode_init init ok");

  toggle_inject_payload();

  reset_mainboard();
  
  
  while (1){
   tight_loop_contents();
  }

error:
  info("Failed with err=%d",err);
  while (1){
   tight_loop_contents();
  }
}
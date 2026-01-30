#include "postcode.h"
#include "postcode.pio.h"

#include "macros.h"

#include <hardware/pio.h>
#include <hardware/dma.h>
#include <hardware/sync.h>

#define ARRAYOF(a) (sizeof(a)/sizeof(*a))

#define POSTCODE_PIO  pio2
#define POSTCODE_SM   0


#pragma mark static vars
static int gPostcode_pio_pc = -1;
static int gDMAChannel = -1;

/*
  Note: alignment needed for DMA wrap!
  Also, do not change pcodes bufsize, more hardcoded stuff done below in code!
*/
static __attribute__((aligned(0x800))) uint16_t pcodes[0x800/2] = {};
static int pcodesPtr = 0;

void postcode_task(void){
  uint16_t curcode = 0;

  static uint32_t postcode = 0;
  static uint8_t *p = (uint8_t*)&postcode;
  static int pcnt = 0;

  while ((curcode = pcodes[pcodesPtr])){
    pcodes[pcodesPtr] = 0;
    pcodesPtr = (pcodesPtr+1) & (ARRAYOF(pcodes))-1;
    uint8_t num = curcode & 0xFF;
    uint8_t val = curcode >> 8;

    if ((num & 0xF0) == 0x80){
      // if (val == 0x39) printf("P 0x%02x V 0x%02x\n",num,val);
      // if (val == 0x39) printf("*");
      if ((num & 0xF) == 0){
        pcnt = 0;
        postcode = 0;
      }
      if ((num & 0xF) == pcnt){
        p[pcnt] = val;
        if (++pcnt == 4){
          printf("POST: 0x%08x\n",postcode);
        } else {
          continue;
        }
      }
    }
    //reset
    postcode = 0;
    pcnt = 0;
  }
}

#pragma mark constructors
int postcode_init(uint32_t pin_LAD_base, uint32_t pin_LFRAME, uint32_t pin_LCLK){
  int err = 0;

  cretassure(pin_LAD_base+4 == pin_LCLK, "LAD[0:3],LCLK should be consequitive PINS!");

  {
    /*
    Make sure all SPI pins are INPUT by default, unless otherwise specified further down
    */
    for (int i = 0; i < 4; i++){
      gpio_init(pin_LAD_base+i);
      gpio_disable_pulls(pin_LAD_base+i);
    }    
    gpio_init(pin_LFRAME);
    gpio_init(pin_LCLK);

    gpio_disable_pulls(pin_LFRAME);
    gpio_disable_pulls(pin_LCLK);
  }

  if (gPostcode_pio_pc == -1){
    pio_sm_set_enabled(POSTCODE_PIO, POSTCODE_SM, false);

    gPostcode_pio_pc = pio_add_program(POSTCODE_PIO, &postcode_program);
    pio_sm_config c = postcode_program_get_default_config(gPostcode_pio_pc);

    sm_config_set_clkdiv(&c, 1);
    sm_config_set_in_pins(&c, pin_LAD_base);
    sm_config_set_in_shift(&c, false, false, 32);
    sm_config_set_jmp_pin(&c, pin_LFRAME);

    pio_sm_set_pindirs_with_mask(POSTCODE_PIO, POSTCODE_SM, 0, ~0); //All pins are input

    pio_sm_init(POSTCODE_PIO, POSTCODE_SM, gPostcode_pio_pc + postcode_offset_start, &c);
    pio_sm_set_enabled(POSTCODE_PIO, POSTCODE_SM, true);
  }

  if (gDMAChannel == -1){
    gDMAChannel = dma_claim_unused_channel(true);

    {
        dma_channel_config c = dma_channel_get_default_config(gDMAChannel);
        channel_config_set_dreq(&c, pio_get_dreq(POSTCODE_PIO, POSTCODE_SM, false)); //false=read data from SM, true=write data to SM
        channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
        channel_config_set_read_increment(&c, false);
        channel_config_set_write_increment(&c, true);
        channel_config_set_ring(&c, true, 11);
        dma_channel_configure(  gDMAChannel,
                                &c,
                                &pcodes[0],
                                &POSTCODE_PIO->rxf[POSTCODE_SM],
                                ARRAYOF(pcodes) | (1u << 28) /*trigger self*/,
                                false
        );
    }
    dma_channel_start(gDMAChannel);
  }

error:
  return err;  
}

void postcode_deinit(void){
  if (gPostcode_pio_pc != -1){
      pio_sm_set_enabled(POSTCODE_PIO, POSTCODE_SM, false);
      pio_remove_program(POSTCODE_PIO, &postcode_program, gPostcode_pio_pc);
      gPostcode_pio_pc = -1;
  }
  if (gDMAChannel != -1){
      dma_channel_abort(gDMAChannel);
      dma_channel_unclaim(gDMAChannel);
      gDMAChannel = -1;
  }
}

uint32_t reverse_nibbles(uint32_t in_val) {
    uint32_t out_val = 0;

    for(uint8_t i = 0; i < 32; i += 4) {
        out_val <<= 4;
        out_val |= in_val >> i & 0xf;
    }
    return out_val;
}

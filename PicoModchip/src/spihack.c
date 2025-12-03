#include "spihack.h"
#include "spi_inject.pio.h"
#include "spi_trigger.pio.h"
#include "spi_reader.pio.h"

#include "macros.h"

#include <hardware/pio.h>
#include <hardware/dma.h>
#include <hardware/irq.h>

#define ARRAYOF(a) (sizeof(a)/sizeof(*a))


#define TRIGGER_PIO   pio0
#define INJECTOR_PIO  pio1
#define READER_PIO    pio1


#define TRIGGER_SM  0
#define INJECTOR_SM 0
#define READER_SM   1

#define SPI_INJECT_SELECTOR 21

#pragma mark static vars
static int gInjector_pio_pc = -1;
static int gTrigger_pio_pc = -1;
static int gReader_pio_pc = -1;

static int gInjectDMAChannel = -1;
static int gReaderDMAChannel = -1;

static f_injectdone_cb gUserIrqCallback = NULL;
static f_spiwrite_cb gUserSpiwriteCallback = NULL;


/*
  Note: alignment needed for DMA wrap!
  Also, do not change pcodes bufsize, more hardcoded stuff done below in code!
*/
static __attribute__((aligned(0x4000))) uint32_t gSPIWritesCache[0x4000/4] = {};
static uint32_t gSPIWritesPtr = 0;


#pragma mark private
static void dmaIRQHandler(void){
  if (gUserIrqCallback){
    gUserIrqCallback();
  }
  dma_channel_acknowledge_irq0(gInjectDMAChannel);
}

#pragma mark constructors
int spihack_init(uint32_t pin_miso, uint32_t pin_mosi, uint32_t pin_clk, uint32_t pin_mainboard_cs, uint32_t pin_chip_cs){
  int err = 0;

  cretassure(pin_clk+1 == pin_mainboard_cs, "CLK,MAINBOARD_CS should be consequitive PINS!");
  cretassure(pin_mosi+2 == pin_clk, "MOSI,<UNK>,CLK should be consequitive PINS!");

  gpio_init(SPI_INJECT_SELECTOR);
  gpio_set_dir(SPI_INJECT_SELECTOR, GPIO_OUT);
  gpio_put(SPI_INJECT_SELECTOR, 0);

  {
    /*
      Make sure all SPI pins are INPUT by default, unless otherwise specified further down
    */
    gpio_init(pin_miso);
    gpio_init(pin_mosi);
    gpio_init(pin_clk);
    gpio_init(pin_mainboard_cs);
    gpio_init(pin_chip_cs);
  }

  if (gInjector_pio_pc == -1){
    gpio_pull_up(pin_chip_cs);

    pio_gpio_init(INJECTOR_PIO, pin_chip_cs);
    pio_gpio_init(INJECTOR_PIO, pin_miso);
    pio_sm_set_enabled(INJECTOR_PIO, INJECTOR_SM, false);

    gInjector_pio_pc = pio_add_program(INJECTOR_PIO, &spi_inject_program);
    pio_sm_config c = spi_inject_program_get_default_config(gInjector_pio_pc);

    sm_config_set_clkdiv(&c, 1);

    sm_config_set_out_pins(&c, pin_miso, 1);
    sm_config_set_in_pins(&c, pin_clk);
    sm_config_set_jmp_pin(&c, SPI_INJECT_SELECTOR);
    sm_config_set_set_pins(&c, pin_chip_cs, 1);
    sm_config_set_sideset_pins(&c, pin_miso);
    sm_config_set_sideset(&c, 2, true, true);
    sm_config_set_out_shift(
        &c,
        false,  // ShiftDir : true: shift ISR to right, false: shift ISR to left
        true,   // AutoPull : true: enabled, false: disabled
        8       // AutoPull threshold: <0-32>
    );

    pio_sm_set_pindirs_with_mask(INJECTOR_PIO, INJECTOR_SM, ((1 << pin_chip_cs) | (1 << pin_miso)), ~0);

    pio_sm_init(INJECTOR_PIO, INJECTOR_SM, gInjector_pio_pc, &c);
    pio_sm_set_enabled(INJECTOR_PIO, INJECTOR_SM, true);
  }

  if (gReader_pio_pc == -1){
    pio_sm_set_enabled(READER_PIO, READER_SM, false);

    gReader_pio_pc = pio_add_program(READER_PIO, &spi_reader_program);
    pio_sm_config c = spi_reader_program_get_default_config(gReader_pio_pc);

    sm_config_set_clkdiv(&c, 1);

    sm_config_set_in_pins(&c, pin_mosi);
    sm_config_set_jmp_pin(&c, pin_mainboard_cs);
    sm_config_set_in_shift(
        &c,
        false,  // ShiftDir : true: shift ISR to right, false: shift ISR to left
        false,  // AutoPush : true: enabled, false: disabled
        32      // AutoPush threshold: <0-32>
    );

    pio_sm_init(READER_PIO, READER_SM, gReader_pio_pc, &c);
    pio_sm_set_enabled(READER_PIO, READER_SM, true);
  }

  if (gTrigger_pio_pc == -1){
    pio_gpio_init(TRIGGER_PIO, SPI_INJECT_SELECTOR);

    pio_sm_set_enabled(TRIGGER_PIO, TRIGGER_SM, false);

    gTrigger_pio_pc = pio_add_program(TRIGGER_PIO, &spi_trigger_program);
    pio_sm_config c = spi_trigger_program_get_default_config(gTrigger_pio_pc);

    sm_config_set_clkdiv(&c, 1);
    sm_config_set_in_pins(&c, pin_mosi);
    sm_config_set_jmp_pin(&c, pin_mainboard_cs);
    sm_config_set_out_pins(&c, SPI_INJECT_SELECTOR, 1);
    sm_config_set_set_pins(&c, SPI_INJECT_SELECTOR, 1);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX); //disable RX, make TX fifo twice as long
    sm_config_set_in_shift(
        &c,
        false,  // ShiftDir : true: shift ISR to right, false: shift ISR to left
        false, // AutoPush : true: enabled, false: disabled
        0      // AutoPush threshold: <0-32>
    );

    pio_sm_set_pindirs_with_mask(TRIGGER_PIO, TRIGGER_SM, (1 << SPI_INJECT_SELECTOR), ~0);

    pio_sm_init(TRIGGER_PIO, TRIGGER_SM, gTrigger_pio_pc, &c);
    pio_sm_set_enabled(TRIGGER_PIO, TRIGGER_SM, true);
  }

  if (gInjectDMAChannel == -1){
    gInjectDMAChannel = dma_claim_unused_channel(true);

    irq_set_exclusive_handler(DMA_IRQ_0, dmaIRQHandler);      // Set interrupt handler
    irq_set_enabled(DMA_IRQ_0, true);                         // Enable the IRQ
    dma_channel_set_irq0_enabled(gInjectDMAChannel, true);    // Enable IRQ for RX DMA
  }

  if (gReaderDMAChannel == -1){
      gReaderDMAChannel = dma_claim_unused_channel(true);

    {
        dma_channel_config c = dma_channel_get_default_config(gReaderDMAChannel);
        channel_config_set_dreq(&c, pio_get_dreq(READER_PIO, READER_SM, false)); //false=read data from SM, true=write data to SM
        channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
        channel_config_set_read_increment(&c, false);
        channel_config_set_write_increment(&c, true);
        channel_config_set_ring(&c, true, 14);
        dma_channel_configure(  gReaderDMAChannel,
                                &c,
                                &gSPIWritesCache[0],
                                &READER_PIO->rxf[READER_SM],
                                1 | (0xfu << 28) /*trigger infinite*/,
                                true
        );
    }
  }

error:
  return err;
}

void spihack_deinit(void){  
  if (gInjector_pio_pc != -1){
      pio_sm_set_enabled(INJECTOR_PIO, INJECTOR_SM, false);
      pio_remove_program(INJECTOR_PIO, &spi_inject_program, gInjector_pio_pc);
      gInjector_pio_pc = -1;
  }

  if (gTrigger_pio_pc != -1){
      pio_sm_set_enabled(TRIGGER_PIO, TRIGGER_SM, false);
      pio_remove_program(TRIGGER_PIO, &spi_inject_program, gTrigger_pio_pc);
      gTrigger_pio_pc = -1;
  }

  if (gInjectDMAChannel != -1){
    dma_channel_set_irq0_enabled(gInjectDMAChannel, false);
    irq_set_enabled(DMA_IRQ_0, false); 
    irq_remove_handler(DMA_IRQ_0, dmaIRQHandler);

    dma_channel_abort(gInjectDMAChannel);
    dma_channel_unclaim(gInjectDMAChannel);
    gInjectDMAChannel = -1;
  }

  if (gReaderDMAChannel != -1){
    dma_channel_abort(gReaderDMAChannel);
    dma_channel_unclaim(gReaderDMAChannel);
    gReaderDMAChannel = -1;
  }
}

void spihack_push_filter(uint32_t address, bool doInject){
  pio_sm_put_blocking(TRIGGER_PIO, TRIGGER_SM, address);
  pio_sm_put_blocking(TRIGGER_PIO, TRIGGER_SM, doInject != 0);
}

void spihack_inject_payload_manual(uint8_t *buf, uint32_t bufSize){
  for (size_t i = 0; i < bufSize; i++){
    pio_sm_put_blocking(INJECTOR_PIO, INJECTOR_SM, buf[i]);
  }
}

void spihack_inject_payload(uint32_t addressStart, uint32_t addressEnd, void *buf, size_t bufSize){
  dma_channel_abort(gInjectDMAChannel);
  {
    dma_channel_config c = dma_channel_get_default_config(gInjectDMAChannel);
    channel_config_set_dreq(&c, pio_get_dreq(INJECTOR_PIO, INJECTOR_SM, true)); //false=read data from SM, true=write data to SM
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    uint32_t transferCNT = bufSize & ~3;
    dma_channel_configure(gInjectDMAChannel,
                          &c,
                          &INJECTOR_PIO->txf[INJECTOR_SM],
                          buf,
                          transferCNT,
                          true
    );
  }

  //enable trigger
  spihack_push_filter(addressStart, true);
  spihack_push_filter(addressEnd, false);
}

void spihack_register_injectdone_callback(f_injectdone_cb cb){
  gUserIrqCallback = cb;
}

void spihack_register_spiwrite_callback(f_spiwrite_cb cb){
  gUserSpiwriteCallback = cb;  
}

void spihack_task(void){
  uint64_t *spiwrites = (uint64_t*)gSPIWritesCache;
  const uint32_t cacheCnt = sizeof(gSPIWritesCache)/sizeof(uint64_t);
  uint64_t curWrite;

  while ((curWrite = spiwrites[gSPIWritesPtr])){
    spiwrites[gSPIWritesPtr] = 0;
    gSPIWritesPtr = (gSPIWritesPtr+1) & (cacheCnt-1);

    uint32_t addr = curWrite >> 32;
    uint32_t data = __builtin_bswap32(curWrite);

    if (gUserSpiwriteCallback){
      gUserSpiwriteCallback(addr,data);
    }
  }
}
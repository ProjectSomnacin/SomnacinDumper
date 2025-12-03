#include <stdint.h>
#include "printf.h"
#include "utils.h"

#pragma mark defines

#define REG_32(addr) *(volatile uint32_t*)(addr)
#define REG_16(addr) *(volatile uint16_t*)(addr)
#define REG_8(addr)  *(volatile uint8_t*)(addr)

#define CCP_BASE 0x3002000

#define CCP_CTRL_STATUS REG_32(CCP_BASE)
#define CCP_TAIL REG_32(CCP_BASE+4)
#define CCP_HEAD REG_32(CCP_BASE+8)
#define CCP_ERR REG_32(CCP_BASE+0x100)

#define CCP_MEMTYPE_SYSTEM 0
#define CCP_MEMTYPE_LSB 1
#define CCP_MEMTYPE_PSP 2
#define CCP_HI_ADDR(addr, type) ((addr) | (type << 16))

#define CCP_TYPED_ADDR(addr, type) (((uint64_t)addr) | (((uint64_t)type) << 48))

#define SMN_FLASH_ADDR  0x27000000
#define SPI_PRINTF_ADDR ((uint32_t*)(SMN_FLASH_ADDR+0x21000))
#define SPI_DUMP_ADDR ((uint32_t*)(SMN_FLASH_ADDR+0x20000))

#pragma mark structs

/*
  Defs from https://reviews.freebsd.org/D12723
*/

enum ccp_aes_mode {
	CCP_AES_MODE_ECB = 0,
	CCP_AES_MODE_CBC,
	CCP_AES_MODE_OFB,
	CCP_AES_MODE_CFB,
	CCP_AES_MODE_CTR,
	CCP_AES_MODE_CMAC,
	CCP_AES_MODE_GHASH,
	CCP_AES_MODE_GCTR,
	CCP_AES_MODE_IAPM_NIST,
	CCP_AES_MODE_IAPM_IPSEC,

	/* Not a real hardware mode; used as a sentinel value internally. */
	CCP_AES_MODE_XTS,
};

enum ccp_engine {
	CCP_ENGINE_AES = 0,
	CCP_ENGINE_XTS_AES,
	CCP_ENGINE_3DES,
	CCP_ENGINE_SHA,
	CCP_ENGINE_RSA,
	CCP_ENGINE_PASSTHRU,
	CCP_ENGINE_ZLIB_DECOMPRESS,
	CCP_ENGINE_ECC,
};

enum ccp_aes_type {
	CCP_AES_TYPE_128 = 0,
	CCP_AES_TYPE_192,
	CCP_AES_TYPE_256,
};

typedef struct {
    uint32_t cmd;
    uint32_t len;
    uint32_t src_lo;
    uint32_t src_hi;
    uint32_t dst_lo;
    uint32_t dst_hi;
    uint32_t key_lo;
    uint32_t key_hi;
} ccp_desc;


#pragma mark global uninitialized variables
void (*postcode)(uint32_t code);

#pragma mark code
void ccp_passthrough(uint64_t dst, uint64_t src, uint32_t len) {
    ccp_desc *desc = (ccp_desc*) 0x54000;
    
    desc->cmd = 0x500011;
    desc->len = len;
    desc->src_lo = src;
    desc->src_hi = src >> 32;
    desc->dst_lo = dst;
    desc->dst_hi = dst >> 32;
    desc->key_lo = 0;
    desc->key_hi = 0;
    
    CCP_HEAD = 0x54000;
    CCP_TAIL = 0;
    CCP_CTRL_STATUS |= 0x17;

    while ((CCP_CTRL_STATUS & 3) != 2) {
      delay(10);
    }
}

static uint32_t gChainCounter = 0;
void ccp_passthrough_chain_push(uint64_t dst, uint64_t src, uint32_t len) {
    ccp_desc *desc = (ccp_desc*) 0x55000;
    
    desc->cmd = 0x500000;
    if (gChainCounter == 0){
      desc->cmd |= 0x08; //start of message
    }
    
    desc->len = len;
    desc->src_lo = src;
    desc->src_hi = src >> 32;
    desc->dst_lo = dst;
    desc->dst_hi = dst >> 32;
    desc->key_lo = 0;
    desc->key_hi = 0;

    ccp_passthrough(CCP_TYPED_ADDR(0x20*gChainCounter, CCP_MEMTYPE_SYSTEM), CCP_TYPED_ADDR(0x55000, CCP_MEMTYPE_PSP), sizeof(ccp_desc));    
    gChainCounter++;
}

void ccp_passthrough_chain_finalize() {
  ccp_desc *desc = (ccp_desc*) 0x55000;
  uint32_t lstIxd = gChainCounter-1;
  ccp_passthrough(CCP_TYPED_ADDR(0x55000, CCP_MEMTYPE_PSP), CCP_TYPED_ADDR(0x20*lstIxd, CCP_MEMTYPE_SYSTEM), sizeof(ccp_desc));    
  desc->cmd |= 0x11;
  ccp_passthrough(CCP_TYPED_ADDR(0x20*lstIxd, CCP_MEMTYPE_SYSTEM), CCP_TYPED_ADDR(0x55000, CCP_MEMTYPE_PSP), sizeof(ccp_desc));    
}


void findResetRegister(void) {
  /*
    0x03000000 - 0x03020000 MMIO but no hang
    0x03020000 - 0x03030000 Nothing in this range
    0x03030000 - 0x03040000 MMIO but no hang
    0x03040000 - 0x03200000 Nothing in this range (checked in 64K steps)
  */
  uint32_t base = 0x03200000;

  int z = 0;
  uint32_t skipaddrsArr[] = { 0x3200044, 0 };

  for (size_t i = 0; 1; i++) {
    uint32_t addr = base + (i * 4);
    if (skipaddrsArr[z] == addr) { z++; continue; }
    printf("[%d] Will write to address 0x%08x\n", i, addr);
    REG_32(addr) = 0xffffffff;
    printf("[%d] Wrote 0xffffffff to address 0x%08x\n", i, addr);
  }
  printf("done\n"); //0x3200090 is the lucky target!
}

void c_main(uint32_t arg) {
  postcode = (void(*)(uint32_t))arg;
  postcode(0x71727374); //hello
  printf("Hello world from EL3 payload!\n");

  uint8_t buf[0x100];

  // findResetRegister();

#define WRITE_SYSTEM_MEM_START 0x16000

  /*
    First set mode to 0, and dump rom to sysmem, 
    then recompile the payload with mode=1 (let the pico very quickly reset the board)
    and dump sysmem into SPI magic address.
  */
  int mode = 0; //SET TO 1 FOR DUMPINNG TO THE OUTSIDE WORLD!

  if (mode){
    for (size_t i = 0; i < 0x10000; i+=sizeof(buf)){
      ccp_passthrough(CCP_TYPED_ADDR(buf, CCP_MEMTYPE_PSP), CCP_TYPED_ADDR(i, CCP_MEMTYPE_SYSTEM), sizeof(buf));
      for (size_t z = 0; z < sizeof(buf)/4; z++){
        uint32_t *bb = (uint32_t*)buf;
        *SPI_DUMP_ADDR = bb[z];
        delay(0x100000);
      }
    }
    return;
  }

  gChainCounter = 0;
  ccp_passthrough_chain_push(CCP_TYPED_ADDR(WRITE_SYSTEM_MEM_START+0x1000, CCP_MEMTYPE_SYSTEM), CCP_TYPED_ADDR(SMN_FLASH_ADDR, CCP_MEMTYPE_PSP), 0x80);

  for (size_t i = 0; i < 2800; i++){
    ccp_passthrough_chain_push(CCP_TYPED_ADDR(0, CCP_MEMTYPE_SYSTEM), CCP_TYPED_ADDR(0, CCP_MEMTYPE_SYSTEM), 0x18000);
  }

  ccp_passthrough_chain_push(CCP_TYPED_ADDR(0, CCP_MEMTYPE_SYSTEM), CCP_TYPED_ADDR(0xFFFF0000, CCP_MEMTYPE_PSP), 0x10000);

  ccp_passthrough_chain_finalize();

  printf("About to dump ROM to sysmem, there will be no further messages.\n");
  printf("Wait for a few seconds after seeing this, then re-run with mode=1\n");
  printf("to retrieve dump.\n");

  
  CCP_HEAD = 0x00+0x00;
  CCP_TAIL = 0x00+0x20*gChainCounter;
  CCP_CTRL_STATUS = 0x73;
  delay(2);
  *(uint32_t*)0x3200090 = 1;
  
  while (1)
    ;

  // printf("Payload done!\n");
  postcode(0x81828384); //goodbye
}

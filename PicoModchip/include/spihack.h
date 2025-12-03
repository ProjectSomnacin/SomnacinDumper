#ifndef SPIHACK_H
#define SPIHACK_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*f_injectdone_cb)(void);
typedef void (*f_spiwrite_cb)(uint32_t addr, uint32_t data);

int spihack_init(uint32_t pin_miso, uint32_t pin_mosi, uint32_t pin_clk, uint32_t pin_cs_mainboard, uint32_t pin_cs_chip);
void spihack_deinit(void);

void spihack_push_filter(uint32_t address, bool doInject);
void spihack_inject_payload_manual(uint8_t *buf, uint32_t bufSize);
void spihack_inject_payload(uint32_t addressStart, uint32_t addressEnd, void *buf, size_t bufSIze);

void spihack_register_injectdone_callback(f_injectdone_cb cb);

void spihack_register_spiwrite_callback(f_spiwrite_cb cb);

void spihack_task(void);


#ifdef __cplusplus
}
#endif

#endif // SPIHACK_H
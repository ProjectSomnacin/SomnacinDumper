#ifndef POSTCODE_H
#define POSTCODE_H

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

int postcode_init(uint32_t pin_LAD_base, uint32_t pin_LFRAME, uint32_t pin_LCLK);
void postcode_deinit(void);

void postcode_task(void);

#ifdef __cplusplus
}
#endif

#endif // POSTCODE_H
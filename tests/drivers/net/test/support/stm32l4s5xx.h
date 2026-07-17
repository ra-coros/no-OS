#ifndef _STM32L4S5XX_STUB_H_
#define _STM32L4S5XX_STUB_H_

#include <stdint.h>

#define __NVIC_PRIO_BITS  4U

static inline uint32_t __get_BASEPRI(void) { return 0; }
static inline void __set_BASEPRI(uint32_t value) { (void)value; }
static inline void __DSB(void) {}
static inline void __ISB(void) {}

#endif /* _STM32L4S5XX_STUB_H_ */

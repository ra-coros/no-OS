/*******************************************************************************
 *   @file   boardsupport.h
 *   @brief  Header file for the Board Support Package.
 *   @author Ra Coros (ra.coros@analog.com)
 ********************************************************************************
 * Copyright 2026(c) Analog Devices, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of Analog Devices, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES, INC. "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL ANALOG DEVICES, INC. BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *******************************************************************************/
#ifndef BOARDSUPPORT_H
#define BOARDSUPPORT_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "stm32l4xx_hal.h"

#include "bsp_config.h"
#include "bsp_def.h"
#include "capi_irq.h"
#include "dma.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

struct bsp_desc {
	struct bsp_dma_desc  *dma;
	struct bsp_gpio_desc *gpio;
	struct bsp_spi_desc  *spi;
	struct bsp_uart_desc *uart;
};

extern char aDebugString[150u];
extern char aDebugChar;
void common_Fail(char *failure_reason);
void common_Perf(char *info_string);
void common_perf_read(char *info_string);
uint32_t common_readline(char *buffer, uint32_t max_len);
int32_t common_readinteger(void);

#define DEBUG_MESSAGE(...) \
  do { \
    sprintf(aDebugString,__VA_ARGS__); \
    common_Perf(aDebugString); \
  } while(0)

#define DEBUG_RESULT(s,result,expected_value) \
  do { \
    if ((result) != (expected_value)) { \
      sprintf(aDebugString,"%s  %d", __FILE__,__LINE__); \
      common_Fail(aDebugString); \
      sprintf(aDebugString,"%s Error Code: 0x%08X\n\rFailed\n\r",(s),(result)); \
      common_Perf(aDebugString); \
      exit(0); \
    } \
  } while (0)

#define DEBUG_READ(var) \
  do { \
    *(var) = (char)common_readinteger(); \
  } while(0)

#define DEBUG_READ_STRING(buffer, max_len) \
  do { \
    common_readline((buffer), (max_len)); \
  } while(0)

int             bsp_system_init(struct bsp_desc **desc);
void            bsp_system_remove(struct bsp_desc *desc);
uint32_t        bsp_sys_now(void);
uint32_t        bsp_register_irq_callback(struct bsp_desc *desc,
					capi_isr_callback_t callback,
					void *param);
void            bsp_disable_irq(struct bsp_desc *desc);
void            bsp_enable_irq(struct bsp_desc *desc);
void            bsp_hw_reset(struct bsp_desc *desc, bool set);
void            bsp_heart_beat(struct bsp_desc *desc);
void            bsp_heart_beat_led(struct bsp_desc *desc, bool on);
void            bsp_error_led(struct bsp_desc *desc, bool on);
void            bsp_func_led1(struct bsp_desc *desc, bool on);
void            bsp_func_led1_toggle(struct bsp_desc *desc);
void            bsp_func_led2(struct bsp_desc *desc, bool on);
void            bsp_func_led2_toggle(struct bsp_desc *desc);
void            bsp_led_toggle_all(struct bsp_desc *desc);
void            bsp_get_config_pins(struct bsp_desc *desc, uint16_t *value);

uint32_t msg_write(char *ptr);
uint32_t msg_read(char *ptr, uint32_t size);
void     bsp_set_debug_uart(struct bsp_uart_desc *uart);

#endif /* BOARDSUPPORT_H */

/*******************************************************************************
 *   @file   gpio.h
 *   @brief  Header file for the GPIO driver.
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
 * THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES, INC. “AS IS” AND ANY EXPRESS OR
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
#ifndef _GPIO_H
#define _GPIO_H

#include "capi_gpio.h"
#include "capi_irq.h"
#include "bsp_config.h"

struct bsp_gpio_desc {
	struct capi_gpio_port_handle *porta;
	struct capi_gpio_port_handle *portb;
	struct capi_gpio_port_handle *portc;
	struct capi_gpio_port_handle *porte;
#if defined(EVAL_ADIN1110EBZ)
	struct capi_gpio_port_handle *portg;
#endif

	struct capi_gpio_pin eth_reset;
	struct capi_gpio_pin eth_spi_ss;
	struct capi_gpio_pin eth_int_n;
	struct capi_gpio_pin led1;
	struct capi_gpio_pin led2;
	struct capi_gpio_pin led3;
	struct capi_gpio_pin led4;
#if !defined(EVAL_ADIN1110EBZ)
	struct capi_gpio_pin led5;
#endif
	struct capi_gpio_pin cfg0;
	struct capi_gpio_pin cfg1;
	struct capi_gpio_pin cfg2;
	struct capi_gpio_pin cfg3;

	capi_isr_callback_t int_callback;
	void *int_cb_param;
};

int      gpio_init(struct bsp_gpio_desc **desc);
void     gpio_remove(struct bsp_gpio_desc *desc);
uint32_t gpio_register_irq_callback(struct bsp_gpio_desc *desc,
				    capi_isr_callback_t callback,
				    void *param);
uint32_t gpio_disable_irq(struct bsp_gpio_desc *desc);
uint32_t gpio_enable_irq(struct bsp_gpio_desc *desc);

#endif /* _GPIO_H */

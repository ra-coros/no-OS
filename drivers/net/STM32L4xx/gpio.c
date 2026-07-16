/*******************************************************************************
 *   @file   gpio.c
 *   @brief  Implementation for the GPIO driver.
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
#include <errno.h>
#include <string.h>
#include "gpio.h"
#include "capi_irq.h"
#include "stm32_capi_gpio.h"
#include "stm32_capi_irq.h"
#include "bsp_def.h"

#define GPIO_PIN_NUM(pin_mask) (__builtin_ctz(pin_mask))

static int init_port(struct capi_gpio_port_handle **handle,
		     uint64_t identifier, uint32_t pull)
{
	struct stm32_capi_gpio_port_config extra = {
		.mode     = GPIO_MODE_OUTPUT_PP,
		.speed    = GPIO_SPEED_FREQ_LOW,
		.alternate = 0,
		.pull     = pull,
	};

	struct capi_gpio_port_config cfg = {
		.ops        = &stm32_capi_gpio_ops,
		.identifier = identifier,
		.num_pins   = 16,
		.flags      = NULL,
		.extra      = &extra,
	};

	*handle = NULL;
	return capi_gpio_port_init(handle, &cfg);
}

static void init_output_pin(struct capi_gpio_pin *pin,
			    struct capi_gpio_port_handle *port,
			    uint32_t pin_mask, uint8_t initial_value)
{
	pin->port_handle = port;
	pin->number      = GPIO_PIN_NUM(pin_mask);
	pin->flags       = CAPI_GPIO_ACTIVE_HIGH;

	capi_gpio_pin_set_direction(pin, CAPI_GPIO_OUTPUT);
	capi_gpio_pin_set_value(pin, initial_value);
}

static void init_input_pin(struct capi_gpio_pin *pin,
			   struct capi_gpio_port_handle *port,
			   uint32_t pin_mask)
{
	pin->port_handle = port;
	pin->number      = GPIO_PIN_NUM(pin_mask);
	pin->flags       = CAPI_GPIO_ACTIVE_HIGH;

	capi_gpio_pin_set_direction(pin, CAPI_GPIO_INPUT);
}

static void eth_int_n_isr(void *arg)
{
	struct bsp_gpio_desc *d = (struct bsp_gpio_desc *)arg;

	if (d && d->int_callback)
		d->int_callback(d->int_cb_param);
}

static struct bsp_gpio_desc gpio_instance;

int gpio_init(struct bsp_gpio_desc **desc)
{
	int ret;
	struct bsp_gpio_desc *d = &gpio_instance;

	if (!desc)
		return -EINVAL;

	memset(d, 0, sizeof(*d));

	ret = init_port(&d->porta, (uint64_t)(uintptr_t)GPIOA, GPIO_NOPULL);
	if (ret)
		return ret;

	ret = init_port(&d->portb, (uint64_t)(uintptr_t)GPIOB, GPIO_NOPULL);
	if (ret)
		return ret;

	ret = init_port(&d->portc, (uint64_t)(uintptr_t)GPIOC, GPIO_NOPULL);
	if (ret)
		return ret;

	ret = init_port(&d->porte, (uint64_t)(uintptr_t)GPIOE, GPIO_NOPULL);
	if (ret)
		return ret;

#if defined(EVAL_ADIN1110EBZ)
	ret = init_port(&d->portg, (uint64_t)(uintptr_t)GPIOG, GPIO_NOPULL);
	if (ret)
		return ret;

	HAL_PWREx_EnableVddIO2();
#endif

#if defined(EVAL_ADIN1110EBZ)
	init_output_pin(&d->eth_reset, d->portc,
			ETH_RESET_Pin, CAPI_GPIO_HIGH);
#else
	init_output_pin(&d->eth_reset, d->portb,
			ETH_RESET_Pin, CAPI_GPIO_HIGH);
#endif

	init_output_pin(&d->eth_spi_ss, d->portb,
			ETH_SPI_SS_Pin, CAPI_GPIO_LOW);

#if defined(EVAL_ADIN1110EBZ)
	init_output_pin(&d->led1, d->portc,
			BSP_LED1_PIN, CAPI_GPIO_HIGH);
	init_output_pin(&d->led2, d->porte,
			BSP_LED2_PIN, CAPI_GPIO_HIGH);
	init_output_pin(&d->led3, d->porte,
			BSP_LED3_PIN, CAPI_GPIO_HIGH);
	init_output_pin(&d->led4, d->portg,
			BSP_LED4_PIN, CAPI_GPIO_HIGH);
#else
	init_output_pin(&d->led1, d->portb,
			BSP_LED1_PIN, CAPI_GPIO_LOW);
	init_output_pin(&d->led2, d->porte,
			BSP_LED2_PIN, CAPI_GPIO_LOW);
	init_output_pin(&d->led3, d->portb,
			BSP_LED3_PIN, CAPI_GPIO_LOW);
	init_output_pin(&d->led4, d->porte,
			BSP_LED4_PIN, CAPI_GPIO_LOW);
	init_output_pin(&d->led5, d->portb,
			BSP_LED5_PIN, CAPI_GPIO_LOW);
#endif

	init_input_pin(&d->cfg0, d->portb, CFG0_Pin);
	init_input_pin(&d->cfg1, d->portb, CFG1_Pin);
	init_input_pin(&d->cfg2, d->portb, CFG2_Pin);
	init_input_pin(&d->cfg3, d->portb, CFG3_Pin);

#if defined(EVAL_ADIN1110EBZ)
	init_input_pin(&d->eth_int_n, d->portb, ETH_INT_N_Pin);
#else
	init_input_pin(&d->eth_int_n, d->porta, ETH_INT_N_Pin);
#endif

	ret = capi_irq_set_level_edge_trigger(ETH_INT_N_IRQn,
					      CAPI_IRQ_EDGE_FALLING);
	if (ret)
		return ret;

	ret = capi_irq_set_priority(ETH_INT_N_IRQn, 2);
	if (ret)
		return ret;

	*desc = d;
	return 0;
}

void gpio_remove(struct bsp_gpio_desc *desc)
{
	if (!desc)
		return;

	capi_irq_disable(ETH_INT_N_IRQn);
	stm32_capi_irq_disconnect(ETH_INT_N_IRQn);

	desc->int_callback = NULL;
	desc->int_cb_param = NULL;

#if defined(EVAL_ADIN1110EBZ)
	capi_gpio_port_deinit(&desc->portg);
#endif
	capi_gpio_port_deinit(&desc->porte);
	capi_gpio_port_deinit(&desc->portc);
	capi_gpio_port_deinit(&desc->portb);
	capi_gpio_port_deinit(&desc->porta);
}

uint32_t gpio_register_irq_callback(struct bsp_gpio_desc *desc,
				    capi_isr_callback_t callback,
				    void *param)
{
	if (!desc)
		return 1;

	desc->int_callback = callback;
	desc->int_cb_param = param;

	capi_irq_connect(ETH_INT_N_IRQn, eth_int_n_isr, desc);
	return 0;
}

uint32_t gpio_disable_irq(struct bsp_gpio_desc *desc)
{
	(void)desc;
	capi_irq_disable(ETH_INT_N_IRQn);
	return 0;
}

uint32_t gpio_enable_irq(struct bsp_gpio_desc *desc)
{
	(void)desc;
	capi_irq_enable(ETH_INT_N_IRQn);
	return 0;
}

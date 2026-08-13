/*******************************************************************************
 *   @file   dma.c
 *   @brief  DMA controller initialization using CAPI.
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
#include "dma.h"
#include "capi_irq.h"
#include "stm32_capi_dma.h"
#include "stm32_hal.h"

static struct bsp_dma_desc dma_instance;

static void dma_irq_handler(void *arg)
{
	struct bsp_dma_desc *d = (struct bsp_dma_desc *)arg;

	if (d && d->handle)
		capi_dma_isr(d->handle);
}

int dma_init(struct bsp_dma_desc **desc, struct bsp_dma_init_param *param)
{
	int ret;
	struct bsp_dma_desc *d = &dma_instance;

	if (!desc || !param)
		return -EINVAL;

	memset(d, 0, sizeof(*d));
	d->num_channels = param->num_channels;

	__HAL_RCC_DMAMUX1_CLK_ENABLE();
	__HAL_RCC_DMA1_CLK_ENABLE();

	struct capi_dma_config dma_cfg = {
		.id        = 0,
		.num_chans = d->num_channels,
		.ops       = &stm32_capi_dma_ops,
		.irq_handle = NULL,
		.extra     = NULL,
	};

	d->handle = NULL;
	ret = capi_dma_init(&d->handle, &dma_cfg);
	if (ret)
		return ret;

	ret = capi_irq_set_priority(DMA1_Channel1_IRQn, 0);
	if (ret)
		return ret;
	capi_irq_connect(DMA1_Channel1_IRQn, dma_irq_handler, d);
	ret = capi_irq_enable(DMA1_Channel1_IRQn);
	if (ret)
		return ret;

	ret = capi_irq_set_priority(DMA1_Channel2_IRQn, 0);
	if (ret)
		return ret;
	capi_irq_connect(DMA1_Channel2_IRQn, dma_irq_handler, d);
	ret = capi_irq_enable(DMA1_Channel2_IRQn);
	if (ret)
		return ret;

	*desc = d;
	return 0;
}

void dma_remove(struct bsp_dma_desc *desc)
{
	if (!desc)
		return;

	capi_irq_disable(DMA1_Channel1_IRQn);
	capi_irq_disable(DMA1_Channel2_IRQn);

	if (desc->handle) {
		capi_dma_deinit(desc->handle);
		desc->handle = NULL;
	}
}

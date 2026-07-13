/*******************************************************************************
 *   @file   standard_spi.h
 *   @brief  Header file for the Standard SPI driver.
 *   @author Ra Coros (ra.coros@analog.com)
 ********************************************************************************
 * Copyright 2025(c) Analog Devices, Inc.
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
#ifndef _NO_OS_STANDARD_SPI_H
#define _NO_OS_STANDARD_SPI_H

#include "platform/stm32/stm32_capi_spi.h"
#include "no_os_util.h"
#include <stdint.h>


enum standard_spi_buffer_state {
	STANDARD_SPI_STATE_READY = 0,
	STANDARD_SPI_STATE_REGISTER_READ,
	STANDARD_SPI_STATE_REGISTER_WRITE,
	STANDARD_SPI_STATE_FIFO_READ_START,
	STANDARD_SPI_STATE_FIFO_READ_END,
	STANDARD_SPI_STATE_FIFO_WRITE_START,
	STANDARD_SPI_STATE_FIFO_WRITE_END
};

enum standard_spi_timestamp_format {
	STANDARD_SPI_TS_FORMAT_NONE = 0,
	STANDARD_SPI_TS_FORMAT_32B_FREE,
	STANDARD_SPI_TS_FORMAT_32B_1588,
	STANDARD_SPI_TS_FORMAT_64B_1588
};

struct standard_spi_init_param {
	struct no_os_spi_desc *comm_desc;

	/* The OASPI device uses Protected SPI for control transactions */
	bool prote_spi;
};

struct  standard_spi_desc  {
	struct capi_spi_device *comm_desc;
	struct capi_spi_transfer *msg;
	volatile enum standard_spi_buffer_state spi_state;
	uint8_t num_ports;
	bool append_crc;
	bool rx_queue_hp_en;
	bool fcs_check_en; 
	enum standard_spi_timestamp_format ts_format;
	void *app_device;
	uint8_t tx_queue; /*placeholder */
	uint8_t rx_queue; /*placeholder */
	uint8_t rx_queue_lp; /*placeholder */
	uint8_t rx_queue_hp; /*placeholder */
	bool blocking;
	uint32_t register_address;
	uint8_t *data;
	uint32_t *byte_size;
	bool pending_control;
};



#endif /* _NO_OS_STANDARD_SPI_H */

/*******************************************************************************
 *   @file   standard_spi.c
 *   @brief  Implementation for the Standard SPI driver.
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
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "no_os_crc8.h"
#include "no_os_alloc.h"
#include "standard_spi.h"


int standard_spi_init(struct standard_spi_desc **desc,
		    const struct no_os_spi_init_param *param)
{
	return -ENOSYS;
}

int standard_spi_remove(struct standard_spi_desc *desc)
{
	return -ENOSYS;
}

int standard_spi_reg_write(struct standard_spi_desc *desc,
			       uint16_t register_address,
			       const uint8_t *data,
			       uint32_t *bytes_number,
			       bool blocking)
{
	if (desc->spi_state != STANDARD_SPI_STATE_READY)
		return -EBUSY;
	
	desc->register_address = register_address;
	desc->data = (uint8_t *)data;
	desc->byte_size = bytes_number;
	desc->blocking = blocking;
	desc->spi_state = STANDARD_SPI_STATE_REGISTER_WRITE;

	return standard_spi_state_machine(&desc);
}


int standard_spi_reg_read(struct standard_spi_desc *desc,
			       uint16_t register_address,
			       const uint8_t *data,
			       uint32_t *bytes_number,
			       bool blocking)
{	
	if (desc->spi_state != STANDARD_SPI_STATE_READY)
		return -EBUSY;

	desc->register_address = register_address;
	desc->data = (uint8_t *)data;
	desc->byte_size = bytes_number;
	desc->blocking = blocking;
	desc->spi_state = STANDARD_SPI_STATE_REGISTER_READ;

	return standard_spi_state_machine(&desc);
}

int standard_spi_fifo_write(struct standard_spi_desc *desc,
			       uint16_t register_address,
			       const uint8_t *data,
			       uint32_t *bytes_number,
			       bool blocking)
{
	if (desc->spi_state != STANDARD_SPI_STATE_READY)
		return -EBUSY;

	desc->register_address = register_address;
	desc->data = (uint8_t *)data;
	desc->byte_size = bytes_number;
	desc->blocking = blocking;
	desc->spi_state = STANDARD_SPI_STATE_FIFO_WRITE_START;

	return standard_spi_state_machine(&desc);
}

int standard_spi_fifo_read(struct standard_spi_desc *desc,
				   uint8_t port,
			       uint16_t register_address,
			       const uint8_t *data,
			       uint32_t *bytes_number,
			       bool blocking)
{
	if (desc->spi_state != STANDARD_SPI_STATE_READY)
		return -EBUSY;
	
	desc->register_address = register_address;
	desc->data = (uint8_t *)data;
	desc->byte_size = bytes_number;
	desc->blocking = blocking;
	desc->spi_state = STANDARD_SPI_STATE_FIFO_READ_START;

	return standard_spi_state_machine(&desc);
}

static int spi_register_read(struct standard_spi_desc *desc)
{
	uint8_t crc;
	uint8_t recv_crc;
	uint32_t header_len = ADIN1110_RD_HEADER_LEN;

	struct capi_spi_transfer xfer = {
		.tx_buff = desc->data,
		.rx_buff = desc->data,
		.tx_size = ADIN1110_WR_FRAME_SIZE,
		.rx_size = ADIN1110_WR_FRAME_SIZE,
		.timeout = 0,
		.xfer_delay_clk_cycles = 0,
	};
	int ret;

	no_os_put_unaligned_be16(addr, &desc->data[0]);
	desc->data[0] |= ADIN1110_SPI_CD;
	desc->data[2] = 0x0;

	if (desc->append_crc) {
		xfer.tx_size += ADIN1110_CRC_LEN;
		xfer.rx_size += ADIN1110_CRC_LEN;
		desc->data[2] = no_os_crc8(_crc_table, desc->data, 2, 0);
		desc->data[3] = 0x0;
		header_len++;
	}

	xfer.tx_size += header_len;
	xfer.rx_size += header_len;
	if (desc->blocking) {
		ret = capi_spi_transceive(desc->comm_desc, &xfer);
	} else {
		ret = capi_spi_transceive_async(desc->comm_desc, &xfer);
	}
	if (ret)
		return ret;

	if (desc->append_crc) {
		crc = no_os_crc8(_crc_table, &desc->data[header_len], 4, 0);
		recv_crc = desc->data[header_len + ADIN1110_REG_LEN];

		if (crc != recv_crc)
			return -EINVAL;
	}

	*data = no_os_get_unaligned_be32(&desc->data[header_len]);

	return ret;
}

static int spi_register_write(struct standard_spi_desc *desc)
{
	int ret = 0;
	uint32_t header_len = ADIN1110_WR_HDR_SIZE;
	
	struct capi_spi_transfer xfer = {
		.tx_buf = desc->data,
		.rx_buf = desc->data,
		.tx_size = ADIN1110_WR_FRAME_SIZE,
		.rx_size = ADIN1110_WR_FRAME_SIZE,
		.timeout = 0,
		.xfer_delay_clk_cycles = 0,
	};

	addr &= ADIN1110_ADDR_MASK;
	addr |= ADIN1110_CD_MASK | ADIN1110_RW_MASK;
	no_os_put_unaligned_be16(addr, desc->data);

	if (desc->append_crc) {
		desc->data[2] = no_os_crc8(_crc_table, desc->data, 2, 0);
		header_len++;
		xfer.tx_size++;
		xfer.rx_size++;
	}

	no_os_put_unaligned_be32(data, &desc->data[header_len]);
	if (desc->append_crc) {
		desc->data[header_len + ADIN1110_REG_LEN] =
			no_os_crc8(_crc_table, &desc->data[header_len], ADIN1110_REG_LEN, 0);
		xfer.tx_size++;
		xfer.rx_size++;
	}

	if (desc->blocking) {
		ret = capi_spi_transceive(desc->comm_desc, &xfer);
	} else {
		ret = capi_spi_transceive_async(desc->comm_desc, &xfer);
	}
	return ret;
}

static int spi_fifo_read_start(struct standard_spi_desc *desc)
{
	int ret = 0;

	//do the FIFO read start transaction
	//if blocking, wait for the transaction to complete
	//if non-blocking, return and let the interrupt handler handle the rest

	return ret;
}

static int spi_fifo_read_end(struct standard_spi_desc *desc)
{
	int ret = 0;

	//do the FIFO read end transaction
	//if blocking, wait for the transaction to complete
	//if non-blocking, return and let the interrupt handler handle the rest

	return ret;
}

static int spi_fifo_write_start(struct standard_spi_desc *desc)
{
	int ret = 0;

	//do the FIFO write start transaction
	//if blocking, wait for the transaction to complete
	//if non-blocking, return and let the interrupt handler handle the rest
	if (ret != 0)
	{
		desc->spi_state = STANDARD_SPI_STATE_READY;
	}
	else
	{
		desc->spi_state = STANDARD_SPI_STATE_FIFO_WRITE_END;
	}
	return ret;
}

static int spi_fifo_write_end(struct standard_spi_desc *desc)
{
	int ret = 0;

	//do the FIFO write end transaction
	//if blocking, wait for the transaction to complete
	//if non-blocking, return and let the interrupt handler handle the rest

	return ret;
}

void *standard_spi_callback(struct standard_spi_desc *desc)
{
	standard_spi_state_machine(&desc);
}

static int standard_spi_state_machine(struct standard_spi_desc *desc)
{
	int ret = 0;

	switch(desc->spi_state) {
	case STANDARD_SPI_STATE_REGISTER_READ:
	ret = spi_register_read(desc);	
	break;
	case STANDARD_SPI_STATE_REGISTER_WRITE:	
	ret = spi_register_write(desc);
	break;
	case STANDARD_SPI_STATE_FIFO_READ_START:
	ret = spi_fifo_read_start(desc);	
	break;
	case STANDARD_SPI_STATE_FIFO_READ_END:	
	ret	= spi_fifo_read_end(desc);
	break;
	case STANDARD_SPI_STATE_FIFO_WRITE_START:
	ret = spi_fifo_write_start(desc);	
	break;
	case STANDARD_SPI_STATE_FIFO_WRITE_END:
	ret = spi_fifo_write_end(desc);	
	break;
	default:
		/* do nothing */
	break;
	}

	if ((!desc->blocking) || (!desc->pending_control)) {
		//enable_interrupts();
	}
	desc->spi_state = STANDARD_SPI_STATE_READY;
	return ret;
} 
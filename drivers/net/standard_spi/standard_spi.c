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
#include "utilities/net_queue.h"


static uint8_t tx_buf[BUFFERSIZE + 2] __attribute__((aligned(4)));
static uint8_t rx_buf[BUFFERSIZE + 2] __attribute__((aligned(4)));


/* CRC-8/CCITT MSB-first LUT (poly=0x07, seed=0x00) */
static const uint8_t crc8_be_lut[256] = {
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
    0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
    0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
    0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
    0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
    0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
    0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
    0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
    0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
    0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
    0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
    0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
    0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
    0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
    0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
    0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
    0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
    0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
    0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
    0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
    0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
    0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
    0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
    0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
    0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
    0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
    0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

/* CRC-8/CCITT LSB-first (reflected) LUT (poly=0xE0, seed=0x00) */
static const uint8_t crc8_le_lut[256] = {
    0x00, 0x91, 0xE3, 0x72, 0x07, 0x96, 0xE4, 0x75,
    0x0E, 0x9F, 0xED, 0x7C, 0x09, 0x98, 0xEA, 0x7B,
    0x1C, 0x8D, 0xFF, 0x6E, 0x1B, 0x8A, 0xF8, 0x69,
    0x12, 0x83, 0xF1, 0x60, 0x15, 0x84, 0xF6, 0x67,
    0x38, 0xA9, 0xDB, 0x4A, 0x3F, 0xAE, 0xDC, 0x4D,
    0x36, 0xA7, 0xD5, 0x44, 0x31, 0xA0, 0xD2, 0x43,
    0x24, 0xB5, 0xC7, 0x56, 0x23, 0xB2, 0xC0, 0x51,
    0x2A, 0xBB, 0xC9, 0x58, 0x2D, 0xBC, 0xCE, 0x5F,
    0x70, 0xE1, 0x93, 0x02, 0x77, 0xE6, 0x94, 0x05,
    0x7E, 0xEF, 0x9D, 0x0C, 0x79, 0xE8, 0x9A, 0x0B,
    0x6C, 0xFD, 0x8F, 0x1E, 0x6B, 0xFA, 0x88, 0x19,
    0x62, 0xF3, 0x81, 0x10, 0x65, 0xF4, 0x86, 0x17,
    0x48, 0xD9, 0xAB, 0x3A, 0x4F, 0xDE, 0xAC, 0x3D,
    0x46, 0xD7, 0xA5, 0x34, 0x41, 0xD0, 0xA2, 0x33,
    0x54, 0xC5, 0xB7, 0x26, 0x53, 0xC2, 0xB0, 0x21,
    0x5A, 0xCB, 0xB9, 0x28, 0x5D, 0xCC, 0xBE, 0x2F,
    0xE0, 0x71, 0x03, 0x92, 0xE7, 0x76, 0x04, 0x95,
    0xEE, 0x7F, 0x0D, 0x9C, 0xE9, 0x78, 0x0A, 0x9B,
    0xFC, 0x6D, 0x1F, 0x8E, 0xFB, 0x6A, 0x18, 0x89,
    0xF2, 0x63, 0x11, 0x80, 0xF5, 0x64, 0x16, 0x87,
    0xD8, 0x49, 0x3B, 0xAA, 0xDF, 0x4E, 0x3C, 0xAD,
    0xD6, 0x47, 0x35, 0xA4, 0xD1, 0x40, 0x32, 0xA3,
    0xC4, 0x55, 0x27, 0xB6, 0xC3, 0x52, 0x20, 0xB1,
    0xCA, 0x5B, 0x29, 0xB8, 0xCD, 0x5C, 0x2E, 0xBF,
    0x90, 0x01, 0x73, 0xE2, 0x97, 0x06, 0x74, 0xE5,
    0x9E, 0x0F, 0x7D, 0xEC, 0x99, 0x08, 0x7A, 0xEB,
    0x8C, 0x1D, 0x6F, 0xFE, 0x8B, 0x1A, 0x68, 0xF9,
    0x82, 0x13, 0x61, 0xF0, 0x85, 0x14, 0x66, 0xF7,
    0xA8, 0x39, 0x4B, 0xDA, 0xAF, 0x3E, 0x4C, 0xDD,
    0xA6, 0x37, 0x45, 0xD4, 0xA1, 0x30, 0x42, 0xD3,
    0xB4, 0x25, 0x57, 0xC6, 0xB3, 0x22, 0x50, 0xC1,
    0xBA, 0x2B, 0x59, 0xC8, 0xBD, 0x2C, 0x5E, 0xCF
};

int standard_spi_init(struct standard_spi_desc **desc,
		    const struct standard_spi_init_param *param)
{

	return 0;
}

int standard_spi_remove(struct standard_spi_desc *desc)
{
	return 0;
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
    int ret = 0;
    uint32_t fill_count = 0;
    uint32_t  byte_count = desc->byte_size;
    uint8_t  *data = (uint8_t *)desc->data;
    eth_mac_header eth_header;
    uint32_t data_count = 0;
    uint8_t index = 0;
    net_queue rx_queue  = *desc->rx_queue;
    eth_frame_struct *entries   = rx_queue->pEntries;
    uint32_t data_offset  = (uint32_t)SPI_TX_HEADER + 1;

	struct capi_spi_transfer xfer = {
		.timeout = 0,
		.xfer_delay_clk_cycles = 0,
	};

    if (desc == NULL)
        return ADI_ETH_INVALID_HANDLE;

    if (desc->data == NULL) {
        return -EINVAL;
    }

    if (byte_count > (BUFFERSIZE - 2)) {
      return -EINVAL;
    }

    eth_header.CD = (uint16_t)ADI_MAC_SPI_TRANSACTION_CONTROL;
    eth_header.FD = (uint16_t)ADI_MAC_SPI_HALF_DUPLEX;
    eth_header.RW = (uint16_t)ADI_MAC_SPI_READ;
    eth_header.ADDR = desc->register_address;
    tx_buf[index++] = eth_header.VALUE16 >> 8;
    tx_buf[index++] = eth_header.VALUE16 & 0xFF;

    if (desc->is_crc_enabled)
    {
        tx_buf[index] = crc_block8(tx_buf, (uint32_t)ADI_SPI_HEADER_SIZE, 0, false);
        fill_count = byte_count + 1; 
        byte_count = SPI_TX_HEADER + fill_count + 1;
    }
    else
    {
        fill_count = byte_count + 1;
        byte_count = SPI_TX_HEADER + fill_count;
    }

    for (uint32_t i = (byte_count - fill_count); i < byte_count; i++) 
    {
        tx_buf[i] = 0x00;
    }
	/* initialize variables for data processing */
	xfer.tx_buf = tx_buf;
	xfer.rx_buf = rx_buf;
	xfer.tx_size = byte_count;
	xfer.rx_size = byte_count;


	if (desc->blocking) {
		ret = capi_spi_transceive(desc->comm_desc, &xfer);
	} else {
		ret = capi_spi_transceive_async(desc->comm_desc, &xfer);
	}
	if (ret)
		return ret;

    if (desc->blocking) {
        /* 1 is the turnaround byte */
        if (desc->is_crc_enabled) {
            data_offset++;
        }
		for (uint32_t i  = data_offset; i < data_offset + fill_count - 1; i += (ADI_MAC_SPI_ACCESS_SIZE + 1))
		{
			if (rx_buf[i + ADI_MAC_SPI_ACCESS_SIZE] != crc_block8(&rx_buf[i], ADI_MAC_SPI_ACCESS_SIZE, 0, false))
            {
                return -EINVAL;
            }
            *(uint32_t *)&desc->data[data_counter] = *(uint32_t *)&rx_buf[i];
            data_counter += ADI_MAC_SPI_ACCESS_SIZE;
        }
    }
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
    uint32_t fill_count = 0;
    uint32_t  byte_count = desc->byte_size;
    uint8_t  *data = (uint8_t *)desc->data;
    eth_mac_header eth_header;
    uint32_t data_count = 0;
    uint8_t index = 0;
    net_queue rx_queue  = *desc->rx_queue;
    eth_frame_struct *frame_entries   = rx_queue->pEntries;
    uint32_t data_offset  = (uint32_t)SPI_TX_HEADER + 1;

	struct capi_spi_transfer xfer = {
		.timeout = 0,
		.xfer_delay_clk_cycles = 0,
	};

    if (desc == NULL)
        return ADI_ETH_INVALID_HANDLE;

    if (desc->data == NULL) {
        return -EINVAL;
    }

    if (byte_count > (BUFFERSIZE - 2)) {
      return -EINVAL;
    }

    eth_header.CD = (uint16_t)ADI_MAC_SPI_TRANSACTION_CONTROL;
    eth_header.FD = (uint16_t)ADI_MAC_SPI_HALF_DUPLEX;
    eth_header.RW = (uint16_t)ADI_MAC_SPI_READ;
    eth_header.ADDR = desc->register_address;
    tx_buf[index++] = eth_header.VALUE16 >> 8;
    tx_buf[index++] = eth_header.VALUE16 & 0xFF;

    if (desc->is_crc_enabled)
    {
        tx_buf[index] = crc_block8(tx_buf, (uint32_t)ADI_SPI_HEADER_SIZE, 0, false);
        /* Bytes for registers + 1 byte CRC per register + turnaround */
        fill_count = ((byte_count) + (byte_count / ADI_MAC_SPI_ACCESS_SIZE)) + 1;
        byte_count = SPI_TX_HEADER + fill_count + 1;
    }
    else
    {
        fill_count = byte_count + 1;
        byte_count = SPI_TX_HEADER + fill_count;
    }

    for (uint32_t i = (byte_count - fill_count); i < byte_count; i++) 
    {
        tx_buf[i] = 0x00;
    }

	if (net_queue_is_empty(rx_queue)) {
            return -EINVAL;
        }

    if (byte_count > frame_entries[rx_queue->tail].buf_desc->buf_size) {
        return -EINVAL;
    }

	/* initialize variables for data processing */
	xfer.tx_buf = tx_buf;
	xfer.rx_buf = rx_buf;
	xfer.tx_size = byte_count;
	xfer.rx_size = byte_count;
	desc->spi_state = STANDARD_SPI_STATE_FIFO_READ_END;


	if (desc->blocking) {
		ret = capi_spi_transceive(desc->comm_desc, &xfer);
	} else {
		ret = capi_spi_transceive_async(desc->comm_desc, &xfer);
	}
	if (ret)
		return ret;

    if (desc->blocking) {
        /* 1 is the turnaround byte */
        if (desc->is_crc_enabled) {
            data_offset++;
        }
		memcpy(desc->data, &rx_buf[data_offset], byte_count); 

    }
    return ret;
}


static int spi_fifo_read_end(struct standard_spi_desc *desc)
{
    uint32_t tail;
    eth_frame_header   header;
    uint32_t byte_offset;
    uint8_t *rx_buf;
    uint32_t expected_fcs = 0;
    uint8_t timestamp_bytes[ADI_TIMESTAMP_BYTE_SIZE];
    uint32_t actual_fcs;
    mac_rx_fifo_prio prio;
    net_queue *tx_queue;
    net_queue *rx_queue;
    eth_frame_struct *entries;
    (void)arg;

    if (cb_param == NULL)
		return -EINVAL;


    rx_queue  = *desc->pRxQueue;
    tx_queue   = desc->txQueue;

    /* Set state early so queuing more Rx descriptors from callback does not fail due to */
    /* hDevice->spiState still being set to ADI_SPI_STATE_RX_FRAME or ADI_SPI_STATE_TX_FRAME. */
    spi_state = desc->spi_state;
    desc->spi_state = STANDARD_SPI_STATE_READY;
    desc->spi_error = event;

    /* We need to read the header early because we may need to decide which priority queue to use */
    /* "+1" for the turnaround byte*/
    byte_offset = (uint32_t)ADI_SPI_HEADER_SIZE + 1;
	
	if (desc->is_crc_enabled)
		byte_offset++;

        /* Frame header for timestamp information */
    header = *(eth_mac_header *)&desc->rx_buf[byte_offset];
    header.VALUE16 = HTON16(header.VALUE16);

    prio = (mac_rx_fifo_prio)header.PRI;
    entries = rx_queue->pEntries;
    if (desc->rx_queue_hp_en)
    {
		if (prio == MAC_RX_FIFO_PRIO_LOW)
		{
            (*(desc->rx_queue)) = &*(desc->rx_queue_lp);
        }
        else
        {
            (*(desc->rx_queue)) = &*(desc->rx_queue_hp);
        }
    }
	
	if (!net_queue_is_empty(rx_queue)) {

		rx_buf = entries[rx_queue->tail].buff_desc->pBuf;

            /* Adjust buffer size for frame header */
        byte_offset += (uint32_t)ADI_FRAME_HEADER_SIZE;
        entries[rx_queue->tail].buff_desc->trx_size -= (uint32_t)ADI_FRAME_HEADER_SIZE;

         /* Get timestamp and adjust buffer size for Timestamp */
        if (header.TIME_STAMP_PRESENT)
        {
			if (desc->timestampFormat == STANDARD_SPI_TS_FORMAT_64B_1588) {
                /* 64-bit timestamps. */
                entries[rx_queue->tail].buff_desc->timestamp_ext = HTON32((*(uint32_t *)&desc->rx_buf[byte_offset]));
                memcpy(&timestamp_bytes[4], &desc->rx_buf[byte_offset], 4);
                byte_offset += 4;
                    
                entries[rx_queue->tail].buff_desc->timestamp = HTON32((*(uint32_t *)&desc->rx_buf[byte_offset]));
                memcpy(&timestamp_bytes[0], &desc->rx_buf[byte_offset], 4);
                byte_offset += 4;
                    
                entries[rx_queue->tail].buff_desc->trx_size -= 8;

                /* MAC_CalculateParity returns 1 if timestamp_bytes has odd parity, and TIME_STAMP_PARITY is 0 if timestamp_bytes has odd parity. */
                entries[rx_queue->tail].buff_desc->timestamp_valid = (calculate_parity(timestamp_bytes, 8, 0) != header.TIME_STAMP_PARITY);
            }
            else {
                    /* 32-bit timestamps. */
                entries[rx_queue->tail].buff_desc->timestamp_ext = 0;
                entries[rx_queue->tail].buff_desc->timestamp = HTON32((*(uint32_t *)&desc->rx_buf[byte_offset]));
                memcpy(&timestamp_bytes[0], &desc->rx_buf[byte_offset], 4);

                byte_offset += 4;
                entries[rx_queue->tail].buff_desc->trx_size -= 4;

                    /* MAC_CalculateParity returns 1 if timestamp_bytes has odd parity, and TIME_STAMP_PARITY is 0 if timestamp_bytes has odd parity. */
                entries[rx_queue->tail].buff_desc->timestamp_valid = (calculate_parity(timestamp_bytes, 4, 0) != header.TIME_STAMP_PARITY);
            }
        }

        memcpy(rx_buf, (const uint8_t *)&desc->rx_buf[byte_offset], entries[rx_queue->tail].buff_desc->trx_size);

        /* Adjust buffer size for FCS */
        entries[rx_queue->tail].buff_desc->trx_size -= FCS_SIZE;
        entries[rx_queue->tail].buff_desc->prio = prio;
		
		if (desc->device == ADIN_2111)
			entries[rx_queue->tail].buff_desc->port = header.PORT;
    

        if (desc->fcsCheckEn)
        {
            memcpy(&actual_fcs, &rx_buf[entries[rx_queue->tail].buff_desc->trx_size], FCS_SIZE);
            expected_fcs = calculate_fcs(desc->appDevice, rx_buf, entries[rx_queue->tail].buff_desc->trx_size);

            if (expected_fcs != actual_fcs)
            {
                Event |= 1 << 0; /* FCS error */
            }
        }
        tail = rx_queue->tail;
        queue_removeEntry(rx_queue);

        if ((desc->device == ADIN_2111) && (desc->cb_func[ADI_MAC_EVT_DYN_TBL_UPDATE]))
			desc->cb_func[ADI_MAC_EVT_DYN_TBL_UPDATE]((desc->appDevice), Event, entries[tail].buff_desc);
                
        if (entries[tail].buff_desc->cb_func)
			entries[tail].buff_desc->cb_func((desc->appDevice), Event, entries[tail].buff_desc);
            
    }
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


static uint8_t crc8_le(uint8_t crc, uint8_t *buf, uint32_t numbytes, uint32_t numbits)
{
    for (uint32_t i = 0; i < numbytes; i++)
        crc = crc8_le_lut[crc ^ buf[i]];

    /* Handle any trailing partial byte */
    if (numbits > 0)
        crc = crc8_le_lut[crc ^ (buf[numbytes] & ((1u << numbits) - 1u))];

    return crc;
}

static uint8_t crc8_be(uint8_t crc, uint8_t *buf, uint32_t numbytes, uint32_t numbits)
{
    for (uint32_t i = 0; i < numbytes; i++)
        crc = crc8_be_lut[crc ^ buf[i]];

    /* Handle any trailing partial byte */
    if (numbits > 0)
        crc = crc8_be_lut[crc ^ (buf[numbytes] >> (8u - numbits) << (8u - numbits))];

    return crc;
}

static uint8_t crc_block8(uint8_t *start_addr, uint32_t byte_size, uint32_t bit_size, bool lsb_first)
{
    /* Initialize CRC with the same seed as in TestCrc */
    uint8_t crc = 0x00;

    if (lsb_first)
    {
        return crc8_le(crc, start_addr, byte_size, bit_size);
    }
    else
    {
        return crc8_be(crc, start_addr, byte_size, bit_size);
    }
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
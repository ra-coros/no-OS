/*
 *---------------------------------------------------------------------------
 *
 * Copyright (c) 2020 - 2025 Analog Devices, Inc. All Rights Reserved.
 * This software is proprietary to Analog Devices, Inc.
 * and its licensors.By using this software you agree to the terms of the
 * associated Analog Devices Software License Agreement.
 *
 *---------------------------------------------------------------------------
 */
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "utilities.h"
#include "spi.h"
#include "capi_spi.h"
#include "capi_irq.h"
#include "stm32_capi_spi.h"
#include "stm32_capi_irq.h"

#include "standard_spi.h"

/* Transmit data buffer 2 bytes overhead for command*/
DMA_BUFFER_ALIGN(uint8_t spi_rx_buf[BUFFERSIZE + 2], 4);
DMA_BUFFER_ALIGN(uint8_t spi_tx_buf[BUFFERSIZE + 2], 4);

static int standard_spi_write_transfer(struct standard_spi_desc *desc,
									   uint16_t regAddr, void *buf,
									   uint32_t nBytes, bool blocking);
static int standard_spi_read_transfer(struct standard_spi_desc *desc,
									  uint16_t regAddr, void *buf,
									  uint32_t nBytes, bool blocking);
static uint8_t standard_spi_crc_block8(uint8_t *StartAddr, uint32_t NumBytes,
									   uint32_t NumBits, bool bLsbFirst);

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
					  struct standard_spi_init_param *param)
{
		int    ret = 0;
		struct standard_spi_desc   *d;

        if (desc == NULL || param == NULL) 
                return ETH_INVALID_HANDLE;

        if (param->dev_mem_size < sizeof(struct standard_spi_desc))
                return ETH_INVALID_PARAM;
        
        if (param->dev_mem == NULL) {
                *desc = (struct standard_spi_desc *)calloc(1, sizeof(**desc));
                if (!*desc)
                        return -ENOMEM;
        } else {
                if (param->dev_mem_size < sizeof(**desc))
                        return -ENOMEM;
        
                memset(param->dev_mem, 0, param->dev_mem_size);
                *desc = (struct standard_spi_desc *)param->dev_mem;
        }
		
        d = *desc;

        /* Implies state is uninitialized */
        d->comm_desc = param->comm_param;
        d->rx_queue_hp_en = param->rx_queue_hp_en;
        d->fcs_check_en = param->fcs_check_en;
        d->crc_en = param->crc_en;
        d->num_ports = param->num_ports;
        d->eth_irq_num = param->eth_irq_num;

        d->ts_format = STANDARD_SPI_TS_FORMAT_NONE;

        d->spi_state = STANDARD_SPI_STATE_READY;
        d->spi_error = 0;

        return ret;
}

int standard_spi_remove(struct standard_spi_desc *desc)
{
		if (!desc)
				return -EINVAL;

		desc->spi_state = STANDARD_SPI_STATE_READY;
		desc->comm_desc = NULL;
		memset(desc, 0, sizeof(struct standard_spi_desc));
		capi_free(desc);
		return 0;
}

int is_spi_ready(struct standard_spi_desc *desc)
{
		/* Already ready, return immediately */
		if (desc->spi_state == STANDARD_SPI_STATE_READY)
				return 0;

		/* Wait for SPI state to be ready*/
		uint32_t startTime = HAL_GetTick();
		while (desc->spi_state != STANDARD_SPI_STATE_READY) {
				/* Check if timeout is greater than 50
				   communication timeout. Otherwise, the SPI state is ready. */
				if ((HAL_GetTick() - startTime) >= 50)
						return ETH_COMM_TIMEOUT;
		}
		
		return 0;
}

int standard_spi_write(struct standard_spi_desc *desc, 
					   enum standard_spi_state spi_state, uint16_t regAddr,
					   void *buf, uint32_t nBytes, bool blocking)
{
		int ret = 0;

		if (desc == NULL)
				return ETH_INVALID_HANDLE;

		if (buf == NULL)
				return ETH_INVALID_PARAM;

		if ((spi_state == STANDARD_SPI_STATE_CONTROL_START) || 
			(spi_state == STANDARD_SPI_STATE_DATA_START)) {		
				ret = standard_spi_write_transfer(desc, regAddr, buf, nBytes, 
											  blocking);
		} else {
				ret = ETH_INVALID_PARAM;
		}
	return ret;
}

int standard_spi_read(struct standard_spi_desc *desc, 
		enum standard_spi_state spi_state, uint16_t regAddr, void *buf,
		uint32_t nBytes,  bool blocking)
{
        int ret = 0;

        if (desc == NULL)
                return ETH_INVALID_HANDLE;

        if (buf == NULL)
                return ETH_INVALID_PARAM;

        if (spi_state == STANDARD_SPI_STATE_CONTROL_START || 
            spi_state == STANDARD_SPI_STATE_DATA_START)
                ret = standard_spi_read_transfer(desc, regAddr, buf, nBytes,
												 blocking);
        else
                ret = ETH_INVALID_PARAM;

        if (ret != 0) 
        {
				desc->spi_state = STANDARD_SPI_STATE_READY;
				if (!blocking)
						capi_irq_enable(desc->eth_irq_num);
        }
        return ret;
}

int standard_spi_write_frame(struct standard_spi_desc *desc, 
		struct eth_frame_struct *frame, bool blocking)
{	
		int ret = 0;
		uint32_t byte_count;
		eth_spi_header spi_header;
		uint32_t byte_idx = 0;
		uint32_t frame_header_size = (uint32_t)ADI_FRAME_HEADER_SIZE;
		uint32_t spi_header_size = (uint32_t)ADI_SPI_HEADER_SIZE;
		struct capi_spi_transfer xfer;

		if (desc == NULL)
				return ETH_INVALID_HANDLE;

		/* Check if the SPI master is already in use */
		/* If it is, return and retry later. */
		if (desc->spi_state != STANDARD_SPI_STATE_READY)
				return ETH_COMM_BUSY;

		desc->spi_state = STANDARD_SPI_STATE_TX_FRAME;

		byte_count = frame->buf_desc->trx_size + frame_header_size +
					 spi_header_size;
		/* If SPI CRC is enabled, we have an extra byte for CRC. */
		if (desc->crc_en)
				byte_count++;

		if (byte_count > BUFFERSIZE)
				return ETH_PARAM_OUT_OF_RANGE;

		/* SPI header, this is placed in the first 2 bytes of the transmit buffer */
		spi_header.CD = ADI_MAC_SPI_TRANSACTION_CONTROL;
		spi_header.FD = ADI_MAC_SPI_HALF_DUPLEX;
		spi_header.RW = ADI_MAC_SPI_WRITE;
		spi_header.ADDR = 0x031; /* SPI FIFO write address */

		spi_tx_buf[byte_idx++] = spi_header.VALUE16 >> 8;
		spi_tx_buf[byte_idx++] = spi_header.VALUE16 & 0xFF;

		/* If SPI CRC is enabled, add it for the SPI header. Note there is no CRC */
		/* for rest of the transaction (frame header + frame). */
		if (desc->crc_en) {
			spi_tx_buf[byte_idx++] = standard_spi_crc_block8(&spi_tx_buf[0], 
									(uint32_t)ADI_SPI_HEADER_SIZE, 0, false);
		}

		/* Append frame header */
		spi_tx_buf[byte_idx++] = frame->header.VALUE16 >> 8;
		spi_tx_buf[byte_idx++] = frame->header.VALUE16 & 0xFF;

		/* Copy the frame contents to the SPI transmit buffer. */
		memcpy(&spi_tx_buf[byte_idx], frame->buf_desc->buf, 
			   frame->buf_desc->trx_size);

		/* Odd-size frames need an extra byte */
		if (frame->buf_desc->trx_size & 1)
				byte_count++;
		
		 /* initialize parameters */
		xfer.tx_buf = spi_tx_buf;
		xfer.rx_buf = spi_rx_buf;
		xfer.tx_size = byte_count;
		xfer.rx_size = byte_count;
		xfer.timeout = 0;
		xfer.xfer_delay_clk_cycles = 0;

		if (blocking)
				ret = capi_spi_transceive(desc->comm_desc, &xfer);
		else if (byte_count >= MIN_SIZE_FOR_DMA)
				ret = capi_spi_transceive_dma_async(desc->comm_desc, &xfer);
		else
				ret = capi_spi_transceive_async(desc->comm_desc, &xfer);

		return ret;
}

int standard_spi_get_status(struct standard_spi_desc *desc, 
		uint8_t *backup, bool bIsCtrl)
{
		int ret = 0;

		if (desc == NULL)
				return ETH_INVALID_HANDLE;

		if (bIsCtrl) {
				struct critical_state basepri = 
						adi_hal_enter_critical_section(HAL_INT_PRI_DMA_SPI_RX);
				
				if (desc->spi_state == STANDARD_SPI_STATE_READY)
						*backup = capi_irq_get_enable(desc->eth_irq_num);

				capi_irq_disable(desc->eth_irq_num);
				*(desc->pending_control) = true;
				
				adi_hal_exit_critical_section(basepri);

				if (desc->spi_state != STANDARD_SPI_STATE_READY)
						ret = ETH_COMM_TIMEOUT;
			
		} else {
				if (desc->spi_state != STANDARD_SPI_STATE_READY)
						ret = ETH_COMM_BUSY;
		}
		return ret;
}


static uint8_t crc8_le(uint8_t crc, uint8_t *buf, uint32_t numbytes,
		uint32_t numbits, uint8_t poly)
{
		(void)poly;
		for (uint32_t i = 0; i < numbytes; i++)
				crc = crc8_le_lut[crc ^ buf[i]];

		/* Handle any trailing partial byte */
		if (numbits > 0)
				crc = crc8_le_lut[crc ^ (buf[numbytes] & ((1u << numbits) - 1u))];

		return crc;
}

static uint8_t crc8_be(uint8_t crc, uint8_t *buf, uint32_t numbytes,
		uint32_t numbits, uint8_t poly)
{
		(void)poly;
		for (uint32_t i = 0; i < numbytes; i++)
				crc = crc8_be_lut[crc ^ buf[i]];

		/* Handle any trailing partial byte */
		if (numbits > 0)
				crc = crc8_be_lut[crc ^ (buf[numbytes] >> (8u - numbits) <<
									(8u - numbits))];

		return crc;
}

static uint8_t standard_spi_crc_block8(uint8_t *StartAddr, uint32_t NumBytes,
		uint32_t NumBits, bool bLsbFirst)
{
		/* Initialize CRC with the same seed as in TestCrc */
		uint8_t crc = 0x00;

		if (bLsbFirst) {
				return crc8_le(crc, StartAddr, NumBytes, NumBits, 
					CRC8CCITT_POLYNOMIAL_LE);
		} else {
				return crc8_be(crc, StartAddr, NumBytes, NumBits, 
					CRC8CCITT_POLYNOMIAL_BE);
		}
}

void standard_spi_callback(enum capi_async_event event, void *arg, int event_extra)
{
		struct standard_spi_desc *desc;
		uint32_t tail;
		struct eth_frame_header header;
		uint32_t byte_offset;
		uint8_t  *pRxBuf;
		enum standard_spi_state spi_state;
		uint32_t expectedFcs = 0;
		uint8_t timestampBytes[ADI_TIMESTAMP_BYTE_SIZE];
		uint32_t actualFcs;
		enum mac_rx_fifo_prio prio;
		struct net_queue *tx_queue;
		struct net_queue *rx_queue;
		struct eth_frame_struct *entries;
    
		(void)event_extra;
	
        desc = (struct standard_spi_desc *)arg;
	
		if (event != CAPI_SPI_EVENT_XFR_DONE)
                desc->spi_error = 1;
		else
                desc->spi_error = 0;
			   
		rx_queue  = *desc->rx_queue;
		tx_queue   = desc->tx_queue;

		spi_state = desc->spi_state;
		desc->spi_state = STANDARD_SPI_STATE_READY;

		if (spi_state == STANDARD_SPI_STATE_RX_FRAME) {
				/* We need to read the header early because we may need 
				   to decide which priority queue to use "+1" for the turnaround byte */
				byte_offset = (uint32_t)ADI_SPI_HEADER_SIZE + 1;

				if (desc->crc_en)
						byte_offset++;

				/* Frame header for timestamp information */
				header = *(struct eth_frame_header *)&spi_rx_buf[byte_offset];
				header.VALUE16 = HTON16(header.VALUE16);

				prio = (enum mac_rx_fifo_prio)header.PRI;
				entries = rx_queue->entries;
				if (desc->rx_queue_hp_en) {
						/* The transaction size is recorded in the current active queue, we need to save that */
						/* and restore in case we switch queues.                                              */
						uint32_t trx_size_backup = entries[rx_queue->tail].buf_desc->trx_size;
						if (prio == MAC_RX_FIFO_PRIO_LOW)
							(*(desc->rx_queue)) = &*(desc->rx_queue_lp);
						else
							*(desc->rx_queue) = &*(desc->rx_queue_hp);
						
						entries[rx_queue->tail].buf_desc->trx_size = trx_size_backup;
				}

				if (!net_queue_is_empty(rx_queue)) {
						pRxBuf = entries[rx_queue->tail].buf_desc->buf;

						/* Adjust buffer size for frame header */
						byte_offset += (uint32_t)ADI_FRAME_HEADER_SIZE;
						entries[rx_queue->tail].buf_desc->trx_size -= (uint32_t)ADI_FRAME_HEADER_SIZE;

						/* Get timestamp and adjust buffer size for Timestamp */
						if (header.TIME_STAMP_PRESENT) {
							if (desc->ts_format == STANDARD_SPI_TS_FORMAT_64B_1588) {
									/* 64-bit timestamps. */
									entries[rx_queue->tail].buf_desc->timestamp_ext =
									HTON32((*(uint32_t *)&spi_rx_buf[byte_offset]));
									
									memcpy(&timestampBytes[4], &spi_rx_buf[byte_offset], 4);
									byte_offset += 4;
									
									entries[rx_queue->tail].buf_desc->timestamp = HTON32((*(uint32_t *)&spi_rx_buf[byte_offset]));
									memcpy(&timestampBytes[0], &spi_rx_buf[byte_offset], 4);
									byte_offset += 4;
									
									entries[rx_queue->tail].buf_desc->trx_size -= 8;

									/* MAC_CalculateParity returns 1 if timestampBytes has odd parity, 
									and TIME_STAMP_PARITY is 0 if timestampBytes has odd parity. */
									entries[rx_queue->tail].buf_desc->timestamp_valid = 
									(calculate_parity(timestampBytes, 8, 0) != header.TIME_STAMP_PARITY);
							} else {
									/* 32-bit timestamps. */
									entries[rx_queue->tail].buf_desc->timestamp_ext = 0;
									entries[rx_queue->tail].buf_desc->timestamp = 
									HTON32((*(uint32_t *)&spi_rx_buf[byte_offset]));
									
									memcpy(&timestampBytes[0], &spi_rx_buf[byte_offset], 4);

									byte_offset += 4;
									entries[rx_queue->tail].buf_desc->trx_size -= 4;

									/* MAC_CalculateParity returns 1 if timestampBytes has odd parity,
									   and TIME_STAMP_PARITY is 0 if timestampBytes has odd parity. */
									entries[(*(desc->rx_queue))->tail].buf_desc->timestamp_valid = 
									(calculate_parity(timestampBytes, 4, 0) != header.TIME_STAMP_PARITY);
							}
					}

					memcpy(pRxBuf, (const uint8_t *)&spi_rx_buf[byte_offset], 
					entries[rx_queue->tail].buf_desc->trx_size);

					/* Adjust buffer size for FCS */
					entries[rx_queue->tail].buf_desc->trx_size -= STD_FRAME_FCS_SIZE;

					entries[rx_queue->tail].buf_desc->prio = prio;

					if (desc->num_ports == 2) 
							entries[rx_queue->tail].buf_desc->port = header.PORT;

					if (desc->fcs_check_en) {
							memcpy(&actualFcs, 
							&pRxBuf[entries[rx_queue->tail].buf_desc->trx_size],
							STD_FRAME_FCS_SIZE);
							
							expectedFcs = calculate_fcs(pRxBuf,
							entries[rx_queue->tail].buf_desc->trx_size);
							
							if (expectedFcs != actualFcs)
									desc->spi_error |= ADI_MAC_CALLBACK_STATUS_FCS_ERROR;
					}
					tail = rx_queue->tail;
					net_queue_remove_entry(rx_queue);

					if (desc->num_ports ==  2) {
							/* Update the dynamic forwarding table */
							if (desc->cb_func[ADI_MAC_EVT_DYN_TBL_UPDATE] != NULL) {
								desc->cb_func[ADI_MAC_EVT_DYN_TBL_UPDATE]((desc->app_device),
								desc->spi_error, entries[tail].buf_desc);
							}
					}

					if (entries[tail].buf_desc->cb_func) {
							entries[tail].buf_desc->cb_func((desc->app_device), 
															desc->spi_error,
							entries[tail].buf_desc);
					}
				}

				desc->spi_state = STANDARD_SPI_STATE_READY;
				
				if (!net_queue_is_empty(tx_queue))
						capi_irq_set_pending(desc->eth_irq_num);

		}
		else
		{
			if (spi_state == STANDARD_SPI_STATE_TX_FRAME)
			{
					tail = desc->tx_queue->tail;
					net_queue_remove_entry(&*(desc->tx_queue));
					entries = tx_queue->entries;

					/* Decrement the reference count, and call the callback  
					   function only if the reference count is 0. This ensures 
					   that if the intent was to send the buffer to both ports, it
					   will be returned to the buffer pool only after sending to 
						both ports has completed.  								   */
					entries[tail].buf_desc->ref_count--;
					if (entries[tail].buf_desc->cb_func && 
						(!entries[tail].buf_desc->ref_count)) {
						entries[tail].buf_desc->cb_func((desc->app_device),
														desc->spi_error, 
						entries[tail].buf_desc);
					}

					/* If more TX frames are queued, force an INT-N re-entry
					   immediately rather than waiting for the ADIN to reassert TX_RDY. 
					   This drains the TX queuet the maximum SPI rate and prevents the 
					   main-loop busy-wait from firing. */
					if (!net_queue_is_empty(tx_queue))
							capi_irq_set_pending(desc->eth_irq_num);
			}
		}

		if (!*(desc->pending_control))
				capi_irq_enable(desc->eth_irq_num);
}

static int standard_spi_write_transfer(struct standard_spi_desc *desc, 
		uint16_t regAddr, void *buf, uint32_t nBytes, bool blocking)
{	
		int ret = 0;
		uint8_t index = 0;
		uint32_t byte_offset = (uint32_t)ADI_SPI_HEADER_SIZE;
		uint32_t byte_count = nBytes;
		uint8_t *pData = (uint8_t *)buf;
		eth_spi_header spi_header;
		uint8_t tmpcrc = 0;
		struct capi_spi_transfer xfer;
		
		if (desc == NULL)
				return ETH_INVALID_HANDLE;

		if (byte_count > BUFFERSIZE || buf == NULL)
				return ETH_INVALID_PARAM;

		if (desc->spi_state != STANDARD_SPI_STATE_READY)
				return ETH_COMM_BUSY;

		/* Create the SPI header and populate it at the start of the 
		   SPI Tx buffer */
		spi_header.CD = ADI_MAC_SPI_TRANSACTION_CONTROL;
		spi_header.FD = ADI_MAC_SPI_HALF_DUPLEX;
		spi_header.RW = ADI_MAC_SPI_WRITE;
		spi_header.ADDR = regAddr;
		spi_tx_buf[index++] = spi_header.VALUE16 >> 8;
		spi_tx_buf[index++] = spi_header.VALUE16 & 0xFF;

		if (desc->crc_en) {
			spi_tx_buf[index] = standard_spi_crc_block8(spi_tx_buf, 
			uint32_t)ADI_SPI_HEADER_SIZE, 0, false);
			
			if(regAddr != ADDR_MAC_TX) {
				byte_count = ((nBytes) + (nBytes / ADI_MAC_SPI_ACCESS_SIZE)) + 1;
				
				for (uint32_t i = 1, j = 0, chunkCount = 0; i < byte_count; i++) {
					if (chunkCount == ADI_MAC_SPI_ACCESS_SIZE) {
						tmpcrc = standard_spi_crc_block8((pData + 
								(j - ADI_MAC_SPI_ACCESS_SIZE)), 
								ADI_MAC_SPI_ACCESS_SIZE, 0, false);
						spi_tx_buf[i + byte_offset] = tmpcrc;
						chunkCount = 0;
					} else {
						spi_tx_buf[i + byte_offset] = pData[j++];
						chunkCount++;
					}
				}
			}
			else {
				byte_count = nBytes;
				byte_offset += 1;
				memcpy(&spi_tx_buf[byte_offset], pData, nBytes);
			}
		} else {
			for (uint32_t i = 0u; i < byte_count  ; i++) {
				spi_tx_buf[i + byte_offset] = pData[i];
			}      
		}

		byte_count = byte_count + byte_offset;

		if (regAddr == ADDR_MAC_TX)
				desc->spi_state = STANDARD_SPI_STATE_TX_FRAME;
		else
				desc->spi_state = STANDARD_SPI_STATE_TX;  
		
		/* initialize parameters */
		xfer.tx_buf = spi_tx_buf;
		xfer.rx_buf = spi_rx_buf;
		xfer.tx_size = byte_count;
		xfer.rx_size = byte_count;
		xfer.timeout = 0;
		xfer.xfer_delay_clk_cycles = 0;

		/* Determine if it's worth using DMA based on the transaction size. */
		if (blocking)
				ret = capi_spi_transceive(desc->comm_desc, &xfer);
		else if (byte_count >= MIN_SIZE_FOR_DMA)
				ret = capi_spi_transceive_dma_async(desc->comm_desc, &xfer);
		else
				ret = capi_spi_transceive_async(desc->comm_desc, &xfer);

		return ret;
}

static int standard_spi_read_transfer(struct standard_spi_desc *desc,
		uint16_t regAddr, void *buf, uint32_t nBytes, bool blocking)
{
		int ret = 0;
		uint32_t fillCount = 0;
		uint32_t byte_count = nBytes;
		uint8_t *pData = (uint8_t *)buf;
		eth_spi_header spi_header;
		uint32_t dataCounter = 0;
		uint8_t index = 0;
		struct net_queue *rx_queue = *desc->rx_queue;
		struct eth_frame_struct *entries = rx_queue->entries;
		uint32_t dataOffset = (uint32_t)SPI_TX_HEADER + 1;

		struct capi_spi_transfer xfer;
		
		if (desc == NULL)
				return ETH_INVALID_HANDLE;

		if (buf == NULL)
				return ETH_INVALID_PARAM;
		
		/* Todo: Should this account for CRC/turnaround bytes? */
		if (byte_count > (BUFFERSIZE - 2))
				return ETH_INVALID_PARAM;
		
		if (desc->spi_state != STANDARD_SPI_STATE_READY)
				return ETH_COMM_BUSY;

		spi_header.CD = (uint16_t)ADI_MAC_SPI_TRANSACTION_CONTROL;
		spi_header.FD = (uint16_t)ADI_MAC_SPI_HALF_DUPLEX;
		spi_header.RW = (uint16_t)ADI_MAC_SPI_READ;
		spi_header.ADDR = regAddr;
		spi_tx_buf[index++] = spi_header.VALUE16 >> 8;
		spi_tx_buf[index++] = spi_header.VALUE16 & 0xFF;

		if (desc->crc_en) {
			spi_tx_buf[index] = standard_spi_crc_block8(spi_tx_buf, 
								(uint32_t)ADI_SPI_HEADER_SIZE, 0, false);
			if((regAddr != ADDR_MAC_P1_RX) && (regAddr != ADDR_MAC_P2_RX)) {
					/* Bytes for registers + 1 byte CRC per register + turnaround */
					fillCount = ((nBytes) + (nBytes / ADI_MAC_SPI_ACCESS_SIZE)) + 1;
			}
			else {
					fillCount = nBytes + 1; 
			}
			byte_count = SPI_TX_HEADER + fillCount + 1;
		} else {
				fillCount = nBytes + 1;
				byte_count = SPI_TX_HEADER + fillCount;
		}

		for (uint32_t i = (byte_count - fillCount); i < byte_count; i++) {
			spi_tx_buf[i] = 0x00;
		}


		if ((regAddr == ADDR_MAC_P1_RX) || (regAddr == ADDR_MAC_P2_RX )) {			
			if (net_queue_is_empty(rx_queue))
					return ETH_QUEUE_EMPTY;

			if (nBytes > entries[rx_queue->tail].buf_desc->buf_size)
					return ETH_BUFFER_TOO_SMALL;
			
			desc->spi_state = STANDARD_SPI_STATE_RX_FRAME;
		} else {
				desc->spi_state = STANDARD_SPI_STATE_RX;
		}
		
		/* initialize parameters */
		xfer.tx_buf = spi_tx_buf;
		xfer.rx_buf = spi_rx_buf;
		xfer.tx_size = byte_count;
		xfer.rx_size = byte_count;
		xfer.timeout = 0;
		xfer.xfer_delay_clk_cycles = 0;

		/* Determine if it's worth using DMA based on the transaction size. */
		if (blocking)
				ret = capi_spi_transceive(desc->comm_desc, &xfer);
		else if (byte_count >= MIN_SIZE_FOR_DMA)
				ret = capi_spi_transceive_dma_async(desc->comm_desc, &xfer);
		else
				ret = capi_spi_transceive_async(desc->comm_desc, &xfer);

		if (ret) 
				return ret;

		if (blocking) {
			/* 1 is the turnaround byte */
			if (desc->crc_en)
					dataOffset++;

			if ((regAddr == ADDR_MAC_P1_RX) || (regAddr == ADDR_MAC_P2_RX)
				|| ((!desc->crc_en))) {
				memcpy(pData, &spi_rx_buf[dataOffset], nBytes);             
			} else {
					for (uint32_t i  = dataOffset; i < dataOffset + fillCount - 1;
						i += (ADI_MAC_SPI_ACCESS_SIZE + 1)) {
							if (spi_rx_buf[i + ADI_MAC_SPI_ACCESS_SIZE] != 
								standard_spi_crc_block8(&spi_rx_buf[i], 
								ADI_MAC_SPI_ACCESS_SIZE, 0, false)) {
									return ETH_CRC_ERROR;
							}
							*(uint32_t *)&pData[dataCounter] = *(uint32_t *)&spi_rx_buf[i];
							dataCounter += ADI_MAC_SPI_ACCESS_SIZE;
					}
			}
		}
		return ret;
}
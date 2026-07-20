/*******************************************************************************
 *   @file   oa_tc6.c
 *   @brief  Implementation for the Open Alliance TC6 SPI driver.
 *   @author Ciprian Regus (ciprian.regus@analog.com)
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

#include "capi_alloc.h"
#include "oa_tc6.h"

/*! SPI receive data buffer. */
DMA_BUFFER_ALIGN(static uint8_t spi_rx_buf[BUFFER_SIZE], 4);
/*! SPI transmit data buffer. */
DMA_BUFFER_ALIGN(static uint8_t spi_tx_buf[BUFFER_SIZE], 4);

static int oa_tc6_state_machine (struct oa_tc6_desc *desc);
static int oa_tc6_start_ctrl_transaction (struct oa_tc6_desc *const desc);
static int oa_tc6_end_ctrl_transaction (struct oa_tc6_desc *const desc);
static int oa_tc6_start_data_transaction (struct oa_tc6_desc *const desc);
static int oa_tc6_end_data_transaction (struct oa_tc6_desc *const desc);
static int oa_tc6_start_irq(struct oa_tc6_desc *const desc);

static int oa_tc6_spi_process(struct oa_tc6_desc *const desc);
static int oa_tc6_create_next_chunk(struct oa_tc6_desc *const desc, uint8_t *buf, bool tx_en);

static void oa_tc6_ctrl_cmd_header(uint8_t *dst_buf, uint32_t wnr,uint32_t addr, uint32_t cnt);
static int oa_tc6_ctrl_cmd_read_data(uint32_t *dst, uint8_t *src, uint32_t cnt, bool prote_spi);
static void oa_tc6_ctrl_cmd_write_data(uint8_t *dst, uint32_t *src, uint32_t cnt, bool prote_spi);

static bool oa_tc6_chunk_error_detector(struct oa_tc6_desc *const desc, uint32_t rx_footer);
static void oa_tc6_timestamp_handler(struct oa_tc6_desc *const desc, uint32_t spi_rx_buf_byte_offset, uint32_t *frame_dest, uint8_t *timestamp_bytes, uint32_t *byte_offset, enum oa_tc6_ts_operation bit_operation);
static void oa_tc6_fcs_checker(struct oa_tc6_desc *const desc, uint32_t fcs_size, uint8_t *rx_buf, uint32_t *event);
static void oa_tc6_cb_function_caller(struct oa_tc6_desc *const desc, uint32_t tail, uint32_t event, uint8_t num_ports);
static void oa_tc6_end_data_chunk_processor(struct oa_tc6_desc *const desc, uint32_t *event, uint8_t *rx_buf, uint32_t oa_rx_footer, uint32_t chunk_start);
static void oa_tc6_process_64_bit_timestamp(struct oa_tc6_desc *const desc, uint32_t chunk_start, uint32_t chunk_size, uint8_t *timestamp_bytes, uint32_t *byte_offset);
static void oa_tc6_full_frame_in_chunk_process(struct oa_tc6_desc *const desc, uint32_t *event, uint32_t fcs_size, uint8_t *rx_buf);
static void oa_tc6_start_data_chunk_processor(struct oa_tc6_desc *desc, uint32_t *event, uint32_t *byte_offset, uint32_t chunk_start, uint8_t *timestamp_bytes, uint32_t chunk_size, uint8_t *rx_buf, uint32_t oa_rx_footer);
static void oa_tc6_mid_data_chunk_processor(struct oa_tc6_desc *const desc, uint32_t chunk_size, uint32_t oa_rx_footer, uint8_t *rx_buf, uint32_t *event, uint32_t byte_offset, uint32_t chunk_start);
static void oa_tc6_complete_transmission_checker(struct oa_tc6_desc *const desc, uint32_t tx_header, uint32_t *event);

static int oa_tc6_get_first_tx_frame(struct oa_tc6_desc *desc, struct oa_tc6_frame_buffer **buffer);
static int oa_tc6_get_empty_rx_buff(struct oa_tc6_desc *desc, struct oa_tc6_frame_buffer **buffer, bool new_buffer);


static int oa_tc6_ctrl_setup(uint8_t *buf, uint32_t wnr, uint32_t reg_addr, 
        uint32_t *reg_data, uint32_t *len, bool prote_spi);


int oa_rx_chunk_to_frame(struct oa_tc6_desc *desc, uint8_t *chunks,
			 uint32_t len);

/**
 * @brief Compute the CRC value of the header/footer.
 * @param header - value of the header/footer field in a chunk.
 * @return CRC value.
 */
static uint8_t oa_tc6_crc1(uint32_t header)
{
	uint8_t p = 1;

	while (header) {
		p ^= header & 0x1;
		header >>= 1;
	}

	return p;
}

/**
 * @brief Prepare a register read control chunk.
 * @param desc - the OA TC6 descriptor.
 * @param addr - Register address.
 */
static void oa_tc6_prepare_rx_ctrl(struct oa_tc6_desc *desc, uint32_t addr)
{
	uint32_t header;

	header = no_os_field_prep(OA_CTRL_ADDR_MMS_MASK, addr);
	header |= no_os_field_prep(OA_CTRL_HEADER_AID_MASK, 1);
	header |= oa_tc6_crc1(header);

	no_os_put_unaligned_be32(header, desc->ctrl_chunks);
	desc->ctrl_rx_credit++;
}

/**
 * @brief Prepare a register write control chunk.
 * @param desc - the OA TC6 descriptor.
 * @param addr - Register address.
 * @param val - Register value.
 */
static void oa_tc6_prepare_tx_ctrl(struct oa_tc6_desc *desc, uint32_t addr,
				   uint32_t val)
{
	uint32_t header;

	header = no_os_field_prep(OA_CTRL_ADDR_MMS_MASK, addr);
	header |= no_os_field_prep(OA_CTRL_HEADER_AID_MASK, 1);
	header |= no_os_field_prep(OA_CTRL_HEADER_WNR_MASK, 1);
	header |= oa_tc6_crc1(header);

	no_os_put_unaligned_be32(header, desc->ctrl_chunks);
	no_os_put_unaligned_be32(val, &desc->ctrl_chunks[OA_HEADER_LEN]);

	if (desc->prote_spi) {
		no_os_put_unaligned_be32(val ^ NO_OS_GENMASK(31, 0),
					 &desc->ctrl_chunks[OA_HEADER_LEN + OA_REG_LEN]);
	}

	desc->ctrl_tx_credit++;
}

/**
 * @brief       Creates the control command header for burst read/write.
 *
 * @param [in]  dst_buf Pointer to the destination buffer for the header.
 * @param [in]  wnr     Transaction type.
 * @param [in]  addr    Register address.
 * @param [in]  cnt     Number of registers.
 *
 */
static void oa_tc6_ctrl_cmd_header(uint8_t *dst_buf, uint32_t wnr,
                                   uint32_t addr, uint32_t cnt)
{
        uint32_t header = 0;
        uint8_t mms = 0;
        
        /* Set mms = 0; If addr is standard ctrl and status (0x00 - 0x020U) */
        /* Set mms = 1; If addr is MAC(from SPI Addr 0X030U) */
        if (addr >= 0X030U)
            mms = 1;
        
        header |= no_os_field_prep(OA_CTRL_HEADER_DNC_MASK, 0);
        header |= no_os_field_prep(OA_CTRL_HEADER_HDRB_MASK, 0);
        header |= no_os_field_prep(OA_CTRL_HEADER_WNR_MASK, wnr);
        header |= no_os_field_prep(OA_CTRL_HEADER_AID_MASK, 0);
        header |= no_os_field_prep(OA_CTRL_HEADER_MMS_MASK, mms);
        header |= no_os_field_prep(OA_CTRL_HEADER_ADDR_MASK, addr);
        header |= no_os_field_prep(OA_CTRL_HEADER_LEN_MASK, cnt - 1);
        header |= no_os_field_prep(OA_CTRL_HEADER_P_MASK, 1);
 
        uint8_t p = calculate_parity((uint8_t *)&header, 4, 0);
        header &= ~OA_CTRL_HEADER_P_MASK;
        header |= no_os_field_prep(OA_CTRL_HEADER_P_MASK, p);
        
        no_os_put_unaligned_be32(header, dst_buf);
}

/*!
 * @brief           Write control data.
 *
 * @param [out]     dst         Pointer to write transaction data.
 * @param [in]      src         Pointer to source register data.
 * @param [in]      cnt         Number of registers to write.
 *
 * @details         Populates the control transaction data with values to be
 *                  written to registers.
 *
 *                  If protection is enabled (#SPI_PROT_EN), it will add the 
 *                  integrity check values as defined by the OPEN Alliance
 *                  specification.
 */
static void oa_tc6_ctrl_cmd_write_data(uint8_t *dst, uint32_t *src, uint32_t cnt,
                                   bool prote_spi)
{   
        if (prote_spi) {
                for (uint32_t i = 0; i < cnt; i++) {
                        no_os_put_unaligned_be32(src[i], &dst[2 * i * 4]);
                        no_os_put_unaligned_be32(~src[i], &dst[(2 * i + 1) * 4]);
                }
        } else {
                for (uint32_t i = 0; i < cnt; i++) {
                        no_os_put_unaligned_be32(src[i], &dst[i * 4]);
                }
        }

}

/*!
 * @brief           Read control data.
 *
 * @param [out]     dst         Pointer to register data buffer.
 * @param [in]      src         Pointer to read transaction data.
 * @param [in]      cnt         Number of registers to read.
 *
 * @details         Reads the data received from a control transaction a
 *                  nd converts it to 32-but register data.
 *
 *                  If protection is enabled, it checks the integrity of the
 *                  received data and return -EPROTO in case of failure.
 *
 */
static int oa_tc6_ctrl_cmd_read_data(uint32_t *dst, uint8_t *src, uint32_t cnt,
				     bool prote_spi)
{
	uint32_t val32[2];

	if (prote_spi) {
		for (uint32_t i = 0; i < cnt; i++) {
			val32[0] = no_os_get_unaligned_be32(&src[2 * i * 4]);
			val32[1] = no_os_get_unaligned_be32(&src[(2 * i + 1) 
                                                            * 4]);
			if (val32[0] != ~val32[1])
				return -EPROTO;
			dst[i] = val32[0];
		}
	} else {
		for (uint32_t i = 0; i < cnt; i++) {
			dst[i] = no_os_get_unaligned_be32(&src[i * 4]);
		}
	}

	return 0;
}


static int oa_tc6_ctrl_setup(uint8_t *buf, uint32_t wnr, uint32_t reg_addr,
                             uint32_t *reg_data, uint32_t *len, bool prote_spi)
{
        uint32_t byte_len;
        
        /* Size of header and echoed header */
        byte_len = 2 * (uint32_t)OA_HEADER_SIZE;
        
        /* Convert words to bytes */
        byte_len += (uint32_t)OA_ACCESS_SIZE * (*len);

        if (prote_spi)
                byte_len += (uint32_t)OA_ACCESS_SIZE * (*len);

        if (byte_len > ((uint32_t)OA_CTRL_BUF_SIZE - 2))
                return -EINVAL;
        
        /* Create the control command header for read */
        oa_tc6_ctrl_cmd_header(buf, wnr, reg_addr, *len);
        
        if (wnr == OA_SPI_WRITE) {
                oa_tc6_ctrl_cmd_write_data(&buf[OA_HEADER_SIZE], 
                        (uint32_t *)reg_data, (*len), prote_spi); 
        }
        
        *len = byte_len;
        
        return 0;
}


static int oa_tc6_start_ctrl_transaction (struct oa_tc6_desc *const desc)
{   
        struct capi_spi_transfer xfer;
        uint32_t len = desc->cnt;
        int ret;

        oa_tc6_ctrl_setup(&desc->ctrl_tx_buf[0], desc->wnr, desc->reg_addr,
                      desc->reg_data, &len, desc->prote_spi);

        desc->spi_state = OA_SPI_STATE_CTRL_END;
        
        xfer.tx_buf  = &desc->ctrl_tx_buf[0];
        xfer.rx_buf  = &desc->ctrl_rx_buf[0];
        xfer.tx_size = (uint16_t)len;
        xfer.rx_size = (uint16_t)len;
    
        ret = capi_spi_transceive_async(desc->comm_desc, &xfer);
        if (ret)
                return ret;
    
        return 0;
}

/*!
 * @brief           Creates SPI transmit payload for one chunk.
 *
 * @param [in]      desc Device driver handle.
 * @param [in]      buf Pointer to the destination buffer for the chunk data.
 * @param [in]      tx_en Enables inserting Tx data into the chunk.
 *
 * @details         Creates the 4-byte transmit header and the chunk payload
 *                  data for one chunk.
 *
 *                  The chunk purpose may be to read frame data from the RxFIFO 
 *                  without writing any new frame data to the TxFIFO, for
 *                  example if TXC=0 and RCA>0. This isflagged via the tx_en
 *                  parameter: if true it will populate frame data into the 
 *                  chunk, if false it will only read.
 */
static int oa_tc6_create_next_chunk(struct oa_tc6_desc *const desc,
				    uint8_t *buf, bool tx_en)
{
	struct net_queue *tx_queue = desc->tx_queue;
        struct net_queue *rx_queue = *desc->rx_queue;
	struct eth_frame_struct *entries = tx_queue->entries;
	struct eth_frame_struct *frame = NULL;
	uint32_t chunk_size = (1 << desc->oa_cps);
	uint32_t chunk_bytes_remaining = chunk_size;
	uint32_t chunk_byte_idx = OA_HEADER_SIZE;
	uint32_t bytes_remaining;
	uint32_t bsize;
	uint32_t swo;
	uint32_t tx_header = 0;
	bool first_chunk = false;

	tx_header = (uint32_t)no_os_field_prep(OA_DATA_HEADER_DNC_MASK, 1);

	/* If no Rx buffers available, ask MAC not to send data to host */
	if (desc->oa_rx_use_backup_buf || net_queue_is_empty(rx_queue))
		tx_header |= no_os_field_prep(OA_DATA_HEADER_NORX_MASK, 1);

	if (tx_en && (net_queue_is_full(tx_queue) ||
	    (desc->oa_tx_cur_buf_idx != tx_queue->head))) {
		frame = &entries[desc->oa_tx_cur_buf_idx];
	}

	if (frame == NULL) {
		tx_header |= no_os_field_prep(OA_DATA_HEADER_DV_MASK, 0);
	} else {
		tx_header |= no_os_field_prep(OA_DATA_HEADER_DV_MASK, 1);

		/* Set VS for every chunk with valid data (port selection) */
		if (desc->num_ports == 2)
			tx_header |= 
                            no_os_field_prep(OA_DATA_HEADER_VS_MASK,
                                             frame->buf_desc->port & 0x1);

		/* Have we already transmitted bytes from this frame? */
		if (!desc->oa_tx_cur_buf_byte_offset)
			first_chunk = true;

		bytes_remaining = frame->buf_desc->trx_size -
                        desc->oa_tx_cur_buf_byte_offset;

		if (bytes_remaining > chunk_bytes_remaining)
			bsize = chunk_bytes_remaining;
		else
			bsize = bytes_remaining;

		bytes_remaining -= bsize;
		chunk_bytes_remaining -= bsize;

		memcpy(&buf[chunk_byte_idx],
		       &frame->buf_desc->buf[desc->oa_tx_cur_buf_byte_offset],
                       bsize);
		desc->oa_tx_cur_buf_byte_offset += bsize;

		if (first_chunk) {
			tx_header |= no_os_field_prep(OA_DATA_HEADER_SV_MASK, 
                                                      1);
			tx_header |= no_os_field_prep(OA_DATA_HEADER_SWO_MASK, 
                                                      0);
			tx_header |=
                            no_os_field_prep(OA_DATA_HEADER_TSC_MASK,
                                             (uint32_t)(frame->buf_desc->egress_capt));
			first_chunk = false;
		}

		if (bytes_remaining == 0) {
			tx_header |= no_os_field_prep(OA_DATA_HEADER_EV_MASK, 
                                                      1);
			tx_header |= no_os_field_prep(OA_DATA_HEADER_EBO_MASK,
						      bsize - 1);
			desc->oa_tx_cur_buf_idx++;
                        
			if (desc->oa_tx_cur_buf_idx == TX_QUEUE_NUM_ENTRIES_RAW)
				desc->oa_tx_cur_buf_idx = 0;
                        
			desc->oa_tx_cur_buf_byte_offset = 0;
		}

		/* Check if there's room to start next frame in same chunk */
		if ((chunk_bytes_remaining >= 4) &&
		    (desc->oa_tx_cur_buf_idx != tx_queue->head)) {
                        
                        /* There is room in the chunk to start transmit next
                        frame. However we need to make sure this would not 
                        lead to a need  for duplicate SV or EV. */
                        
                        /* If there is already a valid SV, do not try to start
                        a new frame */
			if (!no_os_field_get(OA_DATA_HEADER_SV_MASK, 
                                             tx_header)) {
				frame = &entries[desc->oa_tx_cur_buf_idx];
				swo = (bsize + 3) / 4;
                                
                                 /* If we already have a valid EV, make sure
                                than the available bytes in the chunk. */
				if (no_os_field_get(OA_DATA_HEADER_EV_MASK, 
                                                    tx_header) && 
                                    (frame->buf_desc->trx_size + swo * 4 <=
                                     chunk_size))
                                        swo++;
                                
				chunk_byte_idx = chunk_byte_idx + swo * 4;
				chunk_bytes_remaining = chunk_size - swo * 4;
				memcpy(&buf[chunk_byte_idx],
				       &frame->buf_desc->buf[desc->oa_tx_cur_buf_byte_offset],
				       chunk_bytes_remaining);
				desc->oa_tx_cur_buf_byte_offset += 
                                    chunk_bytes_remaining;

				tx_header |= 
                                    no_os_field_prep(OA_DATA_HEADER_SV_MASK, 1);
				tx_header |= 
                                    no_os_field_prep(OA_DATA_HEADER_SWO_MASK, 
                                                     swo);

				
                                /* Update VS for the new frame's port */
                                if (desc->num_ports == 2) {          
                                        tx_header &= ~OA_DATA_HEADER_VS_MASK;
                                        tx_header |= no_os_field_prep(
                                                     OA_DATA_HEADER_VS_MASK,
                                                     (frame->buf_desc->port & 0x1));
                                }

				tx_header |= 
                                    no_os_field_prep(OA_DATA_HEADER_TSC_MASK,
                                                     (uint32_t)(frame->buf_desc->egress_capt));
			}
		}
	}

	/* Calculate parity and write header to buffer */
	tx_header |= no_os_field_prep(OA_DATA_HEADER_P_MASK, 1);
        uint8_t p = calculate_parity((uint8_t *)&tx_header, 4, 0);
        tx_header &= ~OA_DATA_HEADER_P_MASK;
        tx_header |= no_os_field_prep(OA_DATA_HEADER_P_MASK, p);
        
	no_os_put_unaligned_be32(tx_header, buf);

	return 0;
}


/*!
 * @brief           Process transmit queue and create payload for data transactions.
 *
 * @param [in]      desc     Device driver handle.
 *y.
 *
 * @details         Creates a SPI Tx payload of maximum #OA_MAX_CHUNK_COUNT chunks,
 *                  based on the available data in the Tx queue and TXC/RCA values
 *                  read from the MAC device.
 *
 *                  This function can be executed as a result of INT_N assertion
 *                  (MAC interrupt handler), in which case it will create a
 *                  one-chunk transaction.
 *
 */
static int oa_tc6_spi_process(struct oa_tc6_desc *const desc)
{
        struct net_queue *tx_queue= NULL;
        struct eth_frame_struct *frame_entries = NULL;
        uint32_t chunk_count = 0;
        uint32_t tx_chunk_count  = 0;
        uint32_t queue_byte_count = 0;
        uint8_t *buf;
        uint32_t tx_idx;
        bool tx_en;

        if (!desc)
                return -ENODEV;
    
        tx_queue = desc->tx_queue;
        frame_entries = tx_queue->entries;
    
        /* First figure out how many chunks to handle in the SPI transaction */
        if (!net_queue_is_empty(tx_queue))
        {
                tx_idx = desc->oa_tx_cur_buf_idx;
                queue_byte_count = 0;
                do
                {
                        queue_byte_count += 
                                frame_entries[tx_idx].buf_desc->trx_size;
                        tx_idx++;
                        if (tx_idx == TX_QUEUE_NUM_ENTRIES_RAW)
                                tx_idx = 0;
                } while (tx_idx != desc->tx_queue->head);
                
                /* The current buffer may have been already partially
                transmitted */
                queue_byte_count -= desc->oa_tx_cur_buf_byte_offset;

                /* Round up the number of chunks */
                chunk_count = (queue_byte_count >> desc->oa_cps) + 
                ((queue_byte_count & ((1 << desc->oa_cps) - 1)) ? 1: 0);

                if (chunk_count > desc->oa_txc)
                        chunk_count = desc->oa_txc;

                /* Save this for later, to indicate if a chunk has 
                Tx data or not. */
                tx_chunk_count = chunk_count;

        }
        if (desc->oa_rca > chunk_count)
                chunk_count = desc->oa_rca;

        if (chunk_count > desc->oa_max_chunk_count)
                chunk_count = desc->oa_max_chunk_count;

        if (desc->oa_rx_use_backup_buf && (chunk_count > 
                                           OA_RX_BACKUP_BUF_CHUNK_COUNT))
                chunk_count = OA_RX_BACKUP_BUF_CHUNK_COUNT;

        /* This is from IRQ handler: the host needs to initiate a data transfer 
        in response to an IRQ. */
        if ((!chunk_count) && (desc->spi_state == OA_SPI_STATE_IRQ_START))
                /* Fixed to a minimum transfer size is 1 chunk. */
                chunk_count = 1;

        if (desc->oa_rx_use_backup_buf)
                buf = &desc->oa_rx_backup_buf[0];
        else
                buf = &spi_tx_buf[0];

        desc->oa_trx_size = 0;
        for (uint32_t i = 0; i < chunk_count; i++)
        {
                tx_en = (desc->spi_state == OA_SPI_STATE_IRQ_START) ? false : 
                        (i < tx_chunk_count);
                oa_tc6_create_next_chunk(desc, &buf[desc->oa_trx_size], tx_en);
                desc->oa_trx_size += OA_HEADER_SIZE + (1 << desc->oa_cps);
        }

        return 0;
}


static int oa_tc6_end_ctrl_transaction(struct oa_tc6_desc *const desc)
{
        int ret = 0;
        uint32_t cHdr;
        uint32_t eHdr;

        /* Check header vs. echoed header for errors */
        cHdr = *(uint32_t *)&desc->ctrl_tx_buf[0];
        eHdr = *(uint32_t *)&desc->ctrl_rx_buf[OA_HEADER_SIZE];

        if (cHdr != eHdr) {
                desc->spi_err = 1;
        } else {
                desc->spi_err = 0;
                if (desc->wnr == OA_SPI_READ) {
                        ret = oa_tc6_ctrl_cmd_read_data(
                        (uint32_t *)desc->reg_data,
                        &desc->ctrl_rx_buf[2 * OA_HEADER_SIZE],
                        desc->cnt, desc->prote_spi);
                }
        }

        desc->spi_state = OA_SPI_STATE_READY;
        return ret;
}

static int oa_tc6_start_data_transaction(struct oa_tc6_desc *const desc)
{
	struct capi_spi_transfer xfer;
	int ret;

	ret = oa_tc6_spi_process(desc);

	if ((ret == 0) && desc->oa_trx_size) {
		desc->spi_state = OA_SPI_STATE_DATA_END;

		xfer.tx_buf = spi_tx_buf;
		xfer.rx_buf = spi_rx_buf;
		xfer.tx_size = desc->oa_trx_size;
		xfer.rx_size = desc->oa_trx_size;
                
                if (desc->blocking)
                        ret = capi_spi_transceive(desc->comm_desc, &xfer); //blocking xfer
                else if (desc->oa_trx_size >= MIN_SIZE_FOR_DMA)
                        ret = capi_spi_transceive_dma_async(desc->comm_desc, &xfer); //DMA xfer
                else 
                        ret = capi_spi_transceive_async(desc->comm_desc, &xfer); //IT xfer
	} else {
		desc->spi_state = OA_SPI_STATE_READY;

		if (!*(desc->pending_ctrl))
			capi_irq_enable(desc->eth_irq);//HAL_enableIrq(); //update this, not final, just placeholder
	}

	return ret;
}

static bool oa_tc6_chunk_error_detector(struct oa_tc6_desc *const desc,
					uint32_t rx_footer)
{
	bool error_detected = false;

	if (rx_footer == OA_HEADER_BAD) {
		/* Parity error on transmitted header */
		desc->error_stats.hdr_parity_error_count++;
		error_detected = true;
	} else {
		/* Ignore the chunk if the footer parity check fails. */
		if (!calculate_parity((uint8_t *)&rx_footer, OA_FOOTER_SIZE, 0)) {
			desc->error_stats.ftr_parity_error_count++;
			error_detected = true;
		}
		if (!no_os_field_get(OA_DATA_FOOTER_SYNC_MASK, rx_footer)) {
			desc->error_stats.sync_error_count++;
			error_detected = true;
		}
	}

	return error_detected;
}

static void oa_tc6_timestamp_handler(struct oa_tc6_desc *const desc,
				uint32_t spi_rx_buf_byte_offset,
                                uint32_t *frame_dest, uint8_t *timestamp_bytes,
                                uint32_t *byte_offset,
				enum oa_tc6_ts_operation bit_operation)
{
        struct net_queue *rx_queue = *desc->rx_queue;
        struct eth_frame_struct *entries = rx_queue->entries;
        uint32_t timestamp_byte_offset = 0;
        bool is_calculate_parity = false;
        uint8_t  bytes = 0;

        switch (bit_operation)
        {
                case OA_TS_UPPER_32_BIT:
                    timestamp_byte_offset = OA_TIMESTAMP_UPPER_OFFSET;
                    break;
                case OA_TS_LOWER_32_BIT:
                    bytes = OA_TIMESTAMP_64BIT_SIZE;
                    break;
                case OA_TS_32_BIT:
                    bytes = OA_TIMESTAMP_32BIT_SIZE;
                    is_calculate_parity = true;
                    break;
                default:
                    return;
        }

        *frame_dest = no_os_get_unaligned_be32(&spi_rx_buf[spi_rx_buf_byte_offset]);

        memcpy(&timestamp_bytes[timestamp_byte_offset],
               &spi_rx_buf[spi_rx_buf_byte_offset], OA_TIMESTAMP_32BIT_SIZE);
        *byte_offset += OA_TIMESTAMP_32BIT_SIZE;

        if(is_calculate_parity) {
                entries[rx_queue->tail].buf_desc->timestamp_valid = 
                (calculate_parity(timestamp_bytes, bytes, 0) !=
                 desc->oa_timestamp_parity);
        }
}

static void oa_tc6_fcs_checker(struct oa_tc6_desc *const desc, uint32_t fcs_size, uint8_t *rx_buf, uint32_t *event)
{
    struct net_queue *rx_queue  = *desc->rx_queue;
    struct eth_frame_struct *entries = rx_queue->entries;
    uint32_t actual_fcs = 0;
    uint32_t expected_fcs = 0;

    /* Adjust buffer size for FCS */
    if (fcs_size > FCS_SIZE)
    {
        rx_buf[entries[rx_queue->tail].buf_desc->trx_size] -= FCS_SIZE;
        if (desc->fcs_check_en)
        {
            memcpy(&actual_fcs, &rx_buf[entries[rx_queue->tail].buf_desc->trx_size], FCS_SIZE);
            expected_fcs = calculate_fcs(rx_buf, entries[rx_queue->tail].buf_desc->trx_size);

            if (expected_fcs != actual_fcs)
                *event |= MAC_CALLBACK_STATUS_FCS_ERROR;
        }
    }
    else
    {
        *event |= MAC_CALLBACK_STATUS_FCS_ERROR;
        rx_buf[entries[rx_queue->tail].buf_desc->trx_size] = 0;
    }
}

static void oa_tc6_cb_function_caller(struct oa_tc6_desc *const desc, uint32_t tail, uint32_t event, uint8_t num_ports)
{
    struct net_queue *rx_queue = *desc->rx_queue;
    struct eth_frame_struct *entries = rx_queue->entries;

    if (num_ports == 2) {
        /* Update the dynamic forwarding table */
	/* If there was an FCS error, it will be passed on via the callback argument. */
	if (desc->cb_func[MAC_EVT_DYN_TBL_UPDATE] != NULL)
        {
            desc->cb_func[MAC_EVT_DYN_TBL_UPDATE]((desc->app_device), event, entries[tail].buf_desc);
        }
    }

    if (entries[tail].buf_desc->cb_func)
    {
        entries[tail].buf_desc->cb_func((desc->app_device), event, entries[tail].buf_desc);
    }

	desc->oa_rx_cur_buf_byte_offset = 0;
}

                                   
static void oa_tc6_end_data_chunk_processor(struct oa_tc6_desc *const desc, uint32_t *event, uint8_t *rx_buf, uint32_t oa_rx_footer, uint32_t chunk_start)
{
    struct net_queue *rx_queue = *desc->rx_queue;
    struct eth_frame_struct *entries = rx_queue->entries;
    uint32_t tail;
    uint32_t ebo;
    uint32_t sbo;
    
	/* If FD = 1, frame needs to be dropped */
	if (no_os_field_get(OA_DATA_FOOTER_FD_MASK, oa_rx_footer))
	{
		/* Reset the receive index to reuse current buffer for next frame */
		desc->oa_rx_cur_buf_byte_offset = 0;
		desc->error_stats.fd_count++;
		return;
	}
	
	/* Special case: a full frame is sent in a single chunk, this can occur */
	/* when frame is 64 bytes (including FCS) and the chunk is 64 bytes.    */
	/* This case is handled by SV=1 case.                                   */
	struct critical_state basepri = adi_hal_enter_critical_section(HAL_INT_PRI_DMA_SPI_TX);
	ebo = no_os_field_get(OA_DATA_FOOTER_EBO_MASK, oa_rx_footer);
	sbo = no_os_field_get(OA_DATA_FOOTER_SWO_MASK, oa_rx_footer) * (sizeof(uint32_t)); 
	
	if (!(no_os_field_get(OA_DATA_FOOTER_SV_MASK, oa_rx_footer) && (ebo > sbo))) {
		if (desc->oa_valid_flag != OA_VALID_FLAG_START) {
		
			/* No prior SV was received, this is an error */
			/* Reset the receive index to reuse current buffer for next frame */
			desc->oa_rx_cur_buf_byte_offset = 0;

			desc->error_stats.invalid_ev_count++;
		} else {
			if (desc->oa_rx_cur_buf_byte_offset + ebo + 1 > entries[rx_queue->tail].buf_desc->buf_size) {
				/* Rx buffer too small for the incoming frame, notify the user.*/
				*event |= MAC_CALLBACK_STATUS_RX_BUF_OVF;
			} else {
				memcpy(&rx_buf[desc->oa_rx_cur_buf_byte_offset], &spi_rx_buf[chunk_start], ebo + 1);
				entries[rx_queue->tail].buf_desc->trx_size = desc->oa_rx_cur_buf_byte_offset + ebo + 1;
				oa_tc6_fcs_checker(desc, entries[rx_queue->tail].buf_desc->trx_size, (uint8_t *)rx_buf, event);
			}

			tail = rx_queue->tail;
			net_queue_remove_entry(rx_queue);
			oa_tc6_cb_function_caller(desc, tail, *event, 2);
		}
		desc->oa_valid_flag = OA_VALID_FLAG_END;
	}
	adi_hal_exit_critical_section(basepri);
}

static void oa_tc6_process_64_bit_timestamp(struct oa_tc6_desc *const desc,
					    uint32_t chunk_start, uint32_t chunk_size,
					    uint8_t *timestamp_bytes, uint32_t *byte_offset)
{
	struct net_queue *rx_queue = *desc->rx_queue;
	struct eth_frame_struct *entries = rx_queue->entries;
	uint32_t *timestamp_ext = &entries[rx_queue->tail].buf_desc->timestamp_ext;
	uint32_t *timestamp = &entries[rx_queue->tail].buf_desc->timestamp;
	uint32_t spi_rx_buf_byte_offset = chunk_start + *byte_offset;

	/* Process upper 32 bits first (timestamp_ext) */
	oa_tc6_timestamp_handler(desc, spi_rx_buf_byte_offset, timestamp_ext,
				 timestamp_bytes, byte_offset, OA_TS_UPPER_32_BIT);

	desc->oa_timestamp_split = true;

	/* If there is not room in the chunk for the lower 32b of the timestamp, */
	/* those bits will be in the next chunk.                                  */
	if (!((chunk_start + *byte_offset + OA_TIMESTAMP_32BIT_SIZE) > (chunk_start + chunk_size))) {
		/* The lower 32b of the timestamp are in the same chunk. */
		desc->oa_timestamp_split = false;

		/* Recalculate offset for lower 32 bits */
		/* (byte_offset was incremented by upper 32 bit processing) */
		spi_rx_buf_byte_offset = chunk_start + *byte_offset;

		/* Process lower 32 bits (timestamp) */
		oa_tc6_timestamp_handler(desc, spi_rx_buf_byte_offset, timestamp,
					 timestamp_bytes, byte_offset, OA_TS_LOWER_32_BIT);
	}
}

static void oa_tc6_full_frame_in_chunk_process(struct oa_tc6_desc *const desc,
					       uint32_t *event, uint32_t fcs_size,
					       uint8_t *rx_buf)
{
	struct net_queue *rx_queue = *desc->rx_queue;
	struct eth_frame_struct *entries = rx_queue->entries;
	uint32_t tail;

	oa_tc6_fcs_checker(desc, fcs_size, rx_buf, event);
	entries[rx_queue->tail].buf_desc->trx_size = fcs_size;

	tail = (*desc->rx_queue)->tail;
	net_queue_remove_entry(*desc->rx_queue);
	oa_tc6_cb_function_caller(desc, tail, *event, 2);

	desc->oa_valid_flag = OA_VALID_FLAG_END;
}

                                         
static void oa_tc6_start_data_chunk_processor(struct oa_tc6_desc *desc,
					      uint32_t *event, uint32_t *byte_offset,
					      uint32_t chunk_start, uint8_t *timestamp_bytes,
					      uint32_t chunk_size, uint8_t *rx_buf,
					      uint32_t oa_rx_footer)
{
	struct net_queue *rx_queue = *desc->rx_queue;
	struct eth_frame_struct *entries = rx_queue->entries;
	uint32_t vs = no_os_field_get(OA_DATA_FOOTER_VS_MASK, oa_rx_footer);
	uint32_t ebo = no_os_field_get(OA_DATA_FOOTER_EBO_MASK, oa_rx_footer);
	uint32_t sbo = no_os_field_get(OA_DATA_FOOTER_SWO_MASK, oa_rx_footer) * sizeof(uint32_t);
	enum mac_rx_fifo_prio prio = (enum mac_rx_fifo_prio)((vs & 0x2) >> 1);

	entries[rx_queue->tail].buf_desc->timestamp_valid = false;

	if ((desc->oa_valid_flag != OA_VALID_FLAG_NONE) &&
	    (desc->oa_valid_flag != OA_VALID_FLAG_END)) {
		/* No prior EV was received, this is an error */
		desc->error_stats.invalid_sv_count++;
	}

	if (desc->rx_queue_hp_en) {
		if (prio == MAC_RX_FIFO_PRIO_LOW)
			*(desc->rx_queue) = desc->rx_queue_lp;
		else
			*(desc->rx_queue) = desc->rx_queue_hp;
	}

	if (net_queue_is_empty(rx_queue) && !desc->oa_rx_use_backup_buf) {
		/* If a new frame is being received (SV=1) but there are no Rx buffers queued,  */
		/* we drop the incoming data until Rx buffers are added to the queue. This is    */
		/* simpler, but may drop extra frames if the application is slow to refill the   */
		/* queue. The alternative is to use a backup buffer while the Rx queue is empty   */
		/* and switch back to parsing chunks from spi_rx_buf when new buffers are added   */
		/* to the Rx queue. There are hooks for this in the code but the switching        */
		/* between oa_rx_backup_buf and spi_rx_buf is not implemented yet. Perhaps an     */
		/* option to use one option or the other can be useful to serve different use      */
		/* cases.                                                                          */
		desc->oa_rx_use_backup_buf = false;
		desc->oa_rx_buf_chunk_start = chunk_start;
		desc->oa_rx_buf_trx_size = desc->oa_trx_size;
	} else {
		entries[rx_queue->tail].buf_desc->prio = prio;

		if (desc->num_ports == 2)
			entries[rx_queue->tail].buf_desc->port = vs & 0x1;

		rx_buf = entries[rx_queue->tail].buf_desc->buf;

		desc->oa_rx_cur_buf_byte_offset = 0;
		/* SWO is multiple of words */
		*byte_offset = no_os_field_get(OA_DATA_FOOTER_SWO_MASK, oa_rx_footer) * sizeof(uint32_t);

		if (no_os_field_get(OA_DATA_FOOTER_RTSA_MASK, oa_rx_footer)) {
			/* Store the parity in case the timestamp is split across multiple chunks. */
			/* RTSP is only valid for the chunk in which RTSA is 1.                    */
			desc->oa_timestamp_parity = no_os_field_get(OA_DATA_FOOTER_RTSP_MASK, oa_rx_footer);
			if (desc->ts_format == OA_TS_FORMAT_64B_1588) {
				oa_tc6_process_64_bit_timestamp(desc, chunk_start,
					chunk_size, timestamp_bytes, byte_offset);
			} else {
				oa_tc6_timestamp_handler(desc, chunk_start + *byte_offset,
					&entries[rx_queue->tail].buf_desc->timestamp,
					timestamp_bytes, byte_offset, OA_TS_32_BIT);
			}
		}

		if (chunk_size - *byte_offset) {
			/* Rx buffer minimum size is greater than the maximum chunk size, no buffer overflow. */
			memcpy(&rx_buf[0], &spi_rx_buf[chunk_start + *byte_offset],
			       chunk_size - *byte_offset);
			desc->oa_rx_cur_buf_byte_offset += chunk_size - *byte_offset;
		}

		if (no_os_field_get(OA_DATA_FOOTER_EV_MASK, oa_rx_footer) && (ebo > sbo)) {
			oa_tc6_full_frame_in_chunk_process(desc, event,
				ebo + 1 - sbo - FCS_SIZE, rx_buf);
		} else {
			desc->oa_valid_flag = OA_VALID_FLAG_START;
		}
	}
}

static void oa_tc6_mid_data_chunk_processor(struct oa_tc6_desc *const desc,
					    uint32_t chunk_size,
					    uint32_t oa_rx_footer,
					    uint8_t *rx_buf, uint32_t *event,
					    uint32_t byte_offset, uint32_t chunk_start)
{
	struct net_queue *rx_queue = *desc->rx_queue;
	struct eth_frame_struct *entries = rx_queue->entries;
	uint32_t tail;

	if (!no_os_field_get(OA_DATA_FOOTER_EV_MASK, oa_rx_footer) &&
	    (desc->oa_valid_flag == OA_VALID_FLAG_START)) {
		if (desc->oa_rx_cur_buf_byte_offset + chunk_size >
		    entries[rx_queue->tail].buf_desc->buf_size) {
			/* Rx buffer too small for the incoming frame, notify the user.      */
			/* At this point the current frame reception finishes and callback is */
			/* executed.                                                          */
			*event |= MAC_CALLBACK_STATUS_RX_BUF_OVF;

			tail = (*desc->rx_queue)->tail;
			net_queue_remove_entry(*(desc->rx_queue));

			if (entries[tail].buf_desc->cb_func) {
				entries[tail].buf_desc->cb_func(desc->app_device,
					*event, entries[tail].buf_desc);
			}

			desc->oa_rx_cur_buf_byte_offset = 0;
		} else {
			memcpy(&rx_buf[desc->oa_rx_cur_buf_byte_offset],
			       &spi_rx_buf[chunk_start + byte_offset],
			       chunk_size - byte_offset);

			desc->oa_rx_cur_buf_byte_offset += chunk_size - byte_offset;
		}
	}
}

static void oa_tc6_complete_transmission_checker(struct oa_tc6_desc *const desc,
						 uint32_t tx_header,
						 uint32_t *event)
{
	struct net_queue *tx_queue = desc->tx_queue;
	struct eth_frame_struct *entries = tx_queue->entries;
	uint32_t tail;

	if (no_os_field_get(OA_DATA_HEADER_EV_MASK, tx_header)) {
		tail = tx_queue->tail;
		net_queue_remove_entry(desc->tx_queue);

		/* Decrement the reference count, and call the callback function only  */
		/* if the reference count is 0. This ensures that if the intent was to */
		/* send the buffer to both ports, it will be returned to the buffer    */
		/* pool only after sending to both ports has completed.                */
		entries[tail].buf_desc->ref_count--;
		if (entries[tail].buf_desc->cb_func && (!entries[tail].buf_desc->ref_count)) {
			entries[tail].buf_desc->cb_func(desc->app_device,
				*event, entries[tail].buf_desc);
		}
	}
}

static int oa_tc6_end_data_transaction(struct oa_tc6_desc *const desc)
{
	struct capi_spi_transfer xfer;
	struct net_queue *rx_queue = *desc->rx_queue;
	struct eth_frame_struct *entries = rx_queue->entries;
	uint32_t chunk_size = (1 << desc->oa_cps);
	uint32_t rx_footer;
	uint32_t tx_header;
	uint32_t exst = 0;
	uint32_t byte_offset;
	uint32_t len;
	uint32_t event = MAC_CALLBACK_STATUS_RX_BUF_OVF;
	uint8_t *rx_buf;
	uint8_t timestamp_bytes[TS_BYTES_SIZE];
	int ret = 0;

	for (uint32_t chunk_start = 0; chunk_start < desc->oa_trx_size;
	     chunk_start += OA_HEADER_SIZE + chunk_size) {

		rx_footer = no_os_get_unaligned_be32(&spi_rx_buf[chunk_start 
                                                     + chunk_size]);

		if (oa_tc6_chunk_error_detector(desc, rx_footer))
			continue;

		exst |= no_os_field_get(OA_DATA_FOOTER_EXST_MASK, rx_footer);

		desc->oa_txc = no_os_field_get(OA_DATA_FOOTER_TXC_MASK, 
                                               rx_footer);
		desc->oa_rca = no_os_field_get(OA_DATA_FOOTER_RCA_MASK, 
                                               rx_footer);

		if (no_os_field_get(OA_DATA_FOOTER_DV_MASK, rx_footer) &&
		    !desc->oa_rx_use_backup_buf) {

			rx_buf = entries[rx_queue->tail].buf_desc->buf;
			byte_offset = 0;

			if (desc->oa_timestamp_split) {
				/* If the timestamp was split into two chunks,
				the remaining 32b will be at the start of this
				chunk. */
				desc->oa_timestamp_split = false;
				oa_tc6_timestamp_handler(desc, chunk_start,
					&entries[rx_queue->tail].buf_desc->timestamp,
					timestamp_bytes, &byte_offset,
					OA_TS_LOWER_32_BIT);
			}

			if (no_os_field_get(OA_DATA_FOOTER_EV_MASK, rx_footer))
				oa_tc6_end_data_chunk_processor(desc, &event,
					rx_buf, rx_footer, chunk_start);

			if (no_os_field_get(OA_DATA_FOOTER_SV_MASK, rx_footer))
				oa_tc6_start_data_chunk_processor(desc, &event,
					&byte_offset, chunk_start,
					timestamp_bytes, chunk_size,
					rx_buf, rx_footer);
			else
				oa_tc6_mid_data_chunk_processor(desc, chunk_size,
					rx_footer, rx_buf, &event,
					byte_offset, chunk_start);
		}

		tx_header = no_os_get_unaligned_be32(&spi_tx_buf[chunk_start]);

		oa_tc6_complete_transmission_checker(desc, tx_header, &event);
	}

	if (exst || !no_os_field_get(OA_DATA_FOOTER_SYNC_MASK, rx_footer)) {
		/* Read status registers, addr: 0X008U */
		len = 2;

		ret = oa_tc6_ctrl_setup(&desc->ctrl_tx_buf[0], OA_SPI_READ,
				0X008U, desc->reg_data,
				&len, desc->prote_spi);

		desc->spi_state = OA_SPI_STATE_READ_STATUS;

		xfer.tx_buf = &desc->ctrl_tx_buf[0];
		xfer.rx_buf = &desc->ctrl_rx_buf[0];
		xfer.tx_size = (uint16_t)len;
		xfer.rx_size = (uint16_t)len;

		if (desc->blocking)
			ret = capi_spi_transceive(desc->comm_desc, &xfer);
                else if (desc->oa_trx_size >= MIN_SIZE_FOR_DMA)
			ret = capi_spi_transceive_dma_async(desc->comm_desc, &xfer);
                else 
			ret = capi_spi_transceive_async(desc->comm_desc, &xfer); //IT xfer

	} else {
		if ((!net_queue_is_empty(&*(desc->tx_queue)) && desc->oa_txc) ||
		    (!net_queue_is_empty(*(desc->rx_queue)) && desc->oa_rca)) {

			ret = oa_tc6_spi_process(desc);

			desc->spi_state = OA_SPI_STATE_DATA_END;

			xfer.tx_buf = spi_tx_buf;
			xfer.rx_buf = spi_rx_buf;
			xfer.tx_size = len;
			xfer.rx_size = len;
			
			/* Determine if it's worth using DMA based on the transaction size. */
			if (desc->blocking)
				ret = capi_spi_transceive(desc->comm_desc, &xfer);
			else if (desc->oa_trx_size >= MIN_SIZE_FOR_DMA)
				ret = capi_spi_transceive_dma_async(desc->comm_desc, &xfer);
			else 
				ret = capi_spi_transceive_async(desc->comm_desc, &xfer); //IT xfer
		} else {
			desc->spi_state = OA_SPI_STATE_READY;

			if (!*(desc->pending_ctrl))
				capi_irq_enable(15);//FIX THIS WITH FINAL IRQ FOR INT_N
		}
	}

	return ret;
}                                   

static int oa_tc6_start_irq(struct oa_tc6_desc *const desc)
{
	struct capi_spi_transfer xfer;
	uint32_t tx_header = 0;
        uint32_t val32;
        uint8_t bytes[OA_SPI_BYTE_SIZE];
	int ret = 0;

	/* Single data chunk in response to an IRQ */
	tx_header |= no_os_field_prep(OA_DATA_HEADER_DNC_MASK, 1);
	tx_header |= no_os_field_prep(OA_DATA_HEADER_NORX_MASK, 1);
	tx_header |= no_os_field_prep(OA_DATA_HEADER_P_MASK, 1);
        val32 = tx_header >> 1;
        *(uint32_t *)bytes = val32;
        
	uint8_t p = calculate_parity(bytes, 4, 1);
	tx_header &= ~OA_DATA_HEADER_P_MASK;
	tx_header |= no_os_field_prep(OA_DATA_HEADER_P_MASK, p);

	no_os_put_unaligned_be32(tx_header, &spi_tx_buf[0]);

	desc->oa_trx_size = (uint32_t)OA_HEADER_SIZE + (1 << desc->oa_cps);

	desc->spi_state = OA_SPI_STATE_DATA_END;

	xfer.tx_buf = spi_tx_buf;
	xfer.rx_buf = spi_rx_buf;
	xfer.tx_size = (uint16_t)desc->oa_trx_size;
	xfer.rx_size = (uint16_t)desc->oa_trx_size;
	
	/* Determine if it's worth using DMA based on the transaction size. */
	if (desc->blocking)
		ret = capi_spi_transceive(desc->comm_desc, &xfer);
	else if (desc->oa_trx_size >= MIN_SIZE_FOR_DMA)
		ret = capi_spi_transceive_dma_async(desc->comm_desc, &xfer);
	else
		ret = capi_spi_transceive_async(desc->comm_desc, &xfer); //IT mode xfer

	return ret;
}

static int oa_tc6_phy_reg_read_start(struct oa_tc6_desc *desc, uint32_t *mdio_cmd,
				     uint32_t prtad, uint32_t reg_addr)
{
	struct capi_spi_transfer xfer;
	uint32_t len;
	int ret;

	*mdio_cmd = 0;
	*mdio_cmd |= no_os_field_prep(MDIO_DEVAD_MASK, DEVTYPE(reg_addr));
	*mdio_cmd |= no_os_field_prep(MDIO_ST_MASK, MDIOACC_N_MDIO_ST_CLAUSE45);
	*mdio_cmd |= no_os_field_prep(MDIO_PRTAD_MASK, prtad);
	*mdio_cmd |= no_os_field_prep(MDIO_OP_MASK, MDIOACC_N_MDIO_OP_MD_ADDR);
	*mdio_cmd |= no_os_field_prep(MDIO_DATA_MASK, REGADDR(reg_addr));

	desc->wnr = OA_SPI_WRITE;
	desc->reg_addr = 0X020U; /*addr MDIOACC_0 :0X020U */
	desc->reg_data = mdio_cmd;
	desc->cnt = 1;

	len = desc->cnt;
	ret = oa_tc6_ctrl_setup(&desc->ctrl_tx_buf[0], desc->wnr, desc->reg_addr,
				desc->reg_data, &len, desc->prote_spi);
	if (ret)
		return ret;

	desc->spi_state = OA_SPI_STATE_READ_PHY_REG;

	xfer.tx_buf = &desc->ctrl_tx_buf[0];
	xfer.rx_buf = &desc->ctrl_rx_buf[0];
	xfer.tx_size = (uint16_t)len;
	xfer.rx_size = (uint16_t)len;

	if (desc->blocking)
		ret = capi_spi_transceive(desc->comm_desc, &xfer);
	else if (len >= MIN_SIZE_FOR_DMA)
		ret = capi_spi_transceive_dma_async(desc->comm_desc, &xfer);
	else
		ret = capi_spi_transceive_async(desc->comm_desc, &xfer);

	return ret;
}

static int oa_tc6_spi_int_handle(struct oa_tc6_desc *const desc)
{
	struct capi_spi_transfer xfer;
	uint32_t status0_masked;
	uint32_t status1_masked;
	struct timestamp_rdy timestamp_ready = {0};
	enum eth_link_status link_status;
	int ret = 0;

	if (desc == NULL)
		return -EINVAL;

	status0_masked = desc->status_regs->status0_masked;
	status1_masked = desc->status_regs->status1_masked;

	/* Link status behavior differs on non-dual-port devices. */
	if (desc->num_ports != 2) {
		if (no_os_field_get(STATUS1_LINK_CHANGE_MASK, status1_masked)) {
			if (desc->cb_func[MAC_EVT_LINK_CHANGE] != NULL) {
				link_status = (enum eth_link_status)no_os_field_get(STATUS1_P1_LINK_STATUS_MASK, desc->status_regs->status1);
				desc->cb_func[MAC_EVT_LINK_CHANGE](desc->cb_param_p[MAC_EVT_LINK_CHANGE],MAC_EVT_LINK_CHANGE,(void *)&link_status);
			}
		}
	}

	/* Captured timestamp availability */
	if (desc->num_ports == 2) {
		if (no_os_field_get(STATUS0_TTSCAA_MASK, status0_masked) |
		    no_os_field_get(STATUS0_TTSCAB_MASK, status0_masked) |
		    no_os_field_get(STATUS0_TTSCAC_MASK, status0_masked) |
		    no_os_field_get(STATUS1_P2_TTSCAA_MASK, status1_masked) |
		    no_os_field_get(STATUS1_P2_TTSCAB_MASK, status1_masked) |
		    no_os_field_get(STATUS1_P2_TTSCAC_MASK, status1_masked)) {
			if (desc->cb_func[MAC_EVT_TIMESTAMP_RDY] != NULL) {
				timestamp_ready.p1_timestamp_ready_a = (bool)no_os_field_get(STATUS0_TTSCAA_MASK, status0_masked);
				timestamp_ready.p1_timestamp_ready_b = (bool)no_os_field_get(STATUS0_TTSCAB_MASK, status0_masked);
				timestamp_ready.p1_timestamp_ready_c = (bool)no_os_field_get(STATUS0_TTSCAC_MASK, status0_masked);
				timestamp_ready.p2_timestamp_ready_a = (bool)no_os_field_get(STATUS1_P2_TTSCAA_MASK, status1_masked);
				timestamp_ready.p2_timestamp_ready_b = (bool)no_os_field_get(STATUS1_P2_TTSCAB_MASK, status1_masked);
				timestamp_ready.p2_timestamp_ready_c = (bool)no_os_field_get(STATUS1_P2_TTSCAC_MASK, status1_masked);
				desc->cb_func[MAC_EVT_TIMESTAMP_RDY]( desc->cb_param_p[MAC_EVT_TIMESTAMP_RDY], MAC_EVT_TIMESTAMP_RDY, (void *)&timestamp_ready);
			}
		}
	} else {
		if (no_os_field_get(STATUS0_TTSCAA_MASK, status0_masked) |
		    no_os_field_get(STATUS0_TTSCAB_MASK, status0_masked) |
		    no_os_field_get(STATUS0_TTSCAC_MASK, status0_masked)) {
			if (desc->cb_func[MAC_EVT_TIMESTAMP_RDY] != NULL) {
				timestamp_ready.timestamp_ready_a = (bool)no_os_field_get(STATUS0_TTSCAA_MASK, status0_masked);
				timestamp_ready.timestamp_ready_b = (bool)no_os_field_get(STATUS0_TTSCAB_MASK, status0_masked);
				timestamp_ready.timestamp_ready_c = (bool)no_os_field_get(STATUS0_TTSCAC_MASK, status0_masked);
				desc->cb_func[MAC_EVT_TIMESTAMP_RDY](desc->cb_param_p[MAC_EVT_TIMESTAMP_RDY], MAC_EVT_TIMESTAMP_RDY, (void *)&timestamp_ready);
			}
		}
	}

	/* General status callback */
	if (status0_masked || status1_masked) {
		if (desc->cb_func[MAC_EVT_STATUS] != NULL) {
			desc->cb_func[MAC_EVT_STATUS](desc->cb_param_p[MAC_EVT_STATUS], MAC_EVT_STATUS, (void *)desc->status_regs);
		}
	}

	/* Clear status bits — write back STATUS0/STATUS1 */
	desc->wnr = OA_SPI_WRITE;
	desc->reg_addr = 0X008U; /* STATUS0: 0X008U */
	desc->reg_data = (uint32_t *)desc->status_regs;
	desc->cnt = 2;

	ret = oa_tc6_ctrl_setup(&desc->ctrl_tx_buf[0], desc->wnr, desc->reg_addr,
				desc->reg_data, &desc->cnt, desc->prote_spi);
	if (ret)
		return ret;

	desc->spi_state = OA_SPI_STATE_DATA_START;

	xfer.tx_buf = &desc->ctrl_tx_buf[0];
	xfer.rx_buf = &desc->ctrl_rx_buf[0];
	xfer.tx_size = (uint16_t)desc->cnt;
	xfer.rx_size = (uint16_t)desc->cnt;

	if (desc->blocking)
		ret = capi_spi_transceive(desc->comm_desc, &xfer);
	else if (desc->cnt >= MIN_SIZE_FOR_DMA)
		ret = capi_spi_transceive_dma_async(desc->comm_desc, &xfer);
	else
		ret = capi_spi_transceive_async(desc->comm_desc, &xfer);

	return ret;
}

static int oa_tc6_phy_reg_read_step(struct oa_tc6_desc *const desc, uint32_t *mdio_cmd)
{
	struct capi_spi_transfer xfer;
	uint32_t len;
	int ret = 0;

	*mdio_cmd &= ~MDIO_OP_MASK;
	*mdio_cmd |= no_os_field_prep(MDIO_OP_MASK, MDIOACC_N_MDIO_OP_MD_RD);
	*mdio_cmd &= ~MDIO_DATA_MASK;
	*mdio_cmd &= ~MDIO_TRDONE_MASK;

	desc->reg_addr = 0X021U; /* ADDR_MDIOACC_1 :0X021U */
	desc->reg_data = mdio_cmd;
	desc->cnt = 1;

	len = desc->cnt;
	ret = oa_tc6_ctrl_setup(&desc->ctrl_tx_buf[0], desc->wnr, desc->reg_addr,
				desc->reg_data, &len, desc->prote_spi);
	if (ret)
		return ret;

	desc->spi_state = OA_SPI_STATE_READ_PHY_REG;

	xfer.tx_buf = &desc->ctrl_tx_buf[0];
	xfer.rx_buf = &desc->ctrl_rx_buf[0];
	xfer.tx_size = (uint16_t)len;
	xfer.rx_size = (uint16_t)len;

	if (desc->blocking)
		ret = capi_spi_transceive(desc->comm_desc, &xfer);
	else if (len >= MIN_SIZE_FOR_DMA)
		ret = capi_spi_transceive_dma_async(desc->comm_desc, &xfer);
	else
		ret = capi_spi_transceive_async(desc->comm_desc, &xfer);

	return ret;
}

static void oa_tc6_record_port_status(struct oa_tc6_desc *const desc,
				      uint32_t *const mdio_cmd, uint8_t port_number)
{
	struct reg_val *port_status;
	struct reg_val *status_masked;
	struct reg_val reg_data;
	struct reg_val stat_init;
	struct reg_val phy_irq_mask;

	if (desc == NULL || mdio_cmd == NULL)
		return;

	reg_data.reg_val = *desc->reg_data;
	
	uint32_t status_init = OA_PHY_STATUS_INIT_VAL;
	stat_init = *(struct reg_val*)&status_init;
	
	phy_irq_mask.reg_val = *(desc->phy_irq_mask);

	if (port_number == 1) {
		port_status = (struct reg_val *)&desc->status_regs->p1_status;
		status_masked = (struct reg_val *)&desc->status_regs->p1_status_masked;
	} else if ((desc->num_ports == 2) && (port_number == 2)) {
		port_status = (struct reg_val *)&desc->status_regs->p2_status;
		status_masked = (struct reg_val *)&desc->status_regs->p2_status_masked;
	} else {
		return;
	}

	/* Was CRSM_IRQ_STATUS read yet? */
	if (status_masked->lower == stat_init.lower) {
		/* Record state of CRSM_IRQ_STATUS */
		port_status->lower = reg_data.lower;
		phy_irq_mask.upper &= 0x0000;
		status_masked->lower = 0x0000;
		status_masked->reg_val |= (port_status->reg_val & phy_irq_mask.reg_val);

		/* Move on to PHY_SUBSYS_IRQ_STATUS */
                 /* ADDR PHY_SUBSYS_IRQ_STATUS: 0X1F0011U */
		oa_tc6_phy_reg_read_start(desc, mdio_cmd, port_number,
					  0X1F0011U);
	} else {
		/* Record state of PHY_SUBSYS_IRQ_STATUS */
		/* statusMasked will always only retain the lower bits since the */
		/* process on the right will always equate to 0x0000             */
		port_status->upper = reg_data.lower;
		phy_irq_mask.lower &= 0x0000;
		status_masked->upper = 0x0000;
		status_masked->reg_val |= ((port_status->reg_val & phy_irq_mask.reg_val) << 16);
	}

	if (desc->num_ports == 2) {
		if ((port_number == 1) && (status_masked->reg_val & STATUS1_P2_PHYINT)) {
			/* At this point we haven't read anything so if the PHYINT is set, */
			/* we need to go ahead and read from PHY registers 		   */
			oa_tc6_phy_reg_read_start(desc, mdio_cmd, 2, ADDR_CRSM_IRQ_STATUS);
		} else {
			oa_tc6_spi_int_handle(desc);
		}
	} else {
		oa_tc6_spi_int_handle(desc);
	}
}

                                             
static int oa_tc6_read_phy_reg(struct oa_tc6_desc *const desc,
			       uint32_t *const mdio_cmd)
{
	uint32_t check_hdr;
	uint32_t echo_hdr;
	int ret = 0;

	if (desc == NULL || mdio_cmd == NULL)
		return -EINVAL;

	/* Check header vs. echoed header for errors */
	memcpy(&check_hdr, &desc->ctrl_tx_buf[0], 4);
	memcpy(&echo_hdr, &desc->ctrl_rx_buf[OA_HEADER_SIZE], 4);

	if (check_hdr != echo_hdr) {
		desc->spi_err = 1;
		oa_tc6_spi_int_handle(desc);
		return ret;
	}

	desc->spi_err = 0;
	/* ADDR_MDIOACC_0: (0X020U) */
	if (desc->reg_addr == 0X020U) {
		/* Second register write */
		desc->wnr = OA_SPI_WRITE;
		oa_tc6_phy_reg_read_step(desc, mdio_cmd);
		return ret;
	}

	if (desc->wnr == OA_SPI_WRITE) {
		/* Both writes completed, now read */
		desc->wnr = OA_SPI_READ;
		oa_tc6_phy_reg_read_step(desc, mdio_cmd);
		return ret;
	}

	/* Poll until TRDONE bit is set in the MDIO access register */
	ret = oa_tc6_ctrl_cmd_read_data((uint32_t *)desc->reg_data,
			&desc->ctrl_rx_buf[2 * OA_HEADER_SIZE],
			desc->cnt, desc->prote_spi);

	if (no_os_field_get(MDIO_TRDONE_MASK, *desc->reg_data)) {
		/* Successful read, record the status */
		if (no_os_field_get(STATUS0_PHYINT_MASK,desc->status_regs->status0_masked) &&
		    ((desc->status_regs->p1_status & 0xFFFF0000) ==
		     (OA_PHY_STATUS_INIT_VAL & 0xFFFF0000)))
			oa_tc6_record_port_status(desc, mdio_cmd, 1);

		if (desc->num_ports == 2) {
			if (no_os_field_get(STATUS1_P2_PHYINT_MASK, desc->status_regs->status1_masked) &&
			    ((desc->status_regs->p2_status & 0xFFFF0000) ==
			     (OA_PHY_STATUS_INIT_VAL & 0xFFFF0000)))
				oa_tc6_record_port_status(desc, mdio_cmd, 2);
		}
	} else {
		/* Keep polling until TRDONE is set */
		desc->wnr = OA_SPI_READ;
		oa_tc6_phy_reg_read_step(desc, mdio_cmd);
	}

	return ret;
}




static int oa_tc6_read_status(struct oa_tc6_desc *const desc, uint32_t *const mdio_cmd_p)
{
    int ret = 0;
    uint32_t prtad = (uint32_t)PHY_PORT_2_ADDRESS;

    if (desc == NULL || mdio_cmd_p == NULL)
		return -EINVAL;

    ret = oa_tc6_ctrl_cmd_read_data((uint32_t *)&desc->status_regs->status0,
                               &desc->ctrl_rx_buf[2 * OA_HEADER_SIZE], 2, desc->prote_spi);

    desc->status_regs->status0_masked = desc->status_regs->status0
                                             & ~*(desc->irq_mask0);
    desc->status_regs->status1_masked = desc->status_regs->status1 &
                                             ~*(desc->irq_mask1);
    desc->status_regs->p1_status_masked = OA_PHY_STATUS_INIT_VAL;
    desc->status_regs->p1_status = OA_PHY_STATUS_INIT_VAL;

    if (desc->num_ports == 2) {

        desc->status_regs->p2_status_masked = OA_PHY_STATUS_INIT_VAL;
        desc->status_regs->p2_status = OA_PHY_STATUS_INIT_VAL;

        if ((desc->status_regs->status0_masked & STATUS0_PHYINT) || 
            (desc->status_regs->status1_masked & STATUS1_P2_PHYINT)) {
            /* Read PHY interrupt status registers */
            if (desc->status_regs->status0_masked & STATUS0_PHYINT)
                prtad = PHY_PORT_1_ADDRESS;            

            oa_tc6_phy_reg_read_start(desc, mdio_cmd_p, prtad, ADDR_CRSM_IRQ_STATUS);
        } else {
            oa_tc6_spi_int_handle(desc);
        }
    } else {
        if (desc->status_regs->status0_masked & STATUS0_PHYINT) {
            /* Read PHY interrupt status registers */
            if (desc->status_regs->status0_masked & STATUS0_PHYINT) {
                prtad = PHY_PORT_1_ADDRESS;
            }
	    
            oa_tc6_phy_reg_read_start(desc, mdio_cmd_p, prtad, ADDR_CRSM_IRQ_STATUS);
        } else {
            oa_tc6_spi_int_handle(desc);
        }
    }
    return ret;
}

/*!
 * @brief           OPEN Alliance SPI state machine.
 *
 * @param [in]      desc     Device driver handle.
 *
 */
static int oa_tc6_state_machine(struct oa_tc6_desc *desc)
{
        int ret;
        static uint32_t mdio_cmd;

        switch (desc->spi_state) {
        case OA_SPI_STATE_CTRL_START:
                ret = oa_tc6_start_ctrl_transaction(desc);
                break;

        case OA_SPI_STATE_CTRL_END:
                ret = oa_tc6_end_ctrl_transaction(desc);
                break;

        case OA_SPI_STATE_DATA_START:
                ret = oa_tc6_start_data_transaction(desc);
                break;

        case OA_SPI_STATE_DATA_END:
                ret = oa_tc6_end_data_transaction(desc);
                break;

        case OA_SPI_STATE_IRQ_START:
                ret = oa_tc6_start_irq(desc);
                break;

        case OA_SPI_STATE_READ_STATUS:
                ret = oa_tc6_read_status(desc, &mdio_cmd);
                break;

        case OA_SPI_STATE_READ_PHY_REG:
                ret = oa_tc6_read_phy_reg(desc, &mdio_cmd);
                break;
                
        default:
                desc->spi_err = 1;
                break;
        }

        if (desc->spi_err) {
                /* If an error occurred, reset the state machine to a    */
                /* known 'good' state. This will allow the driver to     */
                /* recover.                                              */
                desc->spi_state = OA_SPI_STATE_READY;

                /* Return value will notify the caller of the error to   */
                /* proceed with its handling.                            */
                ret = -EPROTO;

                /* IRQ is enabled to ensure driver is responsive and     */
                /* will not cause missed events.                         */
                capi_irq_enable(desc->eth_irq);//HAL_EnableIrq();
        }
        
        return ret;
}

/**
 * @brief Read a register value.
 * @param desc - the OA TC6 descriptor.
 * @param addr - Register address.
 * @param val - Register value.
 * @return 0 in case of success, negative error code otherwise
 */
int oa_tc6_reg_read(struct oa_tc6_desc *desc, uint32_t addr, uint32_t *val)
{
	uint32_t comp_val;
	int ret;

	if (!desc)
		return -ENODEV;

	oa_tc6_prepare_rx_ctrl(desc, addr);
	ret = oa_tc6_thread(desc);
	if (ret)
		return ret;

	*val = no_os_get_unaligned_be32(&desc->ctrl_chunks[2 * OA_HEADER_LEN]);
	desc->ctrl_rx_credit = 0;

	if (desc->prote_spi) {
		comp_val = no_os_get_unaligned_be32(&desc->ctrl_chunks[3 * OA_HEADER_LEN]);
		if (*val != (comp_val ^ NO_OS_GENMASK(31, 0)))
			return -EINVAL;
	}

	return 0;
}

/**
 * @brief Read a register value asynchronously.
 * @param desc - the OA TC6 descriptor.
 * @param addr - Register address.
 * @param val - Register value.
 * @param num_reg - Number of registers in the burst control transaction 
 * @return 0 in case of success, negative error code otherwise
 */
int oa_tc6_reg_read_async(struct oa_tc6_desc *desc, uint32_t addr, uint32_t *val,
                         uint32_t num_reg)
{    
        if (!desc)
                return -ENODEV;
                
        desc->spi_state = OA_SPI_STATE_CTRL_START;
        desc->wnr = OA_SPI_READ;
        desc->reg_addr = addr;
        desc->reg_data = val;
        desc->cnt = num_reg;
        
        return oa_tc6_state_machine(desc);
}

/**
 * @brief Write a register value asynchronously.
 * @param desc - the OA TC6 descriptor.
 * @param addr - Register address.
 * @param val - Register value.
 * @param num_reg - Number of registers in the burst control transaction 
 * @return 0 in case of success, negative error code otherwise
 */
int oa_tc6_reg_write_async(struct oa_tc6_desc *desc, uint32_t addr, uint32_t *val,
                         uint32_t num_reg)
{     
        if (!desc)
                return -ENODEV;
                
        desc->spi_state = OA_SPI_STATE_CTRL_START;
        desc->wnr = OA_SPI_WRITE;
        desc->reg_addr = addr;
        desc->reg_data = val;
        desc->cnt = num_reg;
        
        return oa_tc6_state_machine(desc);
}

/**
 * @brief Write a register value.
 * @param desc - the OA TC6 descriptor.
 * @param addr - Register address.
 * @param val - Register value.
 * @return 0 in case of success, negative error code otherwise
 */
int oa_tc6_reg_write(struct oa_tc6_desc *desc, uint32_t addr, uint32_t val)
{
	int ret;

	if (!desc)
		return -ENODEV;

	oa_tc6_prepare_tx_ctrl(desc, addr, val);
	ret = oa_tc6_thread(desc);
	if (ret)
		return ret;

	desc->ctrl_tx_credit = 0;

	return 0;
}

/**
 * @brief Update a field inside a register.
 * @param desc - the OA TC6 descriptor.
 * @param addr - Register address.
 * @param val - Field value.
 * @param mask - Bit mask corresponding to the register field.
 * @return 0 in case of success, negative error code otherwise
 */
int oa_tc6_reg_update(struct oa_tc6_desc *desc, uint32_t addr,
		      uint32_t val, uint32_t mask)
{
	uint32_t reg_val;
	int ret;

	ret = oa_tc6_reg_read(desc, addr, &reg_val);
	if (ret)
		return ret;

	reg_val &= ~mask;
	reg_val |= val & mask;

	return oa_tc6_reg_write(desc, addr, reg_val);
}

/**
 * @brief Initiate a data frame SPI transaction via the OA TC6 state machine.
 * @param desc     - the OA TC6 descriptor.
 * @param blocking - if true, wait for the transaction to complete before 
 *                   returning.
 * @return 0 in case of success, negative error code otherwise.
 */                                                                 
int oa_tc6_process_tx_frame(struct oa_tc6_desc *const desc, bool blocking)
{   
    desc->blocking = blocking;
    desc->spi_state = OA_SPI_STATE_DATA_START;
    return oa_tc6_state_machine(desc);
}

/**
 * @brief       OPEN-Alliance IRQ handler. Called from the INT_N interrupt
 *              handler. Executes the OA state machine.
 * @param desc - the OA TC6 descriptor.
 *
 * @return 0 in case of success, negative error code otherwise.
 */  
int oa_tc6_irq_handler (struct oa_tc6_desc *const desc)
{
	int ret = 0;

	if (desc == NULL)
		return -EINVAL;

	HAL_OA_STATE_MACHINE_LOCK();
	if (desc->spi_state == OA_SPI_STATE_READY)
	{   
		capi_irq_disable(desc->eth_irq);//HAL_DisableIrq();
		(desc->spi_state) = OA_SPI_STATE_IRQ_START;
		ret = oa_tc6_state_machine(desc);
	}
	HAL_OA_STATE_MACHINE_UNLOCK();
	return ret;
}


/*!
 * @brief       SPI callback function. Called from the SPI interrupt
 *              handler (if SPI uses interrupts) or DMA interrupt handler
 *              (if SPI uses DMA). Executes the OA state machine.
 *
 * @param [in]  desc     Device driver handle.
 *
 * @return      None                
 */
void oa_tc6_spi_callback(void *cb_param, uint32_t event, void *arg)
{   
        HAL_OA_STATE_MACHINE_LOCK();
        struct oa_tc6_desc  *desc = (struct oa_tc6_desc *)cb_param;

        oa_tc6_state_machine(desc);
        HAL_OA_STATE_MACHINE_UNLOCK();
}

/**
 * @brief Get a frame buffer that can be filled by user.
 * @param desc - the OA TC6 descriptor.
 * @param buffer - buffer containing the frame to be transmitted.
 * @return 0 in case of success, negative error code otherwise
 */
int oa_tc6_get_tx_frame(struct oa_tc6_desc *desc,
			struct oa_tc6_frame_buffer **buffer)
{
	if (!desc)
		return -EINVAL;

	for (int i = 0; i < OA_TX_FRAME_BUFF_NUM; i++) {
		if (desc->user_tx_frame_buffer[i].state == OA_BUFF_FREE) {
			*buffer = &desc->user_tx_frame_buffer[i];
			desc->user_tx_frame_buffer[i].state = OA_BUFF_TX_BUSY;
			memset(desc->user_tx_frame_buffer[i].data, 0,
			       CONFIG_OA_CHUNK_BUFFER_SIZE);

			return 0;
		}
	}

	return -ENOBUFS;
}

/**
 * @brief Mark a frame buffer as ready to be transmitted.
 * @param desc - the OA TC6 descriptor.
 * @param buffer - buffer containing the frame to be transmitted.
 * @return 0 in case of success, negative error code otherwise
 */
int oa_tc6_put_tx_frame(struct oa_tc6_desc *desc,
			struct oa_tc6_frame_buffer *buffer)
{
	if (!desc || !buffer)
		return -EINVAL;

	buffer->state = OA_BUFF_TX_READY;

	return 0;
}

/**
 * @brief Get the first TX frame buffer that is ready to be transmitted.
 * @param desc - the OA TC6 descriptor.
 * @param buffer - buffer to be filled with the frame to be transmitted.
 * @return 0 in case of success, negative error code otherwise
 */
static int oa_tc6_get_first_tx_frame(struct oa_tc6_desc *desc,
				     struct oa_tc6_frame_buffer **buffer)
{
	for (int i = 0; i < OA_TX_FRAME_BUFF_NUM; i++) {
		if (desc->user_tx_frame_buffer[i].state == OA_BUFF_TX_READY) {
			*buffer = &desc->user_tx_frame_buffer[i];

			return 0;
		}
	}

	return -ENOENT;
}

/**
 * @brief Get an empty RX buffer.
 * @param desc - the OA TC6 descriptor.
 * @param buffer - buffer to be filled with the frame received.
 * @param new_buffer - if true, get a new buffer, otherwise get
 * the buffer that is currently being written.
 * @return 0 in case of success, negative error code otherwise
 */
static int oa_tc6_get_empty_rx_buff(struct oa_tc6_desc *desc,
				    struct oa_tc6_frame_buffer **buffer,
				    bool new_buffer)
{
	if (!new_buffer) {
		for (int i = 0; i < OA_RX_FRAME_BUFF_NUM; i++) {
			if (desc->user_rx_frame_buffer[i].state == OA_BUFF_RX_IN_PROGRESS) {
				*buffer = &desc->user_rx_frame_buffer[i];

				return 0;
			}
		}
	}

	for (int i = 0; i < OA_RX_FRAME_BUFF_NUM; i++) {
		if (desc->user_rx_frame_buffer[i].state == OA_BUFF_FREE) {
			*buffer = &desc->user_rx_frame_buffer[i];
			desc->user_rx_frame_buffer[i].index = 0;
			desc->user_rx_frame_buffer[i].len = 0;

			return 0;
		}
	}

	return -ENOBUFS;
}

/**
 * @brief Get a frame buffer that is ready to be read by the user.
 * The VS field in the chunk footer fields should match what is provided
 * @param desc - the OA TC6 descriptor.
 * @param buffer - buffer containing the frame received.
 * @param vs - the value of the VS field to match.
 * @param mask - the mask to apply to the VS field.
 * @return 0 in case of success, negative error code otherwise
 */
int oa_tc6_get_rx_frame_match_vs(struct oa_tc6_desc *desc,
				 struct oa_tc6_frame_buffer **buffer,
				 uint8_t vs, uint8_t mask)
{
	for (int i = 0; i < OA_RX_FRAME_BUFF_NUM; i++) {
		if (desc->user_rx_frame_buffer[i].state == OA_BUFF_RX_COMPLETE &&
		    (vs & mask) == (desc->user_rx_frame_buffer[i].vs & mask)) {
			*buffer = &desc->user_rx_frame_buffer[i];
			desc->user_rx_frame_buffer[i].state = OA_BUFF_RX_USER_OWNED;

			return 0;
		}
	}

	return -ENOENT;
}

/**
 * @brief Get a frame buffer that is ready to be read by the user.
 * @param desc - the OA TC6 descriptor.
 * @param buffer - buffer containing the frame received.
 * @return 0 in case of success, negative error code otherwise
 */
int oa_tc6_get_rx_frame(struct oa_tc6_desc *desc,
			struct oa_tc6_frame_buffer **buffer)
{
	for (int i = 0; i < OA_RX_FRAME_BUFF_NUM; i++) {
		if (desc->user_rx_frame_buffer[i].state == OA_BUFF_RX_COMPLETE) {
			*buffer = &desc->user_rx_frame_buffer[i];
			desc->user_rx_frame_buffer[i].state = OA_BUFF_RX_USER_OWNED;

			return 0;
		}
	}

	return -ENOENT;
}

/**
 * @brief Mark a frame buffer as used and ready to be rewritten.
 * @param desc - the OA TC6 descriptor.
 * @param buffer - buffer containing the frame read by the user.
 * @return 0 in case of success, negative error code otherwise
 */
int oa_tc6_put_rx_frame(struct oa_tc6_desc *desc,
			struct oa_tc6_frame_buffer *buffer)
{
	if (!desc || !buffer || buffer->state != OA_BUFF_RX_USER_OWNED)
		return -EINVAL;

	buffer->index = 0;
	buffer->len = 0;
	buffer->state = OA_BUFF_FREE;

	return 0;
}

/**
 * @brief Convert frames in the OA_BUFF_TX_READY state to chunks.
 * Configure empty chunks if we need to receive more then transmit.
 * @param desc - the OA TC6 descriptor
 * @param tx_buffer - the buffer containing the chunks
 * @param tx_credit - the number of chunks available for transmission
 * @param rx_nchunks - the number of chunks available for reception
 * @param tx_written - the number of chunks written in the buffer
 * @return 0 in case of success, negative error code otherwise
 */
static int oa_tc6_tx_frame_to_chunks(struct oa_tc6_desc *desc,
				     uint8_t *tx_buffer,
				     uint32_t tx_credit, uint32_t rx_nchunks,
				     uint32_t *tx_written)
{
	uint32_t spi_buffer_index = 0;
	uint32_t tx_frame_num_chunks;
	uint32_t spi_buff_max_chunks;
	uint32_t chunks_written = 0;
	uint32_t frame_offset = 0;
	uint32_t chunks_limit;
	uint32_t frame_len;
	uint32_t header;
	uint32_t i;
	int ret;

	struct oa_tc6_frame_buffer *frame_buffer;
	spi_buff_max_chunks = OA_SPI_BUFF_LEN / (OA_CHUNK_SIZE + OA_HEADER_LEN);

	/* The maximum number of chunks we can potentially send, given the size of our SPI buffer. */
	chunks_limit = no_os_min(spi_buff_max_chunks, tx_credit);

	do {
		ret = oa_tc6_get_first_tx_frame(desc, &frame_buffer);
		if (ret)
			break;

		frame_len = frame_buffer->len;
		tx_frame_num_chunks = NO_OS_DIV_ROUND_UP(frame_len, OA_CHUNK_SIZE);

		/* Check if we can fit the current frame into the SPI buffer (as a whole). */
		if (!frame_len || ((chunks_written + tx_frame_num_chunks) > chunks_limit))
			break;

		frame_offset = 0;
		for (i = 0; i < tx_frame_num_chunks; i++) {
			header = no_os_field_prep(OA_DATA_HEADER_DNC_MASK, 1);
			header |= no_os_field_prep(OA_DATA_HEADER_DV_MASK, 1);
			header |= no_os_field_prep(OA_DATA_HEADER_VS_MASK, frame_buffer->vs);

			if (!i) {
				header |= no_os_field_prep(OA_DATA_HEADER_SV_MASK, 1);
				header |= no_os_field_prep(OA_DATA_HEADER_TSC_MASK, frame_buffer->tsc);
			}

			if (i == tx_frame_num_chunks - 1) {
				header |= no_os_field_prep(OA_DATA_HEADER_EV_MASK, 1);
				header |= no_os_field_prep(OA_DATA_HEADER_EBO_MASK, frame_len - 1);
			}

			header |= oa_tc6_crc1(header);

			no_os_put_unaligned_be32(header, &tx_buffer[spi_buffer_index]);
			spi_buffer_index += OA_HEADER_LEN;
			memcpy(&tx_buffer[spi_buffer_index], &frame_buffer->data[frame_offset],
			       OA_CHUNK_SIZE);
			frame_offset += OA_CHUNK_SIZE;
			spi_buffer_index += OA_CHUNK_SIZE;

			frame_len -= OA_CHUNK_SIZE;
		}
		chunks_written += tx_frame_num_chunks;

		frame_buffer->len = 0;
		frame_buffer->index = 0;
		frame_buffer->state = OA_BUFF_FREE;
	} while (1);

	/*
	 * The TX queue may be empty, there is no space in the SPI buffer,
	 * or we're out of tx credits.
	 * If rx_chunks > tx_chunks, we need to add dummy chunks (DV = 0) as long
	 * as there is enough room in the buffer.
	 */
	while ((rx_nchunks > chunks_written) && (chunks_written < chunks_limit)) {
		header = no_os_field_prep(OA_DATA_HEADER_DNC_MASK, 1);
		no_os_put_unaligned_be32(header, &tx_buffer[spi_buffer_index]);
		spi_buffer_index += OA_CHUNK_SIZE + OA_HEADER_LEN;
		chunks_written++;
	}

	*tx_written = spi_buffer_index;

	return 0;
}

/**
 * @brief Convert the received chunks into frames.
 * @param desc - the OA TC6 descriptor
 * @param chunks - array containing chunks received from the MAC
 * @param len - the number of chunks
 * @return 0 in case of success, negative error code otherwise
 */
static int oa_tc6_rx_chunk_to_frame(struct oa_tc6_desc *desc, uint8_t *chunks,
				    uint32_t len)
{
	uint32_t footer = 0;
	uint32_t sbo;
	uint32_t ebo;
	uint32_t ev;
	uint32_t sv;
	int ret;

	struct oa_tc6_frame_buffer *frame_buffer;

	ret = oa_tc6_get_empty_rx_buff(desc, &frame_buffer, false);
	if (ret)
		return ret;

	for (uint32_t i = 0; i < len; i++) {
		footer = no_os_get_unaligned_be32(&chunks[OA_CHUNK_SIZE]);

		ev = footer & OA_DATA_FOOTER_EV_MASK;
		sv = footer & OA_DATA_FOOTER_SV_MASK;
		ebo = no_os_field_get(OA_DATA_FOOTER_EBO_MASK, footer);

		/*
		 * The footer contains an SWO (Start WORD offset). Convert to a start
		 * BYTE offset to be used directly in buffer access and math
		 */
		sbo = no_os_field_get(OA_DATA_FOOTER_SWO_MASK, footer) * 4;

		/* Always update the transfer flags, even if DV=0 */
		desc->xfer_flags.flags_valid = true;
		desc->xfer_flags.exst |= !!(footer & OA_DATA_FOOTER_EXST_MASK); /* Latched */
		desc->xfer_flags.hdrb |= !!(footer & OA_DATA_FOOTER_HDRB_MASK); /* Latched */
		desc->xfer_flags.sync = !!(footer &
					   OA_DATA_FOOTER_SYNC_MASK);  /* Instantaneous */

		if (!(footer & OA_DATA_FOOTER_DV_MASK)) {
			chunks += OA_CHUNK_SIZE + OA_FOOTER_LEN;
			continue;
		}

		if (sv && ev) {
			if (sbo > ebo) {
				/* There are 2 frames in the current chunk. Finish the existing. */
				memcpy(&(frame_buffer->data[frame_buffer->index]), chunks, ebo + 1);
				frame_buffer->index += ebo + 1;
				frame_buffer->len = frame_buffer->index;
				frame_buffer->state = OA_BUFF_RX_COMPLETE;

				/* Flags valid when EV=1 Only */
				frame_buffer->frame_drop = !!(footer & OA_DATA_FOOTER_FD_MASK);

				/* Now get a new buffer for the second frame */
				ret = oa_tc6_get_empty_rx_buff(desc, &frame_buffer, true);
				if (ret)
					return ret;

				/*
				 * Overwrite the EBO to be the end of the chunk so the next
				 * block of code is generic
				 */
				ebo = OA_CHUNK_SIZE - 1;

				/* Next frame will be in progress */
				frame_buffer->state = OA_BUFF_RX_IN_PROGRESS;
			} else {
				/* A single frame in current chunk. It will be completed */
				frame_buffer->state = OA_BUFF_RX_COMPLETE;
			}

			/*
			 * At this point it is either a single frame encapsulated in the
			 * chunk, or the start of the 2nd Frame. EBO should be set
			 * accordingly above.
			 */
			memcpy(&frame_buffer->data[frame_buffer->index], &chunks[sbo],
			       ebo - sbo + 1);
			frame_buffer->index += ebo - sbo + 1;
			frame_buffer->len = frame_buffer->index;

			//Flags valid when SV=1
			frame_buffer->rtsa = !!(footer & OA_DATA_FOOTER_RTSA_MASK);
			frame_buffer->rtsp = !!(footer & OA_DATA_FOOTER_RTSP_MASK);

			frame_buffer->vs = no_os_field_get(OA_DATA_FOOTER_VS_MASK, footer);

			if (frame_buffer->state == OA_BUFF_RX_COMPLETE) {
				/* Get a new buffer for the next iteration */
				ret = oa_tc6_get_empty_rx_buff(desc, &frame_buffer, true);
				if (ret)
					return ret;
			}

			chunks += OA_CHUNK_SIZE + OA_FOOTER_LEN;
			continue;
		}

		if (ev) {
			ebo = no_os_field_get(OA_DATA_FOOTER_EBO_MASK, footer);

			/* The current chunk may be shorter than chunk_size, since it contains the end of a frame */
			memcpy(&(frame_buffer->data[frame_buffer->index]), chunks, ebo + 1);
			frame_buffer->len = frame_buffer->index + ebo + 1;
			frame_buffer->state = OA_BUFF_RX_COMPLETE;

			/* Flags valid when EV=1 Only */
			frame_buffer->frame_drop = !!(footer & OA_DATA_FOOTER_FD_MASK);

			/* Get a new buffer for the next iteration */
			ret = oa_tc6_get_empty_rx_buff(desc, &frame_buffer, true);
			if (ret)
				return ret;

			frame_buffer->state = OA_BUFF_RX_IN_PROGRESS;

			chunks += OA_CHUNK_SIZE + OA_FOOTER_LEN;
			continue;
		}

		if (sv) {
			/* The current chunk contains the start of a frame at offset SWO */
			memcpy(&frame_buffer->data[frame_buffer->index], &chunks[sbo],
			       OA_CHUNK_SIZE - sbo);

			frame_buffer->index += OA_CHUNK_SIZE - sbo;
			frame_buffer->len = frame_buffer->index;
			frame_buffer->state = OA_BUFF_RX_IN_PROGRESS;

			//Flags valid when SV=1
			frame_buffer->rtsa = !!(footer & OA_DATA_FOOTER_RTSA_MASK);
			frame_buffer->rtsp = !!(footer & OA_DATA_FOOTER_RTSP_MASK);

			frame_buffer->vs = no_os_field_get(OA_DATA_FOOTER_VS_MASK, footer);
			chunks += OA_CHUNK_SIZE + OA_FOOTER_LEN;
			continue;
		}

		if ((!ev && !sv) && (frame_buffer->state == OA_BUFF_RX_IN_PROGRESS)) {
			/*
			 * The current chunk does not contain either the start or end of a frame,
			 * and the contains valid data for each byte
			 */
			memcpy(&frame_buffer->data[frame_buffer->index], chunks, OA_CHUNK_SIZE);
			frame_buffer->index += OA_CHUNK_SIZE;
			frame_buffer->len = frame_buffer->index;
		}

		chunks += OA_CHUNK_SIZE + OA_FOOTER_LEN;
	}

	desc->data_rx_credit = no_os_field_get(OA_DATA_FOOTER_RCA_MASK, footer);
	desc->data_tx_credit = no_os_field_get(OA_DATA_FOOTER_TXC_MASK, footer);

	return 0;
}

/**
 * @brief Update the number of RX and TX credit available in the MAC FIFOs.
 * @param desc - the OA TC6 descriptor
 * @return 0 in case of success, negative error code otherwise
 */
static int oa_tc6_update_stats(struct oa_tc6_desc *desc)
{
	uint32_t reg_val;
	int ret;

	ret = oa_tc6_reg_read(desc, OA_TC6_BUFST_REG, &reg_val);
	if (ret)
		return ret;

	desc->data_tx_credit = no_os_field_get(OA_TC6_BUFSTS_TXC_MASK, reg_val);
	desc->data_rx_credit = no_os_field_get(OA_TC6_BUFSTS_RCA_MASK, reg_val);

	return 0;
}

/**
 * @brief Gets the latched transfer flags that are read from the data chunk
 * footer. Optionally clears the latched values
 * @param desc - the OA TC6 descriptor
 * @param flags - Storage location for flag values
 * @param clear - If set, clears the latch and valid
 * @return 0 in case of success, negative error code otherwise
 */
int oa_tc6_get_xfer_flags(struct oa_tc6_desc *desc, struct oa_tc6_flags *flags,
			  bool clear)
{
	if (!desc || !flags)
		return -EINVAL;

	memcpy(flags, &desc->xfer_flags, sizeof(struct oa_tc6_flags));

	if (clear)
		memset(&desc->xfer_flags, 0, sizeof(struct oa_tc6_flags));

	return 0;
}

/**
 * @brief Transmit all the frames in the OA_BUFF_TX_READY state and receive the
 * frames in the OA_BUFF_RX_COMPLETE state.
 * @param desc - the OA TC6 descriptor
 * @return 0 in case of success, negative error code otherwise
 */
int oa_tc6_thread(struct oa_tc6_desc *desc)
{
	uint32_t tx_chunks_avail = 0;
	uint32_t rx_limit = 0;
	uint32_t bytes_total;
	int ret;

	struct oa_tc6_frame_buffer *frame_buffer;
	struct capi_spi_transfer xfer = {0};

	if (desc->ctrl_rx_credit || desc->ctrl_tx_credit) {
		xfer.tx_buf = desc->ctrl_chunks;
		xfer.rx_buf = desc->ctrl_chunks;

		if (desc->prote_spi)
			xfer.tx_size = 4 * OA_HEADER_LEN + 2 * OA_REG_LEN;
		else
			xfer.tx_size = 2 * OA_HEADER_LEN + OA_REG_LEN;

		xfer.rx_size = xfer.tx_size;

		return capi_spi_transceive(desc->comm_desc, &xfer);
	}

	ret = oa_tc6_update_stats(desc);
	if (ret)
		return ret;

	if (desc->data_tx_credit) {
		ret = oa_tc6_get_first_tx_frame(desc, &frame_buffer);
		if (!ret)
			tx_chunks_avail = frame_buffer->len;
	}

	while (desc->data_rx_credit || tx_chunks_avail) {
		oa_tc6_tx_frame_to_chunks(desc, desc->data_chunks, desc->data_tx_credit,
					  desc->data_rx_credit, &bytes_total);

		xfer.tx_buf = desc->data_chunks;
		xfer.rx_buf = desc->data_chunks;
		xfer.tx_size = bytes_total;
		xfer.rx_size = bytes_total;

		ret = capi_spi_transceive(desc->comm_desc, &xfer);
		if (ret) {
			memset(desc->data_chunks, 0, bytes_total);

			return ret;
		}

		ret = oa_tc6_rx_chunk_to_frame(desc, desc->data_chunks,
					       bytes_total / (OA_CHUNK_SIZE + OA_HEADER_LEN));
		if (ret)
			return ret;

		ret = oa_tc6_get_first_tx_frame(desc, &frame_buffer);
		if (!ret)
			tx_chunks_avail = frame_buffer->len;
		else
			tx_chunks_avail = 0;

		rx_limit++;

		if (rx_limit > CONFIG_OA_THREAD_RX_LIMIT)
			break;
	}

	return 0;
}

/**
 * @brief Attempt to acquire the SPI state machine for a new transaction.
 * @param desc - the OA TC6 descriptor.
 * @param back_up    - pointer to store the current IRQ enable state before
 *                    disabling interrupts. Only written when is_ctrl is true
 *                    and the SPI state is READY.
 * @param is_ctrl   - if true, prepares for a control transaction: saves IRQ
 *                    state, disables IRQ, and sets the pending control flag,
 *                    then verifies the SPI state is READY. Returns
 *                    ETIMEDOUT if not ready.
 *                    If false, checks only that the state machine is idle.
 *                    Returns EBUSY if not ready.
 * @return 0 if the state machine is available, negative error
 *         code otherwise.
 */
int oa_tc6_wait_get_status(struct oa_tc6_desc *const desc, uint8_t *back_up, bool is_ctrl) 
 {

    if (desc == NULL)
        return -EINVAL;

	if (is_ctrl) 
	{
		struct critical_state basepri = adi_hal_enter_critical_section(HAL_INT_PRI_DMA_SPI_RX);
		
		HAL_OA_STATE_MACHINE_LOCK();
		if (desc->spi_state == OA_SPI_STATE_READY) 
			*back_up = capi_irq_get_enable(desc->eth_irq);//HAL_GetEnableIrq();

		HAL_OA_STATE_MACHINE_UNLOCK();
		
		capi_irq_disable(desc->eth_irq);//HAL_DisableIrq();
		*(desc->pending_ctrl) = true;

		adi_hal_exit_critical_section(basepri);

		HAL_OA_STATE_MACHINE_LOCK();
		
		if (desc->spi_state != OA_SPI_STATE_READY)
			return -ETIMEDOUT;
			
		HAL_OA_STATE_MACHINE_UNLOCK();
		
	} else {
		HAL_OA_STATE_MACHINE_UNLOCK();
		
		if (desc->spi_state != OA_SPI_STATE_READY) 
			return -EBUSY;
			
		HAL_OA_STATE_MACHINE_UNLOCK();
	}
	return 0;
}

/**
 * @brief Wait until the OA SPI state is READY for 50 ms.
 * @param desc     - the OA TC6 descriptor.
 *
 * @return 0 in case of success, negative error code otherwise.
 */  
int oa_tc6_wait_spi_ready(struct oa_tc6_desc *const desc)
{
    /* Already ready, return immediately */
    if (desc->spi_state == OA_SPI_STATE_READY)
        return 0;
    
    /* Wait for SPI state to be ready */
    uint64_t start_us, now_us;
    HAL_OA_STATE_MACHINE_LOCK();
    capi_uptime(&start_us);
    while (desc->spi_state != OA_SPI_STATE_READY) {
        /* Check if timeout is greater than 50 ms, communication timeout.
           Otherwise, the SPI state is ready.                       */
        capi_uptime(&now_us);
        if ((now_us - start_us) >= (uint64_t)SPI_COMMS_TIMEOUT_US)
            return -ETIMEDOUT;
    }
    HAL_OA_STATE_MACHINE_UNLOCK();
    return 0;
}
                             
/**
 * @brief Allocate resources for the OA TC6 driver.
 * @param desc - the device descriptor to be initialized
 * @param param - the device's parameter
 * @return 0 in case of success, negative error code otherwise
 */
int oa_tc6_init(struct oa_tc6_desc **desc, struct oa_tc6_init_param *param)
{
	struct oa_tc6_desc *descriptor;
        
        if(desc == NULL || param == NULL)
                return -EINVAL;
        
        if (param->dev_mem_size < sizeof(desc))
                return -ENOMEM;;
        
        memset(param->p_dev_mem, 0, param->dev_mem_size);
        *desc = (struct oa_tc6_desc *)param->p_dev_mem;
        descriptor = *desc;
    
        descriptor->comm_desc = param->comm_desc;
        descriptor->prote_spi = param->prote_spi;
        descriptor->rx_queue_hp_en = param->rx_queue_hp_en ;
        descriptor->fcs_check_en = param->fcs_check_en;;
        descriptor->num_ports = param->num_ports;
        descriptor->eth_irq = param->eth_irq;

        descriptor->spi_state = OA_SPI_STATE_READY;
        descriptor->spi_err = 0;

        descriptor->ts_format = OA_TS_FORMAT_NONE;

        memset(descriptor->ctrl_tx_buf, 0, OA_CTRL_BUF_SIZE);
        memset(descriptor->ctrl_rx_buf, 0, OA_CTRL_BUF_SIZE);
        memset(descriptor->oa_rx_backup_buf, 0, OA_RX_BACKUP_BUF_SIZE);

        /* Initialize with maximum number of Tx credits */
        descriptor->oa_txc = 31;
        /* Initialize with no Rx chunks available */
        descriptor->oa_rca = 0;
        /* The index in the buffer is 0 */
        descriptor->oa_tx_cur_buf_byte_offset = 0;
        descriptor->oa_rx_cur_buf_byte_offset = 0;
        descriptor->oa_tx_cur_buf_idx = 0;
        descriptor->oa_rx_cur_buf_idx = 0;

        descriptor->oa_cps = (uint32_t)OA_DEFAULT_CPS;
        descriptor->oa_max_chunk_count = (uint32_t)OA_MAX_CHUNK64_COUNT;
        descriptor->oa_rx_use_backup_buf = false;

        descriptor->oa_valid_flag = OA_VALID_FLAG_NONE;
        descriptor->oa_timestamp_split = false;

        descriptor->error_stats.fd_count = 0;
        descriptor->error_stats.invalid_sv_count = 0;
        descriptor->error_stats.invalid_ev_count = 0;
        descriptor->error_stats.ftr_parity_error_count = 0;
        descriptor->error_stats.hdr_parity_error_count = 0;
        descriptor->error_stats.sync_error_count = 0;

        descriptor->oa_trx_size = 0;

        descriptor->wnr = 0;
        descriptor->reg_addr = 0;
        descriptor->reg_data = NULL;
        descriptor->cnt = 0;

        descriptor->oa_rx_buf_chunk_start = 0;
        descriptor->oa_rx_buf_trx_size = 0;

#if CONFIG_OA_ZERO_SWO_ONLY
	/* For now, we'll only support receiving frames with SWO = 0 */
	int ret = oa_tc6_reg_update(descriptor, OA_TC6_CONFIG0_REG,
				    OA_TC6_CONFIG0_ZARFE_MASK,
				    OA_TC6_CONFIG0_ZARFE_MASK);
	if (ret) {
		capi_free(descriptor);

		return ret;
	}
#endif
	return 0;
}

/**
 * @brief Free a device descriptor
 * @param desc - the device descriptor to be removed.
 * @return 0
 */
int oa_tc6_remove(struct oa_tc6_desc *desc)
{
	if (!desc)
		return -ENODEV;

        desc->spi_state = OA_SPI_STATE_UNINITIALIZED;
        desc->comm_desc = NULL;
        memset(desc, 0, sizeof(struct oa_tc6_desc));
        
	return 0;
}

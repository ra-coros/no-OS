/*******************************************************************************
 *   @file   oa_tc6.c
 *   @brief  Implementation for the Open Alliance TC6 SPI driver.
 *   @author Ciprian Regus (ciprian.regus@analog.com)
 ********************************************************************************
 * Copyright 2025(c) Analog Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *******************************************************************************/
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "capi_alloc.h"
#include "oa_tc6.h"

/*
 * Optional DMA offload.
 *
 * DMA is not part of the portable CAPI SPI interface, so the backend is
 * chosen at build time: define exactly one OA_TC6_SPI_DMA_* flag and the
 * matching platform entry point is wired in below. With no flag set this
 * file never references a platform symbol and simply uses the portable
 * async transfer. Mirrors the pattern in adin1110_capi_v2/adin1110.c.
 *
 * Forced on here (rather than left to the project build settings) so the
 * STM32 DMA path is always compiled in for this driver.
 */
#ifndef OA_TC6_SPI_DMA_STM32
#define OA_TC6_SPI_DMA_STM32
#endif

#if defined(OA_TC6_SPI_DMA_STM32)
#include "stm32_capi_spi.h"
#define OA_TC6_SPI_DMA_ASYNC	stm32_capi_spi_transfer_dma_async
#endif

int oa_rx_chunk_to_frame(struct oa_tc6_desc *desc, uint8_t *chunks,
			 uint32_t len);

/* Async-path SPI DMA buffers now live per-instance in struct oa_tc6_desc
 * (spi_tx_raw/spi_rx_raw); each function derives a 4-byte-aligned view. */
static int oa_tc6_state_machine (struct oa_tc6_desc *desc);
static int oa_tc6_start_ctrl_transaction (struct oa_tc6_desc *const desc);
static int oa_tc6_end_ctrl_transaction (struct oa_tc6_desc *const desc);
static int oa_tc6_start_data_transaction (struct oa_tc6_desc *const desc);
static int oa_tc6_end_data_transaction (struct oa_tc6_desc *const desc);
static int oa_tc6_start_irq(struct oa_tc6_desc *const desc);

static int oa_tc6_spi_process(struct oa_tc6_desc *const desc);
static int oa_tc6_create_next_chunk(struct oa_tc6_desc *const desc,
				    uint8_t *buf, bool tx_en);

static void oa_tc6_ctrl_cmd_header(uint8_t *dst_buf, uint32_t wnr,uint32_t addr,
				   uint32_t cnt);
static int oa_tc6_ctrl_cmd_read_data(uint32_t *dst, uint8_t *src, uint32_t cnt,
				     bool prote_spi);
static void oa_tc6_ctrl_cmd_write_data(uint8_t *dst, uint32_t *src,
				       uint32_t cnt, bool prote_spi);

static bool oa_tc6_chunk_error_detector(struct oa_tc6_desc *const desc,
					uint32_t rx_footer);
static void oa_tc6_timestamp_handler(struct oa_tc6_desc *const desc,
				     uint32_t spi_rx_buf_byte_offset, uint32_t *frame_dest, uint8_t *timestamp_bytes,
				     uint32_t *byte_offset, enum oa_tc6_ts_operation bit_operation);
static void oa_tc6_cb_function_caller(struct oa_tc6_desc *const desc,
				      uint32_t tail, uint32_t event, uint8_t num_ports);
static void oa_tc6_end_data_chunk_processor(struct oa_tc6_desc *const desc,
		uint32_t *event, uint8_t *rx_buf, uint32_t oa_rx_footer, uint32_t chunk_start);
static void oa_tc6_process_64_bit_timestamp(struct oa_tc6_desc *const desc,
		uint32_t chunk_start, uint32_t chunk_size, uint8_t *timestamp_bytes,
		uint32_t *byte_offset);
static void oa_tc6_full_frame_in_chunk_process(struct oa_tc6_desc *const desc,
		uint32_t *event, uint32_t fcs_size, uint8_t *rx_buf);
static void oa_tc6_start_data_chunk_processor(struct oa_tc6_desc *desc,
		uint32_t *event, uint32_t *byte_offset, uint32_t chunk_start,
		uint8_t *timestamp_bytes, uint32_t chunk_size, uint8_t *rx_buf,
		uint32_t oa_rx_footer);
static void oa_tc6_mid_data_chunk_processor(struct oa_tc6_desc *const desc,
		uint32_t chunk_size, uint32_t oa_rx_footer, uint8_t *rx_buf, uint32_t *event,
		uint32_t byte_offset, uint32_t chunk_start);
static void oa_tc6_complete_transmission_checker(struct oa_tc6_desc *const desc,
		uint32_t tx_header, uint32_t *event);

static int oa_tc6_get_first_tx_frame(struct oa_tc6_desc *desc,
				     struct oa_tc6_frame_buffer **buffer);
static int oa_tc6_get_empty_rx_buff(struct oa_tc6_desc *desc,
				    struct oa_tc6_frame_buffer **buffer, bool new_buffer);

static int oa_tc6_transceive_select(struct oa_tc6_desc *const desc,
				    struct capi_spi_transfer *xfer, uint32_t byte_count);


static int oa_tc6_ctrl_setup(uint8_t *buf, uint32_t wnr, uint32_t reg_addr,
			     uint32_t *reg_data, uint32_t *len, bool prote_spi);

/* Self-contained queue helpers */
#define OA_TC6_QUEUE_PMOD(N, D) (((N) < (D)) ? (N) : ((N) - (D)))

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
			xfer.tx_size = 2 * (OA_HEADER_LEN + OA_REG_LEN);
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

static inline uint32_t oa_tc6_net_queue_count(const struct oa_tc6_net_queue *q)
{
	uint32_t cur_head = q->head;
	uint32_t cur_tail = q->tail;
	uint32_t cur_num_entries = q->num_entries;
	uint32_t n = cur_head + cur_num_entries - cur_tail;

	return OA_TC6_QUEUE_PMOD(n, cur_num_entries);
}

static inline int oa_tc6_net_queue_init(struct oa_tc6_net_queue *q,
					void *entries, uint32_t num_entries)
{
	if (q == NULL || entries == NULL || num_entries == 0)
		return -EINVAL;

	q->entries = entries;
	q->num_entries = num_entries;
	q->head = 0;
	q->tail = 0;

	return 0;
}

static inline bool oa_tc6_net_queue_is_full(const struct oa_tc6_net_queue *q)
{
	return (q->num_entries - 1) == oa_tc6_net_queue_count(q);
}

static inline bool oa_tc6_net_queue_is_empty(const struct oa_tc6_net_queue *q)
{
	uint32_t cur_head = q->head;
	uint32_t cur_tail = q->tail;

	return cur_head == cur_tail;
}

static inline void oa_tc6_net_queue_add_entry(struct oa_tc6_net_queue *q)
{
	q->head = OA_TC6_QUEUE_PMOD(q->head + 1, q->num_entries);
}

static inline void oa_tc6_net_queue_remove_entry(struct oa_tc6_net_queue *q)
{
	q->tail = OA_TC6_QUEUE_PMOD(q->tail + 1, q->num_entries);
}

#ifdef OA_TC6_SPI_DMA_ASYNC
/**
 * @brief       Completion callback for OA_TC6_SPI_DMA_ASYNC() transfers.
 *
 * stm32_capi_spi_transfer_dma_async()'s callback is a plain void(*)(void*)
 * with no event/error parameter, unlike oa_tc6_spi_callback()'s
 * capi_spi_callback_t signature which the IRQ/blocking paths use.
 * desc->last_dma_result carries the outcome across that gap:
 * oa_tc6_transceive_select() records the DMA starter's return code there
 * before this fires, so a start-time failure (bad channel config,
 * HAL_ERROR, etc.) still reaches oa_tc6_spi_callback() as
 * CAPI_SPI_EVENT_ERROR instead of being silently treated as success.
 *
 * @param [in]  arg      The oa_tc6_desc, as passed to OA_TC6_SPI_DMA_ASYNC().
 */
static void oa_tc6_dma_done_thunk(void *arg)
{
	struct oa_tc6_desc *desc = (struct oa_tc6_desc *)arg;

	oa_tc6_spi_callback(desc->last_dma_result ? CAPI_SPI_EVENT_ERROR :
			    CAPI_SPI_EVENT_XFR_DONE, arg, 0);
}
#endif /* OA_TC6_SPI_DMA_ASYNC */

/**
 * @brief       Select blocking, DMA, or interrupt-driven SPI delivery, and
 *              run the transfer.
 *
 * Centralizes the transport decision used at every SPI transceive call
 * site in this file. The blocking transport (capi_spi_transceive()) has no
 * ISR/DMA-completion path of its own to fire oa_tc6_spi_callback(), so this
 * helper fires it explicitly once the blocking call returns. The IT
 * transport still gets its callback invocation from the platform's own
 * completion handling. The DMA transport (when OA_TC6_SPI_DMA_ASYNC is
 * defined) fires it via oa_tc6_dma_done_thunk() once
 * stm32_capi_spi_transfer_dma_async() completes; without a DMA backend
 * configured, DMA-sized transfers fall back to the portable
 * capi_spi_transceive_dma_async(), which uses the platform's own
 * completion handling like the IT transport.
 *
 * @param [in]  desc         The OA TC6 descriptor.
 * @param [in]  xfer         The transfer to run.
 * @param [in]  byte_count   Transfer size used for the DMA threshold check.
 * @return      0 in case of success, negative error code otherwise.
 */
static int oa_tc6_transceive_select(struct oa_tc6_desc *const desc,
				    struct capi_spi_transfer *xfer,
				    uint32_t byte_count)
{
	int ret;

	if (desc->blocking) {
		ret = capi_spi_transceive(desc->comm_desc, xfer);
		oa_tc6_spi_callback(ret ? CAPI_SPI_EVENT_ERROR :
				    CAPI_SPI_EVENT_XFR_DONE, desc, ret);
		return ret;
	}

	if (byte_count >= MIN_SIZE_FOR_DMA) {
		ret = OA_TC6_SPI_DMA_ASYNC(desc->comm_desc, xfer,
					   oa_tc6_dma_done_thunk, desc);
		desc->last_dma_result = ret;
		return ret;
	}

	return capi_spi_transceive_async(desc->comm_desc, xfer);
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
	header |= oa_tc6_crc1(header);

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
static void oa_tc6_ctrl_cmd_write_data(uint8_t *dst, uint32_t *src,
				       uint32_t cnt,
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
	struct oa_tc6_net_queue *tx_queue = desc->tx_queue;
	struct oa_tc6_net_queue *rx_queue = *desc->rx_queue;
	struct oa_tc6_frame_struct *entries = tx_queue->entries;
	struct oa_tc6_frame_struct *frame = NULL;
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
	if (desc->oa_rx_use_backup_buf || oa_tc6_net_queue_is_empty(rx_queue))
		tx_header |= no_os_field_prep(OA_DATA_HEADER_NORX_MASK, 1);

	if (tx_en && (oa_tc6_net_queue_is_full(tx_queue) ||
		      (desc->oa_tx_cur_buf_idx != tx_queue->head))) {
		frame = &entries[desc->oa_tx_cur_buf_idx];
	}

	if (frame == NULL) {
		tx_header |= no_os_field_prep(OA_DATA_HEADER_DV_MASK, 0);
	} else {
		tx_header |= no_os_field_prep(OA_DATA_HEADER_DV_MASK, 1);

		/* Set VS for every chunk with valid data (port selection) */
		if (desc->num_ports == 2)
			tx_header |= no_os_field_prep(OA_DATA_HEADER_VS_MASK,
						      frame->buf_desc->port & 0x1);

		/* Have we already transmitted bytes from this frame? */
		if (!desc->oa_tx_cur_buf_byte_offset)
			first_chunk = true;

		bytes_remaining = frame->buf_desc->trx_size - desc->oa_tx_cur_buf_byte_offset;

		if (bytes_remaining > chunk_bytes_remaining)
			bsize = chunk_bytes_remaining;
		else
			bsize = bytes_remaining;

		bytes_remaining -= bsize;
		chunk_bytes_remaining -= bsize;

		memcpy(&buf[chunk_byte_idx],
		       &frame->buf_desc->buf[desc->oa_tx_cur_buf_byte_offset], bsize);
		desc->oa_tx_cur_buf_byte_offset += bsize;

		if (first_chunk) {
			tx_header |= no_os_field_prep(OA_DATA_HEADER_SV_MASK, 1);
			tx_header |= no_os_field_prep(OA_DATA_HEADER_SWO_MASK, 0);
			tx_header |= no_os_field_prep(OA_DATA_HEADER_TSC_MASK,
						      (uint32_t)(frame->buf_desc->egress_capt));
			first_chunk = false;
		}

		if (bytes_remaining == 0) {
			tx_header |= no_os_field_prep(OA_DATA_HEADER_EV_MASK, 1);
			tx_header |= no_os_field_prep(OA_DATA_HEADER_EBO_MASK, bsize - 1);
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
			if (!no_os_field_get(OA_DATA_HEADER_SV_MASK, tx_header)) {
				frame = &entries[desc->oa_tx_cur_buf_idx];
				swo = (bsize + 3) / 4;

				/* If we already have a valid EV, make sure
				than the available bytes in the chunk. */
				if (no_os_field_get(OA_DATA_HEADER_EV_MASK, tx_header) &&
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

				tx_header |= no_os_field_prep(OA_DATA_HEADER_SV_MASK, 1);
				tx_header |= no_os_field_prep(OA_DATA_HEADER_SWO_MASK, swo);

				/* Update VS for the new frame's port */
				if (desc->num_ports == 2) {
					tx_header &= ~OA_DATA_HEADER_VS_MASK;
					tx_header |= no_os_field_prep(
							     OA_DATA_HEADER_VS_MASK,
							     (frame->buf_desc->port & 0x1));
				}

				tx_header |=  no_os_field_prep(OA_DATA_HEADER_TSC_MASK,
							       (uint32_t)(frame->buf_desc->egress_capt));
			}
		}
	}

	/* Calculate parity and write header to buffer */
	tx_header |= oa_tc6_crc1(tx_header);

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
	struct oa_tc6_net_queue *tx_queue= NULL;
	struct oa_tc6_frame_struct *frame_entries = NULL;
	uint32_t chunk_count = 0;
	uint32_t tx_chunk_count  = 0;
	uint32_t queue_byte_count = 0;
	uint8_t *buf;
	uint32_t tx_idx;
	bool tx_en;

	if (!desc)
		return -ENODEV;

	uint8_t *spi_tx_buf = (uint8_t *)no_os_align((uintptr_t)desc->spi_tx_raw, 4);

	tx_queue = desc->tx_queue;
	frame_entries = tx_queue->entries;

	/* First figure out how many chunks to handle in the SPI transaction */
	if (!oa_tc6_net_queue_is_empty(tx_queue)) {
		tx_idx = desc->oa_tx_cur_buf_idx;
		queue_byte_count = 0;
		do {
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

	if (desc->oa_rx_use_backup_buf && (chunk_count > OA_RX_BACKUP_BUF_CHUNK_COUNT))
		chunk_count = OA_RX_BACKUP_BUF_CHUNK_COUNT;

	/* This is from IRQ handler: the host needs to initiate a data transfer
	in response to an IRQ. */
	if ((!chunk_count) && (desc->spi_state == OA_SPI_STATE_IRQ_START)) {
		/* Fixed to a minimum transfer size is 1 chunk. */
		chunk_count = 1;
	}

	if (desc->oa_rx_use_backup_buf)
		buf = &desc->oa_rx_backup_buf[0];
	else
		buf = &spi_tx_buf[0];

	desc->oa_trx_size = 0;
	for (uint32_t i = 0; i < chunk_count; i++) {
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

	uint8_t *spi_tx_buf = (uint8_t *)no_os_align((uintptr_t)desc->spi_tx_raw, 4);
	uint8_t *spi_rx_buf = (uint8_t *)no_os_align((uintptr_t)desc->spi_rx_raw, 4);

	ret = oa_tc6_spi_process(desc);

	if ((ret == 0) && desc->oa_trx_size) {
		desc->spi_state = OA_SPI_STATE_DATA_END;

		xfer.tx_buf = spi_tx_buf;
		xfer.rx_buf = spi_rx_buf;
		xfer.tx_size = desc->oa_trx_size;
		xfer.rx_size = desc->oa_trx_size;

		ret = oa_tc6_transceive_select(desc, &xfer, desc->oa_trx_size);
	} else {
		desc->spi_state = OA_SPI_STATE_READY;

		if (!*(desc->pending_ctrl))
			capi_irq_enable(desc->eth_irq);
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
		if (oa_tc6_crc1(rx_footer)) {
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
	struct oa_tc6_net_queue *rx_queue = *desc->rx_queue;
	struct oa_tc6_frame_struct *entries = rx_queue->entries;
	uint32_t timestamp_byte_offset = 0;
	bool is_calculate_parity = false;
	uint8_t  bytes = 0;
	uint8_t *spi_rx_buf = (uint8_t *)no_os_align((uintptr_t)desc->spi_rx_raw, 4);

	switch (bit_operation) {
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
		uint8_t acc = 0;
		for (uint8_t i = 0; i < bytes; i++)
			acc ^= timestamp_bytes[i];
		entries[rx_queue->tail].buf_desc->timestamp_valid =
			((oa_tc6_crc1(acc) ^ 1u) != desc->oa_timestamp_parity);
	}
}

static void oa_tc6_cb_function_caller(struct oa_tc6_desc *const desc,
				      uint32_t tail, uint32_t event, uint8_t num_ports)
{
	struct oa_tc6_net_queue *rx_queue = *desc->rx_queue;
	struct oa_tc6_frame_struct *entries = rx_queue->entries;

	if (num_ports == 2) {
		/* Dual-port dynamic forwarding table update; FCS errors flow via event arg. */
		if (desc->host_ops.rx_frame_ready)
			desc->host_ops.rx_frame_ready(desc->status_ctx, event, entries[tail].buf_desc);
	}

	if (entries[tail].buf_desc->cb_func) {
		entries[tail].buf_desc->cb_func((desc->app_device), event,
						entries[tail].buf_desc);
	}

	desc->oa_rx_cur_buf_byte_offset = 0;
}


static void oa_tc6_end_data_chunk_processor(struct oa_tc6_desc *const desc,
		uint32_t *event, uint8_t *rx_buf, uint32_t oa_rx_footer, uint32_t chunk_start)
{
	struct oa_tc6_net_queue *rx_queue = *desc->rx_queue;
	struct oa_tc6_frame_struct *entries = rx_queue->entries;
	uint32_t tail;
	uint32_t ebo;
	uint32_t sbo;
	uint8_t *spi_rx_buf = (uint8_t *)no_os_align((uintptr_t)desc->spi_rx_raw, 4);

	/* If FD = 1, frame needs to be dropped */
	if (no_os_field_get(OA_DATA_FOOTER_FD_MASK, oa_rx_footer)) {
		/* Reset the receive index to reuse current buffer for next frame */
		desc->oa_rx_cur_buf_byte_offset = 0;
		desc->error_stats.fd_count++;
		return;
	}

	/* Special case: a full frame is sent in a single chunk, this can occur */
	/* when frame is 64 bytes (including FCS) and the chunk is 64 bytes.    */
	/* This case is handled by SV=1 case.                                   */
	struct oa_tc6_critical_state basepri = oa_tc6_enter_critical_section(
			OA_TC6_INT_PRI_DMA_SPI_TX);
	ebo = no_os_field_get(OA_DATA_FOOTER_EBO_MASK, oa_rx_footer);
	sbo = no_os_field_get(OA_DATA_FOOTER_SWO_MASK,
			      oa_rx_footer) * (sizeof(uint32_t));

	if (!(no_os_field_get(OA_DATA_FOOTER_SV_MASK, oa_rx_footer) && (ebo > sbo))) {
		if (desc->oa_valid_flag != OA_VALID_FLAG_START) {

			/* No prior SV was received, this is an error */
			/* Reset the receive index to reuse current buffer for next frame */
			desc->oa_rx_cur_buf_byte_offset = 0;

			desc->error_stats.invalid_ev_count++;
		} else {
			tail = rx_queue->tail;
			if (desc->oa_rx_cur_buf_byte_offset + ebo + 1 >
			    entries[tail].buf_desc->buf_size) {
				/* Rx buffer too small for the incoming frame, notify the user.*/
				*event |= OA_TC6_CB_STATUS_RX_BUF_OVF;
			} else {
				memcpy(&rx_buf[desc->oa_rx_cur_buf_byte_offset], &spi_rx_buf[chunk_start],
				       ebo + 1);
				entries[tail].buf_desc->trx_size = desc->oa_rx_cur_buf_byte_offset + ebo + 1;
				if (desc->host_ops.fcs_check)
					desc->host_ops.fcs_check(desc->status_ctx, entries[tail].buf_desc->trx_size,
								 (uint8_t *)rx_buf, entries[tail].buf_desc, event);
			}

			oa_tc6_net_queue_remove_entry(rx_queue);
			oa_tc6_cb_function_caller(desc, tail, *event, 2);
		}
		desc->oa_valid_flag = OA_VALID_FLAG_END;
	}
	oa_tc6_exit_critical_section(basepri);
}

static void oa_tc6_process_64_bit_timestamp(struct oa_tc6_desc *const desc,
		uint32_t chunk_start, uint32_t chunk_size, uint8_t *timestamp_bytes,
		uint32_t *byte_offset)
{
	struct oa_tc6_net_queue *rx_queue = *desc->rx_queue;
	struct oa_tc6_frame_struct *entries = rx_queue->entries;
	uint32_t *timestamp_ext = &entries[rx_queue->tail].buf_desc->timestamp_ext;
	uint32_t *timestamp = &entries[rx_queue->tail].buf_desc->timestamp;
	uint32_t spi_rx_buf_byte_offset = chunk_start + *byte_offset;

	/* Process upper 32 bits first (timestamp_ext) */
	oa_tc6_timestamp_handler(desc, spi_rx_buf_byte_offset, timestamp_ext,
				 timestamp_bytes, byte_offset, OA_TS_UPPER_32_BIT);

	desc->oa_timestamp_split = true;

	/* If there is not room in the chunk for the lower 32b of the timestamp, */
	/* those bits will be in the next chunk.                                  */
	if (!((chunk_start + *byte_offset + OA_TIMESTAMP_32BIT_SIZE) >
	      (chunk_start + chunk_size))) {
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
	struct oa_tc6_net_queue *rx_queue = *desc->rx_queue;
	struct oa_tc6_frame_struct *entries = rx_queue->entries;
	uint32_t tail;

	if (desc->host_ops.fcs_check)
		desc->host_ops.fcs_check(desc->status_ctx, fcs_size, rx_buf,
					 entries[rx_queue->tail].buf_desc, event);
	entries[rx_queue->tail].buf_desc->trx_size = fcs_size;

	tail = (*desc->rx_queue)->tail;
	oa_tc6_net_queue_remove_entry(*desc->rx_queue);
	oa_tc6_cb_function_caller(desc, tail, *event, 2);

	desc->oa_valid_flag = OA_VALID_FLAG_END;
}


static void oa_tc6_start_data_chunk_processor(struct oa_tc6_desc *desc,
		uint32_t *event, uint32_t *byte_offset,
		uint32_t chunk_start, uint8_t *timestamp_bytes,
		uint32_t chunk_size, uint8_t *rx_buf,
		uint32_t oa_rx_footer)
{
	struct oa_tc6_net_queue *rx_queue = *desc->rx_queue;
	struct oa_tc6_frame_struct *entries = rx_queue->entries;
	uint32_t vs = no_os_field_get(OA_DATA_FOOTER_VS_MASK, oa_rx_footer);
	uint32_t ebo = no_os_field_get(OA_DATA_FOOTER_EBO_MASK, oa_rx_footer);
	uint32_t sbo = no_os_field_get(OA_DATA_FOOTER_SWO_MASK,
				       oa_rx_footer) * sizeof(uint32_t);
	enum oa_tc6_rx_fifo_prio prio = (enum oa_tc6_rx_fifo_prio)((vs & 0x2) >> 1);
	uint8_t *spi_rx_buf = (uint8_t *)no_os_align((uintptr_t)desc->spi_rx_raw, 4);

	entries[rx_queue->tail].buf_desc->timestamp_valid = false;

	if ((desc->oa_valid_flag != OA_VALID_FLAG_NONE) &&
	    (desc->oa_valid_flag != OA_VALID_FLAG_END)) {
		/* No prior EV was received, this is an error */
		desc->error_stats.invalid_sv_count++;
	}

	if (desc->rx_queue_hp_en) {
		if (prio == OA_TC6_RX_FIFO_PRIO_LOW)
			*(desc->rx_queue) = desc->rx_queue_lp;
		else
			*(desc->rx_queue) = desc->rx_queue_hp;
	}

	if (oa_tc6_net_queue_is_empty(rx_queue) && !desc->oa_rx_use_backup_buf) {
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
		*byte_offset = no_os_field_get(OA_DATA_FOOTER_SWO_MASK,
					       oa_rx_footer) * sizeof(uint32_t);

		if (no_os_field_get(OA_DATA_FOOTER_RTSA_MASK, oa_rx_footer)) {
			/* Store the parity in case the timestamp is split across multiple chunks. */
			/* RTSP is only valid for the chunk in which RTSA is 1.                    */
			desc->oa_timestamp_parity = no_os_field_get(OA_DATA_FOOTER_RTSP_MASK,
				oa_rx_footer);
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
	struct oa_tc6_net_queue *rx_queue = *desc->rx_queue;
	struct oa_tc6_frame_struct *entries = rx_queue->entries;
	uint32_t tail;
	uint8_t *spi_rx_buf = (uint8_t *)no_os_align((uintptr_t)desc->spi_rx_raw, 4);

	if (!no_os_field_get(OA_DATA_FOOTER_EV_MASK, oa_rx_footer) &&
	    (desc->oa_valid_flag == OA_VALID_FLAG_START)) {
		if (desc->oa_rx_cur_buf_byte_offset + chunk_size >
		    entries[rx_queue->tail].buf_desc->buf_size) {
			/* Rx buffer too small for the incoming frame, notify the user.      */
			/* At this point the current frame reception finishes and callback is */
			/* executed.                                                          */
			*event |= OA_TC6_CB_STATUS_RX_BUF_OVF;

			tail = (*desc->rx_queue)->tail;
			oa_tc6_net_queue_remove_entry(*(desc->rx_queue));

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
	struct oa_tc6_net_queue *tx_queue = desc->tx_queue;
	struct oa_tc6_frame_struct *entries = tx_queue->entries;
	uint32_t tail;

	if (no_os_field_get(OA_DATA_HEADER_EV_MASK, tx_header)) {
		tail = tx_queue->tail;
		oa_tc6_net_queue_remove_entry(desc->tx_queue);

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
	struct oa_tc6_net_queue *rx_queue = *desc->rx_queue;
	struct oa_tc6_frame_struct *entries = rx_queue->entries;
	uint32_t chunk_size = (1 << desc->oa_cps);
	uint32_t rx_footer;
	uint32_t tx_header;
	uint32_t exst = 0;
	uint32_t byte_offset;
	uint32_t len;
	uint32_t event = OA_TC6_CB_STATUS_RX_BUF_OVF;
	uint8_t *rx_buf;
	uint8_t timestamp_bytes[TS_BYTES_SIZE];
	int ret = 0;
	uint8_t *spi_tx_buf = (uint8_t *)no_os_align((uintptr_t)desc->spi_tx_raw, 4);
	uint8_t *spi_rx_buf = (uint8_t *)no_os_align((uintptr_t)desc->spi_rx_raw, 4);

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

			if (no_os_field_get(OA_DATA_FOOTER_EV_MASK, rx_footer)) {
				oa_tc6_end_data_chunk_processor(desc, &event, rx_buf, rx_footer, chunk_start);
			}

			if (no_os_field_get(OA_DATA_FOOTER_SV_MASK, rx_footer)) {
				oa_tc6_start_data_chunk_processor(desc, &event,
								  &byte_offset, chunk_start,
								  timestamp_bytes, chunk_size,
								  rx_buf, rx_footer);
			} else {
				oa_tc6_mid_data_chunk_processor(desc, chunk_size,
								rx_footer, rx_buf, &event,
								byte_offset, chunk_start);
			}
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

		ret = oa_tc6_transceive_select(desc, &xfer, len);

	} else {
		if ((!oa_tc6_net_queue_is_empty(&*(desc->tx_queue)) && desc->oa_txc) ||
		    (!oa_tc6_net_queue_is_empty(*(desc->rx_queue)) && desc->oa_rca)) {

			ret = oa_tc6_spi_process(desc);

			desc->spi_state = OA_SPI_STATE_DATA_END;

			xfer.tx_buf = spi_tx_buf;
			xfer.rx_buf = spi_rx_buf;
			xfer.tx_size = desc->oa_trx_size;
			xfer.rx_size = desc->oa_trx_size;

			ret = oa_tc6_transceive_select(desc, &xfer, desc->oa_trx_size);
		} else {
			desc->spi_state = OA_SPI_STATE_READY;

			if (!*(desc->pending_ctrl))
				capi_irq_enable(desc->eth_irq);
		}
	}

	return ret;
}

static int oa_tc6_start_irq(struct oa_tc6_desc *const desc)
{
	struct capi_spi_transfer xfer;
	uint32_t tx_header = 0;
	int ret = 0;
	uint8_t *spi_tx_buf = (uint8_t *)no_os_align((uintptr_t)desc->spi_tx_raw, 4);
	uint8_t *spi_rx_buf = (uint8_t *)no_os_align((uintptr_t)desc->spi_rx_raw, 4);

	/* Single data chunk in response to an IRQ */
	tx_header |= no_os_field_prep(OA_DATA_HEADER_DNC_MASK, 1);
	tx_header |= no_os_field_prep(OA_DATA_HEADER_NORX_MASK, 1);
	tx_header |= oa_tc6_crc1(tx_header);

	no_os_put_unaligned_be32(tx_header, &spi_tx_buf[0]);

	desc->oa_trx_size = (uint32_t)OA_HEADER_SIZE + (1 << desc->oa_cps);

	desc->spi_state = OA_SPI_STATE_DATA_END;

	xfer.tx_buf = spi_tx_buf;
	xfer.rx_buf = spi_rx_buf;
	xfer.tx_size = (uint16_t)desc->oa_trx_size;
	xfer.rx_size = (uint16_t)desc->oa_trx_size;

	ret = oa_tc6_transceive_select(desc, &xfer, desc->oa_trx_size);

	return ret;
}

int oa_tc6_mdio_read_async(struct oa_tc6_desc *desc, uint32_t prtad,
			   uint32_t reg_addr)
{
	struct capi_spi_transfer xfer;
	uint32_t len;
	int ret;

	desc->mdio_cmd = 0;
	desc->mdio_cmd |= no_os_field_prep(MDIO_DEVAD_MASK, DEVTYPE(reg_addr));
	desc->mdio_cmd |= no_os_field_prep(MDIO_ST_MASK, MDIOACC_N_MDIO_ST_CLAUSE45);
	desc->mdio_cmd |= no_os_field_prep(MDIO_PRTAD_MASK, prtad);
	desc->mdio_cmd |= no_os_field_prep(MDIO_OP_MASK, MDIOACC_N_MDIO_OP_MD_ADDR);
	desc->mdio_cmd |= no_os_field_prep(MDIO_DATA_MASK, REG_ADDR(reg_addr));

	desc->wnr = OA_SPI_WRITE;
	desc->reg_addr = 0X020U; /*addr MDIOACC_0 :0X020U */
	desc->reg_data = &desc->mdio_cmd;
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

	ret = oa_tc6_transceive_select(desc, &xfer, len);

	return ret;
}

int oa_tc6_status_clear_resume(struct oa_tc6_desc *desc, uint32_t clr0,
			       uint32_t clr1)
{
	struct capi_spi_transfer xfer;
	int ret = 0;

	if (desc == NULL)
		return -EINVAL;

	/* Clear status bits — write back host-supplied STATUS0/STATUS1 values */
	desc->oa_status_clear[0] = clr0;
	desc->oa_status_clear[1] = clr1;
	desc->wnr = OA_SPI_WRITE;
	desc->reg_addr = 0X008U; /* STATUS0: 0X008U */
	desc->reg_data = &desc->oa_status_clear[0];
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

	ret = oa_tc6_transceive_select(desc, &xfer, desc->cnt);

	return ret;
}

static int oa_tc6_phy_reg_read_step(struct oa_tc6_desc *const desc)
{
	struct capi_spi_transfer xfer;
	uint32_t len;
	int ret = 0;

	desc->mdio_cmd &= ~MDIO_OP_MASK;
	desc->mdio_cmd |= no_os_field_prep(MDIO_OP_MASK, MDIOACC_N_MDIO_OP_MD_RD);
	desc->mdio_cmd &= ~MDIO_DATA_MASK;
	desc->mdio_cmd &= ~MDIO_TRDONE_MASK;

	desc->reg_addr = 0X021U; /* ADDR_MDIOACC_1 :0X021U */
	desc->reg_data = &desc->mdio_cmd;
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

	ret = oa_tc6_transceive_select(desc, &xfer, len);

	return ret;
}

static int oa_tc6_read_phy_reg(struct oa_tc6_desc *const desc)
{
	uint32_t check_hdr;
	uint32_t echo_hdr;
	uint16_t value;
	int ret = 0;

	if (desc == NULL)
		return -EINVAL;

	/* Check header vs. echoed header for errors */
	memcpy(&check_hdr, &desc->ctrl_tx_buf[0], 4);
	memcpy(&echo_hdr, &desc->ctrl_rx_buf[OA_HEADER_SIZE], 4);

	if (check_hdr != echo_hdr) {
		desc->spi_err = 1;
		oa_tc6_status_clear_resume(desc, desc->oa_status_read[0],
					   desc->oa_status_read[1]);
		return ret;
	}

	desc->spi_err = 0;
	/* ADDR_MDIOACC_0: (0X020U) */
	if (desc->reg_addr == 0X020U) {
		/* Second register write */
		desc->wnr = OA_SPI_WRITE;
		oa_tc6_phy_reg_read_step(desc);
		return ret;
	}

	if (desc->wnr == OA_SPI_WRITE) {
		/* Both writes completed, now read */
		desc->wnr = OA_SPI_READ;
		oa_tc6_phy_reg_read_step(desc);
		return ret;
	}

	/* Poll until TRDONE bit is set in the MDIO access register */
	ret = oa_tc6_ctrl_cmd_read_data(&desc->mdio_cmd,
					&desc->ctrl_rx_buf[2 * OA_HEADER_SIZE],
					desc->cnt, desc->prote_spi);

	if (no_os_field_get(MDIO_TRDONE_MASK, desc->mdio_cmd)) {
		/* Vendor PHY interrupt handling is owned by the host MAC layer. */
		value = (uint16_t)no_os_field_get(MDIO_DATA_MASK, desc->mdio_cmd);
		if (desc->host_ops.mdio_done)
			desc->host_ops.mdio_done(desc->status_ctx, desc, value);
	} else {
		/* Keep polling until TRDONE is set */
		desc->wnr = OA_SPI_READ;
		oa_tc6_phy_reg_read_step(desc);
	}

	return ret;
}


static int oa_tc6_read_status(struct oa_tc6_desc *const desc)
{
	int ret = 0;

	if (desc == NULL)
		return -EINVAL;

	ret = oa_tc6_ctrl_cmd_read_data(&desc->oa_status_read[0],
					&desc->ctrl_rx_buf[2 * OA_HEADER_SIZE], 2, desc->prote_spi);

	/* STATUS masking + interpretation is owned by the host MAC layer. */
	if (desc->host_ops.status_begin)
		desc->host_ops.status_begin(desc->status_ctx, desc,
					    desc->oa_status_read[0], desc->oa_status_read[1]);

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
		ret = oa_tc6_read_status(desc);
		break;

	case OA_SPI_STATE_READ_PHY_REG:
		ret = oa_tc6_read_phy_reg(desc);
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
		capi_irq_enable(desc->eth_irq);
	}

	return ret;
}

/**
 * @brief Read a register value asynchronously.
 * @param desc - the OA TC6 descriptor.
 * @param addr - Register address.
 * @param val - Register value.
 * @param num_reg - Number of registers in the burst control transaction
 * @return 0 in case of success, negative error code otherwise
 */
int oa_tc6_reg_read_async(struct oa_tc6_desc *desc, uint32_t addr,
			  uint32_t *val,
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
int oa_tc6_reg_write_async(struct oa_tc6_desc *desc, uint32_t addr,
			   uint32_t *val,
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
 * @brief Add an app TX frame (zero-copy buf_desc) into the OA-owned tx queue.
 * @param desc     - the OA TC6 descriptor.
 * @param header   - the per-frame header.
 * @param buf_desc - the frame buffer descriptor to transmit.
 * @return 0 on success, -ENOSPC if full, negative error code otherwise.
 */
int oa_tc6_submit_tx(struct oa_tc6_desc *desc,
		     struct oa_tc6_frame_header header,
		     struct oa_tc6_buf_desc *buf_desc)
{
	struct oa_tc6_frame_struct *entries;
	struct oa_tc6_critical_state basepri;
	int ret = 0;

	if (desc == NULL || buf_desc == NULL)
		return -EINVAL;

	entries = (struct oa_tc6_frame_struct *)desc->tx_queue->entries;

	basepri = oa_tc6_enter_critical_section(OA_TC6_INT_PRI_DMA_SPI_TX);

	if (oa_tc6_net_queue_is_full(desc->tx_queue)) {
		ret = -ENOSPC;
		goto out;
	}

	entries[desc->tx_queue->head].header.VALUE16 = header.VALUE16;
	entries[desc->tx_queue->head].buf_desc = buf_desc;
	oa_tc6_net_queue_add_entry(desc->tx_queue);

out:
	oa_tc6_exit_critical_section(basepri);
	return ret;
}

/* True when the OA-owned tx queue has no pending frames. */
bool oa_tc6_tx_queue_is_empty(struct oa_tc6_desc *desc)
{
	if (desc == NULL || desc->tx_queue == NULL)
		return true;
	return oa_tc6_net_queue_is_empty(desc->tx_queue);
}

uint32_t oa_tc6_tx_queue_avail(struct oa_tc6_desc *desc)
{
	if (desc == NULL || desc->tx_queue == NULL)
		return 0;
	return (desc->tx_queue->num_entries - 1) - oa_tc6_net_queue_count(
		       desc->tx_queue);
}

/* True when the OA-owned tx queue cannot accept another frame. */
bool oa_tc6_tx_queue_is_full(struct oa_tc6_desc *desc)
{
	if (desc == NULL || desc->tx_queue == NULL)
		return true;
	return oa_tc6_net_queue_is_full(desc->tx_queue);
}

/* Add an app RX buffer (zero-copy buf_desc) into the OA-owned rx queue. */
int oa_tc6_submit_rx_buffer(struct oa_tc6_desc *desc,
			    enum oa_tc6_rx_fifo_prio prio,
			    struct oa_tc6_buf_desc *buf_desc)
{
	struct oa_tc6_net_queue *q;
	struct oa_tc6_frame_struct *entries;
	struct oa_tc6_critical_state basepri;
	int ret = 0;

	if (desc == NULL || buf_desc == NULL)
		return -EINVAL;

	q = (prio == OA_TC6_RX_FIFO_PRIO_HIGH) ? &desc->rx_queue_hp_storage
	    : &desc->rx_queue_lp_storage;
	entries = (struct oa_tc6_frame_struct *)q->entries;

	basepri = oa_tc6_enter_critical_section(OA_TC6_INT_PRI_DMA_SPI_RX);

	if (oa_tc6_net_queue_is_full(q)) {
		ret = -ENOSPC;
		goto out;
	}

	entries[q->head].header.VALUE16 = 0x0000;
	entries[q->head].buf_desc = buf_desc;
	oa_tc6_net_queue_add_entry(q);

out:
	oa_tc6_exit_critical_section(basepri);
	return ret;
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
 * @brief       Process receive frame via the OA TC6 state machine.
 * @param desc - the OA TC6 descriptor.
 * @param blocking - if true, wait for the transaction to complete before
 *                   returning.
 * @return 0 in case of success, negative error code otherwise.
 */
int oa_tc6_process_rx_frame(struct oa_tc6_desc *const desc, bool blocking)
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
	if (desc->spi_state == OA_SPI_STATE_READY) {
		capi_irq_disable(desc->eth_irq);
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
void oa_tc6_spi_callback(enum capi_async_event event, void *arg,
			 int event_extra)
{
	(void)event_extra;
	HAL_OA_STATE_MACHINE_LOCK();

	struct oa_tc6_desc *desc = (struct oa_tc6_desc *)arg;

	if (event != CAPI_SPI_EVENT_XFR_DONE)
		desc->spi_err = 1;
	else
		desc->spi_err = 0;

	oa_tc6_state_machine(desc);
	HAL_OA_STATE_MACHINE_UNLOCK();
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
int oa_tc6_wait_get_status(struct oa_tc6_desc *const desc, uint8_t *back_up,
			   bool is_ctrl)
{

	if (desc == NULL)
		return -EINVAL;

	if (is_ctrl) {
		struct oa_tc6_critical_state basepri = oa_tc6_enter_critical_section(
				OA_TC6_INT_PRI_DMA_SPI_RX);

		HAL_OA_STATE_MACHINE_LOCK();
        
		if (desc->spi_state == OA_SPI_STATE_READY) {
                bool enabled = true;
                GET_ENABLE_IRQ(desc->eth_irq, &enabled);
                *back_up = enabled;
        } else {
                /* Function was called while a SPI transaction is ongoing
                (non-blocking, using DMA).In this situation we want to
                re-enable the IRQ, it will always be disabled at this point.*/
                *back_up = 1;
        }
        
		HAL_OA_STATE_MACHINE_UNLOCK();

		capi_irq_disable(desc->eth_irq);
		*(desc->pending_ctrl) = true;

		oa_tc6_exit_critical_section(basepri);

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

	uint32_t startTime = HAL_GetTick();

	// Wait until SPI state is ready
	HAL_OA_STATE_MACHINE_LOCK();
	while ((desc->spi_state != OA_SPI_STATE_READY)) {
		/* Check if timeout is greater than 50, communication timeout.
		   Otherwise, the SPI state is ready.                       */
		if ((HAL_GetTick() - startTime) >= 50) {
			return -ETIMEDOUT;
		}
		HAL_IPOS_UnlockSleepLock();
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

	if (param->p_dev_mem == NULL) {
		*desc = (struct oa_tc6_desc *)capi_calloc(1, sizeof(**desc));
		if (!*desc)
			return -ENOMEM;
	} else {
		if (param->dev_mem_size < sizeof(desc))
			return -ENOMEM;

		memset(param->p_dev_mem, 0, param->dev_mem_size);
		*desc = (struct oa_tc6_desc *)param->p_dev_mem;
	}

	descriptor = *desc;
    
    descriptor->dev_mem = param->p_dev_mem;
	descriptor->comm_desc = param->comm_desc;
	descriptor->prote_spi = param->prote_spi;
	descriptor->rx_queue_hp_en = param->rx_queue_hp_en ;
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

	/* OA owns its TX frame queue (decoupled from eth_mac). */
	oa_tc6_net_queue_init(&descriptor->tx_queue_storage,
			      descriptor->tx_queue_frames,
			      TX_QUEUE_NUM_ENTRIES_RAW);
	descriptor->tx_queue = &descriptor->tx_queue_storage;

	/* OA owns its RX frame queues (lp/hp) and the active-queue pointer. */
	oa_tc6_net_queue_init(&descriptor->rx_queue_lp_storage,
			      descriptor->rx_queue_lp_frames, RX_QUEUE_NUM_ENTRIES_RAW);
	oa_tc6_net_queue_init(&descriptor->rx_queue_hp_storage,
			      descriptor->rx_queue_hp_frames, RX_QUEUE_NUM_ENTRIES_RAW);
	descriptor->rx_queue_active = &descriptor->rx_queue_lp_storage;
	descriptor->rx_queue = &descriptor->rx_queue_active;
	descriptor->rx_queue_lp = &descriptor->rx_queue_lp_storage;
	descriptor->rx_queue_hp = &descriptor->rx_queue_hp_storage;

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
    capi_free(desc);
    
	return 0;
}

/***************************************************************************//**
 *   @file   adin1110.c
 *   @brief  Source file of the ADIN1110 driver.
 *   @author Ciprian Regus (ciprian.regus@analog.com)
********************************************************************************
 * Copyright 2023(c) Analog Devices, Inc.
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
#include <string.h>
#include "no_os_spi.h"
#include "capi_spi.h"
#include "capi_gpio.h"
#include "adin1110.h"
#include "no_os_alloc.h"
#include "no_os_crc8.h"
#include "no_os_delay.h"
#include "no_os_util.h"

#define ADIN1110_CRC_POLYNOMIAL	0x7

NO_OS_DECLARE_CRC8_TABLE(_crc_table);

struct _adin1110_priv {
	uint32_t phy_id;
	uint32_t num_ports;
};

static struct _adin1110_priv driver_data[2] = {
	[ADIN_1110] = {
		.phy_id = ADIN1110_PHY_ID,
		.num_ports = 1,
	},
	[ADIN_2111] = {
		.phy_id = ADIN2111_PHY_ID,
		.num_ports = 2,
	},
};

/**
 * @brief Write a register's value
 * @param desc - the device descriptor
 * @param addr - register's address
 * @param data - register's value
 * @return 0 in case of success, negative error code otherwise
 */
static int adin1110_standard_spi_reg_write(struct adin1110_desc *desc,
		uint16_t addr, uint32_t data)
{
	uint32_t header_len = ADIN1110_WR_HDR_SIZE;
	struct no_os_spi_msg xfer = {
		.tx_buff = desc->data,
		.rx_buff = desc->data,
		.bytes_number = ADIN1110_WR_FRAME_SIZE,
		.cs_change = 1,
	};

	addr &= ADIN1110_ADDR_MASK;
	addr |= ADIN1110_CD_MASK | ADIN1110_RW_MASK;
	no_os_put_unaligned_be16(addr, desc->data);

	if (desc->append_crc) {
		desc->data[2] = no_os_crc8(_crc_table, desc->data, 2, 0);
		header_len++;
		xfer.bytes_number++;
	}

	no_os_put_unaligned_be32(data, &desc->data[header_len]);
	if (desc->append_crc) {
		desc->data[header_len + ADIN1110_REG_LEN] =
			no_os_crc8(_crc_table, &desc->data[header_len], ADIN1110_REG_LEN, 0);
		xfer.bytes_number++;
	}

	return no_os_spi_transfer(desc->comm_desc, &xfer, 1);
}

/**
 * @brief Write a register's value
 * @param desc - the device descriptor
 * @param addr - register's address
 * @param data - register's value
 * @return 0 in case of success, negative error code otherwise
 */
int adin1110_reg_write(struct adin1110_desc *desc, uint16_t addr, uint32_t data)
{
	uint8_t mms = 0;

	if (desc->oa_tc6_spi) {
		if (addr >= 0x30)
			mms = 1;

		return oa_tc6_reg_write(desc->oa_desc, OA_MMS_REG(mms, addr), data);
	}

	return adin1110_standard_spi_reg_write(desc, addr, data);
}

/**
 * @brief Read a register's value
 * @param desc - the device descriptor
 * @param addr - register's address
 * @param data - register's value
 * @return 0 in case of success, negative error code otherwise
 */
static int adin1110_standard_spi_reg_read(struct adin1110_desc *desc,
		uint16_t addr, uint32_t *data)
{
	uint8_t crc;
	uint8_t recv_crc;
	uint32_t header_len = ADIN1110_RD_HEADER_LEN;
	struct no_os_spi_msg xfer = {
		.tx_buff = desc->data,
		.rx_buff = desc->data,
		.bytes_number = ADIN1110_REG_LEN,
		.cs_change = 1,
	};
	int ret;

	no_os_put_unaligned_be16(addr, &desc->data[0]);
	desc->data[0] |= ADIN1110_SPI_CD;
	desc->data[2] = 0x0;

	if (desc->append_crc) {
		xfer.bytes_number += ADIN1110_CRC_LEN;
		desc->data[2] = no_os_crc8(_crc_table, desc->data, 2, 0);
		desc->data[3] = 0x0;
		header_len++;
	}

	xfer.bytes_number += header_len;
	ret = no_os_spi_transfer(desc->comm_desc, &xfer, 1);
	if (ret)
		return ret;

	if (desc->append_crc) {
		crc = no_os_crc8(_crc_table, &desc->data[header_len], 4, 0);
		recv_crc = desc->data[header_len + ADIN1110_REG_LEN];

		if (crc != recv_crc)
			return -EINVAL;
	}

	*data = no_os_get_unaligned_be32(&desc->data[header_len]);

	return 0;
}

/**
 * @brief Read a register's value
 * @param desc - the device descriptor
 * @param addr - register's address
 * @param data - register's value
 * @return 0 in case of success, negative error code otherwise
 */
int adin1110_reg_read(struct adin1110_desc *desc, uint16_t addr, uint32_t *data)
{
	uint8_t mms = 0;

	if (desc->oa_tc6_spi) {
		if (addr >= 0x30)
			mms = 1;

		return oa_tc6_reg_read(desc->oa_desc, OA_MMS_REG(mms, addr), data);
	}

	return adin1110_standard_spi_reg_read(desc, addr, data);
}

/**
 * @brief Update a register's value based on a mask
 * @param desc - the device descriptor
 * @param addr - register's address
 * @param mask - the bits that may be modified
 * @param data - register's value
 * @return 0 in case of success, negative error code otherwise
 */
int adin1110_reg_update(struct adin1110_desc *desc, uint16_t addr,
			uint32_t mask, uint32_t data)
{
	uint32_t val;
	int ret;

	ret = adin1110_reg_read(desc, addr, &val);
	if (ret)
		return ret;

	val &= ~mask;
	val |= mask & data;

	return adin1110_reg_write(desc, addr, val);
}

/**
 * @brief Read a PHY register using clause 22
 * @param desc - the device descriptor
 * @param phy_id - the phy device's id
 * @param reg - register's address
 * @param data - register's value
 * @return 0 in case of success, negative error code otherwise
 */
int adin1110_mdio_read(struct adin1110_desc *desc, uint32_t phy_id,
		       uint32_t reg, uint16_t *data)
{
	uint32_t mdio_done = 0;
	uint32_t mdio_val;
	uint32_t val = 0;
	int ret;

	/* Use clause 22 for the MDIO register addressing */
	val = no_os_field_prep(ADIN1110_MDIO_ST, 0x1) |
	      no_os_field_prep(ADIN1110_MDIO_OP, ADIN1110_MDIO_OP_RD) |
	      no_os_field_prep(ADIN1110_MDIO_PRTAD, phy_id) |
	      no_os_field_prep(ADIN1110_MDIO_DEVAD, reg);

	ret = adin1110_reg_write(desc, ADIN1110_MDIOACC(0), val);
	if (ret)
		return ret;

	/* The PHY will set the MDIO_TRDONE bit to 1 once a transaction completes */
	while (!mdio_done) {
		ret = adin1110_reg_read(desc, ADIN1110_MDIOACC(0), &mdio_val);
		if (ret)
			return ret;

		mdio_done = no_os_field_get(ADIN1110_MDIO_TRDONE, mdio_val);
	}

	*data = no_os_field_get(ADIN1110_MDIO_DATA, mdio_val);

	return 0;
}

/**
 * @brief Write a PHY register using clause 22
 * @param desc - the device descriptor
 * @param phy_id - the phy device's id
 * @param reg - register's address
 * @param data - register's value
 * @return 0 in case of success, negative error code otherwise
 */
int adin1110_mdio_write(struct adin1110_desc *desc, uint32_t phy_id,
			uint32_t reg, uint16_t data)
{
	uint32_t mdio_done = 0;
	uint32_t mdio_val;
	uint32_t val;
	int ret;

	/* Use clause 22 for the MDIO register addressing */
	val = no_os_field_prep(ADIN1110_MDIO_ST, 0x1) |
	      no_os_field_prep(ADIN1110_MDIO_OP, ADIN1110_MDIO_OP_WR) |
	      no_os_field_prep(ADIN1110_MDIO_PRTAD, phy_id) |
	      no_os_field_prep(ADIN1110_MDIO_DEVAD, reg) |
	      no_os_field_prep(ADIN1110_MDIO_DATA, data);

	ret = adin1110_reg_write(desc, ADIN1110_MDIOACC(0), val);
	if (ret)
		return ret;

	/* The PHY will set the MDIO_TRDONE bit to 1 once a transaction completes */
	while (!mdio_done) {
		ret = adin1110_reg_read(desc, ADIN1110_MDIOACC(0), &mdio_val);
		if (ret)
			return ret;

		mdio_done = no_os_field_get(ADIN1110_MDIO_TRDONE, mdio_val);
	}

	return 0;
}

/**
 * @brief Write a PHY register using clause 45
 * @param desc - the device descriptor
 * @param phy_id - the phy device's MDIO id
 * @param dev_id - the device id of the register
 * @param reg - register's address
 * @param data - register's value
 * @return 0 in case of success, negative error code otherwise
 */
int adin1110_mdio_write_c45(struct adin1110_desc *desc, uint32_t phy_id,
			    uint32_t dev_id, uint32_t reg, uint16_t data)
{
	uint32_t mdio_done = 0;
	uint32_t mdio_val;
	uint32_t val;
	int ret;

	val = no_os_field_prep(ADIN1110_MDIO_ST, 0x0) |
	      no_os_field_prep(ADIN1110_MDIO_OP, ADIN1110_MDIO_OP_ADDR) |
	      no_os_field_prep(ADIN1110_MDIO_PRTAD, phy_id) |
	      no_os_field_prep(ADIN1110_MDIO_DEVAD, dev_id) |
	      no_os_field_prep(ADIN1110_MDIO_DATA, reg);

	ret = adin1110_reg_write(desc, ADIN1110_MDIOACC(0), val);
	if (ret)
		return ret;

	val = no_os_field_prep(ADIN1110_MDIO_ST, 0x0) |
	      no_os_field_prep(ADIN1110_MDIO_OP, ADIN1110_MDIO_OP_WR) |
	      no_os_field_prep(ADIN1110_MDIO_PRTAD, phy_id) |
	      no_os_field_prep(ADIN1110_MDIO_DEVAD, dev_id) |
	      no_os_field_prep(ADIN1110_MDIO_DATA, data);

	ret = adin1110_reg_write(desc, ADIN1110_MDIOACC(1), val);
	if (ret)
		return ret;

	/* The PHY will set the MDIO_TRDONE bit to 1 once a transaction completes */
	while (!mdio_done) {
		ret = adin1110_reg_read(desc, ADIN1110_MDIOACC(0), &mdio_val);
		if (ret)
			return ret;

		mdio_done = no_os_field_get(ADIN1110_MDIO_TRDONE, mdio_val);
	}

	mdio_done = 0;
	while (!mdio_done) {
		ret = adin1110_reg_read(desc, ADIN1110_MDIOACC(1), &mdio_val);
		if (ret)
			return ret;

		mdio_done = no_os_field_get(ADIN1110_MDIO_TRDONE, mdio_val);
	}

	return 0;
}

/**
 * @brief Read a PHY register using clause 45
 * @param desc - the device descriptor
 * @param phy_id - the phy device's MDIO id
 * @param dev_id - the device id of the register
 * @param reg - register's address
 * @param data - register's value
 * @return 0 in case of success, negative error code otherwise
 */
int adin1110_mdio_read_c45(struct adin1110_desc *desc, uint32_t phy_id,
			   uint32_t dev_id, uint16_t reg, uint16_t *data)
{
	uint32_t mdio_done = 0;
	uint32_t mdio_val;
	uint32_t val;
	int ret;

	val = no_os_field_prep(ADIN1110_MDIO_ST, 0x0) |
	      no_os_field_prep(ADIN1110_MDIO_OP, ADIN1110_MDIO_OP_ADDR) |
	      no_os_field_prep(ADIN1110_MDIO_PRTAD, phy_id) |
	      no_os_field_prep(ADIN1110_MDIO_DEVAD, dev_id) |
	      no_os_field_prep(ADIN1110_MDIO_DATA, reg);

	ret = adin1110_reg_write(desc, ADIN1110_MDIOACC(0), val);
	if (ret)
		return ret;

	while (!mdio_done) {
		ret = adin1110_reg_read(desc, ADIN1110_MDIOACC(0), &mdio_val);
		if (ret)
			return ret;

		mdio_done = no_os_field_get(ADIN1110_MDIO_TRDONE, mdio_val);
	}

	val = no_os_field_prep(ADIN1110_MDIO_ST, 0x0) |
	      no_os_field_prep(ADIN1110_MDIO_OP, ADIN1110_MDIO_OP_RD) |
	      no_os_field_prep(ADIN1110_MDIO_DEVAD, dev_id) |
	      no_os_field_prep(ADIN1110_MDIO_PRTAD, phy_id);

	ret = adin1110_reg_write(desc, ADIN1110_MDIOACC(1), val);
	if (ret)
		return ret;

	mdio_done = 0;
	while (!mdio_done) {
		ret = adin1110_reg_read(desc, ADIN1110_MDIOACC(1), &mdio_val);
		if (ret)
			return ret;

		mdio_done = no_os_field_get(ADIN1110_MDIO_TRDONE, mdio_val);
	}

	*data = no_os_field_get(ADIN1110_MDIO_DATA, mdio_val);

	return 0;
}

/**
 * @brief Set a MAC address destination filter, frames who's DA doesn't match
 * 	  are dropped.
 * @param desc - the device descriptor
 * @param mac_address - the MAC filter to be set
 * @return 0 in case of success, negative error code otherwise
 */
int adin1110_set_mac_addr(struct adin1110_desc *desc,
			  uint8_t mac_address[ADIN1110_ETH_ALEN])
{
	uint32_t addr_upr;
	uint32_t addr_lwr;
	uint32_t temp;
	int ret;
	int i;

	addr_upr = no_os_get_unaligned_be16(&mac_address[0]);

	/* Forward frames from both ports to the host */
	addr_upr |= ADIN1110_MAC_ADDR_APPLY2PORT | ADIN1110_MAC_ADDR_TO_HOST;
	if (desc->chip_type == ADIN_2111)
		addr_upr |= ADIN2111_MAC_ADDR_APPLY2PORT2;

	/* Look for a free slot. */
	for (i = 0; i < ADIN1110_ADDR_FILT_LEN; i++) {
		ret = adin1110_reg_read(desc, ADIN1110_MAC_ADDR_FILT_UPR_REG(i), &temp);
		if (ret)
			return ret;

		if (temp != 0)
			continue;

		ret = adin1110_reg_read(desc, ADIN1110_MAC_ADDR_FILT_LWR_REG(i), &temp);
		if (ret)
			return ret;

		if (temp == 0)
			break;
	}

	ret = adin1110_reg_write(desc, ADIN1110_MAC_ADDR_FILT_UPR_REG(i), addr_upr);
	if (ret)
		return ret;

	addr_lwr = no_os_get_unaligned_be32(&mac_address[2]);

	return adin1110_reg_write(desc, ADIN1110_MAC_ADDR_FILT_LWR_REG(i), addr_lwr);
}

/**
 * @brief Drop a MAC address filter.
 * @param desc - the device descriptor
 * @param mac_address - the MAC filter to be cleared
 * @return 0 in case of success, negative error code otherwise
 */
int adin1110_clear_mac_addr(struct adin1110_desc *desc,
			    uint8_t mac_address[ADIN1110_ETH_ALEN])
{
	uint32_t addr_upr;
	uint32_t addr_lwr;
	uint32_t temp;
	int ret;
	int i;

	addr_upr = no_os_get_unaligned_be16(&mac_address[0]);
	addr_lwr = no_os_get_unaligned_be32(&mac_address[2]);

	for (i = 0; i < ADIN1110_ADDR_FILT_LEN; i++) {
		ret = adin1110_reg_read(desc, ADIN1110_MAC_ADDR_FILT_UPR_REG(i), &temp);
		if (ret)
			return ret;

		temp &= ADIN1110_MAC_ADDR_UPR_MASK;
		if (addr_upr != temp)
			continue;

		ret = adin1110_reg_read(desc, ADIN1110_MAC_ADDR_FILT_LWR_REG(i), &temp);
		if (ret)
			return ret;

		if (addr_lwr == temp) {
			ret = adin1110_reg_write(desc, ADIN1110_MAC_ADDR_FILT_UPR_REG(i), 0);
			if (ret)
				return ret;

			ret = adin1110_reg_write(desc, ADIN1110_MAC_ADDR_FILT_LWR_REG(i), 0);
			if (ret)
				return ret;
		}
	}

	return 0;
}

/**
 * @brief Set/clear a broadcast filter. By enabling this, broadcast frames will
 * 	be forwarded to the host.
 * @param desc - the device descriptor
 * @param enabled - the set/clear option
 * @return 0 in case of success, negative error code otherwise
 */
int adin1110_broadcast_filter(struct adin1110_desc *desc, bool enabled)
{
	uint8_t broadcast_addr[ADIN1110_ETH_ALEN];

	memset(broadcast_addr, 0xFF, ADIN1110_ETH_ALEN);

	if (enabled)
		return adin1110_set_mac_addr(desc, broadcast_addr);

	return adin1110_clear_mac_addr(desc, broadcast_addr);
}

/**
 * @brief Write a frame to the TX FIFO.
 * @param desc - the device descriptor
 * @param port - the port for the frame to be transmitted on.
 * @param eth_buff - the frame to be transmitted.
 * @return 0 in case of success, negative error code otherwise
 */
int adin1110_write_fifo(struct adin1110_desc *desc, uint32_t port,
			struct adin1110_eth_buff *eth_buff)
{
	uint32_t header_len = ADIN1110_WR_HEADER_LEN;
	uint32_t frame_offset;
	uint32_t padding = 0;
	uint32_t padded_len;
	uint32_t round_len;
	uint32_t tx_space;
	int ret;

	struct no_os_spi_msg xfer = {0};

	if (port >= driver_data[desc->chip_type].num_ports)
		return -EINVAL;

	if (desc->oa_tc6_spi) {
		struct oa_tc6_frame_buffer *oa_frame_buffer;

		frame_offset = 0;
		ret = oa_tc6_get_tx_frame(desc->oa_desc, &oa_frame_buffer);
		if (ret)
			return ret;

		memcpy(&oa_frame_buffer->data[frame_offset], (void *)&eth_buff->mac_dest[0],
		       ADIN1110_ETH_HDR_LEN);
		frame_offset += ADIN1110_ETH_HDR_LEN;
		memcpy(&oa_frame_buffer->data[frame_offset], eth_buff->payload,
		       eth_buff->len - ADIN1110_ETH_HDR_LEN);

		if (eth_buff->len < 64)
			oa_frame_buffer->len = 64;
		else
			oa_frame_buffer->len = eth_buff->len;

		oa_frame_buffer->vs = port;

		oa_tc6_put_tx_frame(desc->oa_desc, oa_frame_buffer);

		return oa_tc6_thread(desc->oa_desc);
	}

	/* The minimum frame length is 64 bytes */
	if (eth_buff->len + ADIN1110_FCS_LEN < 64)
		padding = 64 - (eth_buff->len + ADIN1110_FCS_LEN);

	xfer.tx_buff = desc->data;
	xfer.rx_buff = desc->data;
	xfer.cs_change = 1;

	padded_len = eth_buff->len + padding + ADIN1110_FRAME_HEADER_LEN;

	/** Align the frame length to 4 bytes */
	round_len = no_os_align(padded_len, 4);

	ret = adin1110_reg_read(desc, ADIN1110_TX_SPACE_REG, &tx_space);
	if (ret)
		return ret;

	/*
	 * Check if there is enough space for the frame in the TX FIFO.
	 * The tx_space value is expressed in 16 bit words.
	 */
	if (padded_len > 2 * (tx_space - ADIN1110_FRAME_HEADER_LEN))
		return -EAGAIN;

	ret = adin1110_reg_write(desc, ADIN1110_TX_FSIZE_REG, padded_len);
	if (ret)
		return ret;

	no_os_put_unaligned_be16(ADIN1110_TX_REG, &desc->data[0]);
	desc->data[0] |= ADIN1110_SPI_CD | ADIN1110_SPI_RW;

	if (desc->append_crc) {
		desc->data[2] = no_os_crc8(_crc_table, desc->data, 2, 0);
		header_len++;
	}

	/* Set the port on which to send the frame */
	no_os_put_unaligned_be16(port, &desc->data[header_len]);
	xfer.bytes_number = round_len + header_len;
	frame_offset = header_len + ADIN1110_FRAME_HEADER_LEN;

	memcpy(&desc->data[frame_offset], (void *)&eth_buff->mac_dest[0],
	       ADIN1110_ETH_HDR_LEN);
	frame_offset += ADIN1110_ETH_HDR_LEN;
	memcpy(&desc->data[frame_offset], eth_buff->payload,
	       eth_buff->len - ADIN1110_ETH_HDR_LEN);

	return no_os_spi_transfer(desc->comm_desc, &xfer, 1);
}

/**
 * @brief Read a frame from the RX FIFO.
 * @param desc - the device descriptor
 * @param port - the port from which the frame shall be received.
 * @param eth_buff - the frame to be received.
 * @return 0 in case of success, negative error code otherwise
 */
int adin1110_read_fifo(struct adin1110_desc *desc, uint32_t port,
		       struct adin1110_eth_buff *eth_buff)
{
	uint32_t field_offset = ADIN1110_RD_HEADER_LEN;
	uint32_t payload_length;
	uint32_t fifo_fsize_reg;
	uint32_t rounded_len;
	uint32_t frame_size;
	uint32_t fifo_reg;
	struct no_os_spi_msg xfer = {0};
	int ret;

	if (port >= driver_data[desc->chip_type].num_ports)
		return -EINVAL;

	if (desc->oa_tc6_spi) {
		struct oa_tc6_frame_buffer *frame;

		oa_tc6_thread(desc->oa_desc);
		ret = oa_tc6_get_rx_frame_match_vs(desc->oa_desc, &frame, port, 0x1);
		if (ret)
			return ret;

		field_offset = 0;
		memcpy((void *)&eth_buff->mac_dest[0], &frame->data[field_offset],
		       ADIN1110_ETH_HDR_LEN);
		field_offset += ADIN1110_ETH_HDR_LEN;
		memcpy(eth_buff->payload, &frame->data[field_offset],
		       frame->len - ADIN1110_ETH_HDR_LEN);
		eth_buff->len = frame->len;

		oa_tc6_put_rx_frame(desc->oa_desc, frame);

		return 0;
	}

	xfer.tx_buff = desc->data;
	xfer.rx_buff = desc->data;
	xfer.cs_change = 1;

	if (!port) {
		fifo_reg = ADIN1110_RX_REG;
		fifo_fsize_reg = ADIN1110_RX_FSIZE_REG;
	} else {
		fifo_reg = ADIN2111_RX_P2_REG;
		fifo_fsize_reg = ADIN2111_RX_P2_FSIZE_REG;
	}

	ret = adin1110_reg_read(desc, fifo_fsize_reg, &frame_size);
	if (ret)
		return ret;

	if (frame_size < ADIN1110_FRAME_HEADER_LEN + ADIN1110_FEC_LEN)
		return 0;

	memset(desc->data, 0, ADIN1110_BUFF_LEN);
	no_os_put_unaligned_be16(fifo_reg, &desc->data[0]);
	desc->data[0] |= ADIN1110_SPI_CD;
	desc->data[2] = 0x0;

	if (desc->append_crc) {
		desc->data[2] = no_os_crc8(_crc_table, desc->data, 2, 0);
		desc->data[3] = 0x0;
		field_offset++;
	}

	/* Set the port from which to receive the frame */
	no_os_put_unaligned_be16(port, &desc->data[field_offset]);
	rounded_len = no_os_align(frame_size, 4);

	/* Can only read multiples of 4 bytes (the last bytes might be 0) */
	xfer.bytes_number = rounded_len + field_offset;
	field_offset += ADIN1110_FRAME_HEADER_LEN;

	/** Burst read the whole frame */
	ret = no_os_spi_transfer(desc->comm_desc, &xfer, 1);
	if (ret)
		return ret;

	payload_length = frame_size - ADIN1110_FRAME_HEADER_LEN - ADIN1110_ETH_HDR_LEN;
	memcpy((void *)&eth_buff->mac_dest[0], &desc->data[field_offset],
	       ADIN1110_ETH_HDR_LEN);
	field_offset += ADIN1110_ETH_HDR_LEN;
	memcpy(eth_buff->payload, &desc->data[field_offset], payload_length);
	eth_buff->len = frame_size - ADIN1110_FRAME_HEADER_LEN;

	return 0;
}

/**
 * @brief Reset the MAC device.
 * @param desc - the device descriptor
 * @return 0 in case of success, negative error code otherwise
 */
int adin1110_mac_reset(struct adin1110_desc *desc)
{
	uint32_t val;
	int ret;

	ret = adin1110_reg_write(desc, ADIN1110_SOFT_RST_REG, ADIN1110_SWRESET_KEY1);
	if (ret)
		return ret;

	ret = adin1110_reg_write(desc, ADIN1110_SOFT_RST_REG, ADIN1110_SWRESET_KEY2);
	if (ret)
		return ret;

	ret = adin1110_reg_write(desc, ADIN1110_SOFT_RST_REG, ADIN1110_SWRELEASE_KEY1);
	if (ret)
		return ret;

	ret = adin1110_reg_write(desc, ADIN1110_SOFT_RST_REG, ADIN1110_SWRELEASE_KEY2);
	if (ret)
		return ret;

	ret = adin1110_reg_read(desc, ADIN1110_MAC_RST_STATUS_REG, &val);
	if (ret)
		return ret;

	if (!val)
		return -EBUSY;

	return 0;
}

/**
 * @brief Complete the reset sequence.
 * @param desc - the device descriptor.
 * @return 0 in case of success, negative error code otherwise
 */
static int adin1110_check_reset(struct adin1110_desc *desc)
{
	uint32_t reg_val;
	int ret;

	ret = adin1110_reg_read(desc, ADIN1110_STATUS0_REG, &reg_val);
	if (ret)
		return ret;

	reg_val = no_os_field_get(ADIN1110_RESETC_MASK, reg_val);
	if (!reg_val)
		return -EBUSY;

	return adin1110_reg_update(desc, ADIN1110_CONFIG1_REG,
				   ADIN1110_CONFIG1_SYNC, ADIN1110_CONFIG1_SYNC);
}

/**
 * @brief Reset the PHY device.
 * @param desc - the device descriptor
 * @return 0 in case of success, negative error code otherwise
 */
int adin1110_phy_reset(struct adin1110_desc *desc)
{
	uint32_t phy_id;
	uint32_t expected_id;
	int ret;

	/* The timing values for the reset sequence are spcified in the datasheet */
	ret = no_os_gpio_set_value(desc->reset_gpio, NO_OS_GPIO_LOW);
	if (ret)
		return ret;

	no_os_mdelay(10);

	ret = no_os_gpio_set_value(desc->reset_gpio, NO_OS_GPIO_HIGH);
	if (ret)
		return ret;

	no_os_mdelay(90);

	ret = adin1110_reg_read(desc, ADIN1110_PHY_ID_REG, &phy_id);
	if (ret)
		return ret;

	expected_id = driver_data[desc->chip_type].phy_id;

	if (phy_id != expected_id)
		return -EINVAL;

	return 0;
}

/**
 * @brief Reset both the MAC and PHY.
 * @param desc - the device descriptor
 * @return 0 in case of success, negative error code otherwise
 */
int adin1110_sw_reset(struct adin1110_desc *desc)
{
	return adin1110_reg_write(desc, ADIN1110_RESET_REG, 0x1);
}

/**
 * @brief Reset both the MAC and PHY.
 * @param desc - the device descriptor
 * @param state - status (up/down) of the link
 * @return 0 in case of success, negative error code otherwise
 */
int adin1110_link_state(struct adin1110_desc *desc, uint32_t *state)
{
	int ret;

	ret = adin1110_reg_read(desc, ADIN1110_STATUS1_REG, state);
	if (ret)
		return ret;

	*state = no_os_field_get(ADIN1110_LINK_STATE_MASK, *state);

	return 0;
}

/**
 * @brief Set a port in promiscuous mode. All MAC filters are dropped.
 * @param desc - the device descriptor
 * @param port - the port for which the mode will be applied
 * @param promisc - either enable or disable the promiscuous mode.
 * @return 0 in case of success, negative error code otherwise
 */
int adin1110_set_promisc(struct adin1110_desc *desc, uint32_t port,
			 bool promisc)
{
	uint32_t fwd_mask;

	if (port >= driver_data[desc->chip_type].num_ports)
		return -EINVAL;

	if (!port)
		fwd_mask = ADIN1110_FWD_UNK2HOST_MASK;
	else
		fwd_mask = ADIN2111_P2_FWD_UNK2HOST_MASK;

	return adin1110_reg_update(desc, ADIN1110_CONFIG2_REG, fwd_mask,
				   promisc ? fwd_mask : 0);
}

/**
 * @brief Get the PHY out of software reset.
 * @param desc - the device descriptor
 * @return 0 in case of success, negative error code otherwise
 */
static int adin1110_setup_phy(struct adin1110_desc *desc)
{
	uint16_t reg_val;
	uint32_t ports;
	uint32_t sw_pd;
	size_t i;
	int ret;

	ports = driver_data[desc->chip_type].num_ports;

	for (i = 0; i < ports; i++) {
		ret = adin1110_mdio_read_c45(desc, ADIN1110_MDIO_PHY_ID(i),
					     0x1E, ADIN1110_CRSM_SFT_PD_CNTRL_REG, &reg_val);
		if (ret)
			return ret;

		/* Get the PHY out of software power down to start the autonegotiation process*/
		sw_pd = no_os_field_get(ADIN1110_CRSM_SFT_PD_MASK, reg_val);
		if (sw_pd) {
			while (reg_val & ADIN1110_CRSM_SFT_PD_MASK) {
				ret = adin1110_mdio_write_c45(desc, ADIN1110_MDIO_PHY_ID(i),
							      0x1E, ADIN1110_CRSM_SFT_PD_CNTRL_REG, 0x0);
				if (ret)
					return ret;
				ret = adin1110_mdio_read_c45(desc, ADIN1110_MDIO_PHY_ID(i),
							     0x1E, ADIN1110_CRSM_SFT_PD_CNTRL_REG, &reg_val);
				if (ret)
					return ret;
			}
		}
	}

	return 0;
}

/* MAC interface wrappers matching eth_mac_init_param function pointer signatures */
static int adin1110_oa_mac_read(void *dev, enum eth_mac_state state,
		uint16_t reg_addr, void *buf, uint32_t size, bool blocking)
{
	struct adin1110_desc *d = (struct adin1110_desc *)dev;
	return oa_tc6_reg_read_async(d->oa_desc, reg_addr, buf, size);
}

static int adin1110_oa_mac_write(void *dev, enum eth_mac_state state,
		uint16_t reg_addr, void *buf, uint32_t size, bool blocking)
{
	struct adin1110_desc *d = (struct adin1110_desc *)dev;
	return oa_tc6_reg_write_async(d->oa_desc, reg_addr, buf, size);
}

static int adin1110_oa_mac_get_status(void *dev, uint8_t *backup, bool ctrl)
{
	struct adin1110_desc *d = (struct adin1110_desc *)dev;
	return oa_tc6_wait_get_status(d->oa_desc, backup, ctrl);
}

static void adin1110_oa_mac_irq_handler(void *dev)
{
	struct adin1110_desc *d = (struct adin1110_desc *)dev;
	oa_tc6_irq_handler(d->oa_desc);
}

static int adin1110_oa_mac_wait_ready(void *dev)
{
	struct adin1110_desc *d = (struct adin1110_desc *)dev;
	return oa_tc6_wait_spi_ready(d->oa_desc);
}

static int adin1110_oa_mac_process_tx_frame(void *dev, bool blocking)
{
	struct adin1110_desc *d = (struct adin1110_desc *)dev;
	return oa_tc6_process_tx_frame(d->oa_desc, blocking);
}

static int adin1110_oa_mac_process_rx_frame(void *dev, bool blocking)
{
	struct adin1110_desc *d = (struct adin1110_desc *)dev;
	return oa_tc6_process_rx_frame(d->oa_desc, blocking);
}

static int adin1110_std_mac_read(void *dev, enum eth_mac_state state,
		uint16_t reg_addr, void *buf, uint32_t size, bool blocking)
{
	struct adin1110_desc *d = (struct adin1110_desc *)dev;

	if (state == ETH_MAC_STATE_DATA_START)
		return standard_spi_read(d->std_desc, STANDARD_SPI_STATE_DATA_START,
								 reg_addr, buf, size, blocking);
        
    return standard_spi_read(d->std_desc, STANDARD_SPI_STATE_CONTROL_START,
							 reg_addr, buf, size, blocking);
}

static int adin1110_std_mac_write(void *dev, enum eth_mac_state state,
		uint16_t reg_addr, void *buf, uint32_t size, bool blocking)
{
	struct adin1110_desc *d = (struct adin1110_desc *)dev;
        
        if (state == ETH_MAC_STATE_DATA_START)
		return standard_spi_write(d->std_desc,STANDARD_SPI_STATE_DATA_START,
								  reg_addr, buf, size, blocking);
	
        return standard_spi_write(d->std_desc, STANDARD_SPI_STATE_CONTROL_START,
								  reg_addr, buf, size, blocking);
}

static int adin1110_std_mac_write_frame(void *dev,
		struct eth_frame_struct *frame, bool blocking)
{
	struct adin1110_desc *d = (struct adin1110_desc *)dev;
	
        return standard_spi_write_frame(d->std_desc, frame, blocking);
}

static int adin1110_std_mac_get_status(void *dev, uint8_t *backup, bool ctrl)
{
	struct adin1110_desc *d = (struct adin1110_desc *)dev;
	return standard_spi_get_status(d->std_desc, backup, ctrl);
}

static int adin1110_std_mac_wait_ready(void *dev)
{
	struct adin1110_desc *d = (struct adin1110_desc *)dev;
	return is_spi_ready(d->std_desc);
}

static uint32_t phy_write(adin1110_desc *desc, uint8_t hwAddr, 
						  uint32_t regAddr, uint16_t data)
{
    return (uint32_t)eth_mac_phy_write(desc->mac_desc, hwAddr, regAddr, data);
}

static uint32_t phy_read(adin1110_desc *desc, uint8_t hwAddr, uint32_t regAddr,
						 uint16_t *data)
{
    return (uint32_t)eth_mac_phy_read (desc->mac_desc, hwAddr,regAddr, data);
}

int adin1110_init_new(struct adin1110_desc **desc,
		  struct adin1110_init_param *param)
{
        struct adin1110_desc *descriptor;
        int ret;

        if (desc == NULL)
                return ETH_INVALID_HANDLE;

        if ((param == NULL))
                return ETH_INVALID_PARAM;
        
        if (param->dev_mem == NULL)
        {
                *desc = (struct adin1110_desc *)no_os_calloc(1, sizeof(**desc));
                if (!*desc)
                        return -ENOMEM;
                descriptor = *desc;
        }
        else
        {
                if (param->oa_tc6_spi){
                        if (param->dev_mem_size < ADIN1110_DEVICE_SIZE_OA) 
                                return ETH_INVALID_PARAM;   
                } else {
                        if (param->dev_mem_size < ADIN1110_DEVICE_SIZE)
                                return ETH_INVALID_PARAM;
                }

                if (param->oa_tc6_spi)
                        descriptor =(struct adin1110_desc *)((uint8_t *)param->dev_mem
									+ OA_SPI_DEVICE_SIZE + ETH_MAC_DEVICE_SIZE + 
									SPE_PHY_DEVICE_SIZE);
                else
                        descriptor = (struct adin1110_desc *)((uint8_t *)param->dev_mem 
									 + ADI_SPI_DEVICE_SIZE + ETH_MAC_DEVICE_SIZE + 
									 SPE_PHY_DEVICE_SIZE);
                
                memset(descriptor, 0, sizeof(*descriptor));
        }
             
        memcpy(descriptor->mac_address, param->mac_address, ADIN1110_ETH_ALEN);
        
        descriptor->chip_type = param->chip_type;
        descriptor->oa_tc6_spi = param->oa_tc6_spi;
        descriptor->append_crc = param->append_crc;
        descriptor->comm_desc = param->comm_desc;
        descriptor->reset_gpio = param->reset_gpio;
                
        if (param->oa_tc6_spi){
                /* Initialize the SPI configuration structure. */
                param->oa_init_param->comm_desc = param->comm_desc;
                param->oa_init_param->p_dev_mem = (void *)param->dev_mem;
                param->oa_init_param->dev_mem_size = OA_SPI_DEVICE_SIZE;
                
                if (param->chip_type == ADIN_1110)
                        param->oa_init_param->num_ports = 1;
                else
                        param->oa_init_param->num_ports = 2;

                param->oa_init_param->fcs_check_en = param->fcs_check_en;
                param->oa_init_param->rx_queue_hp_en = param->rx_queue_hp_en;
                param->oa_init_param->prote_spi = param->oa_tc6_prote_en;
                param->oa_init_param->eth_irq = ETH_INT_N_IRQn;
        } else {
                /* Initialize the SPI configuration structure. */
                param->std_init_param->comm_param = param->comm_desc;
                param->std_init_param->dev_mem = (void *)param->dev_mem;
                param->std_init_param->dev_mem_size = ADI_SPI_DEVICE_SIZE;

                if (param->chip_type == ADIN_1110)
                        param->std_init_param->num_ports = 1;
                else
                        param->std_init_param->num_ports = 2;

                param->std_init_param->fcs_check_en = param->fcs_check_en;
                param->std_init_param->rx_queue_hp_en = param->rx_queue_hp_en;
                param->std_init_param->crc_en = param->append_crc;
                param->std_init_param->eth_irq_num = ETH_INT_N_IRQn;
        }

        descriptor->oa_tc6_spi = param->oa_tc6_spi;

        if (descriptor->oa_tc6_spi) {
                ret = oa_tc6_init(&descriptor->oa_desc, param->oa_init_param);
                if (ret)
                        goto free_oa;
        } else {
                ret = standard_spi_init(&descriptor->std_desc,
										param->std_init_param);
                if (ret)
                        goto free_spi;
                descriptor->std_desc->app_device = descriptor;
        }
                
        /* Initialize the MAC configuration structure. *
         * Configure macDrvConfig.dev_mem              */
        if (param->oa_tc6_spi) {
                param->mac_init_param->dev_mem = param->dev_mem ?
                        (void *)((uint8_t *)param->dev_mem + 
								OA_SPI_DEVICE_SIZE) : NULL;
        } else {
                param->mac_init_param->dev_mem = param->dev_mem ?
                        (void *)((uint8_t *)param->dev_mem +
								ADI_SPI_DEVICE_SIZE) : NULL;
        }

        param->mac_init_param->mem_size = ETH_MAC_DEVICE_SIZE;
        param->mac_init_param->fcs_check_en = param->fcs_check_en;

        if (param->chip_type == ADIN_1110)
                param->mac_init_param->device_type = ADIN_1110;
        else
                param->mac_init_param->device_type = ADIN_2111;

        param->mac_init_param->is_rx_queue_hp_enabled = param->rx_queue_hp_en; 
        param->mac_init_param->is_oa_spi_enabled = param->oa_tc6_spi; 
        param->mac_init_param->eth_irq = ETH_INT_N_IRQn;

        /* Initialize the PHY configuration structure.  *
         * Configure phyDrvConfig.dev_mem               */
        if (param->oa_tc6_spi) {
                param->phy_init_param->dev_mem = param->dev_mem ?
                        (void *)((uint8_t *)param->dev_mem + OA_SPI_DEVICE_SIZE
						+ ETH_MAC_DEVICE_SIZE) : NULL;
        } else {
                param->phy_init_param->dev_mem = param->dev_mem ?
                        (void *)((uint8_t *)param->dev_mem + ADI_SPI_DEVICE_SIZE
						+ ETH_MAC_DEVICE_SIZE) : NULL;
        }

        param->phy_init_param->mem_size = SPE_PHY_DEVICE_SIZE;
        param->phy_init_param->enableIrq  = false;

        /* Configure MAC interface function pointers via ADIN interface functions */
        param->mac_init_param->app_main = descriptor;
        if (param->oa_tc6_spi)
        {
                param->mac_init_param->mac_read = adin1110_oa_mac_read;
                param->mac_init_param->mac_write = adin1110_oa_mac_write;
                param->mac_init_param->mac_get_comm_status = 
						adin1110_oa_mac_get_status;
                param->mac_init_param->mac_request_irq_handler = 
						adin1110_oa_mac_irq_handler;
                param->mac_init_param->mac_wait_until_ready = 
						adin1110_oa_mac_wait_ready;
                param->mac_init_param->mac_process_tx_frame = 
						adin1110_oa_mac_process_tx_frame;
                param->mac_init_param->mac_process_rx_frame = 
						adin1110_oa_mac_process_rx_frame;
        }
        else
        {
                param->mac_init_param->mac_read = adin1110_std_mac_read;
                param->mac_init_param->mac_write = adin1110_std_mac_write;
                param->mac_init_param->mac_write_frame = 
						adin1110_std_mac_write_frame;
                param->mac_init_param->mac_get_comm_status = 
						adin1110_std_mac_get_status;
                param->mac_init_param->mac_wait_until_ready = 
						adin1110_std_mac_wait_ready;
        }

        ret = eth_mac_init(&descriptor->mac_desc, param->mac_init_param);
        if (ret)
                goto free_mac;

        if (param->oa_tc6_spi) {         
                descriptor->oa_desc->cb_func = descriptor->mac_desc->cb_func ;
                descriptor->oa_desc->cb_param_p = &(descriptor->mac_desc->cb_param[0]);
                descriptor->oa_desc->app_device = descriptor;
                descriptor->oa_desc->rx_queue = &(descriptor->mac_desc->rx_queue);
                descriptor->oa_desc->tx_queue = &(descriptor->mac_desc->tx_queue);
                descriptor->oa_desc->rx_queue_lp = &(descriptor->mac_desc->rx_queue_lp);
        } else {
                descriptor->std_desc->cb_func[0] = descriptor->mac_desc->cb_func[0];
                descriptor->std_desc->cb_param= &(descriptor->mac_desc->cb_param[0]);
                
                descriptor->std_desc->rx_queue = &(descriptor->mac_desc->rx_queue);
                descriptor->std_desc->tx_queue = &(descriptor->mac_desc->tx_queue);
                descriptor->std_desc->rx_queue_lp = &(descriptor->mac_desc->rx_queue_lp);        
        }
                
        if (param->rx_queue_hp_en) {
                if (param->oa_tc6_spi)
                        descriptor->oa_desc->rx_queue_hp = &(descriptor->mac_desc->rx_queue_hp);
                else
                        descriptor->std_desc->rx_queue_hp = &(descriptor->mac_desc->rx_queue_hp);
        } 

        if (param->oa_tc6_spi) {
                descriptor->oa_desc->pending_ctrl = &(descriptor->mac_desc->pending_ctrl);
        } else {
                descriptor->std_desc->pending_control = &(descriptor->mac_desc->pending_ctrl);
        } 

        if (param->oa_tc6_spi) {
                descriptor->oa_desc->irq_mask0 = &(descriptor->mac_desc->irq_mask0);
                descriptor->oa_desc->irq_mask1 = &(descriptor->mac_desc->irq_mask1);
                descriptor->oa_desc->status_regs =
                        &(descriptor->mac_desc->status_registers);
                descriptor->oa_desc->phy_irq_mask =
                        &(descriptor->mac_desc->phy_irq_mask);
        }

        if (param->oa_tc6_spi) {
                descriptor->oa_desc->ts_format = OA_TS_FORMAT_NONE;
        } else {
                descriptor->std_desc->ts_format = STANDARD_SPI_TS_FORMAT_NONE;
        }

        /* Register SPI completion callback */
        if (param->oa_tc6_spi) {
                ret = capi_spi_register_callback(
						descriptor->oa_desc->comm_desc->controller, 
						oa_tc6_spi_callback, descriptor->oa_desc);
        } else {
                ret = capi_spi_register_callback(
						descriptor->std_desc->comm_desc->controller, 
						standard_spi_callback, descriptor->std_desc);
        }

        if (ret)
                goto free_spi;

        /* State needs to advance from uninitialized */
        descriptor->mac_desc->state = ETH_MAC_STATE_INITIALIZED;
        
        /* Perform a software reset of the MAC */
        ret = eth_mac_reset(descriptor->mac_desc, ETH_MAC_RESET_TYPE_MAC_PHY);
        if (ret != ETH_SUCCESS)
                return ret;
        
        descriptor->mac_desc->state = ETH_MAC_STATE_READY;
        
        /* PHY address, used in internal MDIO accesses, is fixed in hardware. */
        param->phy_init_param->phyAddr = ADIN1110_PHY_ADDR;
        param->phy_init_param->adinDevice = descriptor;
        param->phy_init_param->readFn = PhyRead;
        param->phy_init_param->writeFn = PhyWrite;

        ret = spe_phy_Init(&descriptor->phy_desc, param->phy_init_param);
        if (ret)
                goto free_phy;

        descriptor->mac_desc->phy_addr = param->phy_init_param->phyAddr;
        descriptor->mac_desc->phy_irq_mask = ADIN1110_PHY_IRQ_MASK;

        *desc = descriptor;

        return 0;

free_phy:
		spe_phy_remove(descriptor->phy_desc);
		
free_mac:
		eth_mac_remove(descriptor->mac_desc);
		
free_oa:
		oa_tc6_remove(descriptor->oa_desc);
		
free_spi:
		standard_spi_remove(descriptor->std_desc);

        return ret;
}

/**
 * @brief Enable CRC, interrupts and a MAC filter for the MAC device.
 * @param desc - the device descriptor
 * @return 0 in case of success, negative error code otherwise
 */
static int adin1110_setup_mac(struct adin1110_desc *desc)
{
	int ret;
	uint32_t reg_val;
	
	ret = adin1110_reg_update(desc, ADIN1110_CONFIG2_REG, ADIN1110_CRC_APPEND,
				  ADIN1110_CRC_APPEND);
	if (ret)
		return ret;

	reg_val = ADIN1110_TX_RDY_IRQ | ADIN1110_RX_RDY_IRQ | ADIN1110_SPI_ERR_IRQ;
	if (desc->chip_type == ADIN_2111)
		reg_val |= ADIN2111_RX_RDY_IRQ;

	ret = adin1110_reg_write(desc, ADIN1110_IMASK1_REG, ~reg_val);
	if (ret)
		return ret;

	return adin1110_set_mac_addr(desc, desc->mac_address);
}

/**
 * @brief Initialize the device
 * @param desc - the device descriptor to be initialized
 * @param param - the device's parameter
 * @return 0 in case of success, negative error code otherwise
 */
int adin1110_init(struct adin1110_desc **desc,
		  struct adin1110_init_param *param)
{
	struct oa_tc6_init_param oa_param;
	struct standard_spi_init_param *std_param;
	struct eth_mac_init_param *mac_param;
	struct spe_phy_init_param *phy_param;
	struct adin1110_desc *descriptor;
	int ret;

	descriptor = no_os_calloc(1, sizeof(*descriptor));
	if (!descriptor)
		return -ENOMEM;

	ret = no_os_gpio_get_optional(&descriptor->reset_gpio, &param->reset_param);
	if (ret)
		goto free_desc;

	if (descriptor->reset_gpio) {
		ret = no_os_gpio_direction_output(descriptor->reset_gpio, NO_OS_GPIO_LOW);
		if (ret)
			goto free_rst_gpio;

		/*
		 * Minimum required time for the reset GPIO to be in the low state
		 * in order to reset the ADIN1110.
		 */
		no_os_mdelay(10);

		ret = no_os_gpio_set_value(descriptor->reset_gpio, NO_OS_GPIO_HIGH);
		if (ret)
			goto free_rst_gpio;

		/* Wait for the MAC and PHY digital interface to intialize after reset */
		no_os_mdelay(90);
	}

	ret = no_os_spi_init(&descriptor->comm_desc, &param->comm_param);
	if (ret)
		goto free_rst_gpio;

	// no_os_crc8_populate_msb(_crc_table, ADIN1110_CRC_POLYNOMIAL);
	strncpy((char *)descriptor->mac_address, (char *)param->mac_address,
		ADIN1110_MAC_LEN);

	memcpy(descriptor->mac_address, param->mac_address, ADIN1110_ETH_ALEN);
	descriptor->chip_type = param->chip_type;
	descriptor->append_crc = param->append_crc;

	if (!descriptor->reset_gpio) {
		ret = adin1110_sw_reset(descriptor);
		if (ret)
			goto free_spi;

		/* Wait for the MAC and PHY digital interface to intialize */
		no_os_mdelay(90);
	}

	descriptor->oa_tc6_spi = param->oa_tc6_spi;

	if (descriptor->oa_tc6_spi) {
		// oa_param.comm_desc = descriptor->comm_desc;
		ret = oa_tc6_init(&descriptor->oa_desc, descriptor->oa_init_param);
		if (ret)
			goto free_spi;
	} else {
		// descriptor->data = no_os_calloc(ADIN1110_BUFF_LEN,
						// sizeof(*descriptor->data));
		// if (!descriptor->data) {
			// ret = -ENOMEM;
			// goto free_spi;
		// }
		ret = standard_spi_init(&descriptor->std_desc, descriptor->std_init_param);
		if (ret)
			goto free_spi;
	}

	ret = eth_mac_init(&descriptor->mac_desc, descriptor->mac_init_param);
	if (ret)
		goto free_mac;

	ret = adin1110_setup_mac(descriptor);
	if (ret)
		goto free_oa;

	ret = spe_phy_init(&descriptor->phy_desc, descriptor->phy_init_param);
	if (ret)
		goto free_phy;

	ret = adin1110_setup_phy(descriptor);
	if (ret)
		goto free_oa;

	ret = adin1110_check_reset(descriptor);
	if (ret)
		goto free_oa;

	*desc = descriptor;

	if (desc->oa_tc6_spi)
	{
		desc->mac_desc->mac_read = adin1110_mac_read_async;
		desc->mac_desc->mac_write = adin1110_mac_write_async;
		desc->mac_desc->mac_get_comm_status = adin1110_mac_wait_get_status;
		desc->mac_desc->mac_request_irq_handler = adin1110_irq_handler;
		desc->mac_desc->mac_wait_until_ready = adin1110_wait_spi_ready;
	}
	else
	{
		desc->mac_desc->mac_read = adin1110_mac_reg_read;
		desc->mac_desc->mac_write = adin1110_mac_reg_write;
		desc->mac_desc->mac_write_frame = adin1110_mac_fifo_write;
		desc->mac_desc->mac_get_comm_status = adin1110_mac_get_status;
		desc->mac_desc->mac_wait_until_ready = adin1110_is_spi_ready;
	}

	return 0;

free_phy:
	spe_phy_remove(descriptor->phy_desc);
free_mac:
	eth_mac_remove(descriptor->mac_desc);
free_oa:
	// no_os_free(descriptor->data);
	oa_tc6_remove(descriptor->oa_desc);
free_spi:
	no_os_spi_remove(descriptor->comm_desc);
free_rst_gpio:
	no_os_gpio_remove(descriptor->reset_gpio);
free_desc:
	no_os_free(descriptor);

	return ret;
}

/**
 * @brief Free a device descriptor
 * @param desc - the device descriptor to be removed.
 * @return 0 in case of success, negative error code otherwise
 */
int adin1110_remove(struct adin1110_desc *desc)
{
	int ret;

	if (!desc)
		return -EINVAL;

	ret = no_os_spi_remove(desc->comm_desc);
	if (ret)
		return ret;

	if (desc->reset_gpio) {
		ret = no_os_gpio_remove(desc->reset_gpio);
		if (ret)
			return ret;
	}
	
	no_os_free(desc->data);
	spe_phy_remove(desc->phy_desc);
	eth_mac_remove(desc->mac_desc);
	oa_tc6_remove(desc->oa_desc);
	standard_spi_remove(desc->std_desc);
	no_os_free(desc);

	return 0;
}

int adin1110_phy_read(struct adin1110_desc *desc, uint32_t reg_addr,
						uint16_t *reg_data)
{
    if ((desc == NULL) || if (reg_data == NULL))
    {
        return -EINVAL;
    }

	return eth_mac_phy_read(desc->mac_desc, desc->phy_desc->phyAddr, reg_addr,
							reg_data);
}

int adin1110_get_device_id(struct adin1110_desc *desc,
							struct adin1110_device_id *dev_id)
{
    uint16_t            val16 = 0;

    if ((desc == NULL) || (dev_id == NULL))
        return -EINVAL;

    if (desc->mac_desc == NULL)
        return -EINVAL;

    result = adin1110_phy_read(desc->phy_desc, ADDR_MMD1_DEV_ID1, &val16);
    if (result)
        return result;

    dev_id->phy_id = (val16 << 16);

    result = adin1110_phy_read(desc->phy_desc, ADDR_MMD1_DEV_ID2, &val16);
    if (result)
        return result;

    dev_id->phy_id |= val16;

    result = adin1110_phy_read(desc->phy_desc, ADDR_MGMT_PRT_PKG, &val16);
    if (result)
        return result;

    dev_id->pkg_type = val16;

    result = adin1110_phy_read(desc->phy_desc, 0x1E900E, &val16);
    if (result)
        return result;

    dev_id->dig_rev_num = val16 & 0xFF;

    return 0;
}

int adin1110_reset(struct adin1110_desc *desc,
					struct eth_mac_reset_type reset_type)
{
    if ((desc == NULL) || (desc->mac_desc == NULL) || (desc->phy_desc == NULL))
        return -EINVAL;

    if ((resetType != ETH_MAC_RESET_TYPE_MAC_ONLY) &&
        (resetType != ETH_MAC_RESET_TYPE_MAC_PHY))
        return -EINVAL;

	result = eth_mac_reset(desc->mac_desc, reset_type);
	if ((result == 0) && (resetType == ETH_MAC_RESET_TYPE_MAC_PHY))
	{
		/* Re-initialize the PHY if needed */
		return spe_phy_reinit_phy(desc->phy_desc);
	}

    return 0;
}

int adin1110_set_promiscuous_mode(struct adin1110_desc *desc, bool mode)
{
    if (desc == NULL)
        return -EINVAL;

    if (desc->device != ADIN_1110)
        return -ENODEV;

    return eth_mac_set_promiscuous_mode(desc->mac_desc, mode);
}

int adin1110_get_promiscuous_mode(struct adin1110_desc *desc, bool *mode)
{
    if (desc == NULL)
        return -EINVAL;

    if (desc->device != ADIN_1110)
        return -ENODEV;

    if (mode == NULL)
        return ADI_ETH_INVALID_PARAM;

    return eth_mac_get_promiscuous_mode(desc->mac_desc, mode);
}

int adin1110_sync_config(struct adin1110_desc *desc)
{
    if ((desc == NULL) || (desc->mac_desc == NULL))
        return -EINVAL;

    return eth_mac_sync_config(desc->mac_desc);
}

int adin1110_get_link_status(struct adin1110_desc *desc,
								enum eth_mac_link_status *link_status)
{
    int result = 0;

	if ((desc == NULL) || (link_status == NULL) || (desc->mac_desc == NULL))
        return -EINVAL;
    
	/* For OA SPI, wait for any in-progress SPI transaction  */
	/* to complete before attempting register access.        */
	if (desc->mac_desc->is_oa_spi_enabled)
	{
		result = adin1110_wait_spi_ready(desc);
		if (result)
			return result;
	}
	
	return eth_mac_get_link_status(desc->mac_desc, 0, link_status);
}

int adin1110_get_stat_counters(struct adin1110_desc *desc,
                                          struct eth_mac_stat_counter *stat)
{
    int result = 0;
    uint8_t retries = 0;
    
    if (desc == NULL)
        return -EINVAL;

    /* For OA SPI, wait for any in-progress SPI                     */
    /* transaction to complete before attempting  register access.  */
    if (!desc->mac_desc->is_oa_spi_enabled)
	{
        result = adin1110_wait_spi_ready(desc);
        if (result)
            return result;
    }
    
    do {
        result = eth_mac_get_stat_counters(desc->mac_desc, 0, stat);
        if (result == 0)
            break;
		else
            retries++;
    } while(retries < 3 && result != 0);

  return result;
}

int adin1110_led_enable(struct adin1110_desc *desc, bool enable)
{
    if ((desc == NULL) || (desc->phy_desc == NULL ))
		return -EINVAL;

	return spe_phy_LedEn(desc->phy_desc, ADI_PHY_LED_0, enable);
}

int adin1110_set_loopback_mode(struct adin1110_desc *desc,
								enum spe_phy_LoopbackMode_e mode)
{
    if (desc == NULL)
        return -EINVAL;

	return spe_phy_SetLoopbackMode(desc->phy_desc, mode);


int adin1110_set_test_mode(struct adin1110_desc *desc,
							enum spe_phy_TestMode_e mode)
{
    if ((desc == NULL) || ( desc->phy_desc == NULL ))
		return -EINVAL;

	return spe_phy_SetTestMode(desc->phy_desc, mode);
}

int adin1110_add_address_filter(struct adin1110_desc *desc, uint8_t *mac_addr,
								uint8_t *mac_addr_mask, uint32_t priority,
								struct eth_mac_addr_rule rule)
{
    if ((desc == NULL) || (macAddr == NULL))
        return -EINVAL;
    
    /* Address rules only make sense if TO_HOST=1,       */
    /* because dropping frames is the default behaviour. */
	rule.VALUE16 = 0;
	rule.TO_HOST = 1;
	rule.HOST_PRI = priority & 
     (BITM_MAC_ADDR_FILT_UPR_N__HOST_PRI >> BITP_MAC_ADDR_FILT_UPR_N__HOST_PRI);
	rule.APPLY2PORT1 = 1;

    return eth_mac_add_address_filter(desc->mac_desc, mac_addr, mac_addr_mask,
										rule.VALUE16);
}

int adin1110_clear_address_filter(struct adin1110_desc *desc,
									uint32_t addr_index)
{
    if (desc == NULL)
        return -EINVAL;

    return eth_mac_clr_address_filter(desc->mac_desc, addr_index);
}

int adin1110_submit_tx_buffer(struct adin1110_desc *desc,
								struct eth_mac_buf_desc *buf_desc)
{
    struct eth_mac_frame_header		header;

    if ((desc == NULL) || if (buf_desc == NULL))
        return -EINVAL;

    if ((desc->mac_desc == NULL) || (desc->phy_desc == NULL))
        return -EINVAL;

	header.VALUE16 = 0x0000;
	header.EGRESS_CAPTURE = buf_desc->egress_capt;   

	/* For the ADIN1110, reference counter is always 1 */
	buf_desc->ref_count = 1;

	return eth_mac_submit_tx_buffer(desc->mac_desc, header, buf_desc);
}

int adin1110_submit_rx_buffer(struct adin1110_desc *desc,
								struct eth_mac_buf_desc *buf_desc)
{
    if ((desc == NULL) || (buf_desc == NULL))
        return -EINVAL;

    return eth_mac_submit_rx_buffer(desc->mac_desc, buf_desc);
}

int adin1110_set_chunk_size(struct adin1110_desc *desc, enum eth_mac_oa_cps cps)
{
    if ((desc == NULL) || (cps > ADI_MAC_OA_CPS_64BYTE) ||
		(desc->mac_desc == NULL))
        return -EINVAL;

    return eth_mac_set_chunk_size(desc->mac_desc, cps);
}

int adin1110_get_chunk_size(struct adin1110_desc *desc, 
							enum eth_mac_oa_cps *cps)
{
    if ((desc == NULL) || (cps == NULL))
        return -EINVAL;

    return eth_mac_get_chunk_size(desc->mac_desc, cps);
}

int adin1110_set_cut_through_mode(struct adin1110_desc *desc, bool tx_cte,
									bool rx_cte)
{
    if (desc == NULL)
        return -EINVAL;

	return eth_mac_set_cut_through_mode(desc->mac_desc, tx_cte, rx_cte);
}

int adin1110_get_cut_through_mode(struct adin1110_desc *desc, bool *tx_cte,
									bool *rx_cte)
{
    if ((desc == NULL) || (tx_cte == NULL) || (rx_cte == NULL))
        return -EINVAL;

	return eth_mac_get_cut_through_mode(desc->mac_desc, tx_cte, rx_cte);
}

int adin1110_set_fifo_sizes(struct adin1110_desc *desc,
							struct eth_mac_fifo_size fifo_sizes)
{
    uint32_t write_val;
    uint32_t total_size;

    if (desc == NULL)
        return -EINVAL;

	total_size	=	(2 * (uint32_t)fifo_sizes.txSize)	+
					(2 * (uint32_t)fifo_sizes.rxLoSize)	+ 
					(2 * (uint32_t)fifo_sizes.rxHiSize);

	if (total_size > ETH_MAC_FIFO_MAX_SIZE)
		return -ERANGE;

	write_val =	(fifo_sizes.txSize << BITP_MAC_FIFO_SIZE_HTX_SIZE)			|
				(fifo_sizes.rxLoSize << BITP_MAC_FIFO_SIZE_P1_RX_LO_SIZE)	|
				(fifo_sizes.rxHiSize << BITP_MAC_FIFO_SIZE_P1_RX_HI_SIZE);

	return eth_mac_set_fifo_sizes(desc->mac_desc, write_val);
}

int adin1110_get_fifo_sizes(struct adin1110_desc *desc,
							struct eth_mac_fifo_size *fifo_sizes)
{
    int			result = 0;
    uint32_t	read_val;

    if ((desc == NULL) || (fifo_sizes == NULL))
        return -EINVAL;

	result = eth_mac_get_fifo_sizes(desc->mac_desc, &read_val);

	fifo_sizes->txSize    = (eth_mac_HtxFifoSize_e)((read_val &
							 BITM_MAC_FIFO_SIZE_HTX_SIZE) >>
							 BITP_MAC_FIFO_SIZE_HTX_SIZE);
	fifo_sizes->rxLoSize  = (eth_mac_RxFifoSize_e)((read_val &
							 BITM_MAC_FIFO_SIZE_P1_RX_LO_SIZE) >>
							 BITP_MAC_FIFO_SIZE_P1_RX_LO_SIZE);
	fifo_sizes->rxHiSize  = (eth_mac_RxFifoSize_e)((read_val &
							 BITM_MAC_FIFO_SIZE_P1_RX_HI_SIZE) >>
							 BITP_MAC_FIFO_SIZE_P1_RX_HI_SIZE);
    return result;
}

int adin1110_clear_fifos(struct adin1110_desc *desc,
					     enum eth_mac_fifo_clr_mode clear_mode)
{
    bool mode_found = false;

    if (desc == NULL)
        return -EINVAL;

    for (enum eth_mac_fifo_clr_mode mode = ETH_MAC_CLEAR_ALL_FIFOS;
         mode <= ETH_MAC_CLEAR_P2P_FIFOS; mode++)
    {
        if (mode == clear_mode)
        {
            mode_found = true;
            break;
        }
    }

    if (!mode_found)
        return -EINVAL;

  
    if (ETH_MAC_CLEAR_P2P_FIFOS % clear_mode)
        return -EINVAL;

    return eth_mac_clear_fifo(desc->mac_desc, clear_mode);
}

int adin1110_ts_enable(struct adin1110_desc *desc, 
						enum eth_mac_ts_format format)
{
    if (desc == NULL)
        return -EINVAL;

    /* Update SPI device timestampFormat for OA SPI timestamp processing */
    if (desc->oa_tc6_spi)
    {
        desc->oa_desc->ts_format = (enum oa_tc6_ts_format)format;
    } else 
    {
        desc->std_desc->ts_format = (enum standard_spi_timestamp_format)format;
    }

    return eth_mac_ts_enable(desc->mac_desc, format);
}

int adin1110_ts_clear(struct adin1110_desc *desc)
{
    if (desc == NULL)
        return -EINVAL;

    return eth_mac_ts_clear(desc->mac_desc);
}

int adin1110_ts_timer_start(struct adin1110_desc *desc,
							struct eth_mac_ts_timer_cfg *timer_config)
{
    if ((desc == NULL) || (timer_config == NULL))
        return -EINVAL;

    return eth_mac_ts_timer_start(desc->mac_desc, timer_config);
}

int adin1110_ts_timer_stop(struct adin1110_desc *desc)
{
    if (desc == NULL)
        return -EINVAL;

    return eth_mac_ts_timer_stop(desc->mac_desc);
}

int adin1110_ts_get_ext_capt_timestamp(struct adin1110_desc *desc,
								  struct eth_mac_ts_timespec *captured_timespec)
{
    if ((desc == NULL) || (captured_timespec == NULL))
        return -EINVAL;

    return eth_mac_ts_get_ext_capt_timestamp(desc->mac_desc, captured_timespec);
}

int adin1110_ts_set_timer_absolute(struct adin1110_desc *desc, uint32_t seconds,
								uint32_t nanoseconds)
{
    if (desc == NULL)
        return -EINVAL;

    return eth_mac_ts_set_timer_absolute(desc->mac_desc, seconds, nanoseconds);
}

int adin1110_ts_sync_clock(struct adin1110_desc *desc, int64_t time_error,
				   uint64_t reference_time_ns_diff, uint64_t local_time_ns_diff)
{
    if (desc == NULL)
        return -EINVAL;

    return eth_mac_ts_sync_clock(desc->mac_desc, time_error,
								 reference_time_ns_diff, local_time_ns_diff);
}

int adin1110_ts_get_egress_timestamp(struct adin1110_desc *desc,
								  enum eth_mac_egress_capture egress_reg,
								  struct eth_mac_ts_timespec *captured_timespec)
{
    if ((desc == NULL) || (captured_timespec == NULL))
        return -EINVAL;
    
    return eth_mac_ts_get_egress_timestamp(desc->mac_desc, egress_reg,
										   captured_timespec);
}

int adin1110_ts_convert(struct adin1110_desc *desc, uint32_t timestamp_low_word,
						uint32_t timestamp_high_word,
						enum eth_mac_ts_format format,
						struct eth_mac_ts_timespec *timespec)
{
    if ((desc == NULL) || (timespec == NULL))
        return -EINVAL;

    return eth_mac_ts_convert(timestamp_low_word, timestamp_high_word, format,
							  timespec);
}

int64_t adin1110_ts_subtract(struct adin1110_desc *desc,
							 struct eth_mac_ts_timespec *ts_a,
							 struct eth_mac_ts_timespec *ts_b)
{
    if ((desc == NULL) || (ts_a == NULL) || (ts_b == NULL))
        return -EINVAL;

    return eth_mac_ts_subtract(ts_a, ts_b);
}

int adin1110_register_callback(struct adin1110_desc *desc,
							   struct eth_mac_callback cb_func,
							   enum eth_mac_event cb_event)
{
    if ((desc == NULL) || (cb_func == NULL) || (cb_event >= ETH_MAC_EVT_MAX))
        return -EINVAL;

    return eth_mac_register_callback(desc->mac_desc, cb_func, cb_event,
									 (void *)desc);
}

int adin1110_write_register(struct adin1110_desc *desc, uint16_t reg_addr,
						   uint32_t reg_data)
{
    if (desc == NULL)
        return -EINVAL;

    return eth_mac_write_register(desc->mac_desc, reg_addr, reg_data);
}

int adin1110_ReadRegister(struct adin1110_desc *desc, uint16_t reg_addr,
						  uint32_t *reg_data)
{
	int result = 0;

	if ((desc == NULL) || (reg_data == NULL))
        return -EINVAL;

    /* For OA SPI, wait for any in-progress SPI transaction to complete */
    /* before attempting register access. This prevents timeout errors  */
    /* when the IRQ handler is actively processing frame data.          */
    if (desc->mac_desc->is_oa_spi_enabled)
    {
        result = adin1110_wait_spi_ready(desc);
		if (result)
            return result;
    }

    return eth_mac_read_register(desc->mac_desc, reg_addr, reg_data);
}

int adin1110_phy_write(struct adin1110_desc *desc, uint32_t reg_addr,
					   uint16_t reg_data)
{
    if (desc == NULL)
        return -EINVAL;

	return eth_mac_phy_write(desc->mac_desc, desc->phy_desc->phyAddr, reg_addr,
						     reg_data);
}

int adin1110_phy_read(struct adin1110_desc *desc, uint32_t reg_addr,
					  uint16_t *reg_data)
{
    if ((desc == NULL) || (reg_data == NULL))
        return -EINVAL;

	return eth_mac_phy_read(desc->mac_desc, desc->phy_desc->phyAddr, reg_addr,
							reg_data);
}

int adin1110_enable(struct adin1110_desc *desc)
{
    if (desc == NULL)
        return -EINVAL;

    if (desc->device != ADIN_1110)
        return -ENODEV;

    return spe_phy_ExitSoftwarePowerdown(desc->phy_desc);
}

int adin1110_disable(struct adin1110_desc *desc)
{
    if (desc == NULL)
        return -EINVAL;

    if (desc->device != ADIN_1110)
        return -ENODEV;

    return spe_phy_EnterSoftwarePowerdown(desc->phy_desc);
}

int adin1110_get_mse_link_quality(struct adin1110_desc *desc,
							  struct spe_phy_MseLinkQuality_t *mse_link_quality)
{
    if ((desc == NULL) || (mse_link_quality == NULL))
        return -EINVAL;

	return spe_phy_GetMseLinkQuality(desc->phy_desc, mse_link_quality);
}

int adin1110_frame_gen_en(struct adin1110_desc *desc, bool enable)
{
    if (desc == NULL)
        return -EINVAL;

	return spe_phy_FrameGenEn(desc->phy_desc, enable);
}

int adin1110_frame_gen_set_mode(struct adin1110_desc *desc,
								enum spe_phy_FrameGenMode_e mode)
{
    if ((desc == NULL) || (mode > SPE_PHY_FRAME_GEN_MODE_CONT))
        return -EINVAL;

	return spe_phy_FrameGenSetMode(desc->phy_desc, mode);
}

int adin1110_frame_gen_set_frame_cnt(struct adin1110_desc *desc,
									 uint32_t frame_count)
{
    if (desc == NULL)
        return -EINVAL;

	return spe_phy_FrameGenSetFrameCnt(desc->phy_desc, frame_count);
}

int adin1110_frame_gen_set_frame_payload(struct adin1110_desc *desc,
										 enum spe_phy_FrameGenPayload_e payload)
{
    bool payload_found = false;

    if (desc == NULL)
        return -EINVAL;

    for (enum spe_phy_FrameGenPayload_e payload_e = SPE_PHY_FRAME_GEN_PAYLOAD_NONE;
         payload_e <= SPE_PHY_FRAME_GEN_PAYLOAD_DECR; payload_e++)
    {
        if (payload_e == payload)
        {
            payload_found = true;
            break;
        }
    }

    if (!payload_found)
        return -EINVAL;

	return spe_phy_FrameGenSetFramePayload(desc->phy_desc, payload);
}

int adin1110_frame_gen_set_frame_len(struct adin1110_desc *desc,
									 uint16_t frame_len)
{
    if ((desc == NULL) || (frame_len < 46) || (frame_len > 1518))
        return -EINVAL;

	return spe_phy_FrameGenSetFrameLen(desc->phy_desc, frame_len);
}

int adin1110_frame_gen_set_ifg_len(struct adin1110_desc *desc, uint16_t ifg_len)
{
    if (desc == NULL)
        return -EINVAL;

	return spe_phy_FrameGenSetIfgLen(desc->phy_desc, ifg_len);
}

int adin1110_frame_gen_restart(struct adin1110_desc *desc)
{
    if (desc == NULL)
        return -EINVAL;

	return spe_phy_FrameGenRestart(desc->phy_desc);
}

int adin1110_frame_gen_done(struct adin1110_desc *desc, bool *fg_done)
{
    if ((desc == NULL) || (fg_done == NULL))
        return -EINVAL;

	return spe_phy_FrameGenDone(desc->phy_desc, fg_done);
}

int adin1110_frame_check_enable(struct adin1110_desc *desc, bool enable)
{
    if (desc == NULL)
        return -EINVAL;

	return spe_phy_FrameChkEn(desc->phy_desc, enable);
}

int adin1110_frame_check_source_select(struct adin1110_desc *desc,
										enum spe_phy_FrameChkSource_e source)
{
    if ((desc == NULL) || ((source != SPE_PHY_FRAME_CHK_SOURCE_MAC) &&
        (source != SPE_PHY_FRAME_CHK_SOURCE_PHY)))
        return -EINVAL;
    
	return spe_phy_FrameChkSourceSelect(desc->phy_desc, source);
}

int adin1110_frame_check_read_frame_count(struct adin1110_desc *desc,
										  uint32_t *count)
{
    if ((desc == NULL) || (count == NULL))
        return -EINVAL;

	return spe_phy_FrameChkReadFrameCnt(desc->phy_desc, count);
}

int adin1110_frame_check_read_rx_err_counnt(struct adin1110_desc *desc,
										    uint16_t *count)
{
    if ((desc == NULL) || (count == NULL))
        return -EINVAL;

	return spe_phy_FrameChkReadRxErrCnt(desc->phy_desc, count);
}

int adin1110_frame_check_read_error_count(struct adin1110_desc *desc,
								  struct spe_phy_FrameChkErrorCounters_t *count)
{
    if ((desc == NULL) || (count == NULL))
        return -EINVAL;

	return spe_phy_FrameChkReadErrorCnt(desc->phy_desc, count);
}

int adin1110_set_user_context(struct adin1110_desc *desc, void *context)
{
    if (desc == NULL)
        return -EINVAL;

    if (desc->device != ADIN_1110)
        return -ENODEV;

    if ((desc->phy_desc == NULL)                                ||
        (desc->phy_desc->state == SPE_PHY_STATE_UNINITIALIZED)	||
        (desc->mac_desc == NULL)                                ||
        (desc->mac_desc->state == ETH_MAC_STATE_UNINITIALIZED))
    {
        return -EBADF;
    }

    desc->user_context = context;

    return 0;
}

void *adin1110_get_user_context(struct adin1110_desc *desc)
{
    if (desc == NULL)
        return NULL;

    if ((desc->phy_desc == NULL)                                ||
        (desc->phy_desc->state == SPE_PHY_STATE_UNINITIALIZED)	||
        (desc->mac_desc == NULL)                                ||
        (desc->mac_desc->state == ETH_MAC_STATE_UNINITIALIZED))
    {
        return NULL;
    }

    return desc->user_context;
}

enum eth_mac_ts_format adin1110_get_timestamp_format(void)
{
	return timestamp_format;
}

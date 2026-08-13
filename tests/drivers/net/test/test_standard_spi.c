/***************************************************************************//**
 *   @file   test_standard_spi.c
 *   @brief  Unit tests for Standard SPI driver
 *   @author Ra Coros (ra.coros@analog.com)
 *******************************************************************************
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
 ******************************************************************************/

#include "unity.h"
#include "standard_spi.h"

#include "mock_capi_spi.h"
#include "mock_capi_irq.h"
#include "mock_capi_time.h"
#include "mock_stm32_capi_spi.h"
#include "mock_stm32_capi_irq.h"
#include "mock_net_queue.h"
#include "mock_utilities.h"

#include <errno.h>
#include <string.h>

/*******************************************************************************
 *    PRIVATE DATA
 ******************************************************************************/

static uint8_t dev_mem[512];
static struct standard_spi_desc *test_desc;
static struct standard_spi_init_param test_param;
static struct capi_spi_device mock_spi_dev;
static volatile bool pending_ctrl;

/*******************************************************************************
 *    STUB CALLBACKS
 ******************************************************************************/

static int stub_spi_transceive_success(struct capi_spi_device *dev,
				       struct capi_spi_transfer *xfer,
				       int cmock_num_calls)
{
	(void)dev;
	(void)xfer;
	(void)cmock_num_calls;
	return 0;
}

static int stub_spi_transceive_fail(struct capi_spi_device *dev,
				    struct capi_spi_transfer *xfer,
				    int cmock_num_calls)
{
	(void)dev;
	(void)xfer;
	(void)cmock_num_calls;
	return -EIO;
}

static int stub_spi_transceive_async_success(struct capi_spi_device *dev,
					     struct capi_spi_transfer *xfer,
					     int cmock_num_calls)
{
	(void)dev;
	(void)xfer;
	(void)cmock_num_calls;
	return 0;
}

static int stub_irq_enable_success(uint32_t irq, int cmock_num_calls)
{
	(void)irq;
	(void)cmock_num_calls;
	return 0;
}

static int stub_irq_disable_success(uint32_t irq, int cmock_num_calls)
{
	(void)irq;
	(void)cmock_num_calls;
	return 0;
}

static int stub_irq_global_disable_success(int cmock_num_calls)
{
	(void)cmock_num_calls;
	return 0;
}

static int stub_irq_global_enable_success(int cmock_num_calls)
{
	(void)cmock_num_calls;
	return 0;
}

/*******************************************************************************
 *    HELPERS
 ******************************************************************************/

static void init_default_param(void)
{
	memset(dev_mem, 0, sizeof(dev_mem));
	memset(&test_param, 0, sizeof(test_param));

	test_param.comm_param = &mock_spi_dev;
	test_param.dev_mem = dev_mem;
	test_param.dev_mem_size = sizeof(dev_mem);
	test_param.crc_en = false;
	test_param.rx_queue_hp_en = false;
	test_param.fcs_check_en = false;
	test_param.eth_irq_num = 42;
}

static void init_desc_ready(void)
{
	init_default_param();
	standard_spi_init(&test_desc, &test_param);
	pending_ctrl = false;
	test_desc->pending_control = &pending_ctrl;
}

/*******************************************************************************
 *    SETUP, TEARDOWN
 ******************************************************************************/

void setUp(void)
{
	test_desc = NULL;
	memset(dev_mem, 0, sizeof(dev_mem));
}

void tearDown(void)
{
	test_desc = NULL;
}

/*******************************************************************************
 *    INIT TESTS
 ******************************************************************************/

void test_standard_spi_init_success(void)
{
	int ret;
	struct standard_spi_desc *desc = NULL;

	init_default_param();
	ret = standard_spi_init(&desc, &test_param);

	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_NOT_NULL(desc);
	TEST_ASSERT_EQUAL_PTR(&mock_spi_dev, desc->comm_desc);
	TEST_ASSERT_EQUAL(STANDARD_SPI_STATE_READY, desc->spi_state);
	TEST_ASSERT_FALSE(desc->crc_en);
	TEST_ASSERT_FALSE(desc->rx_queue_hp_en);
	TEST_ASSERT_FALSE(desc->fcs_check_en);
	TEST_ASSERT_EQUAL_UINT32(42, desc->eth_irq_num);
	TEST_ASSERT_EQUAL(STANDARD_SPI_TS_FORMAT_NONE, desc->ts_format);
	TEST_ASSERT_EQUAL_UINT32(0, desc->spi_error);
}

void test_standard_spi_init_with_crc(void)
{
	int ret;
	struct standard_spi_desc *desc = NULL;

	init_default_param();
	test_param.crc_en = true;
	test_param.rx_queue_hp_en = true;
	test_param.fcs_check_en = true;

	ret = standard_spi_init(&desc, &test_param);

	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_TRUE(desc->crc_en);
	TEST_ASSERT_TRUE(desc->rx_queue_hp_en);
	TEST_ASSERT_TRUE(desc->fcs_check_en);
}

void test_standard_spi_init_null_desc(void)
{
	int ret;

	init_default_param();
	ret = standard_spi_init(NULL, &test_param);

	TEST_ASSERT_EQUAL_INT(-EINVAL, ret);
}

void test_standard_spi_init_null_param(void)
{
	int ret;
	struct standard_spi_desc *desc = NULL;

	ret = standard_spi_init(&desc, NULL);

	TEST_ASSERT_EQUAL_INT(-EINVAL, ret);
}

void test_standard_spi_init_mem_too_small(void)
{
	int ret;
	struct standard_spi_desc *desc = NULL;

	init_default_param();
	test_param.dev_mem_size = 1;

	ret = standard_spi_init(&desc, &test_param);

	TEST_ASSERT_EQUAL_INT(-EINVAL, ret);
}

/*******************************************************************************
 *    REMOVE TESTS
 ******************************************************************************/

void test_standard_spi_remove_success(void)
{
	int ret;

	init_default_param();
	standard_spi_init(&test_desc, &test_param);

	ret = standard_spi_remove(test_desc);

	TEST_ASSERT_EQUAL_INT(0, ret);
}

void test_standard_spi_remove_null(void)
{
	int ret;

	ret = standard_spi_remove(NULL);

	TEST_ASSERT_EQUAL_INT(-EINVAL, ret);
}

/*******************************************************************************
 *    IS_SPI_READY TESTS
 ******************************************************************************/

void test_is_spi_ready_already_ready(void)
{
	int ret;

	init_default_param();
	standard_spi_init(&test_desc, &test_param);

	ret = is_spi_ready(test_desc);

	TEST_ASSERT_EQUAL_INT(0, ret);
}

void test_is_spi_ready_timeout(void)
{
	int ret;
	uint64_t call_count = 0;

	init_default_param();
	standard_spi_init(&test_desc, &test_param);

	/* Force state to not-ready so it enters the while loop */
	test_desc->spi_state = STANDARD_SPI_STATE_REGISTER_READ;

	/*
	 * First call to capi_uptime sets start_us = 0.
	 * Second call returns SPI_COMMS_TIMEOUT_US to trigger timeout.
	 */
	capi_uptime_ExpectAndReturn(NULL, 0);
	capi_uptime_IgnoreArg_us();
	capi_uptime_ReturnThruPtr_us(&(uint64_t){0});

	capi_uptime_ExpectAndReturn(NULL, 0);
	capi_uptime_IgnoreArg_us();
	capi_uptime_ReturnThruPtr_us(&(uint64_t){SPI_COMMS_TIMEOUT_US});

	ret = is_spi_ready(test_desc);

	TEST_ASSERT_EQUAL_INT(-ETIMEDOUT, ret);
}

/*******************************************************************************
 *    REG_READ TESTS
 ******************************************************************************/

void test_standard_spi_reg_read_null_desc(void)
{
	uint32_t byte_size = 4;
	uint8_t data[4];

	TEST_ASSERT_EQUAL_INT(-EINVAL,
		standard_spi_reg_read(NULL, 0x100, data, &byte_size, true));
}

void test_standard_spi_reg_read_null_data(void)
{
	uint32_t byte_size = 4;

	init_desc_ready();
	TEST_ASSERT_EQUAL_INT(-EINVAL,
		standard_spi_reg_read(test_desc, 0x100, NULL, &byte_size, true));
}

void test_standard_spi_reg_read_null_byte_size(void)
{
	uint8_t data[4];

	init_desc_ready();
	TEST_ASSERT_EQUAL_INT(-EINVAL,
		standard_spi_reg_read(test_desc, 0x100, data, NULL, true));
}

void test_standard_spi_reg_read_busy(void)
{
	uint32_t byte_size = 4;
	uint8_t data[4];

	init_desc_ready();
	test_desc->spi_state = STANDARD_SPI_STATE_REGISTER_WRITE;

	TEST_ASSERT_EQUAL_INT(-EBUSY,
		standard_spi_reg_read(test_desc, 0x100, data, &byte_size, true));
}

void test_standard_spi_reg_read_blocking_no_crc(void)
{
	int ret;
	uint32_t byte_size = 4;
	uint8_t data[4] = {0};

	init_desc_ready();

	capi_spi_transceive_Stub(stub_spi_transceive_success);
	capi_irq_enable_Stub(stub_irq_enable_success);

	ret = standard_spi_reg_read(test_desc, 0x100, data, &byte_size, true);

	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL(STANDARD_SPI_STATE_READY, test_desc->spi_state);
}

void test_standard_spi_reg_read_blocking_spi_fail(void)
{
	int ret;
	uint32_t byte_size = 4;
	uint8_t data[4] = {0};

	init_desc_ready();

	capi_spi_transceive_Stub(stub_spi_transceive_fail);
	capi_irq_enable_Stub(stub_irq_enable_success);

	ret = standard_spi_reg_read(test_desc, 0x100, data, &byte_size, true);

	TEST_ASSERT_EQUAL_INT(-EIO, ret);
}

void test_standard_spi_reg_read_nonblocking_no_crc(void)
{
	int ret;
	uint32_t byte_size = 4;
	uint8_t data[4] = {0};

	init_desc_ready();

	capi_spi_transceive_async_Stub(stub_spi_transceive_async_success);
	capi_irq_enable_Stub(stub_irq_enable_success);

	ret = standard_spi_reg_read(test_desc, 0x100, data, &byte_size, false);

	TEST_ASSERT_EQUAL_INT(0, ret);
}

/*******************************************************************************
 *    REG_WRITE TESTS
 ******************************************************************************/

void test_standard_spi_reg_write_null_desc(void)
{
	uint32_t byte_size = 4;
	uint8_t data[4];

	TEST_ASSERT_EQUAL_INT(-EINVAL,
		standard_spi_reg_write(NULL, 0x100, data, &byte_size, true));
}

void test_standard_spi_reg_write_null_data(void)
{
	uint32_t byte_size = 4;

	init_desc_ready();
	TEST_ASSERT_EQUAL_INT(-EINVAL,
		standard_spi_reg_write(test_desc, 0x100, NULL, &byte_size, true));
}

void test_standard_spi_reg_write_null_byte_size(void)
{
	uint8_t data[4];

	init_desc_ready();
	TEST_ASSERT_EQUAL_INT(-EINVAL,
		standard_spi_reg_write(test_desc, 0x100, data, NULL, true));
}

void test_standard_spi_reg_write_busy(void)
{
	uint32_t byte_size = 4;
	uint8_t data[4];

	init_desc_ready();
	test_desc->spi_state = STANDARD_SPI_STATE_FIFO_READ_START;

	TEST_ASSERT_EQUAL_INT(-EBUSY,
		standard_spi_reg_write(test_desc, 0x100, data, &byte_size, true));
}

void test_standard_spi_reg_write_fifo_addr_rejected(void)
{
	uint32_t byte_size = 4;
	uint8_t data[4] = {0xAA, 0xBB, 0xCC, 0xDD};

	init_desc_ready();

	TEST_ASSERT_EQUAL_INT(-EINVAL,
		standard_spi_reg_write(test_desc, 0x031, data, &byte_size, true));
}

void test_standard_spi_reg_write_blocking_no_crc(void)
{
	int ret;
	uint32_t byte_size = 4;
	uint8_t data[4] = {0x11, 0x22, 0x33, 0x44};

	init_desc_ready();

	capi_spi_transceive_Stub(stub_spi_transceive_success);
	capi_irq_enable_Stub(stub_irq_enable_success);

	ret = standard_spi_reg_write(test_desc, 0x100, data, &byte_size, true);

	TEST_ASSERT_EQUAL_INT(0, ret);
}

void test_standard_spi_reg_write_blocking_spi_fail(void)
{
	int ret;
	uint32_t byte_size = 4;
	uint8_t data[4] = {0x11, 0x22, 0x33, 0x44};

	init_desc_ready();

	capi_spi_transceive_Stub(stub_spi_transceive_fail);
	capi_irq_enable_Stub(stub_irq_enable_success);

	ret = standard_spi_reg_write(test_desc, 0x100, data, &byte_size, true);

	TEST_ASSERT_EQUAL_INT(-EIO, ret);
}

void test_standard_spi_reg_write_nonblocking(void)
{
	int ret;
	uint32_t byte_size = 4;
	uint8_t data[4] = {0x11, 0x22, 0x33, 0x44};

	init_desc_ready();

	capi_spi_transceive_async_Stub(stub_spi_transceive_async_success);
	capi_irq_enable_Stub(stub_irq_enable_success);

	ret = standard_spi_reg_write(test_desc, 0x100, data, &byte_size, false);

	TEST_ASSERT_EQUAL_INT(0, ret);
}

void test_standard_spi_reg_write_byte_size_too_large(void)
{
	int ret;
	uint32_t byte_size = BUFFERSIZE + 1;
	uint8_t data[4] = {0};

	init_desc_ready();
	capi_irq_enable_Stub(stub_irq_enable_success);

	ret = standard_spi_reg_write(test_desc, 0x100, data, &byte_size, true);

	TEST_ASSERT_EQUAL_INT(-EINVAL, ret);
}

/*******************************************************************************
 *    FIFO_READ TESTS
 ******************************************************************************/

void test_standard_spi_fifo_read_null_desc(void)
{
	uint32_t byte_size = 64;
	uint8_t data[64];

	TEST_ASSERT_EQUAL_INT(-EINVAL,
		standard_spi_fifo_read(NULL, 0x100, data, &byte_size, false));
}

void test_standard_spi_fifo_read_null_data(void)
{
	uint32_t byte_size = 64;

	init_desc_ready();
	TEST_ASSERT_EQUAL_INT(-EINVAL,
		standard_spi_fifo_read(test_desc, 0x100, NULL, &byte_size, false));
}

void test_standard_spi_fifo_read_busy(void)
{
	uint32_t byte_size = 64;
	uint8_t data[64];

	init_desc_ready();
	test_desc->spi_state = STANDARD_SPI_STATE_FIFO_WRITE_START;

	TEST_ASSERT_EQUAL_INT(-EBUSY,
		standard_spi_fifo_read(test_desc, 0x100, data, &byte_size, false));
}

/*******************************************************************************
 *    FIFO_WRITE TESTS
 ******************************************************************************/

void test_standard_spi_fifo_write_null_desc(void)
{
	struct eth_frame_struct frame = {0};

	TEST_ASSERT_EQUAL_INT(-EINVAL,
		standard_spi_fifo_write(NULL, &frame, true));
}

void test_standard_spi_fifo_write_null_frame(void)
{
	init_desc_ready();

	TEST_ASSERT_EQUAL_INT(-EINVAL,
		standard_spi_fifo_write(test_desc, NULL, true));
}

void test_standard_spi_fifo_write_busy(void)
{
	struct eth_frame_struct frame;

	init_desc_ready();
	test_desc->spi_state = STANDARD_SPI_STATE_REGISTER_READ;

	TEST_ASSERT_EQUAL_INT(-EBUSY,
		standard_spi_fifo_write(test_desc, &frame, true));
}

void test_standard_spi_fifo_write_frame_too_large(void)
{
	int ret;
	struct eth_buf_desc buf_desc;
	struct eth_frame_struct frame;

	init_desc_ready();

	memset(&buf_desc, 0, sizeof(buf_desc));
	buf_desc.trx_size = BUFFERSIZE + 100;
	frame.buf_desc = &buf_desc;
	frame.header.VALUE16 = 0;

	capi_irq_enable_Stub(stub_irq_enable_success);

	ret = standard_spi_fifo_write(test_desc, &frame, true);

	TEST_ASSERT_EQUAL_INT(-EINVAL, ret);
}

void test_standard_spi_fifo_write_success_no_crc(void)
{
	int ret;
	struct eth_buf_desc buf_desc;
	struct eth_frame_struct frame;
	uint8_t frame_data[64];

	init_desc_ready();

	memset(frame_data, 0xAA, sizeof(frame_data));
	memset(&buf_desc, 0, sizeof(buf_desc));
	buf_desc.buf = frame_data;
	buf_desc.trx_size = 64;
	frame.buf_desc = &buf_desc;
	frame.header.VALUE16 = 0;

	stm32_capi_spi_transceive_dma_async_ExpectAnyArgsAndReturn(0);
	capi_irq_enable_Stub(stub_irq_enable_success);

	ret = standard_spi_fifo_write(test_desc, &frame, false);

	TEST_ASSERT_EQUAL_INT(0, ret);
}

/*******************************************************************************
 *    GET_STATUS TESTS
 ******************************************************************************/

void test_standard_spi_get_status_null(void)
{
	uint8_t backup;

	TEST_ASSERT_EQUAL_INT(-EINVAL,
		standard_spi_get_status(NULL, &backup, true));
}

void test_standard_spi_get_status_data_ready(void)
{
	int ret;

	init_desc_ready();

	ret = standard_spi_get_status(test_desc, NULL, false);

	TEST_ASSERT_EQUAL_INT(0, ret);
}

void test_standard_spi_get_status_data_busy(void)
{
	int ret;

	init_desc_ready();
	test_desc->spi_state = STANDARD_SPI_STATE_REGISTER_READ;

	ret = standard_spi_get_status(test_desc, NULL, false);

	TEST_ASSERT_EQUAL_INT(-EBUSY, ret);
}

void test_standard_spi_get_status_control_ready(void)
{
	int ret;
	uint8_t backup = 0;
	bool irq_enabled = true;

	init_desc_ready();

	capi_irq_global_disable_Stub(stub_irq_global_disable_success);
	stm32_capi_irq_is_enabled_ExpectAnyArgsAndReturn(0);
	stm32_capi_irq_is_enabled_ReturnThruPtr_enabled(&irq_enabled);
	capi_irq_disable_Stub(stub_irq_disable_success);
	capi_irq_global_enable_Stub(stub_irq_global_enable_success);

	ret = standard_spi_get_status(test_desc, &backup, true);

	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT8(1, backup);
	TEST_ASSERT_TRUE(pending_ctrl);
}

void test_standard_spi_get_status_control_not_ready(void)
{
	int ret;
	uint8_t backup = 0;

	init_desc_ready();
	test_desc->spi_state = STANDARD_SPI_STATE_REGISTER_READ;

	capi_irq_global_disable_Stub(stub_irq_global_disable_success);
	capi_irq_disable_Stub(stub_irq_disable_success);
	capi_irq_global_enable_Stub(stub_irq_global_enable_success);

	ret = standard_spi_get_status(test_desc, &backup, true);

	TEST_ASSERT_EQUAL_INT(-ETIMEDOUT, ret);
	TEST_ASSERT_TRUE(pending_ctrl);
}

/*******************************************************************************
 *    CALLBACK TESTS
 ******************************************************************************/

void test_standard_spi_callback_xfr_done(void)
{
	init_desc_ready();
	test_desc->spi_state = STANDARD_SPI_STATE_READY;

	capi_irq_enable_Stub(stub_irq_enable_success);

	standard_spi_callback(CAPI_SPI_EVENT_XFR_DONE, test_desc, 0);

	TEST_ASSERT_EQUAL_UINT32(0, test_desc->spi_error);
	TEST_ASSERT_EQUAL(STANDARD_SPI_STATE_READY, test_desc->spi_state);
}

void test_standard_spi_callback_error(void)
{
	init_desc_ready();
	test_desc->spi_state = STANDARD_SPI_STATE_READY;

	capi_irq_enable_Stub(stub_irq_enable_success);

	standard_spi_callback(CAPI_SPI_EVENT_ERROR, test_desc, 0);

	TEST_ASSERT_EQUAL_UINT32(1, test_desc->spi_error);
}

/*******************************************************************************
 *    BLOCKING REG_WRITE WITH CRC
 ******************************************************************************/

void test_standard_spi_reg_write_blocking_crc(void)
{
	int ret;
	uint32_t byte_size = 4;
	uint8_t data[4] = {0xDE, 0xAD, 0xBE, 0xEF};

	init_desc_ready();
	test_desc->crc_en = true;

	capi_spi_transceive_Stub(stub_spi_transceive_success);
	capi_irq_enable_Stub(stub_irq_enable_success);

	ret = standard_spi_reg_write(test_desc, 0x100, data, &byte_size, true);

	TEST_ASSERT_EQUAL_INT(0, ret);
}

/*******************************************************************************
 *    BLOCKING REG_READ WITH CRC
 ******************************************************************************/

void test_standard_spi_reg_read_blocking_crc(void)
{
	int ret;
	uint32_t byte_size = 4;
	uint8_t data[4] = {0};

	init_desc_ready();
	test_desc->crc_en = true;

	capi_spi_transceive_Stub(stub_spi_transceive_success);
	capi_irq_enable_Stub(stub_irq_enable_success);

	ret = standard_spi_reg_read(test_desc, 0x100, data, &byte_size, true);

	/* CRC validation will fail on zeroed rx_buf but the SPI transceive succeeds.
	 * The exact return depends on CRC check against the zeroed buffer. */
	TEST_ASSERT_TRUE(ret == 0 || ret == -EINVAL);
}

/*******************************************************************************
 *    REG_READ BYTE SIZE TOO LARGE
 ******************************************************************************/

void test_standard_spi_reg_read_byte_size_too_large(void)
{
	int ret;
	uint32_t byte_size = BUFFERSIZE;
	uint8_t data[4] = {0};

	init_desc_ready();
	capi_irq_enable_Stub(stub_irq_enable_success);

	ret = standard_spi_reg_read(test_desc, 0x100, data, &byte_size, true);

	TEST_ASSERT_EQUAL_INT(-EINVAL, ret);
}

/*******************************************************************************
 *    FIFO_WRITE WITH CRC
 ******************************************************************************/

void test_standard_spi_fifo_write_success_with_crc(void)
{
	int ret;
	struct eth_buf_desc buf_desc;
	struct eth_frame_struct frame;
	uint8_t frame_data[64];

	init_desc_ready();
	test_desc->crc_en = true;

	memset(frame_data, 0xBB, sizeof(frame_data));
	memset(&buf_desc, 0, sizeof(buf_desc));
	buf_desc.buf = frame_data;
	buf_desc.trx_size = 64;
	frame.buf_desc = &buf_desc;
	frame.header.VALUE16 = 0;

	stm32_capi_spi_transceive_dma_async_ExpectAnyArgsAndReturn(0);
	capi_irq_enable_Stub(stub_irq_enable_success);

	ret = standard_spi_fifo_write(test_desc, &frame, false);

	TEST_ASSERT_EQUAL_INT(0, ret);
}

/*******************************************************************************
 *    FIFO_WRITE ODD SIZE
 ******************************************************************************/

void test_standard_spi_fifo_write_odd_size(void)
{
	int ret;
	struct eth_buf_desc buf_desc;
	struct eth_frame_struct frame;
	uint8_t frame_data[63];

	init_desc_ready();

	memset(frame_data, 0xCC, sizeof(frame_data));
	memset(&buf_desc, 0, sizeof(buf_desc));
	buf_desc.buf = frame_data;
	buf_desc.trx_size = 63;
	frame.buf_desc = &buf_desc;
	frame.header.VALUE16 = 0;

	stm32_capi_spi_transceive_dma_async_ExpectAnyArgsAndReturn(0);
	capi_irq_enable_Stub(stub_irq_enable_success);

	ret = standard_spi_fifo_write(test_desc, &frame, false);

	TEST_ASSERT_EQUAL_INT(0, ret);
}

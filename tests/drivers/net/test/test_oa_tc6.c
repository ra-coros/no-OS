/*******************************************************************************
 *   @file   test_oa_tc6.c
 *   @brief  Unit tests for OA TC6 driver
 *   @author Christine Joy Murillo (christinejoy.murillo@analog.com)
 *******************************************************************************
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
 ******************************************************************************/
E
#include "unity.h"

#include <errno.h>
#include <string.h>
#include <stdint.h>

#include "no_os_util.h"
#include "mock_capi_spi.h"     /* auto-mocks capi_spi_transceive, etc. */
#include "mock_capi_irq.h"     /* auto-mocks capi_irq_enable, capi_irq_disable */
#include "mock_capi_alloc.h"   /* auto-mocks capi_calloc, capi_free */
#include "mock_capi_time.h"    /* auto-mocks capi_uptime */
#include "mock_net_queue.h"    /* auto-mocks net_queue_is_empty, etc. */
#include "mock_utilities.h"    /* auto-mocks calculate_parity, calculate_fcs */

#include "oa_tc6.c"            /*Exposes static functions for testing */
#include "oa_tc6.h"
E
/******************************************************************************/
/*                    Test Data and Helpers                                    */
/******************************************************************************/

/* Fixture set 1: for tests that go through oa_tc6_init(). */
static struct capi_spi_device test_spi_dev;
static struct oa_tc6_desc *test_desc;
static struct oa_tc6_init_param test_param;
static uint8_t test_dev_mem[sizeof(struct oa_tc6_desc)] __attribute__((aligned(4)));

/* Fixture set 2: for tests that pre-populate a descriptor directly */
static struct capi_spi_device g_spi_dev;
static struct oa_tc6_desc g_desc;
static struct oa_tc6_init_param g_param;
static uint8_t g_dev_mem[sizeof(struct oa_tc6_desc)] __attribute__((aligned(4)));
static struct net_queue g_rx_queue_storage;
static struct net_queue *g_rx_queue_ptr = &g_rx_queue_storage;
static struct net_queue g_tx_queue_storage;
static volatile bool g_pending_ctrl;
static struct eth_status_registers g_status_regs;
static uint32_t g_irq_mask0;
static uint32_t g_irq_mask1;
static uint32_t g_phy_irq_mask;
static uint32_t g_reg_data;

/* RX/TX frame fixture: one entry each */
#define G_FRAME_BUF_SIZE 128
static uint8_t g_rx_frame_buf[G_FRAME_BUF_SIZE];
static uint8_t g_tx_frame_buf[G_FRAME_BUF_SIZE];
static struct eth_buf_desc g_rx_buf_desc;
static struct eth_buf_desc g_tx_buf_desc;
static struct eth_frame_struct g_rx_entries[1];
static struct eth_frame_struct g_tx_entries[1];
static eth_callback_t g_cb_funcs[MAC_EVT_MAX];

/* Test-scope callback capture counters (used by test_cb_function_caller_*). */
/* These are callbacks set by the application layer */
static uint32_t g_cb_dyn_calls;
static uint32_t g_cb_buf_calls;
static void g_cb_dyn_tbl_stub(void *cd_param, uint32_t event, void *arg)
{
	(void)cd_param; (void)event; (void)arg;
	g_cb_dyn_calls++;
}
static void g_cb_buf_stub(void *cd_param, uint32_t event, void *arg)
{
	(void)cd_param; (void)event; (void)arg;
	g_cb_buf_calls++;
}

static void init_default_param(void)
{
	memset(&test_spi_dev, 0, sizeof(test_spi_dev));
	memset(&test_dev_mem, 0, sizeof(test_dev_mem));
	memset(&test_param, 0, sizeof(test_param));

	test_param.comm_desc = &test_spi_dev;
	test_param.p_dev_mem = test_dev_mem;
	test_param.dev_mem_size = sizeof(test_dev_mem);
	test_param.prote_spi = false;
	test_param.rx_queue_hp_en = false;
	test_param.fcs_check_en = true;
	test_param.num_ports = 1;

	test_desc = NULL;
}

/* Reset the "direct-desc" fixture to a known-good, ready-to-drive state. */
static void reset_fixtures(void)
{
	memset(&g_spi_dev, 0, sizeof(g_spi_dev));
	memset(&g_desc,    0, sizeof(g_desc));
	memset(&g_param,   0, sizeof(g_param));
	memset(g_dev_mem,  0, sizeof(g_dev_mem));
	memset(&g_rx_queue_storage, 0, sizeof(g_rx_queue_storage));
	memset(&g_tx_queue_storage, 0, sizeof(g_tx_queue_storage));
	memset(&g_status_regs, 0, sizeof(g_status_regs));
	memset(g_rx_frame_buf, 0, sizeof(g_rx_frame_buf));
	memset(g_tx_frame_buf, 0, sizeof(g_tx_frame_buf));
	memset(&g_rx_buf_desc, 0, sizeof(g_rx_buf_desc));
	memset(&g_tx_buf_desc, 0, sizeof(g_tx_buf_desc));
	memset(g_rx_entries, 0, sizeof(g_rx_entries));
	memset(g_tx_entries, 0, sizeof(g_tx_entries));
	memset(g_cb_funcs, 0, sizeof(g_cb_funcs));
	g_cb_dyn_calls = 0;
	g_cb_buf_calls = 0;
	g_irq_mask0 = 0;
	g_irq_mask1 = 0;
	g_phy_irq_mask = 0;
	g_reg_data = 0;
	g_pending_ctrl = false;

	g_rx_buf_desc.buf         = g_rx_frame_buf;
	g_rx_buf_desc.buf_size    = G_FRAME_BUF_SIZE;
	g_tx_buf_desc.buf         = g_tx_frame_buf;
	g_tx_buf_desc.buf_size    = G_FRAME_BUF_SIZE;
	g_rx_entries[0].buf_desc  = &g_rx_buf_desc;
	g_tx_entries[0].buf_desc  = &g_tx_buf_desc;
	g_rx_queue_storage.entries     = g_rx_entries;
	g_rx_queue_storage.num_entries = 1;
	g_tx_queue_storage.entries     = g_tx_entries;
	g_tx_queue_storage.num_entries = 1;

	g_desc.comm_desc     = &g_spi_dev;
	g_desc.spi_state     = OA_SPI_STATE_READY;
	g_desc.spi_err       = 0;
	g_desc.oa_cps        = OA_DEFAULT_CPS;
	g_desc.oa_txc        = 31;
	g_desc.oa_rca        = 0;
	g_desc.num_ports     = 1;
	g_desc.fcs_check_en  = true;
	g_desc.prote_spi     = false;
	g_desc.tx_queue      = &g_tx_queue_storage;
	g_desc.rx_queue      = &g_rx_queue_ptr;
	g_desc.pending_ctrl  = &g_pending_ctrl;
	g_desc.eth_irq       = 15;
	g_desc.oa_max_chunk_count = OA_MAX_CHUNK64_COUNT;
	g_desc.status_regs   = &g_status_regs;
	g_desc.irq_mask0     = &g_irq_mask0;
	g_desc.irq_mask1     = &g_irq_mask1;
	g_desc.phy_irq_mask  = &g_phy_irq_mask;
	g_desc.reg_data      = &g_reg_data;
	g_desc.cb_func       = g_cb_funcs;
}

/******************************************************************************/
/*                          setUp / tearDown                                   */
/******************************************************************************/

void setUp(void)
{
	init_default_param();
	reset_fixtures();
}

void tearDown(void)
{
	test_desc = NULL;
}

/******************************************************************************/
/*                      oa_tc6_init test cases                                 */
/******************************************************************************/

void test_oa_tc6_init_null_desc_returns_einval(void)
{
	int ret = oa_tc6_init(NULL, &test_param);

	TEST_ASSERT_EQUAL_INT(-EINVAL, ret);
}

void test_oa_tc6_init_null_param_returns_einval(void)
{
	int ret = oa_tc6_init(&test_desc, NULL);

	TEST_ASSERT_EQUAL_INT(-EINVAL, ret);
}

void test_oa_tc6_init_dev_mem_too_small_returns_enomem(void)
{
	test_param.dev_mem_size = 4;

	int ret = oa_tc6_init(&test_desc, &test_param);

	TEST_ASSERT_EQUAL_INT(-ENOMEM, ret);
}

void test_oa_tc6_init_success_sets_desc_pointer(void)
{
	int ret = oa_tc6_init(&test_desc, &test_param);

	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_NOT_NULL(test_desc);
	TEST_ASSERT_EQUAL_PTR(test_dev_mem, test_desc);
}

void test_oa_tc6_init_success_sets_comm_desc(void)
{
	oa_tc6_init(&test_desc, &test_param);

	TEST_ASSERT_EQUAL_PTR(&test_spi_dev, test_desc->comm_desc);
}

void test_oa_tc6_init_success_sets_prote_spi(void)
{
	test_param.prote_spi = true;

	oa_tc6_init(&test_desc, &test_param);

	TEST_ASSERT_TRUE(test_desc->prote_spi);
}

void test_oa_tc6_init_success_sets_num_ports(void)
{
	test_param.num_ports = 2;

	oa_tc6_init(&test_desc, &test_param);

	TEST_ASSERT_EQUAL_UINT8(2, test_desc->num_ports);
}

/* Full-state init check for the num_ports=2 configuration.
 * Verifies that flipping num_ports to 2 does not perturb the rest of the
 * ready-state contract (spi_state, cps/txc/rca defaults, valid_flag, stats). */
void test_oa_tc6_init_num_ports_2_full_ready_state(void)
{
	test_param.num_ports = 2;

	int ret = oa_tc6_init(&test_desc, &test_param);

	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_NOT_NULL(test_desc);
	TEST_ASSERT_EQUAL_UINT8(2, test_desc->num_ports);
	TEST_ASSERT_EQUAL_PTR(&test_spi_dev, test_desc->comm_desc);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_READY, test_desc->spi_state);
	TEST_ASSERT_EQUAL_UINT32(0, test_desc->spi_err);
	TEST_ASSERT_EQUAL_UINT32(OA_DEFAULT_CPS, test_desc->oa_cps);
	TEST_ASSERT_EQUAL_UINT32(OA_MAX_CHUNK64_COUNT, test_desc->oa_max_chunk_count);
	TEST_ASSERT_EQUAL_UINT32(31, test_desc->oa_txc);
	TEST_ASSERT_EQUAL_UINT32(0, test_desc->oa_rca);
	TEST_ASSERT_EQUAL_UINT(OA_VALID_FLAG_NONE, test_desc->oa_valid_flag);
	TEST_ASSERT_EQUAL_UINT(OA_TS_FORMAT_NONE, test_desc->ts_format);
	TEST_ASSERT_EQUAL_UINT32(0, test_desc->error_stats.fd_count);
	TEST_ASSERT_EQUAL_UINT32(0, test_desc->error_stats.invalid_sv_count);
	TEST_ASSERT_EQUAL_UINT32(0, test_desc->error_stats.invalid_ev_count);
	TEST_ASSERT_EQUAL_UINT32(0, test_desc->error_stats.ftr_parity_error_count);
	TEST_ASSERT_EQUAL_UINT32(0, test_desc->error_stats.hdr_parity_error_count);
	TEST_ASSERT_EQUAL_UINT32(0, test_desc->error_stats.sync_error_count);
}

void test_oa_tc6_init_success_sets_fcs_check_en(void)
{
	test_param.fcs_check_en = true;

	oa_tc6_init(&test_desc, &test_param);

	TEST_ASSERT_TRUE(test_desc->fcs_check_en);
}

void test_oa_tc6_init_success_sets_rx_queue_hp_en(void)
{
	test_param.rx_queue_hp_en = true;

	oa_tc6_init(&test_desc, &test_param);

	TEST_ASSERT_TRUE(test_desc->rx_queue_hp_en);
}

void test_oa_tc6_init_success_state_is_ready(void)
{
	oa_tc6_init(&test_desc, &test_param);

	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_READY, test_desc->spi_state);
}

void test_oa_tc6_init_success_spi_err_is_zero(void)
{
	oa_tc6_init(&test_desc, &test_param);

	TEST_ASSERT_EQUAL_UINT32(0, test_desc->spi_err);
}

void test_oa_tc6_init_success_txc_initialized_to_31(void)
{
	oa_tc6_init(&test_desc, &test_param);

	TEST_ASSERT_EQUAL_UINT32(31, test_desc->oa_txc);
}

void test_oa_tc6_init_success_rca_initialized_to_zero(void)
{
	oa_tc6_init(&test_desc, &test_param);

	TEST_ASSERT_EQUAL_UINT32(0, test_desc->oa_rca);
}

void test_oa_tc6_init_success_cps_set_to_default(void)
{
	oa_tc6_init(&test_desc, &test_param);

	TEST_ASSERT_EQUAL_UINT32(OA_DEFAULT_CPS, test_desc->oa_cps);
}

void test_oa_tc6_init_success_max_chunk_count_set(void)
{
	oa_tc6_init(&test_desc, &test_param);

	TEST_ASSERT_EQUAL_UINT32(OA_MAX_CHUNK64_COUNT, test_desc->oa_max_chunk_count);
}

void test_oa_tc6_init_success_valid_flag_is_none(void)
{
	oa_tc6_init(&test_desc, &test_param);

	TEST_ASSERT_EQUAL_UINT(OA_VALID_FLAG_NONE, test_desc->oa_valid_flag);
}

void test_oa_tc6_init_success_error_stats_zeroed(void)
{
	oa_tc6_init(&test_desc, &test_param);

	TEST_ASSERT_EQUAL_UINT32(0, test_desc->error_stats.fd_count);
	TEST_ASSERT_EQUAL_UINT32(0, test_desc->error_stats.invalid_sv_count);
	TEST_ASSERT_EQUAL_UINT32(0, test_desc->error_stats.invalid_ev_count);
	TEST_ASSERT_EQUAL_UINT32(0, test_desc->error_stats.ftr_parity_error_count);
	TEST_ASSERT_EQUAL_UINT32(0, test_desc->error_stats.hdr_parity_error_count);
	TEST_ASSERT_EQUAL_UINT32(0, test_desc->error_stats.sync_error_count);
}

void test_oa_tc6_init_success_buffers_zeroed(void)
{
	uint8_t zero_buf[OA_CTRL_BUF_SIZE] = {0};

	oa_tc6_init(&test_desc, &test_param);

	TEST_ASSERT_EQUAL_UINT8_ARRAY(zero_buf, test_desc->ctrl_tx_buf, OA_CTRL_BUF_SIZE);
	TEST_ASSERT_EQUAL_UINT8_ARRAY(zero_buf, test_desc->ctrl_rx_buf, OA_CTRL_BUF_SIZE);
}

void test_oa_tc6_init_success_timestamp_format_none(void)
{
	oa_tc6_init(&test_desc, &test_param);

	TEST_ASSERT_EQUAL_UINT(OA_TS_FORMAT_NONE, test_desc->ts_format);
}

/******************************************************************************/
/*                      oa_tc6_remove test cases                               */
/******************************************************************************/

void test_oa_tc6_remove_null_returns_enodev(void)
{
	int ret = oa_tc6_remove(NULL);

	TEST_ASSERT_EQUAL_INT(-ENODEV, ret);
}

void test_oa_tc6_remove_success_returns_zero(void)
{
	oa_tc6_init(&test_desc, &test_param);

	int ret = oa_tc6_remove(test_desc);

	TEST_ASSERT_EQUAL_INT(0, ret);
}

void test_oa_tc6_remove_zeroes_descriptor(void)
{
	uint8_t zero_buf[sizeof(struct oa_tc6_desc)] = {0};

	oa_tc6_init(&test_desc, &test_param);
	oa_tc6_remove(test_desc);

	TEST_ASSERT_EQUAL_UINT8_ARRAY(zero_buf, (uint8_t *)test_desc, sizeof(struct oa_tc6_desc));
}

void test_oa_tc6_remove_clears_comm_desc(void)
{
	oa_tc6_init(&test_desc, &test_param);
	oa_tc6_remove(test_desc);

	TEST_ASSERT_NULL(test_desc->comm_desc);
}

void test_oa_tc6_remove_after_init_full_cycle(void)
{
	int ret;

	ret = oa_tc6_init(&test_desc, &test_param);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_NOT_NULL(test_desc);

	ret = oa_tc6_remove(test_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
}

/*==============================================================================
 * PUBLIC API tests
 *============================================================================*/

/* ------ oa_tc6_reg_read_async -------------------------------------------------
 * Contract:
 *   - NULL desc -> -ENODEV
 *   - Populates desc->{wnr,reg_addr,reg_data,cnt} then transitions to
 *     OA_SPI_STATE_CTRL_START and enters the state machine which fires
 *     capi_spi_transceive_async(). */
void test_reg_read_async_null_desc_returns_enodev(void)
{
	uint32_t data = 0;
	TEST_ASSERT_EQUAL_INT(-ENODEV, oa_tc6_reg_read_async(NULL, 0x08, &data, 1));
}

void test_reg_read_async_sets_wnr_read_and_fields(void)
{
	capi_spi_transceive_async_IgnoreAndReturn(0);
	calculate_parity_IgnoreAndReturn(1);
	uint32_t val = 0;
	int ret = oa_tc6_reg_read_async(&g_desc, 0x1234, &val, 2);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT32(OA_SPI_READ, g_desc.wnr);
	TEST_ASSERT_EQUAL_UINT32(0x1234, g_desc.reg_addr);
	TEST_ASSERT_EQUAL_PTR  (&val, g_desc.reg_data);
	TEST_ASSERT_EQUAL_UINT32(2, g_desc.cnt);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_CTRL_END, g_desc.spi_state);
}

/* ------ oa_tc6_reg_write_async ------------------------------------------------
 * Same shape as read_async but sets wnr=OA_SPI_WRITE. */
void test_reg_write_async_null_desc_returns_enodev(void)
{
	uint32_t val = 0xDEABCAFE;
	TEST_ASSERT_EQUAL_INT(-ENODEV, oa_tc6_reg_write_async(NULL, 0x08, &val, 1));
}

void test_reg_write_async_sets_wnr_write(void)
{
	capi_spi_transceive_async_IgnoreAndReturn(0);
	calculate_parity_IgnoreAndReturn(1);
	uint32_t val = 0xAABBCCDD;
	int ret = oa_tc6_reg_write_async(&g_desc, 0x0004, &val, 1);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT32(OA_SPI_WRITE, g_desc.wnr);
	TEST_ASSERT_EQUAL_UINT32(0x0004,       g_desc.reg_addr);
}

/* ------ oa_tc6_process_tx_frame ----------------------------------------------
 * Stores `blocking`, sets state to DATA_START, calls state machine. */
void test_process_tx_frame_stores_blocking_flag(void)
{
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);
	capi_irq_enable_IgnoreAndReturn(0);
	capi_irq_disable_IgnoreAndReturn(0);
	calculate_parity_IgnoreAndReturn(1);
	net_queue_is_empty_IgnoreAndReturn(true);
	net_queue_is_full_IgnoreAndReturn(false);

	g_desc.oa_rca = 1;
	int ret = oa_tc6_process_tx_frame(&g_desc, true);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_TRUE(g_desc.blocking);
        TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_DATA_END, g_desc.spi_state);
}

/* ------ oa_tc6_irq_handler ----------------------------------------------------
 * Contract:
 *   - NULL desc -> -EINVAL
 *   - If state != READY, no-op.
 *   - If state == READY, disable IRQ, move to IRQ_START, run state machine. */
void test_irq_handler_null_desc_returns_einval(void)
{
	TEST_ASSERT_EQUAL_INT(-EINVAL, oa_tc6_irq_handler(NULL));
}

void test_irq_handler_busy_state(void)
{
	g_desc.spi_state = OA_SPI_STATE_CTRL_END;
	int ret = oa_tc6_irq_handler(&g_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_CTRL_END, g_desc.spi_state);
}

void test_irq_handler_ready_state_starts_irq(void)
{
	capi_irq_disable_IgnoreAndReturn(0);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);
	calculate_parity_IgnoreAndReturn(1);

	int ret = oa_tc6_irq_handler(&g_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_DATA_END, g_desc.spi_state);
}

/* ------ oa_tc6_spi_callback ---------------------------------------------------
 * Casts cb_param to (struct oa_tc6_desc *) and re-runs state machine. */
void test_spi_callback_runs_state_machine(void)
{
	g_desc.spi_state = OA_SPI_STATE_READY;
	capi_irq_enable_IgnoreAndReturn(0);
	oa_tc6_spi_callback(&g_desc, 0, NULL);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_READY, g_desc.spi_state);
}

/* ------ oa_tc6_wait_spi_ready ------------------------------------------------
 * State already READY -> returns 0 immediately.
 * Else polls capi_uptime until timeout -> -ETIMEDOUT. */
void test_wait_spi_ready_already_ready(void)
{
	g_desc.spi_state = OA_SPI_STATE_READY;
	TEST_ASSERT_EQUAL_INT(0, oa_tc6_wait_spi_ready(&g_desc));
}

void test_wait_spi_ready_times_out(void)
{
	g_desc.spi_state = OA_SPI_STATE_CTRL_END;
	capi_uptime_ExpectAnyArgsAndReturn(0);
	capi_uptime_ReturnThruPtr_us(&(uint64_t){0});
	capi_uptime_ExpectAnyArgsAndReturn(0);
	capi_uptime_ReturnThruPtr_us(&(uint64_t){SPI_COMMS_TIMEOUT_US + 1});

	TEST_ASSERT_EQUAL_INT(-ETIMEDOUT, oa_tc6_wait_spi_ready(&g_desc));
}

/* ------ oa_tc6_wait_get_status -----------------------------------------------
 * NULL desc -> -EINVAL
 * is_ctrl=false: state != READY -> -EBUSY, else 0. */
void test_wait_get_status_null_desc_returns_einval(void)
{
	uint8_t backup = 0;
	TEST_ASSERT_EQUAL_INT(-EINVAL, oa_tc6_wait_get_status(NULL, &backup, false));
}

void test_wait_get_status_non_ctrl_ready_returns_zero(void)
{
	uint8_t backup = 0;
	g_desc.spi_state = OA_SPI_STATE_READY;
	TEST_ASSERT_EQUAL_INT(0, oa_tc6_wait_get_status(&g_desc, &backup, false));
}

void test_wait_get_status_non_ctrl_busy_returns_ebusy(void)
{
	uint8_t backup = 0;
	g_desc.spi_state = OA_SPI_STATE_CTRL_END;
	TEST_ASSERT_EQUAL_INT(-EBUSY, oa_tc6_wait_get_status(&g_desc, &backup, false));
}

/* ------ oa_tc6_ctrl_cmd_header -----------------------------------------------
 * Packs 4-byte BE header:
 *   DNC(31) | HDRB(30) | WNR(29) | AID(28) | MMS(27:24) |
 *   ADDR(23:8) | LEN(7:1) | P(0)
 * MMS derived from addr: 0 if addr < 0x30, else 1. LEN = cnt-1. */
void test_ctrl_cmd_header_read_mms0_addr8(void)
{
	calculate_parity_ExpectAnyArgsAndReturn(1);
	uint8_t buf[4] = {0};
        uint32_t addr = 0x08;
	oa_tc6_ctrl_cmd_header(buf, OA_SPI_READ, addr, 2);

	uint32_t val = no_os_get_unaligned_be32(buf);
	TEST_ASSERT_EQUAL_UINT32(0x00, no_os_field_get(OA_CTRL_HEADER_DNC_MASK, val));
	TEST_ASSERT_EQUAL_UINT32(0x00, no_os_field_get(OA_CTRL_HEADER_HDRB_MASK, val));
        TEST_ASSERT_EQUAL_UINT32(OA_SPI_READ, no_os_field_get(OA_CTRL_HEADER_WNR_MASK, val));
        TEST_ASSERT_EQUAL_UINT32(0x00, no_os_field_get(OA_CTRL_HEADER_AID_MASK, val));
        TEST_ASSERT_EQUAL_UINT32(0x00, no_os_field_get(OA_CTRL_HEADER_HDRB_MASK, val));
	TEST_ASSERT_EQUAL_UINT32(0x00, no_os_field_get(OA_CTRL_HEADER_MMS_MASK, val));
	TEST_ASSERT_EQUAL_UINT32(0x08, no_os_field_get(OA_CTRL_HEADER_ADDR_MASK, val));
	TEST_ASSERT_EQUAL_UINT32(1, no_os_field_get(OA_CTRL_HEADER_LEN_MASK,  val));
        TEST_ASSERT_EQUAL_UINT32(1, no_os_field_get(OA_CTRL_HEADER_P_MASK, val));
}

void test_ctrl_cmd_header_write_mms1_when_addr_ge_0x30(void)
{
	calculate_parity_ExpectAnyArgsAndReturn(1);
	uint8_t buf[4] = {0};
	oa_tc6_ctrl_cmd_header(buf, OA_SPI_WRITE, 0x40, 2);

	uint32_t val = no_os_get_unaligned_be32(buf);
	TEST_ASSERT_EQUAL_UINT32(0x00, no_os_field_get(OA_CTRL_HEADER_DNC_MASK, val));
	TEST_ASSERT_EQUAL_UINT32(0x00, no_os_field_get(OA_CTRL_HEADER_HDRB_MASK, val));
        TEST_ASSERT_EQUAL_UINT32(OA_SPI_WRITE, no_os_field_get(OA_CTRL_HEADER_WNR_MASK, val));
        TEST_ASSERT_EQUAL_UINT32(0x00, no_os_field_get(OA_CTRL_HEADER_AID_MASK, val));
        TEST_ASSERT_EQUAL_UINT32(0x00, no_os_field_get(OA_CTRL_HEADER_HDRB_MASK, val));
	TEST_ASSERT_EQUAL_UINT32(1, no_os_field_get(OA_CTRL_HEADER_MMS_MASK, val));
	TEST_ASSERT_EQUAL_UINT32(0x40, no_os_field_get(OA_CTRL_HEADER_ADDR_MASK, val));
	TEST_ASSERT_EQUAL_UINT32(1, no_os_field_get(OA_CTRL_HEADER_LEN_MASK,  val));
        TEST_ASSERT_EQUAL_UINT32(1, no_os_field_get(OA_CTRL_HEADER_P_MASK, val));
}

/* ------ oa_tc6_ctrl_cmd_write_data -------------------------------------------
 * Unprotected: N regs -> N * 4 BE bytes.
 * Protected: for each reg, packs value THEN its bitwise complement. */
void test_ctrl_cmd_write_data_unprotected(void)
{
	uint8_t dst[8] = {0};
	uint32_t src[2] = { 0x11223344, 0xAABBCCDD };
	oa_tc6_ctrl_cmd_write_data(dst, src, 2, false);
	TEST_ASSERT_EQUAL_HEX32(0x11223344, no_os_get_unaligned_be32(&dst[0]));
	TEST_ASSERT_EQUAL_HEX32(0xAABBCCDD, no_os_get_unaligned_be32(&dst[4]));
}

void test_ctrl_cmd_write_data_protected_adds_complement(void)
{
	uint8_t dst[16] = {0};
	uint32_t src[2] = { 0x11223344, 0xAABBCCDD };
	oa_tc6_ctrl_cmd_write_data(dst, src, 2, true);
	TEST_ASSERT_EQUAL_HEX32(0x11223344, no_os_get_unaligned_be32(&dst[0]));
	TEST_ASSERT_EQUAL_HEX32(~0x11223344u, no_os_get_unaligned_be32(&dst[4]));
	TEST_ASSERT_EQUAL_HEX32(0xAABBCCDD, no_os_get_unaligned_be32(&dst[8]));
	TEST_ASSERT_EQUAL_HEX32(~0xAABBCCDDu, no_os_get_unaligned_be32(&dst[12]));
}

/* ------ oa_tc6_ctrl_cmd_read_data --------------------------------------------
 * Unprotected: reads N 4-byte BE words from src.
 * Protected: reads value + complement, -EPROTO on mismatch. */
void test_ctrl_cmd_read_data_unprotected(void)
{
	uint32_t dst[2] = {0};
	uint8_t  src[8] = {0};
	no_os_put_unaligned_be32(0x11223344, &src[0]);
	no_os_put_unaligned_be32(0x55667788, &src[4]);
	TEST_ASSERT_EQUAL_INT(0, oa_tc6_ctrl_cmd_read_data(dst, src, 2, false));
	TEST_ASSERT_EQUAL_HEX32(0x11223344, dst[0]);
	TEST_ASSERT_EQUAL_HEX32(0x55667788, dst[1]);
}

void test_ctrl_cmd_read_data_protected_matches_complement(void)
{
	uint32_t dst[1] = {0};
	uint8_t  src[8] = {0};
	no_os_put_unaligned_be32(0x11223344,  &src[0]);
	no_os_put_unaligned_be32(~0x11223344u, &src[4]);
	TEST_ASSERT_EQUAL_INT(0, oa_tc6_ctrl_cmd_read_data(dst, src, 1, true));
	TEST_ASSERT_EQUAL_HEX32(0x11223344, dst[0]);
}

void test_ctrl_cmd_read_data_protected_mismatch_returns_eproto(void)
{
	uint32_t dst[1] = {0};
	uint8_t  src[8] = {0};
	no_os_put_unaligned_be32(0x11223344, &src[0]);
	no_os_put_unaligned_be32(0xABCDEFAA, &src[4]);
	TEST_ASSERT_EQUAL_INT(-EPROTO, oa_tc6_ctrl_cmd_read_data(dst, src, 1, true));
}

/* ------ oa_tc6_ctrl_setup ----------------------------------------------------
 * Builds header at buf[0..3], (optionally) write payload from buf[4..],
 * updates *len from word count to byte count.
 * -EINVAL if requested size > OA_CTRL_BUF_SIZE - 2. */
void test_ctrl_setup_read_no_prote_computes_len_in_bytes(void)
{
	uint8_t buf[64] = {0};
	uint32_t len = 1;
	calculate_parity_ExpectAnyArgsAndReturn(0);
	int ret = oa_tc6_ctrl_setup(buf, OA_SPI_READ, 0x08, NULL, &len, false);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT32(12, len);
}

void test_ctrl_setup_write_prote_doubles_payload_size(void)
{
	uint8_t buf[64] = {0};
	uint32_t data = 0xAABBCCDD;
	uint32_t len = 1;
	calculate_parity_ExpectAnyArgsAndReturn(0);
	int ret = oa_tc6_ctrl_setup(buf, OA_SPI_WRITE, 0x08, &data, &len, true);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT32(16, len);
}

void test_ctrl_setup_too_large_returns_einval(void)
{
	uint8_t buf[OA_CTRL_BUF_SIZE] = {0};
	uint32_t len = OA_CTRL_BUF_SIZE;
	int ret = oa_tc6_ctrl_setup(buf, OA_SPI_READ, 0x08, NULL, &len, false);
	TEST_ASSERT_EQUAL_INT(-EINVAL, ret);
}

/* ------ oa_tc6_chunk_error_detector ------------------------------------------
 * Footer == OA_HEADER_BAD -> hdr_parity_error_count++, returns true.
 * Otherwise checks footer parity and SYNC bit. */
void test_chunk_error_detector_bad_header_pattern(void)
{
	TEST_ASSERT_TRUE(oa_tc6_chunk_error_detector(&g_desc, OA_HEADER_BAD));
	TEST_ASSERT_EQUAL_UINT32(1, g_desc.error_stats.hdr_parity_error_count);
}

void test_chunk_error_detector_parity_fail(void)
{
	calculate_parity_ExpectAnyArgsAndReturn(0);
	TEST_ASSERT_TRUE(oa_tc6_chunk_error_detector(&g_desc, 0x00000000));
	TEST_ASSERT_EQUAL_UINT32(1, g_desc.error_stats.ftr_parity_error_count);
	TEST_ASSERT_EQUAL_UINT32(1, g_desc.error_stats.sync_error_count);
}

void test_chunk_error_detector_valid_footer(void)
{
	calculate_parity_ExpectAnyArgsAndReturn(1);
	uint32_t footer = OA_DATA_FOOTER_SYNC_MASK;
	TEST_ASSERT_FALSE(oa_tc6_chunk_error_detector(&g_desc, footer));
	TEST_ASSERT_EQUAL_UINT32(0, g_desc.error_stats.ftr_parity_error_count);
	TEST_ASSERT_EQUAL_UINT32(0, g_desc.error_stats.sync_error_count);
}

/* ------ oa_tc6_complete_transmission_checker ---------------------------------
 * EV=0 -> no-op. EV=1 requires eth_frame_struct fixture. */
void test_complete_transmission_checker_ev_zero_is_noop(void)
{
	uint32_t event = 0;
	oa_tc6_complete_transmission_checker(&g_desc, 0, &event);
	TEST_ASSERT_EQUAL_UINT32(0, event);
}

/*==============================================================================
 * TRANSACTION / STATE MACHINE tests
 *============================================================================*/

void test_state_machine_default_case_sets_spi_err_and_recovers(void)
{
	g_desc.spi_state = (enum oa_tc6_spi_state)0xFF;
	capi_irq_enable_IgnoreAndReturn(0);
	int ret = oa_tc6_state_machine(&g_desc);
	TEST_ASSERT_EQUAL_INT(-EPROTO, ret);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_READY, g_desc.spi_state);
	TEST_ASSERT_EQUAL_UINT32(1, g_desc.spi_err);
}

void test_state_machine_ctrl_start_dispatches_start_ctrl(void)
{
	g_desc.spi_state = OA_SPI_STATE_CTRL_START;
	g_desc.wnr = OA_SPI_READ;
	g_desc.cnt = 1;
	capi_spi_transceive_async_IgnoreAndReturn(0);
	calculate_parity_IgnoreAndReturn(1);

	int ret = oa_tc6_state_machine(&g_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_CTRL_END, g_desc.spi_state);
}

/* ------ oa_tc6_start_ctrl_transaction ----------------------------------------
 * Builds ctrl_tx_buf via ctrl_setup, transitions to CTRL_END, fires SPI. */
void test_start_ctrl_transaction_transitions_to_ctrl_end(void)
{
	g_desc.wnr = OA_SPI_READ;
	g_desc.cnt = 1;
	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_async_ExpectAnyArgsAndReturn(0);

	int ret = oa_tc6_start_ctrl_transaction(&g_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_CTRL_END, g_desc.spi_state);
}

void test_start_ctrl_transaction_propagates_spi_error(void)
{
	g_desc.wnr = OA_SPI_READ;
	g_desc.cnt = 1;
	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_async_ExpectAnyArgsAndReturn(-EIO);

	int ret = oa_tc6_start_ctrl_transaction(&g_desc);
	TEST_ASSERT_EQUAL_INT(-EIO, ret);
}

/* ------ oa_tc6_end_ctrl_transaction ------------------------------------------
 * Layout: cHdr = ctrl_tx_buf[0..3] must equal eHdr = ctrl_rx_buf[4..7],
 * register value read from ctrl_rx_buf[8..11] (2 * OA_HEADER_SIZE). */
void test_end_ctrl_transaction_transitions_to_ready_after_read(void)
{
	uint32_t val = 0;
	g_desc.wnr = OA_SPI_READ;
	g_desc.cnt = 1;
	g_desc.reg_data = &val;
	g_desc.prote_spi = false;
	*(uint32_t *)&g_desc.ctrl_tx_buf[0] = 0xAABBCCDD;
	*(uint32_t *)&g_desc.ctrl_rx_buf[OA_HEADER_SIZE] = 0xAABBCCDD;
	no_os_put_unaligned_be32(0xC0FFEE00, &g_desc.ctrl_rx_buf[2 * OA_HEADER_SIZE]);
	capi_irq_enable_IgnoreAndReturn(0);

	oa_tc6_end_ctrl_transaction(&g_desc);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_READY, g_desc.spi_state);
	TEST_ASSERT_EQUAL_HEX32(0xC0FFEE00, val);
}

void test_end_ctrl_transaction_bad_header(void)
{
	uint32_t val = 0;
	g_desc.wnr = OA_SPI_READ;
	g_desc.cnt = 1;
	g_desc.reg_data = &val;
	g_desc.prote_spi = false;
	*(uint32_t *)&g_desc.ctrl_tx_buf[0] = 0xAABBCCDD;
	*(uint32_t *)&g_desc.ctrl_rx_buf[OA_HEADER_SIZE] = 0xDDAABBCC;
	no_os_put_unaligned_be32(0xC0FFEE00, &g_desc.ctrl_rx_buf[2 * OA_HEADER_SIZE]);
	capi_irq_enable_IgnoreAndReturn(0);

	oa_tc6_end_ctrl_transaction(&g_desc);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_READY, g_desc.spi_state);
        TEST_ASSERT_EQUAL_UINT(1, g_desc.spi_err);
}

/* ------ oa_tc6_start_data_transaction ----------------------------------------
 * Builds batch, transitions to DATA_END, fires SPI.
 * Set oa_rca=1 to force at least one chunk to be built. */
void test_start_data_transaction_transitions_to_data_end(void)
{
	net_queue_is_empty_IgnoreAndReturn(true);
	net_queue_is_full_IgnoreAndReturn(false);
	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);
	capi_irq_enable_IgnoreAndReturn(0);
	g_desc.oa_rca = 1;
	g_desc.blocking = true;
	oa_tc6_start_data_transaction(&g_desc);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_DATA_END, g_desc.spi_state);
}

/* ------ oa_tc6_start_data_transaction (no chunks path) ----------------------
 * With oa_rca=0 and tx_queue empty, spi_process returns ret=0 and leaves
 * oa_trx_size=0. This forces the else branch: spi_state -> READY, and since
 * pending_ctrl is false, capi_irq_enable is called with desc->eth_irq. */
void test_start_data_transaction_invalid_trx_size(void)
{
	net_queue_is_empty_IgnoreAndReturn(true);
	net_queue_is_full_IgnoreAndReturn(false);
	g_desc.oa_rca = 0;
	g_desc.blocking = true;
	g_pending_ctrl = false;

	capi_irq_enable_ExpectAndReturn(g_desc.eth_irq, 0);

	oa_tc6_start_data_transaction(&g_desc);

	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_READY, g_desc.spi_state);
	TEST_ASSERT_EQUAL_UINT32(0, g_desc.oa_trx_size);
}

/* ------ oa_tc6_start_irq -----------------------------------------------------
 * Builds single-chunk IRQ header (DNC=1, NORX=1), dispatches SPI, DATA_END. */
void test_start_irq_moves_state_to_data_end(void)
{
	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);
	int ret = oa_tc6_start_irq(&g_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_DATA_END, g_desc.spi_state);
	TEST_ASSERT_EQUAL_UINT32(OA_HEADER_SIZE + (1u << OA_DEFAULT_CPS),
	                         g_desc.oa_trx_size);
}

void test_start_irq_propagates_spi_error(void)
{
	calculate_parity_IgnoreAndReturn(1);
	g_desc.blocking = false;
	capi_spi_transceive_dma_async_ExpectAnyArgsAndReturn(-EIO);
	int ret = oa_tc6_start_irq(&g_desc);
	TEST_ASSERT_EQUAL_INT(-EIO, ret);
}

/*==============================================================================
 * CHUNK / RX PROCESSOR tests
 *   Most require eth_frame_struct scaffolding; kept as TEST_IGNORE placeholders.
 *============================================================================*/

/* ------ oa_tc6_fcs_checker ---------------------------------------------------
 * fcs_size <= FCS_SIZE forces the "else" branch that flags MAC_CALLBACK_STATUS_FCS_ERROR
 * and zeroes rx_buf[trx_size]. */
void test_fcs_checker_size_le_fcs_size_flags_error(void)
{
	uint32_t event = 0;
	g_rx_buf_desc.trx_size = 0;
	g_rx_frame_buf[0] = 0xAB;
	oa_tc6_fcs_checker(&g_desc, 2, g_rx_frame_buf, &event);
	TEST_ASSERT_TRUE(event & MAC_CALLBACK_STATUS_FCS_ERROR);
	TEST_ASSERT_EQUAL_HEX8(0, g_rx_frame_buf[0]);
}

/* ------ oa_tc6_timestamp_handler ---------------------------------------------
 * Default (unknown enum) returns early without touching frame_dest / byte_offset. */
void test_timestamp_handler_default_case_returns_early(void)
{
	uint32_t frame_dest = 0xAACCDDEEu;
	uint8_t  ts_bytes[8] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22 };
	uint8_t  ts_bytes_orig[8];
	uint32_t byte_offset = 5;
	memcpy(ts_bytes_orig, ts_bytes, sizeof(ts_bytes));

	oa_tc6_timestamp_handler(&g_desc, 0, &frame_dest, ts_bytes, &byte_offset,
				 (enum oa_tc6_ts_operation)99);

	TEST_ASSERT_EQUAL_HEX32(0xAACCDDEEu, frame_dest);
	TEST_ASSERT_EQUAL_UINT32(5, byte_offset);
	TEST_ASSERT_EQUAL_MEMORY(ts_bytes_orig, ts_bytes, sizeof(ts_bytes));
}

/* OA_TS_UPPER_32_BIT: stores 4 bytes into ts_bytes[UPPER_OFFSET..], no parity call. */
void test_timestamp_handler_upper_32_bit(void)
{
	uint32_t frame_dest = 0;
	uint8_t  ts_bytes[8] = {0};
	uint32_t byte_offset = 0;

	memset(spi_rx_buf, 0, 8);
	spi_rx_buf[0] = 0x11; spi_rx_buf[1] = 0x22;
	spi_rx_buf[2] = 0x33; spi_rx_buf[3] = 0x44;

	oa_tc6_timestamp_handler(&g_desc, 0, &frame_dest, ts_bytes, &byte_offset,
				 OA_TS_UPPER_32_BIT);

	TEST_ASSERT_EQUAL_HEX32(0x11223344u, frame_dest);
	TEST_ASSERT_EQUAL_UINT32(OA_TIMESTAMP_32BIT_SIZE, byte_offset);
	TEST_ASSERT_EQUAL_HEX8(0x11, ts_bytes[OA_TIMESTAMP_UPPER_OFFSET + 0]);
	TEST_ASSERT_EQUAL_HEX8(0x44, ts_bytes[OA_TIMESTAMP_UPPER_OFFSET + 3]);
	TEST_ASSERT_EQUAL_HEX8(0x00, ts_bytes[0]);
}

/* OA_TS_LOWER_32_BIT: writes into ts_bytes[0..3], no parity call (is_calculate_parity=false). */
void test_timestamp_handler_lower_32_bit(void)
{
	uint32_t frame_dest = 0;
	uint8_t  ts_bytes[8] = {0};
	uint32_t byte_offset = 2;

	memset(spi_rx_buf, 0, 8);
	spi_rx_buf[0] = 0xAA; spi_rx_buf[1] = 0xBB;
	spi_rx_buf[2] = 0xCC; spi_rx_buf[3] = 0xDD;

	oa_tc6_timestamp_handler(&g_desc, 0, &frame_dest, ts_bytes, &byte_offset,
				 OA_TS_LOWER_32_BIT);

	TEST_ASSERT_EQUAL_HEX32(0xAABBCCDDu, frame_dest);
	TEST_ASSERT_EQUAL_UINT32(2 + OA_TIMESTAMP_32BIT_SIZE, byte_offset);
	TEST_ASSERT_EQUAL_HEX8(0xAA, ts_bytes[0]);
	TEST_ASSERT_EQUAL_HEX8(0xDD, ts_bytes[3]);
	TEST_ASSERT_EQUAL_HEX8(0x00, ts_bytes[OA_TIMESTAMP_UPPER_OFFSET]);
}

/* OA_TS_32_BIT: is_calculate_parity=true -> calculate_parity called; timestamp_valid mismatch=true. */
void test_timestamp_handler_32_bit_parity_mismatch_valid(void)
{
	uint32_t frame_dest = 0;
	uint8_t  ts_bytes[8] = {0};
	uint32_t byte_offset = 0;

	memset(spi_rx_buf, 0, 4);
	spi_rx_buf[0] = 0x39; spi_rx_buf[1] = 0x71;
	spi_rx_buf[2] = 0x4C; spi_rx_buf[3] = 0x82;

	g_desc.oa_timestamp_parity = 0;
	g_rx_buf_desc.timestamp_valid = false;

	calculate_parity_ExpectAnyArgsAndReturn(1);

	oa_tc6_timestamp_handler(&g_desc, 0, &frame_dest, ts_bytes, &byte_offset,
				 OA_TS_32_BIT);

	TEST_ASSERT_EQUAL_HEX32(0x39714C82u, frame_dest);
	TEST_ASSERT_EQUAL_UINT32(OA_TIMESTAMP_32BIT_SIZE, byte_offset);
	TEST_ASSERT_TRUE(g_rx_buf_desc.timestamp_valid);
}

/* OA_TS_32_BIT: parity matches -> timestamp_valid=false. */
void test_timestamp_handler_32_bit_parity_match_invalid(void)
{
	uint32_t frame_dest = 0;
	uint8_t  ts_bytes[8] = {0};
	uint32_t byte_offset = 0;

	memset(spi_rx_buf, 0, 4);
	spi_rx_buf[0] = 0x01; spi_rx_buf[1] = 0x02;
	spi_rx_buf[2] = 0x03; spi_rx_buf[3] = 0x04;

	g_desc.oa_timestamp_parity = 1;
	g_rx_buf_desc.timestamp_valid = true;

	calculate_parity_ExpectAnyArgsAndReturn(1);

	oa_tc6_timestamp_handler(&g_desc, 0, &frame_dest, ts_bytes, &byte_offset,
				 OA_TS_32_BIT);

	TEST_ASSERT_EQUAL_HEX32(0x01020304u, frame_dest);
	TEST_ASSERT_EQUAL_UINT32(OA_TIMESTAMP_32BIT_SIZE, byte_offset);
	TEST_ASSERT_FALSE(g_rx_buf_desc.timestamp_valid);
}

/* ------ oa_tc6_end_data_chunk_processor --------------------------------------
 * SV=0 + prior START -> commits frame, calls FCS check, advances valid_flag to END. */
void test_end_data_chunk_processor_full_flow(void)
{
	uint32_t event = 0;
	uint32_t oa_rx_footer = 0;
	oa_rx_footer |= no_os_field_prep(OA_DATA_FOOTER_EBO_MASK, 63u);
	/* SV=0, FD=0, SWO=0 */
	g_desc.oa_valid_flag = OA_VALID_FLAG_START;
	g_desc.oa_rx_cur_buf_byte_offset = 0;
	g_desc.fcs_check_en = true;

	calculate_fcs_IgnoreAndReturn(0);
	net_queue_remove_entry_Ignore();

	oa_tc6_end_data_chunk_processor(&g_desc, &event, g_rx_frame_buf,
					oa_rx_footer, 0);

	TEST_ASSERT_EQUAL_UINT(OA_VALID_FLAG_END, g_desc.oa_valid_flag);
	TEST_ASSERT_EQUAL_UINT32(64, g_rx_buf_desc.trx_size);
}

/* ------ oa_tc6_start_data_chunk_processor ------------------------------------
 * SV=1, EV=0, RTSA=0 -> takes the "start new frame" path and sets valid_flag=START. */
void test_start_data_chunk_processor_new_frame(void)
{
	uint32_t event = 0;
	uint32_t byte_offset = 0;
	uint8_t  ts_bytes[8] = {0};
	uint32_t oa_rx_footer = no_os_field_prep(OA_DATA_FOOTER_SV_MASK, 1u);

	g_desc.oa_valid_flag = OA_VALID_FLAG_END;
	g_desc.oa_rx_use_backup_buf = false;
	g_desc.rx_queue_hp_en = false;
	g_desc.num_ports = 1;

	net_queue_is_empty_IgnoreAndReturn(false);

	oa_tc6_start_data_chunk_processor(&g_desc, &event, &byte_offset, 0,
					  ts_bytes, 64, g_rx_frame_buf,
					  oa_rx_footer);

	TEST_ASSERT_EQUAL_UINT(OA_VALID_FLAG_START, g_desc.oa_valid_flag);
	TEST_ASSERT_EQUAL_UINT32(64, g_desc.oa_rx_cur_buf_byte_offset);
}

/* ------ oa_tc6_mid_data_chunk_processor --------------------------------------
 * EV=0 + valid_flag=START + fits in buffer -> copies chunk bytes and advances offset. */
void test_mid_data_chunk_processor_mid_frame(void)
{
	uint32_t event = 0;
	uint32_t oa_rx_footer = 0;   /* EV=0 */
	g_desc.oa_valid_flag = OA_VALID_FLAG_START;
	g_desc.oa_rx_cur_buf_byte_offset = 0;

	memset(spi_rx_buf, 0xAB, 64);
	memset(g_rx_frame_buf, 0, sizeof(g_rx_frame_buf));

	oa_tc6_mid_data_chunk_processor(&g_desc, 64, oa_rx_footer,
					g_rx_frame_buf, &event, 0, 0);

	TEST_ASSERT_EQUAL_UINT32(64, g_desc.oa_rx_cur_buf_byte_offset);
	TEST_ASSERT_EQUAL_HEX8(0xAB, g_rx_frame_buf[0]);
	TEST_ASSERT_EQUAL_HEX8(0xAB, g_rx_frame_buf[63]);
}

/* ------ oa_tc6_full_frame_in_chunk_process -----------------------------------
 * fcs_size == FCS_SIZE -> "else" branch: flags FCS_ERROR, sets valid_flag=END. */
void test_full_frame_in_chunk_process(void)
{
	uint32_t event = 0;
	g_rx_buf_desc.trx_size = 0;

	net_queue_remove_entry_Ignore();

	oa_tc6_full_frame_in_chunk_process(&g_desc, &event, FCS_SIZE,
					   g_rx_frame_buf);

	TEST_ASSERT_TRUE(event & MAC_CALLBACK_STATUS_FCS_ERROR);
	TEST_ASSERT_EQUAL_UINT(OA_VALID_FLAG_END, g_desc.oa_valid_flag);
	TEST_ASSERT_EQUAL_UINT32(FCS_SIZE, g_rx_buf_desc.trx_size);
}

/* ------ oa_tc6_process_64_bit_timestamp --------------------------------------
 * Chunk big enough for both halves -> oa_timestamp_split=false, byte_offset advanced by 8. */
void test_process_64_bit_timestamp(void)
{
	uint32_t byte_offset = 0;
	uint8_t  ts_bytes[16] = {0};
	uint32_t chunk_start = 0;
	uint32_t chunk_size = 64;

	g_desc.oa_timestamp_parity = 0;
	calculate_parity_IgnoreAndReturn(0);

	oa_tc6_process_64_bit_timestamp(&g_desc, chunk_start, chunk_size,
					ts_bytes, &byte_offset);

	TEST_ASSERT_FALSE(g_desc.oa_timestamp_split);
	TEST_ASSERT_EQUAL_UINT32(2u * OA_TIMESTAMP_32BIT_SIZE, byte_offset);
}

/* ------ oa_tc6_cb_function_caller --------------------------------------------
 * num_ports=2 -> invokes both cb_func[MAC_EVT_DYN_TBL_UPDATE] and buf_desc->cb_func. */
void test_cb_function_caller_num_ports_2(void)
{
	g_cb_dyn_calls = 0;
	g_cb_buf_calls = 0;
	g_cb_funcs[MAC_EVT_DYN_TBL_UPDATE] = g_cb_dyn_tbl_stub;
	g_rx_buf_desc.cb_func = g_cb_buf_stub;

	oa_tc6_cb_function_caller(&g_desc, 0, 0, 2);

	TEST_ASSERT_EQUAL_UINT32(1, g_cb_dyn_calls);
	TEST_ASSERT_EQUAL_UINT32(1, g_cb_buf_calls);
	TEST_ASSERT_EQUAL_UINT32(0, g_desc.oa_rx_cur_buf_byte_offset);
}

/* ------ oa_tc6_create_next_chunk ---------------------------------------------
 * No pending TX, empty RX -> tx_header DNC=1, NORX=1, DV=0. */
void test_create_next_chunk_no_tx_no_rx_sets_norx(void)
{
	uint8_t buf[OA_HEADER_SIZE + 64] = {0};
	net_queue_is_empty_ExpectAndReturn(g_rx_queue_ptr, true);
	calculate_parity_IgnoreAndReturn(1);

	oa_tc6_create_next_chunk(&g_desc, buf, false);
	uint32_t hdr = no_os_get_unaligned_be32(buf);
	TEST_ASSERT_EQUAL_UINT32(1, no_os_field_get(OA_DATA_HEADER_DNC_MASK, hdr));
	TEST_ASSERT_EQUAL_UINT32(1, no_os_field_get(OA_DATA_HEADER_NORX_MASK, hdr));
	TEST_ASSERT_EQUAL_UINT32(0, no_os_field_get(OA_DATA_HEADER_DV_MASK, hdr));
}

/* ------ oa_tc6_spi_process ---------------------------------------------------
 * Batches N chunks up to txc/rca/max_chunk_count. */
void test_spi_process_batches_chunks(void)
{
	net_queue_is_empty_IgnoreAndReturn(true);
	net_queue_is_full_IgnoreAndReturn(false);
	calculate_parity_IgnoreAndReturn(1);
	g_desc.oa_rca = 1;
	g_desc.oa_txc = 0;
	oa_tc6_spi_process(&g_desc);
	TEST_ASSERT_TRUE(g_desc.oa_trx_size >= OA_HEADER_SIZE + (1u << OA_DEFAULT_CPS));
}

/* ------ oa_tc6_end_data_transaction ------------------------------------------
 * NOTE: this driver reads rx_footer AFTER the chunk loop even when oa_trx_size=0,
 * which is UB. To keep the test deterministic we run one full chunk with a
 * well-formed footer (SYNC=1, DV=0, EXST=0) so rx_footer is initialized and
 * both queue-check branches fall through to the READY state. */
void test_end_data_transaction_zero_trx_size_returns_success(void)
{
	uint32_t chunk_size = (1u << OA_DEFAULT_CPS);
	uint32_t footer = no_os_field_prep(OA_DATA_FOOTER_SYNC_MASK, 1);
	memset(spi_rx_buf, 0, chunk_size + OA_HEADER_SIZE);
	memset(spi_tx_buf, 0, chunk_size + OA_HEADER_SIZE);
	no_os_put_unaligned_be32(footer, &spi_rx_buf[chunk_size]);

	g_desc.oa_trx_size = OA_HEADER_SIZE + chunk_size;
	g_desc.oa_txc = 0;
	g_desc.oa_rca = 0;
	net_queue_is_empty_IgnoreAndReturn(true);
	capi_irq_enable_IgnoreAndReturn(0);
	calculate_parity_IgnoreAndReturn(1);

	int ret = oa_tc6_end_data_transaction(&g_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_READY, g_desc.spi_state);
}

/*==============================================================================
 * PHY / STATUS tests
 *============================================================================*/

/* ------ oa_tc6_read_status --------------------------------------------------- */
void test_read_status_dispatches_ctrl_transfer(void)
{
	uint32_t mdio_cmd = 0;
	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);
	oa_tc6_read_status(&g_desc, &mdio_cmd);
	TEST_ASSERT_NOT_EQUAL(OA_SPI_STATE_READY, g_desc.spi_state);
}

/* ------ oa_tc6_phy_reg_read_start -------------------------------------------- */
void test_phy_reg_read_start_builds_mdio_cmd(void)
{
	uint32_t mdio_cmd = 0;
	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);
	oa_tc6_phy_reg_read_start(&g_desc, &mdio_cmd, PHY_PORT_1_ADDRESS, 0x1E0000);
	TEST_ASSERT_EQUAL_UINT32(PHY_PORT_1_ADDRESS,
	                         no_os_field_get(MDIO_PRTAD_MASK, mdio_cmd));
}

/* ------ oa_tc6_phy_reg_read_step -------------------------------------------- */
void test_phy_reg_read_step_progresses_state(void)
{
	uint32_t mdio_cmd = 0;
	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);
	oa_tc6_phy_reg_read_step(&g_desc, &mdio_cmd);
	TEST_ASSERT_NOT_EQUAL(OA_SPI_STATE_READY, g_desc.spi_state);
}

/* ------ oa_tc6_record_port_status -------------------------------------------
 * With num_ports=1, record_port_status falls through to oa_tc6_spi_int_handle
 * which requires ctrl_setup (calculate_parity) + SPI transceive mocks and
 * transitions state to DATA_START. */
void test_record_port_status_updates_desc(void)
{
	uint32_t mdio_cmd = 0;
	mdio_cmd |= no_os_field_prep(MDIO_DATA_MASK, 0xABCD);
	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);

	oa_tc6_record_port_status(&g_desc, &mdio_cmd, 1);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_DATA_START, g_desc.spi_state);
}

/* ------ oa_tc6_read_phy_reg ------------------------------------------------- */
void test_read_phy_reg_dispatches_spi(void)
{
	uint32_t mdio_cmd = 0;
	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);
	oa_tc6_read_phy_reg(&g_desc, &mdio_cmd);
	TEST_ASSERT_NOT_EQUAL(OA_SPI_STATE_READY, g_desc.spi_state);
}

/* ------ oa_tc6_spi_int_handle ----------------------------------------------- */
void test_spi_int_handle_ready_returns_without_crash(void)
{
	g_desc.spi_state = OA_SPI_STATE_READ_STATUS;
	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);
	capi_irq_enable_IgnoreAndReturn(0);
	net_queue_is_empty_IgnoreAndReturn(true);
	oa_tc6_spi_int_handle(&g_desc);
	TEST_PASS();
}

/*==============================================================================
 *   These target the code paths gated on `desc->num_ports == 2` that are not
 *   otherwise exercised by the num_ports=1 tests above.
 *============================================================================*/

/* ------ oa_tc6_create_next_chunk (num_ports=2 path) --------------------------
 * When num_ports=2 and a Tx frame is present, VS in the data header is loaded
 * from frame->buf_desc->port. */
void test_create_next_chunk_num_ports_2_sets_vs_from_port(void)
{
	uint8_t buf[OA_HEADER_SIZE + 64] = {0};

	g_desc.num_ports = 2;
	g_desc.oa_tx_cur_buf_byte_offset = 0;
	g_desc.oa_tx_cur_buf_idx = 0;
	g_tx_queue_storage.head = 0;
	g_tx_queue_storage.tail = 0;
	g_tx_buf_desc.port = 1;
	g_tx_buf_desc.trx_size = 32;

	net_queue_is_empty_IgnoreAndReturn(true);
	net_queue_is_full_IgnoreAndReturn(true);
	calculate_parity_IgnoreAndReturn(1);

	oa_tc6_create_next_chunk(&g_desc, buf, true);

	uint32_t hdr = no_os_get_unaligned_be32(buf);
	TEST_ASSERT_EQUAL_UINT32(1, no_os_field_get(OA_DATA_HEADER_DV_MASK, hdr));
	TEST_ASSERT_EQUAL_UINT32(1, no_os_field_get(OA_DATA_HEADER_VS_MASK, hdr));
}

/* ------ oa_tc6_start_data_chunk_processor (num_ports=2 path) -----------------
 * When num_ports=2 and SV=1 with VS bit0=1, buf_desc->port is captured from VS. */
void test_start_data_chunk_processor_num_ports_2_sets_port(void)
{
	uint32_t event = 0;
	uint32_t byte_offset = 0;
	uint8_t  ts_bytes[8] = {0};
	uint32_t oa_rx_footer = 0;
	oa_rx_footer |= no_os_field_prep(OA_DATA_FOOTER_SV_MASK, 1u);
	oa_rx_footer |= no_os_field_prep(OA_DATA_FOOTER_VS_MASK, 1u); /* bit0 = 1 */

	g_desc.num_ports = 2;
	g_desc.oa_valid_flag = OA_VALID_FLAG_END;
	g_desc.oa_rx_use_backup_buf = false;
	g_desc.rx_queue_hp_en = false;
	g_rx_buf_desc.port = 0xFF;
	net_queue_is_empty_IgnoreAndReturn(false);

	oa_tc6_start_data_chunk_processor(&g_desc, &event, &byte_offset, 0,
					  ts_bytes, 64, g_rx_frame_buf,
					  oa_rx_footer);

	TEST_ASSERT_EQUAL_UINT32(1, g_rx_buf_desc.port);
	TEST_ASSERT_EQUAL_UINT(OA_VALID_FLAG_START, g_desc.oa_valid_flag);
}

/* ------ oa_tc6_record_port_status (num_ports=2, port_number=2 branch) --------
 * With num_ports=2 and port_number=2, function selects p2_status registers.
 * With p2_status_masked seeded to OA_PHY_STATUS_INIT_VAL, the CRSM branch runs
 * and copies reg_data.lower (from *desc->reg_data) into p2_status.lower. */
void test_record_port_status_num_ports_2_port_2(void)
{
	uint32_t mdio_cmd = 0;

	g_desc.num_ports = 2;
	g_reg_data = 0x0000ABCDu;
	g_status_regs.p2_status = 0;
	g_status_regs.p2_status_masked = OA_PHY_STATUS_INIT_VAL;

	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);

	oa_tc6_record_port_status(&g_desc, &mdio_cmd, 2);

	TEST_ASSERT_EQUAL_HEX16(0xABCDu, (uint16_t)(g_status_regs.p2_status & 0xFFFFu));
	TEST_ASSERT_NOT_EQUAL(OA_SPI_STATE_READY, g_desc.spi_state);
}

/* ------ oa_tc6_record_port_status (num_ports=1, port_number=2 -> early ret) -- */
void test_record_port_status_num_ports_1_port_2_returns_early(void)
{
	uint32_t mdio_cmd = 0;

	g_desc.num_ports = 1;
	g_desc.spi_state = OA_SPI_STATE_READY;
	g_status_regs.p2_status =  0xAACCDDEEu;

	oa_tc6_record_port_status(&g_desc, &mdio_cmd, 2);

	/* Early return: state unchanged, p2 registers untouched. */
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_READY, g_desc.spi_state);
	TEST_ASSERT_EQUAL_HEX32( 0xAACCDDEEu, g_status_regs.p2_status);
}

/* ------ oa_tc6_read_status (num_ports=2 path) --------------------------------
 * With num_ports=2 the p2_status/p2_status_masked registers are initialized
 * to OA_PHY_STATUS_INIT_VAL, which the num_ports=1 path does not touch. */
void test_read_status_num_ports_2_inits_p2_registers(void)
{
	uint32_t mdio_cmd = 0;

	g_desc.num_ports = 2;
	g_status_regs.p2_status = 0;
	g_status_regs.p2_status_masked = 0;

	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);
	capi_irq_enable_IgnoreAndReturn(0);
	net_queue_is_empty_IgnoreAndReturn(true);

	oa_tc6_read_status(&g_desc, &mdio_cmd);

	TEST_ASSERT_EQUAL_HEX32(OA_PHY_STATUS_INIT_VAL, g_status_regs.p2_status);
	TEST_ASSERT_EQUAL_HEX32(OA_PHY_STATUS_INIT_VAL, g_status_regs.p2_status_masked);
}

/* ------ oa_tc6_cb_function_caller (num_ports=1 path skips dyn tbl callback) -- */
void test_cb_function_caller_num_ports_1_skips_dyn_tbl(void)
{
	g_cb_dyn_calls = 0;
	g_cb_buf_calls = 0;
	g_cb_funcs[MAC_EVT_DYN_TBL_UPDATE] = g_cb_dyn_tbl_stub;
	g_rx_buf_desc.cb_func = g_cb_buf_stub;

	oa_tc6_cb_function_caller(&g_desc, 0, 0, 1);

	TEST_ASSERT_EQUAL_UINT32(0, g_cb_dyn_calls); /* skipped when num_ports != 2 */
	TEST_ASSERT_EQUAL_UINT32(1, g_cb_buf_calls);
}

/* ------ oa_tc6_cb_function_caller: num_ports=2 with DYN_TBL cb NULL -------- */
void test_cb_function_caller_num_ports_2_dyn_cb_null(void)
{
	g_cb_dyn_calls = 0;
	g_cb_buf_calls = 0;
	g_cb_funcs[MAC_EVT_DYN_TBL_UPDATE] = NULL;
	g_rx_buf_desc.cb_func = g_cb_buf_stub;

	oa_tc6_cb_function_caller(&g_desc, 0, 0, 2);

	TEST_ASSERT_EQUAL_UINT32(0, g_cb_dyn_calls);
	TEST_ASSERT_EQUAL_UINT32(1, g_cb_buf_calls);
}

/* ------ oa_tc6_cb_function_caller: buf_desc->cb_func NULL ------------------ */
void test_cb_function_caller_buf_cb_null(void)
{
	g_cb_dyn_calls = 0;
	g_cb_buf_calls = 0;
	g_cb_funcs[MAC_EVT_DYN_TBL_UPDATE] = g_cb_dyn_tbl_stub;
	g_rx_buf_desc.cb_func = NULL;

	oa_tc6_cb_function_caller(&g_desc, 0, 0, 2);

	TEST_ASSERT_EQUAL_UINT32(1, g_cb_dyn_calls);
	TEST_ASSERT_EQUAL_UINT32(0, g_cb_buf_calls);
}

/* ------ oa_tc6_fcs_checker: size > FCS, fcs_check_en=false ---------------- */
void test_fcs_checker_size_gt_fcs_size_check_disabled(void)
{
	uint32_t event = 0;
	g_rx_buf_desc.trx_size = 4;
	g_desc.fcs_check_en = false;
	g_rx_frame_buf[4] = 0x50;

	oa_tc6_fcs_checker(&g_desc, FCS_SIZE + 1, g_rx_frame_buf, &event);

	TEST_ASSERT_FALSE(event & MAC_CALLBACK_STATUS_FCS_ERROR);
	TEST_ASSERT_EQUAL_HEX8((uint8_t)(0x50u - FCS_SIZE), g_rx_frame_buf[4]);
}

/* ------ oa_tc6_fcs_checker: size > FCS, fcs_check_en=true, FCS matches --- */
void test_fcs_checker_fcs_matches_no_error(void)
{
	uint32_t event = 0;
	g_rx_buf_desc.trx_size = 4;
	g_desc.fcs_check_en = true;
	g_rx_frame_buf[4] = 0x50 + FCS_SIZE; /* after decrement -> 0x50 */
	g_rx_frame_buf[5] = 0x71;
	g_rx_frame_buf[6] = 0x2C;
	g_rx_frame_buf[7] = 0x83;

	/* mimic the memcpy: little-endian on host */
	uint32_t actual = (uint32_t)0x50u
			| ((uint32_t)0x71u << 8)
			| ((uint32_t)0x2Cu << 16)
			| ((uint32_t)0x83u << 24);
	calculate_fcs_IgnoreAndReturn(actual);

	oa_tc6_fcs_checker(&g_desc, FCS_SIZE + 1, g_rx_frame_buf, &event);

	TEST_ASSERT_FALSE(event & MAC_CALLBACK_STATUS_FCS_ERROR);
}

/* ------ oa_tc6_fcs_checker: size > FCS, fcs_check_en=true, mismatch ----- */
void test_fcs_checker_fcs_mismatch_flags_error(void)
{
	uint32_t event = 0;
	g_rx_buf_desc.trx_size = 4;
	g_desc.fcs_check_en = true;
	g_rx_frame_buf[4] = 0x50;

	calculate_fcs_IgnoreAndReturn(0x27615394u);

	oa_tc6_fcs_checker(&g_desc, FCS_SIZE + 1, g_rx_frame_buf, &event);

	TEST_ASSERT_TRUE(event & MAC_CALLBACK_STATUS_FCS_ERROR);
}

/* ------ oa_tc6_complete_transmission_checker: EV=1, cb set, ref_count>1 - */
void test_complete_transmission_checker_ev_ref_count_gt_1_no_cb(void)
{
	uint32_t event = 0;
	uint32_t tx_header = no_os_field_prep(OA_DATA_HEADER_EV_MASK, 1);
	g_cb_buf_calls = 0;
	g_tx_buf_desc.cb_func = g_cb_buf_stub;
	g_tx_buf_desc.ref_count = 2;

	net_queue_remove_entry_Ignore();

	oa_tc6_complete_transmission_checker(&g_desc, tx_header, &event);

	TEST_ASSERT_EQUAL_UINT32(0, g_cb_buf_calls);
	TEST_ASSERT_EQUAL_UINT32(1, g_tx_buf_desc.ref_count);
}

/* ------ oa_tc6_complete_transmission_checker: EV=1, cb set, ref_count==1 - */
void test_complete_transmission_checker_ev_ref_count_1_fires_cb(void)
{
	uint32_t event = 0;
	uint32_t tx_header = no_os_field_prep(OA_DATA_HEADER_EV_MASK, 1);
	g_cb_buf_calls = 0;
	g_tx_buf_desc.cb_func = g_cb_buf_stub;
	g_tx_buf_desc.ref_count = 1;

	net_queue_remove_entry_Ignore();

	oa_tc6_complete_transmission_checker(&g_desc, tx_header, &event);

	TEST_ASSERT_EQUAL_UINT32(1, g_cb_buf_calls);
	TEST_ASSERT_EQUAL_UINT32(0, g_tx_buf_desc.ref_count);
}

/* ------ oa_tc6_complete_transmission_checker: EV=1, cb NULL --------------- */
void test_complete_transmission_checker_ev_cb_null(void)
{
	uint32_t event = 0;
	uint32_t tx_header = no_os_field_prep(OA_DATA_HEADER_EV_MASK, 1);
	g_tx_buf_desc.cb_func = NULL;
	g_tx_buf_desc.ref_count = 1;

	net_queue_remove_entry_Ignore();

	oa_tc6_complete_transmission_checker(&g_desc, tx_header, &event);

	TEST_ASSERT_EQUAL_UINT32(0, g_tx_buf_desc.ref_count);
}

/* ------ oa_tc6_end_ctrl_transaction: headers match, wnr=WRITE (skip read) - */
void test_end_ctrl_transaction_write_skips_read_data(void)
{
	uint32_t val = 0x27615394u;
	g_desc.wnr = OA_SPI_WRITE;
	g_desc.cnt = 1;
	g_desc.reg_data = &val;
	g_desc.prote_spi = false;
	*(uint32_t *)&g_desc.ctrl_tx_buf[0] = 0x39714C82u;
	*(uint32_t *)&g_desc.ctrl_rx_buf[OA_HEADER_SIZE] = 0x39714C82u;

	oa_tc6_end_ctrl_transaction(&g_desc);

	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_READY, g_desc.spi_state);
	TEST_ASSERT_EQUAL_UINT(0, g_desc.spi_err);
	TEST_ASSERT_EQUAL_HEX32(0x27615394u, val);
}

/* ------ oa_tc6_start_data_transaction: non-blocking, size >= MIN_SIZE_FOR_DMA (DMA) */
void test_start_data_transaction_dma_path(void)
{
	g_desc.blocking = false;
	g_desc.oa_rca = 1;
	g_desc.oa_txc = 0;
	g_desc.oa_max_chunk_count = 1;
	g_desc.oa_cps = OA_DEFAULT_CPS; /* chunk = 64 -> total > MIN_SIZE_FOR_DMA */
	g_pending_ctrl = false;

	net_queue_is_empty_IgnoreAndReturn(true);
	net_queue_is_full_IgnoreAndReturn(false);
	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_dma_async_ExpectAnyArgsAndReturn(0);

	int ret = oa_tc6_start_data_transaction(&g_desc);

	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_DATA_END, g_desc.spi_state);
}

/* ------ oa_tc6_start_data_transaction: non-blocking, size < MIN_SIZE_FOR_DMA (IT) */
void test_start_data_transaction_it_path(void)
{
	g_desc.blocking = false;
	g_desc.oa_rca = 1;
	g_desc.oa_txc = 0;
	g_desc.oa_max_chunk_count = 1;
	g_desc.oa_cps = 2; /* chunk = 4, +4 header = 8 bytes < 16 */
	g_pending_ctrl = false;

	net_queue_is_empty_IgnoreAndReturn(true);
	net_queue_is_full_IgnoreAndReturn(false);
	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_async_ExpectAnyArgsAndReturn(0);

	int ret = oa_tc6_start_data_transaction(&g_desc);

	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_DATA_END, g_desc.spi_state);
}

/* ------ oa_tc6_start_data_transaction: empty chunk path, pending_ctrl=true - */
void test_start_data_transaction_invalid_trx_size_pending_ctrl_true(void)
{
	net_queue_is_empty_IgnoreAndReturn(true);
	net_queue_is_full_IgnoreAndReturn(false);
	g_desc.oa_rca = 0;
	g_desc.blocking = true;
	g_pending_ctrl = true;
	/* capi_irq_enable must NOT be called; strict-ordering would fail if it was */

	oa_tc6_start_data_transaction(&g_desc);

	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_READY, g_desc.spi_state);
	TEST_ASSERT_EQUAL_UINT32(0, g_desc.oa_trx_size);
}

/* ------ oa_tc6_start_irq: blocking=true path -------------------------------- */
void test_start_irq_blocking_path(void)
{
	g_desc.blocking = true;
	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_ExpectAnyArgsAndReturn(0);
	int ret = oa_tc6_start_irq(&g_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_DATA_END, g_desc.spi_state);
}

/* ------ oa_tc6_start_irq: non-blocking, size < MIN_SIZE_FOR_DMA (IT path) --- */
void test_start_irq_it_path(void)
{
	g_desc.blocking = false;
	g_desc.oa_cps = 2; /* chunk = 4, + 4 header = 8 < 16 */
	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_async_ExpectAnyArgsAndReturn(0);
	int ret = oa_tc6_start_irq(&g_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
}

/* ------ oa_tc6_wait_get_status: is_ctrl=true, state=READY (returns 0) ------ */
void test_wait_get_status_ctrl_ready(void)
{
	uint8_t backup = 0;
	g_desc.spi_state = OA_SPI_STATE_READY;
	capi_irq_get_enable_ExpectAndReturn(g_desc.eth_irq, 1);
	capi_irq_disable_ExpectAndReturn(g_desc.eth_irq, 0);
	g_pending_ctrl = false;

	int ret = oa_tc6_wait_get_status(&g_desc, &backup, true);

	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_TRUE(g_pending_ctrl);
	TEST_ASSERT_EQUAL_UINT8(1, backup);
}

/* ------ oa_tc6_wait_get_status: is_ctrl=true, state!=READY (-ETIMEDOUT) ---- */
void test_wait_get_status_ctrl_not_ready_returns_etimedout(void)
{
	uint8_t backup = 0;
	g_desc.spi_state = OA_SPI_STATE_CTRL_END;
	capi_irq_disable_ExpectAndReturn(g_desc.eth_irq, 0);

	int ret = oa_tc6_wait_get_status(&g_desc, &backup, true);

	TEST_ASSERT_EQUAL_INT(-ETIMEDOUT, ret);
}

/* CTRL_END: matching headers, WRITE -> no ctrl_cmd_read_data; state -> READY */
void test_state_machine_ctrl_end_dispatch_write(void)
{
	uint8_t hdr[4] = {0x2B, 0x5D, 0xA3, 0xF1};
	memcpy(&g_desc.ctrl_tx_buf[0], hdr, 4);
	memcpy(&g_desc.ctrl_rx_buf[OA_HEADER_SIZE], hdr, 4);
	g_desc.wnr = OA_SPI_WRITE;
	g_desc.spi_state = OA_SPI_STATE_CTRL_END;

	int ret = oa_tc6_state_machine(&g_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_READY, g_desc.spi_state);
	TEST_ASSERT_EQUAL_UINT32(0, g_desc.spi_err);
}

/* CTRL_END: mismatching headers -> spi_err=1 -> state_machine recovery */
void test_state_machine_ctrl_end_dispatch_mismatch_recovers(void)
{
	uint8_t tx[4] = {0x2B, 0x5D, 0xA3, 0xF1};
	uint8_t rx[4] = {0x39, 0x71, 0x4C, 0x82};
	memcpy(&g_desc.ctrl_tx_buf[0], tx, 4);
	memcpy(&g_desc.ctrl_rx_buf[OA_HEADER_SIZE], rx, 4);
	g_desc.wnr = OA_SPI_WRITE;
	g_desc.spi_state = OA_SPI_STATE_CTRL_END;
	capi_irq_enable_IgnoreAndReturn(0);

	int ret = oa_tc6_state_machine(&g_desc);
	TEST_ASSERT_EQUAL_INT(-EPROTO, ret);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_READY, g_desc.spi_state);
	TEST_ASSERT_EQUAL_UINT32(1, g_desc.spi_err);
}

/* DATA_START dispatch: blocking xfer path via state_machine */
void test_state_machine_data_start_dispatch(void)
{
	g_desc.spi_state = OA_SPI_STATE_DATA_START;
	g_desc.blocking = true;
	g_desc.oa_txc = 1;
	g_desc.oa_rca = 1;
	g_desc.oa_cps = 2;
	g_pending_ctrl = false;
	calculate_parity_IgnoreAndReturn(1);
	net_queue_is_empty_IgnoreAndReturn(true);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);
	capi_irq_enable_IgnoreAndReturn(0);

	int ret = oa_tc6_state_machine(&g_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
}

/* IRQ_START dispatch: blocking xfer path via state_machine */
void test_state_machine_irq_start_dispatch(void)
{
	g_desc.spi_state = OA_SPI_STATE_IRQ_START;
	g_desc.blocking = true;
	g_desc.oa_cps = 2;
	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_ExpectAnyArgsAndReturn(0);

	int ret = oa_tc6_state_machine(&g_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
}

/* READ_PHY_REG dispatch: header mismatch -> spi_err set -> recovery */
void test_state_machine_read_phy_reg_dispatch_mismatch(void)
{
	uint8_t tx[4] = {0x14, 0x62, 0x9D, 0x37};
	uint8_t rx[4] = {0xC5, 0x08, 0x71, 0xEA};
	memcpy(&g_desc.ctrl_tx_buf[0], tx, 4);
	memcpy(&g_desc.ctrl_rx_buf[OA_HEADER_SIZE], rx, 4);
	g_desc.spi_state = OA_SPI_STATE_READ_PHY_REG;
	g_desc.num_ports = 1;
	g_desc.blocking = true;
	capi_irq_enable_IgnoreAndReturn(0);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);
	net_queue_is_empty_IgnoreAndReturn(true);
	calculate_parity_IgnoreAndReturn(1);

	int ret = oa_tc6_state_machine(&g_desc);
	TEST_ASSERT_EQUAL_INT(-EPROTO, ret);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_READY, g_desc.spi_state);
	TEST_ASSERT_EQUAL_UINT32(1, g_desc.spi_err);
}

/* end_data_chunk_processor: FD=1 -> resets offset, increments fd_count, returns */
void test_end_data_chunk_processor_fd_bit_drops_frame(void)
{
	uint32_t event = 0;
	uint32_t oa_rx_footer = no_os_field_prep(OA_DATA_FOOTER_FD_MASK, 1u);
	g_desc.oa_rx_cur_buf_byte_offset = 42;
	g_desc.error_stats.fd_count = 0;
	enum oa_tc6_valid_flag before = g_desc.oa_valid_flag;

	oa_tc6_end_data_chunk_processor(&g_desc, &event, g_rx_frame_buf,
					oa_rx_footer, 0);

	TEST_ASSERT_EQUAL_UINT32(0, g_desc.oa_rx_cur_buf_byte_offset);
	TEST_ASSERT_EQUAL_UINT32(1, g_desc.error_stats.fd_count);
	TEST_ASSERT_EQUAL_UINT(before, g_desc.oa_valid_flag);
}

/* end_data_chunk_processor: no prior START -> invalid_ev_count, valid_flag=END */
void test_end_data_chunk_processor_invalid_ev(void)
{
	uint32_t event = 0;
	uint32_t oa_rx_footer = 0;
	g_desc.oa_valid_flag = OA_VALID_FLAG_END;
	g_desc.oa_rx_cur_buf_byte_offset = 20;
	g_desc.error_stats.invalid_ev_count = 0;

	oa_tc6_end_data_chunk_processor(&g_desc, &event, g_rx_frame_buf,
					oa_rx_footer, 0);

	TEST_ASSERT_EQUAL_UINT32(1, g_desc.error_stats.invalid_ev_count);
	TEST_ASSERT_EQUAL_UINT32(0, g_desc.oa_rx_cur_buf_byte_offset);
	TEST_ASSERT_EQUAL_UINT(OA_VALID_FLAG_END, g_desc.oa_valid_flag);
}

/* end_data_chunk_processor: SV=1 && ebo>sbo -> outer if skipped entirely */
void test_end_data_chunk_processor_sv_with_ebo_gt_sbo_noop(void)
{
	uint32_t event = 0;
	uint32_t oa_rx_footer = no_os_field_prep(OA_DATA_FOOTER_SV_MASK, 1u) |
				no_os_field_prep(OA_DATA_FOOTER_EBO_MASK, 40u) |
				no_os_field_prep(OA_DATA_FOOTER_SWO_MASK, 0u);
	g_desc.oa_valid_flag = OA_VALID_FLAG_START;
	g_desc.oa_rx_cur_buf_byte_offset = 30;

	oa_tc6_end_data_chunk_processor(&g_desc, &event, g_rx_frame_buf,
					oa_rx_footer, 0);

	TEST_ASSERT_EQUAL_UINT(OA_VALID_FLAG_START, g_desc.oa_valid_flag);
	TEST_ASSERT_EQUAL_UINT32(30, g_desc.oa_rx_cur_buf_byte_offset);
}

/* end_data_chunk_processor: buffer overflow -> event |= RX_BUF_OVF */
void test_end_data_chunk_processor_buffer_overflow_sets_event(void)
{
	uint32_t event = 0;
	uint32_t oa_rx_footer = no_os_field_prep(OA_DATA_FOOTER_EBO_MASK, 63u);
	g_desc.oa_valid_flag = OA_VALID_FLAG_START;
	g_desc.oa_rx_cur_buf_byte_offset = G_FRAME_BUF_SIZE - 10;
	net_queue_remove_entry_Ignore();

	oa_tc6_end_data_chunk_processor(&g_desc, &event, g_rx_frame_buf,
					oa_rx_footer, 0);

	TEST_ASSERT_TRUE(event & MAC_CALLBACK_STATUS_RX_BUF_OVF);
	TEST_ASSERT_EQUAL_UINT(OA_VALID_FLAG_END, g_desc.oa_valid_flag);
}

/* start_data_chunk_processor: invalid_sv path (valid_flag=START at entry) */
void test_start_data_chunk_processor_invalid_sv(void)
{
	uint32_t event = 0;
	uint32_t byte_offset = 0;
	uint8_t  ts_bytes[8] = {0};
	uint32_t oa_rx_footer = no_os_field_prep(OA_DATA_FOOTER_SV_MASK, 1u);

	g_desc.oa_valid_flag = OA_VALID_FLAG_START;
	g_desc.oa_rx_use_backup_buf = false;
	g_desc.rx_queue_hp_en = false;
	g_desc.num_ports = 1;
	g_desc.error_stats.invalid_sv_count = 0;
	net_queue_is_empty_IgnoreAndReturn(false);

	oa_tc6_start_data_chunk_processor(&g_desc, &event, &byte_offset, 0,
					  ts_bytes, 64, g_rx_frame_buf,
					  oa_rx_footer);

	TEST_ASSERT_EQUAL_UINT32(1, g_desc.error_stats.invalid_sv_count);
}

/* start_data_chunk_processor: empty queue -> sets backup buf state, returns early */
void test_start_data_chunk_processor_empty_queue_sets_backup(void)
{
	uint32_t event = 0;
	uint32_t byte_offset = 0;
	uint8_t  ts_bytes[8] = {0};
	uint32_t oa_rx_footer = no_os_field_prep(OA_DATA_FOOTER_SV_MASK, 1u);

	g_desc.oa_valid_flag = OA_VALID_FLAG_END;
	g_desc.oa_rx_use_backup_buf = false;
	g_desc.rx_queue_hp_en = false;
	g_desc.num_ports = 1;
	g_desc.oa_trx_size = 128;
	g_desc.oa_rx_buf_chunk_start = 0;
	net_queue_is_empty_IgnoreAndReturn(true);

	oa_tc6_start_data_chunk_processor(&g_desc, &event, &byte_offset, 5,
					  ts_bytes, 64, g_rx_frame_buf,
					  oa_rx_footer);

	TEST_ASSERT_EQUAL_UINT32(5, g_desc.oa_rx_buf_chunk_start);
	TEST_ASSERT_EQUAL_UINT32(128, g_desc.oa_rx_buf_trx_size);
	TEST_ASSERT_FALSE(g_desc.oa_rx_use_backup_buf);
}

/* start_data_chunk_processor: RTSA=1 + ts_format=32B -> timestamp_handler(32B) */
void test_start_data_chunk_processor_rtsa_ts_32bit(void)
{
	uint32_t event = 0;
	uint32_t byte_offset = 0;
	uint8_t  ts_bytes[8] = {0};
	uint32_t oa_rx_footer = no_os_field_prep(OA_DATA_FOOTER_SV_MASK, 1u) |
				no_os_field_prep(OA_DATA_FOOTER_RTSA_MASK, 1u) |
				no_os_field_prep(OA_DATA_FOOTER_RTSP_MASK, 1u);

	g_desc.oa_valid_flag = OA_VALID_FLAG_END;
	g_desc.oa_rx_use_backup_buf = false;
	g_desc.rx_queue_hp_en = false;
	g_desc.num_ports = 1;
	g_desc.ts_format = OA_TS_FORMAT_32B_1588;
	net_queue_is_empty_IgnoreAndReturn(false);
	calculate_parity_IgnoreAndReturn(1);

	oa_tc6_start_data_chunk_processor(&g_desc, &event, &byte_offset, 0,
					  ts_bytes, 64, g_rx_frame_buf,
					  oa_rx_footer);

	TEST_ASSERT_EQUAL_UINT32(1, g_desc.oa_timestamp_parity);
	TEST_ASSERT_EQUAL_UINT(OA_VALID_FLAG_START, g_desc.oa_valid_flag);
}

/* start_data_chunk_processor: RTSA=1 + ts_format=64B_1588 -> process_64_bit_timestamp */
void test_start_data_chunk_processor_rtsa_ts_64bit(void)
{
	uint32_t event = 0;
	uint32_t byte_offset = 0;
	uint8_t  ts_bytes[16] = {0};
	uint32_t oa_rx_footer = no_os_field_prep(OA_DATA_FOOTER_SV_MASK, 1u) |
				no_os_field_prep(OA_DATA_FOOTER_RTSA_MASK, 1u);

	g_desc.oa_valid_flag = OA_VALID_FLAG_END;
	g_desc.oa_rx_use_backup_buf = false;
	g_desc.rx_queue_hp_en = false;
	g_desc.num_ports = 1;
	g_desc.ts_format = OA_TS_FORMAT_64B_1588;
	net_queue_is_empty_IgnoreAndReturn(false);
	calculate_parity_IgnoreAndReturn(0);

	oa_tc6_start_data_chunk_processor(&g_desc, &event, &byte_offset, 0,
					  ts_bytes, 64, g_rx_frame_buf,
					  oa_rx_footer);

	TEST_ASSERT_EQUAL_UINT(OA_VALID_FLAG_START, g_desc.oa_valid_flag);
}

/* start_data_chunk_processor: EV=1 && ebo>sbo -> full_frame_in_chunk_process */
void test_start_data_chunk_processor_ev_full_frame(void)
{
	uint32_t event = 0;
	uint32_t byte_offset = 0;
	uint8_t  ts_bytes[16] = {0};
	/* SV=1, EV=1, SWO=0 (sbo=0), EBO=63 -> fcs_size = 63+1-0-4 = 60 */
	uint32_t oa_rx_footer = no_os_field_prep(OA_DATA_FOOTER_SV_MASK, 1u) |
				no_os_field_prep(OA_DATA_FOOTER_EV_MASK, 1u) |
				no_os_field_prep(OA_DATA_FOOTER_EBO_MASK, 63u);

	g_desc.oa_valid_flag = OA_VALID_FLAG_END;
	g_desc.oa_rx_use_backup_buf = false;
	g_desc.rx_queue_hp_en = false;
	g_desc.num_ports = 1;
	g_desc.fcs_check_en = false;
	g_rx_queue_storage.tail = 0;
	g_rx_buf_desc.trx_size = 0;
	g_rx_buf_desc.cb_func = NULL;

	net_queue_is_empty_IgnoreAndReturn(false);
	net_queue_remove_entry_Ignore();

	oa_tc6_start_data_chunk_processor(&g_desc, &event, &byte_offset, 0,
					  ts_bytes, 64, g_rx_frame_buf,
					  oa_rx_footer);

	/* full_frame_in_chunk_process sets valid_flag back to END */
	TEST_ASSERT_EQUAL_UINT(OA_VALID_FLAG_END, g_desc.oa_valid_flag);
	/* And sets buf_desc->trx_size to fcs_size (60) */
	TEST_ASSERT_EQUAL_UINT32(60, g_rx_buf_desc.trx_size);
}

/* mid_data_chunk_processor: valid_flag != START -> whole block skipped */
void test_mid_data_chunk_processor_no_prior_start_noop(void)
{
	uint32_t event = 0;
	uint32_t oa_rx_footer = 0; /* EV=0 */

	g_desc.oa_valid_flag = OA_VALID_FLAG_END;
	g_desc.oa_rx_cur_buf_byte_offset = 100;

	oa_tc6_mid_data_chunk_processor(&g_desc, 64, oa_rx_footer,
					g_rx_frame_buf, &event, 0, 0);

	/* Nothing should have happened: no event, no offset change */
	TEST_ASSERT_EQUAL_UINT32(0, event);
	TEST_ASSERT_EQUAL_UINT32(100, g_desc.oa_rx_cur_buf_byte_offset);
}

/* mid_data_chunk_processor: overflow with cb_func set -> fires cb */
static uint32_t g_mid_cb_calls;
static void g_mid_cb_stub(void *cd_param, uint32_t event, void *arg)
{
	(void)cd_param; (void)event; (void)arg;
	g_mid_cb_calls++;
}
void test_mid_data_chunk_processor_overflow_fires_cb(void)
{
	uint32_t event = 0;
	uint32_t oa_rx_footer = 0; /* EV=0 */

	g_desc.oa_valid_flag = OA_VALID_FLAG_START;
	g_desc.oa_rx_cur_buf_byte_offset = G_FRAME_BUF_SIZE - 8;
	g_rx_queue_storage.tail = 0;
	g_rx_buf_desc.cb_func = g_mid_cb_stub;
	g_mid_cb_calls = 0;

	net_queue_remove_entry_Ignore();

	oa_tc6_mid_data_chunk_processor(&g_desc, 64, oa_rx_footer,
					g_rx_frame_buf, &event, 0, 0);

	TEST_ASSERT_TRUE(event & MAC_CALLBACK_STATUS_RX_BUF_OVF);
	TEST_ASSERT_EQUAL_UINT32(1, g_mid_cb_calls);
	TEST_ASSERT_EQUAL_UINT32(0, g_desc.oa_rx_cur_buf_byte_offset);
}

/* mid_data_chunk_processor: overflow with cb_func NULL -> skip cb call */
void test_mid_data_chunk_processor_overflow_cb_null(void)
{
	uint32_t event = 0;
	uint32_t oa_rx_footer = 0;
	g_desc.oa_valid_flag = OA_VALID_FLAG_START;
	g_desc.oa_rx_cur_buf_byte_offset = G_FRAME_BUF_SIZE - 8;
	g_rx_buf_desc.cb_func = NULL;
	net_queue_remove_entry_Ignore();

	oa_tc6_mid_data_chunk_processor(&g_desc, 64, oa_rx_footer,
					g_rx_frame_buf, &event, 0, 0);

	TEST_ASSERT_TRUE(event & MAC_CALLBACK_STATUS_RX_BUF_OVF);
	TEST_ASSERT_EQUAL_UINT32(0, g_desc.oa_rx_cur_buf_byte_offset);
}

/* process_64_bit_timestamp: lower 32b split into next chunk */
void test_process_64_bit_timestamp_split(void)
{
	uint32_t byte_offset = 0;
	uint8_t  ts_bytes[16] = {0};
	uint32_t chunk_start = 0;
	uint32_t chunk_size = 4;

	g_desc.oa_timestamp_parity = 0;
	calculate_parity_IgnoreAndReturn(0);

	oa_tc6_process_64_bit_timestamp(&g_desc, chunk_start, chunk_size,
					ts_bytes, &byte_offset);

	TEST_ASSERT_TRUE(g_desc.oa_timestamp_split);
}

/* Mid-frame: oa_tx_cur_buf_byte_offset != 0 -> no SV bit, keeps building same frame */
void test_create_next_chunk_mid_frame_no_sv(void)
{
	uint8_t buf[OA_HEADER_SIZE + 64] = {0};

	g_desc.num_ports = 1;
	g_desc.oa_cps = 6;
	g_desc.oa_tx_cur_buf_byte_offset = 32;
	g_desc.oa_tx_cur_buf_idx = 0;
	/* head=1 so after idx++ (idx becomes 1), the second-frame block
	 * (line 401-402) is not entered: idx(1) != head(1) is false.
	 * This prevents an OOB access on g_tx_entries which has only 1 element. */
	g_tx_queue_storage.head = 1;
	g_tx_queue_storage.tail = 0;
	g_tx_buf_desc.port = 0;
	g_tx_buf_desc.trx_size = 40;

	net_queue_is_empty_IgnoreAndReturn(true);
	net_queue_is_full_IgnoreAndReturn(true);
	calculate_parity_IgnoreAndReturn(1);

	oa_tc6_create_next_chunk(&g_desc, buf, true);

	uint32_t hdr = no_os_get_unaligned_be32(buf);
	TEST_ASSERT_EQUAL_UINT32(1, no_os_field_get(OA_DATA_HEADER_DV_MASK, hdr));
	TEST_ASSERT_EQUAL_UINT32(0, no_os_field_get(OA_DATA_HEADER_SV_MASK, hdr));
	TEST_ASSERT_EQUAL_UINT32(1, no_os_field_get(OA_DATA_HEADER_EV_MASK, hdr));
}

/* Bytes remaining > chunk_bytes_remaining -> bsize=chunk, no EV, oa_tx_cur_buf_idx unchanged */
void test_create_next_chunk_bytes_gt_chunk_no_ev(void)
{
	uint8_t buf[OA_HEADER_SIZE + 64] = {0};

	g_desc.num_ports = 1;
	g_desc.oa_cps = 6;
	g_desc.oa_tx_cur_buf_byte_offset = 0;
	g_desc.oa_tx_cur_buf_idx = 0;
	g_tx_queue_storage.head = 0;
	g_tx_queue_storage.tail = 0;
	g_tx_buf_desc.port = 0;
	g_tx_buf_desc.trx_size = 100;

	net_queue_is_empty_IgnoreAndReturn(true);
	net_queue_is_full_IgnoreAndReturn(true);
	calculate_parity_IgnoreAndReturn(1);

	oa_tc6_create_next_chunk(&g_desc, buf, true);

	uint32_t hdr = no_os_get_unaligned_be32(buf);
	TEST_ASSERT_EQUAL_UINT32(1, no_os_field_get(OA_DATA_HEADER_DV_MASK, hdr));
	TEST_ASSERT_EQUAL_UINT32(1, no_os_field_get(OA_DATA_HEADER_SV_MASK, hdr));
	TEST_ASSERT_EQUAL_UINT32(0, no_os_field_get(OA_DATA_HEADER_EV_MASK, hdr));
	TEST_ASSERT_EQUAL_UINT32(0, g_desc.oa_tx_cur_buf_idx);
	TEST_ASSERT_EQUAL_UINT32(64, g_desc.oa_tx_cur_buf_byte_offset);
}

/* Wrap idx: oa_tx_cur_buf_idx == TX_QUEUE_NUM_ENTRIES_RAW after ++ -> wraps to 0 */
void test_create_next_chunk_wrap_idx(void)
{
	uint8_t buf[OA_HEADER_SIZE + 64] = {0};

	g_desc.num_ports = 1;
	g_desc.oa_cps = 6;
	g_desc.oa_tx_cur_buf_byte_offset = 0;
	g_desc.oa_tx_cur_buf_idx = TX_QUEUE_NUM_ENTRIES_RAW - 1;
	g_tx_queue_storage.head = 0;
	g_tx_queue_storage.tail = 0;
	/* Point the last entry's buf_desc so cur_buf_idx is valid */
	g_tx_entries[0].buf_desc = &g_tx_buf_desc;
	g_tx_buf_desc.port = 0;
	g_tx_buf_desc.trx_size = 16;

	net_queue_is_empty_IgnoreAndReturn(true);
	net_queue_is_full_IgnoreAndReturn(true);
	calculate_parity_IgnoreAndReturn(1);

	static struct eth_frame_struct big_entries[TX_QUEUE_NUM_ENTRIES_RAW];
	memset(big_entries, 0, sizeof(big_entries));
	big_entries[TX_QUEUE_NUM_ENTRIES_RAW - 1].buf_desc = &g_tx_buf_desc;
	g_tx_queue_storage.entries = big_entries;
        
        oa_tc6_create_next_chunk(&g_desc, buf, true);

	TEST_ASSERT_EQUAL_UINT32(0, g_desc.oa_tx_cur_buf_idx);

	/* Restore fixture */
	g_tx_queue_storage.entries = g_tx_entries;
}


/* NULL desc -> -ENODEV */
void test_spi_process_null_desc_returns_enodev(void)
{
	int ret = oa_tc6_spi_process(NULL);
	TEST_ASSERT_EQUAL_INT(-ENODEV, ret);
}

/* Non-empty tx queue path: computes queue_byte_count from entries. */
void test_spi_process_nonempty_queue_computes_chunks(void)
{
	g_desc.oa_cps = 6; /* chunk_size=64 */
	g_desc.oa_tx_cur_buf_idx = 0;
	g_desc.oa_tx_cur_buf_byte_offset = 0;
	g_desc.oa_txc = 10;
	g_desc.oa_rca = 0;
	g_desc.oa_max_chunk_count = 16;
	g_desc.oa_rx_use_backup_buf = false;
	g_desc.spi_state = OA_SPI_STATE_READY;
	g_tx_queue_storage.head = 1;
	g_tx_buf_desc.trx_size = 64; /* exactly one chunk */

	net_queue_is_empty_IgnoreAndReturn(false);
	net_queue_is_full_IgnoreAndReturn(false);
	calculate_parity_IgnoreAndReturn(1);

	int ret = oa_tc6_spi_process(&g_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
	/* One chunk built: OA_HEADER_SIZE + 64 */
	TEST_ASSERT_EQUAL_UINT32(OA_HEADER_SIZE + 64, g_desc.oa_trx_size);
}

/* chunk_count clamped to oa_max_chunk_count. */
void test_spi_process_clamps_to_max_chunk_count(void)
{
	g_desc.oa_cps = 6;
	g_desc.oa_rca = 100; /* huge, will drive chunk_count up */
	g_desc.oa_txc = 0;
	g_desc.oa_max_chunk_count = 3;
	g_desc.oa_rx_use_backup_buf = false;
	g_desc.spi_state = OA_SPI_STATE_READY;

	net_queue_is_empty_IgnoreAndReturn(true);
	net_queue_is_full_IgnoreAndReturn(false);
	calculate_parity_IgnoreAndReturn(1);

	int ret = oa_tc6_spi_process(&g_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
	/* Exactly 3 chunks */
	TEST_ASSERT_EQUAL_UINT32(3 * (OA_HEADER_SIZE + 64), g_desc.oa_trx_size);
}

/* oa_rx_use_backup_buf && chunk_count > OA_RX_BACKUP_BUF_CHUNK_COUNT -> clamp to 1 */
void test_spi_process_backup_buf_clamps_to_backup_chunks(void)
{
	g_desc.oa_cps = 6;
	g_desc.oa_rca = 5;
	g_desc.oa_txc = 0;
	g_desc.oa_max_chunk_count = 16;
	g_desc.oa_rx_use_backup_buf = true;
	g_desc.spi_state = OA_SPI_STATE_READY;

	net_queue_is_empty_IgnoreAndReturn(true);
	net_queue_is_full_IgnoreAndReturn(false);
	calculate_parity_IgnoreAndReturn(1);

	int ret = oa_tc6_spi_process(&g_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
	/* Clamped to OA_RX_BACKUP_BUF_CHUNK_COUNT (=1) */
	TEST_ASSERT_EQUAL_UINT32(OA_RX_BACKUP_BUF_CHUNK_COUNT *
				 (OA_HEADER_SIZE + 64), g_desc.oa_trx_size);
}

/* IRQ_START && chunk_count==0 -> bumped to 1, tx_en=false in loop */
void test_spi_process_irq_start_bumps_to_one_chunk(void)
{
	g_desc.oa_cps = 6;
	g_desc.oa_rca = 0;
	g_desc.oa_txc = 0;
	g_desc.oa_max_chunk_count = 16;
	g_desc.oa_rx_use_backup_buf = false;
	g_desc.spi_state = OA_SPI_STATE_IRQ_START;

	net_queue_is_empty_IgnoreAndReturn(true);
	net_queue_is_full_IgnoreAndReturn(false);
	calculate_parity_IgnoreAndReturn(1);

	int ret = oa_tc6_spi_process(&g_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT32(OA_HEADER_SIZE + 64, g_desc.oa_trx_size);
}


/* chunk_error true -> continue (skip DV block); exst=0, SYNC set later -> READY */
void test_end_data_transaction_chunk_error_continues(void)
{
	uint32_t chunk_size = (1u << OA_DEFAULT_CPS);
	/* First chunk footer = OA_HEADER_BAD -> chunk_error_detector returns true */
	memset(spi_rx_buf, 0, chunk_size + OA_HEADER_SIZE);
	memset(spi_tx_buf, 0, chunk_size + OA_HEADER_SIZE);
	no_os_put_unaligned_be32(OA_HEADER_BAD, &spi_rx_buf[chunk_size]);

	g_desc.oa_trx_size = OA_HEADER_SIZE + chunk_size;
	g_desc.oa_txc = 0;
	g_desc.oa_rca = 0;
	g_desc.error_stats.hdr_parity_error_count = 0;

	/* rx_footer variable retains OA_HEADER_BAD after loop, SYNC bit is 0 in it
	 * -> !SYNC -> takes READ_STATUS path */
	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);

	int ret = oa_tc6_end_data_transaction(&g_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT32(1, g_desc.error_stats.hdr_parity_error_count);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_READ_STATUS, g_desc.spi_state);
}

/* DV=1 with SV=1, EV=0 -> start_data_chunk_processor invoked */
void test_end_data_transaction_dv_sv_invokes_start(void)
{
	uint32_t chunk_size = (1u << OA_DEFAULT_CPS);
	uint32_t footer = no_os_field_prep(OA_DATA_FOOTER_SYNC_MASK, 1) |
			  no_os_field_prep(OA_DATA_FOOTER_DV_MASK, 1) |
			  no_os_field_prep(OA_DATA_FOOTER_SV_MASK, 1);
	memset(spi_rx_buf, 0, chunk_size + OA_HEADER_SIZE);
	memset(spi_tx_buf, 0, chunk_size + OA_HEADER_SIZE);
	no_os_put_unaligned_be32(footer, &spi_rx_buf[chunk_size]);

	g_desc.oa_trx_size = OA_HEADER_SIZE + chunk_size;
	g_desc.oa_txc = 0;
	g_desc.oa_rca = 0;
	g_desc.oa_rx_use_backup_buf = false;
	g_desc.oa_valid_flag = OA_VALID_FLAG_END;
	g_desc.rx_queue_hp_en = false;
	g_desc.num_ports = 1;
	g_desc.fcs_check_en = false;
	g_rx_queue_storage.tail = 0;

	net_queue_is_empty_IgnoreAndReturn(true); /* used by start_data_chunk_processor */
	capi_irq_enable_IgnoreAndReturn(0);
	calculate_parity_IgnoreAndReturn(1);

	int ret = oa_tc6_end_data_transaction(&g_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_READY, g_desc.spi_state);
}

/* Non-empty tx queue with oa_txc -> reenter spi_process, DATA_END */
void test_end_data_transaction_reenters_spi_process(void)
{
	uint32_t chunk_size = (1u << OA_DEFAULT_CPS);
	uint32_t footer = no_os_field_prep(OA_DATA_FOOTER_SYNC_MASK, 1) |
			  no_os_field_prep(OA_DATA_FOOTER_TXC_MASK, 5);
	memset(spi_rx_buf, 0, chunk_size + OA_HEADER_SIZE);
	memset(spi_tx_buf, 0, chunk_size + OA_HEADER_SIZE);
	no_os_put_unaligned_be32(footer, &spi_rx_buf[chunk_size]);

	g_desc.oa_trx_size = OA_HEADER_SIZE + chunk_size;
	g_desc.oa_rca = 0;
	g_desc.oa_rx_use_backup_buf = false;
	g_desc.spi_state = OA_SPI_STATE_READY;
	g_desc.oa_max_chunk_count = 16;
	/* head=1 so spi_process' do-while exits after tx_idx=1 iteration
	 * (g_tx_entries has only 1 element). */
	g_tx_queue_storage.head = 1;
	g_tx_buf_desc.trx_size = 0;

	/* First call for the DV=0 chunk -> mid; then non-empty tx branch */
	net_queue_is_empty_IgnoreAndReturn(false);
	net_queue_is_full_IgnoreAndReturn(false);
	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);

	int ret = oa_tc6_end_data_transaction(&g_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_DATA_END, g_desc.spi_state);
}

/* pending_ctrl=true -> skip capi_irq_enable; state READY */
void test_end_data_transaction_pending_ctrl_skips_irq_enable(void)
{
	uint32_t chunk_size = (1u << OA_DEFAULT_CPS);
	uint32_t footer = no_os_field_prep(OA_DATA_FOOTER_SYNC_MASK, 1);
	memset(spi_rx_buf, 0, chunk_size + OA_HEADER_SIZE);
	memset(spi_tx_buf, 0, chunk_size + OA_HEADER_SIZE);
	no_os_put_unaligned_be32(footer, &spi_rx_buf[chunk_size]);

	g_desc.oa_trx_size = OA_HEADER_SIZE + chunk_size;
	g_desc.oa_txc = 0;
	g_desc.oa_rca = 0;
	g_pending_ctrl = true;

	net_queue_is_empty_IgnoreAndReturn(true);
	calculate_parity_IgnoreAndReturn(1);
	/* No capi_irq_enable expected: strict ordering would fail if called */

	int ret = oa_tc6_end_data_transaction(&g_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_READY, g_desc.spi_state);
}

/* read_status: NULL desc -> -EINVAL */
void test_read_status_null_desc_returns_einval(void)
{
	uint32_t cmd = 0;
	int ret = oa_tc6_read_status(NULL, &cmd);
	TEST_ASSERT_EQUAL_INT(-EINVAL, ret);
}

/* read_status: NULL mdio_cmd_p -> -EINVAL */
void test_read_status_null_mdio_cmd_returns_einval(void)
{
	int ret = oa_tc6_read_status(&g_desc, NULL);
	TEST_ASSERT_EQUAL_INT(-EINVAL, ret);
}

/* read_status: num_ports=1, no PHYINT -> spi_int_handle path (state -> DATA_START) */
void test_read_status_num_ports_1_no_phyint_calls_int_handle(void)
{
	uint32_t cmd = 0;
	g_desc.num_ports = 1;
	g_desc.status_regs->status0 = 0;
	g_desc.status_regs->status1 = 0;
	g_irq_mask0 = 0;
	g_irq_mask1 = 0;

	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);

	int ret = oa_tc6_read_status(&g_desc, &cmd);
	TEST_ASSERT_EQUAL_INT(0, ret);
	/* spi_int_handle sets state to DATA_START at the end */
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_DATA_START, g_desc.spi_state);
}

/* read_status: num_ports=2, no PHYINT -> spi_int_handle path */
void test_read_status_num_ports_2_no_phyint_calls_int_handle(void)
{
	uint32_t cmd = 0;
	g_desc.num_ports = 2;
	g_desc.status_regs->status0 = 0;
	g_desc.status_regs->status1 = 0;
	g_irq_mask0 = 0;
	g_irq_mask1 = 0;

	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);

	int ret = oa_tc6_read_status(&g_desc, &cmd);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_DATA_START, g_desc.spi_state);
}

/* read_status: num_ports=1, PHYINT set -> phy_reg_read_start (state READ_PHY_REG) */
void test_read_status_num_ports_1_phyint_starts_phy_read(void)
{
	uint32_t cmd = 0;
	g_desc.num_ports = 1;
	g_desc.prote_spi = false;
	/* ctrl_cmd_read_data reads status0 (BE) from ctrl_rx_buf[2*OA_HEADER_SIZE].
	 * Put STATUS0_PHYINT there so status0_masked & STATUS0_PHYINT is true. */
	memset(g_desc.ctrl_rx_buf, 0, sizeof(g_desc.ctrl_rx_buf));
	no_os_put_unaligned_be32(STATUS0_PHYINT,
				 &g_desc.ctrl_rx_buf[2 * OA_HEADER_SIZE]);
	g_irq_mask0 = 0;
	g_irq_mask1 = 0;

	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);

	int ret = oa_tc6_read_status(&g_desc, &cmd);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_READ_PHY_REG, g_desc.spi_state);
}

/* record_port_status: NULL desc -> return (no crash) */
void test_record_port_status_null_desc_returns(void)
{
	uint32_t cmd = 0;
	oa_tc6_record_port_status(NULL, &cmd, 1);
	TEST_ASSERT_TRUE(1); /* Reached this line without crash */
}

/* record_port_status: NULL mdio_cmd -> return (no crash) */
void test_record_port_status_null_mdio_cmd_returns(void)
{
	oa_tc6_record_port_status(&g_desc, NULL, 1);
	TEST_ASSERT_TRUE(1);
}

/* record_port_status: second call (status_masked != stat_init) -> PHY_SUBSYS_IRQ record */
void test_record_port_status_second_call_records_subsys(void)
{
	uint32_t cmd = 0;
	g_desc.num_ports = 1;
	/* Force !stat_init match by setting status_masked to a distinct value */
	g_desc.status_regs->p1_status_masked = 0;
	g_reg_data = 0x12345678;
	g_phy_irq_mask = 0xFFFFFFFF;

	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);

	oa_tc6_record_port_status(&g_desc, &cmd, 1);
	/* spi_int_handle called at the end, sets state DATA_START */
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_DATA_START, g_desc.spi_state);
}

/* read_phy_reg: NULL desc -> -EINVAL */
void test_read_phy_reg_null_desc_returns_einval(void)
{
	uint32_t cmd = 0;
	int ret = oa_tc6_read_phy_reg(NULL, &cmd);
	TEST_ASSERT_EQUAL_INT(-EINVAL, ret);
}

/* read_phy_reg: NULL mdio_cmd -> -EINVAL */
void test_read_phy_reg_null_mdio_cmd_returns_einval(void)
{
	int ret = oa_tc6_read_phy_reg(&g_desc, NULL);
	TEST_ASSERT_EQUAL_INT(-EINVAL, ret);
}

/* read_phy_reg: check_hdr != echo_hdr -> spi_err=1, dispatches int_handle */
void test_read_phy_reg_hdr_mismatch_sets_err(void)
{
	uint32_t cmd = 0;
	uint32_t chdr = 0x11111111;
	uint32_t ehdr = 0x22222222;
	memcpy(&g_desc.ctrl_tx_buf[0], &chdr, 4);
	memcpy(&g_desc.ctrl_rx_buf[OA_HEADER_SIZE], &ehdr, 4);

	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);

	int ret = oa_tc6_read_phy_reg(&g_desc, &cmd);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT(1, g_desc.spi_err);
}

/* read_phy_reg: reg_addr==0x020, hdrs match -> phy_reg_read_step (2nd write) */
void test_read_phy_reg_reg_addr_020_triggers_step(void)
{
	uint32_t cmd = 0;
	uint32_t hdr = 0x12345678;
	memcpy(&g_desc.ctrl_tx_buf[0], &hdr, 4);
	memcpy(&g_desc.ctrl_rx_buf[OA_HEADER_SIZE], &hdr, 4);
	g_desc.reg_addr = 0X020U;

	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);

	int ret = oa_tc6_read_phy_reg(&g_desc, &cmd);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_READ_PHY_REG, g_desc.spi_state);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_WRITE, g_desc.wnr);
}

/* read_phy_reg: wnr==WRITE (not 0x020) -> phy_reg_read_step (read pass) */
void test_read_phy_reg_wnr_write_transitions_to_read(void)
{
	uint32_t cmd = 0;
	uint32_t hdr = 0x87654321;
	memcpy(&g_desc.ctrl_tx_buf[0], &hdr, 4);
	memcpy(&g_desc.ctrl_rx_buf[OA_HEADER_SIZE], &hdr, 4);
	g_desc.reg_addr = 0X021U;
	g_desc.wnr = OA_SPI_WRITE;

	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);

	int ret = oa_tc6_read_phy_reg(&g_desc, &cmd);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_READ, g_desc.wnr);
}

/* Callback capture state for spi_int_handle tests */
static uint32_t g_link_cb_calls;
static uint32_t g_ts_cb_calls;
static uint32_t g_status_cb_calls;
static void g_link_cb_stub(void *p, uint32_t e, void *a)
{
	(void)p; (void)e; (void)a; g_link_cb_calls++;
}
static void g_ts_cb_stub(void *p, uint32_t e, void *a)
{
	(void)p; (void)e; (void)a; g_ts_cb_calls++;
}
static void g_status_cb_stub(void *p, uint32_t e, void *a)
{
	(void)p; (void)e; (void)a; g_status_cb_calls++;
}
static void *g_cb_params[MAC_EVT_MAX];

/* NULL desc -> -EINVAL */
void test_spi_int_handle_null_desc_returns_einval(void)
{
	int ret = oa_tc6_spi_int_handle(NULL);
	TEST_ASSERT_EQUAL_INT(-EINVAL, ret);
}

/* num_ports=1, LINK_CHANGE set, cb registered -> cb fired */
void test_spi_int_handle_link_change_fires_cb(void)
{
	g_desc.num_ports = 1;
	g_desc.status_regs->status1_masked = STATUS1_LINK_CHANGE_MASK;
	g_desc.status_regs->status1 = 0;
	g_desc.status_regs->status0_masked = 0;
	g_desc.cb_func[MAC_EVT_LINK_CHANGE] = g_link_cb_stub;
	g_desc.cb_param_p = g_cb_params;
	g_link_cb_calls = 0;

	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);

	int ret = oa_tc6_spi_int_handle(&g_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT32(1, g_link_cb_calls);
}

/* num_ports=1, TTSCAA set, ts cb registered -> ts cb fired */
void test_spi_int_handle_ts_ready_fires_cb(void)
{
	g_desc.num_ports = 1;
	g_desc.status_regs->status0_masked = STATUS0_TTSCAA_MASK;
	g_desc.status_regs->status1_masked = 0;
	g_desc.cb_func[MAC_EVT_TIMESTAMP_RDY] = g_ts_cb_stub;
	g_desc.cb_param_p = g_cb_params;
	g_ts_cb_calls = 0;

	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);

	int ret = oa_tc6_spi_int_handle(&g_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT32(1, g_ts_cb_calls);
}

/* num_ports=2, P2 TTSCAA set, ts cb registered -> ts cb fired */
void test_spi_int_handle_num_ports_2_p2_ts_ready_fires_cb(void)
{
	g_desc.num_ports = 2;
	g_desc.status_regs->status0_masked = 0;
	g_desc.status_regs->status1_masked = STATUS1_P2_TTSCAA_MASK;
	g_desc.cb_func[MAC_EVT_TIMESTAMP_RDY] = g_ts_cb_stub;
	g_desc.cb_param_p = g_cb_params;
	g_ts_cb_calls = 0;

	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);

	int ret = oa_tc6_spi_int_handle(&g_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT32(1, g_ts_cb_calls);
}

/* General status cb: status0_masked or status1_masked nonzero -> cb fires */
void test_spi_int_handle_general_status_fires_cb(void)
{
	g_desc.num_ports = 1;
	g_desc.status_regs->status0_masked = 0x01;
	g_desc.status_regs->status1_masked = 0;
	g_desc.cb_func[MAC_EVT_STATUS] = g_status_cb_stub;
	g_desc.cb_param_p = g_cb_params;
	g_status_cb_calls = 0;

	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_IgnoreAndReturn(0);
	capi_spi_transceive_async_IgnoreAndReturn(0);
	capi_spi_transceive_dma_async_IgnoreAndReturn(0);

	int ret = oa_tc6_spi_int_handle(&g_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT32(1, g_status_cb_calls);
}

/* blocking=true -> uses capi_spi_transceive (blocking); state DATA_START */
void test_spi_int_handle_blocking_path(void)
{
	g_desc.blocking = true;
	g_desc.status_regs->status0_masked = 0;
	g_desc.status_regs->status1_masked = 0;

	calculate_parity_IgnoreAndReturn(1);
	capi_spi_transceive_IgnoreAndReturn(0);

	int ret = oa_tc6_spi_int_handle(&g_desc);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_EQUAL_UINT(OA_SPI_STATE_DATA_START, g_desc.spi_state);
}

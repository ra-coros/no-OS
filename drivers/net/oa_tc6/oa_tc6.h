/*******************************************************************************
 *   @file   oa_tc6.h
 *   @brief  Header file for the Open Alliance TC6 SPI driver.
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
#ifndef _NO_OS_OA_TC6_H
#define _NO_OS_OA_TC6_H

#include "capi_spi.h"
#include "capi_dma.h"
#include "capi_gpio.h"
#include "capi_irq.h"
#include "no_os_util.h"
#include "utilities.h"
#include "net_queue.h"
#include <stdint.h>

#define PRAGMA(x) _Pragma(#x)
#define ATTRIBUTE(x) __attribute__((x))

#if defined (__GNUC__)
  /* Gcc uses attributes */
  #define HAL_ALIGNED_PRAGMA(num)
  #define HAL_ALIGNED_ATTRIBUTE(num) ATTRIBUTE(aligned(num))
  #define HAL_UNUSED_ATTRIBUTE ATTRIBUTE(unused)
#elif defined ( __ICCARM__ )
  /* IAR uses a pragma */
  #define HAL_ALIGNED_ATTRIBUTE(num)
  #define HAL_ALIGNED_PRAGMA(num) PRAGMA(data_alignment=num)
  #define HAL_UNUSED_ATTRIBUTE
#elif defined (__CC_ARM)
  /* Keil uses a decorator which is placed in the same position as pragmas */
  #define HAL_ALIGNED_ATTRIBUTE(num)
  #define HAL_ALIGNED_PRAGMA(num) __align(##num)
  #define HAL_UNUSED_ATTRIBUTE ATTRIBUTE(unused)
#else
#error "Toolchain not supported"
#endif

#define DMA_BUFFER_ALIGN(var, alignBytes)   HAL_ALIGNED_PRAGMA(alignBytes) var HAL_ALIGNED_ATTRIBUTE(alignBytes)

/*! Minimum transaction size above which DMA is enabled for a SPI transaction. */
/*  Interrupt-based transactions are used when the transaction size is less than this threshold. */
#ifndef MIN_SIZE_FOR_DMA
#define MIN_SIZE_FOR_DMA        16
#endif


// To be defined by user application
#define     HAL_OA_STATE_MACHINE_LOCK(...)

#define     HAL_OA_STATE_MACHINE_UNLOCK(...)

#define     HAL_IPOS_UnlockSleepLock(...)


/*! Size of the Tx queue, can be previously defined by the application. */
#ifndef TX_QUEUE_NUM_ENTRIES
#define TX_QUEUE_NUM_ENTRIES    4
#endif

/*! Size of the Rx queue, can be previously defined by the application. */
#ifndef RX_QUEUE_NUM_ENTRIES
#define RX_QUEUE_NUM_ENTRIES    4
#endif

/*! Actual size of the Rx queue. */
#ifndef RX_QUEUE_NUM_ENTRIES_RAW
#define RX_QUEUE_NUM_ENTRIES_RAW        (RX_QUEUE_NUM_ENTRIES + 1) 
#endif

/*! Actual Size of the Tx queue. */
#ifndef TX_QUEUE_NUM_ENTRIES_RAW
#define TX_QUEUE_NUM_ENTRIES_RAW        (TX_QUEUE_NUM_ENTRIES + 1)
#endif

/*! FCS size */
#ifndef FCS_SIZE
#define FCS_SIZE 4
#endif

#ifndef PHY_PORT_1_ADDRESS
#define PHY_PORT_1_ADDRESS      1
#endif

#ifndef PHY_PORT_2_ADDRESS
#define PHY_PORT_2_ADDRESS      2
#endif

/*! Initialization value for status variables corresponding to PHY IRQ registers. */
/*  This value cannot be achieved in hardware, also different from 0xFFFFFFFF which can indicate an MDIO error. */
#ifndef OA_PHY_STATUS_INIT_VAL
#define OA_PHY_STATUS_INIT_VAL         (0x7FFF7FFF)
#endif

 /* PHY Interrupt for Port1. */
#ifndef STATUS0_PHYINT
#define STATUS0_PHYINT                 (0X00000080U) 
#endif

/* PHY Interrupt for Port2. */
#ifndef STATUS1_P2_PHYINT
#define STATUS1_P2_PHYINT               (0X00080000U)  
#endif

/* System Interrupt Status Register */
#ifndef ADDR_CRSM_IRQ_STATUS
#define ADDR_CRSM_IRQ_STATUS            (0X1E0010U)
#endif

#ifndef CONFIG_OA_TX_FRAME_BUFF_NUM
#define CONFIG_OA_TX_FRAME_BUFF_NUM	2
#endif

#ifndef CONFIG_OA_RX_FRAME_BUFF_NUM
#define CONFIG_OA_RX_FRAME_BUFF_NUM	5
#endif

#ifndef CONFIG_OA_CHUNK_BUFFER_SIZE
#define CONFIG_OA_CHUNK_BUFFER_SIZE	1514
#endif

#ifndef CONFIG_OA_THREAD_RX_LIMIT
#define CONFIG_OA_THREAD_RX_LIMIT	5
#endif

#ifndef CONFIG_OA_ZERO_SWO_ONLY
#define CONFIG_OA_ZERO_SWO_ONLY		1
#endif

#define OA_TX_FRAME_BUFF_NUM		CONFIG_OA_TX_FRAME_BUFF_NUM
#define OA_RX_FRAME_BUFF_NUM		CONFIG_OA_RX_FRAME_BUFF_NUM

/* Space for one full frame + 24 chunk headers (68 * 24)*/
#define OA_SPI_BUFF_LEN		1632

/* PROTE 1-reg control transaction = 24 bytes on wire (spec V1.1 §7.6). */
#define OA_SPI_CTRL_LEN		24

/*! Size of the buffers used in control transactions, in bytes. */
#define OA_CTRL_BUF_SIZE    256

/*! Size of data buffer used in SPI transactions, in bytes. */
#define BUFFER_SIZE     2000
     
/*! Receive backup buffer size, in chunks. Note this is not currently used! */
#define OA_RX_BACKUP_BUF_CHUNK_COUNT    (1)
/*! Receive backup buffer size, in bytes. Note this is not currently used! */
#define OA_RX_BACKUP_BUF_SIZE           (68 * OA_RX_BACKUP_BUF_CHUNK_COUNT)

#define OA_TC6_CONFIG0_PROTE_MASK	NO_OS_BIT(5)


/*! SPI footer size, in bytes. */
#define OA_FOOTER_SIZE  4

/*! SPI header size, in bytes. */
#define OA_HEADER_SIZE  4

#define OA_DEFAULT_CPS  6
#define OA_CHUNK_SIZE		64

/*! Maximum number of chunks that will be packed into a single SPI transaction */
#define OA_MAX_CHUNK_COUNT              31
#define OA_MAX_CHUNK64_COUNT            16

#define TS_BYTES_SIZE                   8
#define OA_SPI_BYTE_SIZE                4

#define OA_TIMESTAMP_32BIT_SIZE         4
#define OA_TIMESTAMP_64BIT_SIZE         8
#define OA_TIMESTAMP_UPPER_OFFSET       (OA_TIMESTAMP_32BIT_SIZE)  /*!< Offset of the upper 32 bits in an 8-byte timestamp. */
     
/*! When a header parity error is detected, the MAC replies with this pattern. */
#define OA_HEADER_BAD (0x40000000)

#define OA_REG_LEN		4

/*! SPI register access size in bytes. */
#define OA_ACCESS_SIZE 4

#define OA_HEADER_LEN		4
#define OA_FOOTER_LEN		4

/*! SPI header value indicating a read transaction.     */
#define OA_SPI_READ  0

/*! SPI header value indicating a write transaction.    */
#define OA_SPI_WRITE 1

#define SPI_COMMS_TIMEOUT_US    50000

#define OA_MMS_REG(m, r)	(((m) << 16) | ((r) & NO_OS_GENMASK(15, 0)))
#define OA_CTRL_ADDR_MMS_MASK	NO_OS_GENMASK(27, 8)

#define OA_CTRL_HEADER_DNC_MASK	        (uint32_t)NO_OS_BIT(31)
#define OA_CTRL_HEADER_HDRB_MASK	NO_OS_BIT(30)
#define OA_CTRL_HEADER_WNR_MASK	        NO_OS_BIT(29)
#define OA_CTRL_HEADER_AID_MASK         NO_OS_BIT(28)
#define OA_CTRL_HEADER_MMS_MASK         NO_OS_GENMASK(27, 24)
#define OA_CTRL_HEADER_ADDR_MASK	NO_OS_GENMASK(23, 8)
#define OA_CTRL_HEADER_LEN_MASK         NO_OS_GENMASK(7, 1)
#define OA_CTRL_HEADER_P_MASK		NO_OS_BIT(0)

#define OA_DATA_FOOTER_EXST_MASK	(uint32_t)NO_OS_BIT(31)
#define OA_DATA_FOOTER_HDRB_MASK	NO_OS_BIT(30)
#define OA_DATA_FOOTER_SYNC_MASK	NO_OS_BIT(29)
#define OA_DATA_FOOTER_RCA_MASK		NO_OS_GENMASK(28, 24)
#define OA_DATA_FOOTER_VS_MASK		NO_OS_GENMASK(23, 22)
#define OA_DATA_FOOTER_DV_MASK		NO_OS_BIT(21)
#define OA_DATA_FOOTER_SV_MASK		NO_OS_BIT(20)
#define OA_DATA_FOOTER_SWO_MASK		NO_OS_GENMASK(19, 16)
#define OA_DATA_FOOTER_FD_MASK		NO_OS_BIT(15)
#define OA_DATA_FOOTER_EV_MASK		NO_OS_BIT(14)
#define OA_DATA_FOOTER_EBO_MASK		NO_OS_GENMASK(13, 8)
#define OA_DATA_FOOTER_RTSA_MASK        NO_OS_BIT(7)
#define OA_DATA_FOOTER_RTSP_MASK        NO_OS_BIT(6)
#define OA_DATA_FOOTER_TXC_MASK		NO_OS_GENMASK(5, 1)
#define OA_DATA_FOOTER_P_MASK		NO_OS_BIT(0)

#define OA_DATA_HEADER_DNC_MASK		(uint32_t)NO_OS_BIT(31)
#define OA_DATA_HEADER_SEQ_MASK		NO_OS_BIT(30)
#define OA_DATA_HEADER_NORX_MASK        NO_OS_BIT(29)
#define OA_DATA_HEADER_VS_MASK		NO_OS_GENMASK(23, 22)
#define OA_DATA_HEADER_DV_MASK		NO_OS_BIT(21)
#define OA_DATA_HEADER_SV_MASK		NO_OS_BIT(20)
#define OA_DATA_HEADER_SWO_MASK		NO_OS_GENMASK(19, 16)
#define OA_DATA_HEADER_EV_MASK		NO_OS_BIT(14)
#define OA_DATA_HEADER_EBO_MASK		NO_OS_GENMASK(13, 8)
#define OA_DATA_HEADER_TSC_MASK		NO_OS_GENMASK(7, 6)
#define OA_DATA_HEADER_P_MASK		NO_OS_BIT(0)

#define MDIO_TRDONE_MASK                (uint32_t)NO_OS_BIT(31)
#define MDIO_TAERR_MASK                 NO_OS_BIT(30)
#define MDIO_ST_MASK                    NO_OS_GENMASK(29, 28)
#define MDIO_OP_MASK                    NO_OS_GENMASK(27, 26)
#define MDIO_PRTAD_MASK                 NO_OS_GENMASK(25, 21)
#define MDIO_DEVAD_MASK                 NO_OS_GENMASK(20, 16)
#define MDIO_DATA_MASK                  NO_OS_GENMASK(15, 0)

/*! MDIO Device Address extraction from 32-bit PHY register address. */
#define DEVTYPE(a)                          (a >> 16)

/*! MDIO Register Address extraction from 32-bit PHY register address. */
#define REGADDR(a)                          (a & 0xFFFF)

#define MDIOACC_N_MDIO_ST_CLAUSE22     (0X00000001U)  /* MDIO CLAUSE 22 */
#define MDIOACC_N_MDIO_ST_CLAUSE45     (0X00000000U)  /* MDIO CLAUSE 45 */
#define MDIOACC_N_MDIO_OP_MD_ADDR      (0X00000000U)  /* MD Address Command. */
#define MDIOACC_N_MDIO_OP_MD_WR        (0X00000001U)  /* Write Command. */
#define MDIOACC_N_MDIO_OP_MD_RD        (0X00000003U)  /* Read Command. */
#define MDIOACC_N_MDIO_OP_MD_INC_RD    (0X00000002U)  /* Incremental Read Command. */

/* STATUS0*/
#define STATUS0_CDPE_MASK               NO_OS_BIT(12)
#define STATUS0_TXFCSE_MASK             NO_OS_BIT(11)
#define STATUS0_TTSCAC_MASK             NO_OS_BIT(10)
#define STATUS0_TTSCAB_MASK             NO_OS_BIT(9)
#define STATUS0_TTSCAA_MASK             NO_OS_BIT(8)
#define STATUS0_PHYINT_MASK             NO_OS_BIT(7)
#define STATUS0_RESETC_MASK             NO_OS_BIT(6)
#define STATUS0_HDRE_MASK               NO_OS_BIT(5)
#define STATUS0_LOFE_MASK               NO_OS_BIT(4)
#define STATUS0_RXBOE_MASK              NO_OS_BIT(3)
#define STATUS0_TXBUE_MASK              NO_OS_BIT(2)
#define STATUS0_TXBOE_MASK              NO_OS_BIT(1)
#define STATUS0_TXPE_MASK               NO_OS_BIT(0)

/* STATUS1 */
#define STATUS1_P2_TXFCSE_MASK          NO_OS_BIT(24)
#define STATUS1_P2_RX_IFG_ERR_MASK      NO_OS_BIT(23)
#define STATUS1_P2_TTSCAC_MASK          NO_OS_BIT(22)
#define STATUS1_P2_TTSCAB_MASK          NO_OS_BIT(21)
#define STATUS1_P2_TTSCAA_MASK          NO_OS_BIT(20)
#define STATUS1_P2_PHYINT_MASK          NO_OS_BIT(19)
#define STATUS1_P2_RX_RDY_HI_MASK       NO_OS_BIT(18)
#define STATUS1_P2_RX_RDY_MASK          NO_OS_BIT(17)
#define STATUS1_TX_ECC_ERR_MASK         NO_OS_BIT(12)
#define STATUS1_RX_ECC_ERR_MASK         NO_OS_BIT(11)
#define STATUS1_SPI_ERR_MASK            NO_OS_BIT(10)
#define STATUS1_P1_RX_IFG_ERR_MASK      NO_OS_BIT(8)
#define STATUS1_P1_RX_RDY_HI_MASK       NO_OS_BIT(5)
#define STATUS1_P1_RX_RDY_MASK          NO_OS_BIT(4)
#define STATUS1_TX_RDY_MASK             NO_OS_BIT(3)
#define STATUS1_LINK_CHANGE_MASK        NO_OS_BIT(1)
#define STATUS1_P1_LINK_STATUS_MASK     NO_OS_BIT(0)

/* Standard control and status registers (MMS 0) */

#define OA_TC6_IDVER_REG		OA_MMS_REG(0x0, 0x0000)
#define OA_TC6_PHYID_REG		OA_MMS_REG(0x0, 0x0001)
#define OA_TC6_STDCAP_REG		OA_MMS_REG(0x0, 0x0002)
#define OA_TC6_RESET_REG		OA_MMS_REG(0x0, 0x0003)
#define OA_TC6_CONFIG0_REG		OA_MMS_REG(0x0, 0x0004)
#define OA_TC6_CONFIG1_REG		OA_MMS_REG(0x0, 0x0005)
#define OA_TC6_CONFIG2_REG		OA_MMS_REG(0x0, 0x0006)
#define OA_TC6_STATUS0_REG		OA_MMS_REG(0x0, 0x0008)
#define OA_TC6_STATUS1_REG		OA_MMS_REG(0x0, 0x0009)
#define OA_TC6_BUFST_REG		OA_MMS_REG(0x0, 0x000B)
#define OA_TC6_IMSK0_REG		OA_MMS_REG(0x0, 0x000C)
#define OA_TC6_IMSK1_REG		OA_MMS_REG(0x0, 0x000D)

#define OA_TC6_TTSCAH_REG		OA_MMS_REG(0x0, 0x0010)
#define OA_TC6_TTSCAL_REG		OA_MMS_REG(0x0, 0x0011)
#define OA_TC6_TTSCBH_REG		OA_MMS_REG(0x0, 0x0012)
#define OA_TC6_TTSCBL_REG		OA_MMS_REG(0x0, 0x0013)
#define OA_TC6_TTSCCH_REG		OA_MMS_REG(0x0, 0x0014)
#define OA_TC6_TTSCCL_REG		OA_MMS_REG(0x0, 0x0015)
#define OA_TC6_MDIOACC0_REG		OA_MMS_REG(0x0, 0x0020)
#define OA_TC6_MDIOACC1_REG 		OA_MMS_REG(0x0, 0x0021)
#define OA_TC6_MDIOACC2_REG 		OA_MMS_REG(0x0, 0x0022)
#define OA_TC6_MDIOACC3_REG 		OA_MMS_REG(0x0, 0x0023)
#define OA_TC6_MDIOACC4_REG 		OA_MMS_REG(0x0, 0x0024)
#define OA_TC6_MDIOACC5_REG 		OA_MMS_REG(0x0, 0x0025)
#define OA_TC6_MDIOACC6_REG 		OA_MMS_REG(0x0, 0x0026)
#define OA_TC6_MDIOACC7_REG 		OA_MMS_REG(0x0, 0x0027)

#define OA_TC6_CONFIG0_ZARFE_MASK	NO_OS_BIT(12)

#define OA_TC6_BUFSTS_TXC_MASK 		NO_OS_GENMASK(15, 8)
#define OA_TC6_BUFSTS_RCA_MASK 		NO_OS_GENMASK(7, 0)

/**
 * @brief State for data buffers containing Ethernet frames
 */
enum oa_tc6_user_buffer_state {
	/*
	 * Neither the OA nor the user application doesn't currently
	 * use this buffer. OA can start writing it.
	 */
	OA_BUFF_FREE,

	/*
	 * The buffer is currently written by OA, but it doesn't yet
	 * contain a complete frame. The user application cannot reference
	 * the data yet.
	 */
	OA_BUFF_RX_IN_PROGRESS,

	/*
	 * The buffer has a complete frame, and is no longer accessed by OA.
	 * The user application could use this.
	 */
	OA_BUFF_RX_COMPLETE,

	/* The buffer is accessed by the user application. OA will not overwrite it */
	OA_BUFF_RX_USER_OWNED,

	/* The user writes to this buffer. OA doesn't access it. */
	OA_BUFF_TX_BUSY,

	/* The buffer is ready to be transmitted. The user won't access it anymore. */
	OA_BUFF_TX_READY,
};

/**
 * @brief OPEN Alliance SPI States
 */
enum oa_tc6_spi_state  {
        OA_SPI_STATE_READY = 0,
        OA_SPI_STATE_CTRL_START,
        OA_SPI_STATE_CTRL_END,
        OA_SPI_STATE_DATA_START,
        OA_SPI_STATE_DATA_END,
        OA_SPI_STATE_IRQ_START,
        OA_SPI_STATE_READ_STATUS,
        OA_SPI_STATE_READ_PHY_REG,
};

struct reg_val {
	union {
		struct {
			uint16_t lower;
			uint16_t upper;
		};
		uint32_t reg_val;
	};
};

/*!
 * @brief OA TC6 Events.
 */ 
enum oa_tc6_event {
        OA_TC6_EVT_RX_CHUNK_READY,
        OA_TC6_EVT_TX_CHUNK_DONE,
        OA_TC6_EVT_CTRL_DONE,
        OA_TC6_EVT_EXT_STATUS_PENDING,   /* footer EXST=1 */
        OA_TC6_EVT_PROTO_ERROR,          /* HDRB, parity, SYNC loss */
};

/*!
 * @brief Latest valid flag (EV or SV) read from the footer.
 */
enum oa_tc6_valid_flag 
{
        /*!< No SV or EV flag received yet. This is the initial value */
        OA_VALID_FLAG_NONE = 0, 

        /*!< Latest valid flag was SV */
        OA_VALID_FLAG_START,

        /*!< Latest valid flag was EV */
        OA_VALID_FLAG_END,
};

/*!
 * @brief Error statistics (OPEN Alliance).
 */
struct oa_tc6_error_stats 
{
        /*!< Number of frames dropped due to FD=1 in the footer */
        uint32_t fd_count;

        /*!< Number of invalid EV=1 detected in the footer (no preceding SV) */
        uint32_t invalid_ev_count;

        /*!< Number of invalid SV=1 detected in the footer (no preceding EV) */
        uint32_t invalid_sv_count; 

        /*!< Number of footer parity errors detected */
        uint32_t ftr_parity_error_count;

        /*!< Number of header parity errors detected */
        uint32_t hdr_parity_error_count;

        /*!< Number of SYNC errors (SYNC=0 in the footer) */
        uint32_t sync_error_count;
};

/*!
 * @brief Determines how timestamp fields are extracted from the chunk 
 *  payload and stored in the frame buffer.
 */
enum oa_tc6_ts_format  
{
        /*!< No timestamp inserted or captured */
        OA_TS_FORMAT_NONE = 0, 

        /*!< 32-bit free-running counter timestamp */
        OA_TS_FORMAT_32B_FREE,

        /*!< 32-bit nanosecond timestamp */
        OA_TS_FORMAT_32B_1588,

        /*!< 64-bit timestamp (upper 32-bit seconds + lower 32-bit nanoseconds) */
        OA_TS_FORMAT_64B_1588
};

/*!
 * @brief Latest valid flag (EV or SV) read from the footer.
 */
enum oa_tc6_ts_operation   
{
        /*!< No timestamp operation */
        OA_TS_UPPER_32_BIT = 0, 

        /*!< Insert timestamp operation */
        OA_TS_LOWER_32_BIT,

        /*!< Remove timestamp operation  */
        OA_TS_32_BIT
};

/**
 * @brief Stores an Ethernet frame along with metadata needed for parsing.
 * The MAC driver or the user application will receive and submit frames for transmission
 * in this format.
 */
struct oa_tc6_frame_buffer {
	uint32_t index;
	uint32_t len;
	uint8_t data[CONFIG_OA_CHUNK_BUFFER_SIZE];
	enum oa_tc6_user_buffer_state state;
	uint8_t vs;      /**< Vendor Specific. Tx or Rx */
	uint8_t tsc;     /**< Timestamp capture. 2-bits. Tx Only */
	bool frame_drop; /**< Frame should be dropped (is invalid). Rx Only */
	bool rtsa;       /**< Timestamp added. Rx Only */
	bool rtsp;       /**< Timestamp parity. Rx Only */
};

/**
 * @brief Stores the status flags which are provided as part of the footer
 * during data transfers. These flags will be latched by the driver and can be
 * cleared by the user when reading.
 */
struct oa_tc6_flags {
	bool flags_valid; /**< Indicates the flags are latched and valid */
	bool exst;        /**< Latched high until cleared */
	bool hdrb;        /**< Latched high until cleared */
	bool sync;        /**< Instantaneous value */
};

/**
 * @brief Holds the frame buffers and the communication descriptor for the OA TC6 driver.
 */
struct oa_tc6_desc {
	struct capi_spi_device *comm_desc;
	uint8_t ctrl_chunks[OA_SPI_CTRL_LEN];
	uint8_t data_chunks[OA_SPI_BUFF_LEN];

	struct oa_tc6_frame_buffer user_rx_frame_buffer[OA_RX_FRAME_BUFF_NUM];
	struct oa_tc6_frame_buffer user_tx_frame_buffer[OA_TX_FRAME_BUFF_NUM];

	uint32_t data_tx_credit;
	uint32_t data_rx_credit;

	uint32_t ctrl_tx_credit;
	uint32_t ctrl_rx_credit;

        bool prote_spi;            
        bool rx_queue_hp_en;
        bool fcs_check_en;
        uint8_t num_ports;

        volatile enum oa_tc6_spi_state spi_state;
        volatile uint32_t spi_err;

        enum oa_tc6_ts_format ts_format;

        eth_callback_t *cb_func;
        void **cb_param_p;
        void *app_device;

        struct net_queue *tx_queue;
        struct net_queue **rx_queue;
        struct net_queue *rx_queue_lp;
        struct net_queue *rx_queue_hp;

        volatile bool *pending_ctrl;
        struct eth_status_registers *status_regs;
        uint32_t *irq_mask0;
        uint32_t *irq_mask1;
        uint32_t *phy_irq_mask;

        uint32_t oa_txc;
        uint32_t oa_rca;
        uint32_t oa_tx_cur_buf_byte_offset;
        uint32_t oa_rx_cur_buf_byte_offset;
        uint32_t oa_tx_cur_buf_idx;
        uint32_t oa_rx_cur_buf_idx;
        uint32_t oa_cps;
        uint32_t oa_max_chunk_count;
        uint32_t oa_trx_size;
        uint32_t oa_timestamp_split;
        uint32_t oa_timestamp_parity;

        enum oa_tc6_valid_flag oa_valid_flag;
        struct oa_tc6_flags xfer_flags;
        struct oa_tc6_error_stats error_stats;

        bool oa_rx_use_backup_buf;
        uint32_t oa_rx_buf_chunk_start;    
        uint32_t oa_rx_buf_trx_size;
        uint8_t oa_rx_backup_buf[OA_RX_BACKUP_BUF_SIZE];

        bool  blocking;
        uint32_t wnr;
        uint32_t reg_addr;
        uint32_t *reg_data;
        uint32_t cnt;

        uint8_t ctrl_tx_buf[OA_CTRL_BUF_SIZE];
        uint8_t ctrl_rx_buf[OA_CTRL_BUF_SIZE];
};

/**
 * @brief Holds the initialization parameters for the OA TC6 driver.
 */
struct oa_tc6_init_param {
        struct capi_spi_device *comm_desc;
        bool prote_spi;    
        bool rx_queue_hp_en;
        bool fcs_check_en;
        uint8_t num_ports;
};

/* Read a register from the MAC device */
int oa_tc6_reg_read(struct oa_tc6_desc *, uint32_t, uint32_t *);

/* Read a register asynchronously from the MAC device */
int oa_tc6_reg_read_async(struct oa_tc6_desc *, uint32_t, uint32_t *, uint32_t);

/* Write a register of the MAC device */
int oa_tc6_reg_write(struct oa_tc6_desc *, uint32_t, uint32_t);

/* Write a register asynchronously of the MAC device */
int oa_tc6_reg_write_async(struct oa_tc6_desc *, uint32_t, uint32_t *, uint32_t);

/* Update a register field */
int oa_tc6_reg_update(struct oa_tc6_desc *, uint32_t, uint32_t, uint32_t);

/* Get a received frame with a matching VS field (in the RX chunks) */
int oa_tc6_get_rx_frame_match_vs(struct oa_tc6_desc *,
				 struct oa_tc6_frame_buffer **,
				 uint8_t, uint8_t);

/* Get the first frame in the RX queue */
int oa_tc6_get_rx_frame(struct oa_tc6_desc *, struct oa_tc6_frame_buffer **);

/* Mark the frame buffer as ready to be reused for a new frame. */
int oa_tc6_put_rx_frame(struct oa_tc6_desc *, struct oa_tc6_frame_buffer *);

/* Initiate a data frame SPI transaction via the OA TC6 state machine.*/
int oa_tc6_process_tx_frame(struct oa_tc6_desc *const desc, bool blocking);

/* Get a frame buffer which can be filled and submitted for transmission */
int oa_tc6_get_tx_frame(struct oa_tc6_desc *, struct oa_tc6_frame_buffer **);

/* Mark the frame buffer as filled and ready for transmission */
int oa_tc6_put_tx_frame(struct oa_tc6_desc *, struct oa_tc6_frame_buffer *);

/* Gets the latched transfer flags, and optionally clears the latch */
int oa_tc6_get_xfer_flags(struct oa_tc6_desc *, struct oa_tc6_flags *, bool);

/* OPEN-Alliance IRQ handler. Called from the INT_N interrupt handler */
int oa_tc6_irq_handler (struct oa_tc6_desc *const desc);

/* OPEN-Alliance SPI callback function.Called from the SPI interrupt handler */
void oa_tc6_spi_callback(void *cb_param, uint32_t event, void *arg);

/* Wait until the OA SPI state is READY for 50 ms.*/  
int oa_tc6_wait_spi_ready(struct oa_tc6_desc *const desc);

/* Attempt to acquire the SPI state machine for a new transaction. */
int oa_tc6_wait_get_status(struct oa_tc6_desc *const desc, uint8_t *back_up, bool is_ctrl);

/*
 * Transmit all the frames in the OA_BUFF_TX_READY state and receive the
 * available chunks.
 */
int oa_tc6_thread(struct oa_tc6_desc *);

/* Initialize the OA TC6 SPI driver */
int oa_tc6_init(struct oa_tc6_desc **, struct oa_tc6_init_param *);

/* Free the resources allocated by the oa_tc6_init() function */
int oa_tc6_remove(struct oa_tc6_desc *);

#endif /* _NO_OS_OA_TC6_H */

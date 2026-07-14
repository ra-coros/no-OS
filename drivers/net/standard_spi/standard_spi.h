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
#include "utilities/utilities.h"
#include "utilities/net_queue.h"
#include <stdint.h>

/*! SPI duplex selection. */
#define ADI_MAC_SPI_FULL_DUPLEX             (1)
#define ADI_MAC_SPI_HALF_DUPLEX             (0)
#define ADI_MAC_SPI_TRANSACTION_CONTROL     (1)

/*! SPI header size, in bytes. */
#define ADI_SPI_HEADER_SIZE                (2)

/* 16-bit data used to test CRC Data fill mode */
#define CRC16CCITT_POLYNOMIAL_BE            (0x1021)
#define CRC16CCITT_POLYNOMIAL_LE            (0x8408)

/* 16-bit data used to test CRC Data fill mode */
#define CRC16_SEED_VALUE                    (0xFFFFu)

/* 8-bit data used to test CRC Data fill mode */
#define CRC8CCITT_POLYNOMIAL_BE             (0x07)
#define CRC8CCITT_POLYNOMIAL_LE             (0xe0)

/* 8-bit data used to test CRC Data fill mode */
#define CRC8_SEED_VALUE                     (0xFFu)

/*! Number of buffer bytes in TxFIFO to provide a margin for the frame writes. */
#define ADI_SPI_TX_FIFO_BUFFER              (4)

/*! Size of the SPI device                              */
#define ADI_SPI_DEVICE_SIZE                 (sizeof(adi_adin_spi_device_t))
/*! Size of timestamp in bytes                         */
#define ADI_TIMESTAMP_BYTE_SIZE             (8)

#define ADI_HAL_SPI_READY                  (0x01)

#define SPI_TX_HEADER                       (2)

#define SPI_COMMS_TIMEOUT_MS               (50)


/*! Maximum number of iterations to wait for the SPI transaction to finish. */
#define ADI_SPI_TIMEOUT                     (100000)

/*! SPI register access size in bytes. */
#define ADI_MAC_SPI_ACCESS_SIZE             (4)

/*! Size of data buffer used in SPI transactions, in bytes. */
#define BUFFERSIZE                          (2000)

/*! SPI header value indicating a read transaction.     */
#define ADI_MAC_SPI_READ                    (0)

/*! SPI header value indicating a write transaction.    */
#define ADI_MAC_SPI_WRITE                  (1)

/*!
 * @brief SPI header.
 */
typedef struct
{
    union {
        struct {
             uint16_t   ADDR      : 13; /*!< SPI Register Address.          */
             uint16_t   RW        : 1;  /*!< 0 => Read, 1 => Write          */
             uint16_t   FD        : 1;  /*!< Enable Full Duplex.            */
             uint16_t   CD        : 1;  /*!< 0 => Data, 1 => Control        */
        };
        uint16_t VALUE16;
    };
} eth_mac_header;
typedef struct
{
    void            *pDevMem;           /*!< Pointer to memory area used by the driver.                                                         */
    uint32_t        devMemSize;         /*!< Size of the memory used by the driver. Needs to be greater than or equal to #ADI_MAC_DEVICE_SIZE.  */
    bool            fcs_check_en;         /*!< Configure the driver to check FCS on frame receive to the host. Note this is a check for SPI       */
    void            *app_device_type;      /*!< Configure the driver to choose the ADIN device type            */
    bool            rx_queue_hp_en; /*!< Configure the driver to enable high priority rxQueue           */
    bool            is_crc_enabled;       /*!< Configure the driver to enable CRC in SPI transactions         */   
} standard_spi_init_param;

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


struct  standard_spi_desc  {
	struct capi_spi_device *comm_desc;
	volatile enum standard_spi_buffer_state spi_state;
	uint8_t num_ports;
	bool is_crc_enabled;
	bool rx_queue_hp_en;
	bool fcs_check_en; 
	enum standard_spi_timestamp_format ts_format;
	void *app_device;
	net_queue *tx_queue; /*placeholder */
	net_queue **rx_queue; /*placeholder */
	net_queue *rx_queue_lp; /*placeholder */
	net_queue *rx_queue_hp; /*placeholder */
	bool blocking;
	uint32_t register_address;
	uint8_t *data;
	uint32_t *byte_size;
	bool pending_control;
	volatile uint32_t spi_error;
};



#endif /* _NO_OS_STANDARD_SPI_H */

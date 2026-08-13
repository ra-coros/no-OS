/*******************************************************************************
 *   @file   utilities.h
 *   @brief  Header file for utility functions.
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

#ifndef _UTILITIES_H_
#define _UTILITIES_H_

#include <stdint.h>
#include <stdbool.h>
#include "stm32l4s5xx.h"

#define ADI_FRAME_HEADER_SIZE                    (2)

/*! Peripheral interrupt priorities. */
#define     HAL_INT_PRI_ETH_INT_N               (0x2)
#define     HAL_INT_PRI_DMA_SPI_RX              (0x0)
#define     HAL_INT_PRI_DMA_SPI_TX              (0x0)
#define     HAL_INT_PRI_ETH_SPI_INT             (0x1)
#define     HAL_INT_PRI_UART_TX_RX              (0xF)

/*!
 * @name MAC Callback Status
 * List of MAC callback status bits used ued by the buffer descriptor callback functions.
 */
/** @{ */
/*!
 * Status OK.
 */
#define MAC_CALLBACK_STATUS_OK          (0)
/*!
 * An FCS error was encountered. Note this is caused by validating the FCS on
 * the host MCU to check the integrity of the SPI transaction. The FCS of the
 * incoming frame is checked by the MAC hardware when the frame is received,
 * and has no effect on this status bit.
 */
#define MAC_CALLBACK_STATUS_FCS_ERROR   (1 << 0)
/*!
 * Buffer overflow on receive. The buffer submitted by the application is too small
 * to contain the full frame received.
 */
#define MAC_CALLBACK_STATUS_RX_BUF_OVF  (1 << 1)
/** @} */

struct critical_state {
    uint32_t prev_basepri;
};

static inline uint32_t prio_to_basepri(uint32_t pri_value)
{
    return (pri_value << (8U - __NVIC_PRIO_BITS));  /* left-align */
}

static inline struct critical_state adi_hal_enter_critical_section(uint32_t pri_value)
{
    /*  Save the BASEPRI as previous  */
    struct critical_state prev_pri = { __get_BASEPRI() };

    /* Block interrupt priority of current and LOWER (numerically bigger) */
    __set_BASEPRI(pri_value);
    __DSB(); __ISB();
    return prev_pri;
    /* Protected critical region start here. */
}

static inline void adi_hal_exit_critical_section(struct critical_state prev_pri)
{
    /* Protected critical region ends here.  */
    __set_BASEPRI(prev_pri.prev_basepri);  /* Restore BASEPRI to previous */
    __DSB(); __ISB();
    /* Restore interrupt priority of previous and LOWER (numerically bigger) */
}

/*!
 * @brief RX FIFO priority levels for the MAC interface.
 */
enum mac_rx_fifo_prio
{
    MAC_RX_FIFO_PRIO_LOW = 0,
    MAC_RX_FIFO_PRIO_HIGH,
};

/*!
 * @brief Egress timestamp capture.
 */
enum mac_egress_capture
{
        /** No action. */
        MAC_EGRESS_CAPTURE_NONE = 0,
        /** Capture egress timestamp A. */
        MAC_EGRESS_CAPTURE_A,
        /** Capture egress timestamp B. */
        MAC_EGRESS_CAPTURE_B,
        /** Capture egress timestamp C. */
        MAC_EGRESS_CAPTURE_C,
};

/*!
 * @brief MAC callback events.
 * Driver supports installing callbacks for the events defined here.
 */
enum mac_interrupt_evt
{
	MAC_EVT_LINK_CHANGE = 0,	/*!< Link status changed.                                           */
	MAC_EVT_TX_RDY,			/*!< TX_RDY asserted.                                               */
	MAC_EVT_P1_RX_RDY,		/*!< P1_RX_RDY asserted.                                            */
	MAC_EVT_STATUS,			/*!< Nonzero unmasked status.                                       */
	MAC_EVT_DYN_TBL_UPDATE,		/*!< Dynamic table can be updated.                                  */
	MAC_EVT_RX_FRAME_RDY,		/*!< New frame ready to be read from the Rx FIFO (Generic SPI only).*/
	MAC_EVT_TIMESTAMP_RDY,		/*!< Egress timestamp has been captured in TTSCA register.          */
	MAC_EVT_MAX,			/*!< Enumeration size marker.                                       */
};

/*!
 * @brief Frame header structure.
 */
struct eth_frame_header
{
        union {
                struct {
                        uint16_t PORT               : 1;
                        uint16_t RSVD0              : 1;
                        uint16_t TIME_STAMP_PRESENT : 1;
                        uint16_t TIME_STAMP_PARITY  : 1;
                        uint16_t RSVD1              : 2;
                        uint16_t EGRESS_CAPTURE     : 2;
                        uint16_t RSVD2              : 2;
                        uint16_t PRI                : 1;
                        uint16_t RSVD3              : 5;
                };
        uint16_t VALUE16;
        };
};

/*!
 * @brief Callback function definition for the Ethernet devices.
 * @param cd_param Client-supplied callback parameter.
 * @param event    Event ID specific to the Driver/Service.
 * @param arg      Pointer to the event-specific argument.
 */
typedef void (*eth_callback_t)(void *cd_param, uint32_t event, void *arg);

/*!
 * @brief Status register values and interrupt events.
 */
struct eth_status_registers
{
        /**
        * STATUS0 register value masked to only contain active
        * interrupts via \ref MAC dev.irqMask0.
        */
        uint32_t status0_masked;
        /**
        * STATUS1 register value masked to only contain active
        * interrupts via \ref MAC dev.irqMask1.
        */
        uint32_t status1_masked;
        /** Unmasked STATUS0 register value. */
        uint32_t status0;
        /** Unmasked STATUS1 register value. */
        uint32_t status1;
        /**
        * PHY_SUBSYS_IRQ_STATUS and CRSM_IRQ_STATUS register values
        * (the latter in LSBytes) masked to only contain active
        * interrupts via \ref MAC dev.phyIrqMask.
        */
        uint32_t p1_status_masked;
        /**
        * Unmasked PHY_SUBSYS_IRQ_STATUS and CRSM_IRQ_STATUS
        * register values (the latter in LSBytes).
        */
        uint32_t p1_status;
        /**
        * Port 2 PHY_SUBSYS_IRQ_STATUS and CRSM_IRQ_STATUS register
        * values (the latter in LSBytes) masked to only contain
        * active interrupts via \ref MAC dev.phyIrqMask.
        */
        uint32_t p2_status_masked;
        /**
        * Unmasked Port 2 PHY_SUBSYS_IRQ_STATUS and
        * CRSM_IRQ_STATUS register values (the latter in LSBytes).
        */
        uint32_t p2_status;
};

/*!
 * @brief Buffer descriptor for Tx/Rx.
 */
struct eth_buf_desc
{
        /** Pointer to the frame buffer for Tx or Rx. */
        uint8_t *buf;
        /** Buffer size in bytes, used to check for overflows in Rx. */
        uint32_t buf_size;
        /** Frame size (Tx or Rx), in bytes. */
        uint32_t trx_size;
        /**
        * Callback function. Called after the buffer was written to
        * the Tx FIFO in transmit or read from Rx FIFO on receive.
        */
        eth_callback_t  cb_func;
        /** Indicates the Rx FIFO priority the frame was read from. */
        enum mac_rx_fifo_prio prio;
        /** Port (0/1) on which the frame will transmitted/received. */
        uint32_t port;
        /** Reserved. */
        uint32_t rsvd;
        /** Configure capture of the egress timestamp. */
        enum mac_egress_capture egress_capt;
        /** Indicates if the timestamp field is valid. */
        bool timestamp_valid;
        /** Ingress timestamp received from the MAC. */
        uint32_t timestamp;
        /** Extended storage for 64b ingress timestamp. */
        uint32_t timestamp_ext;
        /**
        * Reference counter, indicates if a descriptor needs to be
        * sent more than once.
        */
        uint32_t ref_count;
};

struct timestamp_rdy {
    bool p1_timestamp_ready_a;
    bool p1_timestamp_ready_b;
    bool p1_timestamp_ready_c;
    bool p2_timestamp_ready_a;
    bool p2_timestamp_ready_b;
    bool p2_timestamp_ready_c;
    bool timestamp_ready_a;
    bool timestamp_ready_b;
    bool timestamp_ready_c;
};

enum eth_link_status {
	ETH_LINK_STATUS_DOWN = 0,
	ETH_LINK_STATUS_UP = 1,
};
/*!
 * @brief Frame structure.
 */
struct eth_frame_struct
{
        struct eth_frame_header header; /*!< Frame header. */
        struct eth_buf_desc *buf_desc; /*!< Pointer to the buffer descriptor. */
};


uint32_t calculate_fcs(uint8_t *buff, uint32_t byte_size);
uint8_t calculate_parity(uint8_t *buff, uint32_t byte_size, uint8_t parity);

#endif /* _UTILITIES_H_ */
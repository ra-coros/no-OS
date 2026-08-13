/***************************************************************************//**
*   @file   eth_mac.h
*   @brief  Implementation of MAC Driver
*   @author Christine Joy Murillo (Christinejoy.Murillo@analog.com)
********************************************************************************
* Copyright 2023-2025(c) Analog Devices, Inc.
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
*******************************************************************************/

/** @addtogroup mac MAC Definitions
 *  @{
 */

#include "capi_irq.h"
#include "stm32_capi_irq.h"
#include "capi_time.h"
#include "no_os_util.h"
#include "utilities.h"
#include "net_queue.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

/* Fallback errno codes not always available on embedded toolchains */
#ifndef EXFULL
#define EXFULL          54
#endif
#ifndef EL2NSYNC
#define EL2NSYNC        45
#endif
#ifndef ENOSYS
#define ENOSYS          38
#endif
#ifndef ENODATA
#define ENODATA         61
#endif
#ifndef ENOEXEC
#define ENOEXEC         8
#endif
#ifndef EBADMSG
#define EBADMSG         74
#endif

#ifndef ETH_MAC_H
#define ETH_MAC_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* ====================================================================================================
        MAC Module Register Address Offset Definitions
   ==================================================================================================== */
#define ADDR_MAC_IDVER                                           (0X000U)    /* Identification Version Register. */
#define ADDR_MAC_PHYID                                           (0X001U)    /* PHY Identification Register. */
#define ADDR_MAC_CAPABILITY                                      (0X002U)    /* Supported Capabilities Register. */
#define ADDR_MAC_RESET                                           (0X003U)    /* Reset Control and Status Register. */
#define ADDR_MAC_CONFIG0                                         (0X004U)    /* Configuration Register 0. */
#define ADDR_MAC_CONFIG2                                         (0X006U)    /* Configuration Register 2. */
#define ADDR_MAC_STATUS0                                         (0X008U)    /* Status Register 0. */
#define ADDR_MAC_STATUS1                                         (0X009U)    /* Status Register 1. */
#define ADDR_MAC_BUFSTS                                          (0X00BU)    /* Buffer Status Register. */
#define ADDR_MAC_IMASK0                                          (0X00CU)    /* Interrupt Mask Register 0. */
#define ADDR_MAC_IMASK1                                          (0X00DU)    /* Mask Bits for Driving the Interrupt Pin Register. */
#define ADDR_MAC_TTSCAH                                          (0X010U)    /* Transmit Timestamp Capture Register A (High). */
#define ADDR_MAC_TTSCAL                                          (0X011U)    /* Transmit Timestamp Capture Register A (Low). */
#define ADDR_MAC_TTSCBH                                          (0X012U)    /* Transmit Timestamp Capture Register B (High). */
#define ADDR_MAC_TTSCBL                                          (0X013U)    /* Transmit Timestamp Capture Register B (Low). */
#define ADDR_MAC_TTSCCH                                          (0X014U)    /* Transmit Timestamp Capture Register C (High). */
#define ADDR_MAC_TTSCCL                                          (0X015U)    /* Transmit Timestamp Capture Register C (Low). */
#define ADDR_MAC_MDIOACC_0_                                      (0X020U)    /* MDIO Access Registers. */
#define ADDR_MAC_MDIOACC_1_                                      (0X021U)    /* MDIO Access Registers. */
#define ADDR_MAC_MDIOACC_2_                                      (0X022U)    /* MDIO Access Registers. */
#define ADDR_MAC_MDIOACC_3_                                      (0X023U)    /* MDIO Access Registers. */
#define ADDR_MAC_MDIOACC_4_                                      (0X024U)    /* MDIO Access Registers. */
#define ADDR_MAC_MDIOACC_5_                                      (0X025U)    /* MDIO Access Registers. */
#define ADDR_MAC_MDIOACC_6_                                      (0X026U)    /* MDIO Access Registers. */
#define ADDR_MAC_MDIOACC_7_                                      (0X027U)    /* MDIO Access Registers. */
#define ADDR_MAC_TX_FSIZE                                        (0X030U)    /* MAC Tx Frame Size Register. */
#define ADDR_MAC_TX                                              (0X031U)    /* MAC Transmit Register. */
#define ADDR_MAC_TX_SPACE                                        (0X032U)    /* Tx FIFO Space Register. */
#define ADDR_MAC_TX_THRESH                                       (0X034U)    /* Transmit Threshold Register. */
#define ADDR_MAC_FIFO_CLR                                        (0X036U)    /* MAC FIFO Clear Register. */
#define ADDR_MAC_SCRATCH_0_                                      (0X037U)    /* Scratch Registers. */
#define ADDR_MAC_SCRATCH_1_                                      (0X038U)    /* Scratch Registers. */
#define ADDR_MAC_SCRATCH_2_                                      (0X039U)    /* Scratch Registers. */
#define ADDR_MAC_SCRATCH_3_                                      (0X03AU)    /* Scratch Registers. */
#define ADDR_MAC_MAC_RST_STATUS                                  (0X03BU)    /* MAC Reset Status. */
#define ADDR_MAC_SOFT_RST                                        (0X03CU)    /* Software Reset Register. */
#define ADDR_MAC_SPI_INJ_ERR                                     (0X03DU)    /* Inject an Error on MISO from the DUT. */
#define ADDR_MAC_FIFO_SIZE                                       (0X03EU)    /* FIFO Sizes Register. */
#define ADDR_MAC_TFC                                             (0X03FU)    /* Tx FIFO Frame Count Register. */
#define ADDR_MAC_TXSIZE                                          (0X040U)    /* Tx FIFO Valid Half Words Register. */
#define ADDR_MAC_HTX_OVF_FRM_CNT                                 (0X041U)    /* Host Tx Frames Dropped Due to FIFO Overflow. */
#define ADDR_MAC_MECC_ERR_ADDR                                   (0X042U)    /* Address of a Detected ECC Error in Memory. */
#define ADDR_MAC_CECC_ERR_0_                                     (0X043U)    /* Corrected ECC Error Counters. */
#define ADDR_MAC_CECC_ERR_1_                                     (0X044U)    /* Corrected ECC Error Counters. */
#define ADDR_MAC_CECC_ERR_2_                                     (0X045U)    /* Corrected ECC Error Counters. */
#define ADDR_MAC_CECC_ERR_3_                                     (0X046U)    /* Corrected ECC Error Counters. */
#define ADDR_MAC_CECC_ERR_4_                                     (0X047U)    /* Corrected ECC Error Counters. */
#define ADDR_MAC_CECC_ERR_5_                                     (0X048U)    /* Corrected ECC Error Counters. */
#define ADDR_MAC_CECC_ERR_6_                                     (0X049U)    /* Corrected ECC Error Counters. */
#define ADDR_MAC_ADDR_FILT_UPR_0_                                (0X050U)    /* MAC Address Rule and DA Filter Upper 16 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_UPR_1_                                (0X052U)    /* MAC Address Rule and DA Filter Upper 16 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_UPR_2_                                (0X054U)    /* MAC Address Rule and DA Filter Upper 16 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_UPR_3_                                (0X056U)    /* MAC Address Rule and DA Filter Upper 16 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_UPR_4_                                (0X058U)    /* MAC Address Rule and DA Filter Upper 16 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_UPR_5_                                (0X05AU)    /* MAC Address Rule and DA Filter Upper 16 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_UPR_6_                                (0X05CU)    /* MAC Address Rule and DA Filter Upper 16 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_UPR_7_                                (0X05EU)    /* MAC Address Rule and DA Filter Upper 16 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_UPR_8_                                (0X060U)    /* MAC Address Rule and DA Filter Upper 16 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_UPR_9_                                (0X062U)    /* MAC Address Rule and DA Filter Upper 16 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_UPR_10_                               (0X064U)    /* MAC Address Rule and DA Filter Upper 16 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_UPR_11_                               (0X066U)    /* MAC Address Rule and DA Filter Upper 16 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_UPR_12_                               (0X068U)    /* MAC Address Rule and DA Filter Upper 16 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_UPR_13_                               (0X06AU)    /* MAC Address Rule and DA Filter Upper 16 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_UPR_14_                               (0X06CU)    /* MAC Address Rule and DA Filter Upper 16 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_UPR_15_                               (0X06EU)    /* MAC Address Rule and DA Filter Upper 16 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_LWR_0_                                (0X051U)    /* MAC Address DA Filter Lower 32 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_LWR_1_                                (0X053U)    /* MAC Address DA Filter Lower 32 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_LWR_2_                                (0X055U)    /* MAC Address DA Filter Lower 32 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_LWR_3_                                (0X057U)    /* MAC Address DA Filter Lower 32 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_LWR_4_                                (0X059U)    /* MAC Address DA Filter Lower 32 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_LWR_5_                                (0X05BU)    /* MAC Address DA Filter Lower 32 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_LWR_6_                                (0X05DU)    /* MAC Address DA Filter Lower 32 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_LWR_7_                                (0X05FU)    /* MAC Address DA Filter Lower 32 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_LWR_8_                                (0X061U)    /* MAC Address DA Filter Lower 32 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_LWR_9_                                (0X063U)    /* MAC Address DA Filter Lower 32 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_LWR_10_                               (0X065U)    /* MAC Address DA Filter Lower 32 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_LWR_11_                               (0X067U)    /* MAC Address DA Filter Lower 32 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_LWR_12_                               (0X069U)    /* MAC Address DA Filter Lower 32 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_LWR_13_                               (0X06BU)    /* MAC Address DA Filter Lower 32 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_LWR_14_                               (0X06DU)    /* MAC Address DA Filter Lower 32 Bits Registers. */
#define ADDR_MAC_ADDR_FILT_LWR_15_                               (0X06FU)    /* MAC Address DA Filter Lower 32 Bits Registers. */
#define ADDR_MAC_ADDR_MSK_UPR_0_                                 (0X070U)    /* Upper 16 Bits of the MAC Address Mask. */
#define ADDR_MAC_ADDR_MSK_UPR_1_                                 (0X072U)    /* Upper 16 Bits of the MAC Address Mask. */
#define ADDR_MAC_ADDR_MSK_LWR_0_                                 (0X071U)    /* Lower 32 Bits of the MAC Address Mask. */
#define ADDR_MAC_ADDR_MSK_LWR_1_                                 (0X073U)    /* Lower 32 Bits of the MAC Address Mask. */
#define ADDR_MAC_TS_ADDEND                                       (0X080U)    /* Timestamp Accumulator Addend Register. */
#define ADDR_MAC_TS_1SEC_CMP                                     (0X081U)    /* Timer Update Compare Register. */
#define ADDR_MAC_TS_SEC_CNT                                      (0X082U)    /* Seconds Counter Register. */
#define ADDR_MAC_TS_NS_CNT                                       (0X083U)    /* Nanoseconds Counter Register. */
#define ADDR_MAC_TS_CFG                                          (0X084U)    /* Timer Configuration Register. */
#define ADDR_MAC_TS_TIMER_HI                                     (0X085U)    /* High Period for TS_TIMER Register. */
#define ADDR_MAC_TS_TIMER_LO                                     (0X086U)    /* Low Period for TS_TIMER Register. */
#define ADDR_MAC_TS_TIMER_QE_CORR                                (0X087U)    /* Quantization Error Correction Register. */
#define ADDR_MAC_TS_TIMER_START                                  (0X088U)    /* TS_TIMER Counter Start Time Register. */
#define ADDR_MAC_TS_EXT_CAPT0                                    (0X089U)    /* TS_CAPT Pin 0 Timestamp Register. */
#define ADDR_MAC_TS_EXT_CAPT1                                    (0X08AU)    /* TS_CAPT Pin 1 Timestamp Register. */
#define ADDR_MAC_TS_FREECNT_CAPT                                 (0X08BU)    /* TS_CAPT Free Running Counter Register. */
#define ADDR_MAC_P1_RX_FSIZE                                     (0X090U)    /* P1 MAC Rx Frame Size Register. */
#define ADDR_MAC_P1_RX                                           (0X091U)    /* P1 MAC Receive Register. */
#define ADDR_MAC_P1_RX_FRM_CNT                                   (0X0A0U)    /* P1 Rx Frame Count Register. */
#define ADDR_MAC_P1_RX_BCAST_CNT                                 (0X0A1U)    /* P1 Rx Broadcast Frame Count Register. */
#define ADDR_MAC_P1_RX_MCAST_CNT                                 (0X0A2U)    /* P1 Rx Multicast Frame Count Register. */
#define ADDR_MAC_P1_RX_UCAST_CNT                                 (0X0A3U)    /* P1 Rx Unicast Frame Count Register. */
#define ADDR_MAC_P1_RX_CRC_ERR_CNT                               (0X0A4U)    /* P1 Rx CRC Errored Frame Count Register. */
#define ADDR_MAC_P1_RX_ALGN_ERR_CNT                              (0X0A5U)    /* P1 Rx Align Error Count Register. */
#define ADDR_MAC_P1_RX_LS_ERR_CNT                                (0X0A6U)    /* P1 Rx Long/Short Frame Error Count Register. */
#define ADDR_MAC_P1_RX_PHY_ERR_CNT                               (0X0A7U)    /* P1 Rx PHY Error Count Register. */
#define ADDR_MAC_P1_TX_FRM_CNT                                   (0X0A8U)    /* P1 Tx Frame Count Register. */
#define ADDR_MAC_P1_TX_BCAST_CNT                                 (0X0A9U)    /* P1 Tx Broadcast Frame Count Register. */
#define ADDR_MAC_P1_TX_MCAST_CNT                                 (0X0AAU)    /* P1 Tx Multicast Frame Count Register. */
#define ADDR_MAC_P1_TX_UCAST_CNT                                 (0X0ABU)    /* P1 Tx Unicast Frame Count Register. */
#define ADDR_MAC_P1_RX_DROP_FULL_CNT                             (0X0ACU)    /* P1 Rx Frames Dropped Due to FIFO Full Register. */
#define ADDR_MAC_P1_RX_DROP_FILT_CNT                             (0X0ADU)    /* P1 Rx Frames Dropped Due to Filtering Register. */
#define ADDR_MAC_P1_RX_IFG_ERR_CNT                               (0X0AEU)    /* Frame Received on Port 1 with IFG Errors. */
#define ADDR_MAC_P1_TX_IFG                                       (0X0B0U)    /* P1 Transmit Inter Frame Gap Register. */
#define ADDR_MAC_P1_LOOP                                         (0X0B3U)    /* P1 MAC Loopback Enable Register. */
#define ADDR_MAC_P1_RX_CRC_EN                                    (0X0B4U)    /* P1 CRC Check Enable on Receive Register. */
#define ADDR_MAC_P1_RX_IFG                                       (0X0B5U)    /* P1 Receive Inter Frame Gap Register. */
#define ADDR_MAC_P1_RX_MAX_LEN                                   (0X0B6U)    /* P1 Max Receive Frame Length Register. */
#define ADDR_MAC_P1_RX_MIN_LEN                                   (0X0B7U)    /* P1 Min Receive Frame Length Register. */
#define ADDR_MAC_P1_LO_RFC                                       (0X0B8U)    /* P1 Rx Low Priority FIFO Frame Count Register. */
#define ADDR_MAC_P1_HI_RFC                                       (0X0B9U)    /* P1 Rx High Priority FIFO Frame Count Register. */
#define ADDR_MAC_P1_LO_RXSIZE                                    (0X0BAU)    /* P1 Low Priority Rx FIFO Valid Half Words Register. */
#define ADDR_MAC_P1_HI_RXSIZE                                    (0X0BBU)    /* P1 High Priority Rx FIFO Valid Half Words Register. */

    
/* ----------------------------------------------------------------------------------------------------
          FIFO_CLR                                              Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_FIFO_CLR_MAC_RXF_CLR                         (0U)           /* Clear the Receive FIFO(s). */
#define BITL_MAC_FIFO_CLR_MAC_RXF_CLR                         (1U)           /* Clear the Receive FIFO(s). */
#define BITM_MAC_FIFO_CLR_MAC_RXF_CLR                         (0X00000001U)  /* Clear the Receive FIFO(s). */
#define BITP_MAC_FIFO_CLR_MAC_TXF_CLR                         (1U)           /* Clear the Host Transmit FIFO. */
#define BITL_MAC_FIFO_CLR_MAC_TXF_CLR                         (1U)           /* Clear the Host Transmit FIFO. */
#define BITM_MAC_FIFO_CLR_MAC_TXF_CLR                         (0X00000002U)  /* Clear the Host Transmit FIFO. */
#define BITP_MAC_FIFO_CLR_LES_FIFOS_CLR                       (2U)           /* Clear All FIFOs. */
#define BITL_MAC_FIFO_CLR_LES_FIFOS_CLR                       (1U)           /* Clear All FIFOs. */
#define BITM_MAC_FIFO_CLR_LES_FIFOS_CLR                       (0X00000004U)  /* Clear All FIFOs. */
#define BITP_MAC_FIFO_CLR_LES_P2P_FIFOS_CLR                   (3U)           /* Clear the Port to Port FIFOs on LES. */
#define BITL_MAC_FIFO_CLR_LES_P2P_FIFOS_CLR                   (1U)           /* Clear the Port to Port FIFOs on LES. */
#define BITM_MAC_FIFO_CLR_LES_P2P_FIFOS_CLR                   (0X00000008U)  /* Clear the Port to Port FIFOs on LES. */

#define BITM_MAC_STATUS1_P2_PHYINT                            (0X00080000U)  /* PHY Interrupt for Port2. */

#define BITP_MAC_STATUS1_P1_LINK_STATUS                       (0U)           /* Port 1 Link Status. */
#define BITL_MAC_STATUS1_P1_LINK_STATUS                       (1U)           /* Port 1 Link Status. */
#define BITM_MAC_STATUS1_P1_LINK_STATUS                       (0X00000001U)  /* Port 1 Link Status. */
#define BITP_MAC_STATUS1_LINK_CHANGE                          (1U)           /* Link Status Changed. */
#define BITL_MAC_STATUS1_LINK_CHANGE                          (1U)           /* Link Status Changed. */
#define BITM_MAC_STATUS1_LINK_CHANGE                          (0X00000002U)  /* Link Status Changed. */

#define BITM_MAC_IMASK1_P2_TXFCSEM                            (0X01000000U)  /* Transmit Frame Check Sequence Error Mask for P2_TXFCSE. */
#define BITM_MAC_IMASK1_P2_RX_RDY_MASK                        (0X00020000U)  /* Mask Bit for P2_RX_RDY. */
#define BITM_MAC_IMASK1_P2_TTSCAAM                            (0X00100000U)  /* Transmit Timestamp Capture Available A Mask. */
#define BITM_MAC_IMASK1_P2_TTSCABM                            (0X00200000U)  /* Transmit Timestamp Capture Available B Mask. */
#define BITM_MAC_IMASK1_P2_TTSCACM                            (0X00400000U)  /* Transmit Timestamp Capture Available C Mask. */

#define ADDR_MAC_P2_RX_FSIZE                                  (0X0C0U)       /* P2 MAC Rx Frame Size Register. */
#define ADDR_MAC_P2_RX                                        (0X0C1U)       /* P2 MAC Receive Register. */
#define ADDR_MAC_P2_RX_FRM_CNT                                (0X0D0U)       /* P2 Rx Frame Count Register. */

#define BITM_MAC_IMASK1_P2_PHYINT_MASK                        (0X00080000U)  /* Mask Bit for P2_PHYINT. */
#define BITP_MAC_CONFIG2_PORT_CUT_THRU_EN                     (11U)          /* Enable Cut Through from Port to Port. */
#define BITM_MAC_CONFIG2_PORT_CUT_THRU_EN                     (0X00000800U)  /* Enable Cut Through from Port to Port. */
#define BITP_MAC_FIFO_SIZE_P2_RX_LO_SIZE                      (12U)          /* Port 2 Rx Low Priority FIFO Size. */
#define BITP_MAC_FIFO_SIZE_P2_RX_HI_SIZE                      (16U)          /* Port 2 Rx High Priority FIFO Size. */
#define BITP_MAC_FIFO_SIZE_P1_TX_SIZE                         (20U)          /* Port 2 to Port 1 Transmit FIFO Size. */
#define BITP_MAC_FIFO_SIZE_P2_TX_SIZE                         (24U)          /* Port 1 to Port 2 Transmit FIFO Size. */
#define BITM_MAC_FIFO_SIZE_P2_RX_LO_SIZE                      (0X0000F000U)  /* Port 2 Rx Low Priority FIFO Size. */
#define BITM_MAC_FIFO_SIZE_P2_RX_HI_SIZE                      (0X000F0000U)  /* Port 2 Rx High Priority FIFO Size. */
#define BITM_MAC_FIFO_SIZE_P1_TX_SIZE                         (0X00F00000U)  /* Port 2 to Port 1 Transmit FIFO Size. */
#define BITM_MAC_FIFO_SIZE_P2_TX_SIZE                         (0X0F000000U)  /* Port 1 to Port 2 Transmit FIFO Size. */

    
    
#define BITM_MAC_IMASK1_P2_PHYINT_MASK                        (0X00080000U)  /* Mask Bit for P2_PHYINT. */
#define BITP_MAC_CONFIG2_PORT_CUT_THRU_EN                     (11U)          /* Enable Cut Through from Port to Port. */
#define BITM_MAC_CONFIG2_PORT_CUT_THRU_EN                     (0X00000800U)  /* Enable Cut Through from Port to Port. */
#define BITP_MAC_FIFO_SIZE_P2_RX_LO_SIZE                      (12U)          /* Port 2 Rx Low Priority FIFO Size. */
#define BITP_MAC_FIFO_SIZE_P2_RX_HI_SIZE                      (16U)          /* Port 2 Rx High Priority FIFO Size. */
#define BITP_MAC_FIFO_SIZE_P1_TX_SIZE                         (20U)          /* Port 2 to Port 1 Transmit FIFO Size. */
#define BITP_MAC_FIFO_SIZE_P2_TX_SIZE                         (24U)          /* Port 1 to Port 2 Transmit FIFO Size. */
#define BITM_MAC_FIFO_SIZE_P2_RX_LO_SIZE                      (0X0000F000U)  /* Port 2 Rx Low Priority FIFO Size. */
#define BITM_MAC_FIFO_SIZE_P2_RX_HI_SIZE                      (0X000F0000U)  /* Port 2 Rx High Priority FIFO Size. */
#define BITM_MAC_FIFO_SIZE_P1_TX_SIZE                         (0X00F00000U)  /* Port 2 to Port 1 Transmit FIFO Size. */
#define BITM_MAC_FIFO_SIZE_P2_TX_SIZE                         (0X0F000000U)  /* Port 1 to Port 2 Transmit FIFO Size. */

#define BITM_MAC_CONFIG2_P2_FWD_UNK2P1                        (0x0)         /* Forward Frames from Port2 Not Matching a MAC Addr to Port1. */
#define BITM_MAC_CONFIG2_P1_FWD_UNK2P2                        (0x0)         /* Forward Frames from Port1 Not Matching a MAC Addr to Port2. */
#define BITM_MAC_CONFIG2_P2_FWD_UNK2HOST                      (0x0)         /* Forward Frames Not Matching Any MAC Address to the Host. */
 
#define BITP_MAC_CONFIG2_P2_FWD_UNK2P1                        (0x0)         /* Forward Frames from Port2 Not Matching a MAC Addr to Port1. */
#define BITP_MAC_CONFIG2_P1_FWD_UNK2P2                        (0x0)         /* Forward Frames from Port1 Not Matching a MAC Addr to Port2. */
#define BITP_MAC_CONFIG2_P2_FWD_UNK2HOST                      (0x0)         /* Forward Frames Not Matching Any MAC Address to the Host. */
#define ADDR_MAC_P2_RX_CRC_EN                                 (0x0)

/* eth_callback_t is defined in utilities.h */

/* enum eth_mac_result is defined in utilities.h */


/*!
 * @brief ADIN device type
 */
enum eth_devices {
    ADIN_1110 = 0,    /*!< Device is ADIN1110 */
    ADIN_2111,        /*!< Device is ADIN2111 */
};


/*!
 * @brief Driver states
 * States can be used in both generic SPI and OPEN Alliance SPI, or for just one protocol.
 */
enum eth_mac_state{
    ETH_MAC_STATE_UNINITIALIZED         = 0,    /*!< Driver is not initialized. This is the initial state.  */
    ETH_MAC_STATE_INITIALIZED,                  /*!< Driver initialized.                                    */
    ETH_MAC_STATE_READY,                        /*!< Driver ready.                                          */
    ETH_MAC_STATE_TX_FRAME,                     /*!< Driver is transmitting a frame to the MAC over SPI.    */
    ETH_MAC_STATE_RX_FRAME,                     /*!< Driver is receiving a frame from the MAC over SPI.     */
    ETH_MAC_STATE_CONTROL_START,                /*!< Driver is starting a control transaction.              */
    ETH_MAC_STATE_CONTROL_END,                  /*!< Driver is completing a control transaction.            */
    ETH_MAC_STATE_DATA_START,                   /*!< Driver is starting a data transaction.                 */
    ETH_MAC_STATE_DATA_END,                     /*!< Driver is completing a data transaction.               */
    ETH_MAC_STATE_IRQ_START,                    /*!< Driver is servicing an interrupt request from the MAC. */
    ETH_MAC_STATE_DATA_READ_STATUS,             /*!< Driver is reading status from MAC registers.           */
    ETH_MAC_STATE_DATA_READ_PHY_REGISTER,
};

/* Type aliases for legacy code in eth_mac.c */
typedef enum eth_mac_state eth_mac_State_e;
typedef struct critical_state critical_state_t;

/* Struct tag aliases so "struct eth_mac_status0" resolves to "struct _ETH_MAC_STATUS0_t" */
#define eth_mac_status0 _ETH_MAC_STATUS0_t
#define eth_mac_status1 _ETH_MAC_STATUS1_t

/* ====================================================================================================
   Compatibility macros for baremetal code migration
   ==================================================================================================== */

/* Feature flags */
#define ADI_PAUSE_RX_IF_NO_BUFFERS      0
#define ADI_EDGE_SENSITIVE_IRQ          1

/* Queue function aliases (net_queue API) */
#define queue_init(q, e, n)     net_queue_init(q, e, n)
#define queue_isEmpty(q)        net_queue_is_empty(q)
#define queue_isFull(q)         net_queue_is_full(q)
#define queue_addEntry(q)       net_queue_add_entry(q)
#define queue_removeEntry(q)    net_queue_remove_entry(q)

/* OA TC6 constants */
#ifndef ADI_OA_MAX_CHUNK_COUNT
#define ADI_OA_MAX_CHUNK_COUNT          31
#endif
#ifndef ADI_OA_MAX_CHUNK64_COUNT
#define ADI_OA_MAX_CHUNK64_COUNT        31
#endif
#ifndef ADI_OA_DEFAULT_CPS
#define ADI_OA_DEFAULT_CPS              64
#endif

/* Register address extraction macro */
#ifndef REGADDR
#define REGADDR(x)      ((uint16_t)(x))
#endif

/* COMM_INTERFACE_STARTUP_TIME_MS - wait time for MAC communication interface */
#ifndef COMM_INTERFACE_STARTUP_TIME_MS
#define COMM_INTERFACE_STARTUP_TIME_MS  500U
#endif

/* DMA SPI interrupt priority for critical sections (uses utilities.h values) */
#ifndef ADI_HAL_INT_PRI_DMA_SPI_TX
#define ADI_HAL_INT_PRI_DMA_SPI_TX      prio_to_basepri(HAL_INT_PRI_DMA_SPI_TX)
#endif
#ifndef ADI_HAL_INT_PRI_DMA_SPI_RX
#define ADI_HAL_INT_PRI_DMA_SPI_RX      prio_to_basepri(HAL_INT_PRI_DMA_SPI_RX)
#endif

/* PHY register addresses used by MAC callback to clear PHY interrupts */
#ifndef ADDR_CRSM_IRQ_STATUS
#define ADDR_CRSM_IRQ_STATUS            (0X1E0010U)
#endif
#ifndef ADDR_PHY_SUBSYS_IRQ_STATUS
#define ADDR_PHY_SUBSYS_IRQ_STATUS      (0X1F0011U)
#endif

/* Error code aliases mapping ETH_MAC_* to ETH_* enum values in utilities.h */
#define ETH_MAC_DEVICE_UNINITIALIZED    ETH_DEVICE_UNINITIALIZED
#define ETH_MAC_COMM_TIMEOUT            ETH_COMM_TIMEOUT
#define ETH_MAC_PLACEHOLDER_ERROR       ETH_PLACEHOLDER_ERROR
#define ETH_MAC_PARAM_OUT_OF_RANGE      ETH_PARAM_OUT_OF_RANGE
#define ETH_MAC_QUEUE_FULL              ETH_QUEUE_FULL
#define ETH_MAC_COMM_BUSY               ETH_COMM_BUSY
#define ETH_MAC_NO_TS_FORMAT            ETH_NO_TS_FORMAT

/* HAL_GetTick replacement using CAPI uptime */
//static inline uint32_t _eth_mac_get_tick_ms(void)
//{
//    uint64_t us;
//    capi_uptime(&us);
//    return (uint32_t)(us / 1000U);
//}
//#define HAL_GetTick()   _eth_mac_get_tick_ms()

/* struct eth_frame_header is defined in utilities.h */


/*!
 * @brief Timestamp formats.
 */
enum eth_mac_ts_format {
    ETH_MAC_TS_FORMAT_NONE      = 0,    /*!< No timestamp                                                                       */
    ETH_MAC_TS_FORMAT_32B_FREE,         /*!< 32b free-running counter: 1 lsb = 1/120MHz = 8.33ns.                               */
    ETH_MAC_TS_FORMAT_32B_1588,         /*!< 32b 1588 timestamp: [31:30] seconds count, [29:0] nanoseconds count.               */
    ETH_MAC_TS_FORMAT_64B_1588,         /*!< 64b 1588 timestamp: [63:32] seconds count, [31:30] zero, [29:0] nanoseconds count. */
};


/* enum mac_egress_capture is defined in utilities.h */


/* enum mac_rx_fifo_prio is defined in utilities.h */


/* struct eth_buf_desc is defined in utilities.h */

/* struct eth_frame_struct is defined in utilities.h */


/*! Maximum number of MAC addresses allowed by the MAC device. */
#define ETH_MAC_ADDR_NUM_ENTRIES            (16)

/*! Size of the Tx queue, can be previously defined by the application. */
#if !defined(TX_QUEUE_NUM_ENTRIES)
    #define TX_QUEUE_NUM_ENTRIES            (4)
#endif

/*! Size of the Rx queue, can be previously defined by the application. */
#if !defined(RX_QUEUE_NUM_ENTRIES)
    #define RX_QUEUE_NUM_ENTRIES            (4)
#endif

/*! Actual size of the Rx queue. */
#define RX_QUEUE_NUM_ENTRIES_RAW            (RX_QUEUE_NUM_ENTRIES + 1) 

/*! Actual Size of the Tx queue. */
#define TX_QUEUE_NUM_ENTRIES_RAW            (TX_QUEUE_NUM_ENTRIES + 1)

/*! Size of the MAC device structure, in bytes. Needs to be a multiple of 4.
 Note the actual size will depend on packing of non-uint32_t members.*/
#define ETH_MAC_DEVICE_SIZE                 (sizeof(struct eth_mac_desc)) 

/*! MAC address filter table entries that allow usage of a mask. */
#define ETH_MAC_ADDR_MASK_MAX               (2)

/*! SPI header value indicating a data transaction.     */
#define ETH_MAC_SPI_TRANSACTION_DATA        (0)

/*! SPI header value indicating a control transaction.  */
#define ETH_MAC_SPI_TRANSACTION_CONTROL     (1)

/*! SPI header value indicating a read transaction.     */
#define ETH_MAC_SPI_READ                    (0)

/*! SPI header value indicating a write transaction.    */
#define ETH_MAC_SPI_WRITE                   (1)

/*! SPI register access size in bytes.      */
/* Access size can be either 2 or 4 bytes   */
/* For Silicon 1 : 2 bytes                  */
/* For New Silicon : 4 bytes                */
#define ETH_MAC_SPI_ACCESS_SIZE             (4)

/*! Type of SPI register access size. */
#define ETH_MAC_SPI_ACCESS_UNIT_TYPE        uint32_t

/*! Number of MDIO Access registers. */
#define ETH_MAC_MDIO_MAX_COUNT              (8)

/*! Number of MDIO Access registers to use in parallel. */
#define ETH_MAC_MDIO_PARALLEL_COUNT         (2)

/*! Maximum size of all FIFOs combined, in kBytes. */
#define ETH_MAC_FIFO_MAX_SIZE               (28)

/*! Maximum number of attempts to read MDIO RDY bit. */
#define ETH_MAC_MDIO_MAX_RETRIES            (10)

/*! Size of the buffers used in control transactions, in bytes. */
#define ETH_MAC_SPI_CTRL_BUF_SIZE           (256)
     
/* MAC reset keys */
/*! Key 1 for MAC-only reset. */
#define RST_MAC_ONLY_KEY1                   (0x4F1C)

/*! Key 2 for MAC-only reset. */
#define RST_MAC_ONLY_KEY2                   (0xC1F4)

#define MAC_RESET_KEY_COUNT                 (2)
#define RST_KEY1_IDX                        (0)
#define RST_KEY2_IDX                        (1)
    
/*! Maximum number of attempts to establish the SPI interface is up and responsive. */
#define ETH_MAC_IF_UP_MAX_RETRIES           (25000)

/*! Maximum number of attempts to establish the read the expected value of PHYID register during the initialization. */
/*  This is based on a wait time of 50ms and a SPI read of 2us.                                                      */
#define ETH_MAC_INIT_MAX_RETRIES            (25000)

/*! Minimum frame size, without FCS. */
#define MIN_FRAME_SIZE                      (60)

/*! Maximum frame size, without FCS. */
#define MAX_FRAME_SIZE                      (1518)

/*! FCS size */
#ifndef FCS_SIZE
#define FCS_SIZE                            (4)
#endif

/*! Number of buffer bytes in TxFIFO to provide a margin for the frame writes. */
#define ETH_MAC_TX_FIFO_BUFFER              (4)
    
/*! Frame header size, in bytes. */
#define ETH_FRAME_HEADER_SIZE               (2)
    
/*! Maximum number of chunks that will be packed into a single SPI transaction */
#define ETH_OA_MAX_CHUNK_COUNT              (31)
#define ETH_OA_MAX_CHUNK64_COUNT            (16)

/* Default oaCps value during initialization */
#define ETH_OA_DEFAULT_CPS                  (6)
    
/*! Receive backup buffer size, in chunks. Note this is not currently used! */
#define ETH_OA_RX_BACKUP_BUF_CHUNK_COUNT    (1)

/*! Receive backup buffer size, in bytes. Note this is not currently used! */
#define ETH_OA_RX_BACKUP_BUF_SIZE           (68 * ETH_OA_RX_BACKUP_BUF_CHUNK_COUNT)

/*! Initialization value for status variables corresponding to PHY IRQ registers. */
/*  This value cannot be achieved in hardware, also different from 0xFFFFFFFF which can indicate an MDIO error. */
#define ETH_MAC_PHY_STATUS_INIT_VAL         (0x7FFF7FFF)

/*! Mask for timestamp timer bits that are disregarded through quantization. */
#define ETH_MAC_TS_QE_MASK                  (0xF)

/*! Resolution in nanoseconds of the timestamp timer. */
#define ETH_MAC_TS_MIN_NS                   (16)

/*! Number of nanoseconds in a second. */
#define ETH_MAC_TS_ONE_SECOND_IN_NS         (1000000000ULL)

/*! Mask for the nanoseconds portion of a 32b/64b 1588 timestamp value. */
#define ETH_MAC_TS_1588_NS_MASK             (0x3FFFFFFF)

/*! Mask for the seconds portion of a 32b 1588 timestamp value. */
#define ETH_MAC_TS_1588_SEC_MASK            (0xC0000000)

/*! Bit position  for the seconds portion of a 32b 1588 timestamp value. */
#define ETH_MAC_TS_1588_SEC_POS             (28)

/*! Nominal resolution of free-running counter in nanoseconds. */
#define ETH_MAC_TS_FREE_CNT_LSB_IN_NS       (100.0/12.0)

/*! Byte order conversion for 16-bit values. */
#define HTON16(a)                           ((uint16_t)((((a) & 0xFF) << 8) | (((a) >> 8) & 0xFF)))

/*! Byte order conversion for 32-bit values. */
#define HTON32(a)                           __REV((a))

/*! MDIO Device Address extraction from 32-bit PHY register address. */
#define DEV_TYPE(a)                          (a >> 16)//no_os_shift_right(a, 16)

/*! MDIO Register Address extraction from 32-bit PHY register address. */
#define reg_addr(a)                          (a & 0xFFFF)


/* struct timestamp_rdy is defined in utilities.h */

/*!
 * @brief MAC address rule definition.
 */
struct eth_mac_addr_rule {
    union {
        struct {
             unsigned int TO_HOST          : 1; /**< Forward Frames Matching This MAC Address to the Host. */
             unsigned int TO_OTHER_PORT    : 1; /**< Forward Frames Matching This MAC Address to the Other Port. */
             unsigned int RESERVED2        : 1; /**< Reserved */
             unsigned int HOST_PRI         : 1; /**< Host Rx Port Priority */
             unsigned int RESERVED4        : 10; /**< Reserved */
             unsigned int APPLY2PORT1      : 1; /**< Apply to Port 1. */
             unsigned int APPLY2PORT2      : 1; /**< Apply to Port 2. */
        };
        uint16_t VALUE16;
    };
};

/*!
 * @brief MAC callback events.
 * Driver supports installing callbacks for the events defined here.
 */
enum eth_mac_event {
    ETH_MAC_EVT_LINK_CHANGE         = 0,    /*!< Link status changed.                                           */
    ETH_MAC_EVT_TX_RDY,                     /*!< TX_RDY asserted.                                               */
    ETH_MAC_EVT_P1_RX_RDY,                  /*!< P1_RX_RDY asserted.                                            */
    ETH_MAC_EVT_STATUS,                     /*!< Nonzero unmasked status.                                       */
    ETH_MAC_EVT_DYN_TBL_UPDATE,             /*!< Dynamic table can be updated.                                  */
    ETH_MAC_EVT_RX_FRAME_RDY,               /*!< New frame ready to be read from the Rx FIFO (Generic SPI only).*/
    ETH_MAC_EVT_TIMESTAMP_RDY,              /*!< Egress timestamp has been captured in TTSCA register.          */
    ETH_MAC_EVT_MAX,                        /*!< Enumeration size marker.                                       */
};

/*!
 * @brief MAC statistics counters structure.
 */
struct eth_mac_stat_counter {
    uint32_t RX_FRM_CNT;                    /*!< Rx frame count.                     */
    uint32_t RX_BCAST_CNT;                  /*!< Rx unicast frame count.             */
    uint32_t RX_MCAST_CNT;                  /*!< Rx multicast frame count.           */
    uint32_t RX_UCAST_CNT;                  /*!< Rx broadcast frame count.           */
    uint32_t RX_CRC_ERR_CNT;                /*!< Rx CRC errored frame count.         */
    uint32_t RX_ALGN_ERR_CNT;               /*!< Rx alignment error count.           */
    uint32_t RX_LS_ERR_CNT;                 /*!< Rx long/Short error count.          */
    uint32_t RX_PHY_ERR_CNT;                /*!< Rx PHY error (MII RX_ER).           */
    uint32_t TX_FRM_CNT;                    /*!< Tx frame count.                     */
    uint32_t TX_BCAST_CNT;                  /*!< Tx unicast frame count.             */
    uint32_t TX_MCAST_CNT;                  /*!< Tx multicast frame count.           */
    uint32_t TX_UCAST_CNT;                  /*!< Tx broadcast frame count.           */
    uint32_t RX_DROP_FULL_CNT;              /*!< Rx dropped due to FIFO full count.  */
    uint32_t RX_DROP_FILT_CNT;              /*!< Rx dropped due to filtering count.  */
};

/*!
 * @brief Link status read from the MAC interface  (MAC_STATUS register).
 */
enum eth_mac_link_status {
    ETH_MAC_LINK_STATUS_DOWN        = (0),  /*!< Link down.  */
    ETH_MAC_LINK_STATUS_UP          = (1),  /*!< Link up.    */
};

/*!
 * @brief Reset definitions.
 */
enum eth_mac_reset_type {
    ETH_MAC_RESET_TYPE_MAC_ONLY     = 0,    /*!< Reset the MAC only.          */
    ETH_MAC_RESET_TYPE_MAC_PHY,             /*!< Reset both MAC and PHY.      */
};

/* --- Register field enums (moved here for forward reference) --- */

typedef enum
{
     MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_0K = 0,
     MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_2K = 1,
     MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_4K = 2,
     MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_6K = 3,
     MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_8K = 4,
     MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_10K = 5,
     MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_12K = 6,
     MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_14K = 7,
     MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_16K = 8
}  ETH_MAC_FIFO_SIZE_P1_RX_HI_SIZE;

typedef enum
{
     MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_0K = 0,
     MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_2K = 1,
     MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_4K = 2,
     MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_6K = 3,
     MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_8K = 4,
     MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_10K = 5,
     MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_12K = 6,
     MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_14K = 7,
     MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_16K = 8
}  ETH_MAC_FIFO_SIZE_HTX_SIZE;

typedef enum
{
     MAC_CONFIG0_CPS_CHUNK_8BYTE      = 3,
     MAC_CONFIG0_CPS_CHUNK_16BYTE     = 4,
     MAC_CONFIG0_CPS_CHUNK_32BYTE     = 5,
     MAC_CONFIG0_CPS_CHUNK_64BYTE     = 6
}  ETH_MAC_CONFIG0_CPS;

#define BITM_MAC_FIFO_CLR_MAC_RXF_CLR                         (0X00000001U)
#define BITM_MAC_FIFO_CLR_MAC_TXF_CLR                         (0X00000002U)
#define BITM_MAC_FIFO_CLR_LES_FIFOS_CLR                       (0X00000004U)
#define BITM_MAC_FIFO_CLR_LES_P2P_FIFOS_CLR                   (0X00000008U)

/* --- End register field enums --- */

/*!
 * @brief Receive FIFO size definitions.
 */
enum eth_mac_rx_fifo_size {
    ETH_MAC_RX_FIFO_SIZE_2K  = (MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_2K),
    ETH_MAC_RX_FIFO_SIZE_0K  = (MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_0K),
    ETH_MAC_RX_FIFO_SIZE_4K  = (MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_4K),
    ETH_MAC_RX_FIFO_SIZE_6K  = (MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_6K),
    ETH_MAC_RX_FIFO_SIZE_8K  = (MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_8K),
    ETH_MAC_RX_FIFO_SIZE_10K = (MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_10K),
    ETH_MAC_RX_FIFO_SIZE_12K = (MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_12K),
    ETH_MAC_RX_FIFO_SIZE_14K = (MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_14K),
    ETH_MAC_RX_FIFO_SIZE_16K = (MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_16K),
};

/*!
 * @brief Host transmit FIFO size definitions.
 */
enum eth_mac_htx_fifo_size {
    ETH_MAC_HTX_FIFO_SIZE_0K  = (MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_0K),
    ETH_MAC_HTX_FIFO_SIZE_2K  = (MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_2K),
    ETH_MAC_HTX_FIFO_SIZE_4K  = (MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_4K),
    ETH_MAC_HTX_FIFO_SIZE_6K  = (MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_6K),
    ETH_MAC_HTX_FIFO_SIZE_8K  = (MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_8K),
    ETH_MAC_HTX_FIFO_SIZE_10K = (MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_10K),
    ETH_MAC_HTX_FIFO_SIZE_12K = (MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_12K),
    ETH_MAC_HTX_FIFO_SIZE_14K = (MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_14K),
    ETH_MAC_HTX_FIFO_SIZE_16K = (MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_16K),
};


/*!
 * @brief Port transmit FIFO size definitions.
 */
enum eth_mac_ptx_fifo_size {
    ETH_MAC_PTX_FIFO_SIZE_0K  = 0,
    ETH_MAC_PTX_FIFO_SIZE_2K  = 1,
    ETH_MAC_PTX_FIFO_SIZE_4K  = 2,
    ETH_MAC_PTX_FIFO_SIZE_6K  = 3,
    ETH_MAC_PTX_FIFO_SIZE_8K  = 4,
    ETH_MAC_PTX_FIFO_SIZE_10K = 5,
    ETH_MAC_PTX_FIFO_SIZE_12K = 6,
    ETH_MAC_PTX_FIFO_SIZE_14K = 7,
    ETH_MAC_PTX_FIFO_SIZE_16K = 8,
};


/*!
 * @brief FIFO size configuration.
 */
struct eth_mac_fifo_size {
    enum eth_mac_htx_fifo_size  htxSize;        /*!< Size of the host Tx FIFO. */
    enum eth_mac_rx_fifo_size   p1RxLoSize;     /*!< Size of the Port 1 low-priority Rx FIFO. */
    enum eth_mac_rx_fifo_size   p1RxHiSize;     /*!< Size of the Port 1 high-priority Rx FIFO. */
    enum eth_mac_rx_fifo_size   p2RxLoSize;     /*!< Size of the Port 2 low-priority Rx FIFO. */
    enum eth_mac_rx_fifo_size   p2RxHiSize;     /*!< Size of the Port 2 high-priority Rx FIFO. */
    enum eth_mac_ptx_fifo_size  p1TxSize;       /*!< Size of the Port 2 to Port 1 Tx FIFO. */
    enum eth_mac_ptx_fifo_size  p2TxSize;       /*!< Size of the Port 1 to Port 2 Tx FIFO. */
    enum eth_mac_htx_fifo_size  txSize;         /*!< Size of the Tx FIFO. */
    enum eth_mac_rx_fifo_size   rxLoSize;       /*!< Size of the low-priority Rx FIFO. */
    enum eth_mac_rx_fifo_size   rxHiSize;       /*!< Size of the high-priority Rx FIFO. */
};

/*!
 * @brief FIFO clear mode.
 */
enum eth_mac_fifo_clr_mode {
    ETH_MAC_CLEAR_RX_FIFO       = (BITM_MAC_FIFO_CLR_MAC_RXF_CLR),
    ETH_MAC_CLEAR_TX_FIFO       = (BITM_MAC_FIFO_CLR_MAC_TXF_CLR),
    ETH_MAC_CLEAR_ALL_FIFOS     = (BITM_MAC_FIFO_CLR_LES_FIFOS_CLR),
    ETH_MAC_CLEAR_P2P_FIFOS     = (BITM_MAC_FIFO_CLR_LES_P2P_FIFOS_CLR),
};


/*!
 * @brief Configuration options for TS_TIMER waveform.
 */
struct eth_mac_ts_timer_cfg {
    uint32_t        periodNs;         /*!< Timer waveform period in nanoseconds.                        */
    float           dutyCycle;        /*!< Timer waveform duty cycle. Set to 0.5 for a 50% duty cycle.  */
    bool            idleState;        /*!< Timer waveform idle state. False is logic low.               */
    uint32_t        startTimeNs;      /*!< Timer waveform start time in nanoseconds, must be >=16.      */
};

/*!
 * @brief Structure to hold time from parsed timestamps.
 */
struct eth_mac_ts_timespec {
    uint32_t    sec;    /*!< Number of whole seconds. */
    uint32_t    nsec;   /*!< Number of nanoseconds. */
};

/*!
 * @brief Chunk Payload Selector (OPEN Alliance).
 */
enum eth_mac_oa_cps {
    ETH_MAC_OA_CPS_8BYTE  = (MAC_CONFIG0_CPS_CHUNK_8BYTE),     /*!< 8 bytes per chunk  */
    ETH_MAC_OA_CPS_16BYTE = (MAC_CONFIG0_CPS_CHUNK_16BYTE),    /*!< 16 bytes per chunk */
    ETH_MAC_OA_CPS_32BYTE = (MAC_CONFIG0_CPS_CHUNK_32BYTE),    /*!< 32 bytes per chunk */
    ETH_MAC_OA_CPS_64BYTE = (MAC_CONFIG0_CPS_CHUNK_64BYTE),    /*!< 64 bytes per chunk */
};

/* struct eth_status_registers is defined in utilities.h */

/*!
 * @brief Forwarding mode definitions.
 */
struct eth_mac_forward_mode {
    bool    P1_FWD_UNK_TO_HOST;         /*!< Forward unknown frames received on Port 1 to host.     */
    bool    P2_FWD_UNK_TO_HOST;         /*!< Forward unknown frames received on Port 2 to host.     */
    bool    P1_FWD_UNK_TO_P2;           /*!< Forward unknown frames received on Port 1 to Port 2.   */
    bool    P2_FWD_UNK_TO_P1;           /*!< FOrward unknown frames received on Port 2 to Port 1.   */
};

/* MAC_CALLBACK_STATUS_* macros are defined in utilities.h */

/* Forward declaration of queue structure */
typedef struct net_queue eth_mac_queue;

/* Forward declaration to break circular dependency   *
 * between eth_mac_DriverEntry_t and eth_mac_desc */
// struct eth_mac_driver_entry;

/*!
 * @brief Device descriptor structure.
 */
struct eth_mac_desc {
    enum eth_devices	            device_type;                                  /*!< Configure the driver to choose the ADIN device type    */
    void 			    *dev_mem;                                                
    volatile enum eth_mac_state     state;                                        /*!< MAC state.                                             */
    bool                            is_oa_spi_enabled;                            /*<! Is OPEN Alliance SPI enabled.                          */
    eth_callback_t                   cb_func[ETH_MAC_EVT_MAX];                     /*!< Callback function pointers.                            */
    void                            *cb_param[ETH_MAC_EVT_MAX];                   /*!< Callback functions parameters.                         */
    void                            *device;                                 	  /*!< Pointer to the high-level device ADINxxxx.             */
    void                            *app_dev;                                     /*!< Pointer to user high-level device ADINxxxx.                */
    uint32_t                        phy_addr;                                     /*!< Address of the PHY device.                             */
    uint16_t                        addr_filter_active;                           /*!< Mark address filter entries as active/inactive.        */
    uint32_t                        irq_mask0;                                    /*!< Interrupt mask for the STATUS0 register.               */
    uint32_t                        irq_mask1;                                    /*!< Interrupt mask for the STATUS1 register.               */
    struct eth_frame_struct          tx_queue_frames[TX_QUEUE_NUM_ENTRIES_RAW];    /*!< Transmit frames held by \ref txQueue.                  */
    eth_mac_queue	    	    tx_queue;                                     /*!< Transmit frames queue.                                 */
    eth_mac_queue            	    *rx_queue;                                    /*!< Pointer to active receive frames queue.                */
    struct eth_frame_struct          rx_queue_lp_frames[RX_QUEUE_NUM_ENTRIES_RAW]; /*!< Receive frames held by \ref rxQueueLp.                 */
    eth_mac_queue            	    rx_queue_lp;                                  /*!< Receive frames queue.                                  */
    bool                            is_rx_queue_hp_enabled;                       /*!< Enable high priority rxQueue.                          */
    struct eth_frame_struct          rx_queue_hp_frames[RX_QUEUE_NUM_ENTRIES_RAW]; /*!< High priority receive frames held by \ref rxQueueHp.   */
    eth_mac_queue            	    rx_queue_hp;                                  /*!< High priority receive frames queue.                    */
    volatile bool                   pending_ctrl;                                 /*!< Pending control transaction request flag.              */
    bool                            config_sync;                                  /*!< Configuration has been synchronized.                   */
    bool                            fcs_check_en;                                 /*!< Enable Frame Check Sequence validation on the host.    */
    enum eth_mac_ts_format          timestamp_format;                             /*!< Format of ingress/egress timestamp.                    */
    struct eth_status_registers      status_registers;                             /*!< Status register values updated on interrupt assertion. */
    uint32_t                        phy_irq_mask;                                 /*!< Interrupt mask for PHY status regsiters.               */
    uint32_t                        oa_cps;                                       /*!< OA Chunk Payload Selector currently configured.        */
    uint32_t                        oa_max_chunk_count;                           /*!< Maximum number of chunks in an OA SPI transaction.     */
    bool			    tx_cte;
    uint32_t 			    eth_irq;

    int  (*mac_read)                (void *desc, enum eth_mac_state state, uint16_t reg_addr, void *pBuf, uint32_t nBytes, bool blocking); /*Interface to SPI_Read                 */
    int  (*mac_write)               (void *desc, enum eth_mac_state state, uint16_t reg_addr, void *pBuf, uint32_t nBytes, bool blocking); /*Interface to SPI_Write                */
    int  (*mac_get_comm_status)     (void *desc, uint8_t *backup, bool bIsCtrl);                                                       /*Interface to SPI_GetStatus            */
    int  (*mac_write_frame)         (void *desc, struct eth_frame_struct *pFrame, bool blocking);                                        /*Interface to Generic SPI_WriteFrame   */
    void (*mac_request_irq_handler) (void *desc); /*Interface to oaIrqHandler             */
    int  (*mac_wait_until_ready)    (void *desc);
    int  (*mac_process_tx_frame)    (void *desc, bool blocking);
    int  (*mac_process_rx_frame)    (void *desc, bool blocking);
};

struct eth_mac_init_param {
    enum eth_devices	            device_type;
    void 			    *dev_mem;                                                
    uint32_t			    mem_size;
    void                            *app_main;
    void                            *app_user;
    bool			    is_oa_spi_enabled;    
    bool			    is_rx_queue_hp_enabled;
    bool			    fcs_check_en;
    enum eth_mac_ts_format          timestamp_format;
    uint32_t			    eth_irq;

    int  (*mac_read)                (void *desc, enum eth_mac_state state, uint16_t reg_addr, void *pBuf, uint32_t nBytes, bool blocking); /*Interface to SPI_Read                 */
    int  (*mac_write)               (void *desc, enum eth_mac_state state, uint16_t reg_addr, void *pBuf, uint32_t nBytes, bool blocking); /*Interface to SPI_Write                */
    int  (*mac_get_comm_status)     (void *desc, uint8_t *backup, bool bIsCtrl);                                                       /*Interface to SPI_GetStatus            */
    int  (*mac_write_frame)         (void *desc, struct eth_frame_struct *pFrame, bool blocking);                                        /*Interface to Generic SPI_WriteFrame   */
    void (*mac_request_irq_handler) (void *desc); /*Interface to oaIrqHandler             */
    int  (*mac_wait_until_ready)    (void *desc);
    int  (*mac_process_tx_frame)    (void *desc, bool blocking);
    int  (*mac_process_rx_frame)    (void *desc, bool blocking);
};

int	eth_mac_init_queue                (eth_mac_queue *queue, struct eth_frame_struct *frames, uint32_t num_entries);
int     eth_mac_reset                     (struct eth_mac_desc *desc, enum eth_mac_reset_type reset_type);
int     eth_mac_sync_config		  (struct eth_mac_desc *desc);
int     eth_mac_register_callback         (struct eth_mac_desc *desc, eth_callback_t cb_func, enum eth_mac_event cb_event, void *cb_param);
int     eth_mac_get_link_status           (struct eth_mac_desc *desc, uint32_t port, enum eth_mac_link_status *link_status);
int     eth_mac_get_stat_counters         (struct eth_mac_desc *desc, uint32_t port, struct eth_mac_stat_counter *stat);
int     eth_mac_add_address_filter        (struct eth_mac_desc *desc, uint8_t *mac_addr, uint8_t *mac_addr_mask, uint16_t addr_rule);
int     eth_mac_clr_address_filter        (struct eth_mac_desc *desc, uint32_t addr_index);
int     eth_mac_submit_tx_buffer       	  (struct eth_mac_desc *desc, struct eth_frame_header header, struct eth_buf_desc *buf_desc);
int     eth_mac_submit_rx_buffer          (struct eth_mac_desc *desc, struct eth_buf_desc *buf_desc);
int     eth_mac_submit_rx_buffer_hp       (struct eth_mac_desc *desc, struct eth_buf_desc *buf_desc);
int     eth_mac_set_promiscuous_mode      (struct eth_mac_desc *desc, bool bit_flag);
int     eth_mac_get_promiscuous_mode      (struct eth_mac_desc *desc, bool *bit_flag);
int     eth_mac_phy_read                  (struct eth_mac_desc *desc, uint8_t hw_addr, uint32_t reg_addr, uint16_t *data);
int     eth_mac_phy_write                 (struct eth_mac_desc *desc, uint8_t hw_addr, uint32_t reg_addr, uint16_t data);
int     eth_mac_set_chunk_size            (struct eth_mac_desc *desc, enum eth_mac_oa_cps cps);
int     eth_mac_get_chunk_size            (struct eth_mac_desc *desc, enum eth_mac_oa_cps *pCps);
int     eth_mac_set_cut_through_mode      (struct eth_mac_desc *desc, bool tx_cte, bool rx_cte);
int     eth_mac_get_cut_through_mode      (struct eth_mac_desc *desc, bool *tx_cte, bool *rx_cte);
int     eth_mac_set_fifo_size             (struct eth_mac_desc *desc, uint32_t write_val);
int     eth_mac_get_fifo_size             (struct eth_mac_desc *desc, uint32_t *read_val);
int     eth_mac_clear_fifo                (struct eth_mac_desc *desc, enum eth_mac_fifo_clr_mode clear_mode);
int     eth_mac_ts_enable                 (struct eth_mac_desc *desc, enum eth_mac_ts_format format);
int     eth_mac_ts_clear                  (struct eth_mac_desc *desc);
int     eth_mac_ts_timer_start            (struct eth_mac_desc *desc, struct eth_mac_ts_timer_cfg* timer_config);
int     eth_mac_ts_timer_stop             (struct eth_mac_desc *desc);
int     eth_mac_ts_set_timer_abs      	  (struct eth_mac_desc *desc, uint32_t seconds, uint32_t nanoseconds);
int     eth_mac_ts_sync_clock             (struct eth_mac_desc *desc, int64_t time_error, uint64_t refer_time_ns_diff, uint64_t local_time_ns_diff);
int     eth_mac_ts_get_ext_capt_timestamp (struct eth_mac_desc *desc, struct eth_mac_ts_timespec *captured_timespec);
int     eth_mac_ts_get_egress_timestamp   (struct eth_mac_desc *desc, enum mac_egress_capture egress_reg, struct eth_mac_ts_timespec *captured_timespec);
int     eth_mac_ts_convert                (uint32_t timestamp_low_word, uint32_t timestamp_high_word, enum eth_mac_ts_format format, struct eth_mac_ts_timespec *timespec);
int64_t eth_mac_ts_subtract               (struct eth_mac_ts_timespec *ts_a, struct eth_mac_ts_timespec *ts_b);
int     eth_mac_read_register             (struct eth_mac_desc *desc, uint16_t reg_addr, uint32_t *reg_data);
int     eth_mac_write_register            (struct eth_mac_desc *desc, uint16_t reg_addr, uint32_t reg_data);
int     eth_mac_set_forward_mode          (struct eth_mac_desc *desc, struct eth_mac_forward_mode mode, uint32_t mask);
int     eth_mac_get_forward_mode          (struct eth_mac_desc *desc, struct eth_mac_forward_mode *mode);

/*Forward declaration to provide access to ADIN module*/
int     eth_mac_init                	 (struct eth_mac_desc **desc, struct eth_mac_init_param *param);
int     eth_mac_remove              	 (struct eth_mac_desc *desc);
uint8_t eth_mac_calculate_parity	 (uint8_t *p, uint32_t nBytes);


/* ====================================================================================================
        MAC Module Register ResetValue Definitions
   ==================================================================================================== */
#define RSTVAL_MAC_IDVER                                           (0X10)
#define RSTVAL_MAC_PHYID                                           (0X283BC91)
#define RSTVAL_MAC_CAPABILITY                                      (0X6C3)
#define RSTVAL_MAC_RESET                                           (0X0)
#define RSTVAL_MAC_CONFIG0                                         (0X6)
#define RSTVAL_MAC_CONFIG2                                         (0X800)
#define RSTVAL_MAC_STATUS0                                         (0X40)
#define RSTVAL_MAC_STATUS1                                         (0X0)
#define RSTVAL_MAC_BUFSTS                                          (0X7700)
#define RSTVAL_MAC_IMASK0                                          (0X1FBF)
#define RSTVAL_MAC_IMASK1                                          (0X43FA1F1A)
#define RSTVAL_MAC_TTSCAH                                          (0X0)
#define RSTVAL_MAC_TTSCAL                                          (0X0)
#define RSTVAL_MAC_TTSCBH                                          (0X0)
#define RSTVAL_MAC_TTSCBL                                          (0X0)
#define RSTVAL_MAC_TTSCCH                                          (0X0)
#define RSTVAL_MAC_TTSCCL                                          (0X0)
#define RSTVAL_MAC_MDIOACC_0_                                      (0X8C000000)
#define RSTVAL_MAC_MDIOACC_1_                                      (0X8C000000)
#define RSTVAL_MAC_MDIOACC_2_                                      (0X8C000000)
#define RSTVAL_MAC_MDIOACC_3_                                      (0X8C000000)
#define RSTVAL_MAC_MDIOACC_4_                                      (0X8C000000)
#define RSTVAL_MAC_MDIOACC_5_                                      (0X8C000000)
#define RSTVAL_MAC_MDIOACC_6_                                      (0X8C000000)
#define RSTVAL_MAC_MDIOACC_7_                                      (0X8C000000)
#define RSTVAL_MAC_TX_FSIZE                                        (0X0)
#define RSTVAL_MAC_TX                                              (0X0)
#define RSTVAL_MAC_TX_SPACE                                        (0XFFF)
#define RSTVAL_MAC_TX_THRESH                                       (0X41)
#define RSTVAL_MAC_FIFO_CLR                                        (0X0)
#define RSTVAL_MAC_SCRATCH_0_                                      (0X0)
#define RSTVAL_MAC_SCRATCH_1_                                      (0X0)
#define RSTVAL_MAC_SCRATCH_2_                                      (0X0)
#define RSTVAL_MAC_SCRATCH_3_                                      (0X0)
#define RSTVAL_MAC_MAC_RST_STATUS                                  (0X3)
#define RSTVAL_MAC_SOFT_RST                                        (0X0)
#define RSTVAL_MAC_SPI_INJ_ERR                                     (0X0)
#define RSTVAL_MAC_FIFO_SIZE                                       (0X464)
#define RSTVAL_MAC_TFC                                             (0X0)
#define RSTVAL_MAC_TXSIZE                                          (0X0)
#define RSTVAL_MAC_HTX_OVF_FRM_CNT                                 (0X0)
#define RSTVAL_MAC_MECC_ERR_ADDR                                   (0X0)
#define RSTVAL_MAC_CECC_ERR_0_                                     (0X0)
#define RSTVAL_MAC_CECC_ERR_1_                                     (0X0)
#define RSTVAL_MAC_CECC_ERR_2_                                     (0X0)
#define RSTVAL_MAC_CECC_ERR_3_                                     (0X0)
#define RSTVAL_MAC_CECC_ERR_4_                                     (0X0)
#define RSTVAL_MAC_CECC_ERR_5_                                     (0X0)
#define RSTVAL_MAC_CECC_ERR_6_                                     (0X0)
#define RSTVAL_MAC_ADDR_FILT_UPR_0_                                (0X0)
#define RSTVAL_MAC_ADDR_FILT_UPR_1_                                (0X0)
#define RSTVAL_MAC_ADDR_FILT_UPR_2_                                (0X0)
#define RSTVAL_MAC_ADDR_FILT_UPR_3_                                (0X0)
#define RSTVAL_MAC_ADDR_FILT_UPR_4_                                (0X0)
#define RSTVAL_MAC_ADDR_FILT_UPR_5_                                (0X0)
#define RSTVAL_MAC_ADDR_FILT_UPR_6_                                (0X0)
#define RSTVAL_MAC_ADDR_FILT_UPR_7_                                (0X0)
#define RSTVAL_MAC_ADDR_FILT_UPR_8_                                (0X0)
#define RSTVAL_MAC_ADDR_FILT_UPR_9_                                (0X0)
#define RSTVAL_MAC_ADDR_FILT_UPR_10_                               (0X0)
#define RSTVAL_MAC_ADDR_FILT_UPR_11_                               (0X0)
#define RSTVAL_MAC_ADDR_FILT_UPR_12_                               (0X0)
#define RSTVAL_MAC_ADDR_FILT_UPR_13_                               (0X0)
#define RSTVAL_MAC_ADDR_FILT_UPR_14_                               (0X0)
#define RSTVAL_MAC_ADDR_FILT_UPR_15_                               (0X0)
#define RSTVAL_MAC_ADDR_FILT_LWR_0_                                (0X0)
#define RSTVAL_MAC_ADDR_FILT_LWR_1_                                (0X0)
#define RSTVAL_MAC_ADDR_FILT_LWR_2_                                (0X0)
#define RSTVAL_MAC_ADDR_FILT_LWR_3_                                (0X0)
#define RSTVAL_MAC_ADDR_FILT_LWR_4_                                (0X0)
#define RSTVAL_MAC_ADDR_FILT_LWR_5_                                (0X0)
#define RSTVAL_MAC_ADDR_FILT_LWR_6_                                (0X0)
#define RSTVAL_MAC_ADDR_FILT_LWR_7_                                (0X0)
#define RSTVAL_MAC_ADDR_FILT_LWR_8_                                (0X0)
#define RSTVAL_MAC_ADDR_FILT_LWR_9_                                (0X0)
#define RSTVAL_MAC_ADDR_FILT_LWR_10_                               (0X0)
#define RSTVAL_MAC_ADDR_FILT_LWR_11_                               (0X0)
#define RSTVAL_MAC_ADDR_FILT_LWR_12_                               (0X0)
#define RSTVAL_MAC_ADDR_FILT_LWR_13_                               (0X0)
#define RSTVAL_MAC_ADDR_FILT_LWR_14_                               (0X0)
#define RSTVAL_MAC_ADDR_FILT_LWR_15_                               (0X0)
#define RSTVAL_MAC_ADDR_MSK_UPR_0_                                 (0XFFFF)
#define RSTVAL_MAC_ADDR_MSK_UPR_1_                                 (0XFFFF)
#define RSTVAL_MAC_ADDR_MSK_LWR_0_                                 (0XFFFFFFFF)
#define RSTVAL_MAC_ADDR_MSK_LWR_1_                                 (0XFFFFFFFF)
#define RSTVAL_MAC_TS_ADDEND                                       (0X85555555)
#define RSTVAL_MAC_TS_1SEC_CMP                                     (0X3B9ACA00)
#define RSTVAL_MAC_TS_SEC_CNT                                      (0X0)
#define RSTVAL_MAC_TS_NS_CNT                                       (0X0)
#define RSTVAL_MAC_TS_CFG                                          (0X0)
#define RSTVAL_MAC_TS_TIMER_HI                                     (0X0)
#define RSTVAL_MAC_TS_TIMER_LO                                     (0X0)
#define RSTVAL_MAC_TS_TIMER_QE_CORR                                (0X0)
#define RSTVAL_MAC_TS_TIMER_START                                  (0X0)
#define RSTVAL_MAC_TS_EXT_CAPT0                                    (0X0)
#define RSTVAL_MAC_TS_EXT_CAPT1                                    (0X0)
#define RSTVAL_MAC_TS_FREECNT_CAPT                                 (0X0)
#define RSTVAL_MAC_P1_RX_FSIZE                                     (0X0)
#define RSTVAL_MAC_P1_RX                                           (0X0)
#define RSTVAL_MAC_P1_RX_FRM_CNT                                   (0X0)
#define RSTVAL_MAC_P1_RX_BCAST_CNT                                 (0X0)
#define RSTVAL_MAC_P1_RX_MCAST_CNT                                 (0X0)
#define RSTVAL_MAC_P1_RX_UCAST_CNT                                 (0X0)
#define RSTVAL_MAC_P1_RX_CRC_ERR_CNT                               (0X0)
#define RSTVAL_MAC_P1_RX_ALGN_ERR_CNT                              (0X0)
#define RSTVAL_MAC_P1_RX_LS_ERR_CNT                                (0X0)
#define RSTVAL_MAC_P1_RX_PHY_ERR_CNT                               (0X0)
#define RSTVAL_MAC_P1_TX_FRM_CNT                                   (0X0)
#define RSTVAL_MAC_P1_TX_BCAST_CNT                                 (0X0)
#define RSTVAL_MAC_P1_TX_MCAST_CNT                                 (0X0)
#define RSTVAL_MAC_P1_TX_UCAST_CNT                                 (0X0)
#define RSTVAL_MAC_P1_RX_DROP_FULL_CNT                             (0X0)
#define RSTVAL_MAC_P1_RX_DROP_FILT_CNT                             (0X0)
#define RSTVAL_MAC_P1_RX_IFG_ERR_CNT                               (0X0)
#define RSTVAL_MAC_P1_TX_IFG                                       (0XB)
#define RSTVAL_MAC_P1_LOOP                                         (0X0)
#define RSTVAL_MAC_P1_RX_CRC_EN                                    (0X1)
#define RSTVAL_MAC_P1_RX_IFG                                       (0XA)
#define RSTVAL_MAC_P1_RX_MAX_LEN                                   (0X618)
#define RSTVAL_MAC_P1_RX_MIN_LEN                                   (0X40)
#define RSTVAL_MAC_P1_LO_RFC                                       (0X0)
#define RSTVAL_MAC_P1_HI_RFC                                       (0X0)
#define RSTVAL_MAC_P1_LO_RXSIZE                                    (0X0)
#define RSTVAL_MAC_P1_HI_RXSIZE                                    (0X0)

/* ====================================================================================================
        MAC Module Register BitPositions, Lengths, Masks and Enumerations Definitions
   ==================================================================================================== */

/* ----------------------------------------------------------------------------------------------------
          IDVER                                                 Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_IDVER_MINVER                                 (0U)           /* OA Minor Version. */
#define BITL_MAC_IDVER_MINVER                                 (4U)           /* OA Minor Version. */
#define BITM_MAC_IDVER_MINVER                                 (0X0000000FU)  /* OA Minor Version. */
#define BITP_MAC_IDVER_MAJVER                                 (4U)           /* OA Major Version. */
#define BITL_MAC_IDVER_MAJVER                                 (4U)           /* OA Major Version. */
#define BITM_MAC_IDVER_MAJVER                                 (0X000000F0U)  /* OA Major Version. */

/* ----------------------------------------------------------------------------------------------------
          PHYID                                                 Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_PHYID_REVISION                               (0U)           /* Manufacturerâ€™s Revision Number. */
#define BITL_MAC_PHYID_REVISION                               (4U)           /* Manufacturerâ€™s Revision Number. */
#define BITM_MAC_PHYID_REVISION                               (0X0000000FU)  /* Manufacturerâ€™s Revision Number. */
#define BITP_MAC_PHYID_MODEL                                  (4U)           /* Manufacturerâ€™s Model Number. */
#define BITL_MAC_PHYID_MODEL                                  (6U)           /* Manufacturerâ€™s Model Number. */
#define BITM_MAC_PHYID_MODEL                                  (0X000003F0U)  /* Manufacturerâ€™s Model Number. */
#define BITP_MAC_PHYID_OUI                                    (10U)          /* Organizationally Unique Identifier (Bits 2:23). */
#define BITL_MAC_PHYID_OUI                                    (22U)          /* Organizationally Unique Identifier (Bits 2:23). */
#define BITM_MAC_PHYID_OUI                                    (0XFFFFFC00U)  /* Organizationally Unique Identifier (Bits 2:23). */

/* ----------------------------------------------------------------------------------------------------
          CAPABILITY                                            Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_CAPABILITY_MINCPS                            (0U)           /* Minimum Supported Chunk Payload Size. */
#define BITL_MAC_CAPABILITY_MINCPS                            (3U)           /* Minimum Supported Chunk Payload Size. */
#define BITM_MAC_CAPABILITY_MINCPS                            (0X00000007U)  /* Minimum Supported Chunk Payload Size. */
#define BITP_MAC_CAPABILITY_SEQC                              (4U)           /* TX Data Chunk Sequence and Retry Capability. */
#define BITL_MAC_CAPABILITY_SEQC                              (1U)           /* TX Data Chunk Sequence and Retry Capability. */
#define BITM_MAC_CAPABILITY_SEQC                              (0X00000010U)  /* TX Data Chunk Sequence and Retry Capability. */
#define BITP_MAC_CAPABILITY_AIDC                              (5U)           /* Address Increment Disable Capability. */
#define BITL_MAC_CAPABILITY_AIDC                              (1U)           /* Address Increment Disable Capability. */
#define BITM_MAC_CAPABILITY_AIDC                              (0X00000020U)  /* Address Increment Disable Capability. */
#define BITP_MAC_CAPABILITY_FTSC                              (6U)           /* Frame Timestamp Capability. */
#define BITL_MAC_CAPABILITY_FTSC                              (1U)           /* Frame Timestamp Capability. */
#define BITM_MAC_CAPABILITY_FTSC                              (0X00000040U)  /* Frame Timestamp Capability. */
#define BITP_MAC_CAPABILITY_CTC                               (7U)           /* Cut-Through Capability. */
#define BITL_MAC_CAPABILITY_CTC                               (1U)           /* Cut-Through Capability. */
#define BITM_MAC_CAPABILITY_CTC                               (0X00000080U)  /* Cut-Through Capability. */
#define BITP_MAC_CAPABILITY_DPRAC                             (8U)           /* Direct PHY Register Access Capability. */
#define BITL_MAC_CAPABILITY_DPRAC                             (1U)           /* Direct PHY Register Access Capability. */
#define BITM_MAC_CAPABILITY_DPRAC                             (0X00000100U)  /* Direct PHY Register Access Capability. */
#define BITP_MAC_CAPABILITY_IPRAC                             (9U)           /* Indirect PHY Register Access Capability. */
#define BITL_MAC_CAPABILITY_IPRAC                             (1U)           /* Indirect PHY Register Access Capability. */
#define BITM_MAC_CAPABILITY_IPRAC                             (0X00000200U)  /* Indirect PHY Register Access Capability. */
#define BITP_MAC_CAPABILITY_TXFCSVC                           (10U)          /* Transmit Frame Check Sequence Validation Capability. */
#define BITL_MAC_CAPABILITY_TXFCSVC                           (1U)           /* Transmit Frame Check Sequence Validation Capability. */
#define BITM_MAC_CAPABILITY_TXFCSVC                           (0X00000400U)  /* Transmit Frame Check Sequence Validation Capability. */

#define ENUM_MAC_CAPABILITY_TXFCSVC_TXFCSVC_0                 (0X00000000U)  /* Transmit FCS Validation is Not Supported. */
#define ENUM_MAC_CAPABILITY_TXFCSVC_TXFCSVC_1                 (0X00000001U)  /* Transmit FCS Validation is Supported. */
#define ENUM_MAC_CAPABILITY_IPRAC_IPRAC_0                     (0X00000000U)  /* PHY Registers are Not Indirectly Accessible. */
#define ENUM_MAC_CAPABILITY_IPRAC_IPRAC_1                     (0X00000001U)  /* PHY Registers are Indirectly Accessible. */
#define ENUM_MAC_CAPABILITY_DPRAC_DPRAC_0                     (0X00000000U)  /* PHY Registers are Not Directly Accessible. */
#define ENUM_MAC_CAPABILITY_DPRAC_DPRAC_1                     (0X00000001U)  /* PHY Registers are Directly Accessible. */
#define ENUM_MAC_CAPABILITY_CTC_CTC_0                         (0X00000000U)  /* Cut-Through Not Supported. */
#define ENUM_MAC_CAPABILITY_CTC_CTC_1                         (0X00000001U)  /* Cut-Through Supported. */
#define ENUM_MAC_CAPABILITY_FTSC_FTSC_1                       (0X00000001U)  /* IEEE 1588 Timestamp Capture on Frame Tx/Rx is Supported. */
#define ENUM_MAC_CAPABILITY_FTSC_FTSC_0                       (0X00000000U)  /* IEEE 1588 Timestamp Capture on Frame Tx/Rx is Not Supported. */
#define ENUM_MAC_CAPABILITY_SEQC_SEQC_1                       (0X00000001U)  /* TX Data Chunk Sequence and Retry is Supported. */
#define ENUM_MAC_CAPABILITY_SEQC_SEQC_0                       (0X00000000U)  /* TX Data Chunk Sequence and Retry is Not Supported. */
#define ENUM_MAC_CAPABILITY_MINCPS_CPS64                      (0X00000006U)  /* Minimum Supported Chunk Payload Size is 64 Bytes. */
#define ENUM_MAC_CAPABILITY_MINCPS_CPS32                      (0X00000005U)  /* Minimum Supported Chunk Payload Size is 32 Bytes. */
#define ENUM_MAC_CAPABILITY_MINCPS_CPS16                      (0X00000004U)  /* Minimum Supported Chunk Payload Size is 16 Bytes. */
#define ENUM_MAC_CAPABILITY_MINCPS_CPS8                       (0X00000003U)  /* Minimum Supported Chunk Payload Size is 8 Bytes. */

/* ----------------------------------------------------------------------------------------------------
          RESET                                                 Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_RESET_SWRESET                                (0U)           /* Software Reset. */
#define BITL_MAC_RESET_SWRESET                                (1U)           /* Software Reset. */
#define BITM_MAC_RESET_SWRESET                                (0X00000001U)  /* Software Reset. */

/* ----------------------------------------------------------------------------------------------------
          CONFIG0                                               Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_CONFIG0_CPS                                  (0U)           /* Chunk Payload Selector (N). */
#define BITL_MAC_CONFIG0_CPS                                  (3U)           /* Chunk Payload Selector (N). */
#define BITM_MAC_CONFIG0_CPS                                  (0X00000007U)  /* Chunk Payload Selector (N). */
#define BITP_MAC_CONFIG0_SEQE                                 (4U)           /* Enable TX Data Chunk Sequence and Retry. */
#define BITL_MAC_CONFIG0_SEQE                                 (1U)           /* Enable TX Data Chunk Sequence and Retry. */
#define BITM_MAC_CONFIG0_SEQE                                 (0X00000010U)  /* Enable TX Data Chunk Sequence and Retry. */
#define BITP_MAC_CONFIG0_PROTE                                (5U)           /* Enable Control Data Read Write Protection. */
#define BITL_MAC_CONFIG0_PROTE                                (1U)           /* Enable Control Data Read Write Protection. */
#define BITM_MAC_CONFIG0_PROTE                                (0X00000020U)  /* Enable Control Data Read Write Protection. */
#define BITP_MAC_CONFIG0_FTSS                                 (6U)           /* Receive Frame Timestamp Select. */
#define BITL_MAC_CONFIG0_FTSS                                 (1U)           /* Receive Frame Timestamp Select. */
#define BITM_MAC_CONFIG0_FTSS                                 (0X00000040U)  /* Receive Frame Timestamp Select. */
#define BITP_MAC_CONFIG0_FTSE                                 (7U)           /* Frame Timestamp Enable. */
#define BITL_MAC_CONFIG0_FTSE                                 (1U)           /* Frame Timestamp Enable. */
#define BITM_MAC_CONFIG0_FTSE                                 (0X00000080U)  /* Frame Timestamp Enable. */
#define BITP_MAC_CONFIG0_RXCTE                                (8U)           /* Receive Cut-Through Enable. */
#define BITL_MAC_CONFIG0_RXCTE                                (1U)           /* Receive Cut-Through Enable. */
#define BITM_MAC_CONFIG0_RXCTE                                (0X00000100U)  /* Receive Cut-Through Enable. */
#define BITP_MAC_CONFIG0_TXCTE                                (9U)           /* Transmit Cut-Through Enable. */
#define BITL_MAC_CONFIG0_TXCTE                                (1U)           /* Transmit Cut-Through Enable. */
#define BITM_MAC_CONFIG0_TXCTE                                (0X00000200U)  /* Transmit Cut-Through Enable. */
#define BITP_MAC_CONFIG0_TXCTHRESH                            (10U)          /* Transmit Credit Threshold. */
#define BITL_MAC_CONFIG0_TXCTHRESH                            (2U)           /* Transmit Credit Threshold. */
#define BITM_MAC_CONFIG0_TXCTHRESH                            (0X00000C00U)  /* Transmit Credit Threshold. */
#define BITP_MAC_CONFIG0_ZARFE                                (12U)          /* Zero-Align Receive Frame Enable. */
#define BITL_MAC_CONFIG0_ZARFE                                (1U)           /* Zero-Align Receive Frame Enable. */
#define BITM_MAC_CONFIG0_ZARFE                                (0X00001000U)  /* Zero-Align Receive Frame Enable. */
#define BITP_MAC_CONFIG0_CSARFE                               (13U)          /* CSn Align Receive Frame Enable. */
#define BITL_MAC_CONFIG0_CSARFE                               (1U)           /* CSn Align Receive Frame Enable. */
#define BITM_MAC_CONFIG0_CSARFE                               (0X00002000U)  /* CSn Align Receive Frame Enable. */
#define BITP_MAC_CONFIG0_TXFCSVE                              (14U)          /* Transmit Frame Check Sequence Validation Enable. */
#define BITL_MAC_CONFIG0_TXFCSVE                              (1U)           /* Transmit Frame Check Sequence Validation Enable. */
#define BITM_MAC_CONFIG0_TXFCSVE                              (0X00004000U)  /* Transmit Frame Check Sequence Validation Enable. */
#define BITP_MAC_CONFIG0_SYNC                                 (15U)          /* Configuration Synchronization. */
#define BITL_MAC_CONFIG0_SYNC                                 (1U)           /* Configuration Synchronization. */
#define BITM_MAC_CONFIG0_SYNC                                 (0X00008000U)  /* Configuration Synchronization. */

#define ENUM_MAC_CONFIG0_SYNC_SYNC_0                          (0X00000000U)  /* The MACPHY Has Been Reset and is Not Configured. */
#define ENUM_MAC_CONFIG0_SYNC_SYNC_1                          (0X00000001U)  /* The MACPHY is Configured. */
#define ENUM_MAC_CONFIG0_TXCTHRESH_CREDIT_1                   (0X00000000U)  /* â‰¥ 1 Credit. */
#define ENUM_MAC_CONFIG0_TXCTHRESH_CREDIT_4                   (0X00000001U)  /* â‰¥ 4 Credits. */
#define ENUM_MAC_CONFIG0_TXCTHRESH_CREDIT_8                   (0X00000002U)  /* â‰¥ 8 Credits. */
#define ENUM_MAC_CONFIG0_TXCTHRESH_CREDIT_16                  (0X00000003U)  /* â‰¥ 16 Credits. */
#define ENUM_MAC_CONFIG0_FTSE_FTSE_0                          (0X00000000U)  /* Frame Receive/Transmit Timestamps are Disabled. */
#define ENUM_MAC_CONFIG0_FTSE_FTSE_1                          (0X00000001U)  /* Frame Receive/Transmit Timestamps are Enabled. */
#define ENUM_MAC_CONFIG0_FTSS_FTSS_0                          (0X00000000U)  /* 32-bit Timestamps. */
#define ENUM_MAC_CONFIG0_FTSS_FTSS_1                          (0X00000001U)  /* 64-bit Timestamps. */
#define ENUM_MAC_CONFIG0_CPS_CHUNK_8BYTE                      (0X00000003U)  /* No description provided */
#define ENUM_MAC_CONFIG0_CPS_CHUNK_16BYTE                     (0X00000004U)  /* No description provided */
#define ENUM_MAC_CONFIG0_CPS_CHUNK_32BYTE                     (0X00000005U)  /* No description provided */
#define ENUM_MAC_CONFIG0_CPS_CHUNK_64BYTE                     (0X00000006U)  /* No description provided */

/* ----------------------------------------------------------------------------------------------------
          CONFIG2                                               Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_CONFIG2_MSPEED                               (0U)           /* SPI to MDIO Bridge MDC Clock Speed. */
#define BITL_MAC_CONFIG2_MSPEED                               (2U)           /* SPI to MDIO Bridge MDC Clock Speed. */
#define BITM_MAC_CONFIG2_MSPEED                               (0X00000003U)  /* SPI to MDIO Bridge MDC Clock Speed. */
#define BITP_MAC_CONFIG2_P1_FWD_UNK2HOST                      (2U)           /* Forward Frames Not Matching Any MAC Address to the Host. */
#define BITL_MAC_CONFIG2_P1_FWD_UNK2HOST                      (1U)           /* Forward Frames Not Matching Any MAC Address to the Host. */
#define BITM_MAC_CONFIG2_P1_FWD_UNK2HOST                      (0X00000004U)  /* Forward Frames Not Matching Any MAC Address to the Host. */
#define BITP_MAC_CONFIG2_P1_RCV_IFG_ERR_FRM                   (4U)           /* Admit Frames with IFG Errors on P1. */
#define BITL_MAC_CONFIG2_P1_RCV_IFG_ERR_FRM                   (1U)           /* Admit Frames with IFG Errors on P1. */
#define BITM_MAC_CONFIG2_P1_RCV_IFG_ERR_FRM                   (0X00000010U)  /* Admit Frames with IFG Errors on P1. */
#define BITP_MAC_CONFIG2_CRC_APPEND                           (5U)           /* Enable CRC Append. */
#define BITL_MAC_CONFIG2_CRC_APPEND                           (1U)           /* Enable CRC Append. */
#define BITM_MAC_CONFIG2_CRC_APPEND                           (0X00000020U)  /* Enable CRC Append. */
#define BITP_MAC_CONFIG2_STATS_CLR_ON_RD                      (6U)           /* Statistics Clear on Reading. */
#define BITL_MAC_CONFIG2_STATS_CLR_ON_RD                      (1U)           /* Statistics Clear on Reading. */
#define BITM_MAC_CONFIG2_STATS_CLR_ON_RD                      (0X00000040U)  /* Statistics Clear on Reading. */
#define BITP_MAC_CONFIG2_SFD_DETECT_SRC                       (7U)           /* Determines If the SFD is Detected in the PHY or MAC. */
#define BITL_MAC_CONFIG2_SFD_DETECT_SRC                       (1U)           /* Determines If the SFD is Detected in the PHY or MAC. */
#define BITM_MAC_CONFIG2_SFD_DETECT_SRC                       (0X00000080U)  /* Determines If the SFD is Detected in the PHY or MAC. */
#define BITP_MAC_CONFIG2_TX_RDY_ON_EMPTY                      (8U)           /* Assert TX_RDY When the Tx FIFO is Empty. */
#define BITL_MAC_CONFIG2_TX_RDY_ON_EMPTY                      (1U)           /* Assert TX_RDY When the Tx FIFO is Empty. */
#define BITM_MAC_CONFIG2_TX_RDY_ON_EMPTY                      (0X00000100U)  /* Assert TX_RDY When the Tx FIFO is Empty. */

#define ENUM_MAC_CONFIG2_SFD_DETECT_SRC_PHY                   (0X00000000U)  /* Select the SFD from the PHY. */
#define ENUM_MAC_CONFIG2_SFD_DETECT_SRC_MAC                   (0X00000001U)  /* Select the SFD from the MAC. */
#define ENUM_MAC_CONFIG2_STATS_CLR_ON_RD_HOLD_STAT            (0X00000000U)  /* Statistic Counter is Not Cleared on Reading. */
#define ENUM_MAC_CONFIG2_STATS_CLR_ON_RD_CLR_STAT             (0X00000001U)  /* Clear Statistics Counters on Reading. */
#define ENUM_MAC_CONFIG2_MSPEED_MHZ_2P5                       (0X00000000U)  /* 2.5 MHz. */
#define ENUM_MAC_CONFIG2_MSPEED_MHZ_4P166                     (0X00000001U)  /* 4.166 MHz. */

/* ----------------------------------------------------------------------------------------------------
          STATUS0                                               Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_STATUS0_TXPE                                 (0U)           /* Transmit Protocol Error. */
#define BITL_MAC_STATUS0_TXPE                                 (1U)           /* Transmit Protocol Error. */
#define BITM_MAC_STATUS0_TXPE                                 (0X00000001U)  /* Transmit Protocol Error. */
#define BITP_MAC_STATUS0_TXBOE                                (1U)           /* Host Tx FIFO Overflow. */
#define BITL_MAC_STATUS0_TXBOE                                (1U)           /* Host Tx FIFO Overflow. */
#define BITM_MAC_STATUS0_TXBOE                                (0X00000002U)  /* Host Tx FIFO Overflow. */
#define BITP_MAC_STATUS0_TXBUE                                (2U)           /* Host Tx FIFO Underrun Error. */
#define BITL_MAC_STATUS0_TXBUE                                (1U)           /* Host Tx FIFO Underrun Error. */
#define BITM_MAC_STATUS0_TXBUE                                (0X00000004U)  /* Host Tx FIFO Underrun Error. */
#define BITP_MAC_STATUS0_RXBOE                                (3U)           /* Receive Buffer Overflow Error. */
#define BITL_MAC_STATUS0_RXBOE                                (1U)           /* Receive Buffer Overflow Error. */
#define BITM_MAC_STATUS0_RXBOE                                (0X00000008U)  /* Receive Buffer Overflow Error. */
#define BITP_MAC_STATUS0_LOFE                                 (4U)           /* Loss of Frame Error. */
#define BITL_MAC_STATUS0_LOFE                                 (1U)           /* Loss of Frame Error. */
#define BITM_MAC_STATUS0_LOFE                                 (0X00000010U)  /* Loss of Frame Error. */
#define BITP_MAC_STATUS0_HDRE                                 (5U)           /* Header Error. */
#define BITL_MAC_STATUS0_HDRE                                 (1U)           /* Header Error. */
#define BITM_MAC_STATUS0_HDRE                                 (0X00000020U)  /* Header Error. */
#define BITP_MAC_STATUS0_RESETC                               (6U)           /* Reset Complete. */
#define BITL_MAC_STATUS0_RESETC                               (1U)           /* Reset Complete. */
#define BITM_MAC_STATUS0_RESETC                               (0X00000040U)  /* Reset Complete. */
#define BITP_MAC_STATUS0_PHYINT                               (7U)           /* PHY Interrupt for Port1. */
#define BITL_MAC_STATUS0_PHYINT                               (1U)           /* PHY Interrupt for Port1. */
#define BITM_MAC_STATUS0_PHYINT                               (0X00000080U)  /* PHY Interrupt for Port1. */
#define BITP_MAC_STATUS0_TTSCAA                               (8U)           /* Transmit Timestamp Capture Available A. */
#define BITL_MAC_STATUS0_TTSCAA                               (1U)           /* Transmit Timestamp Capture Available A. */
#define BITM_MAC_STATUS0_TTSCAA                               (0X00000100U)  /* Transmit Timestamp Capture Available A. */
#define BITP_MAC_STATUS0_TTSCAB                               (9U)           /* Transmit Timestamp Capture Available B. */
#define BITL_MAC_STATUS0_TTSCAB                               (1U)           /* Transmit Timestamp Capture Available B. */
#define BITM_MAC_STATUS0_TTSCAB                               (0X00000200U)  /* Transmit Timestamp Capture Available B. */
#define BITP_MAC_STATUS0_TTSCAC                               (10U)          /* Transmit Timestamp Capture Available C. */
#define BITL_MAC_STATUS0_TTSCAC                               (1U)           /* Transmit Timestamp Capture Available C. */
#define BITM_MAC_STATUS0_TTSCAC                               (0X00000400U)  /* Transmit Timestamp Capture Available C. */
#define BITP_MAC_STATUS0_TXFCSE                               (11U)          /* Transmit Frame Check Sequence Error. */
#define BITL_MAC_STATUS0_TXFCSE                               (1U)           /* Transmit Frame Check Sequence Error. */
#define BITM_MAC_STATUS0_TXFCSE                               (0X00000800U)  /* Transmit Frame Check Sequence Error. */
#define BITP_MAC_STATUS0_CDPE                                 (12U)          /* Control Data Protection Error. */
#define BITL_MAC_STATUS0_CDPE                                 (1U)           /* Control Data Protection Error. */
#define BITM_MAC_STATUS0_CDPE                                 (0X00001000U)  /* Control Data Protection Error. */

/* ----------------------------------------------------------------------------------------------------
          STATUS1                                               Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_STATUS1_P1_LINK_STATUS                       (0U)           /* Port 1 Link Status. */
#define BITL_MAC_STATUS1_P1_LINK_STATUS                       (1U)           /* Port 1 Link Status. */
#define BITM_MAC_STATUS1_P1_LINK_STATUS                       (0X00000001U)  /* Port 1 Link Status. */
#define BITP_MAC_STATUS1_LINK_CHANGE                          (1U)           /* Link Status Changed. */
#define BITL_MAC_STATUS1_LINK_CHANGE                          (1U)           /* Link Status Changed. */
#define BITM_MAC_STATUS1_LINK_CHANGE                          (0X00000002U)  /* Link Status Changed. */
#define BITP_MAC_STATUS1_TX_RDY                               (3U)           /* Tx Ready. */
#define BITL_MAC_STATUS1_TX_RDY                               (1U)           /* Tx Ready. */
#define BITM_MAC_STATUS1_TX_RDY                               (0X00000008U)  /* Tx Ready. */
#define BITP_MAC_STATUS1_P1_RX_RDY                            (4U)           /* Port 1 Rx FIFO Contains Data. */
#define BITL_MAC_STATUS1_P1_RX_RDY                            (1U)           /* Port 1 Rx FIFO Contains Data. */
#define BITM_MAC_STATUS1_P1_RX_RDY                            (0X00000010U)  /* Port 1 Rx FIFO Contains Data. */
#define BITP_MAC_STATUS1_P1_RX_RDY_HI                         (5U)           /* Port1 Rx Ready High Priority. */
#define BITL_MAC_STATUS1_P1_RX_RDY_HI                         (1U)           /* Port1 Rx Ready High Priority. */
#define BITM_MAC_STATUS1_P1_RX_RDY_HI                         (0X00000020U)  /* Port1 Rx Ready High Priority. */
#define BITP_MAC_STATUS1_P1_RX_IFG_ERR                        (8U)           /* Rx MAC Inter Frame Gap Error. */
#define BITL_MAC_STATUS1_P1_RX_IFG_ERR                        (1U)           /* Rx MAC Inter Frame Gap Error. */
#define BITM_MAC_STATUS1_P1_RX_IFG_ERR                        (0X00000100U)  /* Rx MAC Inter Frame Gap Error. */
#define BITP_MAC_STATUS1_SPI_ERR                              (10U)          /* Detected an Error on an SPI Transaction. */
#define BITL_MAC_STATUS1_SPI_ERR                              (1U)           /* Detected an Error on an SPI Transaction. */
#define BITM_MAC_STATUS1_SPI_ERR                              (0X00000400U)  /* Detected an Error on an SPI Transaction. */
#define BITP_MAC_STATUS1_RX_ECC_ERR                           (11U)          /* ECC Error on Reading the Frame Size from an Rx FIFO. */
#define BITL_MAC_STATUS1_RX_ECC_ERR                           (1U)           /* ECC Error on Reading the Frame Size from an Rx FIFO. */
#define BITM_MAC_STATUS1_RX_ECC_ERR                           (0X00000800U)  /* ECC Error on Reading the Frame Size from an Rx FIFO. */
#define BITP_MAC_STATUS1_TX_ECC_ERR                           (12U)          /* ECC Error on Reading the Frame Size from a Tx FIFO. */
#define BITL_MAC_STATUS1_TX_ECC_ERR                           (1U)           /* ECC Error on Reading the Frame Size from a Tx FIFO. */
#define BITM_MAC_STATUS1_TX_ECC_ERR                           (0X00001000U)  /* ECC Error on Reading the Frame Size from a Tx FIFO. */

#define ENUM_MAC_STATUS1_P1_LINK_STATUS_DOWN                  (0X00000000U)  /* Link Down. */
#define ENUM_MAC_STATUS1_P1_LINK_STATUS_UP                    (0X00000001U)  /* Link up. */

/* ----------------------------------------------------------------------------------------------------
          BUFSTS                                                Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_BUFSTS_RCA                                   (0U)           /* Receive Chunks Available. */
#define BITL_MAC_BUFSTS_RCA                                   (8U)           /* Receive Chunks Available. */
#define BITM_MAC_BUFSTS_RCA                                   (0X000000FFU)  /* Receive Chunks Available. */
#define BITP_MAC_BUFSTS_TXC                                   (8U)           /* Transmit Credits Available. */
#define BITL_MAC_BUFSTS_TXC                                   (8U)           /* Transmit Credits Available. */
#define BITM_MAC_BUFSTS_TXC                                   (0X0000FF00U)  /* Transmit Credits Available. */

/* ----------------------------------------------------------------------------------------------------
          IMASK0                                                Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_IMASK0_TXPEM                                 (0U)           /* Transmit Protocol Error Mask. */
#define BITL_MAC_IMASK0_TXPEM                                 (1U)           /* Transmit Protocol Error Mask. */
#define BITM_MAC_IMASK0_TXPEM                                 (0X00000001U)  /* Transmit Protocol Error Mask. */
#define BITP_MAC_IMASK0_TXBOEM                                (1U)           /* Transmit Buffer Overflow Error Mask. */
#define BITL_MAC_IMASK0_TXBOEM                                (1U)           /* Transmit Buffer Overflow Error Mask. */
#define BITM_MAC_IMASK0_TXBOEM                                (0X00000002U)  /* Transmit Buffer Overflow Error Mask. */
#define BITP_MAC_IMASK0_TXBUEM                                (2U)           /* Transmit Buffer Underflow Error Mask. */
#define BITL_MAC_IMASK0_TXBUEM                                (1U)           /* Transmit Buffer Underflow Error Mask. */
#define BITM_MAC_IMASK0_TXBUEM                                (0X00000004U)  /* Transmit Buffer Underflow Error Mask. */
#define BITP_MAC_IMASK0_RXBOEM                                (3U)           /* Receive Buffer Overflow Error Mask. */
#define BITL_MAC_IMASK0_RXBOEM                                (1U)           /* Receive Buffer Overflow Error Mask. */
#define BITM_MAC_IMASK0_RXBOEM                                (0X00000008U)  /* Receive Buffer Overflow Error Mask. */
#define BITP_MAC_IMASK0_LOFEM                                 (4U)           /* Loss of Frame Error Mask. */
#define BITL_MAC_IMASK0_LOFEM                                 (1U)           /* Loss of Frame Error Mask. */
#define BITM_MAC_IMASK0_LOFEM                                 (0X00000010U)  /* Loss of Frame Error Mask. */
#define BITP_MAC_IMASK0_HDREM                                 (5U)           /* Header Error Mask. */
#define BITL_MAC_IMASK0_HDREM                                 (1U)           /* Header Error Mask. */
#define BITM_MAC_IMASK0_HDREM                                 (0X00000020U)  /* Header Error Mask. */
#define BITP_MAC_IMASK0_RESETCM                               (6U)           /* RESET Complete Mask. */
#define BITL_MAC_IMASK0_RESETCM                               (1U)           /* RESET Complete Mask. */
#define BITM_MAC_IMASK0_RESETCM                               (0X00000040U)  /* RESET Complete Mask. */
#define BITP_MAC_IMASK0_PHYINTM                               (7U)           /* Physical Layer Interrupt Mask. */
#define BITL_MAC_IMASK0_PHYINTM                               (1U)           /* Physical Layer Interrupt Mask. */
#define BITM_MAC_IMASK0_PHYINTM                               (0X00000080U)  /* Physical Layer Interrupt Mask. */
#define BITP_MAC_IMASK0_TTSCAAM                               (8U)           /* Transmit Timestamp Capture Available A Mask. */
#define BITL_MAC_IMASK0_TTSCAAM                               (1U)           /* Transmit Timestamp Capture Available A Mask. */
#define BITM_MAC_IMASK0_TTSCAAM                               (0X00000100U)  /* Transmit Timestamp Capture Available A Mask. */
#define BITP_MAC_IMASK0_TTSCABM                               (9U)           /* Transmit Timestamp Capture Available B Mask. */
#define BITL_MAC_IMASK0_TTSCABM                               (1U)           /* Transmit Timestamp Capture Available B Mask. */
#define BITM_MAC_IMASK0_TTSCABM                               (0X00000200U)  /* Transmit Timestamp Capture Available B Mask. */
#define BITP_MAC_IMASK0_TTSCACM                               (10U)          /* Transmit Timestamp Capture Available C Mask. */
#define BITL_MAC_IMASK0_TTSCACM                               (1U)           /* Transmit Timestamp Capture Available C Mask. */
#define BITM_MAC_IMASK0_TTSCACM                               (0X00000400U)  /* Transmit Timestamp Capture Available C Mask. */
#define BITP_MAC_IMASK0_TXFCSEM                               (11U)          /* Transmit Frame Check Sequence Error Mask. */
#define BITL_MAC_IMASK0_TXFCSEM                               (1U)           /* Transmit Frame Check Sequence Error Mask. */
#define BITM_MAC_IMASK0_TXFCSEM                               (0X00000800U)  /* Transmit Frame Check Sequence Error Mask. */
#define BITP_MAC_IMASK0_CDPEM                                 (12U)          /* Control Data Protection Error Mask. */
#define BITL_MAC_IMASK0_CDPEM                                 (1U)           /* Control Data Protection Error Mask. */
#define BITM_MAC_IMASK0_CDPEM                                 (0X00001000U)  /* Control Data Protection Error Mask. */

/* ----------------------------------------------------------------------------------------------------
          IMASK1                                                Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_IMASK1_LINK_CHANGE_MASK                      (1U)           /* Mask Bit for LINK_CHANGE. */
#define BITL_MAC_IMASK1_LINK_CHANGE_MASK                      (1U)           /* Mask Bit for LINK_CHANGE. */
#define BITM_MAC_IMASK1_LINK_CHANGE_MASK                      (0X00000002U)  /* Mask Bit for LINK_CHANGE. */
#define BITP_MAC_IMASK1_TX_RDY_MASK                           (3U)           /* Mask Bit for TX_FRM_DONE. */
#define BITL_MAC_IMASK1_TX_RDY_MASK                           (1U)           /* Mask Bit for TX_FRM_DONE. */
#define BITM_MAC_IMASK1_TX_RDY_MASK                           (0X00000008U)  /* Mask Bit for TX_FRM_DONE. */
#define BITP_MAC_IMASK1_P1_RX_RDY_MASK                        (4U)           /* Mask Bit for P1_RX_RDY. */
#define BITL_MAC_IMASK1_P1_RX_RDY_MASK                        (1U)           /* Mask Bit for P1_RX_RDY. */
#define BITM_MAC_IMASK1_P1_RX_RDY_MASK                        (0X00000010U)  /* Mask Bit for P1_RX_RDY. */
#define BITP_MAC_IMASK1_P1_RX_IFG_ERR_MASK                    (8U)           /* Mask Bit for RX_IFG_ERR. */
#define BITL_MAC_IMASK1_P1_RX_IFG_ERR_MASK                    (1U)           /* Mask Bit for RX_IFG_ERR. */
#define BITM_MAC_IMASK1_P1_RX_IFG_ERR_MASK                    (0X00000100U)  /* Mask Bit for RX_IFG_ERR. */
#define BITP_MAC_IMASK1_SPI_ERR_MASK                          (10U)          /* Mask Bit for SPI_ERR. */
#define BITL_MAC_IMASK1_SPI_ERR_MASK                          (1U)           /* Mask Bit for SPI_ERR. */
#define BITM_MAC_IMASK1_SPI_ERR_MASK                          (0X00000400U)  /* Mask Bit for SPI_ERR. */
#define BITP_MAC_IMASK1_RX_ECC_ERR_MASK                       (11U)          /* Mask Bit for RXF_ECC_ERR. */
#define BITL_MAC_IMASK1_RX_ECC_ERR_MASK                       (1U)           /* Mask Bit for RXF_ECC_ERR. */
#define BITM_MAC_IMASK1_RX_ECC_ERR_MASK                       (0X00000800U)  /* Mask Bit for RXF_ECC_ERR. */
#define BITP_MAC_IMASK1_TX_ECC_ERR_MASK                       (12U)          /* Mask Bit for TXF_ECC_ERR. */
#define BITL_MAC_IMASK1_TX_ECC_ERR_MASK                       (1U)           /* Mask Bit for TXF_ECC_ERR. */
#define BITM_MAC_IMASK1_TX_ECC_ERR_MASK                       (0X00001000U)  /* Mask Bit for TXF_ECC_ERR. */

/* ----------------------------------------------------------------------------------------------------
          TTSCAH                                                Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_TTSCAH_TTSCH_A                               (0U)           /* Transmit Timestamp A Bits 63-32. */
#define BITL_MAC_TTSCAH_TTSCH_A                               (32U)          /* Transmit Timestamp A Bits 63-32. */
#define BITM_MAC_TTSCAH_TTSCH_A                               (0XFFFFFFFFU)  /* Transmit Timestamp A Bits 63-32. */

/* ----------------------------------------------------------------------------------------------------
          TTSCAL                                                Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_TTSCAL_TTSCL_A                               (0U)           /* Transmit Timestamp A Bits 31-0. */
#define BITL_MAC_TTSCAL_TTSCL_A                               (32U)          /* Transmit Timestamp A Bits 31-0. */
#define BITM_MAC_TTSCAL_TTSCL_A                               (0XFFFFFFFFU)  /* Transmit Timestamp A Bits 31-0. */

/* ----------------------------------------------------------------------------------------------------
          TTSCBH                                                Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_TTSCBH_TTSCH_B                               (0U)           /* Transmit Timestamp B Bits 63-32. */
#define BITL_MAC_TTSCBH_TTSCH_B                               (32U)          /* Transmit Timestamp B Bits 63-32. */
#define BITM_MAC_TTSCBH_TTSCH_B                               (0XFFFFFFFFU)  /* Transmit Timestamp B Bits 63-32. */

/* ----------------------------------------------------------------------------------------------------
          TTSCBL                                                Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_TTSCBL_TTSCL_B                               (0U)           /* Transmit Timestamp B Bits 31-0. */
#define BITL_MAC_TTSCBL_TTSCL_B                               (32U)          /* Transmit Timestamp B Bits 31-0. */
#define BITM_MAC_TTSCBL_TTSCL_B                               (0XFFFFFFFFU)  /* Transmit Timestamp B Bits 31-0. */

/* ----------------------------------------------------------------------------------------------------
          TTSCCH                                                Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_TTSCCH_TTSCH_C                               (0U)           /* Transmit Timestamp C Bits 63-32. */
#define BITL_MAC_TTSCCH_TTSCH_C                               (32U)          /* Transmit Timestamp C Bits 63-32. */
#define BITM_MAC_TTSCCH_TTSCH_C                               (0XFFFFFFFFU)  /* Transmit Timestamp C Bits 63-32. */

/* ----------------------------------------------------------------------------------------------------
          TTSCCL                                                Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_TTSCCL_TTSCL_C                               (0U)           /* Transmit Timestamp C Bits 31-0. */
#define BITL_MAC_TTSCCL_TTSCL_C                               (32U)          /* Transmit Timestamp C Bits 31-0. */
#define BITM_MAC_TTSCCL_TTSCL_C                               (0XFFFFFFFFU)  /* Transmit Timestamp C Bits 31-0. */

/* ----------------------------------------------------------------------------------------------------
          MDIOACC_0_                                            Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_MDIOACC_N__MDIO_DATA                         (0U)           /* Data/Address Value. */
#define BITL_MAC_MDIOACC_N__MDIO_DATA                         (16U)          /* Data/Address Value. */
#define BITM_MAC_MDIOACC_N__MDIO_DATA                         (0X0000FFFFU)  /* Data/Address Value. */
#define BITP_MAC_MDIOACC_N__MDIO_DEVAD                        (16U)          /* MDIO Device Address. */
#define BITL_MAC_MDIOACC_N__MDIO_DEVAD                        (5U)           /* MDIO Device Address. */
#define BITM_MAC_MDIOACC_N__MDIO_DEVAD                        (0X001F0000U)  /* MDIO Device Address. */
#define BITP_MAC_MDIOACC_N__MDIO_PRTAD                        (21U)          /* MDIO Port Address. */
#define BITL_MAC_MDIOACC_N__MDIO_PRTAD                        (5U)           /* MDIO Port Address. */
#define BITM_MAC_MDIOACC_N__MDIO_PRTAD                        (0X03E00000U)  /* MDIO Port Address. */
#define BITP_MAC_MDIOACC_N__MDIO_OP                           (26U)          /* Operation Code. */
#define BITL_MAC_MDIOACC_N__MDIO_OP                           (2U)           /* Operation Code. */
#define BITM_MAC_MDIOACC_N__MDIO_OP                           (0X0C000000U)  /* Operation Code. */
#define BITP_MAC_MDIOACC_N__MDIO_ST                           (28U)          /* Start of Frame. */
#define BITL_MAC_MDIOACC_N__MDIO_ST                           (2U)           /* Start of Frame. */
#define BITM_MAC_MDIOACC_N__MDIO_ST                           (0X30000000U)  /* Start of Frame. */
#define BITP_MAC_MDIOACC_N__MDIO_TAERR                        (30U)          /* Turnaround Error. */
#define BITL_MAC_MDIOACC_N__MDIO_TAERR                        (1U)           /* Turnaround Error. */
#define BITM_MAC_MDIOACC_N__MDIO_TAERR                        (0X40000000U)  /* Turnaround Error. */
#define BITP_MAC_MDIOACC_N__MDIO_TRDONE                       (31U)          /* Transaction Done. */
#define BITL_MAC_MDIOACC_N__MDIO_TRDONE                       (1U)           /* Transaction Done. */
#define BITM_MAC_MDIOACC_N__MDIO_TRDONE                       (0X80000000U)  /* Transaction Done. */

#define ENUM_MAC_MDIOACC_N__MDIO_ST_CLAUSE22                  (0X00000001U)  /* No description provided */
#define ENUM_MAC_MDIOACC_N__MDIO_ST_CLAUSE45                  (0X00000000U)  /* No description provided */
#define ENUM_MAC_MDIOACC_N__MDIO_OP_MD_ADDR                   (0X00000000U)  /* MD Address Command. */
#define ENUM_MAC_MDIOACC_N__MDIO_OP_MD_WR                     (0X00000001U)  /* Write Command. */
#define ENUM_MAC_MDIOACC_N__MDIO_OP_MD_RD                     (0X00000003U)  /* Read Command. */
#define ENUM_MAC_MDIOACC_N__MDIO_OP_MD_INC_RD                 (0X00000002U)  /* Incremental Read Command. */

/* ----------------------------------------------------------------------------------------------------
          TX_FSIZE                                              Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_TX_FSIZE_TX_FRM_SIZE                         (0U)           /* Transmit Frame Size. */
#define BITL_MAC_TX_FSIZE_TX_FRM_SIZE                         (11U)          /* Transmit Frame Size. */
#define BITM_MAC_TX_FSIZE_TX_FRM_SIZE                         (0X000007FFU)  /* Transmit Frame Size. */

/* ----------------------------------------------------------------------------------------------------
          TX                                                    Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_TX_TDR                                       (0U)           /* Transmit Data Register. */
#define BITL_MAC_TX_TDR                                       (32U)          /* Transmit Data Register. */
#define BITM_MAC_TX_TDR                                       (0XFFFFFFFFU)  /* Transmit Data Register. */

/* ----------------------------------------------------------------------------------------------------
          TX_SPACE                                              Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_TX_SPACE_TX_SPACE                            (0U)           /* Transmit FIFO Space Available in Half Words (16 Bits). */
#define BITL_MAC_TX_SPACE_TX_SPACE                            (14U)          /* Transmit FIFO Space Available in Half Words (16 Bits). */
#define BITM_MAC_TX_SPACE_TX_SPACE                            (0X00003FFFU)  /* Transmit FIFO Space Available in Half Words (16 Bits). */

/* ----------------------------------------------------------------------------------------------------
          TX_THRESH                                             Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_TX_THRESH_HOST_TX_THRESH                     (0U)           /* Host Transmit Start Threshold in Cut Through. */
#define BITL_MAC_TX_THRESH_HOST_TX_THRESH                     (6U)           /* Host Transmit Start Threshold in Cut Through. */
#define BITM_MAC_TX_THRESH_HOST_TX_THRESH                     (0X0000003FU)  /* Host Transmit Start Threshold in Cut Through. */

/* ----------------------------------------------------------------------------------------------------
          FIFO_CLR                                              Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_FIFO_CLR_MAC_RXF_CLR                         (0U)           /* Clear the Receive FIFO(s). */
#define BITL_MAC_FIFO_CLR_MAC_RXF_CLR                         (1U)           /* Clear the Receive FIFO(s). */
#ifndef BITM_MAC_FIFO_CLR_MAC_RXF_CLR
#define BITM_MAC_FIFO_CLR_MAC_RXF_CLR                         (0X00000001U)  /* Clear the Receive FIFO(s). */
#endif
#define BITP_MAC_FIFO_CLR_MAC_TXF_CLR                         (1U)           /* Clear the Host Transmit FIFO. */
#define BITL_MAC_FIFO_CLR_MAC_TXF_CLR                         (1U)           /* Clear the Host Transmit FIFO. */
#ifndef BITM_MAC_FIFO_CLR_MAC_TXF_CLR
#define BITM_MAC_FIFO_CLR_MAC_TXF_CLR                         (0X00000002U)  /* Clear the Host Transmit FIFO. */
#endif

/* ----------------------------------------------------------------------------------------------------
          SCRATCH_0_                                            Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_SCRATCH_N__SCRATCH_DATA                      (0U)           /* Scratch Data. */
#define BITL_MAC_SCRATCH_N__SCRATCH_DATA                      (32U)          /* Scratch Data. */
#define BITM_MAC_SCRATCH_N__SCRATCH_DATA                      (0XFFFFFFFFU)  /* Scratch Data. */

/* ----------------------------------------------------------------------------------------------------
          MAC_RST_STATUS                                        Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_MAC_RST_STATUS_MAC_OSC_CLK_RDY               (0U)           /* MAC Oscillator Clock Ready. */
#define BITL_MAC_MAC_RST_STATUS_MAC_OSC_CLK_RDY               (1U)           /* MAC Oscillator Clock Ready. */
#define BITM_MAC_MAC_RST_STATUS_MAC_OSC_CLK_RDY               (0X00000001U)  /* MAC Oscillator Clock Ready. */
#define BITP_MAC_MAC_RST_STATUS_MAC_CRYSL_CLK_RDY             (1U)           /* MAC Crystal Clock Ready. */
#define BITL_MAC_MAC_RST_STATUS_MAC_CRYSL_CLK_RDY             (1U)           /* MAC Crystal Clock Ready. */
#define BITM_MAC_MAC_RST_STATUS_MAC_CRYSL_CLK_RDY             (0X00000002U)  /* MAC Crystal Clock Ready. */

/* ----------------------------------------------------------------------------------------------------
          SOFT_RST                                              Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_SOFT_RST_RST_KEY                             (0U)           /* Software Reset. */
#define BITL_MAC_SOFT_RST_RST_KEY                             (16U)          /* Software Reset. */
#define BITM_MAC_SOFT_RST_RST_KEY                             (0X0000FFFFU)  /* Software Reset. */

#define ENUM_MAC_SOFT_RST_RST_KEY_RST_MAC_ONLY_KEY1           (0X00004F1CU)  /* Key1 to Reset the MAC Logic Only. */
#define ENUM_MAC_SOFT_RST_RST_KEY_RST_MAC_ONLY_KEY2           (0X0000C1F4U)  /* Key 2 to Reset the MAC Logic Only. */
#define ENUM_MAC_SOFT_RST_RST_KEY_MAC_RST_EXIT_REQ_KEY1       (0X00006F1AU)  /* Key 1 to Request Release of Reset to the MAC Core Logic. */
#define ENUM_MAC_SOFT_RST_RST_KEY_MAC_RST_EXIT_REQ_KEY2       (0X0000A1F6U)  /* Key 2 to Request Release of Reset to the MAC Core Logic. */

/* ----------------------------------------------------------------------------------------------------
          SPI_INJ_ERR                                           Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_SPI_INJ_ERR_TEST_SPI_INJ_ERR                 (0U)           /* Inject an Error on the SPI MISO Path. */
#define BITL_MAC_SPI_INJ_ERR_TEST_SPI_INJ_ERR                 (1U)           /* Inject an Error on the SPI MISO Path. */
#define BITM_MAC_SPI_INJ_ERR_TEST_SPI_INJ_ERR                 (0X00000001U)  /* Inject an Error on the SPI MISO Path. */

/* ----------------------------------------------------------------------------------------------------
          FIFO_SIZE                                             Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_FIFO_SIZE_HTX_SIZE                           (0U)           /* Host Transmit FIFO Size. */
#define BITL_MAC_FIFO_SIZE_HTX_SIZE                           (4U)           /* Host Transmit FIFO Size. */
#define BITM_MAC_FIFO_SIZE_HTX_SIZE                           (0X0000000FU)  /* Host Transmit FIFO Size. */
#define BITP_MAC_FIFO_SIZE_P1_RX_LO_SIZE                      (4U)           /* Port 1 Rx Low Priority FIFO Size. */
#define BITL_MAC_FIFO_SIZE_P1_RX_LO_SIZE                      (4U)           /* Port 1 Rx Low Priority FIFO Size. */
#define BITM_MAC_FIFO_SIZE_P1_RX_LO_SIZE                      (0X000000F0U)  /* Port 1 Rx Low Priority FIFO Size. */
#define BITP_MAC_FIFO_SIZE_P1_RX_HI_SIZE                      (8U)           /* Port 1 Rx High Priority FIFO Size. */
#define BITL_MAC_FIFO_SIZE_P1_RX_HI_SIZE                      (4U)           /* Port 1 Rx High Priority FIFO Size. */
#define BITM_MAC_FIFO_SIZE_P1_RX_HI_SIZE                      (0X00000F00U)  /* Port 1 Rx High Priority FIFO Size. */

#define ENUM_MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_0K            (0X00000000U)  /* 0K Bytes. */
#define ENUM_MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_2K            (0X00000001U)  /* 2K Bytes. */
#define ENUM_MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_4K            (0X00000002U)  /* 4k Bytes. */
#define ENUM_MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_6K            (0X00000003U)  /* 6k Bytes. */
#define ENUM_MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_8K            (0X00000004U)  /* 8k Bytes. */
#define ENUM_MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_10K           (0X00000005U)  /* 10k Bytes. */
#define ENUM_MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_12K           (0X00000006U)  /* 12k Bytes. */
#define ENUM_MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_14K           (0X00000007U)  /* 14k Bytes. */
#define ENUM_MAC_FIFO_SIZE_P1_RX_HI_SIZE_RXSIZE_16K           (0X00000008U)  /* 16k Bytes. */
#define ENUM_MAC_FIFO_SIZE_P1_RX_LO_SIZE_RXSIZE_0K            (0X00000000U)  /* 0K Bytes. */
#define ENUM_MAC_FIFO_SIZE_P1_RX_LO_SIZE_RXSIZE_2K            (0X00000001U)  /* 2k Bytes. */
#define ENUM_MAC_FIFO_SIZE_P1_RX_LO_SIZE_RXSIZE_4K            (0X00000002U)  /* 4k Bytes. */
#define ENUM_MAC_FIFO_SIZE_P1_RX_LO_SIZE_RXSIZE_6K            (0X00000003U)  /* 6k Bytes. */
#define ENUM_MAC_FIFO_SIZE_P1_RX_LO_SIZE_RXSIZE_8K            (0X00000004U)  /* 8k Bytes. */
#define ENUM_MAC_FIFO_SIZE_P1_RX_LO_SIZE_RXSIZE_10K           (0X00000005U)  /* 10k Bytes. */
#define ENUM_MAC_FIFO_SIZE_P1_RX_LO_SIZE_RXSIZE_12K           (0X00000006U)  /* 12k Bytes. */
#define ENUM_MAC_FIFO_SIZE_P1_RX_LO_SIZE_RXSIZE_14K           (0X00000007U)  /* 14k Bytes. */
#define ENUM_MAC_FIFO_SIZE_P1_RX_LO_SIZE_RXSIZE_16K           (0X00000008U)  /* 16k Bytes. */
#define ENUM_MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_0K                 (0X00000000U)  /* 0k Bytes. */
#define ENUM_MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_2K                 (0X00000001U)  /* 2k Bytes. */
#define ENUM_MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_4K                 (0X00000002U)  /* 4k Bytes. */
#define ENUM_MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_6K                 (0X00000003U)  /* 6k Bytes. */
#define ENUM_MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_8K                 (0X00000004U)  /* 8k Bytes. */
#define ENUM_MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_10K                (0X00000005U)  /* 10k Bytes. */
#define ENUM_MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_12K                (0X00000006U)  /* 12k Bytes. */
#define ENUM_MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_14K                (0X00000007U)  /* 14k Bytes. */
#define ENUM_MAC_FIFO_SIZE_HTX_SIZE_TXSIZE_16K                (0X00000008U)  /* 16k Byte. */

/* ----------------------------------------------------------------------------------------------------
          TFC                                                   Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_TFC_TFC                                      (0U)           /* Number of Frames in the Tx FIFO. */
#define BITL_MAC_TFC_TFC                                      (9U)           /* Number of Frames in the Tx FIFO. */
#define BITM_MAC_TFC_TFC                                      (0X000001FFU)  /* Number of Frames in the Tx FIFO. */

/* ----------------------------------------------------------------------------------------------------
          TXSIZE                                                Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_TXSIZE_TX_SIZE                               (0U)           /* Data in the Tx FIFO. Number of Half Words(16 Bit). */
#define BITL_MAC_TXSIZE_TX_SIZE                               (14U)          /* Data in the Tx FIFO. Number of Half Words(16 Bit). */
#define BITM_MAC_TXSIZE_TX_SIZE                               (0X00003FFFU)  /* Data in the Tx FIFO. Number of Half Words(16 Bit). */

/* ----------------------------------------------------------------------------------------------------
          HTX_OVF_FRM_CNT                                       Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_HTX_OVF_FRM_CNT_HTX_OVF_FRM_CNT              (0U)           /* Counts Host Tx Frames Dropped Due to FIFO Overflow. */
#define BITL_MAC_HTX_OVF_FRM_CNT_HTX_OVF_FRM_CNT              (32U)          /* Counts Host Tx Frames Dropped Due to FIFO Overflow. */
#define BITM_MAC_HTX_OVF_FRM_CNT_HTX_OVF_FRM_CNT              (0XFFFFFFFFU)  /* Counts Host Tx Frames Dropped Due to FIFO Overflow. */

/* ----------------------------------------------------------------------------------------------------
          MECC_ERR_ADDR                                         Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_MECC_ERR_ADDR_MECC_ERR_ADDR                  (0U)           /* Address of an Uncorrectable ECC Error in Memory. */
#define BITL_MAC_MECC_ERR_ADDR_MECC_ERR_ADDR                  (14U)          /* Address of an Uncorrectable ECC Error in Memory. */
#define BITM_MAC_MECC_ERR_ADDR_MECC_ERR_ADDR                  (0X00003FFFU)  /* Address of an Uncorrectable ECC Error in Memory. */

/* ----------------------------------------------------------------------------------------------------
          CECC_ERR_0_                                           Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_CECC_ERR_N__CECC_ERR_CNT                     (0U)           /* Corrected ECC Error Count. */
#define BITL_MAC_CECC_ERR_N__CECC_ERR_CNT                     (10U)          /* Corrected ECC Error Count. */
#define BITM_MAC_CECC_ERR_N__CECC_ERR_CNT                     (0X000003FFU)  /* Corrected ECC Error Count. */

/* ----------------------------------------------------------------------------------------------------
          ADDR_FILT_UPR_0_                                      Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_ADDR_FILT_UPR_N__MAC_ADDR                    (0U)           /* MAC Address. */
#define BITL_MAC_ADDR_FILT_UPR_N__MAC_ADDR                    (16U)          /* MAC Address. */
#define BITM_MAC_ADDR_FILT_UPR_N__MAC_ADDR                    (0X0000FFFFU)  /* MAC Address. */
#define BITP_MAC_ADDR_FILT_UPR_N__TO_HOST                     (16U)          /* Forward Frames Matching This MAC Address to the Host. */
#define BITL_MAC_ADDR_FILT_UPR_N__TO_HOST                     (1U)           /* Forward Frames Matching This MAC Address to the Host. */
#define BITM_MAC_ADDR_FILT_UPR_N__TO_HOST                     (0X00010000U)  /* Forward Frames Matching This MAC Address to the Host. */
#define BITP_MAC_ADDR_FILT_UPR_N__HOST_PRI                    (19U)          /* Host Rx Port Priority. */
#define BITL_MAC_ADDR_FILT_UPR_N__HOST_PRI                    (1U)           /* Host Rx Port Priority. */
#define BITM_MAC_ADDR_FILT_UPR_N__HOST_PRI                    (0X00080000U)  /* Host Rx Port Priority. */
#define BITP_MAC_ADDR_FILT_UPR_N__APPLY2PORT1                 (30U)          /* Apply to Port 1. */
#define BITL_MAC_ADDR_FILT_UPR_N__APPLY2PORT1                 (1U)           /* Apply to Port 1. */
#define BITM_MAC_ADDR_FILT_UPR_N__APPLY2PORT1                 (0X40000000U)  /* Apply to Port 1. */

#define ENUM_MAC_ADDR_FILT_UPR_N__APPLY2PORT1_NOTAPPLY        (0X00000000U)  /* Do Not Apply to Port 1. */
#define ENUM_MAC_ADDR_FILT_UPR_N__APPLY2PORT1_APPLY           (0X00000001U)  /* Apply to Port 1. */

/* ----------------------------------------------------------------------------------------------------
          ADDR_FILT_LWR_0_                                      Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_ADDR_FILT_LWR_N__MAC_ADDR                    (0U)           /* MAC Address. */
#define BITL_MAC_ADDR_FILT_LWR_N__MAC_ADDR                    (32U)          /* MAC Address. */
#define BITM_MAC_ADDR_FILT_LWR_N__MAC_ADDR                    (0XFFFFFFFFU)  /* MAC Address. */

/* ----------------------------------------------------------------------------------------------------
          ADDR_MSK_UPR_0_                                       Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_ADDR_MSK_UPR_N__MAC_ADDR_MASK                (0U)           /* MAC Address Bit Mask for the Address Table. */
#define BITL_MAC_ADDR_MSK_UPR_N__MAC_ADDR_MASK                (16U)          /* MAC Address Bit Mask for the Address Table. */
#define BITM_MAC_ADDR_MSK_UPR_N__MAC_ADDR_MASK                (0X0000FFFFU)  /* MAC Address Bit Mask for the Address Table. */

/* ----------------------------------------------------------------------------------------------------
          ADDR_MSK_LWR_0_                                       Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_ADDR_MSK_LWR_N__MAC_ADDR_MASK                (0U)           /* MAC Address Bit Mask for the Address Table. */
#define BITL_MAC_ADDR_MSK_LWR_N__MAC_ADDR_MASK                (32U)          /* MAC Address Bit Mask for the Address Table. */
#define BITM_MAC_ADDR_MSK_LWR_N__MAC_ADDR_MASK                (0XFFFFFFFFU)  /* MAC Address Bit Mask for the Address Table. */

/* ----------------------------------------------------------------------------------------------------
          TS_ADDEND                                             Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_TS_ADDEND_TS_ADDEND                          (0U)           /* Timestamp Accumulator Addend. */
#define BITL_MAC_TS_ADDEND_TS_ADDEND                          (32U)          /* Timestamp Accumulator Addend. */
#define BITM_MAC_TS_ADDEND_TS_ADDEND                          (0XFFFFFFFFU)  /* Timestamp Accumulator Addend. */

/* ----------------------------------------------------------------------------------------------------
          TS_1SEC_CMP                                           Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_TS_1SEC_CMP_TS_1SEC_CMP                      (0U)           /* Timestamp 1 Second Compare Value. */
#define BITL_MAC_TS_1SEC_CMP_TS_1SEC_CMP                      (32U)          /* Timestamp 1 Second Compare Value. */
#define BITM_MAC_TS_1SEC_CMP_TS_1SEC_CMP                      (0XFFFFFFFFU)  /* Timestamp 1 Second Compare Value. */

/* ----------------------------------------------------------------------------------------------------
          TS_SEC_CNT                                            Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_TS_SEC_CNT_TS_SEC_CNT                        (0U)           /* Write to the Seconds Counter. */
#define BITL_MAC_TS_SEC_CNT_TS_SEC_CNT                        (32U)          /* Write to the Seconds Counter. */
#define BITM_MAC_TS_SEC_CNT_TS_SEC_CNT                        (0XFFFFFFFFU)  /* Write to the Seconds Counter. */

/* ----------------------------------------------------------------------------------------------------
          TS_NS_CNT                                             Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_TS_NS_CNT_TS_NS_CNT                          (0U)           /* Write to the Nanoseconds Counter. */
#define BITL_MAC_TS_NS_CNT_TS_NS_CNT                          (32U)          /* Write to the Nanoseconds Counter. */
#define BITM_MAC_TS_NS_CNT_TS_NS_CNT                          (0XFFFFFFFFU)  /* Write to the Nanoseconds Counter. */

/* ----------------------------------------------------------------------------------------------------
          TS_CFG                                                Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_TS_CFG_TS_EN                                 (0U)           /* Enable the 1588 Timestamp Counter. */
#define BITL_MAC_TS_CFG_TS_EN                                 (1U)           /* Enable the 1588 Timestamp Counter. */
#define BITM_MAC_TS_CFG_TS_EN                                 (0X00000001U)  /* Enable the 1588 Timestamp Counter. */
#define BITP_MAC_TS_CFG_TS_CLR                                (1U)           /* Clear the 1588 Timestamp Counters. */
#define BITL_MAC_TS_CFG_TS_CLR                                (1U)           /* Clear the 1588 Timestamp Counters. */
#define BITM_MAC_TS_CFG_TS_CLR                                (0X00000002U)  /* Clear the 1588 Timestamp Counters. */
#define BITP_MAC_TS_CFG_TS_TIMER_DEF                          (2U)           /* The Default Value for the TS_TIMER Output. */
#define BITL_MAC_TS_CFG_TS_TIMER_DEF                          (1U)           /* The Default Value for the TS_TIMER Output. */
#define BITM_MAC_TS_CFG_TS_TIMER_DEF                          (0X00000004U)  /* The Default Value for the TS_TIMER Output. */
#define BITP_MAC_TS_CFG_TS_TIMER_STOP                         (3U)           /* Stop Toggling the TS_TIMER Output. */
#define BITL_MAC_TS_CFG_TS_TIMER_STOP                         (1U)           /* Stop Toggling the TS_TIMER Output. */
#define BITM_MAC_TS_CFG_TS_TIMER_STOP                         (0X00000008U)  /* Stop Toggling the TS_TIMER Output. */
#define BITP_MAC_TS_CFG_TS_CAPT_FREE_CNT                      (4U)           /* Capture the Free Running Counter. */
#define BITL_MAC_TS_CFG_TS_CAPT_FREE_CNT                      (1U)           /* Capture the Free Running Counter. */
#define BITM_MAC_TS_CFG_TS_CAPT_FREE_CNT                      (0X00000010U)  /* Capture the Free Running Counter. */

/* ----------------------------------------------------------------------------------------------------
          TS_TIMER_HI                                           Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_TS_TIMER_HI_TS_TIMER_HI                      (0U)           /* TS_TIMER High Period. */
#define BITL_MAC_TS_TIMER_HI_TS_TIMER_HI                      (32U)          /* TS_TIMER High Period. */
#define BITM_MAC_TS_TIMER_HI_TS_TIMER_HI                      (0XFFFFFFFFU)  /* TS_TIMER High Period. */

/* ----------------------------------------------------------------------------------------------------
          TS_TIMER_LO                                           Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_TS_TIMER_LO_TS_TIMER_LO                      (0U)           /* TS_TIMER Low Period. */
#define BITL_MAC_TS_TIMER_LO_TS_TIMER_LO                      (32U)          /* TS_TIMER Low Period. */
#define BITM_MAC_TS_TIMER_LO_TS_TIMER_LO                      (0XFFFFFFFFU)  /* TS_TIMER Low Period. */

/* ----------------------------------------------------------------------------------------------------
          TS_TIMER_QE_CORR                                      Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_TS_TIMER_QE_CORR_TS_TIMER_QE_CORR            (0U)           /* TS_TIMER Quantization Error Correction Value. */
#define BITL_MAC_TS_TIMER_QE_CORR_TS_TIMER_QE_CORR            (8U)           /* TS_TIMER Quantization Error Correction Value. */
#define BITM_MAC_TS_TIMER_QE_CORR_TS_TIMER_QE_CORR            (0X000000FFU)  /* TS_TIMER Quantization Error Correction Value. */

/* ----------------------------------------------------------------------------------------------------
          TS_TIMER_START                                        Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_TS_TIMER_START_TS_TSTART                     (0U)           /* Point in Time at Which to Start the TS_TIMER Counter. */
#define BITL_MAC_TS_TIMER_START_TS_TSTART                     (32U)          /* Point in Time at Which to Start the TS_TIMER Counter. */
#define BITM_MAC_TS_TIMER_START_TS_TSTART                     (0XFFFFFFFFU)  /* Point in Time at Which to Start the TS_TIMER Counter. */

/* ----------------------------------------------------------------------------------------------------
          TS_EXT_CAPT0                                          Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_TS_EXT_CAPT0_TS_EXT_CAPTD                    (0U)           /* Timestamp Captured on the Assertion of TS_CAPT Pin. */
#define BITL_MAC_TS_EXT_CAPT0_TS_EXT_CAPTD                    (32U)          /* Timestamp Captured on the Assertion of TS_CAPT Pin. */
#define BITM_MAC_TS_EXT_CAPT0_TS_EXT_CAPTD                    (0XFFFFFFFFU)  /* Timestamp Captured on the Assertion of TS_CAPT Pin. */

/* ----------------------------------------------------------------------------------------------------
          TS_EXT_CAPT1                                          Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_TS_EXT_CAPT1_TS_EXT_CAPTD                    (0U)           /* Timestamp Captured on the Assertion of TS_CAPT Pin. */
#define BITL_MAC_TS_EXT_CAPT1_TS_EXT_CAPTD                    (32U)          /* Timestamp Captured on the Assertion of TS_CAPT Pin. */
#define BITM_MAC_TS_EXT_CAPT1_TS_EXT_CAPTD                    (0XFFFFFFFFU)  /* Timestamp Captured on the Assertion of TS_CAPT Pin. */

/* ----------------------------------------------------------------------------------------------------
          TS_FREECNT_CAPT                                       Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_TS_FREECNT_CAPT_TS_CNT_CAPTD                 (0U)           /* Captured Free Running Counter. */
#define BITL_MAC_TS_FREECNT_CAPT_TS_CNT_CAPTD                 (32U)          /* Captured Free Running Counter. */
#define BITM_MAC_TS_FREECNT_CAPT_TS_CNT_CAPTD                 (0XFFFFFFFFU)  /* Captured Free Running Counter. */

/* ----------------------------------------------------------------------------------------------------
          P1_RX_FSIZE                                           Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_RX_FSIZE_P1_RX_FRM_SIZE                   (0U)           /* Receive Frame Size. */
#define BITL_MAC_P1_RX_FSIZE_P1_RX_FRM_SIZE                   (11U)          /* Receive Frame Size. */
#define BITM_MAC_P1_RX_FSIZE_P1_RX_FRM_SIZE                   (0X000007FFU)  /* Receive Frame Size. */

/* ----------------------------------------------------------------------------------------------------
          P1_RX                                                 Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_RX_P1_RDR                                 (0U)           /* Receive Data Register. */
#define BITL_MAC_P1_RX_P1_RDR                                 (32U)          /* Receive Data Register. */
#define BITM_MAC_P1_RX_P1_RDR                                 (0XFFFFFFFFU)  /* Receive Data Register. */

/* ----------------------------------------------------------------------------------------------------
          P1_RX_FRM_CNT                                         Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_RX_FRM_CNT_P1_RX_FRM_CNT                  (0U)           /* Rx Frame Count. */
#define BITL_MAC_P1_RX_FRM_CNT_P1_RX_FRM_CNT                  (32U)          /* Rx Frame Count. */
#define BITM_MAC_P1_RX_FRM_CNT_P1_RX_FRM_CNT                  (0XFFFFFFFFU)  /* Rx Frame Count. */

/* ----------------------------------------------------------------------------------------------------
          P1_RX_BCAST_CNT                                       Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_RX_BCAST_CNT_P1_RX_BCAST_CNT              (0U)           /* Rx Broadcast Frame Count. */
#define BITL_MAC_P1_RX_BCAST_CNT_P1_RX_BCAST_CNT              (32U)          /* Rx Broadcast Frame Count. */
#define BITM_MAC_P1_RX_BCAST_CNT_P1_RX_BCAST_CNT              (0XFFFFFFFFU)  /* Rx Broadcast Frame Count. */

/* ----------------------------------------------------------------------------------------------------
          P1_RX_MCAST_CNT                                       Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_RX_MCAST_CNT_P1_RX_MCAST_CNT              (0U)           /* Rx Multicast Frame Count. */
#define BITL_MAC_P1_RX_MCAST_CNT_P1_RX_MCAST_CNT              (32U)          /* Rx Multicast Frame Count. */
#define BITM_MAC_P1_RX_MCAST_CNT_P1_RX_MCAST_CNT              (0XFFFFFFFFU)  /* Rx Multicast Frame Count. */

/* ----------------------------------------------------------------------------------------------------
          P1_RX_UCAST_CNT                                       Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_RX_UCAST_CNT_P1_RX_UCAST_CNT              (0U)           /* Rx Unicast Frame Count. */
#define BITL_MAC_P1_RX_UCAST_CNT_P1_RX_UCAST_CNT              (32U)          /* Rx Unicast Frame Count. */
#define BITM_MAC_P1_RX_UCAST_CNT_P1_RX_UCAST_CNT              (0XFFFFFFFFU)  /* Rx Unicast Frame Count. */

/* ----------------------------------------------------------------------------------------------------
          P1_RX_CRC_ERR_CNT                                     Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_RX_CRC_ERR_CNT_P1_RX_CRC_ERR_CNT          (0U)           /* Rx CRC Errored Frame Count. */
#define BITL_MAC_P1_RX_CRC_ERR_CNT_P1_RX_CRC_ERR_CNT          (32U)          /* Rx CRC Errored Frame Count. */
#define BITM_MAC_P1_RX_CRC_ERR_CNT_P1_RX_CRC_ERR_CNT          (0XFFFFFFFFU)  /* Rx CRC Errored Frame Count. */

/* ----------------------------------------------------------------------------------------------------
          P1_RX_ALGN_ERR_CNT                                    Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_RX_ALGN_ERR_CNT_P1_RX_ALGN_ERR_CNT        (0U)           /* Rx Align Error Count. */
#define BITL_MAC_P1_RX_ALGN_ERR_CNT_P1_RX_ALGN_ERR_CNT        (32U)          /* Rx Align Error Count. */
#define BITM_MAC_P1_RX_ALGN_ERR_CNT_P1_RX_ALGN_ERR_CNT        (0XFFFFFFFFU)  /* Rx Align Error Count. */

/* ----------------------------------------------------------------------------------------------------
          P1_RX_LS_ERR_CNT                                      Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_RX_LS_ERR_CNT_P1_RX_LS_ERR_CNT            (0U)           /* Rx Long/Short Frame Error Count. */
#define BITL_MAC_P1_RX_LS_ERR_CNT_P1_RX_LS_ERR_CNT            (32U)          /* Rx Long/Short Frame Error Count. */
#define BITM_MAC_P1_RX_LS_ERR_CNT_P1_RX_LS_ERR_CNT            (0XFFFFFFFFU)  /* Rx Long/Short Frame Error Count. */

/* ----------------------------------------------------------------------------------------------------
          P1_RX_PHY_ERR_CNT                                     Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_RX_PHY_ERR_CNT_P1_RX_PHY_ERR_CNT          (0U)           /* Rx PHY Error Count. */
#define BITL_MAC_P1_RX_PHY_ERR_CNT_P1_RX_PHY_ERR_CNT          (32U)          /* Rx PHY Error Count. */
#define BITM_MAC_P1_RX_PHY_ERR_CNT_P1_RX_PHY_ERR_CNT          (0XFFFFFFFFU)  /* Rx PHY Error Count. */

/* ----------------------------------------------------------------------------------------------------
          P1_TX_FRM_CNT                                         Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_TX_FRM_CNT_P1_TX_FRM_CNT                  (0U)           /* Tx Frame Count. */
#define BITL_MAC_P1_TX_FRM_CNT_P1_TX_FRM_CNT                  (32U)          /* Tx Frame Count. */
#define BITM_MAC_P1_TX_FRM_CNT_P1_TX_FRM_CNT                  (0XFFFFFFFFU)  /* Tx Frame Count. */

/* ----------------------------------------------------------------------------------------------------
          P1_TX_BCAST_CNT                                       Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_TX_BCAST_CNT_P1_TX_BCAST_CNT              (0U)           /* Tx Broadcast Frame Count. */
#define BITL_MAC_P1_TX_BCAST_CNT_P1_TX_BCAST_CNT              (32U)          /* Tx Broadcast Frame Count. */
#define BITM_MAC_P1_TX_BCAST_CNT_P1_TX_BCAST_CNT              (0XFFFFFFFFU)  /* Tx Broadcast Frame Count. */

/* ----------------------------------------------------------------------------------------------------
          P1_TX_MCAST_CNT                                       Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_TX_MCAST_CNT_P1_TX_MCAST_CNT              (0U)           /* Tx Multicast Frame Count. */
#define BITL_MAC_P1_TX_MCAST_CNT_P1_TX_MCAST_CNT              (32U)          /* Tx Multicast Frame Count. */
#define BITM_MAC_P1_TX_MCAST_CNT_P1_TX_MCAST_CNT              (0XFFFFFFFFU)  /* Tx Multicast Frame Count. */

/* ----------------------------------------------------------------------------------------------------
          P1_TX_UCAST_CNT                                       Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_TX_UCAST_CNT_P1_TX_UCAST_CNT              (0U)           /* Tx Unicast Frame Count. */
#define BITL_MAC_P1_TX_UCAST_CNT_P1_TX_UCAST_CNT              (32U)          /* Tx Unicast Frame Count. */
#define BITM_MAC_P1_TX_UCAST_CNT_P1_TX_UCAST_CNT              (0XFFFFFFFFU)  /* Tx Unicast Frame Count. */

/* ----------------------------------------------------------------------------------------------------
          P1_RX_DROP_FULL_CNT                                   Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_RX_DROP_FULL_CNT_P1_RX_DROP_FULL_CNT      (0U)           /* Rx Frames Dropped Due to FIFO Full. */
#define BITL_MAC_P1_RX_DROP_FULL_CNT_P1_RX_DROP_FULL_CNT      (32U)          /* Rx Frames Dropped Due to FIFO Full. */
#define BITM_MAC_P1_RX_DROP_FULL_CNT_P1_RX_DROP_FULL_CNT      (0XFFFFFFFFU)  /* Rx Frames Dropped Due to FIFO Full. */

/* ----------------------------------------------------------------------------------------------------
          P1_RX_DROP_FILT_CNT                                   Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_RX_DROP_FILT_CNT_P1_RX_DROP_FILT_CNT      (0U)           /* Rx Frames Dropped Due to Filtering. */
#define BITL_MAC_P1_RX_DROP_FILT_CNT_P1_RX_DROP_FILT_CNT      (32U)          /* Rx Frames Dropped Due to Filtering. */
#define BITM_MAC_P1_RX_DROP_FILT_CNT_P1_RX_DROP_FILT_CNT      (0XFFFFFFFFU)  /* Rx Frames Dropped Due to Filtering. */

/* ----------------------------------------------------------------------------------------------------
          P1_RX_IFG_ERR_CNT                                     Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_RX_IFG_ERR_CNT_P1_RX_IFG_ERR_CNT          (0U)           /* IFG Error Counter for Port 1 Received Frames. */
#define BITL_MAC_P1_RX_IFG_ERR_CNT_P1_RX_IFG_ERR_CNT          (32U)          /* IFG Error Counter for Port 1 Received Frames. */
#define BITM_MAC_P1_RX_IFG_ERR_CNT_P1_RX_IFG_ERR_CNT          (0XFFFFFFFFU)  /* IFG Error Counter for Port 1 Received Frames. */

/* ----------------------------------------------------------------------------------------------------
          P1_TX_IFG                                             Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_TX_IFG_P1_TX_IFG                          (0U)           /* Inter Frame Gap. */
#define BITL_MAC_P1_TX_IFG_P1_TX_IFG                          (8U)           /* Inter Frame Gap. */
#define BITM_MAC_P1_TX_IFG_P1_TX_IFG                          (0X000000FFU)  /* Inter Frame Gap. */

/* ----------------------------------------------------------------------------------------------------
          P1_LOOP                                               Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_LOOP_P1_LOOPBACK_EN                       (0U)           /* MAC Loopback. */
#define BITL_MAC_P1_LOOP_P1_LOOPBACK_EN                       (1U)           /* MAC Loopback. */
#define BITM_MAC_P1_LOOP_P1_LOOPBACK_EN                       (0X00000001U)  /* MAC Loopback. */

#define ENUM_MAC_P1_LOOP_P1_LOOPBACK_EN_LOOPB_DIS             (0X00000000U)  /* Normal Operation - Loopback Disabled. */
#define ENUM_MAC_P1_LOOP_P1_LOOPBACK_EN_LOOPB_EN              (0X00000001U)  /* Loopback Enabled. */

/* ----------------------------------------------------------------------------------------------------
          P1_RX_CRC_EN                                          Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_RX_CRC_EN_P1_CRC_CHK_EN                   (0U)           /* CRC Check Enable on Receive. */
#define BITL_MAC_P1_RX_CRC_EN_P1_CRC_CHK_EN                   (1U)           /* CRC Check Enable on Receive. */
#define BITM_MAC_P1_RX_CRC_EN_P1_CRC_CHK_EN                   (0X00000001U)  /* CRC Check Enable on Receive. */

/* ----------------------------------------------------------------------------------------------------
          P1_RX_IFG                                             Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_RX_IFG_P1_RX_IFG                          (0U)           /* Inter Frame Gap. */
#define BITL_MAC_P1_RX_IFG_P1_RX_IFG                          (6U)           /* Inter Frame Gap. */
#define BITM_MAC_P1_RX_IFG_P1_RX_IFG                          (0X0000003FU)  /* Inter Frame Gap. */

/* ----------------------------------------------------------------------------------------------------
          P1_RX_MAX_LEN                                         Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_RX_MAX_LEN_P1_MAX_FRM_LEN                 (0U)           /* Max Frame Length on Receive. */
#define BITL_MAC_P1_RX_MAX_LEN_P1_MAX_FRM_LEN                 (16U)          /* Max Frame Length on Receive. */
#define BITM_MAC_P1_RX_MAX_LEN_P1_MAX_FRM_LEN                 (0X0000FFFFU)  /* Max Frame Length on Receive. */

/* ----------------------------------------------------------------------------------------------------
          P1_RX_MIN_LEN                                         Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_RX_MIN_LEN_P1_MIN_FRM_LEN                 (0U)           /* Min Frame Length on Receive. */
#define BITL_MAC_P1_RX_MIN_LEN_P1_MIN_FRM_LEN                 (16U)          /* Min Frame Length on Receive. */
#define BITM_MAC_P1_RX_MIN_LEN_P1_MIN_FRM_LEN                 (0X0000FFFFU)  /* Min Frame Length on Receive. */

/* ----------------------------------------------------------------------------------------------------
          P1_LO_RFC                                             Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_LO_RFC_P1_LO_RFC                          (0U)           /* Receive Frame Count for the Low Priority FIFO. */
#define BITL_MAC_P1_LO_RFC_P1_LO_RFC                          (9U)           /* Receive Frame Count for the Low Priority FIFO. */
#define BITM_MAC_P1_LO_RFC_P1_LO_RFC                          (0X000001FFU)  /* Receive Frame Count for the Low Priority FIFO. */

/* ----------------------------------------------------------------------------------------------------
          P1_HI_RFC                                             Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_HI_RFC_P1_HI_RFC                          (0U)           /* Receive Frame Count for the High Priority FIFO. */
#define BITL_MAC_P1_HI_RFC_P1_HI_RFC                          (9U)           /* Receive Frame Count for the High Priority FIFO. */
#define BITM_MAC_P1_HI_RFC_P1_HI_RFC                          (0X000001FFU)  /* Receive Frame Count for the High Priority FIFO. */

/* ----------------------------------------------------------------------------------------------------
          P1_LO_RXSIZE                                          Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_LO_RXSIZE_P1_LO_RXSIZE                    (0U)           /* Data in the Rx FIFO. Number of Half Words(16 Bit). */
#define BITL_MAC_P1_LO_RXSIZE_P1_LO_RXSIZE                    (14U)          /* Data in the Rx FIFO. Number of Half Words(16 Bit). */
#define BITM_MAC_P1_LO_RXSIZE_P1_LO_RXSIZE                    (0X00003FFFU)  /* Data in the Rx FIFO. Number of Half Words(16 Bit). */

/* ----------------------------------------------------------------------------------------------------
          P1_HI_RXSIZE                                          Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_P1_HI_RXSIZE_P1_HI_RXSIZE                    (0U)           /* Data in the Rx FIFO. Number of Half Words(16 Bit). */
#define BITL_MAC_P1_HI_RXSIZE_P1_HI_RXSIZE                    (14U)          /* Data in the Rx FIFO. Number of Half Words(16 Bit). */
#define BITM_MAC_P1_HI_RXSIZE_P1_HI_RXSIZE                    (0X00003FFFU)  /* Data in the Rx FIFO. Number of Half Words(16 Bit). */


/** @defgroup IDVER Identification Version Register. (IDVER) Register
 *  Identification Version Register. (IDVER) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_IDVER_t
 *  \brief Identification Version Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_IDVER_t {
    union {
        struct {
             uint32_t MINVER           : 4; /**< OA Minor Version. */
             uint32_t MAJVER           : 4; /**< OA Major Version. */
             uint32_t RESERVED8        : 24; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_IDVER_t;

/*@}*/

/** @defgroup PHYID PHY Identification Register. (PHYID) Register
 *  PHY Identification Register. (PHYID) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_PHYID_t
 *  \brief PHY Identification Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_PHYID_t {
    union {
        struct {
             uint32_t REVISION         : 4; /**< Manufacturer’s Revision Number. */
             uint32_t MODEL            : 6; /**< Manufacturer’s Model Number. */
             uint32_t OUI              : 22; /**< Organizationally Unique Identifier (Bits 2:23). */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_PHYID_t;

/*@}*/

/** @defgroup CAPABILITY Supported Capabilities Register. (CAPABILITY) Register
 *  Supported Capabilities Register. (CAPABILITY) Register
 *  @{
 */

/*! ========================================================================
 *  \enum ETH_MAC_CAPABILITY_MINCPS
 *  \brief Minimum Supported Chunk Payload Size.
 *  ======================================================================== */
typedef enum
{
     MAC_CAPABILITY_MINCPS_CPS8       = 3,    /**< Minimum Supported Chunk Payload Size is 8 Bytes. */
     MAC_CAPABILITY_MINCPS_CPS16      = 4,    /**< Minimum Supported Chunk Payload Size is 16 Bytes. */
     MAC_CAPABILITY_MINCPS_CPS32      = 5,    /**< Minimum Supported Chunk Payload Size is 32 Bytes. */
     MAC_CAPABILITY_MINCPS_CPS64      = 6     /**< Minimum Supported Chunk Payload Size is 64 Bytes. */
}  ETH_MAC_CAPABILITY_MINCPS;

/*! ========================================================================
 *  \enum ETH_MAC_CAPABILITY_SEQC
 *  \brief TX Data Chunk Sequence and Retry Capability.
 *  ======================================================================== */
typedef enum
{
     MAC_CAPABILITY_SEQC_SEQC_0       = 0,    /**< TX Data Chunk Sequence and Retry is Not Supported. */
     MAC_CAPABILITY_SEQC_SEQC_1       = 1     /**< TX Data Chunk Sequence and Retry is Supported. */
}  ETH_MAC_CAPABILITY_SEQC;

/*! ========================================================================
 *  \enum ETH_MAC_CAPABILITY_FTSC
 *  \brief Frame Timestamp Capability.
 *  ======================================================================== */
typedef enum
{
     MAC_CAPABILITY_FTSC_FTSC_0       = 0,    /**< IEEE 1588 Timestamp Capture on Frame Tx/Rx is Not Supported. */
     MAC_CAPABILITY_FTSC_FTSC_1       = 1     /**< IEEE 1588 Timestamp Capture on Frame Tx/Rx is Supported. */
}  ETH_MAC_CAPABILITY_FTSC;

/*! ========================================================================
 *  \enum ETH_MAC_CAPABILITY_CTC
 *  \brief Cut-Through Capability.
 *  ======================================================================== */
typedef enum
{
     MAC_CAPABILITY_CTC_CTC_0         = 0,    /**< Cut-Through Not Supported. */
     MAC_CAPABILITY_CTC_CTC_1         = 1     /**< Cut-Through Supported. */
}  ETH_MAC_CAPABILITY_CTC;

/*! ========================================================================
 *  \enum ETH_MAC_CAPABILITY_DPRAC
 *  \brief Direct PHY Register Access Capability.
 *  ======================================================================== */
typedef enum
{
     MAC_CAPABILITY_DPRAC_DPRAC_0     = 0,    /**< PHY Registers are Not Directly Accessible. */
     MAC_CAPABILITY_DPRAC_DPRAC_1     = 1     /**< PHY Registers are Directly Accessible. */
}  ETH_MAC_CAPABILITY_DPRAC;

/*! ========================================================================
 *  \enum ETH_MAC_CAPABILITY_IPRAC
 *  \brief Indirect PHY Register Access Capability.
 *  ======================================================================== */
typedef enum
{
     MAC_CAPABILITY_IPRAC_IPRAC_0     = 0,    /**< PHY Registers are Not Indirectly Accessible. */
     MAC_CAPABILITY_IPRAC_IPRAC_1     = 1     /**< PHY Registers are Indirectly Accessible. */
}  ETH_MAC_CAPABILITY_IPRAC;

/*! ========================================================================
 *  \enum ETH_MAC_CAPABILITY_TXFCSVC
 *  \brief Transmit Frame Check Sequence Validation Capability.
 *  ======================================================================== */
typedef enum
{
     MAC_CAPABILITY_TXFCSVC_TXFCSVC_0 = 0,    /**< Transmit FCS Validation is Not Supported. */
     MAC_CAPABILITY_TXFCSVC_TXFCSVC_1 = 1     /**< Transmit FCS Validation is Supported. */
}  ETH_MAC_CAPABILITY_TXFCSVC;

/*! ========================================================================
 *  \struct ETH_MAC_CAPABILITY_t
 *  \brief Supported Capabilities Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_CAPABILITY_t {
    union {
        struct {
             uint32_t MINCPS           : 3; /**< Minimum Supported Chunk Payload Size. */
             uint32_t RESERVED3        : 1; /**< Reserved */
             uint32_t SEQC             : 1; /**< TX Data Chunk Sequence and Retry Capability. */
             uint32_t AIDC             : 1; /**< Address Increment Disable Capability. */
             uint32_t FTSC             : 1; /**< Frame Timestamp Capability. */
             uint32_t CTC              : 1; /**< Cut-Through Capability. */
             uint32_t DPRAC            : 1; /**< Direct PHY Register Access Capability. */
             uint32_t IPRAC            : 1; /**< Indirect PHY Register Access Capability. */
             uint32_t TXFCSVC          : 1; /**< Transmit Frame Check Sequence Validation Capability. */
             uint32_t RESERVED11       : 21; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_CAPABILITY_t;

/*@}*/

/** @defgroup RESET Reset Control and Status Register. (RESET) Register
 *  Reset Control and Status Register. (RESET) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_RESET_t
 *  \brief Reset Control and Status Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_RESET_t {
    union {
        struct {
             uint32_t SWRESET          : 1; /**< Software Reset. */
             uint32_t RESERVED1        : 31; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_RESET_t;

/*@}*/

/** @defgroup CONFIG0 Configuration Register 0. (CONFIG0) Register
 *  Configuration Register 0. (CONFIG0) Register
 *  @{
 */

/* ETH_MAC_CONFIG0_CPS moved to top of header */

/*! ========================================================================
 *  \enum ETH_MAC_CONFIG0_FTSS
 *  \brief Receive Frame Timestamp Select.
 *  ======================================================================== */
typedef enum
{
     MAC_CONFIG0_FTSS_FTSS_0          = 0,    /**< 32-bit Timestamps. */
     MAC_CONFIG0_FTSS_FTSS_1          = 1     /**< 64-bit Timestamps. */
}  ETH_MAC_CONFIG0_FTSS;

/*! ========================================================================
 *  \enum ETH_MAC_CONFIG0_FTSE
 *  \brief Frame Timestamp Enable.
 *  ======================================================================== */
typedef enum
{
     MAC_CONFIG0_FTSE_FTSE_0          = 0,    /**< Frame Receive/Transmit Timestamps are Disabled. */
     MAC_CONFIG0_FTSE_FTSE_1          = 1     /**< Frame Receive/Transmit Timestamps are Enabled. */
}  ETH_MAC_CONFIG0_FTSE;

/*! ========================================================================
 *  \enum ETH_MAC_CONFIG0_TXCTHRESH
 *  \brief Transmit Credit Threshold.
 *  ======================================================================== */
typedef enum
{
     MAC_CONFIG0_TXCTHRESH_CREDIT_1   = 0,    /**< ≥ 1 Credit. */
     MAC_CONFIG0_TXCTHRESH_CREDIT_4   = 1,    /**< ≥ 4 Credits. */
     MAC_CONFIG0_TXCTHRESH_CREDIT_8   = 2,    /**< ≥ 8 Credits. */
     MAC_CONFIG0_TXCTHRESH_CREDIT_16  = 3     /**< ≥ 16 Credits. */
}  ETH_MAC_CONFIG0_TXCTHRESH;

/*! ========================================================================
 *  \enum ETH_MAC_CONFIG0_SYNC
 *  \brief Configuration Synchronization.
 *  ======================================================================== */
typedef enum
{
     MAC_CONFIG0_SYNC_SYNC_0          = 0,    /**< The MACPHY Has Been Reset and is Not Configured. */
     MAC_CONFIG0_SYNC_SYNC_1          = 1     /**< The MACPHY is Configured. */
}  ETH_MAC_CONFIG0_SYNC;

/*! ========================================================================
 *  \struct ETH_MAC_CONFIG0_t
 *  \brief Configuration Register 0.
 *  ======================================================================== */
typedef struct _ETH_MAC_CONFIG0_t {
    union {
        struct {
             uint32_t CPS              : 3; /**< Chunk Payload Selector (N). */
             uint32_t RESERVED3        : 1; /**< Reserved */
             uint32_t SEQE             : 1; /**< Enable TX Data Chunk Sequence and Retry. */
             uint32_t PROTE            : 1; /**< Enable Control Data Read Write Protection. */
             uint32_t FTSS             : 1; /**< Receive Frame Timestamp Select. */
             uint32_t FTSE             : 1; /**< Frame Timestamp Enable. */
             uint32_t RXCTE            : 1; /**< Receive Cut-Through Enable. */
             uint32_t TXCTE            : 1; /**< Transmit Cut-Through Enable. */
             uint32_t TXCTHRESH        : 2; /**< Transmit Credit Threshold. */
             uint32_t ZARFE            : 1; /**< Zero-Align Receive Frame Enable. */
             uint32_t CSARFE           : 1; /**< CSn Align Receive Frame Enable. */
             uint32_t TXFCSVE          : 1; /**< Transmit Frame Check Sequence Validation Enable. */
             uint32_t SYNC             : 1; /**< Configuration Synchronization. */
             uint32_t RESERVED16       : 16; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_CONFIG0_t;

/*@}*/

/** @defgroup CONFIG2 Configuration Register 2. (CONFIG2) Register
 *  Configuration Register 2. (CONFIG2) Register
 *  @{
 */

/*! ========================================================================
 *  \enum ETH_MAC_CONFIG2_MSPEED
 *  \brief SPI to MDIO Bridge MDC Clock Speed.
 *  ======================================================================== */
typedef enum
{
     MAC_CONFIG2_MSPEED_MHZ_2P5       = 0,    /**< 2.5 MHz. */
     MAC_CONFIG2_MSPEED_MHZ_4P166     = 1     /**< 4.166 MHz. */
}  ETH_MAC_CONFIG2_MSPEED;

/*! ========================================================================
 *  \enum ETH_MAC_CONFIG2_STATS_CLR_ON_RD
 *  \brief Statistics Clear on Reading.
 *  ======================================================================== */
typedef enum
{
     MAC_CONFIG2_STATS_CLR_ON_RD_HOLD_STAT = 0,    /**< Statistic Counter is Not Cleared on Reading. */
     MAC_CONFIG2_STATS_CLR_ON_RD_CLR_STAT = 1     /**< Clear Statistics Counters on Reading. */
}  ETH_MAC_CONFIG2_STATS_CLR_ON_RD;

/*! ========================================================================
 *  \enum ETH_MAC_CONFIG2_SFD_DETECT_SRC
 *  \brief Determines If the SFD is Detected in the PHY or MAC.
 *  ======================================================================== */
typedef enum
{
     MAC_CONFIG2_SFD_DETECT_SRC_PHY   = 0,    /**< Select the SFD from the PHY. */
     MAC_CONFIG2_SFD_DETECT_SRC_MAC   = 1     /**< Select the SFD from the MAC. */
}  ETH_MAC_CONFIG2_SFD_DETECT_SRC;

/*! ========================================================================
 *  \struct ETH_MAC_CONFIG2_t
 *  \brief Configuration Register 2.
 *  ======================================================================== */
typedef struct _ETH_MAC_CONFIG2_t {
    union {
        struct {
             uint32_t MSPEED           : 2; /**< SPI to MDIO Bridge MDC Clock Speed. */
             uint32_t P1_FWD_UNK2HOST  : 1; /**< Forward Frames Not Matching Any MAC Address to the Host. */
             uint32_t RESERVED3        : 1; /**< Reserved */
             uint32_t P1_RCV_IFG_ERR_FRM : 1; /**< Admit Frames with IFG Errors on P1. */
             uint32_t CRC_APPEND       : 1; /**< Enable CRC Append. */
             uint32_t STATS_CLR_ON_RD  : 1; /**< Statistics Clear on Reading. */
             uint32_t SFD_DETECT_SRC   : 1; /**< Determines If the SFD is Detected in the PHY or MAC. */
             uint32_t TX_RDY_ON_EMPTY  : 1; /**< Assert TX_RDY When the Tx FIFO is Empty. */
             uint32_t RESERVED9        : 23; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_CONFIG2_t;

/*@}*/

/** @defgroup STATUS0 Status Register 0. (STATUS0) Register
 *  Status Register 0. (STATUS0) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_STATUS0_t
 *  \brief Status Register 0.
 *  ======================================================================== */
typedef struct _ETH_MAC_STATUS0_t {
    union {
        struct {
             uint32_t TXPE             : 1; /**< Transmit Protocol Error. */
             uint32_t TXBOE            : 1; /**< Host Tx FIFO Overflow. */
             uint32_t TXBUE            : 1; /**< Host Tx FIFO Underrun Error. */
             uint32_t RXBOE            : 1; /**< Receive Buffer Overflow Error. */
             uint32_t LOFE             : 1; /**< Loss of Frame Error. */
             uint32_t HDRE             : 1; /**< Header Error. */
             uint32_t RESETC           : 1; /**< Reset Complete. */
             uint32_t PHYINT           : 1; /**< PHY Interrupt for Port1. */
             uint32_t TTSCAA           : 1; /**< Transmit Timestamp Capture Available A. */
             uint32_t TTSCAB           : 1; /**< Transmit Timestamp Capture Available B. */
             uint32_t TTSCAC           : 1; /**< Transmit Timestamp Capture Available C. */
             uint32_t TXFCSE           : 1; /**< Transmit Frame Check Sequence Error. */
             uint32_t CDPE             : 1; /**< Control Data Protection Error. */
             uint32_t RESERVED13       : 19; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_STATUS0_t;

/*@}*/

/** @defgroup STATUS1 Status Register 1. (STATUS1) Register
 *  Status Register 1. (STATUS1) Register
 *  @{
 */

/*! ========================================================================
 *  \enum ETH_MAC_STATUS1_P1_LINK_STATUS
 *  \brief Port 1 Link Status.
 *  ======================================================================== */
typedef enum
{
     MAC_STATUS1_P1_LINK_STATUS_DOWN  = 0,    /**< Link Down. */
     MAC_STATUS1_P1_LINK_STATUS_UP    = 1     /**< Link up. */
}  ETH_MAC_STATUS1_P1_LINK_STATUS;

/*! ========================================================================
 *  \struct ETH_MAC_STATUS1_t
 *  \brief Status Register 1.
 *  ======================================================================== */
typedef struct _ETH_MAC_STATUS1_t {
    union {
        struct {
             uint32_t P1_LINK_STATUS   : 1; /**< Port 1 Link Status. */
             uint32_t LINK_CHANGE      : 1; /**< Link Status Changed. */
             uint32_t RESERVED2        : 1; /**< Reserved */
             uint32_t TX_RDY           : 1; /**< Tx Ready. */
             uint32_t P1_RX_RDY        : 1; /**< Port 1 Rx FIFO Contains Data. */
             uint32_t P1_RX_RDY_HI     : 1; /**< Port1 Rx Ready High Priority. */
             uint32_t RESERVED6        : 2; /**< Reserved */
             uint32_t P1_RX_IFG_ERR    : 1; /**< Rx MAC Inter Frame Gap Error. */
             uint32_t RESERVED9        : 1; /**< Reserved */
             uint32_t SPI_ERR          : 1; /**< Detected an Error on an SPI Transaction. */
             uint32_t RX_ECC_ERR       : 1; /**< ECC Error on Reading the Frame Size from an Rx FIFO. */
             uint32_t TX_ECC_ERR       : 1; /**< ECC Error on Reading the Frame Size from a Tx FIFO. */
             uint32_t RESERVED13       : 19; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_STATUS1_t;

/*@}*/

/** @defgroup BUFSTS Buffer Status Register. (BUFSTS) Register
 *  Buffer Status Register. (BUFSTS) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_BUFSTS_t
 *  \brief Buffer Status Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_BUFSTS_t {
    union {
        struct {
             uint32_t RCA              : 8; /**< Receive Chunks Available. */
             uint32_t TXC              : 8; /**< Transmit Credits Available. */
             uint32_t RESERVED16       : 16; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_BUFSTS_t;

/*@}*/

/** @defgroup IMASK0 Interrupt Mask Register 0. (IMASK0) Register
 *  Interrupt Mask Register 0. (IMASK0) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_IMASK0_t
 *  \brief Interrupt Mask Register 0.
 *  ======================================================================== */
typedef struct _ETH_MAC_IMASK0_t {
    union {
        struct {
             uint32_t TXPEM            : 1; /**< Transmit Protocol Error Mask. */
             uint32_t TXBOEM           : 1; /**< Transmit Buffer Overflow Error Mask. */
             uint32_t TXBUEM           : 1; /**< Transmit Buffer Underflow Error Mask. */
             uint32_t RXBOEM           : 1; /**< Receive Buffer Overflow Error Mask. */
             uint32_t LOFEM            : 1; /**< Loss of Frame Error Mask. */
             uint32_t HDREM            : 1; /**< Header Error Mask. */
             uint32_t RESETCM          : 1; /**< RESET Complete Mask. */
             uint32_t PHYINTM          : 1; /**< Physical Layer Interrupt Mask. */
             uint32_t TTSCAAM          : 1; /**< Transmit Timestamp Capture Available A Mask. */
             uint32_t TTSCABM          : 1; /**< Transmit Timestamp Capture Available B Mask. */
             uint32_t TTSCACM          : 1; /**< Transmit Timestamp Capture Available C Mask. */
             uint32_t TXFCSEM          : 1; /**< Transmit Frame Check Sequence Error Mask. */
             uint32_t CDPEM            : 1; /**< Control Data Protection Error Mask. */
             uint32_t RESERVED13       : 19; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ADIN_MAC_IMASK0_t;

/*@}*/

/** @defgroup IMASK1 Mask Bits for Driving the Interrupt Pin Register. (IMASK1) Register
 *  Mask Bits for Driving the Interrupt Pin Register. (IMASK1) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_IMASK1_t
 *  \brief Mask Bits for Driving the Interrupt Pin Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_IMASK1_t {
    union {
        struct {
             uint32_t RESERVED0        : 1; /**< Reserved */
             uint32_t LINK_CHANGE_MASK : 1; /**< Mask Bit for LINK_CHANGE. */
             uint32_t RESERVED2        : 1; /**< Reserved */
             uint32_t TX_RDY_MASK      : 1; /**< Mask Bit for TX_FRM_DONE. */
             uint32_t P1_RX_RDY_MASK   : 1; /**< Mask Bit for P1_RX_RDY. */
             uint32_t RESERVED5        : 3; /**< Reserved */
             uint32_t P1_RX_IFG_ERR_MASK : 1; /**< Mask Bit for RX_IFG_ERR. */
             uint32_t RESERVED9        : 1; /**< Reserved */
             uint32_t SPI_ERR_MASK     : 1; /**< Mask Bit for SPI_ERR. */
             uint32_t RX_ECC_ERR_MASK  : 1; /**< Mask Bit for RXF_ECC_ERR. */
             uint32_t TX_ECC_ERR_MASK  : 1; /**< Mask Bit for TXF_ECC_ERR. */
             uint32_t RESERVED13       : 19; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_IMASK1_t;

/*@}*/

/** @defgroup TTSCAH Transmit Timestamp Capture Register A (High). (TTSCAH) Register
 *  Transmit Timestamp Capture Register A (High). (TTSCAH) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_TTSCAH_t
 *  \brief Transmit Timestamp Capture Register A (High).
 *  ======================================================================== */
typedef struct _ETH_MAC_TTSCAH_t {
    union {
        struct {
             uint32_t TTSCH_A          : 32; /**< Transmit Timestamp A Bits 63-32. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_TTSCAH_t;

/*@}*/

/** @defgroup TTSCAL Transmit Timestamp Capture Register A (Low). (TTSCAL) Register
 *  Transmit Timestamp Capture Register A (Low). (TTSCAL) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_TTSCAL_t
 *  \brief Transmit Timestamp Capture Register A (Low).
 *  ======================================================================== */
typedef struct _ETH_MAC_TTSCAL_t {
    union {
        struct {
             uint32_t TTSCL_A          : 32; /**< Transmit Timestamp A Bits 31-0. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_TTSCAL_t;

/*@}*/

/** @defgroup TTSCBH Transmit Timestamp Capture Register B (High). (TTSCBH) Register
 *  Transmit Timestamp Capture Register B (High). (TTSCBH) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_TTSCBH_t
 *  \brief Transmit Timestamp Capture Register B (High).
 *  ======================================================================== */
typedef struct _ETH_MAC_TTSCBH_t {
    union {
        struct {
             uint32_t TTSCH_B          : 32; /**< Transmit Timestamp B Bits 63-32. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_TTSCBH_t;

/*@}*/

/** @defgroup TTSCBL Transmit Timestamp Capture Register B (Low). (TTSCBL) Register
 *  Transmit Timestamp Capture Register B (Low). (TTSCBL) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_TTSCBL_t
 *  \brief Transmit Timestamp Capture Register B (Low).
 *  ======================================================================== */
typedef struct _ETH_MAC_TTSCBL_t {
    union {
        struct {
             uint32_t TTSCL_B          : 32; /**< Transmit Timestamp B Bits 31-0. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_TTSCBL_t;

/*@}*/

/** @defgroup TTSCCH Transmit Timestamp Capture Register C (High). (TTSCCH) Register
 *  Transmit Timestamp Capture Register C (High). (TTSCCH) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_TTSCCH_t
 *  \brief Transmit Timestamp Capture Register C (High).
 *  ======================================================================== */
typedef struct _ETH_MAC_TTSCCH_t {
    union {
        struct {
             uint32_t TTSCH_C          : 32; /**< Transmit Timestamp C Bits 63-32. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_TTSCCH_t;

/*@}*/

/** @defgroup TTSCCL Transmit Timestamp Capture Register C (Low). (TTSCCL) Register
 *  Transmit Timestamp Capture Register C (Low). (TTSCCL) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_TTSCCL_t
 *  \brief Transmit Timestamp Capture Register C (Low).
 *  ======================================================================== */
typedef struct _ETH_MAC_TTSCCL_t {
    union {
        struct {
             uint32_t TTSCL_C          : 32; /**< Transmit Timestamp C Bits 31-0. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_TTSCCL_t;

/*@}*/

/** @defgroup MDIOACC_0_ MDIO Access Registers. (MDIOACC_0_) Register
 *  MDIO Access Registers. (MDIOACC_0_) Register
 *  @{
 */

/*! ========================================================================
 *  \enum ETH_MAC_MDIOACC_0__MDIO_OP
 *  \brief Operation Code.
 *  ======================================================================== */
typedef enum
{
     MAC_MDIOACC_0__MDIO_OP_MD_ADDR   = 0,    /**< MD Address Command. */
     MAC_MDIOACC_0__MDIO_OP_MD_WR     = 1,    /**< Write Command. */
     MAC_MDIOACC_0__MDIO_OP_MD_INC_RD = 2,    /**< Incremental Read Command. */
     MAC_MDIOACC_0__MDIO_OP_MD_RD     = 3     /**< Read Command. */
}  ETH_MAC_MDIOACC_0__MDIO_OP;

/*! ========================================================================
 *  \enum ETH_MAC_MDIOACC_0__MDIO_ST
 *  \brief Start of Frame.
 *  ======================================================================== */
typedef enum
{
     MAC_MDIOACC_0__MDIO_ST_CLAUSE45  = 0,    /**< No description provided */
     MAC_MDIOACC_0__MDIO_ST_CLAUSE22  = 1     /**< No description provided */
}  ETH_MAC_MDIOACC_0__MDIO_ST;

/*! ========================================================================
 *  \struct ETH_MAC_MDIOACC_0__t
 *  \brief MDIO Access Registers.
 *  ======================================================================== */
typedef struct _ETH_MAC_MDIOACC_0__t {
    union {
        struct {
             uint32_t MDIO_DATA        : 16; /**< Data/Address Value. */
             uint32_t MDIO_DEVAD       : 5; /**< MDIO Device Address. */
             uint32_t MDIO_PRTAD       : 5; /**< MDIO Port Address. */
             uint32_t MDIO_OP          : 2; /**< Operation Code. */
             uint32_t MDIO_ST          : 2; /**< Start of Frame. */
             uint32_t MDIO_TAERR       : 1; /**< Turnaround Error. */
             uint32_t MDIO_TRDONE      : 1; /**< Transaction Done. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_MDIOACC_0__t;

/*@}*/

/** @defgroup TX_FSIZE MAC Tx Frame Size Register. (TX_FSIZE) Register
 *  MAC Tx Frame Size Register. (TX_FSIZE) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_TX_FSIZE_t
 *  \brief MAC Tx Frame Size Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_TX_FSIZE_t {
    union {
        struct {
             uint32_t TX_FRM_SIZE      : 11; /**< Transmit Frame Size. */
             uint32_t RESERVED11       : 21; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_TX_FSIZE_t;

/*@}*/

/** @defgroup TX MAC Transmit Register. (TX) Register
 *  MAC Transmit Register. (TX) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_TX_t
 *  \brief MAC Transmit Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_TX_t {
    union {
        struct {
             uint32_t TDR              : 32; /**< Transmit Data Register. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_TX_t;

/*@}*/

/** @defgroup TX_SPACE Tx FIFO Space Register. (TX_SPACE) Register
 *  Tx FIFO Space Register. (TX_SPACE) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_TX_SPACE_t
 *  \brief Tx FIFO Space Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_TX_SPACE_t {
    union {
        struct {
             uint32_t TX_SPACE         : 14; /**< Transmit FIFO Space Available in Half Words (16 Bits). */
             uint32_t RESERVED14       : 18; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_TX_SPACE_t;

/*@}*/

/** @defgroup TX_THRESH Transmit Threshold Register. (TX_THRESH) Register
 *  Transmit Threshold Register. (TX_THRESH) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_TX_THRESH_t
 *  \brief Transmit Threshold Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_TX_THRESH_t {
    union {
        struct {
             uint32_t HOST_TX_THRESH   : 6; /**< Host Transmit Start Threshold in Cut Through. */
             uint32_t RESERVED6        : 26; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_TX_THRESH_t;

/*@}*/

/** @defgroup FIFO_CLR MAC FIFO Clear Register. (FIFO_CLR) Register
 *  MAC FIFO Clear Register. (FIFO_CLR) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_FIFO_CLR_t
 *  \brief MAC FIFO Clear Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_FIFO_CLR_t {
    union {
        struct {
             uint32_t MAC_RXF_CLR      : 1; /**< Clear the Receive FIFO(s). */
             uint32_t MAC_TXF_CLR      : 1; /**< Clear the Host Transmit FIFO. */
             uint32_t RESERVED2        : 30; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_FIFO_CLR_t;

/*@}*/

/** @defgroup SCRATCH_0_ Scratch Registers. (SCRATCH_0_) Register
 *  Scratch Registers. (SCRATCH_0_) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_SCRATCH_0__t
 *  \brief Scratch Registers.
 *  ======================================================================== */
typedef struct _ETH_MAC_SCRATCH_0__t {
    union {
        struct {
             uint32_t SCRATCH_DATA     : 32; /**< Scratch Data. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_SCRATCH_0__t;

/*@}*/

/** @defgroup MAC_RST_STATUS MAC Reset Status. (MAC_RST_STATUS) Register
 *  MAC Reset Status. (MAC_RST_STATUS) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_MAC_RST_STATUS_t
 *  \brief MAC Reset Status.
 *  ======================================================================== */
typedef struct _ETH_MAC_MAC_RST_STATUS_t {
    union {
        struct {
             uint32_t MAC_OSC_CLK_RDY  : 1; /**< MAC Oscillator Clock Ready. */
             uint32_t MAC_CRYSL_CLK_RDY : 1; /**< MAC Crystal Clock Ready. */
             uint32_t RESERVED2        : 30; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_MAC_RST_STATUS_t;

/*@}*/

/** @defgroup SOFT_RST Software Reset Register. (SOFT_RST) Register
 *  Software Reset Register. (SOFT_RST) Register
 *  @{
 */

/*! ========================================================================
 *  \enum ETH_MAC_SOFT_RST_RST_KEY
 *  \brief Software Reset.
 *  ======================================================================== */
typedef enum
{
     MAC_SOFT_RST_RST_KEY_RST_MAC_ONLY_KEY1 = 20252,    /**< Key1 to Reset the MAC Logic Only. */
     MAC_SOFT_RST_RST_KEY_MAC_RST_EXIT_REQ_KEY1 = 28442,    /**< Key 1 to Request Release of Reset to the MAC Core Logic. */
     MAC_SOFT_RST_RST_KEY_MAC_RST_EXIT_REQ_KEY2 = 41462,    /**< Key 2 to Request Release of Reset to the MAC Core Logic. */
     MAC_SOFT_RST_RST_KEY_RST_MAC_ONLY_KEY2 = 49652     /**< Key 2 to Reset the MAC Logic Only. */
}  ETH_MAC_SOFT_RST_RST_KEY;

/*! ========================================================================
 *  \struct ETH_MAC_SOFT_RST_t
 *  \brief Software Reset Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_SOFT_RST_t {
    union {
        struct {
             uint32_t RST_KEY          : 16; /**< Software Reset. */
             uint32_t RESERVED16       : 16; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_SOFT_RST_t;

/*@}*/

/** @defgroup SPI_INJ_ERR Inject an Error on MISO from the DUT. (SPI_INJ_ERR) Register
 *  Inject an Error on MISO from the DUT. (SPI_INJ_ERR) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_SPI_INJ_ERR_t
 *  \brief Inject an Error on MISO from the DUT.
 *  ======================================================================== */
typedef struct _ETH_MAC_SPI_INJ_ERR_t {
    union {
        struct {
             uint32_t TEST_SPI_INJ_ERR : 1; /**< Inject an Error on the SPI MISO Path. */
             uint32_t RESERVED1        : 31; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_SPI_INJ_ERR_t;

/*@}*/

/** @defgroup FIFO_SIZE FIFO Sizes Register. (FIFO_SIZE) Register
 *  FIFO Sizes Register. (FIFO_SIZE) Register
 *  @{
 */

/* ETH_MAC_FIFO_SIZE_HTX_SIZE moved to top of header */

/* ETH_MAC_FIFO_SIZE_P1_RX_LO_SIZE moved to top of header */

/* ETH_MAC_FIFO_SIZE_P1_RX_HI_SIZE moved to top of header */

/*! ========================================================================
 *  \struct ETH_MAC_FIFO_SIZE_t
 *  \brief FIFO Sizes Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_FIFO_SIZE_t {
    union {
        struct {
             uint32_t HTX_SIZE         : 4; /**< Host Transmit FIFO Size. */
             uint32_t P1_RX_LO_SIZE    : 4; /**< Port 1 Rx Low Priority FIFO Size. */
             uint32_t P1_RX_HI_SIZE    : 4; /**< Port 1 Rx High Priority FIFO Size. */
             uint32_t RESERVED12       : 20; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_FIFO_SIZE_t;

/*@}*/

/** @defgroup TFC Tx FIFO Frame Count Register. (TFC) Register
 *  Tx FIFO Frame Count Register. (TFC) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_TFC_t
 *  \brief Tx FIFO Frame Count Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_TFC_t {
    union {
        struct {
             uint32_t TFC              : 9; /**< Number of Frames in the Tx FIFO. */
             uint32_t RESERVED9        : 23; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_TFC_t;

/*@}*/

/** @defgroup TXSIZE Tx FIFO Valid Half Words Register. (TXSIZE) Register
 *  Tx FIFO Valid Half Words Register. (TXSIZE) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_TXSIZE_t
 *  \brief Tx FIFO Valid Half Words Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_TXSIZE_t {
    union {
        struct {
             uint32_t TX_SIZE          : 14; /**< Data in the Tx FIFO. Number of Half Words(16 Bit). */
             uint32_t RESERVED14       : 18; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_TXSIZE_t;

/*@}*/

/** @defgroup HTX_OVF_FRM_CNT Host Tx Frames Dropped Due to FIFO Overflow. (HTX_OVF_FRM_CNT) Register
 *  Host Tx Frames Dropped Due to FIFO Overflow. (HTX_OVF_FRM_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_HTX_OVF_FRM_CNT_t
 *  \brief Host Tx Frames Dropped Due to FIFO Overflow.
 *  ======================================================================== */
typedef struct _ETH_MAC_HTX_OVF_FRM_CNT_t {
    union {
        struct {
             uint32_t HTX_OVF_FRM_CNT  : 32; /**< Counts Host Tx Frames Dropped Due to FIFO Overflow. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_HTX_OVF_FRM_CNT_t;

/*@}*/

/** @defgroup MECC_ERR_ADDR Address of a Detected ECC Error in Memory. (MECC_ERR_ADDR) Register
 *  Address of a Detected ECC Error in Memory. (MECC_ERR_ADDR) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_MECC_ERR_ADDR_t
 *  \brief Address of a Detected ECC Error in Memory.
 *  ======================================================================== */
typedef struct _ETH_MAC_MECC_ERR_ADDR_t {
    union {
        struct {
             uint32_t MECC_ERR_ADDR    : 14; /**< Address of an Uncorrectable ECC Error in Memory. */
             uint32_t RESERVED14       : 18; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_MECC_ERR_ADDR_t;

/*@}*/

/** @defgroup CECC_ERR_0_ Corrected ECC Error Counters. (CECC_ERR_0_) Register
 *  Corrected ECC Error Counters. (CECC_ERR_0_) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_CECC_ERR_0__t
 *  \brief Corrected ECC Error Counters.
 *  ======================================================================== */
typedef struct _ETH_MAC_CECC_ERR_0__t {
    union {
        struct {
             uint32_t CECC_ERR_CNT     : 10; /**< Corrected ECC Error Count. */
             uint32_t RESERVED10       : 22; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_CECC_ERR_0__t;

/*@}*/

/** @defgroup ADDR_FILT_UPR_0_ MAC Address Rule and DA Filter Upper 16 Bits Registers. (ADDR_FILT_UPR_0_) Register
 *  MAC Address Rule and DA Filter Upper 16 Bits Registers. (ADDR_FILT_UPR_0_) Register
 *  @{
 */

/*! ========================================================================
 *  \enum ETH_MAC_ADDR_FILT_UPR_0__APPLY2PORT1
 *  \brief Apply to Port 1.
 *  ======================================================================== */
typedef enum
{
     MAC_ADDR_FILT_UPR_0__APPLY2PORT1_NOTAPPLY = 0,    /**< Do Not Apply to Port 1. */
     MAC_ADDR_FILT_UPR_0__APPLY2PORT1_APPLY = 1     /**< Apply to Port 1. */
}  ETH_MAC_ADDR_FILT_UPR_0__APPLY2PORT1;

/*! ========================================================================
 *  \struct ETH_MAC_ADDR_FILT_UPR_0__t
 *  \brief MAC Address Rule and DA Filter Upper 16 Bits Registers.
 *  ======================================================================== */
typedef struct _ETH_MAC_ADDR_FILT_UPR_0__t {
    union {
        struct {
             uint32_t MAC_ADDR         : 16; /**< MAC Address. */
             uint32_t TO_HOST          : 1; /**< Forward Frames Matching This MAC Address to the Host. */
             uint32_t RESERVED17       : 2; /**< Reserved */
             uint32_t HOST_PRI         : 1; /**< Host Rx Port Priority. */
             uint32_t RESERVED20       : 10; /**< Reserved */
             uint32_t APPLY2PORT1      : 1; /**< Apply to Port 1. */
             uint32_t RESERVED31       : 1; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_ADDR_FILT_UPR_0__t;

/*@}*/

/** @defgroup ADDR_FILT_LWR_0_ MAC Address DA Filter Lower 32 Bits Registers. (ADDR_FILT_LWR_0_) Register
 *  MAC Address DA Filter Lower 32 Bits Registers. (ADDR_FILT_LWR_0_) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_ADDR_FILT_LWR_0__t
 *  \brief MAC Address DA Filter Lower 32 Bits Registers.
 *  ======================================================================== */
typedef struct _ETH_MAC_ADDR_FILT_LWR_0__t {
    union {
        struct {
             uint32_t MAC_ADDR         : 32; /**< MAC Address. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_ADDR_FILT_LWR_0__t;

/*@}*/

/** @defgroup ADDR_MSK_UPR_0_ Upper 16 Bits of the MAC Address Mask. (ADDR_MSK_UPR_0_) Register
 *  Upper 16 Bits of the MAC Address Mask. (ADDR_MSK_UPR_0_) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_ADDR_MSK_UPR_0__t
 *  \brief Upper 16 Bits of the MAC Address Mask.
 *  ======================================================================== */
typedef struct _ETH_MAC_ADDR_MSK_UPR_0__t {
    union {
        struct {
             uint32_t MAC_ADDR_MASK    : 16; /**< MAC Address Bit Mask for the Address Table. */
             uint32_t RESERVED16       : 16; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_ADDR_MSK_UPR_0__t;

/*@}*/

/** @defgroup ADDR_MSK_LWR_0_ Lower 32 Bits of the MAC Address Mask. (ADDR_MSK_LWR_0_) Register
 *  Lower 32 Bits of the MAC Address Mask. (ADDR_MSK_LWR_0_) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_ADDR_MSK_LWR_0__t
 *  \brief Lower 32 Bits of the MAC Address Mask.
 *  ======================================================================== */
typedef struct _ETH_MAC_ADDR_MSK_LWR_0__t {
    union {
        struct {
             uint32_t MAC_ADDR_MASK    : 32; /**< MAC Address Bit Mask for the Address Table. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_ADDR_MSK_LWR_0__t;

/*@}*/

/** @defgroup TS_ADDEND Timestamp Accumulator Addend Register. (TS_ADDEND) Register
 *  Timestamp Accumulator Addend Register. (TS_ADDEND) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_TS_ADDEND_t
 *  \brief Timestamp Accumulator Addend Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_TS_ADDEND_t {
    union {
        struct {
             uint32_t TS_ADDEND        : 32; /**< Timestamp Accumulator Addend. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_TS_ADDEND_t;

/*@}*/

/** @defgroup TS_1SEC_CMP Timer Update Compare Register. (TS_1SEC_CMP) Register
 *  Timer Update Compare Register. (TS_1SEC_CMP) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_TS_1SEC_CMP_t
 *  \brief Timer Update Compare Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_TS_1SEC_CMP_t {
    union {
        struct {
             uint32_t TS_1SEC_CMP      : 32; /**< Timestamp 1 Second Compare Value. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_TS_1SEC_CMP_t;

/*@}*/

/** @defgroup TS_SEC_CNT Seconds Counter Register. (TS_SEC_CNT) Register
 *  Seconds Counter Register. (TS_SEC_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_TS_SEC_CNT_t
 *  \brief Seconds Counter Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_TS_SEC_CNT_t {
    union {
        struct {
             uint32_t TS_SEC_CNT       : 32; /**< Write to the Seconds Counter. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_TS_SEC_CNT_t;

/*@}*/

/** @defgroup TS_NS_CNT Nanoseconds Counter Register. (TS_NS_CNT) Register
 *  Nanoseconds Counter Register. (TS_NS_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_TS_NS_CNT_t
 *  \brief Nanoseconds Counter Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_TS_NS_CNT_t {
    union {
        struct {
             uint32_t TS_NS_CNT        : 32; /**< Write to the Nanoseconds Counter. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_TS_NS_CNT_t;

/*@}*/

/** @defgroup TS_CFG Timer Configuration Register. (TS_CFG) Register
 *  Timer Configuration Register. (TS_CFG) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_TS_CFG_t
 *  \brief Timer Configuration Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_TS_CFG_t {
    union {
        struct {
             uint32_t TS_EN            : 1; /**< Enable the 1588 Timestamp Counter. */
             uint32_t TS_CLR           : 1; /**< Clear the 1588 Timestamp Counters. */
             uint32_t TS_TIMER_DEF     : 1; /**< The Default Value for the TS_TIMER Output. */
             uint32_t TS_TIMER_STOP    : 1; /**< Stop Toggling the TS_TIMER Output. */
             uint32_t TS_CAPT_FREE_CNT : 1; /**< Capture the Free Running Counter. */
             uint32_t RESERVED5        : 27; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_TS_CFG_t;

/*@}*/

/** @defgroup TS_TIMER_HI High Period for TS_TIMER Register. (TS_TIMER_HI) Register
 *  High Period for TS_TIMER Register. (TS_TIMER_HI) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_TS_TIMER_HI_t
 *  \brief High Period for TS_TIMER Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_TS_TIMER_HI_t {
    union {
        struct {
             uint32_t TS_TIMER_HI      : 32; /**< TS_TIMER High Period. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_TS_TIMER_HI_t;

/*@}*/

/** @defgroup TS_TIMER_LO Low Period for TS_TIMER Register. (TS_TIMER_LO) Register
 *  Low Period for TS_TIMER Register. (TS_TIMER_LO) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_TS_TIMER_LO_t
 *  \brief Low Period for TS_TIMER Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_TS_TIMER_LO_t {
    union {
        struct {
             uint32_t TS_TIMER_LO      : 32; /**< TS_TIMER Low Period. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_TS_TIMER_LO_t;

/*@}*/

/** @defgroup TS_TIMER_QE_CORR Quantization Error Correction Register. (TS_TIMER_QE_CORR) Register
 *  Quantization Error Correction Register. (TS_TIMER_QE_CORR) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_TS_TIMER_QE_CORR_t
 *  \brief Quantization Error Correction Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_TS_TIMER_QE_CORR_t {
    union {
        struct {
             uint32_t TS_TIMER_QE_CORR : 8; /**< TS_TIMER Quantization Error Correction Value. */
             uint32_t RESERVED8        : 24; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_TS_TIMER_QE_CORR_t;

/*@}*/

/** @defgroup TS_TIMER_START TS_TIMER Counter Start Time Register. (TS_TIMER_START) Register
 *  TS_TIMER Counter Start Time Register. (TS_TIMER_START) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_TS_TIMER_START_t
 *  \brief TS_TIMER Counter Start Time Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_TS_TIMER_START_t {
    union {
        struct {
             uint32_t TS_TSTART        : 32; /**< Point in Time at Which to Start the TS_TIMER Counter. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_TS_TIMER_START_t;

/*@}*/

/** @defgroup TS_EXT_CAPT0 TS_CAPT Pin 0 Timestamp Register. (TS_EXT_CAPT0) Register
 *  TS_CAPT Pin 0 Timestamp Register. (TS_EXT_CAPT0) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_TS_EXT_CAPT0_t
 *  \brief TS_CAPT Pin 0 Timestamp Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_TS_EXT_CAPT0_t {
    union {
        struct {
             uint32_t TS_EXT_CAPTD     : 32; /**< Timestamp Captured on the Assertion of TS_CAPT Pin. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_TS_EXT_CAPT0_t;

/*@}*/

/** @defgroup TS_EXT_CAPT1 TS_CAPT Pin 1 Timestamp Register. (TS_EXT_CAPT1) Register
 *  TS_CAPT Pin 1 Timestamp Register. (TS_EXT_CAPT1) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_TS_EXT_CAPT1_t
 *  \brief TS_CAPT Pin 1 Timestamp Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_TS_EXT_CAPT1_t {
    union {
        struct {
             uint32_t TS_EXT_CAPTD     : 32; /**< Timestamp Captured on the Assertion of TS_CAPT Pin. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_TS_EXT_CAPT1_t;

/*@}*/

/** @defgroup TS_FREECNT_CAPT TS_CAPT Free Running Counter Register. (TS_FREECNT_CAPT) Register
 *  TS_CAPT Free Running Counter Register. (TS_FREECNT_CAPT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_TS_FREECNT_CAPT_t
 *  \brief TS_CAPT Free Running Counter Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_TS_FREECNT_CAPT_t {
    union {
        struct {
             uint32_t TS_CNT_CAPTD     : 32; /**< Captured Free Running Counter. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_TS_FREECNT_CAPT_t;

/*@}*/

/** @defgroup P1_RX_FSIZE P1 MAC Rx Frame Size Register. (P1_RX_FSIZE) Register
 *  P1 MAC Rx Frame Size Register. (P1_RX_FSIZE) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_RX_FSIZE_t
 *  \brief P1 MAC Rx Frame Size Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_RX_FSIZE_t {
    union {
        struct {
             uint32_t P1_RX_FRM_SIZE   : 11; /**< Receive Frame Size. */
             uint32_t RESERVED11       : 21; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_RX_FSIZE_t;

/*@}*/

/** @defgroup P1_RX P1 MAC Receive Register. (P1_RX) Register
 *  P1 MAC Receive Register. (P1_RX) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_RX_t
 *  \brief P1 MAC Receive Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_RX_t {
    union {
        struct {
             uint32_t P1_RDR           : 32; /**< Receive Data Register. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_RX_t;

/*@}*/

/** @defgroup P1_RX_FRM_CNT P1 Rx Frame Count Register. (P1_RX_FRM_CNT) Register
 *  P1 Rx Frame Count Register. (P1_RX_FRM_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_RX_FRM_CNT_t
 *  \brief P1 Rx Frame Count Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_RX_FRM_CNT_t {
    union {
        struct {
             uint32_t P1_RX_FRM_CNT    : 32; /**< Rx Frame Count. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_RX_FRM_CNT_t;

/*@}*/

/** @defgroup P1_RX_BCAST_CNT P1 Rx Broadcast Frame Count Register. (P1_RX_BCAST_CNT) Register
 *  P1 Rx Broadcast Frame Count Register. (P1_RX_BCAST_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_RX_BCAST_CNT_t
 *  \brief P1 Rx Broadcast Frame Count Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_RX_BCAST_CNT_t {
    union {
        struct {
             uint32_t P1_RX_BCAST_CNT  : 32; /**< Rx Broadcast Frame Count. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_RX_BCAST_CNT_t;

/*@}*/

/** @defgroup P1_RX_MCAST_CNT P1 Rx Multicast Frame Count Register. (P1_RX_MCAST_CNT) Register
 *  P1 Rx Multicast Frame Count Register. (P1_RX_MCAST_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_RX_MCAST_CNT_t
 *  \brief P1 Rx Multicast Frame Count Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_RX_MCAST_CNT_t {
    union {
        struct {
             uint32_t P1_RX_MCAST_CNT  : 32; /**< Rx Multicast Frame Count. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_RX_MCAST_CNT_t;

/*@}*/

/** @defgroup P1_RX_UCAST_CNT P1 Rx Unicast Frame Count Register. (P1_RX_UCAST_CNT) Register
 *  P1 Rx Unicast Frame Count Register. (P1_RX_UCAST_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_RX_UCAST_CNT_t
 *  \brief P1 Rx Unicast Frame Count Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_RX_UCAST_CNT_t {
    union {
        struct {
             uint32_t P1_RX_UCAST_CNT  : 32; /**< Rx Unicast Frame Count. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_RX_UCAST_CNT_t;

/*@}*/

/** @defgroup P1_RX_CRC_ERR_CNT P1 Rx CRC Errored Frame Count Register. (P1_RX_CRC_ERR_CNT) Register
 *  P1 Rx CRC Errored Frame Count Register. (P1_RX_CRC_ERR_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_RX_CRC_ERR_CNT_t
 *  \brief P1 Rx CRC Errored Frame Count Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_RX_CRC_ERR_CNT_t {
    union {
        struct {
             uint32_t P1_RX_CRC_ERR_CNT : 32; /**< Rx CRC Errored Frame Count. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_RX_CRC_ERR_CNT_t;

/*@}*/

/** @defgroup P1_RX_ALGN_ERR_CNT P1 Rx Align Error Count Register. (P1_RX_ALGN_ERR_CNT) Register
 *  P1 Rx Align Error Count Register. (P1_RX_ALGN_ERR_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_RX_ALGN_ERR_CNT_t
 *  \brief P1 Rx Align Error Count Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_RX_ALGN_ERR_CNT_t {
    union {
        struct {
             uint32_t P1_RX_ALGN_ERR_CNT : 32; /**< Rx Align Error Count. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_RX_ALGN_ERR_CNT_t;

/*@}*/

/** @defgroup P1_RX_LS_ERR_CNT P1 Rx Long/Short Frame Error Count Register. (P1_RX_LS_ERR_CNT) Register
 *  P1 Rx Long/Short Frame Error Count Register. (P1_RX_LS_ERR_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_RX_LS_ERR_CNT_t
 *  \brief P1 Rx Long/Short Frame Error Count Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_RX_LS_ERR_CNT_t {
    union {
        struct {
             uint32_t P1_RX_LS_ERR_CNT : 32; /**< Rx Long/Short Frame Error Count. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_RX_LS_ERR_CNT_t;

/*@}*/

/** @defgroup P1_RX_PHY_ERR_CNT P1 Rx PHY Error Count Register. (P1_RX_PHY_ERR_CNT) Register
 *  P1 Rx PHY Error Count Register. (P1_RX_PHY_ERR_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_RX_PHY_ERR_CNT_t
 *  \brief P1 Rx PHY Error Count Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_RX_PHY_ERR_CNT_t {
    union {
        struct {
             uint32_t P1_RX_PHY_ERR_CNT : 32; /**< Rx PHY Error Count. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_RX_PHY_ERR_CNT_t;

/*@}*/

/** @defgroup P1_TX_FRM_CNT P1 Tx Frame Count Register. (P1_TX_FRM_CNT) Register
 *  P1 Tx Frame Count Register. (P1_TX_FRM_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_TX_FRM_CNT_t
 *  \brief P1 Tx Frame Count Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_TX_FRM_CNT_t {
    union {
        struct {
             uint32_t P1_TX_FRM_CNT    : 32; /**< Tx Frame Count. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_TX_FRM_CNT_t;

/*@}*/

/** @defgroup P1_TX_BCAST_CNT P1 Tx Broadcast Frame Count Register. (P1_TX_BCAST_CNT) Register
 *  P1 Tx Broadcast Frame Count Register. (P1_TX_BCAST_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_TX_BCAST_CNT_t
 *  \brief P1 Tx Broadcast Frame Count Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_TX_BCAST_CNT_t {
    union {
        struct {
             uint32_t P1_TX_BCAST_CNT  : 32; /**< Tx Broadcast Frame Count. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_TX_BCAST_CNT_t;

/*@}*/

/** @defgroup P1_TX_MCAST_CNT P1 Tx Multicast Frame Count Register. (P1_TX_MCAST_CNT) Register
 *  P1 Tx Multicast Frame Count Register. (P1_TX_MCAST_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_TX_MCAST_CNT_t
 *  \brief P1 Tx Multicast Frame Count Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_TX_MCAST_CNT_t {
    union {
        struct {
             uint32_t P1_TX_MCAST_CNT  : 32; /**< Tx Multicast Frame Count. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_TX_MCAST_CNT_t;

/*@}*/

/** @defgroup P1_TX_UCAST_CNT P1 Tx Unicast Frame Count Register. (P1_TX_UCAST_CNT) Register
 *  P1 Tx Unicast Frame Count Register. (P1_TX_UCAST_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_TX_UCAST_CNT_t
 *  \brief P1 Tx Unicast Frame Count Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_TX_UCAST_CNT_t {
    union {
        struct {
             uint32_t P1_TX_UCAST_CNT  : 32; /**< Tx Unicast Frame Count. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_TX_UCAST_CNT_t;

/*@}*/

/** @defgroup P1_RX_DROP_FULL_CNT P1 Rx Frames Dropped Due to FIFO Full Register. (P1_RX_DROP_FULL_CNT) Register
 *  P1 Rx Frames Dropped Due to FIFO Full Register. (P1_RX_DROP_FULL_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_RX_DROP_FULL_CNT_t
 *  \brief P1 Rx Frames Dropped Due to FIFO Full Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_RX_DROP_FULL_CNT_t {
    union {
        struct {
             uint32_t P1_RX_DROP_FULL_CNT : 32; /**< Rx Frames Dropped Due to FIFO Full. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_RX_DROP_FULL_CNT_t;

/*@}*/

/** @defgroup P1_RX_DROP_FILT_CNT P1 Rx Frames Dropped Due to Filtering Register. (P1_RX_DROP_FILT_CNT) Register
 *  P1 Rx Frames Dropped Due to Filtering Register. (P1_RX_DROP_FILT_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_RX_DROP_FILT_CNT_t
 *  \brief P1 Rx Frames Dropped Due to Filtering Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_RX_DROP_FILT_CNT_t {
    union {
        struct {
             uint32_t P1_RX_DROP_FILT_CNT : 32; /**< Rx Frames Dropped Due to Filtering. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_RX_DROP_FILT_CNT_t;

/*@}*/

/** @defgroup P1_RX_IFG_ERR_CNT Frame Received on Port 1 with IFG Errors. (P1_RX_IFG_ERR_CNT) Register
 *  Frame Received on Port 1 with IFG Errors. (P1_RX_IFG_ERR_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_RX_IFG_ERR_CNT_t
 *  \brief Frame Received on Port 1 with IFG Errors.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_RX_IFG_ERR_CNT_t {
    union {
        struct {
             uint32_t P1_RX_IFG_ERR_CNT : 32; /**< IFG Error Counter for Port 1 Received Frames. */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_RX_IFG_ERR_CNT_t;

/*@}*/

/** @defgroup P1_TX_IFG P1 Transmit Inter Frame Gap Register. (P1_TX_IFG) Register
 *  P1 Transmit Inter Frame Gap Register. (P1_TX_IFG) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_TX_IFG_t
 *  \brief P1 Transmit Inter Frame Gap Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_TX_IFG_t {
    union {
        struct {
             uint32_t P1_TX_IFG        : 8; /**< Inter Frame Gap. */
             uint32_t RESERVED8        : 24; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_TX_IFG_t;

/*@}*/

/** @defgroup P1_LOOP P1 MAC Loopback Enable Register. (P1_LOOP) Register
 *  P1 MAC Loopback Enable Register. (P1_LOOP) Register
 *  @{
 */

/*! ========================================================================
 *  \enum ETH_MAC_P1_LOOP_P1_LOOPBACK_EN
 *  \brief MAC Loopback.
 *  ======================================================================== */
typedef enum
{
     MAC_P1_LOOP_P1_LOOPBACK_EN_LOOPB_DIS = 0,    /**< Normal Operation - Loopback Disabled. */
     MAC_P1_LOOP_P1_LOOPBACK_EN_LOOPB_EN = 1     /**< Loopback Enabled. */
}  ETH_MAC_P1_LOOP_P1_LOOPBACK_EN;

/*! ========================================================================
 *  \struct ETH_MAC_P1_LOOP_t
 *  \brief P1 MAC Loopback Enable Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_LOOP_t {
    union {
        struct {
             uint32_t P1_LOOPBACK_EN   : 1; /**< MAC Loopback. */
             uint32_t RESERVED1        : 31; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_LOOP_t;

/*@}*/

/** @defgroup P1_RX_CRC_EN P1 CRC Check Enable on Receive Register. (P1_RX_CRC_EN) Register
 *  P1 CRC Check Enable on Receive Register. (P1_RX_CRC_EN) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_RX_CRC_EN_t
 *  \brief P1 CRC Check Enable on Receive Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_RX_CRC_EN_t {
    union {
        struct {
             uint32_t P1_CRC_CHK_EN    : 1; /**< CRC Check Enable on Receive. */
             uint32_t RESERVED1        : 31; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_RX_CRC_EN_t;

/*@}*/

/** @defgroup P1_RX_IFG P1 Receive Inter Frame Gap Register. (P1_RX_IFG) Register
 *  P1 Receive Inter Frame Gap Register. (P1_RX_IFG) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_RX_IFG_t
 *  \brief P1 Receive Inter Frame Gap Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_RX_IFG_t {
    union {
        struct {
             uint32_t P1_RX_IFG        : 6; /**< Inter Frame Gap. */
             uint32_t RESERVED6        : 26; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_RX_IFG_t;

/*@}*/

/** @defgroup P1_RX_MAX_LEN P1 Max Receive Frame Length Register. (P1_RX_MAX_LEN) Register
 *  P1 Max Receive Frame Length Register. (P1_RX_MAX_LEN) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_RX_MAX_LEN_t
 *  \brief P1 Max Receive Frame Length Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_RX_MAX_LEN_t {
    union {
        struct {
             uint32_t P1_MAX_FRM_LEN   : 16; /**< Max Frame Length on Receive. */
             uint32_t RESERVED16       : 16; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_RX_MAX_LEN_t;

/*@}*/

/** @defgroup P1_RX_MIN_LEN P1 Min Receive Frame Length Register. (P1_RX_MIN_LEN) Register
 *  P1 Min Receive Frame Length Register. (P1_RX_MIN_LEN) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_RX_MIN_LEN_t
 *  \brief P1 Min Receive Frame Length Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_RX_MIN_LEN_t {
    union {
        struct {
             uint32_t P1_MIN_FRM_LEN   : 16; /**< Min Frame Length on Receive. */
             uint32_t RESERVED16       : 16; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_RX_MIN_LEN_t;

/*@}*/

/** @defgroup P1_LO_RFC P1 Rx Low Priority FIFO Frame Count Register. (P1_LO_RFC) Register
 *  P1 Rx Low Priority FIFO Frame Count Register. (P1_LO_RFC) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_LO_RFC_t
 *  \brief P1 Rx Low Priority FIFO Frame Count Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_LO_RFC_t {
    union {
        struct {
             uint32_t P1_LO_RFC        : 9; /**< Receive Frame Count for the Low Priority FIFO. */
             uint32_t RESERVED9        : 23; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_LO_RFC_t;

/*@}*/

/** @defgroup P1_HI_RFC P1 Rx High Priority FIFO Frame Count Register. (P1_HI_RFC) Register
 *  P1 Rx High Priority FIFO Frame Count Register. (P1_HI_RFC) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_HI_RFC_t
 *  \brief P1 Rx High Priority FIFO Frame Count Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_HI_RFC_t {
    union {
        struct {
             uint32_t P1_HI_RFC        : 9; /**< Receive Frame Count for the High Priority FIFO. */
             uint32_t RESERVED9        : 23; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_HI_RFC_t;

/*@}*/

/** @defgroup P1_LO_RXSIZE P1 Low Priority Rx FIFO Valid Half Words Register. (P1_LO_RXSIZE) Register
 *  P1 Low Priority Rx FIFO Valid Half Words Register. (P1_LO_RXSIZE) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_LO_RXSIZE_t
 *  \brief P1 Low Priority Rx FIFO Valid Half Words Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_LO_RXSIZE_t {
    union {
        struct {
             uint32_t P1_LO_RXSIZE     : 14; /**< Data in the Rx FIFO. Number of Half Words(16 Bit). */
             uint32_t RESERVED14       : 18; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_LO_RXSIZE_t;

/*@}*/

/** @defgroup P1_HI_RXSIZE P1 High Priority Rx FIFO Valid Half Words Register. (P1_HI_RXSIZE) Register
 *  P1 High Priority Rx FIFO Valid Half Words Register. (P1_HI_RXSIZE) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ETH_MAC_P1_HI_RXSIZE_t
 *  \brief P1 High Priority Rx FIFO Valid Half Words Register.
 *  ======================================================================== */
typedef struct _ETH_MAC_P1_HI_RXSIZE_t {
    union {
        struct {
             uint32_t P1_HI_RXSIZE     : 14; /**< Data in the Rx FIFO. Number of Half Words(16 Bit). */
             uint32_t RESERVED14       : 18; /**< Reserved */
        };
        uint32_t VALUE32;
    };
} ETH_MAC_P1_HI_RXSIZE_t;

/*@}*/

/*! @endcond */

/* ====================================================================================================
   ADIN2111-Specific Register Addresses (P2)
   ==================================================================================================== */
#ifndef ADDR_MAC_P2_RX_FSIZE
#define ADDR_MAC_P2_RX_FSIZE                                     (0X0C0U)
#define ADDR_MAC_P2_RX                                           (0X0C1U)
#define ADDR_MAC_P2_RX_FRM_CNT                                   (0X0D0U)
#define ADDR_MAC_P2_RX_BCAST_CNT                                 (0X0D1U)
#define ADDR_MAC_P2_RX_MCAST_CNT                                 (0X0D2U)
#define ADDR_MAC_P2_RX_UCAST_CNT                                 (0X0D3U)
#define ADDR_MAC_P2_RX_CRC_ERR_CNT                               (0X0D4U)
#define ADDR_MAC_P2_RX_ALGN_ERR_CNT                              (0X0D5U)
#define ADDR_MAC_P2_RX_LS_ERR_CNT                                (0X0D6U)
#define ADDR_MAC_P2_RX_PHY_ERR_CNT                               (0X0D7U)
#define ADDR_MAC_P2_TX_FRM_CNT                                   (0X0D8U)
#define ADDR_MAC_P2_TX_BCAST_CNT                                 (0X0D9U)
#define ADDR_MAC_P2_TX_MCAST_CNT                                 (0X0DAU)
#define ADDR_MAC_P2_TX_UCAST_CNT                                 (0X0DBU)
#define ADDR_MAC_P2_RX_DROP_FULL_CNT                             (0X0DCU)
#define ADDR_MAC_P2_RX_DROP_FILT_CNT                             (0X0DDU)
#define ADDR_MAC_P2_RX_IFG_ERR_CNT                               (0X0DEU)
#define ADDR_MAC_P2_TX_IFG                                       (0X0E0U)
#define ADDR_MAC_P2_LOOP                                         (0X0E3U)
#define ADDR_MAC_P2_RX_CRC_EN                                    (0X0E4U)
#define ADDR_MAC_P2_RX_IFG                                       (0X0E5U)
#define ADDR_MAC_P2_RX_MAX_LEN                                   (0X0E6U)
#define ADDR_MAC_P2_RX_MIN_LEN                                   (0X0E7U)
#define ADDR_MAC_P2_LO_RFC                                       (0X0E8U)
#define ADDR_MAC_P2_HI_RFC                                       (0X0E9U)
#define ADDR_MAC_P2_LO_RXSIZE                                    (0X0EAU)
#define ADDR_MAC_P2_HI_RXSIZE                                    (0X0EBU)
#define ADDR_MAC_P2_TTSCAH                                       (0X0F0U)
#define ADDR_MAC_P2_TTSCAL                                       (0X0F1U)
#define ADDR_MAC_P2_TTSCBH                                       (0X0F2U)
#define ADDR_MAC_P2_TTSCBL                                       (0X0F3U)
#define ADDR_MAC_P2_TTSCCH                                       (0X0F4U)
#define ADDR_MAC_P2_TTSCCL                                       (0X0F5U)
#endif /* ADDR_MAC_P2_RX_FSIZE */

/* ====================================================================================================
   ADIN2111 STATUS1 P2 Bit Positions and Masks
   ==================================================================================================== */
#ifndef BITP_MAC_STATUS1_P2_RX_RDY
#define BITP_MAC_STATUS1_P2_RX_RDY                            (17U)
#define BITM_MAC_STATUS1_P2_RX_RDY                            (0X00020000U)
#define BITP_MAC_STATUS1_P2_RX_RDY_HI                         (18U)
#define BITM_MAC_STATUS1_P2_RX_RDY_HI                         (0X00040000U)
#define BITP_MAC_STATUS1_P2_PHYINT                            (19U)
#define BITM_MAC_STATUS1_P2_PHYINT                            (0X00080000U)
#define BITP_MAC_STATUS1_P2_TTSCAA                            (20U)
#define BITM_MAC_STATUS1_P2_TTSCAA                            (0X00100000U)
#define BITP_MAC_STATUS1_P2_TTSCAB                            (21U)
#define BITM_MAC_STATUS1_P2_TTSCAB                            (0X00200000U)
#define BITP_MAC_STATUS1_P2_TTSCAC                            (22U)
#define BITM_MAC_STATUS1_P2_TTSCAC                            (0X00400000U)
#define BITP_MAC_STATUS1_P2_RX_IFG_ERR                        (23U)
#define BITM_MAC_STATUS1_P2_RX_IFG_ERR                        (0X00800000U)
#define BITP_MAC_STATUS1_P2_TXFCSE                            (24U)
#define BITM_MAC_STATUS1_P2_TXFCSE                            (0X01000000U)
#endif /* BITP_MAC_STATUS1_P2_RX_RDY */

/* ====================================================================================================
   ADIN2111 IMASK1 P2 Bit Positions and Masks
   ==================================================================================================== */
#ifndef BITP_MAC_IMASK1_P2_RX_RDY_MASK
#define BITP_MAC_IMASK1_P2_RX_RDY_MASK                        (17U)
#define BITM_MAC_IMASK1_P2_RX_RDY_MASK                        (0X00020000U)
#define BITP_MAC_IMASK1_P2_PHYINT_MASK                        (19U)
#define BITM_MAC_IMASK1_P2_PHYINT_MASK                        (0X00080000U)
#define BITP_MAC_IMASK1_P2_TTSCAAM                            (20U)
#define BITM_MAC_IMASK1_P2_TTSCAAM                            (0X00100000U)
#define BITP_MAC_IMASK1_P2_TTSCABM                            (21U)
#define BITM_MAC_IMASK1_P2_TTSCABM                            (0X00200000U)
#define BITP_MAC_IMASK1_P2_TTSCACM                            (22U)
#define BITM_MAC_IMASK1_P2_TTSCACM                            (0X00400000U)
#define BITP_MAC_IMASK1_P2_RX_IFG_ERR_MASK                    (23U)
#define BITM_MAC_IMASK1_P2_RX_IFG_ERR_MASK                    (0X00800000U)
#define BITP_MAC_IMASK1_P2_TXFCSEM                            (24U)
#define BITM_MAC_IMASK1_P2_TXFCSEM                            (0X01000000U)
#endif /* BITP_MAC_IMASK1_P2_RX_RDY_MASK */

/* ====================================================================================================
   ADIN2111 CONFIG2 Forwarding Bit Positions and Masks
   ==================================================================================================== */
#ifndef BITP_MAC_CONFIG2_P2_FWD_UNK2HOST
#define BITP_MAC_CONFIG2_P2_FWD_UNK2HOST                      (12U)
#define BITM_MAC_CONFIG2_P2_FWD_UNK2HOST                      (0X00001000U)
#define BITP_MAC_CONFIG2_P1_FWD_UNK2P2                        (13U)
#define BITM_MAC_CONFIG2_P1_FWD_UNK2P2                        (0X00002000U)
#define BITP_MAC_CONFIG2_P2_FWD_UNK2P1                        (14U)
#define BITM_MAC_CONFIG2_P2_FWD_UNK2P1                        (0X00004000U)
#endif /* BITP_MAC_CONFIG2_P2_FWD_UNK2HOST */

#ifdef __cplusplus
}
#endif
#endif /* ETH_MAC_H */

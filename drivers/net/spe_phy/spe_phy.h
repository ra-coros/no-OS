/*
 *---------------------------------------------------------------------------
 *
 * Copyright (c) 2020, 2021 Analog Devices, Inc. All Rights Reserved.
 * This software is proprietary to Analog Devices, Inc.
 * and its licensors.By using this software you agree to the terms of the
 * associated Analog Devices Software License Agreement.
 *
 *---------------------------------------------------------------------------
 */

/** @addtogroup phy PHY Definitions
 *  @{
 */

#ifndef SPE_PHY_H
#define SPE_PHY_H

#include "capi_gpio.h"
#include "capi_irq.h"
#include "capi_time.h"
#include "no_os_util.h"
#include "utilities.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#if defined(ADIN1100)
#include "ADIN1100_addr_rdef.h"
#if defined(MDIO_CL22)
#include "ADIN1100_addr_rdef_22.h"
#endif
#endif

// #include "adi_eth_common.h"
// #include "config.h"     
// #include "hal.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*! Size of the PHY device structure, in bytes. Needs to be a multiple of 4. */
#define SPE_PHY_DEVICE_SIZE             (sizeof(spe_phy_desc))

/*! Hardware reset value of MMD1_DEV_ID1 register, used for device identification. */
#define SPE_PHY_DEVID1                  (0x0283)
/*! Hardware reset value of MMD1_DEV_ID2 register (OUI field), used for device identification. */
#define SPE_PHY_DEVID2_OUI              (0x2F)

/*! CRSM interrupt sources showing a hardware error that requires a reset/reconfiguration of the device. */
#define SPE_PHY_CRSM_HW_ERROR           (0x2BFF)

/*!
 * @brief PHY driver states.
 */
enum spe_phy_state {
    SPE_PHY_STATE_UNINITIALIZED = 0,        /*!< Software driver has not been initialized.                        */
    SPE_PHY_STATE_HW_RESET,                 /*!< PHY has been through hardware reset and needs to be initialized. */
    SPE_PHY_STATE_SOFTWARE_POWERDOWN,       /*!< PHY is in software powerdown.                                    */
    SPE_PHY_STATE_OPERATION,                /*!< PHY is in operation mode.                                        */
    SPE_PHY_STATE_DIAGNOSTIC,               /*!< PHY is in diagnostic mode.                                       */
    SPE_PHY_STATE_ERROR,                    /*!< Error state.                                                     */
};

/*!
 * @brief Advertised transmit operating mode.
 */
enum spe_phy_an_adv_tx_req_mode {
    SPE_PHY_AN_ADV_TX_REQ_1P0V = 0,         /*!< Request 1.0V transmit signal amplitude.                        */
    SPE_PHY_AN_ADV_TX_REQ_2P4V,             /*!< Request 2.4V transmit signal amplitude.                        */
    SPE_PHY_AN_ADV_TX_REQ_1P0V_ABLE_2P4V,   /*!< Request 1.0V transmit signal amplitude but able to do 2.4V.    */
};

/*!
 * @brief Resolved transmit operating mode.
 */
enum spe_phy_an_tx_level_mode {
    SPE_PHY_AN_TX_LEVEL_RESOLUTION_NOT_RUN = 0,/*!< Auto-negotiation not run.              */
    SPE_PHY_AN_TX_LEVEL_RESERVED,          /*!< Auto-negotiation configuration fault.  */
    SPE_PHY_AN_TX_LEVEL_1P0V,         /*!< Resolved 1.0V transmit signal amplitude.                        */
    SPE_PHY_AN_TX_LEVEL_2P4V,             /*!< Resolved 2.4V transmit signal amplitude.                        */
};

/*!
 * @brief Auto-negotiation master-slave advertisement.
 */
enum spe_phy_an_adv_master_slave_cfg {
    SPE_PHY_AN_ADV_FORCED_MASTER = 0,       /*!< Force master.          */
    SPE_PHY_AN_ADV_FORCED_SLAVE,            /*!< Force slave.           */
    SPE_PHY_AN_ADV_PREFFERED_MASTER,        /*!< Preferred master.      */
    SPE_PHY_AN_ADV_PREFFERED_SLAVE,         /*!< Preferred slave.       */
};

/*!
 * @brief Auto-negotiation master-slave resolution.
 */
enum spe_phy_AnMsResolution_e {
    SPE_PHY_AN_MS_RESOLUTION_NOT_RUN   = ENUM_AN_STATUS_EXTRA_AN_MS_CONFIG_RSLTN_MS_NOT_RUN,    /*!< Auto-negotiation not run.              */
    SPE_PHY_AN_MS_RESOLUTION_FAULT     = ENUM_AN_STATUS_EXTRA_AN_MS_CONFIG_RSLTN_MS_FAULT,      /*!< Auto-negotiation configuration fault.  */
    SPE_PHY_AN_MS_RESOLUTION_SLAVE     = ENUM_AN_STATUS_EXTRA_AN_MS_CONFIG_RSLTN_MS_SLAVE,      /*!< Success, PHY configured as slave.      */
    SPE_PHY_AN_MS_RESOLUTION_MASTER    = ENUM_AN_STATUS_EXTRA_AN_MS_CONFIG_RSLTN_MS_MASTER,     /*!< Success, PHY configured as master.     */
};

/*!
 * @brief PHY reset types.
 */
enum spe_phy_reset_type {
    SPE_PHY_RESET_TYPE_SW = 0,      /*!< Software reset.                                        */
    SPE_PHY_RESET_TYPE_HW,          /*!< Hardware reset. Note this is currently not supported.  */
};

/*!
 * @brief PHY interrupt events.
 */
enum spe_phy_InterruptEvt_e {
    SPE_PHY_EVT_HW_RESET            = (1 << BITP_CRSM_IRQ_STATUS_CRSM_HRD_RST_IRQ_LH),
    SPE_PHY_EVT_SW_RESET            = (1 << BITP_CRSM_IRQ_STATUS_CRSM_SW_IRQ_LH),
    SPE_PHY_EVT_LINK_STAT_CHANGE    = (1 << (BITP_PHY_SUBSYS_IRQ_STATUS_LINK_STAT_CHNG_LH + 16)),
    SPE_PHY_EVT_MAC_IF_BUF          = (1 << (BITP_PHY_SUBSYS_IRQ_STATUS_MAC_IF_EBUF_ERR_IRQ_LH + 16)),
    SPE_PHY_EVT_CRSM_HW_ERROR       = SPE_PHY_CRSM_HW_ERROR,
};

/*!
 * @name PHY Capabilities
 * List of PHY capabilities.
 */
/** @{ */
#define SPE_PHY_CAP_NONE                    (0)         /*!< No PHY support (base value).                  */
#define SPE_PHY_CAP_TX_HIGH_LEVEL           (1 << 1)    /*!< PHY supports 10BASE-T1L 2.4V transmit level.  */
#define SPE_PHY_CAP_PMA_LOCAL_LOOPBACK      (1 << 2)    /*!< PHY supports 10BASE-T1L PMA local loopback.   */
/** @} */

/*!
* @brief Link status.
*/
enum spe_phy_link_status {
    SPE_PHY_LINK_STATUS_DOWN        = (0),         /*!< Link down.  */
    SPE_PHY_LINK_STATUS_UP          = (1),         /*!< Link up.    */
};

/*!
* @brief PHY loopback modes.
*/
enum spe_phy_loopback_mode {
    SPE_PHY_LOOPBACK_NONE = 0,                  /*!< No loopback (normal operation).                        */
    SPE_PHY_LOOPBACK_PCS,                       /*!< 10BASE-T1L PCS loopback.                               */
    SPE_PHY_LOOPBACK_PMA,                       /*!< 10BASE-T1L PMA loopback.                               */
    SPE_PHY_LOOPBACK_MACIF,                     /*!< MAC interface loopback, Tx data is looped back to Rx.  */
    SPE_PHY_LOOPBACK_MACIF_SUPPRESS_TX,         /*!< MAC interface loopback, Tx data is looped back to Rx.
                                                     Suppress Tx to the PHY.                                */
    SPE_PHY_LOOPBACK_MACIF_REMOTE,              /*!< MAC interface remote loopback. Rx data is looped back to Tx. */
    SPE_PHY_LOOPBACK_MACIF_REMOTE_SUPPRESS_RX,  /*!< MAC interface remote loopback. Rx data is looped back to Tx.
                                                     Suppress Rx to the MAC.                                      */
};

/*!
* @brief PHY test modes.
*/
enum spe_phy_test_mode {
    SPE_PHY_TEST_MODE_NONE = 0,                 /*!< No test mode (normal operation).                                           */
    SPE_PHY_TEST_MODE_1,                        /*!< Test mode 1 from IEEE 802.3cgTM-2019, subclause 146.5.2.                   */
    SPE_PHY_TEST_MODE_2,                        /*!< Test mode 2 from IEEE 802.3cgTM-2019, subclause 146.5.2.                   */
    SPE_PHY_TEST_MODE_3,                        /*!< Test mode 3 from IEEE 802.3cgTM-2019, subclause 146.5.2.                   */
    SPE_PHY_TEST_MODE_TX_DISABLE,               /*!< Transmite disable mode from IEEE 802.3cgTM-2019, subclause 45.2.1.186a.2.  */
};

/*!
* @brief Link quality.
*/
enum spe_phy_link_quality {
    SPE_PHY_LINK_QUALITY_POOR = 0,
    SPE_PHY_LINK_QUALITY_MARGINAL,
    SPE_PHY_LINK_QUALITY_GOOD,
};

/*!
* @brief Link quality.
*/
struct spe_phy_mse_link_quality {
    uint16_t                	mse_val;             /*!< Raw MSE value read from the MSE_VAL register.  */
    enum spe_phy_link_quality   link_quality;        /*!< Link quality measure.                          */
    uint8_t                 	sqi;                /*!< Signal Quality Indicator.                      */
};

/*!
 * @brief PHY LED ports.
 */
enum spe_phy_led_port {
    SPE_PHY_LED_0 = 0,                          /*!< LED 0. */
    SPE_PHY_LED_1,                              /*!< LED 1. */
};

/*!
 * @brief PHY frame generator mode.
 */
enum spe_phy_frame_gen_mode {
    SPE_PHY_FRAME_GEN_MODE_BURST = 0,           /*!< Burst mode.        */
    SPE_PHY_FRAME_GEN_MODE_CONT,                /*!< Continuous mode.   */
};

/*!
 * @brief PHY frame generator payload.
 */
enum spe_phy_frame_gen_payload {
    SPE_PHY_FRAME_GEN_PAYLOAD_NONE          = ENUM_FG_CNTRL_RSTRT_FG_CNTRL_FG_GEN_NONE,                 /*!< No frames after current frame.     */
    SPE_PHY_FRAME_GEN_PAYLOAD_RANDOM        = ENUM_FG_CNTRL_RSTRT_FG_CNTRL_FG_GEN_RANDOM_PAYLOAD,       /*!< Random payload.                    */
    SPE_PHY_FRAME_GEN_PAYLOAD_0X00          = ENUM_FG_CNTRL_RSTRT_FG_CNTRL_FG_GEN_0X00_PAYLOAD,         /*!< Payload of 0x00 repeated.          */
    SPE_PHY_FRAME_GEN_PAYLOAD_0XFF          = ENUM_FG_CNTRL_RSTRT_FG_CNTRL_FG_GEN_0XFF_PAYLOAD,         /*!< Payload of 0xFF repeated.          */
    SPE_PHY_FRAME_GEN_PAYLOAD_0x55          = ENUM_FG_CNTRL_RSTRT_FG_CNTRL_FG_GEN_0X55_PAYLOAD,         /*!< Payload of 0x55 repeated.          */
    SPE_PHY_FRAME_GEN_PAYLOAD_DECR          = ENUM_FG_CNTRL_RSTRT_FG_CNTRL_FG_GEN_DECR_PAYLOAD,         /*!< Payload in decrementing bytes.     */
};

/*!
 * @brief PHY frame check error counters.
 */
struct spe_phy_frame_check_error_counters {
    uint16_t LEN_ERR_CNT;           /*!< Frame checker length error count.              */
    uint16_t ALGN_ERR_CNT;          /*!< Frame checker alignment error count.           */
    uint16_t SYMB_ERR_CNT;          /*!< Frame checker symbol error count.              */
    uint16_t OSZ_CNT;               /*!< Frame checker oversized frame error count.     */
    uint16_t USZ_CNT;               /*!< Frame checker undersized frame error count.    */
    uint16_t ODD_CNT;               /*!< Frame checker odd nibble frame count.          */
    uint16_t ODD_PRE_CNT;           /*!< Frame checker odd preamble packet counter.     */
    uint16_t FALSE_CARRIER_CNT;     /*!< Frame Checker False Carrier Count Register.    */
};

/*!
 * @brief PHY frame checker source.
 */
enum spe_phy_frame_check_source {
    SPE_PHY_FRAME_CHK_SOURCE_PHY = 0,           /*!< Frame checker source is the PHY.   */
    SPE_PHY_FRAME_CHK_SOURCE_MAC,               /*!< Frame checker source is the MAC.   */
};

/*!
 * @brief Auto-negotiation status.
 */
struct spe_phy_an_status {
    bool                        anComplete;         /*!< Auto-negotiation complete.                 */
    enum spe_phy_link_status         anLinkStatus;       /*!< Link status.                               */
    spe_phy_AnMsResolution_e    anMsResolution;     /*!< Auto-negotiation master-slave resolution.  */
    spe_phy_an_tx_level_mode    anTxMode;           /*!< Transmite operating mode.                  */
};

/*! 
 * @brief Pointer to function to use to read PHY registers. 
 */
typedef uint32_t (*HAL_ReadFn_t)    (uint8_t hwAddr, uint32_t reg_addr, uint16_t *data);
/*! 
 * @brief Pointer to function to use to write PHY registers. 
 */
typedef uint32_t (*HAL_WriteFn_t)   (uint8_t hwAddr, uint32_t reg_addr, uint16_t data);


/** @}*/

/*! @cond PRIVATE */

/*! Timeout for MDIO interface bringup after a powerup event or equivalent, in miliseconds. */
#define SPE_PHY_MDIO_POWERUP_TIMEOUT        (100)
/*! Timeout for system ready after a powerup event or equivalent, in miliseconds. */
#define SPE_PHY_SYS_RDY_TIMEOUT             (700)

/*! Timeout for MDIO interface bringup, in number of iterations. */
#define SPE_PHY_MDIO_POWERUP_ITER           (uint32_t)(SPE_PHY_MDIO_POWERUP_TIMEOUT * 1000 / ADI_HAL_MDIO_READ_DURATION)
/*! Timeout for system ready, in number of iterations. */
#define SPE_PHY_SYS_RDY_ITER                (uint32_t)(SPE_PHY_SYS_RDY_TIMEOUT * 1000 / ADI_HAL_MDIO_READ_DURATION)

/*! Number of retries allowed for software powerdown entry, in MDIO read count. */
#define SPE_PHY_SOFT_PD_ITER                (10)

/*! Number of SQI values. */
#define SPE_PHY_SQI_NUM_ENTRIES             (8)

/*! MSE threshold for POOR link quality. */
#define SPE_PHY_LINK_QUALITY_THR_POOR       (0x0766)
/*! MSE threshold for MARGINAL link quality. */
#define SPE_PHY_LINK_QUALITY_THR_MARGINAL   (0x05E1)


struct spe_phy_stats {
    uint32_t link_dropped;
};

struct spe_phy_desc {   
    uint32_t            		phyAddr;
    enum spe_phy_state       	state;
    HAL_ReadFn_t        		readFn;
    HAL_WriteFn_t       		writeFn;
    enum spe_phy_link_status 	link_status;
    adi_eth_Callback_t  		cb_func;
    uint32_t            		cb_events;
    void                		*cb_param;
    void                		*adinDevice;
    enum spe_phy_stats     		stats;
    uint32_t            		irqMask;
    bool                		irqPending;
    uint32_t					eth_irq;
};

struct spe_phy_init_param {   
    uint32_t            		phyAddr;
    void 			    		*dev_mem;                                                
    uint32_t					mem_size;
    enum spe_phy_state       	state;
    HAL_ReadFn_t        		readFn;
    HAL_WriteFn_t       		writeFn;
    enum spe_phy_link_status	link_status;
    void                		*adinDevice;
    bool                		enableIrq;
    uint32_t					eth_irq;
};

int         spe_phy_init                    		(struct spe_phy_desc **desc, struct spe_phy_init_param *param);
int         spe_phy_remove                  		(struct spe_phy_desc *desc);
int         spe_phy_reinit_phy              		(struct spe_phy_desc *desc);
int         spe_phy_register_callback        		(struct spe_phy_desc *desc, adi_eth_Callback_t cb_func, uint32_t cb_events, void *cb_param);
int         spe_phy_read_irq_status           		(struct spe_phy_desc *desc, uint32_t *status);
int         spe_phy_an_adv_tx_mode             		(struct spe_phy_desc *desc, enum spe_phy_an_adv_tx_req_mode tx_mode);
int         spe_phy_an_adv_mst_slv_cfg          	(struct spe_phy_desc *desc, enum spe_phy_an_adv_master_slave_cfg ms_cfg);
int         spe_phy_an_enable                		(struct spe_phy_desc *desc, bool enable);
int         spe_phy_enter_software_powerdown  		(struct spe_phy_desc *desc);
int         spe_phy_exit_software_powerdown   		(struct spe_phy_desc *desc);
int         spe_phy_get_software_powerdown    		(struct spe_phy_desc *desc, bool *enable);
int         spe_phy_get_link_status           		(struct spe_phy_desc *desc, enum spe_phy_link_status *status);
int         spe_phy_renegotiate             		(struct spe_phy_desc *desc);
int         spe_phy_get_an_status             		(struct spe_phy_desc *desc, struct spe_phy_an_status *status);
int         spe_phy_reset                   		(struct spe_phy_desc *desc, enum spe_phy_reset_type reset_type);
int         spe_phy_set_loopback_mode         		(struct spe_phy_desc *desc, enum spe_phy_loopback_mode loopback_mode);
int         spe_phy_set_test_mode             		(struct spe_phy_desc *desc, enum spe_phy_test_mode test_mode);
int         spe_phy_led_enable                   	(struct spe_phy_desc *desc, enum spe_phy_led_port led_sel, bool enable);
int         spe_phy_led_blink_time            		(struct spe_phy_desc *desc, enum spe_phy_led_port led_sel, uint32_t blink_on, uint32_t blink_off);
int         spe_phy_get_capabilities         		(struct spe_phy_desc *desc, uint16_t *capabilities);
int         spe_phy_write                   		(struct spe_phy_desc *desc, uint32_t reg_addr, uint16_t data);
int         spe_phy_read                    		(struct spe_phy_desc *desc, uint32_t reg_addr, uint16_t *data);
int         spe_phy_get_mse_link_quality       		(struct spe_phy_desc *desc, struct spe_phy_mse_link_quality *mse_link_quality);
int         spe_phy_frame_gen_enable              	(struct spe_phy_desc *desc, bool enable);
int         spe_phy_frame_gen_set_mode         		(struct spe_phy_desc *desc, enum spe_phy_frame_gen_mode mode);
int         spe_phy_frame_gen_set_frame_count     	(struct spe_phy_desc *desc, uint32_t frame_count);
int         spe_phy_frame_gen_set_frame_payload 	(struct spe_phy_desc *desc, enum spe_phy_frame_gen_payload payload);
int         spe_phy_frame_gen_set_frame_len     	(struct spe_phy_desc *desc, uint16_t frame_len);
int         spe_phy_frame_gen_set_ifg_len       	(struct spe_phy_desc *desc, uint16_t ifg_len);
int         spe_phy_frame_gen_restart         		(struct spe_phy_desc *desc);
int         spe_phy_frame_gen_done            		(struct spe_phy_desc *desc, bool *done);
int         spe_phy_frame_check_enable              (struct spe_phy_desc *desc, bool enable);
int         spe_phy_frame_check_source_select    	(struct spe_phy_desc *desc, enum spe_phy_frame_check_source source);
int         spe_phy_frame_check_read_frame_count    (struct spe_phy_desc *desc, uint32_t *count);
int         spe_phy_frame_check_read_rx_err_count	(struct spe_phy_desc *desc, uint16_t *count);
int         spe_phy_frame_check_read_error_count    (struct spe_phy_desc *desc, struct spe_phy_frame_check_error_counters *count);


/* ====================================================================================================
        ADIN1110 Module Register Address Offset Definitions
   ==================================================================================================== */
#define ADDR_PMA_PMD_CNTRL1                                      (0X010000U)    /* PMA/PMD Control 1 Register */
#define ADDR_PMA_PMD_STAT1                                       (0X010001U)    /* PMA/PMD Status 1 Register */
#define ADDR_PMA_PMD_DEVS_IN_PKG1                                (0X010005U)    /* PMA/PMD MMD Devices in Package 1 */
#define ADDR_PMA_PMD_DEVS_IN_PKG2                                (0X010006U)    /* PMA/PMD MMD Devices in Package 2 Register */
#define ADDR_PMA_PMD_CNTRL2                                      (0X010007U)    /* PMA/PMD Control 2 Register */
#define ADDR_PMA_PMD_STAT2                                       (0X010008U)    /* PMA/PMD Status 2 */
#define ADDR_PMA_PMD_TX_DIS                                      (0X010009U)    /* PMA/PMD Transmit Disable Register */
#define ADDR_PMA_PMD_EXT_ABILITY                                 (0X01000BU)    /* PMA/PMD Extended Abilities Register */
#define ADDR_PMA_PMD_BT1_ABILITY                                 (0X010012U)    /* BASE-T1 PMA/PMD Extended Ability Register */
#define ADDR_PMA_PMD_BT1_CONTROL                                 (0X010834U)    /* BASE-T1 PMA/PMD Control Register */
#define ADDR_B10L_PMA_CNTRL                                      (0X0108F6U)    /* 10BASE-T1L PMA Control Register */
#define ADDR_B10L_PMA_STAT                                       (0X0108F7U)    /* 10BASE-T1L PMA Status Register */
#define ADDR_B10L_TEST_MODE_CNTRL                                (0X0108F8U)    /* 10BASE-T1L Test Mode Control Register */
#define ADDR_CR_STBL_CHK_FOFFS_SAT_THR                           (0X018015U)    /* Frequency Offset Saturation Threshold for CR Stability Check Register */
#define ADDR_SLV_FLTR_ECHO_ACQ_CR_KP                             (0X0181E7U)    /* Slave IIR Filter Change Echo Acquisition Clock Recovery Proportional Gain Register */
#define ADDR_B10L_PMA_LINK_STAT                                  (0X018302U)    /* 10BASE-T1L PMA Link Status Register */
#define ADDR_MSE_VAL                                             (0X01830BU)    /* MSE Value Register */
#define ADDR_PCS_CNTRL1                                          (0X030000U)    /* PCS Control 1 Register */
#define ADDR_PCS_STAT1                                           (0X030001U)    /* PCS Status 1 Register */
#define ADDR_PCS_DEVS_IN_PKG1                                    (0X030005U)    /* PCS MMD Devices in Package 1 Register */
#define ADDR_PCS_DEVS_IN_PKG2                                    (0X030006U)    /* PCS MMD Devices in Package 2 Register */
#define ADDR_PCS_STAT2                                           (0X030008U)    /* PCS Status 2 Register */
#define ADDR_B10L_PCS_CNTRL                                      (0X0308E6U)    /* 10BASE-T1L PCS Control Register */
#define ADDR_B10L_PCS_STAT                                       (0X0308E7U)    /* 10BASE-T1L PCS Status Register */
#define ADDR_AN_DEVS_IN_PKG1                                     (0X070005U)    /* AUTO-_NEGOTIATION MMD Devices in Package 1 Register */
#define ADDR_AN_DEVS_IN_PKG2                                     (0X070006U)    /* AUTO-_NEGOTIATION MMD Devices in Package 2 Register */
#define ADDR_AN_CONTROL                                          (0X070200U)    /* BASE-T1 Autonegotiation Control Register */
#define ADDR_AN_STATUS                                           (0X070201U)    /* BASE-T1 Autonegotiation Status Register */
#define ADDR_AN_ADV_ABILITY_L                                    (0X070202U)    /* BASE-T1 Autonegotiation Advertisement [15:0] Register */
#define ADDR_AN_ADV_ABILITY_M                                    (0X070203U)    /* BASE-T1 Autonegotiation Advertisement [31:16] Register */
#define ADDR_AN_ADV_ABILITY_H                                    (0X070204U)    /* BASE-T1 Autonegotiation Advertisement [47:32] Register */
#define ADDR_AN_LP_ADV_ABILITY_L                                 (0X070205U)    /* BASE-T1 Autonegotiation Link Partner Base Page Ability [15:0] Register */
#define ADDR_AN_LP_ADV_ABILITY_M                                 (0X070206U)    /* BASE-T1 Autonegotiation Link Partner Base Page Ability [31:16] Register */
#define ADDR_AN_LP_ADV_ABILITY_H                                 (0X070207U)    /* BASE-T1 Autonegotiation Link Partner Base Page Ability [47:32] Register */
#define ADDR_AN_NEXT_PAGE_L                                      (0X070208U)    /* BASE-T1 Autonegotiation Next Page Transmit [15:0] Register */
#define ADDR_AN_NEXT_PAGE_M                                      (0X070209U)    /* BASE-T1 Autonegotiation Next Page Transmit [31:16] Register */
#define ADDR_AN_NEXT_PAGE_H                                      (0X07020AU)    /* BASE-T1 Autonegotiation Next Page Transmit [47:32] Register */
#define ADDR_AN_LP_NEXT_PAGE_L                                   (0X07020BU)    /* BASE-T1 Autonegotiation Link Partner Next Page Ability [15:0] Register */
#define ADDR_AN_LP_NEXT_PAGE_M                                   (0X07020CU)    /* BASE-T1 Autonegotiation Link Partner Next Page Ability [31:16] Register */
#define ADDR_AN_LP_NEXT_PAGE_H                                   (0X07020DU)    /* BASE-T1 Autonegotiation Link Partner Next Page Ability [47:32] Register */
#define ADDR_AN_B10_ADV_ABILITY                                  (0X07020EU)    /* 10BASE-T1 Autonegotiation Control Register */
#define ADDR_AN_B10_LP_ADV_ABILITY                               (0X07020FU)    /* 10BASE-T1 Autonegotiation Status Register */
#define ADDR_AN_FRC_MODE_EN                                      (0X078000U)    /* Autonegotiation Force Mode Enable Register */
#define ADDR_AN_STATUS_EXTRA                                     (0X078001U)    /* Extra Autonegotiation Status Register */
#define ADDR_AN_PHY_INST_STATUS                                  (0X078030U)    /* PHY Instantaneous Status. */
#define ADDR_MMD1_DEV_ID1                                        (0X1E0002U)    /* Vendor Specific MMD 1 Device Identifier High Register */
#define ADDR_MMD1_DEV_ID2                                        (0X1E0003U)    /* Vendor Specific MMD 1 Device Identifier Low Register */
#define ADDR_MMD1_DEVS_IN_PKG1                                   (0X1E0005U)    /* Vendor Specific 1 MMD Devices in Package Register */
#define ADDR_MMD1_DEVS_IN_PKG2                                   (0X1E0006U)    /* Vendor Specific 1 MMD Devices in Package Register */
#define ADDR_MMD1_STATUS                                         (0X1E0008U)    /* Vendor Specific MMD 1 Status Register */
#define ADDR_CRSM_IRQ_STATUS                                     (0X1E0010U)    /* System Interrupt Status Register */
#define ADDR_CRSM_IRQ_MASK                                       (0X1E0020U)    /* System Interrupt Mask Register */
#define ADDR_CRSM_SFT_RST                                        (0X1E8810U)    /* Software Reset Register */
#define ADDR_CRSM_SFT_PD_CNTRL                                   (0X1E8812U)    /* Software Power-down Control Register */
#define ADDR_CRSM_PHY_SUBSYS_RST                                 (0X1E8814U)    /* PHY Subsystem Reset Register */
#define ADDR_CRSM_MAC_IF_RST                                     (0X1E8815U)    /* PHY MAC Interface Reset Register */
#define ADDR_CRSM_STAT                                           (0X1E8818U)    /* System Status Register */
#define ADDR_CRSM_PMG_CNTRL                                      (0X1E8819U)    /* CRSM Power Management Control Register. */
#define ADDR_CRSM_DIAG_CLK_CTRL                                  (0X1E882CU)    /* CRSM Diagnostics Clock Control. */
#define ADDR_MGMT_PRT_PKG                                        (0X1E8C22U)    /* Package Configuration Values Register */
#define ADDR_MGMT_MDIO_CNTRL                                     (0X1E8C30U)    /* MDIO Control Register */
#define ADDR_DIGIO_PINMUX                                        (0X1E8C56U)    /* Pinmux Configuration 1 Register */
#define ADDR_LED0_BLINK_TIME_CNTRL                               (0X1E8C80U)    /* LED 0 ON/_OFF Blink Time Register */
#define ADDR_LED1_BLINK_TIME_CNTRL                               (0X1E8C81U)    /* LED 1 ON/_OFF Blink Time Register */
#define ADDR_LED_CNTRL                                           (0X1E8C82U)    /* LED Control Register */
#define ADDR_LED_POLARITY                                        (0X1E8C83U)    /* LED Polarity Register */
#define ADDR_MMD2_DEV_ID1                                        (0X1F0002U)    /* Vendor Specific MMD 2 Device Identifier High Register. */
#define ADDR_MMD2_DEV_ID2                                        (0X1F0003U)    /* Vendor Specific MMD 2 Device Identifier Low Register. */
#define ADDR_MMD2_DEVS_IN_PKG1                                   (0X1F0005U)    /* Vendor Specific 2 MMD Devices in Package Register */
#define ADDR_MMD2_DEVS_IN_PKG2                                   (0X1F0006U)    /* Vendor Specific 2 MMD Devices in Package Register */
#define ADDR_MMD2_STATUS                                         (0X1F0008U)    /* Vendor Specific MMD 2 Status Register */
#define ADDR_PHY_SUBSYS_IRQ_STATUS                               (0X1F0011U)    /* PHY Subsystem Interrupt Status Register */
#define ADDR_PHY_SUBSYS_IRQ_MASK                                 (0X1F0021U)    /* PHY Subsystem Interrupt Mask Register */
#define ADDR_FC_EN                                               (0X1F8001U)    /* Frame Checker Enable Register */
#define ADDR_FC_IRQ_EN                                           (0X1F8004U)    /* Frame Checker Interrupt Enable Register */
#define ADDR_FC_TX_SEL                                           (0X1F8005U)    /* Frame Checker Transmit Select Register */
#define ADDR_RX_ERR_CNT                                          (0X1F8008U)    /* Receive Error Count Register */
#define ADDR_FC_FRM_CNT_H                                        (0X1F8009U)    /* Frame Checker Count High Register */
#define ADDR_FC_FRM_CNT_L                                        (0X1F800AU)    /* Frame Checker Count Low Register */
#define ADDR_FC_LEN_ERR_CNT                                      (0X1F800BU)    /* Frame Checker Length Error Count Register */
#define ADDR_FC_ALGN_ERR_CNT                                     (0X1F800CU)    /* Frame Checker Alignment Error Count Register */
#define ADDR_FC_SYMB_ERR_CNT                                     (0X1F800DU)    /* Frame Checker Symbol Error Count Register */
#define ADDR_FC_OSZ_CNT                                          (0X1F800EU)    /* Frame Checker Oversized Frame Count Register */
#define ADDR_FC_USZ_CNT                                          (0X1F800FU)    /* Frame Checker Undersized Frame Count Register */
#define ADDR_FC_ODD_CNT                                          (0X1F8010U)    /* Frame Checker Odd Nibble Frame Count Register */
#define ADDR_FC_ODD_PRE_CNT                                      (0X1F8011U)    /* Frame Checker Odd Preamble Packet Count Register */
#define ADDR_FC_FALSE_CARRIER_CNT                                (0X1F8013U)    /* Frame Checker False Carrier Count Register */
#define ADDR_FG_EN                                               (0X1F8020U)    /* Frame Generator Enable Register */
#define ADDR_FG_CNTRL_RSTRT                                      (0X1F8021U)    /* Frame Generator CONTROL/_RESTART Register */
#define ADDR_FG_CONT_MODE_EN                                     (0X1F8022U)    /* Frame Generator Continuous Mode Enable Register */
#define ADDR_FG_IRQ_EN                                           (0X1F8023U)    /* Frame Generator Interrupt Enable Register */
#define ADDR_FG_FRM_LEN                                          (0X1F8025U)    /* Frame Generator Frame Length Register */
#define ADDR_FG_IFG_LEN                                          (0X1F8026U)    /* Frame Generator INTER-_FRAME Gap Length Register */
#define ADDR_FG_NFRM_H                                           (0X1F8027U)    /* Frame Generator Number of Frames High Register */
#define ADDR_FG_NFRM_L                                           (0X1F8028U)    /* Frame Generator Number of Frames Low Register */
#define ADDR_FG_DONE                                             (0X1F8029U)    /* Frame Generator Done Register */
#define ADDR_MAC_IF_LOOPBACK                                     (0X1F8055U)    /* MAC Interface Loopbacks Configuration Register */
#define ADDR_MAC_IF_SOP_CNTRL                                    (0X1F805AU)    /* MAC Start Of Packet (SOP) Generation Control Register */

/* ====================================================================================================
        ADIN1110 Module Register ResetValue Definitions
   ==================================================================================================== */
#define RSTVAL_PMA_PMD_CNTRL1                                      (0X0)
#define RSTVAL_PMA_PMD_STAT1                                       (0X2)
#define RSTVAL_PMA_PMD_DEVS_IN_PKG1                                (0X8B)
#define RSTVAL_PMA_PMD_DEVS_IN_PKG2                                (0XC000)
#define RSTVAL_PMA_PMD_CNTRL2                                      (0X3D)
#define RSTVAL_PMA_PMD_STAT2                                       (0X8301)
#define RSTVAL_PMA_PMD_TX_DIS                                      (0X0)
#define RSTVAL_PMA_PMD_EXT_ABILITY                                 (0X800)
#define RSTVAL_PMA_PMD_BT1_ABILITY                                 (0X4)
#define RSTVAL_PMA_PMD_BT1_CONTROL                                 (0X8002)
#define RSTVAL_B10L_PMA_CNTRL                                      (0X0)
#define RSTVAL_B10L_PMA_STAT                                       (0X2800)
#define RSTVAL_B10L_TEST_MODE_CNTRL                                (0X0)
#define RSTVAL_CR_STBL_CHK_FOFFS_SAT_THR                           (0X8)
#define RSTVAL_SLV_FLTR_ECHO_ACQ_CR_KP                             (0X400)
#define RSTVAL_B10L_PMA_LINK_STAT                                  (0X0)
#define RSTVAL_MSE_VAL                                             (0X0)
#define RSTVAL_PCS_CNTRL1                                          (0X0)
#define RSTVAL_PCS_STAT1                                           (0X2)
#define RSTVAL_PCS_DEVS_IN_PKG1                                    (0X8B)
#define RSTVAL_PCS_DEVS_IN_PKG2                                    (0XC000)
#define RSTVAL_PCS_STAT2                                           (0X8000)
#define RSTVAL_B10L_PCS_CNTRL                                      (0X0)
#define RSTVAL_B10L_PCS_STAT                                       (0X0)
#define RSTVAL_AN_DEVS_IN_PKG1                                     (0X8B)
#define RSTVAL_AN_DEVS_IN_PKG2                                     (0XC000)
#define RSTVAL_AN_CONTROL                                          (0X1000)
#define RSTVAL_AN_STATUS                                           (0X8)
#define RSTVAL_AN_ADV_ABILITY_L                                    (0X1)
#define RSTVAL_AN_ADV_ABILITY_M                                    (0X4000)
#define RSTVAL_AN_ADV_ABILITY_H                                    (0X0)
#define RSTVAL_AN_LP_ADV_ABILITY_L                                 (0X0)
#define RSTVAL_AN_LP_ADV_ABILITY_M                                 (0X0)
#define RSTVAL_AN_LP_ADV_ABILITY_H                                 (0X0)
#define RSTVAL_AN_NEXT_PAGE_L                                      (0X2001)
#define RSTVAL_AN_NEXT_PAGE_M                                      (0X0)
#define RSTVAL_AN_NEXT_PAGE_H                                      (0X0)
#define RSTVAL_AN_LP_NEXT_PAGE_L                                   (0X0)
#define RSTVAL_AN_LP_NEXT_PAGE_M                                   (0X0)
#define RSTVAL_AN_LP_NEXT_PAGE_H                                   (0X0)
#define RSTVAL_AN_B10_ADV_ABILITY                                  (0X8000)
#define RSTVAL_AN_B10_LP_ADV_ABILITY                               (0X0)
#define RSTVAL_AN_FRC_MODE_EN                                      (0X0)
#define RSTVAL_AN_STATUS_EXTRA                                     (0X0)
#define RSTVAL_AN_PHY_INST_STATUS                                  (0X10)
#define RSTVAL_MMD1_DEV_ID1                                        (0X283)
#define RSTVAL_MMD1_DEV_ID2                                        (0XBC91)
#define RSTVAL_MMD1_DEVS_IN_PKG1                                   (0X8B)
#define RSTVAL_MMD1_DEVS_IN_PKG2                                   (0XC000)
#define RSTVAL_MMD1_STATUS                                         (0X8000)
#define RSTVAL_CRSM_IRQ_STATUS                                     (0X1000)
#define RSTVAL_CRSM_IRQ_MASK                                       (0X1FFE)
#define RSTVAL_CRSM_SFT_RST                                        (0X0)
#define RSTVAL_CRSM_SFT_PD_CNTRL                                   (0X0)
#define RSTVAL_CRSM_PHY_SUBSYS_RST                                 (0X0)
#define RSTVAL_CRSM_MAC_IF_RST                                     (0X0)
#define RSTVAL_CRSM_STAT                                           (0X0)
#define RSTVAL_CRSM_PMG_CNTRL                                      (0X0)
#define RSTVAL_CRSM_DIAG_CLK_CTRL                                  (0X2)
#define RSTVAL_MGMT_PRT_PKG                                        (0X0)
#define RSTVAL_MGMT_MDIO_CNTRL                                     (0X0)
#define RSTVAL_DIGIO_PINMUX                                        (0XFE)
#define RSTVAL_LED0_BLINK_TIME_CNTRL                               (0X3636)
#define RSTVAL_LED1_BLINK_TIME_CNTRL                               (0X3636)
#define RSTVAL_LED_CNTRL                                           (0X8480)
#define RSTVAL_LED_POLARITY                                        (0X0)
#define RSTVAL_MMD2_DEV_ID1                                        (0X283)
#define RSTVAL_MMD2_DEV_ID2                                        (0XBC91)
#define RSTVAL_MMD2_DEVS_IN_PKG1                                   (0X8B)
#define RSTVAL_MMD2_DEVS_IN_PKG2                                   (0XC000)
#define RSTVAL_MMD2_STATUS                                         (0X8000)
#define RSTVAL_PHY_SUBSYS_IRQ_STATUS                               (0X0)
#define RSTVAL_PHY_SUBSYS_IRQ_MASK                                 (0X2402)
#define RSTVAL_FC_EN                                               (0X1)
#define RSTVAL_FC_IRQ_EN                                           (0X1)
#define RSTVAL_FC_TX_SEL                                           (0X0)
#define RSTVAL_RX_ERR_CNT                                          (0X0)
#define RSTVAL_FC_FRM_CNT_H                                        (0X0)
#define RSTVAL_FC_FRM_CNT_L                                        (0X0)
#define RSTVAL_FC_LEN_ERR_CNT                                      (0X0)
#define RSTVAL_FC_ALGN_ERR_CNT                                     (0X0)
#define RSTVAL_FC_SYMB_ERR_CNT                                     (0X0)
#define RSTVAL_FC_OSZ_CNT                                          (0X0)
#define RSTVAL_FC_USZ_CNT                                          (0X0)
#define RSTVAL_FC_ODD_CNT                                          (0X0)
#define RSTVAL_FC_ODD_PRE_CNT                                      (0X0)
#define RSTVAL_FC_FALSE_CARRIER_CNT                                (0X0)
#define RSTVAL_FG_EN                                               (0X0)
#define RSTVAL_FG_CNTRL_RSTRT                                      (0X1)
#define RSTVAL_FG_CONT_MODE_EN                                     (0X0)
#define RSTVAL_FG_IRQ_EN                                           (0X0)
#define RSTVAL_FG_FRM_LEN                                          (0X6B)
#define RSTVAL_FG_IFG_LEN                                          (0XC)
#define RSTVAL_FG_NFRM_H                                           (0X0)
#define RSTVAL_FG_NFRM_L                                           (0X100)
#define RSTVAL_FG_DONE                                             (0X0)
#define RSTVAL_MAC_IF_LOOPBACK                                     (0XA)
#define RSTVAL_MAC_IF_SOP_CNTRL                                    (0X1B)

/* ====================================================================================================
        ADIN1110 Module Register BitPositions, Lengths, Masks and Enumerations Definitions
   ==================================================================================================== */

/* ----------------------------------------------------------------------------------------------------
          PMA_PMD_CNTRL1                                        Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_PMA_PMD_CNTRL1_LB_PMA_LOC_EN                     (0U)           /* Enables PMA Local Loopback. */
#define BITL_PMA_PMD_CNTRL1_LB_PMA_LOC_EN                     (1U)           /* Enables PMA Local Loopback. */
#define BITM_PMA_PMD_CNTRL1_LB_PMA_LOC_EN                     (0X0001U)      /* Enables PMA Local Loopback. */
#define BITP_PMA_PMD_CNTRL1_PMA_SFT_PD                        (11U)          /* PMA Software Power-down. */
#define BITL_PMA_PMD_CNTRL1_PMA_SFT_PD                        (1U)           /* PMA Software Power-down. */
#define BITM_PMA_PMD_CNTRL1_PMA_SFT_PD                        (0X0800U)      /* PMA Software Power-down. */
#define BITP_PMA_PMD_CNTRL1_PMA_SFT_RST                       (15U)          /* PMA Software Reset. */
#define BITL_PMA_PMD_CNTRL1_PMA_SFT_RST                       (1U)           /* PMA Software Reset. */
#define BITM_PMA_PMD_CNTRL1_PMA_SFT_RST                       (0X8000U)      /* PMA Software Reset. */

/* ----------------------------------------------------------------------------------------------------
          PMA_PMD_STAT1                                         Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_PMA_PMD_STAT1_PMA_SFT_PD_ABLE                    (1U)           /* PMA Software Powerdown Able */
#define BITL_PMA_PMD_STAT1_PMA_SFT_PD_ABLE                    (1U)           /* PMA Software Powerdown Able */
#define BITM_PMA_PMD_STAT1_PMA_SFT_PD_ABLE                    (0X0002U)      /* PMA Software Powerdown Able */
#define BITP_PMA_PMD_STAT1_PMA_LINK_STAT_OK_LL                (2U)           /* PMA Link Status. */
#define BITL_PMA_PMD_STAT1_PMA_LINK_STAT_OK_LL                (1U)           /* PMA Link Status. */
#define BITM_PMA_PMD_STAT1_PMA_LINK_STAT_OK_LL                (0X0004U)      /* PMA Link Status. */

/* ----------------------------------------------------------------------------------------------------
          PMA_PMD_DEVS_IN_PKG1                                  Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_PMA_PMD_DEVS_IN_PKG1_PMA_PMD_DEVS_IN_PKG1        (0U)           /* PMA/PMD MMD Devices in Package */
#define BITL_PMA_PMD_DEVS_IN_PKG1_PMA_PMD_DEVS_IN_PKG1        (16U)          /* PMA/PMD MMD Devices in Package */
#define BITM_PMA_PMD_DEVS_IN_PKG1_PMA_PMD_DEVS_IN_PKG1        (0XFFFFU)      /* PMA/PMD MMD Devices in Package */

/* ----------------------------------------------------------------------------------------------------
          PMA_PMD_DEVS_IN_PKG2                                  Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_PMA_PMD_DEVS_IN_PKG2_PMA_PMD_DEVS_IN_PKG2        (0U)           /* PMA/PMD MMD Devices in Package */
#define BITL_PMA_PMD_DEVS_IN_PKG2_PMA_PMD_DEVS_IN_PKG2        (16U)          /* PMA/PMD MMD Devices in Package */
#define BITM_PMA_PMD_DEVS_IN_PKG2_PMA_PMD_DEVS_IN_PKG2        (0XFFFFU)      /* PMA/PMD MMD Devices in Package */

/* ----------------------------------------------------------------------------------------------------
          PMA_PMD_CNTRL2                                        Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL                  (0U)           /* PMA/PMD Type Selection */
#define BITL_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL                  (7U)           /* PMA/PMD Type Selection */
#define BITM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL                  (0X007FU)      /* PMA/PMD Type Selection */

#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_CX4_PMA_PMD (0X0000U)      /* TS_10GBASE_CX4_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_EW_PMA_PMD (0X0001U)      /* TS_10GBASE_EW_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_LW_PMA_PMD (0X0002U)      /* TS_10GBASE_LW_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_SW_PMA_PMD (0X0003U)      /* TS_10GBASE_SW_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_LX4_PMA_PMD (0X0004U)      /* TS_10GBASE_LX4_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_ER_PMA_PMD (0X0005U)      /* TS_10GBASE_ER_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_LR_PMA_PMD (0X0006U)      /* TS_10GBASE_LR_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_SR_PMA_PMD (0X0007U)      /* TS_10GBASE_SR_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_LRM_PMA_PMD (0X0008U)      /* TS_10GBASE_LRM_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_T_PMA (0X0009U)      /* TS_10GBASE_T_PMA */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_KX4_PMA_PMD (0X000AU)      /* TS_10GBASE_KX4_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_KR_PMA_PMD (0X000BU)      /* TS_10GBASE_KR_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_1000BASE_T_PMA_PMD (0X000CU)      /* TS_1000BASE_T_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_1000BASE_KX_PMA_PMD (0X000DU)      /* TS_1000BASE_KX_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_100BASE_TX_PMA_PMD (0X000EU)      /* TS_100BASE_TX_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10BASE_T_PMA_PMD (0X000FU)      /* TS_10BASE_T_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10_1GBASE_PRX_D1 (0X0010U)      /* TS_10_1GBASE_PRX_D1 */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10_1GBASE_PRX_D2 (0X0011U)      /* TS_10_1GBASE_PRX_D2 */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10_1GBASE_PRX_D3 (0X0012U)      /* TS_10_1GBASE_PRX_D3 */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_PR_D1 (0X0013U)      /* TS_10GBASE_PR_D1 */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_PR_D2 (0X0014U)      /* TS_10GBASE_PR_D2 */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_PR_D3 (0X0015U)      /* TS_10GBASE_PR_D3 */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10_1GBASE_PRX_U1 (0X0016U)      /* TS_10_1GBASE_PRX_U1 */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10_1GBASE_PRX_U2 (0X0017U)      /* TS_10_1GBASE_PRX_U2 */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10_1GBASE_PRX_U3 (0X0018U)      /* TS_10_1GBASE_PRX_U3 */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_PR_U1 (0X0019U)      /* TS_10GBASE_PR_U1 */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_PR_U3 (0X001AU)      /* TS_10GBASE_PR_U3 */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_RESERVED      (0X001BU)      /* TS_RESERVED */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_PR_D4 (0X001CU)      /* TS_10GBASE_PR_D4 */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10_1GBASE_PRX_D4 (0X001DU)      /* TS_10_1GBASE_PRX_D4 */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_PR_U4 (0X001EU)      /* TS_10GBASE_PR_U4 */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10_1GBASE_PRX_U4 (0X001FU)      /* TS_10_1GBASE_PRX_U4 */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_40GBASE_KR4_PMA_PMD (0X0020U)      /* TS_40GBASE_KR4_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_40GBASE_CR4_PMA_PMD (0X0021U)      /* TS_40GBASE_CR4_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_40GBASE_SR4_PMA_PMD (0X0022U)      /* TS_40GBASE_SR4_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_40GBASE_LR4_PMA_PMD (0X0023U)      /* TS_40GBASE_LR4_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_40GBASE_FR_PMA_PMD (0X0024U)      /* TS_40GBASE_FR_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_40GBASE_ER4_PMA_PMD (0X0025U)      /* TS_40GBASE_ER4_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_40GBASE_T_PMA (0X0026U)      /* TS_40GBASE_T_PMA */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_100GBASE_CR10_PMA_PMD (0X0028U)      /* TS_100GBASE_CR10_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_100GBASE_SR10_PMA_PMD (0X0029U)      /* TS_100GBASE_SR10_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_100GBASE_LR4_PMA_PMD (0X002AU)      /* TS_100GBASE_LR4_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_100GBASE_ER4_PMA_PMD (0X002BU)      /* TS_100GBASE_ER4_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_100GBASE_KP4_PMA_PMD (0X002CU)      /* TS_100GBASE_KP4_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_100GBASE_KR4_PMA_PMD (0X002DU)      /* TS_100GBASE_KR4_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_100GBASE_CR4_PMA_PMD (0X002EU)      /* TS_100GBASE_CR4_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_100GBASE_SR4_PMA_PMD (0X002FU)      /* TS_100GBASE_SR4_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_2_5GBASE_T_PMA (0X0030U)      /* TS_2_5GBASE_T_PMA */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_5GBASE_T_PMA  (0X0031U)      /* TS_5GBASE_T_PMA */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GPASS_XR_D_PMA_PMD (0X0032U)      /* TS_10GPASS_XR_D_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GPASS_XR_U_PMA_PMD (0X0033U)      /* TS_10GPASS_XR_U_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_BASE_H_PMA_PMD (0X0034U)      /* TS_BASE_H_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_25GBASE_LR_PMA_PMD (0X0035U)      /* TS_25GBASE_LR_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_25GBASE_ER_PMA_PMD (0X0036U)      /* TS_25GBASE_ER_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_25GBASE_T_PMA (0X0037U)      /* TS_25GBASE_T_PMA */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_25GBASE_CR_OR_25GBASE_CR_S_PMA_PMD (0X0038U)      /* TS_25GBASE_CR_OR_25GBASE_CR_S_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_25GBASE_KR_OR_25GBASE_KR_S_PMA_PMD (0X0039U)      /* TS_25GBASE_KR_OR_25GBASE_KR_S_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_25GBASE_SR_PMA_PMD (0X003AU)      /* TS_25GBASE_SR_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_BASE_T1_PMA_PMD (0X003DU)      /* TS_BASE_T1_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_200GBASE_DR4_PMA_PMD (0X0053U)      /* TS_200GBASE_DR4_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_200GBASE_FR4_PMA_PMD (0X0054U)      /* TS_200GBASE_FR4_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_200GBASE_LR4_PMA_PMD (0X0055U)      /* TS_200GBASE_LR4_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_400GBASE_SR16_PMA_PMD (0X0059U)      /* TS_400GBASE_SR16_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_400GBASE_DR4_PMA_PMD (0X005AU)      /* TS_400GBASE_DR4_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_400GBASE_FR8_PMA_PMD (0X005BU)      /* TS_400GBASE_FR8_PMA_PMD */
#define ENUM_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_400GBASE_LR8_PMA_PMD (0X005CU)      /* TS_400GBASE_LR8_PMA_PMD */

/* ----------------------------------------------------------------------------------------------------
          PMA_PMD_STAT2                                         Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_PMA_PMD_STAT2_LB_PMA_LOC_ABLE                    (0U)           /* PMA Local Loopback Able */
#define BITL_PMA_PMD_STAT2_LB_PMA_LOC_ABLE                    (1U)           /* PMA Local Loopback Able */
#define BITM_PMA_PMD_STAT2_LB_PMA_LOC_ABLE                    (0X0001U)      /* PMA Local Loopback Able */
#define BITP_PMA_PMD_STAT2_PMA_PMD_TX_DIS_ABLE                (8U)           /* PMA/PMD Tx Disable */
#define BITL_PMA_PMD_STAT2_PMA_PMD_TX_DIS_ABLE                (1U)           /* PMA/PMD Tx Disable */
#define BITM_PMA_PMD_STAT2_PMA_PMD_TX_DIS_ABLE                (0X0100U)      /* PMA/PMD Tx Disable */
#define BITP_PMA_PMD_STAT2_PMA_PMD_EXT_ABLE                   (9U)           /* PHY Extended Abilities Support */
#define BITL_PMA_PMD_STAT2_PMA_PMD_EXT_ABLE                   (1U)           /* PHY Extended Abilities Support */
#define BITM_PMA_PMD_STAT2_PMA_PMD_EXT_ABLE                   (0X0200U)      /* PHY Extended Abilities Support */
#define BITP_PMA_PMD_STAT2_PMA_PMD_PRESENT                    (14U)          /* PMA/PMD Present */
#define BITL_PMA_PMD_STAT2_PMA_PMD_PRESENT                    (2U)           /* PMA/PMD Present */
#define BITM_PMA_PMD_STAT2_PMA_PMD_PRESENT                    (0XC000U)      /* PMA/PMD Present */

/* ----------------------------------------------------------------------------------------------------
          PMA_PMD_TX_DIS                                        Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_PMA_PMD_TX_DIS_PMA_TX_DIS                        (0U)           /* PMD Transmit Disable. */
#define BITL_PMA_PMD_TX_DIS_PMA_TX_DIS                        (1U)           /* PMD Transmit Disable. */
#define BITM_PMA_PMD_TX_DIS_PMA_TX_DIS                        (0X0001U)      /* PMD Transmit Disable. */

/* ----------------------------------------------------------------------------------------------------
          PMA_PMD_EXT_ABILITY                                   Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_PMA_PMD_EXT_ABILITY_PMA_PMD_BT1_ABLE             (11U)          /* PHY Supports BASE-T1 */
#define BITL_PMA_PMD_EXT_ABILITY_PMA_PMD_BT1_ABLE             (1U)           /* PHY Supports BASE-T1 */
#define BITM_PMA_PMD_EXT_ABILITY_PMA_PMD_BT1_ABLE             (0X0800U)      /* PHY Supports BASE-T1 */

/* ----------------------------------------------------------------------------------------------------
          PMA_PMD_BT1_ABILITY                                   Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_PMA_PMD_BT1_ABILITY_B100_ABILITY                 (0U)           /* 100BASE-T1 Ability. */
#define BITL_PMA_PMD_BT1_ABILITY_B100_ABILITY                 (1U)           /* 100BASE-T1 Ability. */
#define BITM_PMA_PMD_BT1_ABILITY_B100_ABILITY                 (0X0001U)      /* 100BASE-T1 Ability. */
#define BITP_PMA_PMD_BT1_ABILITY_B1000_ABILITY                (1U)           /* 1000BASE-T1 Ability. */
#define BITL_PMA_PMD_BT1_ABILITY_B1000_ABILITY                (1U)           /* 1000BASE-T1 Ability. */
#define BITM_PMA_PMD_BT1_ABILITY_B1000_ABILITY                (0X0002U)      /* 1000BASE-T1 Ability. */
#define BITP_PMA_PMD_BT1_ABILITY_B10L_ABILITY                 (2U)           /* 10BASE-T1L Ability. */
#define BITL_PMA_PMD_BT1_ABILITY_B10L_ABILITY                 (1U)           /* 10BASE-T1L Ability. */
#define BITM_PMA_PMD_BT1_ABILITY_B10L_ABILITY                 (0X0004U)      /* 10BASE-T1L Ability. */
#define BITP_PMA_PMD_BT1_ABILITY_B10S_ABILITY                 (3U)           /* 10BASE-T1S Ability. */
#define BITL_PMA_PMD_BT1_ABILITY_B10S_ABILITY                 (1U)           /* 10BASE-T1S Ability. */
#define BITM_PMA_PMD_BT1_ABILITY_B10S_ABILITY                 (0X0008U)      /* 10BASE-T1S Ability. */

/* ----------------------------------------------------------------------------------------------------
          PMA_PMD_BT1_CONTROL                                   Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_PMA_PMD_BT1_CONTROL_BT1_TYPE_SEL                 (0U)           /* BASE-T1 Type Selection */
#define BITL_PMA_PMD_BT1_CONTROL_BT1_TYPE_SEL                 (4U)           /* BASE-T1 Type Selection */
#define BITM_PMA_PMD_BT1_CONTROL_BT1_TYPE_SEL                 (0X000FU)      /* BASE-T1 Type Selection */
#define BITP_PMA_PMD_BT1_CONTROL_CFG_MST                      (14U)          /* Master-slave Config. */
#define BITL_PMA_PMD_BT1_CONTROL_CFG_MST                      (1U)           /* Master-slave Config. */
#define BITM_PMA_PMD_BT1_CONTROL_CFG_MST                      (0X4000U)      /* Master-slave Config. */

#define ENUM_PMA_PMD_BT1_CONTROL_BT1_TYPE_SEL_TS_10BASE_T1L   (0X0002U)      /* 10BASE-T1L */

/* ----------------------------------------------------------------------------------------------------
          B10L_PMA_CNTRL                                        Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_B10L_PMA_CNTRL_B10L_LB_PMA_LOC_EN                (0U)           /* 10BASE-T1L PMA Loopback. */
#define BITL_B10L_PMA_CNTRL_B10L_LB_PMA_LOC_EN                (1U)           /* 10BASE-T1L PMA Loopback. */
#define BITM_B10L_PMA_CNTRL_B10L_LB_PMA_LOC_EN                (0X0001U)      /* 10BASE-T1L PMA Loopback. */
#define BITP_B10L_PMA_CNTRL_B10L_EEE                          (10U)          /* 10BASE-T1L EEE Enable. */
#define BITL_B10L_PMA_CNTRL_B10L_EEE                          (1U)           /* 10BASE-T1L EEE Enable. */
#define BITM_B10L_PMA_CNTRL_B10L_EEE                          (0X0400U)      /* 10BASE-T1L EEE Enable. */
#define BITP_B10L_PMA_CNTRL_B10L_TX_LVL_HI                    (12U)          /* 10BASE-T1L Transmit Voltage Amplitude Control. */
#define BITL_B10L_PMA_CNTRL_B10L_TX_LVL_HI                    (1U)           /* 10BASE-T1L Transmit Voltage Amplitude Control. */
#define BITM_B10L_PMA_CNTRL_B10L_TX_LVL_HI                    (0X1000U)      /* 10BASE-T1L Transmit Voltage Amplitude Control. */
#define BITP_B10L_PMA_CNTRL_B10L_TX_DIS_MODE_EN               (14U)          /* 10BASE-T1L Transmit Disable Mode. */
#define BITL_B10L_PMA_CNTRL_B10L_TX_DIS_MODE_EN               (1U)           /* 10BASE-T1L Transmit Disable Mode. */
#define BITM_B10L_PMA_CNTRL_B10L_TX_DIS_MODE_EN               (0X4000U)      /* 10BASE-T1L Transmit Disable Mode. */

/* ----------------------------------------------------------------------------------------------------
          B10L_PMA_STAT                                         Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_B10L_PMA_STAT_B10L_EEE_ABLE                      (10U)          /* 10BASE-T1L EEE Ability */
#define BITL_B10L_PMA_STAT_B10L_EEE_ABLE                      (1U)           /* 10BASE-T1L EEE Ability */
#define BITM_B10L_PMA_STAT_B10L_EEE_ABLE                      (0X0400U)      /* 10BASE-T1L EEE Ability */
#define BITP_B10L_PMA_STAT_B10L_PMA_SFT_PD_ABLE               (11U)          /* PMA Supports Powerdown */
#define BITL_B10L_PMA_STAT_B10L_PMA_SFT_PD_ABLE               (1U)           /* PMA Supports Powerdown */
#define BITM_B10L_PMA_STAT_B10L_PMA_SFT_PD_ABLE               (0X0800U)      /* PMA Supports Powerdown */
#define BITP_B10L_PMA_STAT_B10L_TX_LVL_HI_ABLE                (12U)          /* 10BASE-T1L High Voltage Tx Ability */
#define BITL_B10L_PMA_STAT_B10L_TX_LVL_HI_ABLE                (1U)           /* 10BASE-T1L High Voltage Tx Ability */
#define BITM_B10L_PMA_STAT_B10L_TX_LVL_HI_ABLE                (0X1000U)      /* 10BASE-T1L High Voltage Tx Ability */
#define BITP_B10L_PMA_STAT_B10L_LB_PMA_LOC_ABLE               (13U)          /* 10BASE-T1L PMA Loopback Ability */
#define BITL_B10L_PMA_STAT_B10L_LB_PMA_LOC_ABLE               (1U)           /* 10BASE-T1L PMA Loopback Ability */
#define BITM_B10L_PMA_STAT_B10L_LB_PMA_LOC_ABLE               (0X2000U)      /* 10BASE-T1L PMA Loopback Ability */

/* ----------------------------------------------------------------------------------------------------
          B10L_TEST_MODE_CNTRL                                  Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_B10L_TEST_MODE_CNTRL_B10L_TX_TEST_MODE           (13U)          /* 10BASE-T1L Transmitter Test Mode. */
#define BITL_B10L_TEST_MODE_CNTRL_B10L_TX_TEST_MODE           (3U)           /* 10BASE-T1L Transmitter Test Mode. */
#define BITM_B10L_TEST_MODE_CNTRL_B10L_TX_TEST_MODE           (0XE000U)      /* 10BASE-T1L Transmitter Test Mode. */

#define ENUM_B10L_TEST_MODE_CNTRL_B10L_TX_TEST_MODE_IEEE_TX_TM_NONE (0X0000U)      /* Normal operation. */
#define ENUM_B10L_TEST_MODE_CNTRL_B10L_TX_TEST_MODE_IEEE_TX_TM_JITTER (0X0001U)      /* Test mode 1 - Transmitter output voltage and timing jitter test mode.  When test mode 1 is enabled, the PHY shall repeatedly transmit the data symbol sequence (+1, -1). */
#define ENUM_B10L_TEST_MODE_CNTRL_B10L_TX_TEST_MODE_IEEE_TX_TM_DROOP (0X0002U)      /* Test mode 2 - Transmitter output droop test mode.  When test mode 2 is enabled, the PHY shall transmit ten "+1" symbols followed by ten "-1" symbols. */
#define ENUM_B10L_TEST_MODE_CNTRL_B10L_TX_TEST_MODE_IEEE_TX_TM_IDLE (0X0003U)      /* Test mode 3 - Normal operation in Idle mode.  When test mode 3 is enabled, the PHY shall transmit as in non-test operation and in the MASTER data mode with data set to normal Inter-Frame idle signals. */

/* ----------------------------------------------------------------------------------------------------
          CR_STBL_CHK_FOFFS_SAT_THR                             Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_CR_STBL_CHK_FOFFS_SAT_THR_CR_STBL_CHK_FOFFS_SAT_THR (0U)           /* Frequency Offset Saturation Threshold for CR Stability Check. */
#define BITL_CR_STBL_CHK_FOFFS_SAT_THR_CR_STBL_CHK_FOFFS_SAT_THR (11U)          /* Frequency Offset Saturation Threshold for CR Stability Check. */
#define BITM_CR_STBL_CHK_FOFFS_SAT_THR_CR_STBL_CHK_FOFFS_SAT_THR (0X07FFU)      /* Frequency Offset Saturation Threshold for CR Stability Check. */

/* ----------------------------------------------------------------------------------------------------
          SLV_FLTR_ECHO_ACQ_CR_KP                               Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_SLV_FLTR_ECHO_ACQ_CR_KP_SLV_FLTR_ECHO_ACQ_CR_KP  (0U)           /* Slave IIR Filter Change Echo Acquisition Clock Recovery Proportional Gain. */
#define BITL_SLV_FLTR_ECHO_ACQ_CR_KP_SLV_FLTR_ECHO_ACQ_CR_KP  (16U)          /* Slave IIR Filter Change Echo Acquisition Clock Recovery Proportional Gain. */
#define BITM_SLV_FLTR_ECHO_ACQ_CR_KP_SLV_FLTR_ECHO_ACQ_CR_KP  (0XFFFFU)      /* Slave IIR Filter Change Echo Acquisition Clock Recovery Proportional Gain. */

/* ----------------------------------------------------------------------------------------------------
          B10L_PMA_LINK_STAT                                    Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_B10L_PMA_LINK_STAT_B10L_LINK_STAT_OK             (0U)           /* Link Status OK */
#define BITL_B10L_PMA_LINK_STAT_B10L_LINK_STAT_OK             (1U)           /* Link Status OK */
#define BITM_B10L_PMA_LINK_STAT_B10L_LINK_STAT_OK             (0X0001U)      /* Link Status OK */
#define BITP_B10L_PMA_LINK_STAT_B10L_LINK_STAT_OK_LL          (1U)           /* Link Status Ok Latch Low */
#define BITL_B10L_PMA_LINK_STAT_B10L_LINK_STAT_OK_LL          (1U)           /* Link Status Ok Latch Low */
#define BITM_B10L_PMA_LINK_STAT_B10L_LINK_STAT_OK_LL          (0X0002U)      /* Link Status Ok Latch Low */
#define BITP_B10L_PMA_LINK_STAT_B10L_DSCR_STAT_OK             (4U)           /* 10BASE-T1L Descrambler Status Ok */
#define BITL_B10L_PMA_LINK_STAT_B10L_DSCR_STAT_OK             (1U)           /* 10BASE-T1L Descrambler Status Ok */
#define BITM_B10L_PMA_LINK_STAT_B10L_DSCR_STAT_OK             (0X0010U)      /* 10BASE-T1L Descrambler Status Ok */
#define BITP_B10L_PMA_LINK_STAT_B10L_DSCR_STAT_OK_LL          (5U)           /* BASE-T1L Descrambler Status Ok Latch Low */
#define BITL_B10L_PMA_LINK_STAT_B10L_DSCR_STAT_OK_LL          (1U)           /* BASE-T1L Descrambler Status Ok Latch Low */
#define BITM_B10L_PMA_LINK_STAT_B10L_DSCR_STAT_OK_LL          (0X0020U)      /* BASE-T1L Descrambler Status Ok Latch Low */
#define BITP_B10L_PMA_LINK_STAT_B10L_LOC_RCVR_STAT_OK         (6U)           /* 10BASE-T1L Local Receiver Status Ok */
#define BITL_B10L_PMA_LINK_STAT_B10L_LOC_RCVR_STAT_OK         (1U)           /* 10BASE-T1L Local Receiver Status Ok */
#define BITM_B10L_PMA_LINK_STAT_B10L_LOC_RCVR_STAT_OK         (0X0040U)      /* 10BASE-T1L Local Receiver Status Ok */
#define BITP_B10L_PMA_LINK_STAT_B10L_LOC_RCVR_STAT_OK_LL      (7U)           /* 10BASE-T1L Local Receiver Status Ok Latch Low */
#define BITL_B10L_PMA_LINK_STAT_B10L_LOC_RCVR_STAT_OK_LL      (1U)           /* 10BASE-T1L Local Receiver Status Ok Latch Low */
#define BITM_B10L_PMA_LINK_STAT_B10L_LOC_RCVR_STAT_OK_LL      (0X0080U)      /* 10BASE-T1L Local Receiver Status Ok Latch Low */
#define BITP_B10L_PMA_LINK_STAT_B10L_REM_RCVR_STAT_OK         (8U)           /* 10BASE-T1L Remote Receiver Status Ok */
#define BITL_B10L_PMA_LINK_STAT_B10L_REM_RCVR_STAT_OK         (1U)           /* 10BASE-T1L Remote Receiver Status Ok */
#define BITM_B10L_PMA_LINK_STAT_B10L_REM_RCVR_STAT_OK         (0X0100U)      /* 10BASE-T1L Remote Receiver Status Ok */
#define BITP_B10L_PMA_LINK_STAT_B10L_REM_RCVR_STAT_OK_LL      (9U)           /* 10BASE-T1L Remote Receiver Status Ok Latch Low */
#define BITL_B10L_PMA_LINK_STAT_B10L_REM_RCVR_STAT_OK_LL      (1U)           /* 10BASE-T1L Remote Receiver Status Ok Latch Low */
#define BITM_B10L_PMA_LINK_STAT_B10L_REM_RCVR_STAT_OK_LL      (0X0200U)      /* 10BASE-T1L Remote Receiver Status Ok Latch Low */

/* ----------------------------------------------------------------------------------------------------
          MSE_VAL                                               Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MSE_VAL_MSE_VAL                                  (0U)           /* MSE Value */
#define BITL_MSE_VAL_MSE_VAL                                  (16U)          /* MSE Value */
#define BITM_MSE_VAL_MSE_VAL                                  (0XFFFFU)      /* MSE Value */

/* ----------------------------------------------------------------------------------------------------
          PCS_CNTRL1                                            Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_PCS_CNTRL1_PCS_SFT_PD                            (11U)          /* PCS Software Power-down. */
#define BITL_PCS_CNTRL1_PCS_SFT_PD                            (1U)           /* PCS Software Power-down. */
#define BITM_PCS_CNTRL1_PCS_SFT_PD                            (0X0800U)      /* PCS Software Power-down. */
#define BITP_PCS_CNTRL1_LB_PCS_EN                             (14U)          /* PCS Loopback Enable */
#define BITL_PCS_CNTRL1_LB_PCS_EN                             (1U)           /* PCS Loopback Enable */
#define BITM_PCS_CNTRL1_LB_PCS_EN                             (0X4000U)      /* PCS Loopback Enable */
#define BITP_PCS_CNTRL1_PCS_SFT_RST                           (15U)          /* PCS Software Reset. */
#define BITL_PCS_CNTRL1_PCS_SFT_RST                           (1U)           /* PCS Software Reset. */
#define BITM_PCS_CNTRL1_PCS_SFT_RST                           (0X8000U)      /* PCS Software Reset. */

/* ----------------------------------------------------------------------------------------------------
          PCS_STAT1                                             Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_PCS_STAT1_PCS_SFT_PD_ABLE                        (1U)           /* PCS Software Powerdown Able */
#define BITL_PCS_STAT1_PCS_SFT_PD_ABLE                        (1U)           /* PCS Software Powerdown Able */
#define BITM_PCS_STAT1_PCS_SFT_PD_ABLE                        (0X0002U)      /* PCS Software Powerdown Able */

/* ----------------------------------------------------------------------------------------------------
          PCS_DEVS_IN_PKG1                                      Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_PCS_DEVS_IN_PKG1_PCS_DEVS_IN_PKG1                (0U)           /* PCS MMD Devices in Package */
#define BITL_PCS_DEVS_IN_PKG1_PCS_DEVS_IN_PKG1                (16U)          /* PCS MMD Devices in Package */
#define BITM_PCS_DEVS_IN_PKG1_PCS_DEVS_IN_PKG1                (0XFFFFU)      /* PCS MMD Devices in Package */

/* ----------------------------------------------------------------------------------------------------
          PCS_DEVS_IN_PKG2                                      Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_PCS_DEVS_IN_PKG2_PCS_DEVS_IN_PKG2                (0U)           /* PCS MMD Devices in Package */
#define BITL_PCS_DEVS_IN_PKG2_PCS_DEVS_IN_PKG2                (16U)          /* PCS MMD Devices in Package */
#define BITM_PCS_DEVS_IN_PKG2_PCS_DEVS_IN_PKG2                (0XFFFFU)      /* PCS MMD Devices in Package */

/* ----------------------------------------------------------------------------------------------------
          PCS_STAT2                                             Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_PCS_STAT2_PCS_PRESENT                            (14U)          /* PCS Present */
#define BITL_PCS_STAT2_PCS_PRESENT                            (2U)           /* PCS Present */
#define BITM_PCS_STAT2_PCS_PRESENT                            (0XC000U)      /* PCS Present */

/* ----------------------------------------------------------------------------------------------------
          B10L_PCS_CNTRL                                        Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_B10L_PCS_CNTRL_B10L_LB_PCS_EN                    (14U)          /* PCS Loopback Enable */
#define BITL_B10L_PCS_CNTRL_B10L_LB_PCS_EN                    (1U)           /* PCS Loopback Enable */
#define BITM_B10L_PCS_CNTRL_B10L_LB_PCS_EN                    (0X4000U)      /* PCS Loopback Enable */

/* ----------------------------------------------------------------------------------------------------
          B10L_PCS_STAT                                         Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_B10L_PCS_STAT_B10L_PCS_DSCR_STAT_OK_LL           (2U)           /* PCS Descrambler Status. */
#define BITL_B10L_PCS_STAT_B10L_PCS_DSCR_STAT_OK_LL           (1U)           /* PCS Descrambler Status. */
#define BITM_B10L_PCS_STAT_B10L_PCS_DSCR_STAT_OK_LL           (0X0004U)      /* PCS Descrambler Status. */

/* ----------------------------------------------------------------------------------------------------
          AN_DEVS_IN_PKG1                                       Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_AN_DEVS_IN_PKG1_AN_DEVS_IN_PKG1                  (0U)           /* Autonegotiation MMD Devices in Package */
#define BITL_AN_DEVS_IN_PKG1_AN_DEVS_IN_PKG1                  (16U)          /* Autonegotiation MMD Devices in Package */
#define BITM_AN_DEVS_IN_PKG1_AN_DEVS_IN_PKG1                  (0XFFFFU)      /* Autonegotiation MMD Devices in Package */

/* ----------------------------------------------------------------------------------------------------
          AN_DEVS_IN_PKG2                                       Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_AN_DEVS_IN_PKG2_AN_DEVS_IN_PKG2                  (0U)           /* Autonegotiation MMD Devices in Package */
#define BITL_AN_DEVS_IN_PKG2_AN_DEVS_IN_PKG2                  (16U)          /* Autonegotiation MMD Devices in Package */
#define BITM_AN_DEVS_IN_PKG2_AN_DEVS_IN_PKG2                  (0XFFFFU)      /* Autonegotiation MMD Devices in Package */

/* ----------------------------------------------------------------------------------------------------
          AN_CONTROL                                            Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_AN_CONTROL_AN_RESTART                            (9U)           /* Autonegotiation Restart */
#define BITL_AN_CONTROL_AN_RESTART                            (1U)           /* Autonegotiation Restart */
#define BITM_AN_CONTROL_AN_RESTART                            (0X0200U)      /* Autonegotiation Restart */
#define BITP_AN_CONTROL_AN_EN                                 (12U)          /* Autonegotiation Enable. */
#define BITL_AN_CONTROL_AN_EN                                 (1U)           /* Autonegotiation Enable. */
#define BITM_AN_CONTROL_AN_EN                                 (0X1000U)      /* Autonegotiation Enable. */

/* ----------------------------------------------------------------------------------------------------
          AN_STATUS                                             Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_AN_STATUS_AN_LINK_STATUS                         (2U)           /* Link Status. */
#define BITL_AN_STATUS_AN_LINK_STATUS                         (1U)           /* Link Status. */
#define BITM_AN_STATUS_AN_LINK_STATUS                         (0X0004U)      /* Link Status. */
#define BITP_AN_STATUS_AN_ABLE                                (3U)           /* Autonegotiation Ability. */
#define BITL_AN_STATUS_AN_ABLE                                (1U)           /* Autonegotiation Ability. */
#define BITM_AN_STATUS_AN_ABLE                                (0X0008U)      /* Autonegotiation Ability. */
#define BITP_AN_STATUS_AN_REMOTE_FAULT                        (4U)           /* Autonegotiation Remote Fault */
#define BITL_AN_STATUS_AN_REMOTE_FAULT                        (1U)           /* Autonegotiation Remote Fault */
#define BITM_AN_STATUS_AN_REMOTE_FAULT                        (0X0010U)      /* Autonegotiation Remote Fault */
#define BITP_AN_STATUS_AN_COMPLETE                            (5U)           /* Autonegotiation Complete. */
#define BITL_AN_STATUS_AN_COMPLETE                            (1U)           /* Autonegotiation Complete. */
#define BITM_AN_STATUS_AN_COMPLETE                            (0X0020U)      /* Autonegotiation Complete. */
#define BITP_AN_STATUS_AN_PAGE_RX                             (6U)           /* Page Received */
#define BITL_AN_STATUS_AN_PAGE_RX                             (1U)           /* Page Received */
#define BITM_AN_STATUS_AN_PAGE_RX                             (0X0040U)      /* Page Received */

/* ----------------------------------------------------------------------------------------------------
          AN_ADV_ABILITY_L                                      Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_AN_ADV_ABILITY_L_AN_ADV_SELECTOR                 (0U)           /* Selector. */
#define BITL_AN_ADV_ABILITY_L_AN_ADV_SELECTOR                 (5U)           /* Selector. */
#define BITM_AN_ADV_ABILITY_L_AN_ADV_SELECTOR                 (0X001FU)      /* Selector. */
#define BITP_AN_ADV_ABILITY_L_AN_ADV_PAUSE                    (10U)          /* Pause Ability. */
#define BITL_AN_ADV_ABILITY_L_AN_ADV_PAUSE                    (2U)           /* Pause Ability. */
#define BITM_AN_ADV_ABILITY_L_AN_ADV_PAUSE                    (0X0C00U)      /* Pause Ability. */
#define BITP_AN_ADV_ABILITY_L_AN_ADV_FORCE_MS                 (12U)          /* Force Master/slave Configuration. */
#define BITL_AN_ADV_ABILITY_L_AN_ADV_FORCE_MS                 (1U)           /* Force Master/slave Configuration. */
#define BITM_AN_ADV_ABILITY_L_AN_ADV_FORCE_MS                 (0X1000U)      /* Force Master/slave Configuration. */
#define BITP_AN_ADV_ABILITY_L_AN_ADV_REMOTE_FAULT             (13U)          /* Remote Fault. */
#define BITL_AN_ADV_ABILITY_L_AN_ADV_REMOTE_FAULT             (1U)           /* Remote Fault. */
#define BITM_AN_ADV_ABILITY_L_AN_ADV_REMOTE_FAULT             (0X2000U)      /* Remote Fault. */
#define BITP_AN_ADV_ABILITY_L_AN_ADV_ACK                      (14U)          /* Acknowledge (ACK). */
#define BITL_AN_ADV_ABILITY_L_AN_ADV_ACK                      (1U)           /* Acknowledge (ACK). */
#define BITM_AN_ADV_ABILITY_L_AN_ADV_ACK                      (0X4000U)      /* Acknowledge (ACK). */
#define BITP_AN_ADV_ABILITY_L_AN_ADV_NEXT_PAGE_REQ            (15U)          /* Next Page Request. */
#define BITL_AN_ADV_ABILITY_L_AN_ADV_NEXT_PAGE_REQ            (1U)           /* Next Page Request. */
#define BITM_AN_ADV_ABILITY_L_AN_ADV_NEXT_PAGE_REQ            (0X8000U)      /* Next Page Request. */

/* ----------------------------------------------------------------------------------------------------
          AN_ADV_ABILITY_M                                      Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_AN_ADV_ABILITY_M_AN_ADV_MST                      (4U)           /* Master/slave Configuration. */
#define BITL_AN_ADV_ABILITY_M_AN_ADV_MST                      (1U)           /* Master/slave Configuration. */
#define BITM_AN_ADV_ABILITY_M_AN_ADV_MST                      (0X0010U)      /* Master/slave Configuration. */
#define BITP_AN_ADV_ABILITY_M_AN_ADV_B10L                     (14U)          /* 10BASE-T1L Ability. */
#define BITL_AN_ADV_ABILITY_M_AN_ADV_B10L                     (1U)           /* 10BASE-T1L Ability. */
#define BITM_AN_ADV_ABILITY_M_AN_ADV_B10L                     (0X4000U)      /* 10BASE-T1L Ability. */

/* ----------------------------------------------------------------------------------------------------
          AN_ADV_ABILITY_H                                      Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_AN_ADV_ABILITY_H_AN_ADV_B10L_TX_LVL_HI_REQ       (12U)          /* 10BASE-T1L High Level Transmit Operating Mode Request. */
#define BITL_AN_ADV_ABILITY_H_AN_ADV_B10L_TX_LVL_HI_REQ       (1U)           /* 10BASE-T1L High Level Transmit Operating Mode Request. */
#define BITM_AN_ADV_ABILITY_H_AN_ADV_B10L_TX_LVL_HI_REQ       (0X1000U)      /* 10BASE-T1L High Level Transmit Operating Mode Request. */
#define BITP_AN_ADV_ABILITY_H_AN_ADV_B10L_TX_LVL_HI_ABL       (13U)          /* 10BASE-T1L High Level Transmit Operating Mode Ability. */
#define BITL_AN_ADV_ABILITY_H_AN_ADV_B10L_TX_LVL_HI_ABL       (1U)           /* 10BASE-T1L High Level Transmit Operating Mode Ability. */
#define BITM_AN_ADV_ABILITY_H_AN_ADV_B10L_TX_LVL_HI_ABL       (0X2000U)      /* 10BASE-T1L High Level Transmit Operating Mode Ability. */

/* ----------------------------------------------------------------------------------------------------
          AN_LP_ADV_ABILITY_L                                   Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_AN_LP_ADV_ABILITY_L_AN_LP_ADV_SELECTOR           (0U)           /* Link Partner Selector. */
#define BITL_AN_LP_ADV_ABILITY_L_AN_LP_ADV_SELECTOR           (5U)           /* Link Partner Selector. */
#define BITM_AN_LP_ADV_ABILITY_L_AN_LP_ADV_SELECTOR           (0X001FU)      /* Link Partner Selector. */
#define BITP_AN_LP_ADV_ABILITY_L_AN_LP_ADV_PAUSE              (10U)          /* Link Partner Pause Ability. */
#define BITL_AN_LP_ADV_ABILITY_L_AN_LP_ADV_PAUSE              (2U)           /* Link Partner Pause Ability. */
#define BITM_AN_LP_ADV_ABILITY_L_AN_LP_ADV_PAUSE              (0X0C00U)      /* Link Partner Pause Ability. */
#define BITP_AN_LP_ADV_ABILITY_L_AN_LP_ADV_FORCE_MS           (12U)          /* Link Partner Force Master/slave Configuration. */
#define BITL_AN_LP_ADV_ABILITY_L_AN_LP_ADV_FORCE_MS           (1U)           /* Link Partner Force Master/slave Configuration. */
#define BITM_AN_LP_ADV_ABILITY_L_AN_LP_ADV_FORCE_MS           (0X1000U)      /* Link Partner Force Master/slave Configuration. */
#define BITP_AN_LP_ADV_ABILITY_L_AN_LP_ADV_REMOTE_FAULT       (13U)          /* Link Partner Remote Fault. */
#define BITL_AN_LP_ADV_ABILITY_L_AN_LP_ADV_REMOTE_FAULT       (1U)           /* Link Partner Remote Fault. */
#define BITM_AN_LP_ADV_ABILITY_L_AN_LP_ADV_REMOTE_FAULT       (0X2000U)      /* Link Partner Remote Fault. */
#define BITP_AN_LP_ADV_ABILITY_L_AN_LP_ADV_ACK                (14U)          /* Link Partner Acknowledge (ACK). */
#define BITL_AN_LP_ADV_ABILITY_L_AN_LP_ADV_ACK                (1U)           /* Link Partner Acknowledge (ACK). */
#define BITM_AN_LP_ADV_ABILITY_L_AN_LP_ADV_ACK                (0X4000U)      /* Link Partner Acknowledge (ACK). */
#define BITP_AN_LP_ADV_ABILITY_L_AN_LP_ADV_NEXT_PAGE_REQ      (15U)          /* Link Partner Next Page Request. */
#define BITL_AN_LP_ADV_ABILITY_L_AN_LP_ADV_NEXT_PAGE_REQ      (1U)           /* Link Partner Next Page Request. */
#define BITM_AN_LP_ADV_ABILITY_L_AN_LP_ADV_NEXT_PAGE_REQ      (0X8000U)      /* Link Partner Next Page Request. */

/* ----------------------------------------------------------------------------------------------------
          AN_LP_ADV_ABILITY_M                                   Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_AN_LP_ADV_ABILITY_M_AN_LP_ADV_MST                (4U)           /* Link Partner MASTER/_SLAVE Configuration. */
#define BITL_AN_LP_ADV_ABILITY_M_AN_LP_ADV_MST                (1U)           /* Link Partner MASTER/_SLAVE Configuration. */
#define BITM_AN_LP_ADV_ABILITY_M_AN_LP_ADV_MST                (0X0010U)      /* Link Partner MASTER/_SLAVE Configuration. */
#define BITP_AN_LP_ADV_ABILITY_M_AN_LP_ADV_B100               (5U)           /* Link Partner 100BASE-T1 Ability. */
#define BITL_AN_LP_ADV_ABILITY_M_AN_LP_ADV_B100               (1U)           /* Link Partner 100BASE-T1 Ability. */
#define BITM_AN_LP_ADV_ABILITY_M_AN_LP_ADV_B100               (0X0020U)      /* Link Partner 100BASE-T1 Ability. */
#define BITP_AN_LP_ADV_ABILITY_M_AN_LP_ADV_B10S_FD            (6U)           /* Link Partner 10BASE-T1S Full Duplex Ability. */
#define BITL_AN_LP_ADV_ABILITY_M_AN_LP_ADV_B10S_FD            (1U)           /* Link Partner 10BASE-T1S Full Duplex Ability. */
#define BITM_AN_LP_ADV_ABILITY_M_AN_LP_ADV_B10S_FD            (0X0040U)      /* Link Partner 10BASE-T1S Full Duplex Ability. */
#define BITP_AN_LP_ADV_ABILITY_M_AN_LP_ADV_B1000              (7U)           /* Link Partner 1000BASE-T1 Ability. */
#define BITL_AN_LP_ADV_ABILITY_M_AN_LP_ADV_B1000              (1U)           /* Link Partner 1000BASE-T1 Ability. */
#define BITM_AN_LP_ADV_ABILITY_M_AN_LP_ADV_B1000              (0X0080U)      /* Link Partner 1000BASE-T1 Ability. */
#define BITP_AN_LP_ADV_ABILITY_M_AN_LP_ADV_B10L               (14U)          /* Link Partner 10BASE-T1L Ability. */
#define BITL_AN_LP_ADV_ABILITY_M_AN_LP_ADV_B10L               (1U)           /* Link Partner 10BASE-T1L Ability. */
#define BITM_AN_LP_ADV_ABILITY_M_AN_LP_ADV_B10L               (0X4000U)      /* Link Partner 10BASE-T1L Ability. */

/* ----------------------------------------------------------------------------------------------------
          AN_LP_ADV_ABILITY_H                                   Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_AN_LP_ADV_ABILITY_H_AN_LP_ADV_B10S_HD            (11U)          /* Link Partner 10BASE-T1S Half Duplex Ability. */
#define BITL_AN_LP_ADV_ABILITY_H_AN_LP_ADV_B10S_HD            (1U)           /* Link Partner 10BASE-T1S Half Duplex Ability. */
#define BITM_AN_LP_ADV_ABILITY_H_AN_LP_ADV_B10S_HD            (0X0800U)      /* Link Partner 10BASE-T1S Half Duplex Ability. */
#define BITP_AN_LP_ADV_ABILITY_H_AN_LP_ADV_B10L_TX_LVL_HI_REQ (12U)          /* Link Partner 10BASE-T1L High Level Transmit Operating Mode Request. */
#define BITL_AN_LP_ADV_ABILITY_H_AN_LP_ADV_B10L_TX_LVL_HI_REQ (1U)           /* Link Partner 10BASE-T1L High Level Transmit Operating Mode Request. */
#define BITM_AN_LP_ADV_ABILITY_H_AN_LP_ADV_B10L_TX_LVL_HI_REQ (0X1000U)      /* Link Partner 10BASE-T1L High Level Transmit Operating Mode Request. */
#define BITP_AN_LP_ADV_ABILITY_H_AN_LP_ADV_B10L_TX_LVL_HI_ABL (13U)          /* Link Partner 10BASE-T1L High Level Transmit Operating Mode Ability. */
#define BITL_AN_LP_ADV_ABILITY_H_AN_LP_ADV_B10L_TX_LVL_HI_ABL (1U)           /* Link Partner 10BASE-T1L High Level Transmit Operating Mode Ability. */
#define BITM_AN_LP_ADV_ABILITY_H_AN_LP_ADV_B10L_TX_LVL_HI_ABL (0X2000U)      /* Link Partner 10BASE-T1L High Level Transmit Operating Mode Ability. */
#define BITP_AN_LP_ADV_ABILITY_H_AN_LP_ADV_B10L_EEE           (14U)          /* Link Partner 10BASE-T1L EEE Ability. */
#define BITL_AN_LP_ADV_ABILITY_H_AN_LP_ADV_B10L_EEE           (1U)           /* Link Partner 10BASE-T1L EEE Ability. */
#define BITM_AN_LP_ADV_ABILITY_H_AN_LP_ADV_B10L_EEE           (0X4000U)      /* Link Partner 10BASE-T1L EEE Ability. */

/* ----------------------------------------------------------------------------------------------------
          AN_NEXT_PAGE_L                                        Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_AN_NEXT_PAGE_L_AN_NP_MESSAGE_CODE                (0U)           /* Message/unformatted Code Field. */
#define BITL_AN_NEXT_PAGE_L_AN_NP_MESSAGE_CODE                (11U)          /* Message/unformatted Code Field. */
#define BITM_AN_NEXT_PAGE_L_AN_NP_MESSAGE_CODE                (0X07FFU)      /* Message/unformatted Code Field. */
#define BITP_AN_NEXT_PAGE_L_AN_NP_TOGGLE                      (11U)          /* Toggle Bit. */
#define BITL_AN_NEXT_PAGE_L_AN_NP_TOGGLE                      (1U)           /* Toggle Bit. */
#define BITM_AN_NEXT_PAGE_L_AN_NP_TOGGLE                      (0X0800U)      /* Toggle Bit. */
#define BITP_AN_NEXT_PAGE_L_AN_NP_ACK2                        (12U)          /* Acknowledge 2. */
#define BITL_AN_NEXT_PAGE_L_AN_NP_ACK2                        (1U)           /* Acknowledge 2. */
#define BITM_AN_NEXT_PAGE_L_AN_NP_ACK2                        (0X1000U)      /* Acknowledge 2. */
#define BITP_AN_NEXT_PAGE_L_AN_NP_MESSAGE_PAGE                (13U)          /* Next Page Encoding. */
#define BITL_AN_NEXT_PAGE_L_AN_NP_MESSAGE_PAGE                (1U)           /* Next Page Encoding. */
#define BITM_AN_NEXT_PAGE_L_AN_NP_MESSAGE_PAGE                (0X2000U)      /* Next Page Encoding. */
#define BITP_AN_NEXT_PAGE_L_AN_NP_ACK                         (14U)          /* Next Page Acknowledge. */
#define BITL_AN_NEXT_PAGE_L_AN_NP_ACK                         (1U)           /* Next Page Acknowledge. */
#define BITM_AN_NEXT_PAGE_L_AN_NP_ACK                         (0X4000U)      /* Next Page Acknowledge. */
#define BITP_AN_NEXT_PAGE_L_AN_NP_NEXT_PAGE_REQ               (15U)          /* Next Page Request. */
#define BITL_AN_NEXT_PAGE_L_AN_NP_NEXT_PAGE_REQ               (1U)           /* Next Page Request. */
#define BITM_AN_NEXT_PAGE_L_AN_NP_NEXT_PAGE_REQ               (0X8000U)      /* Next Page Request. */

#define ENUM_AN_NEXT_PAGE_L_AN_NP_MESSAGE_PAGE_NP_UNFORMATTED_NEXT_PAGE (0X0000U)      /* Unformatted next page */
#define ENUM_AN_NEXT_PAGE_L_AN_NP_MESSAGE_PAGE_NP_MESSAGE_NEXT_PAGE (0X0001U)      /* Message next page */
#define ENUM_AN_NEXT_PAGE_L_AN_NP_MESSAGE_CODE_NP_MESSAGE_NULL (0X0001U)      /* Null Message */
#define ENUM_AN_NEXT_PAGE_L_AN_NP_MESSAGE_CODE_NP_MESSAGE_OUI (0X0005U)      /* Organizationally Unique Identifier Tagged Message */
#define ENUM_AN_NEXT_PAGE_L_AN_NP_MESSAGE_CODE_NP_MESSAGE_AN_DEV_ID (0X0006U)      /* AN Device Identifier Tag Code */

/* ----------------------------------------------------------------------------------------------------
          AN_NEXT_PAGE_M                                        Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_AN_NEXT_PAGE_M_AN_NP_UNFORMATTED1                (0U)           /* Unformatted Code Field 1 */
#define BITL_AN_NEXT_PAGE_M_AN_NP_UNFORMATTED1                (16U)          /* Unformatted Code Field 1 */
#define BITM_AN_NEXT_PAGE_M_AN_NP_UNFORMATTED1                (0XFFFFU)      /* Unformatted Code Field 1 */

/* ----------------------------------------------------------------------------------------------------
          AN_NEXT_PAGE_H                                        Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_AN_NEXT_PAGE_H_AN_NP_UNFORMATTED2                (0U)           /* Unformatted Code Field 2 */
#define BITL_AN_NEXT_PAGE_H_AN_NP_UNFORMATTED2                (16U)          /* Unformatted Code Field 2 */
#define BITM_AN_NEXT_PAGE_H_AN_NP_UNFORMATTED2                (0XFFFFU)      /* Unformatted Code Field 2 */

/* ----------------------------------------------------------------------------------------------------
          AN_LP_NEXT_PAGE_L                                     Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_AN_LP_NEXT_PAGE_L_AN_LP_NP_MESSAGE_CODE          (0U)           /* Link Partner Message/unformatted Code Field. */
#define BITL_AN_LP_NEXT_PAGE_L_AN_LP_NP_MESSAGE_CODE          (11U)          /* Link Partner Message/unformatted Code Field. */
#define BITM_AN_LP_NEXT_PAGE_L_AN_LP_NP_MESSAGE_CODE          (0X07FFU)      /* Link Partner Message/unformatted Code Field. */
#define BITP_AN_LP_NEXT_PAGE_L_AN_LP_NP_TOGGLE                (11U)          /* Link Partner Toggle Bit. */
#define BITL_AN_LP_NEXT_PAGE_L_AN_LP_NP_TOGGLE                (1U)           /* Link Partner Toggle Bit. */
#define BITM_AN_LP_NEXT_PAGE_L_AN_LP_NP_TOGGLE                (0X0800U)      /* Link Partner Toggle Bit. */
#define BITP_AN_LP_NEXT_PAGE_L_AN_LP_NP_ACK2                  (12U)          /* Link Partner Acknowledge 2. */
#define BITL_AN_LP_NEXT_PAGE_L_AN_LP_NP_ACK2                  (1U)           /* Link Partner Acknowledge 2. */
#define BITM_AN_LP_NEXT_PAGE_L_AN_LP_NP_ACK2                  (0X1000U)      /* Link Partner Acknowledge 2. */
#define BITP_AN_LP_NEXT_PAGE_L_AN_LP_NP_MESSAGE_PAGE          (13U)          /* Link Partner Next Page Encoding. */
#define BITL_AN_LP_NEXT_PAGE_L_AN_LP_NP_MESSAGE_PAGE          (1U)           /* Link Partner Next Page Encoding. */
#define BITM_AN_LP_NEXT_PAGE_L_AN_LP_NP_MESSAGE_PAGE          (0X2000U)      /* Link Partner Next Page Encoding. */
#define BITP_AN_LP_NEXT_PAGE_L_AN_LP_NP_ACK                   (14U)          /* Link Partner Next Page Acknowledge. */
#define BITL_AN_LP_NEXT_PAGE_L_AN_LP_NP_ACK                   (1U)           /* Link Partner Next Page Acknowledge. */
#define BITM_AN_LP_NEXT_PAGE_L_AN_LP_NP_ACK                   (0X4000U)      /* Link Partner Next Page Acknowledge. */
#define BITP_AN_LP_NEXT_PAGE_L_AN_LP_NP_NEXT_PAGE_REQ         (15U)          /* Next Page Request. */
#define BITL_AN_LP_NEXT_PAGE_L_AN_LP_NP_NEXT_PAGE_REQ         (1U)           /* Next Page Request. */
#define BITM_AN_LP_NEXT_PAGE_L_AN_LP_NP_NEXT_PAGE_REQ         (0X8000U)      /* Next Page Request. */

#define ENUM_AN_LP_NEXT_PAGE_L_AN_LP_NP_MESSAGE_PAGE_NP_UNFORMATTED_NEXT_PAGE (0X0000U)      /* Unformatted next page */
#define ENUM_AN_LP_NEXT_PAGE_L_AN_LP_NP_MESSAGE_PAGE_NP_MESSAGE_NEXT_PAGE (0X0001U)      /* Message next page */
#define ENUM_AN_LP_NEXT_PAGE_L_AN_LP_NP_MESSAGE_CODE_NP_MESSAGE_NULL (0X0001U)      /* Null Message */
#define ENUM_AN_LP_NEXT_PAGE_L_AN_LP_NP_MESSAGE_CODE_NP_MESSAGE_OUI (0X0005U)      /* Organizationally Unique Identifier Tagged Message */
#define ENUM_AN_LP_NEXT_PAGE_L_AN_LP_NP_MESSAGE_CODE_NP_MESSAGE_AN_DEV_ID (0X0006U)      /* AN Device Identifier Tag Code */

/* ----------------------------------------------------------------------------------------------------
          AN_LP_NEXT_PAGE_M                                     Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_AN_LP_NEXT_PAGE_M_AN_LP_NP_UNFORMATTED1          (0U)           /* Link Partner Unformatted Code Field 1 */
#define BITL_AN_LP_NEXT_PAGE_M_AN_LP_NP_UNFORMATTED1          (16U)          /* Link Partner Unformatted Code Field 1 */
#define BITM_AN_LP_NEXT_PAGE_M_AN_LP_NP_UNFORMATTED1          (0XFFFFU)      /* Link Partner Unformatted Code Field 1 */

/* ----------------------------------------------------------------------------------------------------
          AN_LP_NEXT_PAGE_H                                     Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_AN_LP_NEXT_PAGE_H_AN_LP_NP_UNFORMATTED2          (0U)           /* Link Partner Unformatted Code Field 2 */
#define BITL_AN_LP_NEXT_PAGE_H_AN_LP_NP_UNFORMATTED2          (16U)          /* Link Partner Unformatted Code Field 2 */
#define BITM_AN_LP_NEXT_PAGE_H_AN_LP_NP_UNFORMATTED2          (0XFFFFU)      /* Link Partner Unformatted Code Field 2 */

/* ----------------------------------------------------------------------------------------------------
          AN_B10_ADV_ABILITY                                    Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_AN_B10_ADV_ABILITY_AN_B10_ADV_B10L_TX_LVL_HI_REQ (12U)          /* 10BASE-T1L High Level Transmit Operating Mode Request. */
#define BITL_AN_B10_ADV_ABILITY_AN_B10_ADV_B10L_TX_LVL_HI_REQ (1U)           /* 10BASE-T1L High Level Transmit Operating Mode Request. */
#define BITM_AN_B10_ADV_ABILITY_AN_B10_ADV_B10L_TX_LVL_HI_REQ (0X1000U)      /* 10BASE-T1L High Level Transmit Operating Mode Request. */
#define BITP_AN_B10_ADV_ABILITY_AN_B10_ADV_B10L_TX_LVL_HI_ABL (13U)          /* 10BASE-T1L High Level Transmit Operating Mode Ability. */
#define BITL_AN_B10_ADV_ABILITY_AN_B10_ADV_B10L_TX_LVL_HI_ABL (1U)           /* 10BASE-T1L High Level Transmit Operating Mode Ability. */
#define BITM_AN_B10_ADV_ABILITY_AN_B10_ADV_B10L_TX_LVL_HI_ABL (0X2000U)      /* 10BASE-T1L High Level Transmit Operating Mode Ability. */
#define BITP_AN_B10_ADV_ABILITY_AN_B10_ADV_B10L_EEE           (14U)          /* 10BASE-T1L EEE Ability. */
#define BITL_AN_B10_ADV_ABILITY_AN_B10_ADV_B10L_EEE           (1U)           /* 10BASE-T1L EEE Ability. */
#define BITM_AN_B10_ADV_ABILITY_AN_B10_ADV_B10L_EEE           (0X4000U)      /* 10BASE-T1L EEE Ability. */
#define BITP_AN_B10_ADV_ABILITY_AN_B10_ADV_B10L               (15U)          /* 10BASE-T1L Ability. */
#define BITL_AN_B10_ADV_ABILITY_AN_B10_ADV_B10L               (1U)           /* 10BASE-T1L Ability. */
#define BITM_AN_B10_ADV_ABILITY_AN_B10_ADV_B10L               (0X8000U)      /* 10BASE-T1L Ability. */

/* ----------------------------------------------------------------------------------------------------
          AN_B10_LP_ADV_ABILITY                                 Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_AN_B10_LP_ADV_ABILITY_AN_B10_LP_ADV_B10S_HD      (6U)           /* Link Partner 10BASE-T1S Half Duplex Ability. */
#define BITL_AN_B10_LP_ADV_ABILITY_AN_B10_LP_ADV_B10S_HD      (1U)           /* Link Partner 10BASE-T1S Half Duplex Ability. */
#define BITM_AN_B10_LP_ADV_ABILITY_AN_B10_LP_ADV_B10S_HD      (0X0040U)      /* Link Partner 10BASE-T1S Half Duplex Ability. */
#define BITP_AN_B10_LP_ADV_ABILITY_AN_B10_LP_ADV_B10S_FD      (7U)           /* Link Partner 10BASE-T1S Full Duplex Ability. */
#define BITL_AN_B10_LP_ADV_ABILITY_AN_B10_LP_ADV_B10S_FD      (1U)           /* Link Partner 10BASE-T1S Full Duplex Ability. */
#define BITM_AN_B10_LP_ADV_ABILITY_AN_B10_LP_ADV_B10S_FD      (0X0080U)      /* Link Partner 10BASE-T1S Full Duplex Ability. */
#define BITP_AN_B10_LP_ADV_ABILITY_AN_B10_LP_ADV_B10L_TX_LVL_HI_REQ (12U)          /* 10BASE-T1L High Level Transmit Operating Mode Request. */
#define BITL_AN_B10_LP_ADV_ABILITY_AN_B10_LP_ADV_B10L_TX_LVL_HI_REQ (1U)           /* 10BASE-T1L High Level Transmit Operating Mode Request. */
#define BITM_AN_B10_LP_ADV_ABILITY_AN_B10_LP_ADV_B10L_TX_LVL_HI_REQ (0X1000U)      /* 10BASE-T1L High Level Transmit Operating Mode Request. */
#define BITP_AN_B10_LP_ADV_ABILITY_AN_B10_LP_ADV_B10L_TX_LVL_HI_ABL (13U)          /* 10BASE-T1L High Level Transmit Operating Mode Ability. */
#define BITL_AN_B10_LP_ADV_ABILITY_AN_B10_LP_ADV_B10L_TX_LVL_HI_ABL (1U)           /* 10BASE-T1L High Level Transmit Operating Mode Ability. */
#define BITM_AN_B10_LP_ADV_ABILITY_AN_B10_LP_ADV_B10L_TX_LVL_HI_ABL (0X2000U)      /* 10BASE-T1L High Level Transmit Operating Mode Ability. */
#define BITP_AN_B10_LP_ADV_ABILITY_AN_B10_LP_ADV_B10L_EEE     (14U)          /* 10BASE-T1L EEE Ability. */
#define BITL_AN_B10_LP_ADV_ABILITY_AN_B10_LP_ADV_B10L_EEE     (1U)           /* 10BASE-T1L EEE Ability. */
#define BITM_AN_B10_LP_ADV_ABILITY_AN_B10_LP_ADV_B10L_EEE     (0X4000U)      /* 10BASE-T1L EEE Ability. */
#define BITP_AN_B10_LP_ADV_ABILITY_AN_B10_LP_ADV_B10L         (15U)          /* 10BASE-T1L Ability. */
#define BITL_AN_B10_LP_ADV_ABILITY_AN_B10_LP_ADV_B10L         (1U)           /* 10BASE-T1L Ability. */
#define BITM_AN_B10_LP_ADV_ABILITY_AN_B10_LP_ADV_B10L         (0X8000U)      /* 10BASE-T1L Ability. */

/* ----------------------------------------------------------------------------------------------------
          AN_FRC_MODE_EN                                        Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_AN_FRC_MODE_EN_AN_FRC_MODE_EN                    (0U)           /* Autonegotiation Forced Mode. */
#define BITL_AN_FRC_MODE_EN_AN_FRC_MODE_EN                    (1U)           /* Autonegotiation Forced Mode. */
#define BITM_AN_FRC_MODE_EN_AN_FRC_MODE_EN                    (0X0001U)      /* Autonegotiation Forced Mode. */

/* ----------------------------------------------------------------------------------------------------
          AN_STATUS_EXTRA                                       Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_AN_STATUS_EXTRA_AN_LINK_GOOD                     (0U)           /* Autonegotiation Complete Indication. */
#define BITL_AN_STATUS_EXTRA_AN_LINK_GOOD                     (1U)           /* Autonegotiation Complete Indication. */
#define BITM_AN_STATUS_EXTRA_AN_LINK_GOOD                     (0X0001U)      /* Autonegotiation Complete Indication. */
#define BITP_AN_STATUS_EXTRA_AN_HCD_TECH                      (1U)           /* Highest Common Denominator (HCD) PHY Technology. */
#define BITL_AN_STATUS_EXTRA_AN_HCD_TECH                      (4U)           /* Highest Common Denominator (HCD) PHY Technology. */
#define BITM_AN_STATUS_EXTRA_AN_HCD_TECH                      (0X001EU)      /* Highest Common Denominator (HCD) PHY Technology. */
#define BITP_AN_STATUS_EXTRA_AN_MS_CONFIG_RSLTN               (5U)           /* Master/slave Resolution Result. */
#define BITL_AN_STATUS_EXTRA_AN_MS_CONFIG_RSLTN               (2U)           /* Master/slave Resolution Result. */
#define BITM_AN_STATUS_EXTRA_AN_MS_CONFIG_RSLTN               (0X0060U)      /* Master/slave Resolution Result. */
#define BITP_AN_STATUS_EXTRA_AN_TX_LVL_RSLTN                  (7U)           /* Autonegotiation Tx Level Result */
#define BITL_AN_STATUS_EXTRA_AN_TX_LVL_RSLTN                  (2U)           /* Autonegotiation Tx Level Result */
#define BITM_AN_STATUS_EXTRA_AN_TX_LVL_RSLTN                  (0X0180U)      /* Autonegotiation Tx Level Result */
#define BITP_AN_STATUS_EXTRA_AN_INC_LINK                      (9U)           /* Incompatible Link Indication. */
#define BITL_AN_STATUS_EXTRA_AN_INC_LINK                      (1U)           /* Incompatible Link Indication. */
#define BITM_AN_STATUS_EXTRA_AN_INC_LINK                      (0X0200U)      /* Incompatible Link Indication. */
#define BITP_AN_STATUS_EXTRA_AN_LP_NP_RX                      (10U)          /* Next Page Request Received from Link Partner. */
#define BITL_AN_STATUS_EXTRA_AN_LP_NP_RX                      (1U)           /* Next Page Request Received from Link Partner. */
#define BITM_AN_STATUS_EXTRA_AN_LP_NP_RX                      (0X0400U)      /* Next Page Request Received from Link Partner. */

#define ENUM_AN_STATUS_EXTRA_AN_TX_LVL_RSLTN_TX_LVL_NOT_RUN   (0X0000U)      /* Not run */
#define ENUM_AN_STATUS_EXTRA_AN_TX_LVL_RSLTN_TX_LVL_LOW       (0X0002U)      /* Success, low transmit levels (1.0 Vpp) selected */
#define ENUM_AN_STATUS_EXTRA_AN_TX_LVL_RSLTN_TX_LVL_HIGH      (0X0003U)      /* Success, high transmit levels (2.4 Vpp) selected */
#define ENUM_AN_STATUS_EXTRA_AN_MS_CONFIG_RSLTN_MS_NOT_RUN    (0X0000U)      /* Not run */
#define ENUM_AN_STATUS_EXTRA_AN_MS_CONFIG_RSLTN_MS_FAULT      (0X0001U)      /* Configuration fault */
#define ENUM_AN_STATUS_EXTRA_AN_MS_CONFIG_RSLTN_MS_SLAVE      (0X0002U)      /* Success, PHY is configured as SLAVE */
#define ENUM_AN_STATUS_EXTRA_AN_MS_CONFIG_RSLTN_MS_MASTER     (0X0003U)      /* Success, PHY is configured as MASTER */
#define ENUM_AN_STATUS_EXTRA_AN_HCD_TECH_HCD_NULL             (0X0000U)      /* NULL (not run) */
#define ENUM_AN_STATUS_EXTRA_AN_HCD_TECH_HCD_B10L             (0X0001U)      /* 10BASE-T1L */

/* ----------------------------------------------------------------------------------------------------
          AN_PHY_INST_STATUS                                    Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_AN_PHY_INST_STATUS_IS_TX_LVL_LO                  (0U)           /* Tx Level Low Status */
#define BITL_AN_PHY_INST_STATUS_IS_TX_LVL_LO                  (1U)           /* Tx Level Low Status */
#define BITM_AN_PHY_INST_STATUS_IS_TX_LVL_LO                  (0X0001U)      /* Tx Level Low Status */
#define BITP_AN_PHY_INST_STATUS_IS_TX_LVL_HI                  (1U)           /* Tx Level High Status */
#define BITL_AN_PHY_INST_STATUS_IS_TX_LVL_HI                  (1U)           /* Tx Level High Status */
#define BITM_AN_PHY_INST_STATUS_IS_TX_LVL_HI                  (0X0002U)      /* Tx Level High Status */
#define BITP_AN_PHY_INST_STATUS_IS_CFG_SLV                    (2U)           /* Slave Status */
#define BITL_AN_PHY_INST_STATUS_IS_CFG_SLV                    (1U)           /* Slave Status */
#define BITM_AN_PHY_INST_STATUS_IS_CFG_SLV                    (0X0004U)      /* Slave Status */
#define BITP_AN_PHY_INST_STATUS_IS_CFG_MST                    (3U)           /* Master Status */
#define BITL_AN_PHY_INST_STATUS_IS_CFG_MST                    (1U)           /* Master Status */
#define BITM_AN_PHY_INST_STATUS_IS_CFG_MST                    (0X0008U)      /* Master Status */
#define BITP_AN_PHY_INST_STATUS_IS_AN_TX_EN                   (4U)           /* Autonegotiation Tx Enable Status */
#define BITL_AN_PHY_INST_STATUS_IS_AN_TX_EN                   (1U)           /* Autonegotiation Tx Enable Status */
#define BITM_AN_PHY_INST_STATUS_IS_AN_TX_EN                   (0X0010U)      /* Autonegotiation Tx Enable Status */

/* ----------------------------------------------------------------------------------------------------
          MMD1_DEV_ID1                                          Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MMD1_DEV_ID1_MMD1_DEV_ID1                        (0U)           /* Organizationally Unique Identifier */
#define BITL_MMD1_DEV_ID1_MMD1_DEV_ID1                        (16U)          /* Organizationally Unique Identifier */
#define BITM_MMD1_DEV_ID1_MMD1_DEV_ID1                        (0XFFFFU)      /* Organizationally Unique Identifier */

/* ----------------------------------------------------------------------------------------------------
          MMD1_DEV_ID2                                          Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MMD1_DEV_ID2_MMD1_REV_NUM                        (0U)           /* Revision Number */
#define BITL_MMD1_DEV_ID2_MMD1_REV_NUM                        (4U)           /* Revision Number */
#define BITM_MMD1_DEV_ID2_MMD1_REV_NUM                        (0X000FU)      /* Revision Number */
#define BITP_MMD1_DEV_ID2_MMD1_MODEL_NUM                      (4U)           /* Model Number */
#define BITL_MMD1_DEV_ID2_MMD1_MODEL_NUM                      (6U)           /* Model Number */
#define BITM_MMD1_DEV_ID2_MMD1_MODEL_NUM                      (0X03F0U)      /* Model Number */
#define BITP_MMD1_DEV_ID2_MMD1_DEV_ID2_OUI                    (10U)          /* Organizationally Unique Identifier */
#define BITL_MMD1_DEV_ID2_MMD1_DEV_ID2_OUI                    (6U)           /* Organizationally Unique Identifier */
#define BITM_MMD1_DEV_ID2_MMD1_DEV_ID2_OUI                    (0XFC00U)      /* Organizationally Unique Identifier */

/* ----------------------------------------------------------------------------------------------------
          MMD1_DEVS_IN_PKG1                                     Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MMD1_DEVS_IN_PKG1_MMD1_DEVS_IN_PKG1              (0U)           /* Vendor specific 1 MMD Devices in Package */
#define BITL_MMD1_DEVS_IN_PKG1_MMD1_DEVS_IN_PKG1              (16U)          /* Vendor specific 1 MMD Devices in Package */
#define BITM_MMD1_DEVS_IN_PKG1_MMD1_DEVS_IN_PKG1              (0XFFFFU)      /* Vendor specific 1 MMD Devices in Package */

/* ----------------------------------------------------------------------------------------------------
          MMD1_DEVS_IN_PKG2                                     Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MMD1_DEVS_IN_PKG2_MMD1_DEVS_IN_PKG2              (0U)           /* Vendor specific 1 MMD Devices in Package */
#define BITL_MMD1_DEVS_IN_PKG2_MMD1_DEVS_IN_PKG2              (16U)          /* Vendor specific 1 MMD Devices in Package */
#define BITM_MMD1_DEVS_IN_PKG2_MMD1_DEVS_IN_PKG2              (0XFFFFU)      /* Vendor specific 1 MMD Devices in Package */

/* ----------------------------------------------------------------------------------------------------
          MMD1_STATUS                                           Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MMD1_STATUS_MMD1_STATUS                          (14U)          /* Vendor Specific 1 MMD Status */
#define BITL_MMD1_STATUS_MMD1_STATUS                          (2U)           /* Vendor Specific 1 MMD Status */
#define BITM_MMD1_STATUS_MMD1_STATUS                          (0XC000U)      /* Vendor Specific 1 MMD Status */

/* ----------------------------------------------------------------------------------------------------
          CRSM_IRQ_STATUS                                       Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_CRSM_IRQ_STATUS_CRSM_HRD_RST_IRQ_LH              (12U)          /* Hardware Reset Interrupt */
#define BITL_CRSM_IRQ_STATUS_CRSM_HRD_RST_IRQ_LH              (1U)           /* Hardware Reset Interrupt */
#define BITM_CRSM_IRQ_STATUS_CRSM_HRD_RST_IRQ_LH              (0X1000U)      /* Hardware Reset Interrupt */
#define BITP_CRSM_IRQ_STATUS_CRSM_SW_IRQ_LH                   (15U)          /* Software Requested Interrupt Event. */
#define BITL_CRSM_IRQ_STATUS_CRSM_SW_IRQ_LH                   (1U)           /* Software Requested Interrupt Event. */
#define BITM_CRSM_IRQ_STATUS_CRSM_SW_IRQ_LH                   (0X8000U)      /* Software Requested Interrupt Event. */

/* ----------------------------------------------------------------------------------------------------
          CRSM_IRQ_MASK                                         Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_CRSM_IRQ_MASK_CRSM_HRD_RST_IRQ_EN                (12U)          /* Enable Hardware Reset Interrupt. */
#define BITL_CRSM_IRQ_MASK_CRSM_HRD_RST_IRQ_EN                (1U)           /* Enable Hardware Reset Interrupt. */
#define BITM_CRSM_IRQ_MASK_CRSM_HRD_RST_IRQ_EN                (0X1000U)      /* Enable Hardware Reset Interrupt. */
#define BITP_CRSM_IRQ_MASK_CRSM_SW_IRQ_REQ                    (15U)          /* Software Interrupt Request. */
#define BITL_CRSM_IRQ_MASK_CRSM_SW_IRQ_REQ                    (1U)           /* Software Interrupt Request. */
#define BITM_CRSM_IRQ_MASK_CRSM_SW_IRQ_REQ                    (0X8000U)      /* Software Interrupt Request. */

/* ----------------------------------------------------------------------------------------------------
          CRSM_SFT_RST                                          Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_CRSM_SFT_RST_CRSM_SFT_RST                        (0U)           /* Software Reset Register. */
#define BITL_CRSM_SFT_RST_CRSM_SFT_RST                        (1U)           /* Software Reset Register. */
#define BITM_CRSM_SFT_RST_CRSM_SFT_RST                        (0X0001U)      /* Software Reset Register. */

/* ----------------------------------------------------------------------------------------------------
          CRSM_SFT_PD_CNTRL                                     Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_CRSM_SFT_PD_CNTRL_CRSM_SFT_PD                    (0U)           /* Software Power-down */
#define BITL_CRSM_SFT_PD_CNTRL_CRSM_SFT_PD                    (1U)           /* Software Power-down */
#define BITM_CRSM_SFT_PD_CNTRL_CRSM_SFT_PD                    (0X0001U)      /* Software Power-down */

/* ----------------------------------------------------------------------------------------------------
          CRSM_PHY_SUBSYS_RST                                   Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_CRSM_PHY_SUBSYS_RST_CRSM_PHY_SUBSYS_RST          (0U)           /* PHY Subsystem Reset */
#define BITL_CRSM_PHY_SUBSYS_RST_CRSM_PHY_SUBSYS_RST          (1U)           /* PHY Subsystem Reset */
#define BITM_CRSM_PHY_SUBSYS_RST_CRSM_PHY_SUBSYS_RST          (0X0001U)      /* PHY Subsystem Reset */

/* ----------------------------------------------------------------------------------------------------
          CRSM_MAC_IF_RST                                       Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_CRSM_MAC_IF_RST_CRSM_MAC_IF_RST                  (0U)           /* PHY MAC Interface Reset */
#define BITL_CRSM_MAC_IF_RST_CRSM_MAC_IF_RST                  (1U)           /* PHY MAC Interface Reset */
#define BITM_CRSM_MAC_IF_RST_CRSM_MAC_IF_RST                  (0X0001U)      /* PHY MAC Interface Reset */

/* ----------------------------------------------------------------------------------------------------
          CRSM_STAT                                             Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_CRSM_STAT_CRSM_SYS_RDY                           (0U)           /* System Ready */
#define BITL_CRSM_STAT_CRSM_SYS_RDY                           (1U)           /* System Ready */
#define BITM_CRSM_STAT_CRSM_SYS_RDY                           (0X0001U)      /* System Ready */
#define BITP_CRSM_STAT_CRSM_SFT_PD_RDY                        (1U)           /* Software Power-down Status */
#define BITL_CRSM_STAT_CRSM_SFT_PD_RDY                        (1U)           /* Software Power-down Status */
#define BITM_CRSM_STAT_CRSM_SFT_PD_RDY                        (0X0002U)      /* Software Power-down Status */

/* ----------------------------------------------------------------------------------------------------
          CRSM_PMG_CNTRL                                        Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_CRSM_PMG_CNTRL_CRSM_FRC_OSC_EN                   (0U)           /* Force Digital Boot Oscillator Clock Enable */
#define BITL_CRSM_PMG_CNTRL_CRSM_FRC_OSC_EN                   (1U)           /* Force Digital Boot Oscillator Clock Enable */
#define BITM_CRSM_PMG_CNTRL_CRSM_FRC_OSC_EN                   (0X0001U)      /* Force Digital Boot Oscillator Clock Enable */

/* ----------------------------------------------------------------------------------------------------
          CRSM_DIAG_CLK_CTRL                                    Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_CRSM_DIAG_CLK_CTRL_CRSM_DIAG_CLK_EN              (0U)           /* Enable the Diagnostics Clock */
#define BITL_CRSM_DIAG_CLK_CTRL_CRSM_DIAG_CLK_EN              (1U)           /* Enable the Diagnostics Clock */
#define BITM_CRSM_DIAG_CLK_CTRL_CRSM_DIAG_CLK_EN              (0X0001U)      /* Enable the Diagnostics Clock */

/* ----------------------------------------------------------------------------------------------------
          MGMT_PRT_PKG                                          Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MGMT_PRT_PKG_MGMT_PRT_PKG_VAL                    (0U)           /* Package Type */
#define BITL_MGMT_PRT_PKG_MGMT_PRT_PKG_VAL                    (6U)           /* Package Type */
#define BITM_MGMT_PRT_PKG_MGMT_PRT_PKG_VAL                    (0X003FU)      /* Package Type */

/* ----------------------------------------------------------------------------------------------------
          MGMT_MDIO_CNTRL                                       Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MGMT_MDIO_CNTRL_MGMT_GRP_MDIO_EN                 (0U)           /* Enable MDIO PHY/_PORT Group Address Mode */
#define BITL_MGMT_MDIO_CNTRL_MGMT_GRP_MDIO_EN                 (1U)           /* Enable MDIO PHY/_PORT Group Address Mode */
#define BITM_MGMT_MDIO_CNTRL_MGMT_GRP_MDIO_EN                 (0X0001U)      /* Enable MDIO PHY/_PORT Group Address Mode */

/* ----------------------------------------------------------------------------------------------------
          DIGIO_PINMUX                                          Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_DIGIO_PINMUX_DIGIO_LINK_ST_POLARITY              (0U)           /* LINK_ST Polarity */
#define BITL_DIGIO_PINMUX_DIGIO_LINK_ST_POLARITY              (1U)           /* LINK_ST Polarity */
#define BITM_DIGIO_PINMUX_DIGIO_LINK_ST_POLARITY              (0X0001U)      /* LINK_ST Polarity */
#define BITP_DIGIO_PINMUX_DIGIO_LED1_PINMUX                   (1U)           /* Pin Mux Select for LED_1 */
#define BITL_DIGIO_PINMUX_DIGIO_LED1_PINMUX                   (3U)           /* Pin Mux Select for LED_1 */
#define BITM_DIGIO_PINMUX_DIGIO_LED1_PINMUX                   (0X000EU)      /* Pin Mux Select for LED_1 */
#define BITP_DIGIO_PINMUX_DIGIO_TSCAPT_PINMUX                 (4U)           /* Pin Mux Select for TS_CAPT */
#define BITL_DIGIO_PINMUX_DIGIO_TSCAPT_PINMUX                 (2U)           /* Pin Mux Select for TS_CAPT */
#define BITM_DIGIO_PINMUX_DIGIO_TSCAPT_PINMUX                 (0X0030U)      /* Pin Mux Select for TS_CAPT */
#define BITP_DIGIO_PINMUX_DIGIO_TSTIMER_PINMUX                (6U)           /* Pin Mux Select for TS_TIMER */
#define BITL_DIGIO_PINMUX_DIGIO_TSTIMER_PINMUX                (2U)           /* Pin Mux Select for TS_TIMER */
#define BITM_DIGIO_PINMUX_DIGIO_TSTIMER_PINMUX                (0X00C0U)      /* Pin Mux Select for TS_TIMER */

#define ENUM_DIGIO_PINMUX_DIGIO_TSTIMER_PINMUX_RXD_1          (0X0000U)      /* RXD_1 */
#define ENUM_DIGIO_PINMUX_DIGIO_TSTIMER_PINMUX_LED_0          (0X0001U)      /* LED_0 */
#define ENUM_DIGIO_PINMUX_DIGIO_TSTIMER_PINMUX_INT_N          (0X0002U)      /* INT_N */
#define ENUM_DIGIO_PINMUX_DIGIO_TSTIMER_PINMUX_TS_TIMER_NA    (0X0003U)      /* TS_TIMER not assigned */
#define ENUM_DIGIO_PINMUX_DIGIO_LED1_PINMUX_LED_1             (0X0000U)      /* LED_1 */
#define ENUM_DIGIO_PINMUX_DIGIO_LED1_PINMUX_TX_ER             (0X0001U)      /* TX_ER */
#define ENUM_DIGIO_PINMUX_DIGIO_LED1_PINMUX_TX_EN             (0X0002U)      /* TX_EN */
#define ENUM_DIGIO_PINMUX_DIGIO_LED1_PINMUX_TX_CLK            (0X0003U)      /* TX_CLK */
#define ENUM_DIGIO_PINMUX_DIGIO_LED1_PINMUX_TXD_0             (0X0004U)      /* TXD_0 */
#define ENUM_DIGIO_PINMUX_DIGIO_LED1_PINMUX_TXD_2             (0X0005U)      /* TXD_2 */
#define ENUM_DIGIO_PINMUX_DIGIO_LED1_PINMUX_LINK_ST           (0X0006U)      /* LINK_ST */
#define ENUM_DIGIO_PINMUX_DIGIO_LED1_PINMUX_LED_1_NA          (0X0007U)      /* LED_1 output not enabled */
#define ENUM_DIGIO_PINMUX_DIGIO_LINK_ST_POLARITY_ASSERT_HIGH  (0X0000U)      /* ASSERT_HIGH */
#define ENUM_DIGIO_PINMUX_DIGIO_LINK_ST_POLARITY_ASSERT_LOW   (0X0001U)      /* ASSERT_LOW */

/* ----------------------------------------------------------------------------------------------------
          LED0_BLINK_TIME_CNTRL                                 Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_LED0_BLINK_TIME_CNTRL_LED0_OFF_N4MS              (0U)           /* Led 0 Off Blink Time. */
#define BITL_LED0_BLINK_TIME_CNTRL_LED0_OFF_N4MS              (8U)           /* Led 0 Off Blink Time. */
#define BITM_LED0_BLINK_TIME_CNTRL_LED0_OFF_N4MS              (0X00FFU)      /* Led 0 Off Blink Time. */
#define BITP_LED0_BLINK_TIME_CNTRL_LED0_ON_N4MS               (8U)           /* LED 0 On Blink Time */
#define BITL_LED0_BLINK_TIME_CNTRL_LED0_ON_N4MS               (8U)           /* LED 0 On Blink Time */
#define BITM_LED0_BLINK_TIME_CNTRL_LED0_ON_N4MS               (0XFF00U)      /* LED 0 On Blink Time */

/* ----------------------------------------------------------------------------------------------------
          LED1_BLINK_TIME_CNTRL                                 Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_LED1_BLINK_TIME_CNTRL_LED1_OFF_N4MS              (0U)           /* LED 1 Off Blink Time */
#define BITL_LED1_BLINK_TIME_CNTRL_LED1_OFF_N4MS              (8U)           /* LED 1 Off Blink Time */
#define BITM_LED1_BLINK_TIME_CNTRL_LED1_OFF_N4MS              (0X00FFU)      /* LED 1 Off Blink Time */
#define BITP_LED1_BLINK_TIME_CNTRL_LED1_ON_N4MS               (8U)           /* LED 1 On Blink Time */
#define BITL_LED1_BLINK_TIME_CNTRL_LED1_ON_N4MS               (8U)           /* LED 1 On Blink Time */
#define BITM_LED1_BLINK_TIME_CNTRL_LED1_ON_N4MS               (0XFF00U)      /* LED 1 On Blink Time */

/* ----------------------------------------------------------------------------------------------------
          LED_CNTRL                                             Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_LED_CNTRL_LED0_FUNCTION                          (0U)           /* LED 0 Pin Function */
#define BITL_LED_CNTRL_LED0_FUNCTION                          (5U)           /* LED 0 Pin Function */
#define BITM_LED_CNTRL_LED0_FUNCTION                          (0X001FU)      /* LED 0 Pin Function */
#define BITP_LED_CNTRL_LED0_MODE                              (5U)           /* LED 0 Mode Selection */
#define BITL_LED_CNTRL_LED0_MODE                              (1U)           /* LED 0 Mode Selection */
#define BITM_LED_CNTRL_LED0_MODE                              (0X0020U)      /* LED 0 Mode Selection */
#define BITP_LED_CNTRL_LED0_LINK_ST_QUALIFY                   (6U)           /* Qualify Certain LED 0 Options with LINK_STATUS */
#define BITL_LED_CNTRL_LED0_LINK_ST_QUALIFY                   (1U)           /* Qualify Certain LED 0 Options with LINK_STATUS */
#define BITM_LED_CNTRL_LED0_LINK_ST_QUALIFY                   (0X0040U)      /* Qualify Certain LED 0 Options with LINK_STATUS */
#define BITP_LED_CNTRL_LED0_EN                                (7U)           /* LED 0 Enable */
#define BITL_LED_CNTRL_LED0_EN                                (1U)           /* LED 0 Enable */
#define BITM_LED_CNTRL_LED0_EN                                (0X0080U)      /* LED 0 Enable */
#define BITP_LED_CNTRL_LED1_FUNCTION                          (8U)           /* LED 1 Pin Function */
#define BITL_LED_CNTRL_LED1_FUNCTION                          (5U)           /* LED 1 Pin Function */
#define BITM_LED_CNTRL_LED1_FUNCTION                          (0X1F00U)      /* LED 1 Pin Function */
#define BITP_LED_CNTRL_LED1_MODE                              (13U)          /* LED 1 Mode Selection */
#define BITL_LED_CNTRL_LED1_MODE                              (1U)           /* LED 1 Mode Selection */
#define BITM_LED_CNTRL_LED1_MODE                              (0X2000U)      /* LED 1 Mode Selection */
#define BITP_LED_CNTRL_LED1_LINK_ST_QUALIFY                   (14U)          /* Qualify Certain LED 1 Options with LINK_STATUS */
#define BITL_LED_CNTRL_LED1_LINK_ST_QUALIFY                   (1U)           /* Qualify Certain LED 1 Options with LINK_STATUS */
#define BITM_LED_CNTRL_LED1_LINK_ST_QUALIFY                   (0X4000U)      /* Qualify Certain LED 1 Options with LINK_STATUS */
#define BITP_LED_CNTRL_LED1_EN                                (15U)          /* LED 1 Enable */
#define BITL_LED_CNTRL_LED1_EN                                (1U)           /* LED 1 Enable */
#define BITM_LED_CNTRL_LED1_EN                                (0X8000U)      /* LED 1 Enable */

#define ENUM_LED_CNTRL_LED1_MODE_LED_MODE1                    (0X0000U)      /* LED_MODE1 */
#define ENUM_LED_CNTRL_LED1_MODE_LED_MODE2                    (0X0001U)      /* LED_MODE2 */
#define ENUM_LED_CNTRL_LED1_FUNCTION_LINKUP_TXRX_ACTIVITY     (0X0000U)      /* LINKUP_TXRX_ACTIVITY */
#define ENUM_LED_CNTRL_LED1_FUNCTION_LINKUP_TX_ACTIVITY       (0X0001U)      /* LINKUP_TX_ACTIVITY */
#define ENUM_LED_CNTRL_LED1_FUNCTION_LINKUP_RX_ACTIVITY       (0X0002U)      /* LINKUP_RX_ACTIVITY */
#define ENUM_LED_CNTRL_LED1_FUNCTION_LINKUP_ONLY              (0X0003U)      /* LINKUP_ONLY */
#define ENUM_LED_CNTRL_LED1_FUNCTION_TXRX_ACTIVITY            (0X0004U)      /* TXRX_ACTIVITY */
#define ENUM_LED_CNTRL_LED1_FUNCTION_TX_ACTIVITY              (0X0005U)      /* TX_ACTIVITY */
#define ENUM_LED_CNTRL_LED1_FUNCTION_RX_ACTIVITY              (0X0006U)      /* RX_ACTIVITY */
#define ENUM_LED_CNTRL_LED1_FUNCTION_LINKUP_RX_ER             (0X0007U)      /* LINKUP_RX_ER */
#define ENUM_LED_CNTRL_LED1_FUNCTION_LINKUP_RX_TX_ER          (0X0008U)      /* LINKUP_RX_TX_ER */
#define ENUM_LED_CNTRL_LED1_FUNCTION_RX_ER                    (0X0009U)      /* RX_ER */
#define ENUM_LED_CNTRL_LED1_FUNCTION_RX_TX_ER                 (0X000AU)      /* RX_TX_ER */
#define ENUM_LED_CNTRL_LED1_FUNCTION_TX_SOP                   (0X000BU)      /* TX_SOP */
#define ENUM_LED_CNTRL_LED1_FUNCTION_RX_SOP                   (0X000CU)      /* RX_SOP */
#define ENUM_LED_CNTRL_LED1_FUNCTION_ON                       (0X000DU)      /* ON */
#define ENUM_LED_CNTRL_LED1_FUNCTION_OFF                      (0X000EU)      /* OFF */
#define ENUM_LED_CNTRL_LED1_FUNCTION_BLINK                    (0X000FU)      /* BLINK */
#define ENUM_LED_CNTRL_LED1_FUNCTION_TX_LEVEL_2P4             (0X0010U)      /* TX_LEVEL_2P4 */
#define ENUM_LED_CNTRL_LED1_FUNCTION_TX_LEVEL_1P0             (0X0011U)      /* TX_LEVEL_1P0 */
#define ENUM_LED_CNTRL_LED1_FUNCTION_MASTER                   (0X0012U)      /* MASTER */
#define ENUM_LED_CNTRL_LED1_FUNCTION_SLAVE                    (0X0013U)      /* SLAVE */
#define ENUM_LED_CNTRL_LED1_FUNCTION_INCOMPATIBLE_LINK_CFG    (0X0014U)      /* INCOMPATIBLE_LINK_CFG */
#define ENUM_LED_CNTRL_LED1_FUNCTION_AN_LINK_GOOD             (0X0015U)      /* AN_LINK_GOOD */
#define ENUM_LED_CNTRL_LED1_FUNCTION_AN_COMPLETE              (0X0016U)      /* AN_COMPLETE */
#define ENUM_LED_CNTRL_LED1_FUNCTION_TS_TIMER                 (0X0017U)      /* TS_TIMER */
#define ENUM_LED_CNTRL_LED1_FUNCTION_LOC_RCVR_STATUS          (0X0018U)      /* LOC_RCVR_STATUS */
#define ENUM_LED_CNTRL_LED1_FUNCTION_REM_RCVR_STATUS          (0X0019U)      /* REM_RCVR_STATUS */
#define ENUM_LED_CNTRL_LED1_FUNCTION_CLK25_REF                (0X001AU)      /* CLK25_REF */
#define ENUM_LED_CNTRL_LED1_FUNCTION_TX_TCLK                  (0X001BU)      /* TX_TCLK */
#define ENUM_LED_CNTRL_LED1_FUNCTION_CLK_120MHZ               (0X001CU)      /* CLK_120MHZ */
#define ENUM_LED_CNTRL_LED0_MODE_LED_MODE1                    (0X0000U)      /* LED_MODE1 */
#define ENUM_LED_CNTRL_LED0_MODE_LED_MODE2                    (0X0001U)      /* LED_MODE2 */
#define ENUM_LED_CNTRL_LED0_FUNCTION_LINKUP_TXRX_ACTIVITY     (0X0000U)      /* LINKUP_TXRX_ACTIVITY */
#define ENUM_LED_CNTRL_LED0_FUNCTION_LINKUP_TX_ACTIVITY       (0X0001U)      /* LINKUP_TX_ACTIVITY */
#define ENUM_LED_CNTRL_LED0_FUNCTION_LINKUP_RX_ACTIVITY       (0X0002U)      /* LINKUP_RX_ACTIVITY */
#define ENUM_LED_CNTRL_LED0_FUNCTION_LINKUP_ONLY              (0X0003U)      /* LINKUP_ONLY */
#define ENUM_LED_CNTRL_LED0_FUNCTION_TXRX_ACTIVITY            (0X0004U)      /* TXRX_ACTIVITY */
#define ENUM_LED_CNTRL_LED0_FUNCTION_TX_ACTIVITY              (0X0005U)      /* TX_ACTIVITY */
#define ENUM_LED_CNTRL_LED0_FUNCTION_RX_ACTIVITY              (0X0006U)      /* RX_ACTIVITY */
#define ENUM_LED_CNTRL_LED0_FUNCTION_LINKUP_RX_ER             (0X0007U)      /* LINKUP_RX_ER */
#define ENUM_LED_CNTRL_LED0_FUNCTION_LINKUP_RX_TX_ER          (0X0008U)      /* LINKUP_RX_TX_ER */
#define ENUM_LED_CNTRL_LED0_FUNCTION_RX_ER                    (0X0009U)      /* RX_ER */
#define ENUM_LED_CNTRL_LED0_FUNCTION_RX_TX_ER                 (0X000AU)      /* RX_TX_ER */
#define ENUM_LED_CNTRL_LED0_FUNCTION_TX_SOP                   (0X000BU)      /* TX_SOP */
#define ENUM_LED_CNTRL_LED0_FUNCTION_RX_SOP                   (0X000CU)      /* RX_SOP */
#define ENUM_LED_CNTRL_LED0_FUNCTION_ON                       (0X000DU)      /* ON */
#define ENUM_LED_CNTRL_LED0_FUNCTION_OFF                      (0X000EU)      /* OFF */
#define ENUM_LED_CNTRL_LED0_FUNCTION_BLINK                    (0X000FU)      /* BLINK */
#define ENUM_LED_CNTRL_LED0_FUNCTION_TX_LEVEL_2P4             (0X0010U)      /* TX_LEVEL_2P4 */
#define ENUM_LED_CNTRL_LED0_FUNCTION_TX_LEVEL_1P0             (0X0011U)      /* TX_LEVEL_1P0 */
#define ENUM_LED_CNTRL_LED0_FUNCTION_MASTER                   (0X0012U)      /* MASTER */
#define ENUM_LED_CNTRL_LED0_FUNCTION_SLAVE                    (0X0013U)      /* SLAVE */
#define ENUM_LED_CNTRL_LED0_FUNCTION_INCOMPATIBLE_LINK_CFG    (0X0014U)      /* INCOMPATIBLE_LINK_CFG */
#define ENUM_LED_CNTRL_LED0_FUNCTION_AN_LINK_GOOD             (0X0015U)      /* AN_LINK_GOOD */
#define ENUM_LED_CNTRL_LED0_FUNCTION_AN_COMPLETE              (0X0016U)      /* AN_COMPLETE */
#define ENUM_LED_CNTRL_LED0_FUNCTION_TS_TIMER                 (0X0017U)      /* TS_TIMER */
#define ENUM_LED_CNTRL_LED0_FUNCTION_LOC_RCVR_STATUS          (0X0018U)      /* LOC_RCVR_STATUS */
#define ENUM_LED_CNTRL_LED0_FUNCTION_REM_RCVR_STATUS          (0X0019U)      /* REM_RCVR_STATUS */
#define ENUM_LED_CNTRL_LED0_FUNCTION_CLK25_REF                (0X001AU)      /* CLK25_REF */
#define ENUM_LED_CNTRL_LED0_FUNCTION_TX_TCLK                  (0X001BU)      /* TX_TCLK */
#define ENUM_LED_CNTRL_LED0_FUNCTION_CLK_120MHZ               (0X001CU)      /* CLK_120MHZ */

/* ----------------------------------------------------------------------------------------------------
          LED_POLARITY                                          Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_LED_POLARITY_LED0_POLARITY                       (0U)           /* LED 0 Polarity */
#define BITL_LED_POLARITY_LED0_POLARITY                       (2U)           /* LED 0 Polarity */
#define BITM_LED_POLARITY_LED0_POLARITY                       (0X0003U)      /* LED 0 Polarity */
#define BITP_LED_POLARITY_LED1_POLARITY                       (2U)           /* LED 1 Polarity */
#define BITL_LED_POLARITY_LED1_POLARITY                       (2U)           /* LED 1 Polarity */
#define BITM_LED_POLARITY_LED1_POLARITY                       (0X000CU)      /* LED 1 Polarity */

#define ENUM_LED_POLARITY_LED1_POLARITY_LED_AUTO_SENSE        (0X0000U)      /* LED Auto Sense */
#define ENUM_LED_POLARITY_LED1_POLARITY_LED_ACTIVE_HI         (0X0001U)      /* LED Active High */
#define ENUM_LED_POLARITY_LED1_POLARITY_LED_ACTIVE_LO         (0X0002U)      /* LED Active Low */
#define ENUM_LED_POLARITY_LED0_POLARITY_LED_AUTO_SENSE        (0X0000U)      /* LED Auto Sense */
#define ENUM_LED_POLARITY_LED0_POLARITY_LED_ACTIVE_HI         (0X0001U)      /* LED Active High */
#define ENUM_LED_POLARITY_LED0_POLARITY_LED_ACTIVE_LO         (0X0002U)      /* LED Active Low */

/* ----------------------------------------------------------------------------------------------------
          MMD2_DEV_ID1                                          Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MMD2_DEV_ID1_MMD2_DEV_ID1                        (0U)           /* Vendor specific 2 MMD device identifier */
#define BITL_MMD2_DEV_ID1_MMD2_DEV_ID1                        (16U)          /* Vendor specific 2 MMD device identifier */
#define BITM_MMD2_DEV_ID1_MMD2_DEV_ID1                        (0XFFFFU)      /* Vendor specific 2 MMD device identifier */

/* ----------------------------------------------------------------------------------------------------
          MMD2_DEV_ID2                                          Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MMD2_DEV_ID2_MMD2_REV_NUM                        (0U)           /* Revision number */
#define BITL_MMD2_DEV_ID2_MMD2_REV_NUM                        (4U)           /* Revision number */
#define BITM_MMD2_DEV_ID2_MMD2_REV_NUM                        (0X000FU)      /* Revision number */
#define BITP_MMD2_DEV_ID2_MMD2_MODEL_NUM                      (4U)           /* Model Number */
#define BITL_MMD2_DEV_ID2_MMD2_MODEL_NUM                      (6U)           /* Model Number */
#define BITM_MMD2_DEV_ID2_MMD2_MODEL_NUM                      (0X03F0U)      /* Model Number */
#define BITP_MMD2_DEV_ID2_MMD2_DEV_ID2_OUI                    (10U)          /* OUI bits */
#define BITL_MMD2_DEV_ID2_MMD2_DEV_ID2_OUI                    (6U)           /* OUI bits */
#define BITM_MMD2_DEV_ID2_MMD2_DEV_ID2_OUI                    (0XFC00U)      /* OUI bits */

/* ----------------------------------------------------------------------------------------------------
          MMD2_DEVS_IN_PKG1                                     Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MMD2_DEVS_IN_PKG1_MMD2_DEVS_IN_PKG1              (0U)           /* Vendor Specific 2 MMD Devices in Package */
#define BITL_MMD2_DEVS_IN_PKG1_MMD2_DEVS_IN_PKG1              (16U)          /* Vendor Specific 2 MMD Devices in Package */
#define BITM_MMD2_DEVS_IN_PKG1_MMD2_DEVS_IN_PKG1              (0XFFFFU)      /* Vendor Specific 2 MMD Devices in Package */

/* ----------------------------------------------------------------------------------------------------
          MMD2_DEVS_IN_PKG2                                     Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MMD2_DEVS_IN_PKG2_MMD2_DEVS_IN_PKG2              (0U)           /* Vendor Specific 2 MMD Devices in Package */
#define BITL_MMD2_DEVS_IN_PKG2_MMD2_DEVS_IN_PKG2              (16U)          /* Vendor Specific 2 MMD Devices in Package */
#define BITM_MMD2_DEVS_IN_PKG2_MMD2_DEVS_IN_PKG2              (0XFFFFU)      /* Vendor Specific 2 MMD Devices in Package */

/* ----------------------------------------------------------------------------------------------------
          MMD2_STATUS                                           Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MMD2_STATUS_MMD2_STATUS                          (14U)          /* Vendor specific 2 MMD Status */
#define BITL_MMD2_STATUS_MMD2_STATUS                          (2U)           /* Vendor specific 2 MMD Status */
#define BITM_MMD2_STATUS_MMD2_STATUS                          (0XC000U)      /* Vendor specific 2 MMD Status */

/* ----------------------------------------------------------------------------------------------------
          PHY_SUBSYS_IRQ_STATUS                                 Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_PHY_SUBSYS_IRQ_STATUS_LINK_STAT_CHNG_LH          (1U)           /* Link Status Change. */
#define BITL_PHY_SUBSYS_IRQ_STATUS_LINK_STAT_CHNG_LH          (1U)           /* Link Status Change. */
#define BITM_PHY_SUBSYS_IRQ_STATUS_LINK_STAT_CHNG_LH          (0X0002U)      /* Link Status Change. */
#define BITP_PHY_SUBSYS_IRQ_STATUS_AN_STAT_CHNG_IRQ_LH        (11U)          /* Autonegotiation Status Change Interrupt. */
#define BITL_PHY_SUBSYS_IRQ_STATUS_AN_STAT_CHNG_IRQ_LH        (1U)           /* Autonegotiation Status Change Interrupt. */
#define BITM_PHY_SUBSYS_IRQ_STATUS_AN_STAT_CHNG_IRQ_LH        (0X0800U)      /* Autonegotiation Status Change Interrupt. */
#define BITP_PHY_SUBSYS_IRQ_STATUS_MAC_IF_EBUF_ERR_IRQ_LH     (13U)          /* Mac Interface Buffers Overflow/underflow Interrupt */
#define BITL_PHY_SUBSYS_IRQ_STATUS_MAC_IF_EBUF_ERR_IRQ_LH     (1U)           /* Mac Interface Buffers Overflow/underflow Interrupt */
#define BITM_PHY_SUBSYS_IRQ_STATUS_MAC_IF_EBUF_ERR_IRQ_LH     (0X2000U)      /* Mac Interface Buffers Overflow/underflow Interrupt */
#define BITP_PHY_SUBSYS_IRQ_STATUS_MAC_IF_FC_FG_IRQ_LH        (14U)          /* Mac Interface Frame CHECKER/_GENERATOR Interrupt */
#define BITL_PHY_SUBSYS_IRQ_STATUS_MAC_IF_FC_FG_IRQ_LH        (1U)           /* Mac Interface Frame CHECKER/_GENERATOR Interrupt */
#define BITM_PHY_SUBSYS_IRQ_STATUS_MAC_IF_FC_FG_IRQ_LH        (0X4000U)      /* Mac Interface Frame CHECKER/_GENERATOR Interrupt */

/* ----------------------------------------------------------------------------------------------------
          PHY_SUBSYS_IRQ_MASK                                   Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_PHY_SUBSYS_IRQ_MASK_LINK_STAT_CHNG_IRQ_EN        (1U)           /* Enable Link Status Change Interrupt. */
#define BITL_PHY_SUBSYS_IRQ_MASK_LINK_STAT_CHNG_IRQ_EN        (1U)           /* Enable Link Status Change Interrupt. */
#define BITM_PHY_SUBSYS_IRQ_MASK_LINK_STAT_CHNG_IRQ_EN        (0X0002U)      /* Enable Link Status Change Interrupt. */
#define BITP_PHY_SUBSYS_IRQ_MASK_AN_STAT_CHNG_IRQ_EN          (11U)          /* Enable Autonegotiation Status Change Interrupt. */
#define BITL_PHY_SUBSYS_IRQ_MASK_AN_STAT_CHNG_IRQ_EN          (1U)           /* Enable Autonegotiation Status Change Interrupt. */
#define BITM_PHY_SUBSYS_IRQ_MASK_AN_STAT_CHNG_IRQ_EN          (0X0800U)      /* Enable Autonegotiation Status Change Interrupt. */
#define BITP_PHY_SUBSYS_IRQ_MASK_MAC_IF_EBUF_ERR_IRQ_EN       (13U)          /* Enable Mac Interface Buffers Overflow/underflow Interrupt */
#define BITL_PHY_SUBSYS_IRQ_MASK_MAC_IF_EBUF_ERR_IRQ_EN       (1U)           /* Enable Mac Interface Buffers Overflow/underflow Interrupt */
#define BITM_PHY_SUBSYS_IRQ_MASK_MAC_IF_EBUF_ERR_IRQ_EN       (0X2000U)      /* Enable Mac Interface Buffers Overflow/underflow Interrupt */
#define BITP_PHY_SUBSYS_IRQ_MASK_MAC_IF_FC_FG_IRQ_EN          (14U)          /* Enable Mac Interface Frame CHECKER/_GENERATOR Interrupt */
#define BITL_PHY_SUBSYS_IRQ_MASK_MAC_IF_FC_FG_IRQ_EN          (1U)           /* Enable Mac Interface Frame CHECKER/_GENERATOR Interrupt */
#define BITM_PHY_SUBSYS_IRQ_MASK_MAC_IF_FC_FG_IRQ_EN          (0X4000U)      /* Enable Mac Interface Frame CHECKER/_GENERATOR Interrupt */

/* ----------------------------------------------------------------------------------------------------
          FC_EN                                                 Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_FC_EN_FC_EN                                      (0U)           /* Frame Checker Enable */
#define BITL_FC_EN_FC_EN                                      (1U)           /* Frame Checker Enable */
#define BITM_FC_EN_FC_EN                                      (0X0001U)      /* Frame Checker Enable */

/* ----------------------------------------------------------------------------------------------------
          FC_IRQ_EN                                             Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_FC_IRQ_EN_FC_IRQ_EN                              (0U)           /* Frame Checker Interrupt Enable */
#define BITL_FC_IRQ_EN_FC_IRQ_EN                              (1U)           /* Frame Checker Interrupt Enable */
#define BITM_FC_IRQ_EN_FC_IRQ_EN                              (0X0001U)      /* Frame Checker Interrupt Enable */

/* ----------------------------------------------------------------------------------------------------
          FC_TX_SEL                                             Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_FC_TX_SEL_FC_TX_SEL                              (0U)           /* Frame Checker Transmit Select */
#define BITL_FC_TX_SEL_FC_TX_SEL                              (1U)           /* Frame Checker Transmit Select */
#define BITM_FC_TX_SEL_FC_TX_SEL                              (0X0001U)      /* Frame Checker Transmit Select */

/* ----------------------------------------------------------------------------------------------------
          RX_ERR_CNT                                            Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_RX_ERR_CNT_RX_ERR_CNT                            (0U)           /* Receive Error Count */
#define BITL_RX_ERR_CNT_RX_ERR_CNT                            (16U)          /* Receive Error Count */
#define BITM_RX_ERR_CNT_RX_ERR_CNT                            (0XFFFFU)      /* Receive Error Count */

/* ----------------------------------------------------------------------------------------------------
          FC_FRM_CNT_H                                          Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_FC_FRM_CNT_H_FC_FRM_CNT_H                        (0U)           /* Bits [31:16] of Latched Copy of the Number of Received Frames */
#define BITL_FC_FRM_CNT_H_FC_FRM_CNT_H                        (16U)          /* Bits [31:16] of Latched Copy of the Number of Received Frames */
#define BITM_FC_FRM_CNT_H_FC_FRM_CNT_H                        (0XFFFFU)      /* Bits [31:16] of Latched Copy of the Number of Received Frames */

/* ----------------------------------------------------------------------------------------------------
          FC_FRM_CNT_L                                          Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_FC_FRM_CNT_L_FC_FRM_CNT_L                        (0U)           /* Bits [15:0] of Latched Copy of the Number of Received Frames */
#define BITL_FC_FRM_CNT_L_FC_FRM_CNT_L                        (16U)          /* Bits [15:0] of Latched Copy of the Number of Received Frames */
#define BITM_FC_FRM_CNT_L_FC_FRM_CNT_L                        (0XFFFFU)      /* Bits [15:0] of Latched Copy of the Number of Received Frames */

/* ----------------------------------------------------------------------------------------------------
          FC_LEN_ERR_CNT                                        Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_FC_LEN_ERR_CNT_FC_LEN_ERR_CNT                    (0U)           /* Latched Copy of the Frame Length Error Counter */
#define BITL_FC_LEN_ERR_CNT_FC_LEN_ERR_CNT                    (16U)          /* Latched Copy of the Frame Length Error Counter */
#define BITM_FC_LEN_ERR_CNT_FC_LEN_ERR_CNT                    (0XFFFFU)      /* Latched Copy of the Frame Length Error Counter */

/* ----------------------------------------------------------------------------------------------------
          FC_ALGN_ERR_CNT                                       Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_FC_ALGN_ERR_CNT_FC_ALGN_ERR_CNT                  (0U)           /* Latched Copy of the Frame Alignment Error Counter */
#define BITL_FC_ALGN_ERR_CNT_FC_ALGN_ERR_CNT                  (16U)          /* Latched Copy of the Frame Alignment Error Counter */
#define BITM_FC_ALGN_ERR_CNT_FC_ALGN_ERR_CNT                  (0XFFFFU)      /* Latched Copy of the Frame Alignment Error Counter */

/* ----------------------------------------------------------------------------------------------------
          FC_SYMB_ERR_CNT                                       Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_FC_SYMB_ERR_CNT_FC_SYMB_ERR_CNT                  (0U)           /* Latched Copy of the Symbol Error Counter */
#define BITL_FC_SYMB_ERR_CNT_FC_SYMB_ERR_CNT                  (16U)          /* Latched Copy of the Symbol Error Counter */
#define BITM_FC_SYMB_ERR_CNT_FC_SYMB_ERR_CNT                  (0XFFFFU)      /* Latched Copy of the Symbol Error Counter */

/* ----------------------------------------------------------------------------------------------------
          FC_OSZ_CNT                                            Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_FC_OSZ_CNT_FC_OSZ_CNT                            (0U)           /* Latched copy of the Overisized Frame Error Counter */
#define BITL_FC_OSZ_CNT_FC_OSZ_CNT                            (16U)          /* Latched copy of the Overisized Frame Error Counter */
#define BITM_FC_OSZ_CNT_FC_OSZ_CNT                            (0XFFFFU)      /* Latched copy of the Overisized Frame Error Counter */

/* ----------------------------------------------------------------------------------------------------
          FC_USZ_CNT                                            Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_FC_USZ_CNT_FC_USZ_CNT                            (0U)           /* Latched Copy of the Undersized Frame Error Counter */
#define BITL_FC_USZ_CNT_FC_USZ_CNT                            (16U)          /* Latched Copy of the Undersized Frame Error Counter */
#define BITM_FC_USZ_CNT_FC_USZ_CNT                            (0XFFFFU)      /* Latched Copy of the Undersized Frame Error Counter */

/* ----------------------------------------------------------------------------------------------------
          FC_ODD_CNT                                            Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_FC_ODD_CNT_FC_ODD_CNT                            (0U)           /* Latched Copy of the Odd Nibble Counter */
#define BITL_FC_ODD_CNT_FC_ODD_CNT                            (16U)          /* Latched Copy of the Odd Nibble Counter */
#define BITM_FC_ODD_CNT_FC_ODD_CNT                            (0XFFFFU)      /* Latched Copy of the Odd Nibble Counter */

/* ----------------------------------------------------------------------------------------------------
          FC_ODD_PRE_CNT                                        Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_FC_ODD_PRE_CNT_FC_ODD_PRE_CNT                    (0U)           /* Latched Copy of the Odd Preamble Packet Counter */
#define BITL_FC_ODD_PRE_CNT_FC_ODD_PRE_CNT                    (16U)          /* Latched Copy of the Odd Preamble Packet Counter */
#define BITM_FC_ODD_PRE_CNT_FC_ODD_PRE_CNT                    (0XFFFFU)      /* Latched Copy of the Odd Preamble Packet Counter */

/* ----------------------------------------------------------------------------------------------------
          FC_FALSE_CARRIER_CNT                                  Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_FC_FALSE_CARRIER_CNT_FC_FALSE_CARRIER_CNT        (0U)           /* Latched Copy of the False Carrier Events Counter */
#define BITL_FC_FALSE_CARRIER_CNT_FC_FALSE_CARRIER_CNT        (16U)          /* Latched Copy of the False Carrier Events Counter */
#define BITM_FC_FALSE_CARRIER_CNT_FC_FALSE_CARRIER_CNT        (0XFFFFU)      /* Latched Copy of the False Carrier Events Counter */

/* ----------------------------------------------------------------------------------------------------
          FG_EN                                                 Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_FG_EN_FG_EN                                      (0U)           /* Frame Generator Enable */
#define BITL_FG_EN_FG_EN                                      (1U)           /* Frame Generator Enable */
#define BITM_FG_EN_FG_EN                                      (0X0001U)      /* Frame Generator Enable */

/* ----------------------------------------------------------------------------------------------------
          FG_CNTRL_RSTRT                                        Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_FG_CNTRL_RSTRT_FG_CNTRL                          (0U)           /* Frame Generator Control */
#define BITL_FG_CNTRL_RSTRT_FG_CNTRL                          (3U)           /* Frame Generator Control */
#define BITM_FG_CNTRL_RSTRT_FG_CNTRL                          (0X0007U)      /* Frame Generator Control */
#define BITP_FG_CNTRL_RSTRT_FG_RSTRT                          (3U)           /* Frame Generator Restart */
#define BITL_FG_CNTRL_RSTRT_FG_RSTRT                          (1U)           /* Frame Generator Restart */
#define BITM_FG_CNTRL_RSTRT_FG_RSTRT                          (0X0008U)      /* Frame Generator Restart */

#define ENUM_FG_CNTRL_RSTRT_FG_CNTRL_FG_GEN_NONE              (0X0000U)      /* No description provided */
#define ENUM_FG_CNTRL_RSTRT_FG_CNTRL_FG_GEN_RANDOM_PAYLOAD    (0X0001U)      /* No description provided */
#define ENUM_FG_CNTRL_RSTRT_FG_CNTRL_FG_GEN_0X00_PAYLOAD      (0X0002U)      /* No description provided */
#define ENUM_FG_CNTRL_RSTRT_FG_CNTRL_FG_GEN_0XFF_PAYLOAD      (0X0003U)      /* No description provided */
#define ENUM_FG_CNTRL_RSTRT_FG_CNTRL_FG_GEN_0X55_PAYLOAD      (0X0004U)      /* No description provided */
#define ENUM_FG_CNTRL_RSTRT_FG_CNTRL_FG_GEN_DECR_PAYLOAD      (0X0005U)      /* No description provided */

/* ----------------------------------------------------------------------------------------------------
          FG_CONT_MODE_EN                                       Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_FG_CONT_MODE_EN_FG_CONT_MODE_EN                  (0U)           /* Frame Generator Continuous Mode Enable */
#define BITL_FG_CONT_MODE_EN_FG_CONT_MODE_EN                  (1U)           /* Frame Generator Continuous Mode Enable */
#define BITM_FG_CONT_MODE_EN_FG_CONT_MODE_EN                  (0X0001U)      /* Frame Generator Continuous Mode Enable */

/* ----------------------------------------------------------------------------------------------------
          FG_IRQ_EN                                             Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_FG_IRQ_EN_FG_IRQ_EN                              (0U)           /* Frame Generator Interrupt Enable */
#define BITL_FG_IRQ_EN_FG_IRQ_EN                              (1U)           /* Frame Generator Interrupt Enable */
#define BITM_FG_IRQ_EN_FG_IRQ_EN                              (0X0001U)      /* Frame Generator Interrupt Enable */

/* ----------------------------------------------------------------------------------------------------
          FG_FRM_LEN                                            Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_FG_FRM_LEN_FG_FRM_LEN                            (0U)           /* The Data Field Frame Length in Bytes */
#define BITL_FG_FRM_LEN_FG_FRM_LEN                            (16U)          /* The Data Field Frame Length in Bytes */
#define BITM_FG_FRM_LEN_FG_FRM_LEN                            (0XFFFFU)      /* The Data Field Frame Length in Bytes */

/* ----------------------------------------------------------------------------------------------------
          FG_IFG_LEN                                            Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_FG_IFG_LEN_FG_IFG_LEN                            (0U)           /* Frame Generator Inter-frame Gap Length */
#define BITL_FG_IFG_LEN_FG_IFG_LEN                            (16U)          /* Frame Generator Inter-frame Gap Length */
#define BITM_FG_IFG_LEN_FG_IFG_LEN                            (0XFFFFU)      /* Frame Generator Inter-frame Gap Length */

/* ----------------------------------------------------------------------------------------------------
          FG_NFRM_H                                             Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_FG_NFRM_H_FG_NFRM_H                              (0U)           /* Bits [31:16] of the Number of Frames to be Generated */
#define BITL_FG_NFRM_H_FG_NFRM_H                              (16U)          /* Bits [31:16] of the Number of Frames to be Generated */
#define BITM_FG_NFRM_H_FG_NFRM_H                              (0XFFFFU)      /* Bits [31:16] of the Number of Frames to be Generated */

/* ----------------------------------------------------------------------------------------------------
          FG_NFRM_L                                             Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_FG_NFRM_L_FG_NFRM_L                              (0U)           /* Bits [15:0] of the Number of Frames to be Generated */
#define BITL_FG_NFRM_L_FG_NFRM_L                              (16U)          /* Bits [15:0] of the Number of Frames to be Generated */
#define BITM_FG_NFRM_L_FG_NFRM_L                              (0XFFFFU)      /* Bits [15:0] of the Number of Frames to be Generated */

/* ----------------------------------------------------------------------------------------------------
          FG_DONE                                               Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_FG_DONE_FG_DONE                                  (0U)           /* Frame Generator Done */
#define BITL_FG_DONE_FG_DONE                                  (1U)           /* Frame Generator Done */
#define BITM_FG_DONE_FG_DONE                                  (0X0001U)      /* Frame Generator Done */

/* ----------------------------------------------------------------------------------------------------
          MAC_IF_LOOPBACK                                       Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_IF_LOOPBACK_MAC_IF_LB_EN                     (0U)           /* MAC Interface Loopback Enable. */
#define BITL_MAC_IF_LOOPBACK_MAC_IF_LB_EN                     (1U)           /* MAC Interface Loopback Enable. */
#define BITM_MAC_IF_LOOPBACK_MAC_IF_LB_EN                     (0X0001U)      /* MAC Interface Loopback Enable. */
#define BITP_MAC_IF_LOOPBACK_MAC_IF_LB_TX_SUP_EN              (1U)           /* Suppress Transmission Enable. */
#define BITL_MAC_IF_LOOPBACK_MAC_IF_LB_TX_SUP_EN              (1U)           /* Suppress Transmission Enable. */
#define BITM_MAC_IF_LOOPBACK_MAC_IF_LB_TX_SUP_EN              (0X0002U)      /* Suppress Transmission Enable. */
#define BITP_MAC_IF_LOOPBACK_MAC_IF_REM_LB_EN                 (2U)           /* MAC Interface Remote Loopback Enable. */
#define BITL_MAC_IF_LOOPBACK_MAC_IF_REM_LB_EN                 (1U)           /* MAC Interface Remote Loopback Enable. */
#define BITM_MAC_IF_LOOPBACK_MAC_IF_REM_LB_EN                 (0X0004U)      /* MAC Interface Remote Loopback Enable. */
#define BITP_MAC_IF_LOOPBACK_MAC_IF_REM_LB_RX_SUP_EN          (3U)           /* Suppress RX Enable */
#define BITL_MAC_IF_LOOPBACK_MAC_IF_REM_LB_RX_SUP_EN          (1U)           /* Suppress RX Enable */
#define BITM_MAC_IF_LOOPBACK_MAC_IF_REM_LB_RX_SUP_EN          (0X0008U)      /* Suppress RX Enable */

/* ----------------------------------------------------------------------------------------------------
          MAC_IF_SOP_CNTRL                                      Value             Description
   ---------------------------------------------------------------------------------------------------- */
#define BITP_MAC_IF_SOP_CNTRL_MAC_IF_RX_SOP_DET_EN            (0U)           /* Enable the Generation of the Rx SOP Indication Signal. */
#define BITL_MAC_IF_SOP_CNTRL_MAC_IF_RX_SOP_DET_EN            (1U)           /* Enable the Generation of the Rx SOP Indication Signal. */
#define BITM_MAC_IF_SOP_CNTRL_MAC_IF_RX_SOP_DET_EN            (0X0001U)      /* Enable the Generation of the Rx SOP Indication Signal. */
#define BITP_MAC_IF_SOP_CNTRL_MAC_IF_RX_SOP_SFD_EN            (1U)           /* Enable Rx SOP Signal Indication on SFD Reception. */
#define BITL_MAC_IF_SOP_CNTRL_MAC_IF_RX_SOP_SFD_EN            (1U)           /* Enable Rx SOP Signal Indication on SFD Reception. */
#define BITM_MAC_IF_SOP_CNTRL_MAC_IF_RX_SOP_SFD_EN            (0X0002U)      /* Enable Rx SOP Signal Indication on SFD Reception. */
#define BITP_MAC_IF_SOP_CNTRL_MAC_IF_RX_SOP_LEN_CHK_EN        (2U)           /* Enable Rx SOP Preamble Length Check. */
#define BITL_MAC_IF_SOP_CNTRL_MAC_IF_RX_SOP_LEN_CHK_EN        (1U)           /* Enable Rx SOP Preamble Length Check. */
#define BITM_MAC_IF_SOP_CNTRL_MAC_IF_RX_SOP_LEN_CHK_EN        (0X0004U)      /* Enable Rx SOP Preamble Length Check. */
#define BITP_MAC_IF_SOP_CNTRL_MAC_IF_TX_SOP_DET_EN            (3U)           /* Enable the Generation of the Tx SOP Indication Signal. */
#define BITL_MAC_IF_SOP_CNTRL_MAC_IF_TX_SOP_DET_EN            (1U)           /* Enable the Generation of the Tx SOP Indication Signal. */
#define BITM_MAC_IF_SOP_CNTRL_MAC_IF_TX_SOP_DET_EN            (0X0008U)      /* Enable the Generation of the Tx SOP Indication Signal. */
#define BITP_MAC_IF_SOP_CNTRL_MAC_IF_TX_SOP_SFD_EN            (4U)           /* Enable Tx SOP Signal Indication on SFD. */
#define BITL_MAC_IF_SOP_CNTRL_MAC_IF_TX_SOP_SFD_EN            (1U)           /* Enable Tx SOP Signal Indication on SFD. */
#define BITM_MAC_IF_SOP_CNTRL_MAC_IF_TX_SOP_SFD_EN            (0X0010U)      /* Enable Tx SOP Signal Indication on SFD. */
#define BITP_MAC_IF_SOP_CNTRL_MAC_IF_TX_SOP_LEN_CHK_EN        (5U)           /* Enable Tx SOP Preamble Length Check. */
#define BITL_MAC_IF_SOP_CNTRL_MAC_IF_TX_SOP_LEN_CHK_EN        (1U)           /* Enable Tx SOP Preamble Length Check. */
#define BITM_MAC_IF_SOP_CNTRL_MAC_IF_TX_SOP_LEN_CHK_EN        (0X0020U)      /* Enable Tx SOP Preamble Length Check. */


/** @defgroup PMA_PMD_CNTRL1 PMA/PMD Control 1 Register (PMA_PMD_CNTRL1) Register
 *  PMA/PMD Control 1 Register (PMA_PMD_CNTRL1) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_PMA_PMD_CNTRL1_t
 *  \brief PMA/PMD Control 1 Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_PMA_PMD_CNTRL1_t {
    union {
        struct {
             uint16_t LB_PMA_LOC_EN    : 1; /**< Enables PMA Local Loopback. */
             uint16_t RESERVED1        : 10; /**< Reserved */
             uint16_t PMA_SFT_PD       : 1; /**< PMA Software Power-down. */
             uint16_t RESERVED12       : 3; /**< Reserved */
             uint16_t PMA_SFT_RST      : 1; /**< PMA Software Reset. */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_PMA_PMD_CNTRL1_t;

/*@}*/

/** @defgroup PMA_PMD_STAT1 PMA/PMD Status 1 Register (PMA_PMD_STAT1) Register
 *  PMA/PMD Status 1 Register (PMA_PMD_STAT1) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_PMA_PMD_STAT1_t
 *  \brief PMA/PMD Status 1 Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_PMA_PMD_STAT1_t {
    union {
        struct {
             uint16_t RESERVED0        : 1; /**< Reserved */
             uint16_t PMA_SFT_PD_ABLE  : 1; /**< PMA Software Powerdown Able */
             uint16_t PMA_LINK_STAT_OK_LL : 1; /**< PMA Link Status. */
             uint16_t RESERVED3        : 13; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_PMA_PMD_STAT1_t;

/*@}*/

/** @defgroup PMA_PMD_DEVS_IN_PKG1 PMA/PMD MMD Devices in Package 1 (PMA_PMD_DEVS_IN_PKG1) Register
 *  PMA/PMD MMD Devices in Package 1 (PMA_PMD_DEVS_IN_PKG1) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_PMA_PMD_DEVS_IN_PKG1_t
 *  \brief PMA/PMD MMD Devices in Package 1
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_PMA_PMD_DEVS_IN_PKG1_t {
    union {
        struct {
             uint16_t PMA_PMD_DEVS_IN_PKG1 : 16; /**< PMA/PMD MMD Devices in Package */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_PMA_PMD_DEVS_IN_PKG1_t;

/*@}*/

/** @defgroup PMA_PMD_DEVS_IN_PKG2 PMA/PMD MMD Devices in Package 2 Register (PMA_PMD_DEVS_IN_PKG2) Register
 *  PMA/PMD MMD Devices in Package 2 Register (PMA_PMD_DEVS_IN_PKG2) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_PMA_PMD_DEVS_IN_PKG2_t
 *  \brief PMA/PMD MMD Devices in Package 2 Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_PMA_PMD_DEVS_IN_PKG2_t {
    union {
        struct {
             uint16_t PMA_PMD_DEVS_IN_PKG2 : 16; /**< PMA/PMD MMD Devices in Package */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_PMA_PMD_DEVS_IN_PKG2_t;

/*@}*/

/** @defgroup PMA_PMD_CNTRL2 PMA/PMD Control 2 Register (PMA_PMD_CNTRL2) Register
 *  PMA/PMD Control 2 Register (PMA_PMD_CNTRL2) Register
 *  @{
 */

/*! ========================================================================
 *  \enum ADI_ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL
 *  \brief PMA/PMD Type Selection
 *  ======================================================================== */
typedef enum
{
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_CX4_PMA_PMD = 0,    /**< TS_10GBASE_CX4_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_EW_PMA_PMD = 1,    /**< TS_10GBASE_EW_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_LW_PMA_PMD = 2,    /**< TS_10GBASE_LW_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_SW_PMA_PMD = 3,    /**< TS_10GBASE_SW_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_LX4_PMA_PMD = 4,    /**< TS_10GBASE_LX4_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_ER_PMA_PMD = 5,    /**< TS_10GBASE_ER_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_LR_PMA_PMD = 6,    /**< TS_10GBASE_LR_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_SR_PMA_PMD = 7,    /**< TS_10GBASE_SR_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_LRM_PMA_PMD = 8,    /**< TS_10GBASE_LRM_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_T_PMA = 9,    /**< TS_10GBASE_T_PMA */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_KX4_PMA_PMD = 10,    /**< TS_10GBASE_KX4_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_KR_PMA_PMD = 11,    /**< TS_10GBASE_KR_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_1000BASE_T_PMA_PMD = 12,    /**< TS_1000BASE_T_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_1000BASE_KX_PMA_PMD = 13,    /**< TS_1000BASE_KX_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_100BASE_TX_PMA_PMD = 14,    /**< TS_100BASE_TX_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10BASE_T_PMA_PMD = 15,    /**< TS_10BASE_T_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10_1GBASE_PRX_D1 = 16,    /**< TS_10_1GBASE_PRX_D1 */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10_1GBASE_PRX_D2 = 17,    /**< TS_10_1GBASE_PRX_D2 */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10_1GBASE_PRX_D3 = 18,    /**< TS_10_1GBASE_PRX_D3 */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_PR_D1 = 19,    /**< TS_10GBASE_PR_D1 */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_PR_D2 = 20,    /**< TS_10GBASE_PR_D2 */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_PR_D3 = 21,    /**< TS_10GBASE_PR_D3 */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10_1GBASE_PRX_U1 = 22,    /**< TS_10_1GBASE_PRX_U1 */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10_1GBASE_PRX_U2 = 23,    /**< TS_10_1GBASE_PRX_U2 */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10_1GBASE_PRX_U3 = 24,    /**< TS_10_1GBASE_PRX_U3 */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_PR_U1 = 25,    /**< TS_10GBASE_PR_U1 */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_PR_U3 = 26,    /**< TS_10GBASE_PR_U3 */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_RESERVED = 27,    /**< TS_RESERVED */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_PR_D4 = 28,    /**< TS_10GBASE_PR_D4 */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10_1GBASE_PRX_D4 = 29,    /**< TS_10_1GBASE_PRX_D4 */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GBASE_PR_U4 = 30,    /**< TS_10GBASE_PR_U4 */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10_1GBASE_PRX_U4 = 31,    /**< TS_10_1GBASE_PRX_U4 */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_40GBASE_KR4_PMA_PMD = 32,    /**< TS_40GBASE_KR4_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_40GBASE_CR4_PMA_PMD = 33,    /**< TS_40GBASE_CR4_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_40GBASE_SR4_PMA_PMD = 34,    /**< TS_40GBASE_SR4_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_40GBASE_LR4_PMA_PMD = 35,    /**< TS_40GBASE_LR4_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_40GBASE_FR_PMA_PMD = 36,    /**< TS_40GBASE_FR_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_40GBASE_ER4_PMA_PMD = 37,    /**< TS_40GBASE_ER4_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_40GBASE_T_PMA = 38,    /**< TS_40GBASE_T_PMA */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_100GBASE_CR10_PMA_PMD = 40,    /**< TS_100GBASE_CR10_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_100GBASE_SR10_PMA_PMD = 41,    /**< TS_100GBASE_SR10_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_100GBASE_LR4_PMA_PMD = 42,    /**< TS_100GBASE_LR4_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_100GBASE_ER4_PMA_PMD = 43,    /**< TS_100GBASE_ER4_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_100GBASE_KP4_PMA_PMD = 44,    /**< TS_100GBASE_KP4_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_100GBASE_KR4_PMA_PMD = 45,    /**< TS_100GBASE_KR4_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_100GBASE_CR4_PMA_PMD = 46,    /**< TS_100GBASE_CR4_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_100GBASE_SR4_PMA_PMD = 47,    /**< TS_100GBASE_SR4_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_2_5GBASE_T_PMA = 48,    /**< TS_2_5GBASE_T_PMA */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_5GBASE_T_PMA = 49,    /**< TS_5GBASE_T_PMA */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GPASS_XR_D_PMA_PMD = 50,    /**< TS_10GPASS_XR_D_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_10GPASS_XR_U_PMA_PMD = 51,    /**< TS_10GPASS_XR_U_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_BASE_H_PMA_PMD = 52,    /**< TS_BASE_H_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_25GBASE_LR_PMA_PMD = 53,    /**< TS_25GBASE_LR_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_25GBASE_ER_PMA_PMD = 54,    /**< TS_25GBASE_ER_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_25GBASE_T_PMA = 55,    /**< TS_25GBASE_T_PMA */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_25GBASE_CR_OR_25GBASE_CR_S_PMA_PMD = 56,    /**< TS_25GBASE_CR_OR_25GBASE_CR_S_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_25GBASE_KR_OR_25GBASE_KR_S_PMA_PMD = 57,    /**< TS_25GBASE_KR_OR_25GBASE_KR_S_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_25GBASE_SR_PMA_PMD = 58,    /**< TS_25GBASE_SR_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_BASE_T1_PMA_PMD = 61,    /**< TS_BASE_T1_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_200GBASE_DR4_PMA_PMD = 83,    /**< TS_200GBASE_DR4_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_200GBASE_FR4_PMA_PMD = 84,    /**< TS_200GBASE_FR4_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_200GBASE_LR4_PMA_PMD = 85,    /**< TS_200GBASE_LR4_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_400GBASE_SR16_PMA_PMD = 89,    /**< TS_400GBASE_SR16_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_400GBASE_DR4_PMA_PMD = 90,    /**< TS_400GBASE_DR4_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_400GBASE_FR8_PMA_PMD = 91,    /**< TS_400GBASE_FR8_PMA_PMD */
     ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL_TS_400GBASE_LR8_PMA_PMD = 92     /**< TS_400GBASE_LR8_PMA_PMD */
}  ADI_ADIN1100_PMA_PMD_CNTRL2_PMA_PMD_TYPE_SEL;

/*! ========================================================================
 *  \struct ADI_ADIN1100_PMA_PMD_CNTRL2_t
 *  \brief PMA/PMD Control 2 Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_PMA_PMD_CNTRL2_t {
    union {
        struct {
             uint16_t PMA_PMD_TYPE_SEL : 7; /**< PMA/PMD Type Selection */
             uint16_t RESERVED7        : 9; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_PMA_PMD_CNTRL2_t;

/*@}*/

/** @defgroup PMA_PMD_STAT2 PMA/PMD Status 2 (PMA_PMD_STAT2) Register
 *  PMA/PMD Status 2 (PMA_PMD_STAT2) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_PMA_PMD_STAT2_t
 *  \brief PMA/PMD Status 2
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_PMA_PMD_STAT2_t {
    union {
        struct {
             uint16_t LB_PMA_LOC_ABLE  : 1; /**< PMA Local Loopback Able */
             uint16_t RESERVED1        : 7; /**< Reserved */
             uint16_t PMA_PMD_TX_DIS_ABLE : 1; /**< PMA/PMD Tx Disable */
             uint16_t PMA_PMD_EXT_ABLE : 1; /**< PHY Extended Abilities Support */
             uint16_t RESERVED10       : 4; /**< Reserved */
             uint16_t PMA_PMD_PRESENT  : 2; /**< PMA/PMD Present */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_PMA_PMD_STAT2_t;

/*@}*/

/** @defgroup PMA_PMD_TX_DIS PMA/PMD Transmit Disable Register (PMA_PMD_TX_DIS) Register
 *  PMA/PMD Transmit Disable Register (PMA_PMD_TX_DIS) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_PMA_PMD_TX_DIS_t
 *  \brief PMA/PMD Transmit Disable Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_PMA_PMD_TX_DIS_t {
    union {
        struct {
             uint16_t PMA_TX_DIS       : 1; /**< PMD Transmit Disable. */
             uint16_t RESERVED1        : 15; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_PMA_PMD_TX_DIS_t;

/*@}*/

/** @defgroup PMA_PMD_EXT_ABILITY PMA/PMD Extended Abilities Register (PMA_PMD_EXT_ABILITY) Register
 *  PMA/PMD Extended Abilities Register (PMA_PMD_EXT_ABILITY) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_PMA_PMD_EXT_ABILITY_t
 *  \brief PMA/PMD Extended Abilities Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_PMA_PMD_EXT_ABILITY_t {
    union {
        struct {
             uint16_t RESERVED0        : 11; /**< Reserved */
             uint16_t PMA_PMD_BT1_ABLE : 1; /**< PHY Supports BASE-T1 */
             uint16_t RESERVED12       : 4; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_PMA_PMD_EXT_ABILITY_t;

/*@}*/

/** @defgroup PMA_PMD_BT1_ABILITY BASE-T1 PMA/PMD Extended Ability Register (PMA_PMD_BT1_ABILITY) Register
 *  BASE-T1 PMA/PMD Extended Ability Register (PMA_PMD_BT1_ABILITY) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_PMA_PMD_BT1_ABILITY_t
 *  \brief BASE-T1 PMA/PMD Extended Ability Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_PMA_PMD_BT1_ABILITY_t {
    union {
        struct {
             uint16_t B100_ABILITY     : 1; /**< 100BASE-T1 Ability. */
             uint16_t B1000_ABILITY    : 1; /**< 1000BASE-T1 Ability. */
             uint16_t B10L_ABILITY     : 1; /**< 10BASE-T1L Ability. */
             uint16_t B10S_ABILITY     : 1; /**< 10BASE-T1S Ability. */
             uint16_t RESERVED4        : 12; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_PMA_PMD_BT1_ABILITY_t;

/*@}*/

/** @defgroup PMA_PMD_BT1_CONTROL BASE-T1 PMA/PMD Control Register (PMA_PMD_BT1_CONTROL) Register
 *  BASE-T1 PMA/PMD Control Register (PMA_PMD_BT1_CONTROL) Register
 *  @{
 */

/*! ========================================================================
 *  \enum ADI_ADIN1100_PMA_PMD_BT1_CONTROL_BT1_TYPE_SEL
 *  \brief BASE-T1 Type Selection
 *  ======================================================================== */
typedef enum
{
     ADIN1100_PMA_PMD_BT1_CONTROL_BT1_TYPE_SEL_TS_10BASE_T1L = 2     /**< 10BASE-T1L */
}  ADI_ADIN1100_PMA_PMD_BT1_CONTROL_BT1_TYPE_SEL;

/*! ========================================================================
 *  \struct ADI_ADIN1100_PMA_PMD_BT1_CONTROL_t
 *  \brief BASE-T1 PMA/PMD Control Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_PMA_PMD_BT1_CONTROL_t {
    union {
        struct {
             uint16_t BT1_TYPE_SEL     : 4; /**< BASE-T1 Type Selection */
             uint16_t RESERVED4        : 10; /**< Reserved */
             uint16_t CFG_MST          : 1; /**< Master-slave Config. */
             uint16_t RESERVED15       : 1; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_PMA_PMD_BT1_CONTROL_t;

/*@}*/

/** @defgroup B10L_PMA_CNTRL 10BASE-T1L PMA Control Register (B10L_PMA_CNTRL) Register
 *  10BASE-T1L PMA Control Register (B10L_PMA_CNTRL) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_B10L_PMA_CNTRL_t
 *  \brief 10BASE-T1L PMA Control Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_B10L_PMA_CNTRL_t {
    union {
        struct {
             uint16_t B10L_LB_PMA_LOC_EN : 1; /**< 10BASE-T1L PMA Loopback. */
             uint16_t RESERVED1        : 9; /**< Reserved */
             uint16_t B10L_EEE         : 1; /**< 10BASE-T1L EEE Enable. */
             uint16_t RESERVED11       : 1; /**< Reserved */
             uint16_t B10L_TX_LVL_HI   : 1; /**< 10BASE-T1L Transmit Voltage Amplitude Control. */
             uint16_t RESERVED13       : 1; /**< Reserved */
             uint16_t B10L_TX_DIS_MODE_EN : 1; /**< 10BASE-T1L Transmit Disable Mode. */
             uint16_t RESERVED15       : 1; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_B10L_PMA_CNTRL_t;

/*@}*/

/** @defgroup B10L_PMA_STAT 10BASE-T1L PMA Status Register (B10L_PMA_STAT) Register
 *  10BASE-T1L PMA Status Register (B10L_PMA_STAT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_B10L_PMA_STAT_t
 *  \brief 10BASE-T1L PMA Status Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_B10L_PMA_STAT_t {
    union {
        struct {
             uint16_t RESERVED0        : 10; /**< Reserved */
             uint16_t B10L_EEE_ABLE    : 1; /**< 10BASE-T1L EEE Ability */
             uint16_t B10L_PMA_SFT_PD_ABLE : 1; /**< PMA Supports Powerdown */
             uint16_t B10L_TX_LVL_HI_ABLE : 1; /**< 10BASE-T1L High Voltage Tx Ability */
             uint16_t B10L_LB_PMA_LOC_ABLE : 1; /**< 10BASE-T1L PMA Loopback Ability */
             uint16_t RESERVED14       : 2; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_B10L_PMA_STAT_t;

/*@}*/

/** @defgroup B10L_TEST_MODE_CNTRL 10BASE-T1L Test Mode Control Register (B10L_TEST_MODE_CNTRL) Register
 *  10BASE-T1L Test Mode Control Register (B10L_TEST_MODE_CNTRL) Register
 *  @{
 */

/*! ========================================================================
 *  \enum ADI_ADIN1100_B10L_TEST_MODE_CNTRL_B10L_TX_TEST_MODE
 *  \brief 10BASE-T1L Transmitter Test Mode.
 *  ======================================================================== */
typedef enum
{
     ADIN1100_B10L_TEST_MODE_CNTRL_B10L_TX_TEST_MODE_IEEE_TX_TM_NONE = 0,    /**< Normal operation. */
     ADIN1100_B10L_TEST_MODE_CNTRL_B10L_TX_TEST_MODE_IEEE_TX_TM_JITTER = 1,    /**< Test mode 1 - Transmitter output voltage and timing jitter test mode.  When test mode 1 is enabled, the PHY shall repeatedly transmit the data symbol sequence (+1, -1). */
     ADIN1100_B10L_TEST_MODE_CNTRL_B10L_TX_TEST_MODE_IEEE_TX_TM_DROOP = 2,    /**< Test mode 2 - Transmitter output droop test mode.  When test mode 2 is enabled, the PHY shall transmit ten "+1" symbols followed by ten "-1" symbols. */
     ADIN1100_B10L_TEST_MODE_CNTRL_B10L_TX_TEST_MODE_IEEE_TX_TM_IDLE = 3     /**< Test mode 3 - Normal operation in Idle mode.  When test mode 3 is enabled, the PHY shall transmit as in non-test operation and in the MASTER data mode with data set to normal Inter-Frame idle signals. */
}  ADI_ADIN1100_B10L_TEST_MODE_CNTRL_B10L_TX_TEST_MODE;

/*! ========================================================================
 *  \struct ADI_ADIN1100_B10L_TEST_MODE_CNTRL_t
 *  \brief 10BASE-T1L Test Mode Control Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_B10L_TEST_MODE_CNTRL_t {
    union {
        struct {
             uint16_t RESERVED0        : 13; /**< Reserved */
             uint16_t B10L_TX_TEST_MODE : 3; /**< 10BASE-T1L Transmitter Test Mode. */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_B10L_TEST_MODE_CNTRL_t;

/*@}*/

/** @defgroup CR_STBL_CHK_FOFFS_SAT_THR Frequency Offset Saturation Threshold for CR Stability Check Register (CR_STBL_CHK_FOFFS_SAT_THR) Register
 *  Frequency Offset Saturation Threshold for CR Stability Check Register (CR_STBL_CHK_FOFFS_SAT_THR) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_CR_STBL_CHK_FOFFS_SAT_THR_t
 *  \brief Frequency Offset Saturation Threshold for CR Stability Check Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_CR_STBL_CHK_FOFFS_SAT_THR_t {
    union {
        struct {
             uint16_t CR_STBL_CHK_FOFFS_SAT_THR : 11; /**< Frequency Offset Saturation Threshold for CR Stability Check. */
             uint16_t RESERVED11       : 5; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_CR_STBL_CHK_FOFFS_SAT_THR_t;

/*@}*/

/** @defgroup SLV_FLTR_ECHO_ACQ_CR_KP Slave IIR Filter Change Echo Acquisition Clock Recovery Proportional Gain Register (SLV_FLTR_ECHO_ACQ_CR_KP) Register
 *  Slave IIR Filter Change Echo Acquisition Clock Recovery Proportional Gain Register (SLV_FLTR_ECHO_ACQ_CR_KP) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_SLV_FLTR_ECHO_ACQ_CR_KP_t
 *  \brief Slave IIR Filter Change Echo Acquisition Clock Recovery Proportional Gain Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_SLV_FLTR_ECHO_ACQ_CR_KP_t {
    union {
        struct {
             int16_t SLV_FLTR_ECHO_ACQ_CR_KP : 16; /**< Slave IIR Filter Change Echo Acquisition Clock Recovery Proportional Gain. */
        };
        int16_t VALUE16;
    };
} ADI_ADIN1100_SLV_FLTR_ECHO_ACQ_CR_KP_t;

/*@}*/

/** @defgroup B10L_PMA_LINK_STAT 10BASE-T1L PMA Link Status Register (B10L_PMA_LINK_STAT) Register
 *  10BASE-T1L PMA Link Status Register (B10L_PMA_LINK_STAT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_B10L_PMA_LINK_STAT_t
 *  \brief 10BASE-T1L PMA Link Status Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_B10L_PMA_LINK_STAT_t {
    union {
        struct {
             uint16_t B10L_LINK_STAT_OK : 1; /**< Link Status OK */
             uint16_t B10L_LINK_STAT_OK_LL : 1; /**< Link Status Ok Latch Low */
             uint16_t RESERVED2        : 2; /**< Reserved */
             uint16_t B10L_DSCR_STAT_OK : 1; /**< 10BASE-T1L Descrambler Status Ok */
             uint16_t B10L_DSCR_STAT_OK_LL : 1; /**< BASE-T1L Descrambler Status Ok Latch Low */
             uint16_t B10L_LOC_RCVR_STAT_OK : 1; /**< 10BASE-T1L Local Receiver Status Ok */
             uint16_t B10L_LOC_RCVR_STAT_OK_LL : 1; /**< 10BASE-T1L Local Receiver Status Ok Latch Low */
             uint16_t B10L_REM_RCVR_STAT_OK : 1; /**< 10BASE-T1L Remote Receiver Status Ok */
             uint16_t B10L_REM_RCVR_STAT_OK_LL : 1; /**< 10BASE-T1L Remote Receiver Status Ok Latch Low */
             uint16_t RESERVED10       : 6; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_B10L_PMA_LINK_STAT_t;

/*@}*/

/** @defgroup MSE_VAL MSE Value Register (MSE_VAL) Register
 *  MSE Value Register (MSE_VAL) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_MSE_VAL_t
 *  \brief MSE Value Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_MSE_VAL_t {
    union {
        struct {
             uint16_t MSE_VAL          : 16; /**< MSE Value */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_MSE_VAL_t;

/*@}*/

/** @defgroup PCS_CNTRL1 PCS Control 1 Register (PCS_CNTRL1) Register
 *  PCS Control 1 Register (PCS_CNTRL1) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_PCS_CNTRL1_t
 *  \brief PCS Control 1 Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_PCS_CNTRL1_t {
    union {
        struct {
             uint16_t RESERVED0        : 11; /**< Reserved */
             uint16_t PCS_SFT_PD       : 1; /**< PCS Software Power-down. */
             uint16_t RESERVED12       : 2; /**< Reserved */
             uint16_t LB_PCS_EN        : 1; /**< PCS Loopback Enable */
             uint16_t PCS_SFT_RST      : 1; /**< PCS Software Reset. */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_PCS_CNTRL1_t;

/*@}*/

/** @defgroup PCS_STAT1 PCS Status 1 Register (PCS_STAT1) Register
 *  PCS Status 1 Register (PCS_STAT1) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_PCS_STAT1_t
 *  \brief PCS Status 1 Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_PCS_STAT1_t {
    union {
        struct {
             uint16_t RESERVED0        : 1; /**< Reserved */
             uint16_t PCS_SFT_PD_ABLE  : 1; /**< PCS Software Powerdown Able */
             uint16_t RESERVED2        : 14; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_PCS_STAT1_t;

/*@}*/

/** @defgroup PCS_DEVS_IN_PKG1 PCS MMD Devices in Package 1 Register (PCS_DEVS_IN_PKG1) Register
 *  PCS MMD Devices in Package 1 Register (PCS_DEVS_IN_PKG1) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_PCS_DEVS_IN_PKG1_t
 *  \brief PCS MMD Devices in Package 1 Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_PCS_DEVS_IN_PKG1_t {
    union {
        struct {
             uint16_t PCS_DEVS_IN_PKG1 : 16; /**< PCS MMD Devices in Package */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_PCS_DEVS_IN_PKG1_t;

/*@}*/

/** @defgroup PCS_DEVS_IN_PKG2 PCS MMD Devices in Package 2 Register (PCS_DEVS_IN_PKG2) Register
 *  PCS MMD Devices in Package 2 Register (PCS_DEVS_IN_PKG2) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_PCS_DEVS_IN_PKG2_t
 *  \brief PCS MMD Devices in Package 2 Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_PCS_DEVS_IN_PKG2_t {
    union {
        struct {
             uint16_t PCS_DEVS_IN_PKG2 : 16; /**< PCS MMD Devices in Package */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_PCS_DEVS_IN_PKG2_t;

/*@}*/

/** @defgroup PCS_STAT2 PCS Status 2 Register (PCS_STAT2) Register
 *  PCS Status 2 Register (PCS_STAT2) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_PCS_STAT2_t
 *  \brief PCS Status 2 Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_PCS_STAT2_t {
    union {
        struct {
             uint16_t RESERVED0        : 14; /**< Reserved */
             uint16_t PCS_PRESENT      : 2; /**< PCS Present */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_PCS_STAT2_t;

/*@}*/

/** @defgroup B10L_PCS_CNTRL 10BASE-T1L PCS Control Register (B10L_PCS_CNTRL) Register
 *  10BASE-T1L PCS Control Register (B10L_PCS_CNTRL) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_B10L_PCS_CNTRL_t
 *  \brief 10BASE-T1L PCS Control Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_B10L_PCS_CNTRL_t {
    union {
        struct {
             uint16_t RESERVED0        : 14; /**< Reserved */
             uint16_t B10L_LB_PCS_EN   : 1; /**< PCS Loopback Enable */
             uint16_t RESERVED15       : 1; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_B10L_PCS_CNTRL_t;

/*@}*/

/** @defgroup B10L_PCS_STAT 10BASE-T1L PCS Status Register (B10L_PCS_STAT) Register
 *  10BASE-T1L PCS Status Register (B10L_PCS_STAT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_B10L_PCS_STAT_t
 *  \brief 10BASE-T1L PCS Status Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_B10L_PCS_STAT_t {
    union {
        struct {
             uint16_t RESERVED0        : 2; /**< Reserved */
             uint16_t B10L_PCS_DSCR_STAT_OK_LL : 1; /**< PCS Descrambler Status. */
             uint16_t RESERVED3        : 13; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_B10L_PCS_STAT_t;

/*@}*/

/** @defgroup AN_DEVS_IN_PKG1 AUTO-_NEGOTIATION MMD Devices in Package 1 Register (AN_DEVS_IN_PKG1) Register
 *  AUTO-_NEGOTIATION MMD Devices in Package 1 Register (AN_DEVS_IN_PKG1) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_AN_DEVS_IN_PKG1_t
 *  \brief AUTO-_NEGOTIATION MMD Devices in Package 1 Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_AN_DEVS_IN_PKG1_t {
    union {
        struct {
             uint16_t AN_DEVS_IN_PKG1  : 16; /**< Autonegotiation MMD Devices in Package */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_AN_DEVS_IN_PKG1_t;

/*@}*/

/** @defgroup AN_DEVS_IN_PKG2 AUTO-_NEGOTIATION MMD Devices in Package 2 Register (AN_DEVS_IN_PKG2) Register
 *  AUTO-_NEGOTIATION MMD Devices in Package 2 Register (AN_DEVS_IN_PKG2) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_AN_DEVS_IN_PKG2_t
 *  \brief AUTO-_NEGOTIATION MMD Devices in Package 2 Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_AN_DEVS_IN_PKG2_t {
    union {
        struct {
             uint16_t AN_DEVS_IN_PKG2  : 16; /**< Autonegotiation MMD Devices in Package */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_AN_DEVS_IN_PKG2_t;

/*@}*/

/** @defgroup AN_CONTROL BASE-T1 Autonegotiation Control Register (AN_CONTROL) Register
 *  BASE-T1 Autonegotiation Control Register (AN_CONTROL) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_AN_CONTROL_t
 *  \brief BASE-T1 Autonegotiation Control Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_AN_CONTROL_t {
    union {
        struct {
             uint16_t RESERVED0        : 9; /**< Reserved */
             uint16_t AN_RESTART       : 1; /**< Autonegotiation Restart */
             uint16_t RESERVED10       : 2; /**< Reserved */
             uint16_t AN_EN            : 1; /**< Autonegotiation Enable. */
             uint16_t RESERVED13       : 3; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_AN_CONTROL_t;

/*@}*/

/** @defgroup AN_STATUS BASE-T1 Autonegotiation Status Register (AN_STATUS) Register
 *  BASE-T1 Autonegotiation Status Register (AN_STATUS) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_AN_STATUS_t
 *  \brief BASE-T1 Autonegotiation Status Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_AN_STATUS_t {
    union {
        struct {
             uint16_t RESERVED0        : 2; /**< Reserved */
             uint16_t AN_LINK_STATUS   : 1; /**< Link Status. */
             uint16_t AN_ABLE          : 1; /**< Autonegotiation Ability. */
             uint16_t AN_REMOTE_FAULT  : 1; /**< Autonegotiation Remote Fault */
             uint16_t AN_COMPLETE      : 1; /**< Autonegotiation Complete. */
             uint16_t AN_PAGE_RX       : 1; /**< Page Received */
             uint16_t RESERVED7        : 9; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_AN_STATUS_t;

/*@}*/

/** @defgroup AN_ADV_ABILITY_L BASE-T1 Autonegotiation Advertisement [15:0] Register (AN_ADV_ABILITY_L) Register
 *  BASE-T1 Autonegotiation Advertisement [15:0] Register (AN_ADV_ABILITY_L) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_AN_ADV_ABILITY_L_t
 *  \brief BASE-T1 Autonegotiation Advertisement [15:0] Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_AN_ADV_ABILITY_L_t {
    union {
        struct {
             uint16_t AN_ADV_SELECTOR  : 5; /**< Selector. */
             uint16_t RESERVED5        : 5; /**< Reserved */
             uint16_t AN_ADV_PAUSE     : 2; /**< Pause Ability. */
             uint16_t AN_ADV_FORCE_MS  : 1; /**< Force Master/slave Configuration. */
             uint16_t AN_ADV_REMOTE_FAULT : 1; /**< Remote Fault. */
             uint16_t AN_ADV_ACK       : 1; /**< Acknowledge (ACK). */
             uint16_t AN_ADV_NEXT_PAGE_REQ : 1; /**< Next Page Request. */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_AN_ADV_ABILITY_L_t;

/*@}*/

/** @defgroup AN_ADV_ABILITY_M BASE-T1 Autonegotiation Advertisement [31:16] Register (AN_ADV_ABILITY_M) Register
 *  BASE-T1 Autonegotiation Advertisement [31:16] Register (AN_ADV_ABILITY_M) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_AN_ADV_ABILITY_M_t
 *  \brief BASE-T1 Autonegotiation Advertisement [31:16] Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_AN_ADV_ABILITY_M_t {
    union {
        struct {
             uint16_t RESERVED0        : 4; /**< Reserved */
             uint16_t AN_ADV_MST       : 1; /**< Master/slave Configuration. */
             uint16_t RESERVED5        : 9; /**< Reserved */
             uint16_t AN_ADV_B10L      : 1; /**< 10BASE-T1L Ability. */
             uint16_t RESERVED15       : 1; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_AN_ADV_ABILITY_M_t;

/*@}*/

/** @defgroup AN_ADV_ABILITY_H BASE-T1 Autonegotiation Advertisement [47:32] Register (AN_ADV_ABILITY_H) Register
 *  BASE-T1 Autonegotiation Advertisement [47:32] Register (AN_ADV_ABILITY_H) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_AN_ADV_ABILITY_H_t
 *  \brief BASE-T1 Autonegotiation Advertisement [47:32] Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_AN_ADV_ABILITY_H_t {
    union {
        struct {
             uint16_t RESERVED0        : 12; /**< Reserved */
             uint16_t AN_ADV_B10L_TX_LVL_HI_REQ : 1; /**< 10BASE-T1L High Level Transmit Operating Mode Request. */
             uint16_t AN_ADV_B10L_TX_LVL_HI_ABL : 1; /**< 10BASE-T1L High Level Transmit Operating Mode Ability. */
             uint16_t RESERVED14       : 2; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_AN_ADV_ABILITY_H_t;

/*@}*/

/** @defgroup AN_LP_ADV_ABILITY_L BASE-T1 Autonegotiation Link Partner Base Page Ability [15:0] Register (AN_LP_ADV_ABILITY_L) Register
 *  BASE-T1 Autonegotiation Link Partner Base Page Ability [15:0] Register (AN_LP_ADV_ABILITY_L) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_AN_LP_ADV_ABILITY_L_t
 *  \brief BASE-T1 Autonegotiation Link Partner Base Page Ability [15:0] Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_AN_LP_ADV_ABILITY_L_t {
    union {
        struct {
             uint16_t AN_LP_ADV_SELECTOR : 5; /**< Link Partner Selector. */
             uint16_t RESERVED5        : 5; /**< Reserved */
             uint16_t AN_LP_ADV_PAUSE  : 2; /**< Link Partner Pause Ability. */
             uint16_t AN_LP_ADV_FORCE_MS : 1; /**< Link Partner Force Master/slave Configuration. */
             uint16_t AN_LP_ADV_REMOTE_FAULT : 1; /**< Link Partner Remote Fault. */
             uint16_t AN_LP_ADV_ACK    : 1; /**< Link Partner Acknowledge (ACK). */
             uint16_t AN_LP_ADV_NEXT_PAGE_REQ : 1; /**< Link Partner Next Page Request. */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_AN_LP_ADV_ABILITY_L_t;

/*@}*/

/** @defgroup AN_LP_ADV_ABILITY_M BASE-T1 Autonegotiation Link Partner Base Page Ability [31:16] Register (AN_LP_ADV_ABILITY_M) Register
 *  BASE-T1 Autonegotiation Link Partner Base Page Ability [31:16] Register (AN_LP_ADV_ABILITY_M) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_AN_LP_ADV_ABILITY_M_t
 *  \brief BASE-T1 Autonegotiation Link Partner Base Page Ability [31:16] Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_AN_LP_ADV_ABILITY_M_t {
    union {
        struct {
             uint16_t RESERVED0        : 4; /**< Reserved */
             uint16_t AN_LP_ADV_MST    : 1; /**< Link Partner MASTER/_SLAVE Configuration. */
             uint16_t AN_LP_ADV_B100   : 1; /**< Link Partner 100BASE-T1 Ability. */
             uint16_t AN_LP_ADV_B10S_FD : 1; /**< Link Partner 10BASE-T1S Full Duplex Ability. */
             uint16_t AN_LP_ADV_B1000  : 1; /**< Link Partner 1000BASE-T1 Ability. */
             uint16_t RESERVED8        : 6; /**< Reserved */
             uint16_t AN_LP_ADV_B10L   : 1; /**< Link Partner 10BASE-T1L Ability. */
             uint16_t RESERVED15       : 1; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_AN_LP_ADV_ABILITY_M_t;

/*@}*/

/** @defgroup AN_LP_ADV_ABILITY_H BASE-T1 Autonegotiation Link Partner Base Page Ability [47:32] Register (AN_LP_ADV_ABILITY_H) Register
 *  BASE-T1 Autonegotiation Link Partner Base Page Ability [47:32] Register (AN_LP_ADV_ABILITY_H) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_AN_LP_ADV_ABILITY_H_t
 *  \brief BASE-T1 Autonegotiation Link Partner Base Page Ability [47:32] Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_AN_LP_ADV_ABILITY_H_t {
    union {
        struct {
             uint16_t RESERVED0        : 11; /**< Reserved */
             uint16_t AN_LP_ADV_B10S_HD : 1; /**< Link Partner 10BASE-T1S Half Duplex Ability. */
             uint16_t AN_LP_ADV_B10L_TX_LVL_HI_REQ : 1; /**< Link Partner 10BASE-T1L High Level Transmit Operating Mode Request. */
             uint16_t AN_LP_ADV_B10L_TX_LVL_HI_ABL : 1; /**< Link Partner 10BASE-T1L High Level Transmit Operating Mode Ability. */
             uint16_t AN_LP_ADV_B10L_EEE : 1; /**< Link Partner 10BASE-T1L EEE Ability. */
             uint16_t RESERVED15       : 1; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_AN_LP_ADV_ABILITY_H_t;

/*@}*/

/** @defgroup AN_NEXT_PAGE_L BASE-T1 Autonegotiation Next Page Transmit [15:0] Register (AN_NEXT_PAGE_L) Register
 *  BASE-T1 Autonegotiation Next Page Transmit [15:0] Register (AN_NEXT_PAGE_L) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_AN_NEXT_PAGE_L_t
 *  \brief BASE-T1 Autonegotiation Next Page Transmit [15:0] Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_AN_NEXT_PAGE_L_t {
    union {
        struct {
             uint16_t AN_NP_MESSAGE_CODE : 11; /**< Message/unformatted Code Field. */
             uint16_t AN_NP_TOGGLE     : 1; /**< Toggle Bit. */
             uint16_t AN_NP_ACK2       : 1; /**< Acknowledge 2. */
             uint16_t AN_NP_MESSAGE_PAGE : 1; /**< Next Page Encoding. */
             uint16_t AN_NP_ACK        : 1; /**< Next Page Acknowledge. */
             uint16_t AN_NP_NEXT_PAGE_REQ : 1; /**< Next Page Request. */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_AN_NEXT_PAGE_L_t;

/*@}*/

/** @defgroup AN_NEXT_PAGE_M BASE-T1 Autonegotiation Next Page Transmit [31:16] Register (AN_NEXT_PAGE_M) Register
 *  BASE-T1 Autonegotiation Next Page Transmit [31:16] Register (AN_NEXT_PAGE_M) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_AN_NEXT_PAGE_M_t
 *  \brief BASE-T1 Autonegotiation Next Page Transmit [31:16] Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_AN_NEXT_PAGE_M_t {
    union {
        struct {
             int16_t AN_NP_UNFORMATTED1 : 16; /**< Unformatted Code Field 1 */
        };
        int16_t VALUE16;
    };
} ADI_ADIN1100_AN_NEXT_PAGE_M_t;

/*@}*/

/** @defgroup AN_NEXT_PAGE_H BASE-T1 Autonegotiation Next Page Transmit [47:32] Register (AN_NEXT_PAGE_H) Register
 *  BASE-T1 Autonegotiation Next Page Transmit [47:32] Register (AN_NEXT_PAGE_H) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_AN_NEXT_PAGE_H_t
 *  \brief BASE-T1 Autonegotiation Next Page Transmit [47:32] Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_AN_NEXT_PAGE_H_t {
    union {
        struct {
             int16_t AN_NP_UNFORMATTED2 : 16; /**< Unformatted Code Field 2 */
        };
        int16_t VALUE16;
    };
} ADI_ADIN1100_AN_NEXT_PAGE_H_t;

/*@}*/

/** @defgroup AN_LP_NEXT_PAGE_L BASE-T1 Autonegotiation Link Partner Next Page Ability [15:0] Register (AN_LP_NEXT_PAGE_L) Register
 *  BASE-T1 Autonegotiation Link Partner Next Page Ability [15:0] Register (AN_LP_NEXT_PAGE_L) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_AN_LP_NEXT_PAGE_L_t
 *  \brief BASE-T1 Autonegotiation Link Partner Next Page Ability [15:0] Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_AN_LP_NEXT_PAGE_L_t {
    union {
        struct {
             uint16_t AN_LP_NP_MESSAGE_CODE : 11; /**< Link Partner Message/unformatted Code Field. */
             uint16_t AN_LP_NP_TOGGLE  : 1; /**< Link Partner Toggle Bit. */
             uint16_t AN_LP_NP_ACK2    : 1; /**< Link Partner Acknowledge 2. */
             uint16_t AN_LP_NP_MESSAGE_PAGE : 1; /**< Link Partner Next Page Encoding. */
             uint16_t AN_LP_NP_ACK     : 1; /**< Link Partner Next Page Acknowledge. */
             uint16_t AN_LP_NP_NEXT_PAGE_REQ : 1; /**< Next Page Request. */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_AN_LP_NEXT_PAGE_L_t;

/*@}*/

/** @defgroup AN_LP_NEXT_PAGE_M BASE-T1 Autonegotiation Link Partner Next Page Ability [31:16] Register (AN_LP_NEXT_PAGE_M) Register
 *  BASE-T1 Autonegotiation Link Partner Next Page Ability [31:16] Register (AN_LP_NEXT_PAGE_M) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_AN_LP_NEXT_PAGE_M_t
 *  \brief BASE-T1 Autonegotiation Link Partner Next Page Ability [31:16] Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_AN_LP_NEXT_PAGE_M_t {
    union {
        struct {
             int16_t AN_LP_NP_UNFORMATTED1 : 16; /**< Link Partner Unformatted Code Field 1 */
        };
        int16_t VALUE16;
    };
} ADI_ADIN1100_AN_LP_NEXT_PAGE_M_t;

/*@}*/

/** @defgroup AN_LP_NEXT_PAGE_H BASE-T1 Autonegotiation Link Partner Next Page Ability [47:32] Register (AN_LP_NEXT_PAGE_H) Register
 *  BASE-T1 Autonegotiation Link Partner Next Page Ability [47:32] Register (AN_LP_NEXT_PAGE_H) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_AN_LP_NEXT_PAGE_H_t
 *  \brief BASE-T1 Autonegotiation Link Partner Next Page Ability [47:32] Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_AN_LP_NEXT_PAGE_H_t {
    union {
        struct {
             int16_t AN_LP_NP_UNFORMATTED2 : 16; /**< Link Partner Unformatted Code Field 2 */
        };
        int16_t VALUE16;
    };
} ADI_ADIN1100_AN_LP_NEXT_PAGE_H_t;

/*@}*/

/** @defgroup AN_B10_ADV_ABILITY 10BASE-T1 Autonegotiation Control Register (AN_B10_ADV_ABILITY) Register
 *  10BASE-T1 Autonegotiation Control Register (AN_B10_ADV_ABILITY) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_AN_B10_ADV_ABILITY_t
 *  \brief 10BASE-T1 Autonegotiation Control Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_AN_B10_ADV_ABILITY_t {
    union {
        struct {
             uint16_t RESERVED0        : 12; /**< Reserved */
             uint16_t AN_B10_ADV_B10L_TX_LVL_HI_REQ : 1; /**< 10BASE-T1L High Level Transmit Operating Mode Request. */
             uint16_t AN_B10_ADV_B10L_TX_LVL_HI_ABL : 1; /**< 10BASE-T1L High Level Transmit Operating Mode Ability. */
             uint16_t AN_B10_ADV_B10L_EEE : 1; /**< 10BASE-T1L EEE Ability. */
             uint16_t AN_B10_ADV_B10L  : 1; /**< 10BASE-T1L Ability. */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_AN_B10_ADV_ABILITY_t;

/*@}*/

/** @defgroup AN_B10_LP_ADV_ABILITY 10BASE-T1 Autonegotiation Status Register (AN_B10_LP_ADV_ABILITY) Register
 *  10BASE-T1 Autonegotiation Status Register (AN_B10_LP_ADV_ABILITY) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_AN_B10_LP_ADV_ABILITY_t
 *  \brief 10BASE-T1 Autonegotiation Status Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_AN_B10_LP_ADV_ABILITY_t {
    union {
        struct {
             uint16_t RESERVED0        : 6; /**< Reserved */
             uint16_t AN_B10_LP_ADV_B10S_HD : 1; /**< Link Partner 10BASE-T1S Half Duplex Ability. */
             uint16_t AN_B10_LP_ADV_B10S_FD : 1; /**< Link Partner 10BASE-T1S Full Duplex Ability. */
             uint16_t RESERVED8        : 4; /**< Reserved */
             uint16_t AN_B10_LP_ADV_B10L_TX_LVL_HI_REQ : 1; /**< 10BASE-T1L High Level Transmit Operating Mode Request. */
             uint16_t AN_B10_LP_ADV_B10L_TX_LVL_HI_ABL : 1; /**< 10BASE-T1L High Level Transmit Operating Mode Ability. */
             uint16_t AN_B10_LP_ADV_B10L_EEE : 1; /**< 10BASE-T1L EEE Ability. */
             uint16_t AN_B10_LP_ADV_B10L : 1; /**< 10BASE-T1L Ability. */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_AN_B10_LP_ADV_ABILITY_t;

/*@}*/

/** @defgroup AN_STATUS_EXTRA Extra Autonegotiation Status Register (AN_STATUS_EXTRA) Register
 *  Extra Autonegotiation Status Register (AN_STATUS_EXTRA) Register
 *  @{
 */

/*! ========================================================================
 *  \enum ADI_ADIN1100_AN_STATUS_EXTRA_AN_HCD_TECH
 *  \brief Highest Common Denominator (HCD) PHY Technology.
 *  ======================================================================== */
typedef enum
{
     ADIN1100_AN_STATUS_EXTRA_AN_HCD_TECH_HCD_NULL = 0,    /**< NULL (not run) */
     ADIN1100_AN_STATUS_EXTRA_AN_HCD_TECH_HCD_B10L = 1     /**< 10BASE-T1L */
}  ADI_ADIN1100_AN_STATUS_EXTRA_AN_HCD_TECH;

/*! ========================================================================
 *  \enum ADI_ADIN1100_AN_STATUS_EXTRA_AN_MS_CONFIG_RSLTN
 *  \brief Master/slave Resolution Result.
 *  ======================================================================== */
typedef enum
{
     ADIN1100_AN_STATUS_EXTRA_AN_MS_CONFIG_RSLTN_MS_NOT_RUN = 0,    /**< Not run */
     ADIN1100_AN_STATUS_EXTRA_AN_MS_CONFIG_RSLTN_MS_FAULT = 1,    /**< Configuration fault */
     ADIN1100_AN_STATUS_EXTRA_AN_MS_CONFIG_RSLTN_MS_SLAVE = 2,    /**< Success, PHY is configured as SLAVE */
     ADIN1100_AN_STATUS_EXTRA_AN_MS_CONFIG_RSLTN_MS_MASTER = 3     /**< Success, PHY is configured as MASTER */
}  ADI_ADIN1100_AN_STATUS_EXTRA_AN_MS_CONFIG_RSLTN;

/*! ========================================================================
 *  \struct ADI_ADIN1100_AN_STATUS_EXTRA_t
 *  \brief Extra Autonegotiation Status Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_AN_STATUS_EXTRA_t {
    union {
        struct {
             uint16_t AN_LINK_GOOD     : 1; /**< Autonegotiation Complete Indication. */
             uint16_t AN_HCD_TECH      : 4; /**< Highest Common Denominator (HCD) PHY Technology. */
             uint16_t AN_MS_CONFIG_RSLTN : 2; /**< Master/slave Resolution Result. */
             uint16_t AN_TX_LVL_RSLTN  : 2; /**< Autonegotiation Tx Level Result */
             uint16_t AN_INC_LINK      : 1; /**< Incompatible Link Indication. */
             uint16_t AN_LP_NP_RX      : 1; /**< Next Page Request Received from Link Partner. */
             uint16_t RESERVED11       : 5; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_AN_STATUS_EXTRA_t;

/*@}*/

/** @defgroup AN_PHY_INST_STATUS PHY Instantaneous Status. (AN_PHY_INST_STATUS) Register
 *  PHY Instantaneous Status. (AN_PHY_INST_STATUS) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_AN_PHY_INST_STATUS_t
 *  \brief PHY Instantaneous Status.
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_AN_PHY_INST_STATUS_t {
    union {
        struct {
             uint16_t IS_TX_LVL_LO     : 1; /**< Tx Level Low Status */
             uint16_t IS_TX_LVL_HI     : 1; /**< Tx Level High Status */
             uint16_t IS_CFG_SLV       : 1; /**< Slave Status */
             uint16_t IS_CFG_MST       : 1; /**< Master Status */
             uint16_t IS_AN_TX_EN      : 1; /**< Autonegotiation Tx Enable Status */
             uint16_t RESERVED5        : 11; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_AN_PHY_INST_STATUS_t;

/*@}*/

/** @defgroup MMD1_DEV_ID1 Vendor Specific MMD 1 Device Identifier High Register (MMD1_DEV_ID1) Register
 *  Vendor Specific MMD 1 Device Identifier High Register (MMD1_DEV_ID1) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_MMD1_DEV_ID1_t
 *  \brief Vendor Specific MMD 1 Device Identifier High Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_MMD1_DEV_ID1_t {
    union {
        struct {
             int16_t MMD1_DEV_ID1     : 16; /**< Organizationally Unique Identifier */
        };
        int16_t VALUE16;
    };
} ADI_ADIN1100_MMD1_DEV_ID1_t;

/*@}*/

/** @defgroup MMD1_DEV_ID2 Vendor Specific MMD 1 Device Identifier Low Register (MMD1_DEV_ID2) Register
 *  Vendor Specific MMD 1 Device Identifier Low Register (MMD1_DEV_ID2) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_MMD1_DEV_ID2_t
 *  \brief Vendor Specific MMD 1 Device Identifier Low Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_MMD1_DEV_ID2_t {
    union {
        struct {
             uint16_t MMD1_REV_NUM     : 4; /**< Revision Number */
             uint16_t MMD1_MODEL_NUM   : 6; /**< Model Number */
             uint16_t MMD1_DEV_ID2_OUI : 6; /**< Organizationally Unique Identifier */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_MMD1_DEV_ID2_t;

/*@}*/

/** @defgroup MMD1_DEVS_IN_PKG1 Vendor Specific 1 MMD Devices in Package Register (MMD1_DEVS_IN_PKG1) Register
 *  Vendor Specific 1 MMD Devices in Package Register (MMD1_DEVS_IN_PKG1) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_MMD1_DEVS_IN_PKG1_t
 *  \brief Vendor Specific 1 MMD Devices in Package Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_MMD1_DEVS_IN_PKG1_t {
    union {
        struct {
             uint16_t MMD1_DEVS_IN_PKG1 : 16; /**< Vendor specific 1 MMD Devices in Package */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_MMD1_DEVS_IN_PKG1_t;

/*@}*/

/** @defgroup MMD1_DEVS_IN_PKG2 Vendor Specific 1 MMD Devices in Package Register (MMD1_DEVS_IN_PKG2) Register
 *  Vendor Specific 1 MMD Devices in Package Register (MMD1_DEVS_IN_PKG2) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_MMD1_DEVS_IN_PKG2_t
 *  \brief Vendor Specific 1 MMD Devices in Package Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_MMD1_DEVS_IN_PKG2_t {
    union {
        struct {
             uint16_t MMD1_DEVS_IN_PKG2 : 16; /**< Vendor specific 1 MMD Devices in Package */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_MMD1_DEVS_IN_PKG2_t;

/*@}*/

/** @defgroup MMD1_STATUS Vendor Specific MMD 1 Status Register (MMD1_STATUS) Register
 *  Vendor Specific MMD 1 Status Register (MMD1_STATUS) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_MMD1_STATUS_t
 *  \brief Vendor Specific MMD 1 Status Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_MMD1_STATUS_t {
    union {
        struct {
             uint16_t RESERVED0        : 14; /**< Reserved */
             uint16_t MMD1_STATUS      : 2; /**< Vendor Specific 1 MMD Status */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_MMD1_STATUS_t;

/*@}*/

/** @defgroup CRSM_IRQ_STATUS System Interrupt Status Register (CRSM_IRQ_STATUS) Register
 *  System Interrupt Status Register (CRSM_IRQ_STATUS) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_CRSM_IRQ_STATUS_t
 *  \brief System Interrupt Status Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_CRSM_IRQ_STATUS_t {
    union {
        struct {
             uint16_t RESERVED0        : 12; /**< Reserved */
             uint16_t CRSM_HRD_RST_IRQ_LH : 1; /**< Hardware Reset Interrupt */
             uint16_t RESERVED13       : 2; /**< Reserved */
             uint16_t CRSM_SW_IRQ_LH   : 1; /**< Software Requested Interrupt Event. */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_CRSM_IRQ_STATUS_t;

/*@}*/

/** @defgroup CRSM_IRQ_MASK System Interrupt Mask Register (CRSM_IRQ_MASK) Register
 *  System Interrupt Mask Register (CRSM_IRQ_MASK) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_CRSM_IRQ_MASK_t
 *  \brief System Interrupt Mask Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_CRSM_IRQ_MASK_t {
    union {
        struct {
             uint16_t RESERVED0        : 12; /**< Reserved */
             uint16_t CRSM_HRD_RST_IRQ_EN : 1; /**< Enable Hardware Reset Interrupt. */
             uint16_t RESERVED13       : 2; /**< Reserved */
             uint16_t CRSM_SW_IRQ_REQ  : 1; /**< Software Interrupt Request. */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_CRSM_IRQ_MASK_t;

/*@}*/

/** @defgroup CRSM_SFT_RST Software Reset Register (CRSM_SFT_RST) Register
 *  Software Reset Register (CRSM_SFT_RST) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_CRSM_SFT_RST_t
 *  \brief Software Reset Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_CRSM_SFT_RST_t {
    union {
        struct {
             uint16_t CRSM_SFT_RST     : 1; /**< Software Reset Register. */
             uint16_t RESERVED1        : 15; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_CRSM_SFT_RST_t;

/*@}*/

/** @defgroup CRSM_SFT_PD_CNTRL Software Power-down Control Register (CRSM_SFT_PD_CNTRL) Register
 *  Software Power-down Control Register (CRSM_SFT_PD_CNTRL) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_CRSM_SFT_PD_CNTRL_t
 *  \brief Software Power-down Control Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_CRSM_SFT_PD_CNTRL_t {
    union {
        struct {
             uint16_t CRSM_SFT_PD      : 1; /**< Software Power-down */
             uint16_t RESERVED1        : 15; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_CRSM_SFT_PD_CNTRL_t;

/*@}*/

/** @defgroup CRSM_PHY_SUBSYS_RST PHY Subsystem Reset Register (CRSM_PHY_SUBSYS_RST) Register
 *  PHY Subsystem Reset Register (CRSM_PHY_SUBSYS_RST) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_CRSM_PHY_SUBSYS_RST_t
 *  \brief PHY Subsystem Reset Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_CRSM_PHY_SUBSYS_RST_t {
    union {
        struct {
             uint16_t CRSM_PHY_SUBSYS_RST : 1; /**< PHY Subsystem Reset */
             uint16_t RESERVED1        : 15; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_CRSM_PHY_SUBSYS_RST_t;

/*@}*/

/** @defgroup CRSM_MAC_IF_RST PHY MAC Interface Reset Register (CRSM_MAC_IF_RST) Register
 *  PHY MAC Interface Reset Register (CRSM_MAC_IF_RST) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_CRSM_MAC_IF_RST_t
 *  \brief PHY MAC Interface Reset Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_CRSM_MAC_IF_RST_t {
    union {
        struct {
             uint16_t CRSM_MAC_IF_RST  : 1; /**< PHY MAC Interface Reset */
             uint16_t RESERVED1        : 15; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_CRSM_MAC_IF_RST_t;

/*@}*/

/** @defgroup CRSM_STAT System Status Register (CRSM_STAT) Register
 *  System Status Register (CRSM_STAT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_CRSM_STAT_t
 *  \brief System Status Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_CRSM_STAT_t {
    union {
        struct {
             uint16_t CRSM_SYS_RDY     : 1; /**< System Ready */
             uint16_t CRSM_SFT_PD_RDY  : 1; /**< Software Power-down Status */
             uint16_t RESERVED2        : 14; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_CRSM_STAT_t;

/*@}*/

/** @defgroup CRSM_PMG_CNTRL CRSM Power Management Control Register. (CRSM_PMG_CNTRL) Register
 *  CRSM Power Management Control Register. (CRSM_PMG_CNTRL) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_CRSM_PMG_CNTRL_t
 *  \brief CRSM Power Management Control Register.
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_CRSM_PMG_CNTRL_t {
    union {
        struct {
             uint16_t CRSM_FRC_OSC_EN  : 1; /**< Force Digital Boot Oscillator Clock Enable */
             uint16_t RESERVED1        : 15; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_CRSM_PMG_CNTRL_t;

/*@}*/

/** @defgroup CRSM_MAC_IF_CFG MAC Interface Configuration Register (CRSM_MAC_IF_CFG) Register
 *  MAC Interface Configuration Register (CRSM_MAC_IF_CFG) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_CRSM_MAC_IF_CFG_t
 *  \brief MAC Interface Configuration Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_CRSM_MAC_IF_CFG_t {
    union {
        struct {
             uint16_t CRSM_RGMII_EN    : 1; /**< RGMII MAC Interface Enable */
             uint16_t RESERVED1        : 3; /**< Reserved */
             uint16_t CRSM_RMII_EN     : 1; /**< RMII MAC Interface Enable */
             uint16_t RESERVED5        : 3; /**< Reserved */
             uint16_t CRSM_RMII_MEDIA_CNV_EN : 1; /**< Media Converter Enable */
             uint16_t RESERVED9        : 7; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_CRSM_MAC_IF_CFG_t;

/*@}*/

/** @defgroup CRSM_DIAG_CLK_CTRL CRSM Diagnostics Clock Control. (CRSM_DIAG_CLK_CTRL) Register
 *  CRSM Diagnostics Clock Control. (CRSM_DIAG_CLK_CTRL) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_CRSM_DIAG_CLK_CTRL_t
 *  \brief CRSM Diagnostics Clock Control.
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_CRSM_DIAG_CLK_CTRL_t {
    union {
        struct {
             uint16_t CRSM_DIAG_CLK_EN : 1; /**< Enable the Diagnostics Clock */
             uint16_t RESERVED1        : 15; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_CRSM_DIAG_CLK_CTRL_t;

/*@}*/

/** @defgroup MGMT_PRT_PKG Package Configuration Values Register (MGMT_PRT_PKG) Register
 *  Package Configuration Values Register (MGMT_PRT_PKG) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_MGMT_PRT_PKG_t
 *  \brief Package Configuration Values Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_MGMT_PRT_PKG_t {
    union {
        struct {
             uint16_t MGMT_PRT_PKG_VAL : 6; /**< Package Type */
             uint16_t RESERVED6        : 10; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_MGMT_PRT_PKG_t;

/*@}*/

/** @defgroup MGMT_MDIO_CNTRL MDIO Control Register (MGMT_MDIO_CNTRL) Register
 *  MDIO Control Register (MGMT_MDIO_CNTRL) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_MGMT_MDIO_CNTRL_t
 *  \brief MDIO Control Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_MGMT_MDIO_CNTRL_t {
    union {
        struct {
             uint16_t MGMT_GRP_MDIO_EN : 1; /**< Enable MDIO PHY/_PORT Group Address Mode */
             uint16_t RESERVED1        : 15; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_MGMT_MDIO_CNTRL_t;

/*@}*/

/** @defgroup DIGIO_PINMUX Pinmux Configuration 1 Register (DIGIO_PINMUX) Register
 *  Pinmux Configuration 1 Register (DIGIO_PINMUX) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_DIGIO_PINMUX_t
 *  \brief Pinmux Configuration 1 Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_DIGIO_PINMUX_t {
    union {
        struct {
             uint16_t RESERVED0        : 1; /**< Reserved */
             uint16_t DIGIO_LED1_PINMUX : 3; /**< Pin Mux Select for LED_1 */
             uint16_t RESERVED4        : 12; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_DIGIO_PINMUX_t;

/*@}*/

/** @defgroup DIGIO_PINMUX2 Pinmux Configuration 2 Register (DIGIO_PINMUX2) Register
 *  Pinmux Configuration 2 Register (DIGIO_PINMUX2) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_DIGIO_PINMUX2_t
 *  \brief Pinmux Configuration 2 Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_DIGIO_PINMUX2_t {
    union {
        struct {
             uint16_t DIGIO_TXSOP_PINMUX : 4; /**< Pin Mux Select for TX_SOP: */
             uint16_t DIGIO_RXSOP_PINMUX : 4; /**< Pin Mux Select for RX_SOP */
             uint16_t RESERVED8        : 8; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_DIGIO_PINMUX2_t;

/*@}*/

/** @defgroup LED0_BLINK_TIME_CNTRL LED 0 ON/_OFF Blink Time Register (LED0_BLINK_TIME_CNTRL) Register
 *  LED 0 ON/_OFF Blink Time Register (LED0_BLINK_TIME_CNTRL) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_LED0_BLINK_TIME_CNTRL_t
 *  \brief LED 0 ON/_OFF Blink Time Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_LED0_BLINK_TIME_CNTRL_t {
    union {
        struct {
             uint16_t LED0_OFF_N4MS    : 8; /**< Led 0 Off Blink Time. */
             uint16_t LED0_ON_N4MS     : 8; /**< LED 0 On Blink Time */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_LED0_BLINK_TIME_CNTRL_t;

/*@}*/

/** @defgroup LED1_BLINK_TIME_CNTRL LED 1 ON/_OFF Blink Time Register (LED1_BLINK_TIME_CNTRL) Register
 *  LED 1 ON/_OFF Blink Time Register (LED1_BLINK_TIME_CNTRL) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_LED1_BLINK_TIME_CNTRL_t
 *  \brief LED 1 ON/_OFF Blink Time Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_LED1_BLINK_TIME_CNTRL_t {
    union {
        struct {
             uint16_t LED1_OFF_N4MS    : 8; /**< LED 1 Off Blink Time */
             uint16_t LED1_ON_N4MS     : 8; /**< LED 1 On Blink Time */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_LED1_BLINK_TIME_CNTRL_t;

/*@}*/

/** @defgroup LED_CNTRL LED Control Register (LED_CNTRL) Register
 *  LED Control Register (LED_CNTRL) Register
 *  @{
 */

/*! ========================================================================
 *  \enum ADI_ADIN1100_LED_CNTRL_LED0_FUNCTION
 *  \brief LED 0 Pin Function
 *  ======================================================================== */
typedef enum
{
     ADIN1100_LED_CNTRL_LED0_FUNCTION_LINKUP_TXRX_ACTIVITY = 0,    /**< LINKUP_TXRX_ACTIVITY */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_LINKUP_TX_ACTIVITY = 1,    /**< LINKUP_TX_ACTIVITY */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_LINKUP_RX_ACTIVITY = 2,    /**< LINKUP_RX_ACTIVITY */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_LINKUP_ONLY = 3,    /**< LINKUP_ONLY */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_TXRX_ACTIVITY = 4,    /**< TXRX_ACTIVITY */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_TX_ACTIVITY = 5,    /**< TX_ACTIVITY */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_RX_ACTIVITY = 6,    /**< RX_ACTIVITY */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_LINKUP_RX_ER = 7,    /**< LINKUP_RX_ER */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_LINKUP_RX_TX_ER = 8,    /**< LINKUP_RX_TX_ER */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_RX_ER = 9,    /**< RX_ER */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_RX_TX_ER = 10,    /**< RX_TX_ER */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_TX_SOP = 11,    /**< TX_SOP */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_RX_SOP = 12,    /**< RX_SOP */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_ON = 13,    /**< ON */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_OFF = 14,    /**< OFF */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_BLINK = 15,    /**< BLINK */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_TX_LEVEL_2P4 = 16,    /**< TX_LEVEL_2P4 */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_TX_LEVEL_1P0 = 17,    /**< TX_LEVEL_1P0 */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_MASTER = 18,    /**< MASTER */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_SLAVE = 19,    /**< SLAVE */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_INCOMPATIBLE_LINK_CFG = 20,    /**< INCOMPATIBLE_LINK_CFG */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_AN_LINK_GOOD = 21,    /**< AN_LINK_GOOD */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_AN_COMPLETE = 22,    /**< AN_COMPLETE */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_TS_TIMER = 23,    /**< TS_TIMER */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_LOC_RCVR_STATUS = 24,    /**< LOC_RCVR_STATUS */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_REM_RCVR_STATUS = 25,    /**< REM_RCVR_STATUS */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_CLK25_REF = 26,    /**< CLK25_REF */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_TX_TCLK = 27,    /**< TX_TCLK */
     ADIN1100_LED_CNTRL_LED0_FUNCTION_CLK_120MHZ = 28     /**< CLK_120MHZ */
}  ADI_ADIN1100_LED_CNTRL_LED0_FUNCTION;

/*! ========================================================================
 *  \enum ADI_ADIN1100_LED_CNTRL_LED0_MODE
 *  \brief LED 0 Mode Selection
 *  ======================================================================== */
typedef enum
{
     ADIN1100_LED_CNTRL_LED0_MODE_LED_MODE1 = 0,    /**< LED_MODE1 */
     ADIN1100_LED_CNTRL_LED0_MODE_LED_MODE2 = 1     /**< LED_MODE2 */
}  ADI_ADIN1100_LED_CNTRL_LED0_MODE;

/*! ========================================================================
 *  \enum ADI_ADIN1100_LED_CNTRL_LED1_FUNCTION
 *  \brief LED 1 Pin Function
 *  ======================================================================== */
typedef enum
{
     ADIN1100_LED_CNTRL_LED1_FUNCTION_LINKUP_TXRX_ACTIVITY = 0,    /**< LINKUP_TXRX_ACTIVITY */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_LINKUP_TX_ACTIVITY = 1,    /**< LINKUP_TX_ACTIVITY */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_LINKUP_RX_ACTIVITY = 2,    /**< LINKUP_RX_ACTIVITY */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_LINKUP_ONLY = 3,    /**< LINKUP_ONLY */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_TXRX_ACTIVITY = 4,    /**< TXRX_ACTIVITY */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_TX_ACTIVITY = 5,    /**< TX_ACTIVITY */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_RX_ACTIVITY = 6,    /**< RX_ACTIVITY */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_LINKUP_RX_ER = 7,    /**< LINKUP_RX_ER */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_LINKUP_RX_TX_ER = 8,    /**< LINKUP_RX_TX_ER */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_RX_ER = 9,    /**< RX_ER */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_RX_TX_ER = 10,    /**< RX_TX_ER */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_TX_SOP = 11,    /**< TX_SOP */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_RX_SOP = 12,    /**< RX_SOP */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_ON = 13,    /**< ON */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_OFF = 14,    /**< OFF */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_BLINK = 15,    /**< BLINK */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_TX_LEVEL_2P4 = 16,    /**< TX_LEVEL_2P4 */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_TX_LEVEL_1P0 = 17,    /**< TX_LEVEL_1P0 */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_MASTER = 18,    /**< MASTER */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_SLAVE = 19,    /**< SLAVE */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_INCOMPATIBLE_LINK_CFG = 20,    /**< INCOMPATIBLE_LINK_CFG */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_AN_LINK_GOOD = 21,    /**< AN_LINK_GOOD */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_AN_COMPLETE = 22,    /**< AN_COMPLETE */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_TS_TIMER = 23,    /**< TS_TIMER */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_LOC_RCVR_STATUS = 24,    /**< LOC_RCVR_STATUS */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_REM_RCVR_STATUS = 25,    /**< REM_RCVR_STATUS */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_CLK25_REF = 26,    /**< CLK25_REF */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_TX_TCLK = 27,    /**< TX_TCLK */
     ADIN1100_LED_CNTRL_LED1_FUNCTION_CLK_120MHZ = 28     /**< CLK_120MHZ */
}  ADI_ADIN1100_LED_CNTRL_LED1_FUNCTION;

/*! ========================================================================
 *  \enum ADI_ADIN1100_LED_CNTRL_LED1_MODE
 *  \brief LED 1 Mode Selection
 *  ======================================================================== */
typedef enum
{
     ADIN1100_LED_CNTRL_LED1_MODE_LED_MODE1 = 0,    /**< LED_MODE1 */
     ADIN1100_LED_CNTRL_LED1_MODE_LED_MODE2 = 1     /**< LED_MODE2 */
}  ADI_ADIN1100_LED_CNTRL_LED1_MODE;

/*! ========================================================================
 *  \struct ADI_ADIN1100_LED_CNTRL_t
 *  \brief LED Control Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_LED_CNTRL_t {
    union {
        struct {
             uint16_t LED0_FUNCTION    : 5; /**< LED 0 Pin Function */
             uint16_t LED0_MODE        : 1; /**< LED 0 Mode Selection */
             uint16_t LED0_LINK_ST_QUALIFY : 1; /**< Qualify Certain LED 0 Options with LINK_STATUS */
             uint16_t LED0_EN          : 1; /**< LED 0 Enable */
             uint16_t LED1_FUNCTION    : 5; /**< LED 1 Pin Function */
             uint16_t LED1_MODE        : 1; /**< LED 1 Mode Selection */
             uint16_t LED1_LINK_ST_QUALIFY : 1; /**< Qualify Certain LED 1 Options with LINK_STATUS */
             uint16_t LED1_EN          : 1; /**< LED 1 Enable */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_LED_CNTRL_t;

/*@}*/

/** @defgroup LED_POLARITY LED Polarity Register (LED_POLARITY) Register
 *  LED Polarity Register (LED_POLARITY) Register
 *  @{
 */

/*! ========================================================================
 *  \enum ADI_ADIN1100_LED_POLARITY_LED0_POLARITY
 *  \brief LED 0 Polarity
 *  ======================================================================== */
typedef enum
{
     ADIN1100_LED_POLARITY_LED0_POLARITY_LED_AUTO_SENSE = 0,    /**< LED Auto Sense */
     ADIN1100_LED_POLARITY_LED0_POLARITY_LED_ACTIVE_HI = 1,    /**< LED Active High */
     ADIN1100_LED_POLARITY_LED0_POLARITY_LED_ACTIVE_LO = 2     /**< LED Active Low */
}  ADI_ADIN1100_LED_POLARITY_LED0_POLARITY;

/*! ========================================================================
 *  \enum ADI_ADIN1100_LED_POLARITY_LED1_POLARITY
 *  \brief LED 1 Polarity
 *  ======================================================================== */
typedef enum
{
     ADIN1100_LED_POLARITY_LED1_POLARITY_LED_AUTO_SENSE = 0,    /**< LED Auto Sense */
     ADIN1100_LED_POLARITY_LED1_POLARITY_LED_ACTIVE_HI = 1,    /**< LED Active High */
     ADIN1100_LED_POLARITY_LED1_POLARITY_LED_ACTIVE_LO = 2     /**< LED Active Low */
}  ADI_ADIN1100_LED_POLARITY_LED1_POLARITY;

/*! ========================================================================
 *  \struct ADI_ADIN1100_LED_POLARITY_t
 *  \brief LED Polarity Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_LED_POLARITY_t {
    union {
        struct {
             uint16_t LED0_POLARITY    : 2; /**< LED 0 Polarity */
             uint16_t LED1_POLARITY    : 2; /**< LED 1 Polarity */
             uint16_t RESERVED4        : 12; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_LED_POLARITY_t;

/*@}*/

/** @defgroup MMD2_DEV_ID1 Vendor Specific MMD 2 Device Identifier High Register. (MMD2_DEV_ID1) Register
 *  Vendor Specific MMD 2 Device Identifier High Register. (MMD2_DEV_ID1) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_MMD2_DEV_ID1_t
 *  \brief Vendor Specific MMD 2 Device Identifier High Register.
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_MMD2_DEV_ID1_t {
    union {
        struct {
             uint16_t MMD2_DEV_ID1     : 16; /**< Vendor specific 2 MMD device identifier */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_MMD2_DEV_ID1_t;

/*@}*/

/** @defgroup MMD2_DEV_ID2 Vendor Specific MMD 2 Device Identifier Low Register. (MMD2_DEV_ID2) Register
 *  Vendor Specific MMD 2 Device Identifier Low Register. (MMD2_DEV_ID2) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_MMD2_DEV_ID2_t
 *  \brief Vendor Specific MMD 2 Device Identifier Low Register.
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_MMD2_DEV_ID2_t {
    union {
        struct {
             uint16_t MMD2_REV_NUM     : 4; /**< Revision number */
             uint16_t MMD2_MODEL_NUM   : 6; /**< Model Number */
             uint16_t MMD2_DEV_ID2_OUI : 6; /**< OUI bits */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_MMD2_DEV_ID2_t;

/*@}*/

/** @defgroup MMD2_DEVS_IN_PKG1 Vendor Specific 2 MMD Devices in Package Register (MMD2_DEVS_IN_PKG1) Register
 *  Vendor Specific 2 MMD Devices in Package Register (MMD2_DEVS_IN_PKG1) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_MMD2_DEVS_IN_PKG1_t
 *  \brief Vendor Specific 2 MMD Devices in Package Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_MMD2_DEVS_IN_PKG1_t {
    union {
        struct {
             uint16_t MMD2_DEVS_IN_PKG1 : 16; /**< Vendor Specific 2 MMD Devices in Package */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_MMD2_DEVS_IN_PKG1_t;

/*@}*/

/** @defgroup MMD2_DEVS_IN_PKG2 Vendor Specific 2 MMD Devices in Package Register (MMD2_DEVS_IN_PKG2) Register
 *  Vendor Specific 2 MMD Devices in Package Register (MMD2_DEVS_IN_PKG2) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_MMD2_DEVS_IN_PKG2_t
 *  \brief Vendor Specific 2 MMD Devices in Package Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_MMD2_DEVS_IN_PKG2_t {
    union {
        struct {
             uint16_t MMD2_DEVS_IN_PKG2 : 16; /**< Vendor Specific 2 MMD Devices in Package */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_MMD2_DEVS_IN_PKG2_t;

/*@}*/

/** @defgroup MMD2_STATUS Vendor Specific MMD 2 Status Register (MMD2_STATUS) Register
 *  Vendor Specific MMD 2 Status Register (MMD2_STATUS) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_MMD2_STATUS_t
 *  \brief Vendor Specific MMD 2 Status Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_MMD2_STATUS_t {
    union {
        struct {
             uint16_t RESERVED0        : 14; /**< Reserved */
             uint16_t MMD2_STATUS      : 2; /**< Vendor specific 2 MMD Status */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_MMD2_STATUS_t;

/*@}*/

/** @defgroup PHY_SUBSYS_IRQ_STATUS PHY Subsystem Interrupt Status Register (PHY_SUBSYS_IRQ_STATUS) Register
 *  PHY Subsystem Interrupt Status Register (PHY_SUBSYS_IRQ_STATUS) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_PHY_SUBSYS_IRQ_STATUS_t
 *  \brief PHY Subsystem Interrupt Status Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_PHY_SUBSYS_IRQ_STATUS_t {
    union {
        struct {
             uint16_t RESERVED0        : 1; /**< Reserved */
             uint16_t LINK_STAT_CHNG_LH : 1; /**< Link Status Change. */
             uint16_t RESERVED2        : 9; /**< Reserved */
             uint16_t AN_STAT_CHNG_IRQ_LH : 1; /**< Autonegotiation Status Change Interrupt. */
             uint16_t RESERVED12       : 1; /**< Reserved */
             uint16_t MAC_IF_EBUF_ERR_IRQ_LH : 1; /**< Mac Interface Buffers Overflow/underflow Interrupt */
             uint16_t MAC_IF_FC_FG_IRQ_LH : 1; /**< Mac Interface Frame CHECKER/_GENERATOR Interrupt */
             uint16_t RESERVED15       : 1; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_PHY_SUBSYS_IRQ_STATUS_t;

/*@}*/

/** @defgroup PHY_SUBSYS_IRQ_MASK PHY Subsystem Interrupt Mask Register (PHY_SUBSYS_IRQ_MASK) Register
 *  PHY Subsystem Interrupt Mask Register (PHY_SUBSYS_IRQ_MASK) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_PHY_SUBSYS_IRQ_MASK_t
 *  \brief PHY Subsystem Interrupt Mask Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_PHY_SUBSYS_IRQ_MASK_t {
    union {
        struct {
             uint16_t RESERVED0        : 1; /**< Reserved */
             uint16_t LINK_STAT_CHNG_IRQ_EN : 1; /**< Enable Link Status Change Interrupt. */
             uint16_t RESERVED2        : 9; /**< Reserved */
             uint16_t AN_STAT_CHNG_IRQ_EN : 1; /**< Enable Autonegotiation Status Change Interrupt. */
             uint16_t RESERVED12       : 1; /**< Reserved */
             uint16_t MAC_IF_EBUF_ERR_IRQ_EN : 1; /**< Enable Mac Interface Buffers Overflow/underflow Interrupt */
             uint16_t MAC_IF_FC_FG_IRQ_EN : 1; /**< Enable Mac Interface Frame CHECKER/_GENERATOR Interrupt */
             uint16_t RESERVED15       : 1; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_PHY_SUBSYS_IRQ_MASK_t;

/*@}*/

/** @defgroup FC_EN Frame Checker Enable Register (FC_EN) Register
 *  Frame Checker Enable Register (FC_EN) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_FC_EN_t
 *  \brief Frame Checker Enable Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_FC_EN_t {
    union {
        struct {
             uint16_t FC_EN            : 1; /**< Frame Checker Enable */
             uint16_t RESERVED1        : 15; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_FC_EN_t;

/*@}*/

/** @defgroup FC_IRQ_EN Frame Checker Interrupt Enable Register (FC_IRQ_EN) Register
 *  Frame Checker Interrupt Enable Register (FC_IRQ_EN) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_FC_IRQ_EN_t
 *  \brief Frame Checker Interrupt Enable Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_FC_IRQ_EN_t {
    union {
        struct {
             uint16_t FC_IRQ_EN        : 1; /**< Frame Checker Interrupt Enable */
             uint16_t RESERVED1        : 15; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_FC_IRQ_EN_t;

/*@}*/

/** @defgroup FC_TX_SEL Frame Checker Transmit Select Register (FC_TX_SEL) Register
 *  Frame Checker Transmit Select Register (FC_TX_SEL) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_FC_TX_SEL_t
 *  \brief Frame Checker Transmit Select Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_FC_TX_SEL_t {
    union {
        struct {
             uint16_t FC_TX_SEL        : 1; /**< Frame Checker Transmit Select */
             uint16_t RESERVED1        : 15; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_FC_TX_SEL_t;

/*@}*/

/** @defgroup RX_ERR_CNT Receive Error Count Register (RX_ERR_CNT) Register
 *  Receive Error Count Register (RX_ERR_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_RX_ERR_CNT_t
 *  \brief Receive Error Count Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_RX_ERR_CNT_t {
    union {
        struct {
             int16_t RX_ERR_CNT       : 16; /**< Receive Error Count */
        };
        int16_t VALUE16;
    };
} ADI_ADIN1100_RX_ERR_CNT_t;

/*@}*/

/** @defgroup FC_FRM_CNT_H Frame Checker Count High Register (FC_FRM_CNT_H) Register
 *  Frame Checker Count High Register (FC_FRM_CNT_H) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_FC_FRM_CNT_H_t
 *  \brief Frame Checker Count High Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_FC_FRM_CNT_H_t {
    union {
        struct {
             int16_t FC_FRM_CNT_H     : 16; /**< Bits [31:16] of Latched Copy of the Number of Received Frames */
        };
        int16_t VALUE16;
    };
} ADI_ADIN1100_FC_FRM_CNT_H_t;

/*@}*/

/** @defgroup FC_FRM_CNT_L Frame Checker Count Low Register (FC_FRM_CNT_L) Register
 *  Frame Checker Count Low Register (FC_FRM_CNT_L) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_FC_FRM_CNT_L_t
 *  \brief Frame Checker Count Low Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_FC_FRM_CNT_L_t {
    union {
        struct {
             int16_t FC_FRM_CNT_L     : 16; /**< Bits [15:0] of Latched Copy of the Number of Received Frames */
        };
        int16_t VALUE16;
    };
} ADI_ADIN1100_FC_FRM_CNT_L_t;

/*@}*/

/** @defgroup FC_LEN_ERR_CNT Frame Checker Length Error Count Register (FC_LEN_ERR_CNT) Register
 *  Frame Checker Length Error Count Register (FC_LEN_ERR_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_FC_LEN_ERR_CNT_t
 *  \brief Frame Checker Length Error Count Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_FC_LEN_ERR_CNT_t {
    union {
        struct {
             int16_t FC_LEN_ERR_CNT   : 16; /**< Latched Copy of the Frame Length Error Counter */
        };
        int16_t VALUE16;
    };
} ADI_ADIN1100_FC_LEN_ERR_CNT_t;

/*@}*/

/** @defgroup FC_ALGN_ERR_CNT Frame Checker Alignment Error Count Register (FC_ALGN_ERR_CNT) Register
 *  Frame Checker Alignment Error Count Register (FC_ALGN_ERR_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_FC_ALGN_ERR_CNT_t
 *  \brief Frame Checker Alignment Error Count Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_FC_ALGN_ERR_CNT_t {
    union {
        struct {
             int16_t FC_ALGN_ERR_CNT  : 16; /**< Latched Copy of the Frame Alignment Error Counter */
        };
        int16_t VALUE16;
    };
} ADI_ADIN1100_FC_ALGN_ERR_CNT_t;

/*@}*/

/** @defgroup FC_SYMB_ERR_CNT Frame Checker Symbol Error Count Register (FC_SYMB_ERR_CNT) Register
 *  Frame Checker Symbol Error Count Register (FC_SYMB_ERR_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_FC_SYMB_ERR_CNT_t
 *  \brief Frame Checker Symbol Error Count Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_FC_SYMB_ERR_CNT_t {
    union {
        struct {
             int16_t FC_SYMB_ERR_CNT  : 16; /**< Latched Copy of the Symbol Error Counter */
        };
        int16_t VALUE16;
    };
} ADI_ADIN1100_FC_SYMB_ERR_CNT_t;

/*@}*/

/** @defgroup FC_OSZ_CNT Frame Checker Oversized Frame Count Register (FC_OSZ_CNT) Register
 *  Frame Checker Oversized Frame Count Register (FC_OSZ_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_FC_OSZ_CNT_t
 *  \brief Frame Checker Oversized Frame Count Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_FC_OSZ_CNT_t {
    union {
        struct {
             int16_t FC_OSZ_CNT       : 16; /**< Latched copy of the Overisized Frame Error Counter */
        };
        int16_t VALUE16;
    };
} ADI_ADIN1100_FC_OSZ_CNT_t;

/*@}*/

/** @defgroup FC_USZ_CNT Frame Checker Undersized Frame Count Register (FC_USZ_CNT) Register
 *  Frame Checker Undersized Frame Count Register (FC_USZ_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_FC_USZ_CNT_t
 *  \brief Frame Checker Undersized Frame Count Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_FC_USZ_CNT_t {
    union {
        struct {
             int16_t FC_USZ_CNT       : 16; /**< Latched Copy of the Undersized Frame Error Counter */
        };
        int16_t VALUE16;
    };
} ADI_ADIN1100_FC_USZ_CNT_t;

/*@}*/

/** @defgroup FC_ODD_CNT Frame Checker Odd Nibble Frame Count Register (FC_ODD_CNT) Register
 *  Frame Checker Odd Nibble Frame Count Register (FC_ODD_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_FC_ODD_CNT_t
 *  \brief Frame Checker Odd Nibble Frame Count Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_FC_ODD_CNT_t {
    union {
        struct {
             int16_t FC_ODD_CNT       : 16; /**< Latched Copy of the Odd Nibble Counter */
        };
        int16_t VALUE16;
    };
} ADI_ADIN1100_FC_ODD_CNT_t;

/*@}*/

/** @defgroup FC_ODD_PRE_CNT Frame Checker Odd Preamble Packet Count Register (FC_ODD_PRE_CNT) Register
 *  Frame Checker Odd Preamble Packet Count Register (FC_ODD_PRE_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_FC_ODD_PRE_CNT_t
 *  \brief Frame Checker Odd Preamble Packet Count Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_FC_ODD_PRE_CNT_t {
    union {
        struct {
             int16_t FC_ODD_PRE_CNT   : 16; /**< Latched Copy of the Odd Preamble Packet Counter */
        };
        int16_t VALUE16;
    };
} ADI_ADIN1100_FC_ODD_PRE_CNT_t;

/*@}*/

/** @defgroup FC_FALSE_CARRIER_CNT Frame Checker False Carrier Count Register (FC_FALSE_CARRIER_CNT) Register
 *  Frame Checker False Carrier Count Register (FC_FALSE_CARRIER_CNT) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_FC_FALSE_CARRIER_CNT_t
 *  \brief Frame Checker False Carrier Count Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_FC_FALSE_CARRIER_CNT_t {
    union {
        struct {
             int16_t FC_FALSE_CARRIER_CNT : 16; /**< Latched Copy of the False Carrier Events Counter */
        };
        int16_t VALUE16;
    };
} ADI_ADIN1100_FC_FALSE_CARRIER_CNT_t;

/*@}*/

/** @defgroup FG_EN Frame Generator Enable Register (FG_EN) Register
 *  Frame Generator Enable Register (FG_EN) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_FG_EN_t
 *  \brief Frame Generator Enable Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_FG_EN_t {
    union {
        struct {
             uint16_t FG_EN            : 1; /**< Frame Generator Enable */
             uint16_t RESERVED1        : 15; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_FG_EN_t;

/*@}*/

/** @defgroup FG_CNTRL_RSTRT Frame Generator CONTROL/_RESTART Register (FG_CNTRL_RSTRT) Register
 *  Frame Generator CONTROL/_RESTART Register (FG_CNTRL_RSTRT) Register
 *  @{
 */

/*! ========================================================================
 *  \enum ADI_ADIN1100_FG_CNTRL_RSTRT_FG_CNTRL
 *  \brief Frame Generator Control
 *  ======================================================================== */
typedef enum
{
     ADIN1100_FG_CNTRL_RSTRT_FG_CNTRL_FG_GEN_NONE = 0,    /**< No description provided */
     ADIN1100_FG_CNTRL_RSTRT_FG_CNTRL_FG_GEN_RANDOM_PAYLOAD = 1,    /**< No description provided */
     ADIN1100_FG_CNTRL_RSTRT_FG_CNTRL_FG_GEN_0X00_PAYLOAD = 2,    /**< No description provided */
     ADIN1100_FG_CNTRL_RSTRT_FG_CNTRL_FG_GEN_0XFF_PAYLOAD = 3,    /**< No description provided */
     ADIN1100_FG_CNTRL_RSTRT_FG_CNTRL_FG_GEN_0X55_PAYLOAD = 4,    /**< No description provided */
     ADIN1100_FG_CNTRL_RSTRT_FG_CNTRL_FG_GEN_DECR_PAYLOAD = 5     /**< No description provided */
}  ADI_ADIN1100_FG_CNTRL_RSTRT_FG_CNTRL;

/*! ========================================================================
 *  \struct ADI_ADIN1100_FG_CNTRL_RSTRT_t
 *  \brief Frame Generator CONTROL/_RESTART Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_FG_CNTRL_RSTRT_t {
    union {
        struct {
             uint16_t FG_CNTRL         : 3; /**< Frame Generator Control */
             uint16_t FG_RSTRT         : 1; /**< Frame Generator Restart */
             uint16_t RESERVED4        : 12; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_FG_CNTRL_RSTRT_t;

/*@}*/

/** @defgroup FG_CONT_MODE_EN Frame Generator Continuous Mode Enable Register (FG_CONT_MODE_EN) Register
 *  Frame Generator Continuous Mode Enable Register (FG_CONT_MODE_EN) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_FG_CONT_MODE_EN_t
 *  \brief Frame Generator Continuous Mode Enable Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_FG_CONT_MODE_EN_t {
    union {
        struct {
             uint16_t FG_CONT_MODE_EN  : 1; /**< Frame Generator Continuous Mode Enable */
             uint16_t RESERVED1        : 15; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_FG_CONT_MODE_EN_t;

/*@}*/

/** @defgroup FG_IRQ_EN Frame Generator Interrupt Enable Register (FG_IRQ_EN) Register
 *  Frame Generator Interrupt Enable Register (FG_IRQ_EN) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_FG_IRQ_EN_t
 *  \brief Frame Generator Interrupt Enable Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_FG_IRQ_EN_t {
    union {
        struct {
             uint16_t FG_IRQ_EN        : 1; /**< Frame Generator Interrupt Enable */
             uint16_t RESERVED1        : 15; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_FG_IRQ_EN_t;

/*@}*/

/** @defgroup FG_FRM_LEN Frame Generator Frame Length Register (FG_FRM_LEN) Register
 *  Frame Generator Frame Length Register (FG_FRM_LEN) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_FG_FRM_LEN_t
 *  \brief Frame Generator Frame Length Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_FG_FRM_LEN_t {
    union {
        struct {
             int16_t FG_FRM_LEN       : 16; /**< The Data Field Frame Length in Bytes */
        };
        int16_t VALUE16;
    };
} ADI_ADIN1100_FG_FRM_LEN_t;

/*@}*/

/** @defgroup FG_NFRM_H Frame Generator Number of Frames High Register (FG_NFRM_H) Register
 *  Frame Generator Number of Frames High Register (FG_NFRM_H) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_FG_NFRM_H_t
 *  \brief Frame Generator Number of Frames High Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_FG_NFRM_H_t {
    union {
        struct {
             int16_t FG_NFRM_H        : 16; /**< Bits [31:16] of the Number of Frames to be Generated */
        };
        int16_t VALUE16;
    };
} ADI_ADIN1100_FG_NFRM_H_t;

/*@}*/

/** @defgroup FG_NFRM_L Frame Generator Number of Frames Low Register (FG_NFRM_L) Register
 *  Frame Generator Number of Frames Low Register (FG_NFRM_L) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_FG_NFRM_L_t
 *  \brief Frame Generator Number of Frames Low Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_FG_NFRM_L_t {
    union {
        struct {
             int16_t FG_NFRM_L        : 16; /**< Bits [15:0] of the Number of Frames to be Generated */
        };
        int16_t VALUE16;
    };
} ADI_ADIN1100_FG_NFRM_L_t;

/*@}*/

/** @defgroup FG_DONE Frame Generator Done Register (FG_DONE) Register
 *  Frame Generator Done Register (FG_DONE) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_FG_DONE_t
 *  \brief Frame Generator Done Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_FG_DONE_t {
    union {
        struct {
             uint16_t FG_DONE          : 1; /**< Frame Generator Done */
             uint16_t RESERVED1        : 15; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_FG_DONE_t;

/*@}*/

/** @defgroup RMII_CFG RMII Configuration Register (RMII_CFG) Register
 *  RMII Configuration Register (RMII_CFG) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_RMII_CFG_t
 *  \brief RMII Configuration Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_RMII_CFG_t {
    union {
        struct {
             uint16_t RMII_TXD_CHK_EN  : 1; /**< RMII TXD Check Enable */
             uint16_t RESERVED1        : 15; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_RMII_CFG_t;

/*@}*/

/** @defgroup MAC_IF_LOOPBACK MAC Interface Loopbacks Configuration Register (MAC_IF_LOOPBACK) Register
 *  MAC Interface Loopbacks Configuration Register (MAC_IF_LOOPBACK) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_MAC_IF_LOOPBACK_t
 *  \brief MAC Interface Loopbacks Configuration Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_MAC_IF_LOOPBACK_t {
    union {
        struct {
             uint16_t MAC_IF_LB_EN     : 1; /**< MAC Interface Loopback Enable. */
             uint16_t MAC_IF_LB_TX_SUP_EN : 1; /**< Suppress Transmission Enable. */
             uint16_t MAC_IF_REM_LB_EN : 1; /**< MAC Interface Remote Loopback Enable. */
             uint16_t MAC_IF_REM_LB_RX_SUP_EN : 1; /**< Suppress RX Enable */
             uint16_t RESERVED4        : 12; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_MAC_IF_LOOPBACK_t;

/*@}*/

/** @defgroup MAC_IF_SOP_CNTRL MAC Start Of Packet (SOP) Generation Control Register (MAC_IF_SOP_CNTRL) Register
 *  MAC Start Of Packet (SOP) Generation Control Register (MAC_IF_SOP_CNTRL) Register
 *  @{
 */

/*! ========================================================================
 *  \struct ADI_ADIN1100_MAC_IF_SOP_CNTRL_t
 *  \brief MAC Start Of Packet (SOP) Generation Control Register
 *  ======================================================================== */
typedef struct _ADI_ADIN1100_MAC_IF_SOP_CNTRL_t {
    union {
        struct {
             uint16_t MAC_IF_RX_SOP_DET_EN : 1; /**< Enable the Generation of the Rx SOP Indication Signal. */
             uint16_t MAC_IF_RX_SOP_SFD_EN : 1; /**< Enable Rx SOP Signal Indication on SFD Reception. */
             uint16_t MAC_IF_RX_SOP_LEN_CHK_EN : 1; /**< Enable Rx SOP Preamble Length Check. */
             uint16_t MAC_IF_TX_SOP_DET_EN : 1; /**< Enable the Generation of the Tx SOP Indication Signal. */
             uint16_t MAC_IF_TX_SOP_SFD_EN : 1; /**< Enable Tx SOP Signal Indication on SFD. */
             uint16_t MAC_IF_TX_SOP_LEN_CHK_EN : 1; /**< Enable Tx SOP Preamble Length Check. */
             uint16_t RESERVED6        : 10; /**< Reserved */
        };
        uint16_t VALUE16;
    };
} ADI_ADIN1100_MAC_IF_SOP_CNTRL_t;

/*@}*/

/*! @endcond */

#ifdef __cplusplus
}
#endif

#endif /* SPE_PHY_H */



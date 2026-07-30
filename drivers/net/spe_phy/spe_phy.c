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

#include "spe_phy.h"


/* Forward declarations. */
static int         checkIdentity               (struct spe_phy_desc *desc, uint32_t *modelNum, uint32_t *revNum);
#if defined(ADIN1100)
static int         waitMdio                    (struct spe_phy_desc *desc);
#endif
static int         phyInit                     (struct spe_phy_desc *desc);
static int         phyStaticConfig             (struct spe_phy_desc *desc, uint32_t modelNum, uint32_t revNum);
static void        irqCb                       (void *pCBParam, uint32_t Event, void *pArg);

static const uint16_t convMseToSqi[SPE_PHY_SQI_NUM_ENTRIES - 1] = {
    0x0A74, 0x084E, 0x0698, 0x053D, 0x0429, 0x034E, 0x02A0
};

/*!
 * @brief           PHY device initialization.
 *
 * @param [in]      desc     Device driver handle.
 *
 * @return
 *
 * @details
 *
 * @sa
 */
// int spe_phy_Init(spe_phy_desc **pdesc, HAL_ReadFn_t readFn, HAL_WriteFn_t writeFn)
int spe_phy_init(struct spe_phy_desc **desc, struct spe_phy_init_param *param)
{
    int    result = 0;
    spe_phy_desc    *descriptor;

    if (desc == NULL || param == NULL))
    {
        return -EINVAL;
    }
    
    if (param->dev_mem == NULL)
    {
	descriptor = no_os_calloc(1, sizeof(*descriptor));
	if (!descriptor)
		return -ENOMEM;
    }
    else
    {
	if (param->mem_size < sizeof(struct spe_phy_desc))
	{
		return -EINVAL;
	}

	memset(param->dev_mem, 0, param->mem_size);
	descriptor = (struct spe_phy_desc *)param->dev_mem;
    }

    /* Implies state is uninitialized */
    descriptor->phyAddr = param->addr;
    descriptor->irqPending = false;
    descriptor->readFn = param->readFn;
    descriptor->writeFn = param->writeFn;
    
    /* Reset callback settings */
    descriptor->cb_func = NULL;
    descriptor->cb_events = 0;

    descriptor->adinDevice = param->adinDevice;

    /* Disable IRQ whether the interrupt is enabled or not */
    capi_irq_disable(param->eth_irq);

    /* Only required if the driver is configured to use the PHY interrupt */
    if (param->->enableIrq)
    {
        // ADI_HAL_REGISTER_CALLBACK(descriptor->adinDevice, (HAL_Callback_t const *)(irqCb), descriptor );
	capi_irq_connect(param->eth_irq, (capi_isr_callback_t const *)(irqCb), descriptor)
    }

    result = phyInit(descriptor);
    if (result != 0)
    {
        descriptor->state = SPE_PHY_STATE_ERROR;
        return result;
    }

    if (param->enableIrq)
    {
        /* Enable IRQ */
	capi_irq_enable(param->eth_irq);

        /* We may have a pending IRQ that will be services as soon as the IRQ is enabled, */
        /* set pending IRQ to false and if needed, service it here. */
        descriptor->irqPending = false;
    }

    *desc = descriptor;

    return result;
}

/*!
 * @brief           PHY device uninitialization.
 *
 * @param [in]      desc     Device driver handle.
 * @param [out]
 * @return
 *
 * @details
 *
 * @sa
 */
int spe_phy_remove(struct spe_phy_desc *desc)
{
    int    result = 0;

    if (desc == NULL)
    {
        return -EINVAL;
    }

    capi_irq_disable(param->eth_irq);

    desc->state = SPE_PHY_STATE_UNINITIALIZED;

    if (desc->dev_mem == NULL)
    {
	no_os_free(desc);
    }
    else
    {
	memset(desc, 0, sizeof(struct spe_phy_desc));
    }

    return result;
}


int spe_phy_reinit_phy(struct spe_phy_desc *desc)
{
    if (desc == NULL)
    {
        return -EINVAL;
    }
    
    return phyInit(desc);
}


/*!
 * @brief           Local function called by PHY initialization APIs.
 *
 * @param [in]      desc     Device driver handle.
 * @param [out]
 * @return          - 0
 *                  - SPE_ETH_HW_ERROR
 *
 * @details
 *
 * @sa
 */
static int phyInit(struct spe_phy_desc *desc)
{
    int    result = 0;
    uint16_t            val16;
    uint32_t            irqMask;
    bool                flag;
    int32_t             iter;
    uint32_t            modelNum;
    uint32_t            revNum;

    desc->state = SPE_PHY_STATE_UNINITIALIZED;

#ifdef ADIN1100
    /* Wait until MDIO interface is up, in case this function is called during the powerup sequence of the PHY. */
    result = waitMdio(desc);
    if (result != 0)
    {
        desc->state = SPE_PHY_STATE_ERROR;
        return result;
    }
#endif

    /* Checks the identity of the device based on reading of hardware ID registers */
    /* Ensures the device is supported by the driver, otherwise an error is reported. */
    result = checkIdentity(desc, &modelNum, &revNum);
    if (result != 0)
    {
        desc->state = SPE_PHY_STATE_ERROR;
        return result;
    }

    /* Go to software powerdown, this may already be achieved through pin strap options. */
    /* Note this is not using the driver function because we use a different timeout     */
    /* scheme to account for the powerup sequence of the system included in this step.   */
    val16 = 1 << BITP_CRSM_SFT_PD_CNTRL_CRSM_SFT_PD;
    result = spe_phy_write(desc, ADDR_CRSM_SFT_PD_CNTRL, val16);
    if (result != 0)
    {
        return result;
    }

    iter = SPE_PHY_SYS_RDY_ITER;
    do
    {
        result = spe_phy_get_software_powerdown(desc, &flag);
        if (result != 0)
        {
            return result;
        }
    } while (!flag && (--iter));

    /* Values of event enums are identical to respective interrupt masks */
    /* Hardware reset and hardware error interrupts are always enabled   */
    irqMask = SPE_PHY_CRSM_HW_ERROR | BITM_CRSM_IRQ_MASK_CRSM_HRD_RST_IRQ_EN | desc->cb_events;
    result = spe_phy_write(desc, ADDR_CRSM_IRQ_MASK, (irqMask & 0xFFFF));
    if (result != 0)
    {
        return result;
    }
    result = spe_phy_write(desc, ADDR_PHY_SUBSYS_IRQ_MASK, ((irqMask >> 16) & 0xFFFF));
    if (result != 0)
    {
        return result;
    }

    /* Read IRQ status bits to clear them before enabling IRQ. */
    /* Hardware errors could be asserted if for, we don't care about the contents */
    /* so we just discard the read values. */
    result = spe_phy_read(desc, ADDR_CRSM_IRQ_STATUS, &val16);
    if (result != 0)
    {
        return result;
    }

    if (val16 & SPE_PHY_CRSM_HW_ERROR)
    {
        return SPE_ETH_HW_ERROR;
    }

    result = spe_phy_read(desc, ADDR_PHY_SUBSYS_IRQ_STATUS, &val16);
    if (result != 0)
    {
        return result;
    }

    desc->irqPending = false;

    /* Static configuration: default settings that are different from hardware reset values */
    result = phyStaticConfig(desc, modelNum, revNum);
    if (result != 0)
    {
        return SPE_ETH_PLACEHOLDER_ERROR;
    }

    /* Make sure auto-negotiation is enabled. */
    result = spe_phy_AnEnable(desc, true);
    if (result != 0)
    {
        return result;
    }

    /* At then end of successful initialization, the PHY is in software powerdown. */
    desc->state = SPE_PHY_STATE_SOFTWARE_POWERDOWN;

    return result;
}

/**************************************************/
/***                                            ***/
/***             Interrupt Handling             ***/
/***                                            ***/
/**************************************************/

/*
 * @brief
 *
 * @param [in]      desc     Device driver handle.
 * @param [out]
 * @return
 *
 * @details
 *
 * @sa
 */
int spe_phy_register_callback(struct spe_phy_desc *desc, adi_eth_Callback_t cb_func, uint32_t cb_events, void *cb_param)
{
    int    result = 0;

    desc->cb_func = cb_func;
    desc->cb_events = cb_events;
    desc->cb_param = cb_param;

    /* Values of event enums are identical to respective interrupt masks */
    /* Hardware reset and hardware error interrupts are always enabled   */
    uint32_t irqMask = SPE_PHY_CRSM_HW_ERROR | BITM_CRSM_IRQ_MASK_CRSM_HRD_RST_IRQ_EN | desc->cb_events;

    result = spe_phy_write(desc, ADDR_CRSM_IRQ_MASK, (irqMask & 0xFFFF));
    if (result != 0)
    {
        return result;
    }
    result = spe_phy_write(desc, ADDR_PHY_SUBSYS_IRQ_MASK, ((irqMask >> 16) & 0xFFFF));

    return result;
}


/*
 * @brief           PHY interrupt callback.
 *
 * @param [in]      desc     Device driver handle.
 * @param [out]
 * @return
 *
 * @details
 *
 * @sa
 */
static void irqCb(void *pCBParam, uint32_t Event, void *pArg)
{
    struct spe_phy_desc    *desc = (struct spe_phy_desc *)pCBParam;

    (void)Event;
    (void)pArg;

    /* Set this flag to ensure the IRQ is handled before other actions are taken */
    /* The interrupt may be triggered by a hardware reset, in which case the PHY */
    /* will likely need to be reconfigured.                                      */
    /* The flag is cleared when interrupt status registers are read. Taking      */
    /* appropriate action is the responsibility of the caller.                   */
    desc->irqPending = true;
    if (desc->cb_func != NULL)
    {
        desc->cb_func(desc->cb_param, 0, NULL);
    }
}

/*
 * @brief           Read interrupt status.
 *
 * @param [in]      desc     Device driver handle.
 * @param [out]
 * @return
 *
 * @details
 *
 * @sa
 */
int spe_phy_read_irq_status(struct spe_phy_desc *desc, uint32_t *status)
{
    int    result = 0;
    uint16_t            val16;

    *status = 0;

    result = spe_phy_read(desc, ADDR_CRSM_IRQ_STATUS, &val16);
    if (result != 0)
    {
        return result;
    }
    else
    {
        *status = val16;

        result = spe_phy_read(desc, ADDR_PHY_SUBSYS_IRQ_STATUS, &val16);
        if (result != 0)
        {
            /* Only CRSM_IRQ_STATUS values returned are valid */
            result = -ECOMM;
        }
        *status |= (val16 << 16);

        desc->irqPending = false;
    }

    return result;
}

/**************************************************/
/***                                            ***/
/***                Configuration               ***/
/***                                            ***/
/**************************************************/

/*
 * @brief           Configure the advertised transmit operating mode for auto-negotiation.
 *
 * @param [in]      desc     Device driver handle.
 * @param [out]
 * @return
 *
 * @details
 *
 * @sa
 */
int spe_phy_an_adv_tx_mode(struct spe_phy_desc *desc, enum spe_phy_an_adv_tx_req_mode tx_mode)
{
    int    result = 0;
    uint16_t            val16;

    if (desc->irqPending)
    {
        return SPE_ETH_IRQ_PENDING;
    }

    result = spe_phy_read(desc, ADDR_AN_ADV_ABILITY_H, &val16);
    if (result != 0)
    {
        return result;
    }

    val16 &= ~(BITM_AN_ADV_ABILITY_H_AN_ADV_B10L_TX_LVL_HI_REQ | BITM_AN_ADV_ABILITY_H_AN_ADV_B10L_TX_LVL_HI_ABL);
    if ((tx_mode == SPE_PHY_AN_ADV_TX_REQ_2P4V) || (tx_mode == SPE_PHY_AN_ADV_TX_REQ_1P0V_ABLE_2P4V))
    {
      val16 |= (1 << BITP_AN_ADV_ABILITY_H_AN_ADV_B10L_TX_LVL_HI_ABL);
    }
    if (tx_mode == SPE_PHY_AN_ADV_TX_REQ_2P4V)
    {
      val16 |= (1 << BITP_AN_ADV_ABILITY_H_AN_ADV_B10L_TX_LVL_HI_REQ);
    }
    result = spe_phy_write(desc, ADDR_AN_ADV_ABILITY_H, val16);

    return result;
}


/*
 * @brief           Configure the advertised master/slave configuration for auto-negotiation.
 *
 * @param [in]      desc     Device driver handle.
 * @param [out]
 * @return
 *
 * @details
 *
 * @sa
 */
int spe_phy_an_adv_mst_slv_cfg(struct spe_phy_desc *desc, enum spe_phy_an_adv_master_slave_cfg ms_cfg)
{
    int    result = 0;
    uint16_t            val16;

    if (desc->irqPending)
    {
        return SPE_ETH_IRQ_PENDING;
    }

    /* Forced/Preferred */
    result = spe_phy_read(desc, ADDR_AN_ADV_ABILITY_L, &val16);
    if (result != 0)
    {
        return result;
    }
    val16 &= ~BITM_AN_ADV_ABILITY_L_AN_ADV_FORCE_MS;
    if ((ms_cfg == SPE_PHY_AN_ADV_FORCED_MASTER) || (ms_cfg == SPE_PHY_AN_ADV_FORCED_SLAVE))
    {
        val16 |= (1 << BITP_AN_ADV_ABILITY_L_AN_ADV_FORCE_MS);
    }
    result = spe_phy_write(desc, ADDR_AN_ADV_ABILITY_L, val16);
    if (result != 0)
    {
        return result;
    }

    /* Master/Slave */
    result = spe_phy_read(desc, ADDR_AN_ADV_ABILITY_M, &val16);
    if (result != 0)
    {
        return result;
    }
    val16 &= ~BITM_AN_ADV_ABILITY_M_AN_ADV_MST;
    if ((ms_cfg == SPE_PHY_AN_ADV_FORCED_MASTER) || (ms_cfg == SPE_PHY_AN_ADV_PREFFERED_MASTER))
    {
        val16 |= (1 << BITP_AN_ADV_ABILITY_M_AN_ADV_MST);
    }
    result = spe_phy_write(desc, ADDR_AN_ADV_ABILITY_M, val16);

    return result;
}

/**************************************************/
/***                                            ***/
/***                  Autoneg                   ***/
/***                                            ***/
/**************************************************/

/*
 * @brief           Enable/disable auto-negotiation.
 *
 * @param [in]      desc     Device driver handle.
 * @param [out]
 * @return
 *
 * @details         It is STRONGLY recommended to never disable auto-negotiation!
 *
 * @sa
 */
int spe_phy_an_enable(struct spe_phy_desc *desc, bool enable)
{
    int    result = 0;
    uint16_t            val16;

    if (desc->irqPending)
    {
        return SPE_ETH_IRQ_PENDING;
    }

    /* The only other bit in this register is AN_RESTART, need to write 0 to it */
    val16 = (enable? 1: 0) << BITP_AN_CONTROL_AN_EN;
    result = spe_phy_write(desc, ADDR_AN_CONTROL, val16);

    return result;
}

/*
 * @brief           Restart auto-negotiation.
 *
 * @param [in]      desc     Device driver handle.
 * @param [out]
 * @return
 *
 * @details
 *
 * @sa
 */
int spe_phy_renegotiate(struct spe_phy_desc *desc)
{
    int    result = 0;
    uint16_t            val16;

    if (desc->irqPending)
    {
        return SPE_ETH_IRQ_PENDING;
    }

    /* AN_EN should be 1 or AN_RESTART is ignored. Instead of read-modify-write */
    /* and/or reporting an error, we force AN_EN=1.                             */
    val16 = (1 << BITP_AN_CONTROL_AN_EN) |
            (1 << BITP_AN_CONTROL_AN_RESTART);
    result = spe_phy_write(desc, ADDR_AN_CONTROL, val16);

    return result;
}

/*
 * @brief           Get auto-negotiation status.
 *
 * @param [in]      desc     Device driver handle.
 * @param [out]
 * @return
 *
 * @details
 *
 * @sa
 */
int spe_phy_get_an_status(struct spe_phy_desc *desc, struct spe_phy_an_status *status)
{
    int    result = 0;
    uint16_t            val16;

    result = spe_phy_read(desc, ADDR_AN_STATUS, &val16);
    if (result != 0)
    {
        return result;
    }

    status->anComplete = (val16 & BITM_AN_STATUS_AN_COMPLETE) >> BITP_AN_STATUS_AN_COMPLETE;
    if (!status->anComplete)
    {
        return result;
    }

    /* Use the LL value for the link status. If it indicatesthe link is down,   */
    /* the application will read the link status on its own use GetLinkStatus() */
    status->anLinkStatus = (val16 & BITM_AN_STATUS_AN_LINK_STATUS)? SPE_PHY_LINK_STATUS_UP: SPE_PHY_LINK_STATUS_DOWN;

    /* Master/slave resolution */
    result = spe_phy_read(desc, ADDR_AN_STATUS_EXTRA, &val16);
    if (result != 0)
    {
        return result;
    }
    switch ((val16 & BITM_AN_STATUS_EXTRA_AN_MS_CONFIG_RSLTN) >> BITP_AN_STATUS_EXTRA_AN_MS_CONFIG_RSLTN)
    {
        case 0:
            status->anMsResolution = SPE_PHY_AN_MS_RESOLUTION_NOT_RUN;
            break;

        case 1:
            status->anMsResolution = SPE_PHY_AN_MS_RESOLUTION_FAULT;
            break;

        case 2:
            status->anMsResolution = SPE_PHY_AN_MS_RESOLUTION_SLAVE;
            break;

        case 3:
            status->anMsResolution = SPE_PHY_AN_MS_RESOLUTION_MASTER;
            break;
    }

    switch ((val16 & BITM_AN_STATUS_EXTRA_AN_TX_LVL_RSLTN) >> BITP_AN_STATUS_EXTRA_AN_TX_LVL_RSLTN)
    {
        case 0:
            status->anTxMode = SPE_PHY_AN_TX_LEVEL_RESOLUTION_NOT_RUN;
            break;

        case 1:
            status->anTxMode = SPE_PHY_AN_TX_LEVEL_RESERVED;
            break;

      case 2:
            status->anTxMode = SPE_PHY_AN_TX_LEVEL_1P0V;
            break;

        case 3:
            status->anTxMode = SPE_PHY_AN_TX_LEVEL_2P4V;
            break;
    }

    return result;
}



/**************************************************/
/***                                            ***/
/***                Miscellanous                ***/
/***                                            ***/
/**************************************************/

/*
 * @brief           Get PHY device capabilities.
 *
 * @param [in]      desc     Device driver handle.
 * @param [out]
 * @return
 *
 * @details
 *
 * @sa
 */
int spe_phy_get_capabilities(struct spe_phy_desc *desc, uint16_t *capabilities)
{
    int    result = 0;
    uint16_t            val16;

    if (desc->irqPending)
    {
        return SPE_ETH_IRQ_PENDING;
    }

    /* Read capabilities and store in driver struct */
    *capabilities = SPE_PHY_CAP_NONE;
    result = spe_phy_read(desc, ADDR_B10L_PMA_STAT, &val16);
    if (result == 0)
    {
        if (val16 & BITM_B10L_PMA_STAT_B10L_TX_LVL_HI_ABLE)
        {
            *capabilities |= SPE_PHY_CAP_TX_HIGH_LEVEL;
        }
        if (val16 & BITM_B10L_PMA_STAT_B10L_LB_PMA_LOC_ABLE)
        {
            *capabilities |= SPE_PHY_CAP_PMA_LOCAL_LOOPBACK;
        }
    }

    return result;
}

static int spe_phy_set_software_powerdown(struct spe_phy_desc *desc, bool enable)
{
    int    result = 0;
    uint16_t            val16;
    uint16_t            bitval;
    bool                swpd;
    int32_t             iter = SPE_PHY_SOFT_PD_ITER;

    if (desc->irqPending)
    {
        return SPE_ETH_IRQ_PENDING;
    }

    bitval = (enable)? 1: 0;
    val16 = bitval << BITP_CRSM_SFT_PD_CNTRL_CRSM_SFT_PD;
    result = spe_phy_write(desc, ADDR_CRSM_SFT_PD_CNTRL, val16);
    if (result != 0)
    {
        return result;
    }

    /* Wait with timeout for the PHY device to enter the desired state before returning. */
    do
    {
        result = spe_phy_get_software_powerdown(desc, &swpd);
    } while ((val16 != (uint16_t)swpd) && (--iter));

    if (iter <= 0)
    {
        return -ETIME;
    }

    if (enable)
    {
        desc->state = SPE_PHY_STATE_SOFTWARE_POWERDOWN;
    }
    else
    {
        desc->state = SPE_PHY_STATE_OPERATION;
    }

    return result;
}

/*
 * @brief           Enter software powerdown.
 *
 * @param [in]      desc     Device driver handle.
 * @param [out]
 * @return
 *
 * @details
 *
 * @sa
 */
int spe_phy_enter_software_powerdown(struct spe_phy_desc *desc)
{
    return spe_phy_set_software_powerdown(desc, true);
}

/*
 * @brief           Exit software powerdown.
 *
 * @param [in]      desc     Device driver handle.
 * @param [out]
 * @return
 *
 * @details
 *
 * @sa
 */
int spe_phy_exit_software_powerdown(struct spe_phy_desc *desc)
{
    return spe_phy_set_software_powerdown(desc, false);
}

/*
 * @brief           Get powerdown status.
 *
 * @param [in]      desc     Device driver handle.
 * @param [out]
 * @return
 *
 * @details
 *
 * @sa
 */
int spe_phy_get_software_powerdown(struct spe_phy_desc *desc, bool *enable)
{
    int    result = 0;
    uint16_t            val16;

    if (desc->irqPending)
    {
        return SPE_ETH_IRQ_PENDING;
    }

    val16 = 0;
    result = spe_phy_read(desc, ADDR_CRSM_STAT, &val16);
    if (result != 0)
    {
        return -ECOMM;
    }

    *enable = ((val16 & BITM_CRSM_STAT_CRSM_SFT_PD_RDY) == (1 << BITP_CRSM_STAT_CRSM_SFT_PD_RDY));

    return 0;
}

/*
 * @brief           Get link status.
 *
 * @param [in]      desc     Device driver handle.
 * @param [out]
 * @return
 *
 * @details
 *
 * @sa
 */
int spe_phy_get_link_status(struct spe_phy_desc *desc, enum spe_phy_link_status *status)
{
    int    result = 0;
    uint16_t            val16;

    if (desc->irqPending)
    {
        return SPE_ETH_IRQ_PENDING;
    }

    /* Default to link down, applicable when there are errors, */
    /* such as MDIO communication errors.                      */
    *status = SPE_PHY_LINK_STATUS_DOWN;

    /* Reading AN_STATUS register clears all latched bits, take this into account */
    result = spe_phy_read(desc, ADDR_AN_STATUS, &val16);
    if (result == 0)
    {
        if (val16 & BITM_AN_STATUS_AN_LINK_STATUS)
        {
            *status = SPE_PHY_LINK_STATUS_UP;
        }
        else
        {
            /* Read it again, first record the dropped link in the stats  */
            desc->stats.link_dropped++;
            result = spe_phy_read(desc, ADDR_AN_STATUS, &val16);
            if (result == 0)
            {
                *status = (val16 & BITM_AN_STATUS_AN_LINK_STATUS)? SPE_PHY_LINK_STATUS_UP: SPE_PHY_LINK_STATUS_DOWN;
            }
        }
    }
    return result;
}


/*
 * @brief           Static configuration of the PHY.
 *
 * @param [in]      desc     Device driver handle.
 * @return
 *
 * @details         This function is called during the initialization of the driver
 *                  or after a reset event. It configures a subset of the PHY registers
 *                  to change the default behaviour of the device, increase performance, etc.
 *
 * @sa
 */
static int phyStaticConfig(struct spe_phy_desc *desc, uint32_t modelNum, uint32_t revNum)
{
    int    result = 0;

    if (revNum == 0)
    {

        result = spe_phy_write(desc, 0x1E8C81, 0x0001);
        if (result != 0)
        {
            return result;
        }

        if (modelNum == 10)
        {
            result = spe_phy_write(desc, 0x1E8C80, 0x0001);
        }
        else
        {
            result = spe_phy_write(desc, 0x1E8C80, 0x3636);
        }
        if (result != 0)
        {
            return result;
        }

        result = spe_phy_write(desc, 0x1E881F, 0x0000);
        if (result != 0)
        {
            return result;
        }

        result = spe_phy_write(desc, 0x018154, 0x00F9);
        if (result != 0)
        {
            return result;
        }

        result = spe_phy_write(desc, 0x1E8C40, 0x000B);
        if (result != 0)
        {
            return result;
        }

        result = spe_phy_write(desc, 0x018008, 0x0003);
        if (result != 0)
        {
            return result;
        }
        result = spe_phy_write(desc, 0x018009, 0x0008);
        if (result != 0)
        {
            return result;
        }
        result = spe_phy_write(desc, 0x018167, 0x2000);
        if (result != 0)
        {
            return result;
        }
        result = spe_phy_write(desc, 0x018168, 0x0008);
        if (result != 0)
        {
            return result;
        }
        result = spe_phy_write(desc, 0x01816B, 0x0400);
        if (result != 0)
        {
            return result;
        }
        result = spe_phy_write(desc, 0x0181BD, 0x2000);
        if (result != 0)
        {
            return result;
        }
        result = spe_phy_write(desc, 0x0181BE, 0x0008);
        if (result != 0)
        {
            return result;
        }
        result = spe_phy_write(desc, 0x0181C2, 0x0400);
        if (result != 0)
        {
            return result;
        }
        result = spe_phy_write(desc, 0x0181DB, 0x0400);
        if (result != 0)
        {
            return result;
        }
        result = spe_phy_write(desc, 0x0181E1, 0x0400);
        if (result != 0)
        {
            return result;
        }
        result = spe_phy_write(desc, 0x0181E7, 0x0400);
        if (result != 0)
        {
            return result;
        }
        result = spe_phy_write(desc, 0x0181EB, 0x0400);
        if (result != 0)
        {
            return result;
        }
        result = spe_phy_write(desc, 0x018143, 0x0400);
        if (result != 0)
        {
            return result;
        }

        if (modelNum == 10)
        {
            result = spe_phy_write(desc, 0x1EA400, 0x0001);
            if (result != 0)
            {
                return result;
            }
            result = spe_phy_write(desc, 0x1EA407, 0x0001);
        }

    }

    return result;
}


/*
 * @brief           Reset the PHY device.
 *
 * @param [in]      desc     Device driver handle.
 * @param [out]
 * @return
 *
 * @details
 *
 * @sa
 */
int spe_phy_reset(struct spe_phy_desc *desc, enum spe_phy_reset_type reset_type)
{
    int    result = 0;
    uint16_t            val16;

    if (desc->irqPending)
    {
        return SPE_ETH_IRQ_PENDING;
    }

    switch (reset_type)
    {
        case SPE_PHY_RESET_TYPE_SW:

            val16 = (1 << BITP_CRSM_SFT_RST_CRSM_SFT_RST);
            result = spe_phy_write(desc, ADDR_CRSM_SFT_RST, val16);
            if (result != 0)
            {
                result = SPE_ETH_COMM_ERROR;
            }
            else
            {
                result = phyInit(desc);
            }
            break;

        default:
            result = -ENOSYS;
    }
    return result;
}

/*
 * @brief           Configures a PHY loopback mode.
 *
 * @param [in]      desc     Device driver handle.
 * @param [out]
 * @return
 *
 * @details
 *
 * @sa
 */
int spe_phy_set_loopback_mode(struct spe_phy_desc *desc, enum spe_phy_loopback_mode loopback_mode)
{
    int    result = 0;
    uint16_t            valMacIfLoopback;
    uint16_t            valB10lPcsCntrl;
    uint16_t            valB10lPmaCntrl;
    bool                prevModePmaPcs = false;

    result = spe_phy_read(desc, ADDR_MAC_IF_LOOPBACK, &valMacIfLoopback);
    if (result != 0)
    {
        return result;
    }
    /* Disable loopback */
    valMacIfLoopback &= ~((1 << BITP_MAC_IF_LOOPBACK_MAC_IF_LB_EN) | (1 << BITP_MAC_IF_LOOPBACK_MAC_IF_REM_LB_EN));

    result = spe_phy_read(desc, ADDR_B10L_PCS_CNTRL, &valB10lPcsCntrl);
    if (result != 0)
    {
        return result;
    }
    if (valB10lPcsCntrl & BITM_B10L_PCS_CNTRL_B10L_LB_PCS_EN)
    {
        prevModePmaPcs = true;
    }
    /* Disable loopback */
    valB10lPcsCntrl &= ~(1 << BITP_B10L_PCS_CNTRL_B10L_LB_PCS_EN);

    result = spe_phy_read(desc, ADDR_B10L_PMA_CNTRL, &valB10lPmaCntrl);
    if (result != 0)
    {
        return result;
    }
    if (valB10lPmaCntrl & BITM_B10L_PMA_CNTRL_B10L_LB_PMA_LOC_EN)
    {
        prevModePmaPcs = true;
    }
    /* Disable loopback */
    valB10lPmaCntrl &= ~(1 << BITP_B10L_PMA_CNTRL_B10L_LB_PMA_LOC_EN);

    if (prevModePmaPcs || (loopback_mode == SPE_PHY_LOOPBACK_PCS) || (loopback_mode == SPE_PHY_LOOPBACK_PMA))
    {
        result = spe_phy_enter_software_powerdown(desc);
        if (result != 0)
        {
            return result;
        }

        if ((loopback_mode == SPE_PHY_LOOPBACK_PCS) || (loopback_mode == SPE_PHY_LOOPBACK_PMA))
        {
            result = spe_phy_write(desc, ADDR_AN_CONTROL, 0);
            if (result != 0)
            {
                return result;
            }
            result = spe_phy_write(desc, ADDR_AN_FRC_MODE_EN, 1);
            if (result != 0)
            {
                return result;
            }
        }
        else
        {
            result = spe_phy_write(desc, ADDR_AN_FRC_MODE_EN, 0);
            if (result != 0)
            {
                return result;
            }
            result = spe_phy_write(desc, ADDR_AN_CONTROL, 1);
            if (result != 0)
            {
                return result;
            }
        }

    }

    switch (loopback_mode)
    {
        case SPE_PHY_LOOPBACK_NONE:
            break;
        case SPE_PHY_LOOPBACK_PCS:
            valB10lPcsCntrl |= (1 << BITP_B10L_PCS_CNTRL_B10L_LB_PCS_EN);
            break;
        case SPE_PHY_LOOPBACK_PMA:
            valB10lPmaCntrl |= (1 << BITP_B10L_PMA_CNTRL_B10L_LB_PMA_LOC_EN);
            break;
        case SPE_PHY_LOOPBACK_MACIF:
            valMacIfLoopback |= (1 << BITP_MAC_IF_LOOPBACK_MAC_IF_LB_EN);
            break;
        case SPE_PHY_LOOPBACK_MACIF_SUPPRESS_TX:
            valMacIfLoopback |= ((1 << BITP_MAC_IF_LOOPBACK_MAC_IF_LB_EN) | (1 << BITP_MAC_IF_LOOPBACK_MAC_IF_LB_TX_SUP_EN));
            break;
        case SPE_PHY_LOOPBACK_MACIF_REMOTE:
            valMacIfLoopback |= (1 << BITP_MAC_IF_LOOPBACK_MAC_IF_REM_LB_EN);
            break;
        case SPE_PHY_LOOPBACK_MACIF_REMOTE_SUPPRESS_RX:
            valMacIfLoopback |= ((1 << BITP_MAC_IF_LOOPBACK_MAC_IF_REM_LB_EN) | (1 << BITP_MAC_IF_LOOPBACK_MAC_IF_REM_LB_RX_SUP_EN));
            break;
        default:
            return -EINVAL;
    }

    result = spe_phy_write(desc, ADDR_MAC_IF_LOOPBACK, valMacIfLoopback);
    if (result != 0)
    {
        return result;
    }

    result = spe_phy_write(desc, ADDR_B10L_PCS_CNTRL, valB10lPcsCntrl);
    if (result != 0)
    {
        return result;
    }

    result = spe_phy_write(desc, ADDR_B10L_PMA_CNTRL, valB10lPmaCntrl);
    if (result != 0)
    {
        return result;
    }

    if (prevModePmaPcs || (loopback_mode == SPE_PHY_LOOPBACK_PCS) || (loopback_mode == SPE_PHY_LOOPBACK_PMA))
    {
        result = spe_phy_exit_software_powerdown(desc);
        if (result != 0)
        {
            return result;
        }
    }
    return result;
}

/*
 * @brief           Configures a PHY test mode.
 *
 * @param [in]      desc     Device driver handle.
 * @param [out]
 * @return
 *
 * @details
 *
 * @sa
 */
int spe_phy_set_test_mode(struct spe_phy_desc *desc, enum spe_phy_test_mode test_mode)
{
    int    result = 0;
    uint16_t            b10lPmaCntrl;
    uint16_t            testModeCntrl;

    if (desc == NULL)
    {
        return -EINVAL;
    }

    if ((test_mode != SPE_PHY_TEST_MODE_NONE) && (test_mode != SPE_PHY_TEST_MODE_1) &&
        (test_mode != SPE_PHY_TEST_MODE_2) && (test_mode != SPE_PHY_TEST_MODE_3) &&
        (test_mode != SPE_PHY_TEST_MODE_TX_DISABLE))
    {
        return -EINVAL;
    }

    result = spe_phy_enter_software_powerdown(desc);
    if (result != 0)
    {
        return result;
    }

    result = spe_phy_read(desc, ADDR_B10L_PMA_CNTRL, &b10lPmaCntrl);
    if (result != 0)
    {
        return result;
    }

    /* For most test modes we want to disable the transmit disable mode */
    b10lPmaCntrl &= ~BITM_B10L_PMA_CNTRL_B10L_TX_DIS_MODE_EN;

    if (test_mode == SPE_PHY_TEST_MODE_NONE)
    {
        /* No need for read-modify-write, register has only one bitfield */
        result = spe_phy_write(desc, ADDR_B10L_TEST_MODE_CNTRL, ENUM_B10L_TEST_MODE_CNTRL_B10L_TX_TEST_MODE_IEEE_TX_TM_NONE);
        if (result != 0)
        {
            return result;
        }

        result = spe_phy_write(desc, ADDR_AN_FRC_MODE_EN, 0);
        if (result != 0)
        {
            return result;
        }

        result = spe_phy_write(desc, ADDR_AN_CONTROL, 1);
        if (result != 0)
        {
            return result;
        }

    }
    else
    {
        result = spe_phy_write(desc, ADDR_AN_CONTROL, 0);
        if (result != 0)
        {
            return result;
        }

        result = spe_phy_write(desc, ADDR_AN_FRC_MODE_EN, 1);
        if (result != 0)
        {
            return result;
        }

        if (test_mode == SPE_PHY_TEST_MODE_TX_DISABLE)
        {
            b10lPmaCntrl |= BITM_B10L_PMA_CNTRL_B10L_TX_DIS_MODE_EN;
        }
        else
        {
            switch (test_mode)
            {
                case SPE_PHY_TEST_MODE_1:
                    testModeCntrl = (ENUM_B10L_TEST_MODE_CNTRL_B10L_TX_TEST_MODE_IEEE_TX_TM_JITTER << BITP_B10L_TEST_MODE_CNTRL_B10L_TX_TEST_MODE);
                    break;

                case SPE_PHY_TEST_MODE_2:
                    testModeCntrl = (ENUM_B10L_TEST_MODE_CNTRL_B10L_TX_TEST_MODE_IEEE_TX_TM_DROOP << BITP_B10L_TEST_MODE_CNTRL_B10L_TX_TEST_MODE);
                    break;

                case SPE_PHY_TEST_MODE_3:
                    testModeCntrl = (ENUM_B10L_TEST_MODE_CNTRL_B10L_TX_TEST_MODE_IEEE_TX_TM_IDLE << BITP_B10L_TEST_MODE_CNTRL_B10L_TX_TEST_MODE);
                    break;

            }

            /* No need for read-modify-write, register has only one bitfield */
            result = spe_phy_write(desc, ADDR_B10L_TEST_MODE_CNTRL, testModeCntrl);
            if (result != 0)
            {
                return result;
            }
        }
    }

    result = spe_phy_write(desc, ADDR_B10L_PMA_CNTRL, b10lPmaCntrl);
    if (result != 0)
    {
        return result;
    }

    result = spe_phy_exit_software_powerdown(desc);

    return result;
}

/*
 * @brief           Enable or disable the LED.
 *
 * @param [in]      desc     Device driver handle.
 * @param [in]      led_sel      Selection of the LED port.
 * @param [in]      enable      Enable/disable the LED.
 * @return
 *
 * @details
 *
 * @sa
 */
int spe_phy_led_enable(struct spe_phy_desc *desc, enum spe_phy_led_port led_sel, bool enable)
{
    int    result = 0;
    uint16_t            val16;
    uint16_t            bitval;

    if (desc == NULL)
    {
        return -EINVAL;
    }

    bitval = (enable)? 1: 0;

    result = spe_phy_read(desc, ADDR_LED_CNTRL, &val16);

    if(led_sel == SPE_PHY_LED_0)
    {
      val16 &= ~BITM_LED_CNTRL_LED0_EN;
      val16 |= bitval << BITP_LED_CNTRL_LED0_EN;
    }
    if(led_sel == SPE_PHY_LED_1)
    {
      val16 &= ~BITM_LED_CNTRL_LED1_EN;
      val16 |= bitval << BITP_LED_CNTRL_LED1_EN;
    }
    result = spe_phy_write(desc, ADDR_LED_CNTRL, val16);

    return result;
}

/*
 * @brief
 *
 * @param [in]      desc     Device driver handle.
 * @param [out]
 * @param [in]      led_sel      Selection of the LED port.
 * @param [in]      blink_on     LEN blink ON time.
 * @param [in]      blink_off    LEN blink OFF time.
 * @return
 *
 * @details
 *
 * @sa
 */
int spe_phy_led_blink_time(struct spe_phy_desc *desc, enum spe_phy_led_port led_sel, uint32_t blink_on, uint32_t blink_off)
{
    int    result = 0;

    uint16_t            val16;

    if ((blink_on > 255) || (blink_off > 255))
    {
        return -EINVAL;
    }

    if(led_sel == SPE_PHY_LED_0)
    {
      val16 = ((blink_on << BITP_LED0_BLINK_TIME_CNTRL_LED0_ON_N4MS) | (blink_off << BITP_LED0_BLINK_TIME_CNTRL_LED0_OFF_N4MS));
      /* Full register write, no need for read-modify-write. */
      result = spe_phy_write(desc, ADDR_LED0_BLINK_TIME_CNTRL, val16);
    }

    if(led_sel == SPE_PHY_LED_1)
    {
      val16 = ((blink_on << BITP_LED1_BLINK_TIME_CNTRL_LED1_ON_N4MS) | (blink_off << BITP_LED1_BLINK_TIME_CNTRL_LED1_OFF_N4MS));
      /* Full register write, no need for read-modify-write. */
      result = spe_phy_write(desc, ADDR_LED1_BLINK_TIME_CNTRL, val16);
    }

    return result;
}

/*
* @brief MDIO Write, according to clause 45
 *
 * @param [in] dev - Pointer to the HW driver.
 * @param [in] reg_addr - PHY MDIO Register address to write.
 * @param [out] data -  Data to write.
 *
 * @return 0 in case of success, positive error code otherwise.
*/
int spe_phy_write(struct spe_phy_desc *desc, uint32_t reg_addr, uint16_t data)
{
    int    result = 0;
    uint32_t            backup;

    if (desc == NULL)
    {
        return -EINVAL;
    }
    
    backup = capi_irq_get_enable(desc->eth_irq);
    capi_irq_disable(param->eth_irq);

    if (desc->writeFn(desc->phyAddr, reg_addr, data) != SPE_HAL_SUCCESS)
    {
        result = -ECOMM          ;
    }

    if (backup)
    {
	capi_irq_enable(param->eth_irq);
    }

    return result;
}

/*
* @brief MDIO Read, according to clause 45
 *
 * @param [in] instance - Pointer to the HW driver.
 * @param [in] hwAddr - PHY MDIO Hardware address.
 * @param [in] reg_addr - PHY MDIO Register address to read.
 * @param [out] data - Pointer to the data buffer.
 *
 * @return 0 in case of success, positive error code otherwise.
*/
int spe_phy_read(struct spe_phy_desc *desc, uint32_t reg_addr, uint16_t *data)
{
    int    result = 0;
    uint32_t            backup;

    if (desc == NULL)
    {
        return -EINVAL;
    }

    backup = capi_irq_get_enable(desc->eth_irq);
    capi_irq_disable(param->eth_irq);

    /* The only error returned by the HAL function is caused by 2nd TA bit */
    /* not being pulled low, which indicates the MDIO interface on the PHY */
    /* device is not operational.                                          */
    result = (int)desc->readFn(desc->phyAddr, reg_addr, data);
    if (result != SPE_HAL_SUCCESS)
    {
        result = -ECOMM;
    }

    if (backup)
    {
	capi_irq_enable(param->eth_irq);
    }

    return result;
}

int spe_phy_get_mse_link_quality(struct spe_phy_desc *desc, struct spe_phy_mse_link_quality *mse_link_quality)
{
    int    result = 0;

    result = spe_phy_read(desc, ADDR_MSE_VAL, &mse_link_quality->mse_val);
    if (result != 0)
    {
        return result;
    }

    if (mse_link_quality->mse_val > SPE_PHY_LINK_QUALITY_THR_POOR)
    {
        mse_link_quality->link_quality = SPE_PHY_LINK_QUALITY_POOR;
    }
    else
    {
        if (mse_link_quality->mse_val > SPE_PHY_LINK_QUALITY_THR_MARGINAL)
        {
            mse_link_quality->link_quality = SPE_PHY_LINK_QUALITY_MARGINAL;
        }
        else
        {
            mse_link_quality->link_quality = SPE_PHY_LINK_QUALITY_GOOD;
        }
    }

    for (mse_link_quality->sqi = 0; mse_link_quality->sqi < SPE_PHY_SQI_NUM_ENTRIES - 1; mse_link_quality->sqi++)
    {
        if (mse_link_quality->mse_val > convMseToSqi[mse_link_quality->sqi])
        {
            break;
        }
    }

    return result;
}

int spe_phy_frame_gen_enable(struct spe_phy_desc *desc, bool enable)
{
    int    result = 0;
    uint16_t            frame_control;

    if (enable)
    {
        /* Set frame counter to 0 */
        result = spe_phy_frame_gen_set_frame_count(desc, 0);
        if (result != 0)
        {
            return result;
        }

        /* Enable frame generator, single bitfield in the register */
        result = spe_phy_write(desc, ADDR_FG_EN, (1 << BITP_FG_EN_FG_EN));
        if (result != 0)
        {
            return result;
        }
    }
    else
    {
        /* Backup of the current value in frame control */
        result = spe_phy_read(desc, ADDR_FG_CNTRL_RSTRT, &frame_control);
        if (result != 0)
        {
            return result;
        }

        /* Disable frame generation by setting frame control to NONE */
        result = spe_phy_frame_gen_set_frame_payload(desc, SPE_PHY_FRAME_GEN_PAYLOAD_NONE);
        if (result != 0)
        {
           return result;
        }

        /* Note: we may want to wait a frame length (plus interpacket gap) before checking FG_DONE */
        /* FG_DONE can only be checked if the frame generator is not already stopped */
#if 0
        bool                fg_done;
        do
        {
            result = spe_phy_frame_gen_done(desc, &fg_done);
        } while ((result != 0) || !fg_done);
#endif

        /* Disable frame generator and restore the frame control setting */
        result = spe_phy_write(desc, ADDR_FG_EN, (0 << BITP_FG_EN_FG_EN));
        if (result != 0)
        {
            return result;
        }

        result = spe_phy_write(desc, ADDR_FG_CNTRL_RSTRT, frame_control);
    }
    return result;
}

int spe_phy_frame_gen_set_mode(struct spe_phy_desc *desc, enum spe_phy_frame_gen_mode mode)
{
    int    result = 0;

    result = spe_phy_write(desc, ADDR_FG_CONT_MODE_EN, (mode << BITP_FG_CONT_MODE_EN_FG_CONT_MODE_EN));
    return result;
}

int spe_phy_frame_gen_set_frame_count(struct spe_phy_desc *desc, uint32_t frame_count)
{
    int    result = 0;
    uint16_t            val16;

    if (desc == NULL)
    {
        return SPE_ETH_INVALID_HANDLE;
    }

    val16 = (frame_count >> 16) & 0xFFFF;
    result = spe_phy_write(desc, ADDR_FG_NFRM_H, val16);
    
    if (result != 0)
    {
        return result;
    }

    val16 = frame_count & 0xFFFF;
    result = spe_phy_write(desc, ADDR_FG_NFRM_L, val16);

    return result;
}

int spe_phy_frame_gen_set_frame_payload(struct spe_phy_desc *desc, enum spe_phy_frame_gen_payload payload)
{
    int    result = 0;
    uint16_t            val16 = 0x0000; // This includes setting FG_RSTRT=0

    val16 |= (payload << BITP_FG_CNTRL_RSTRT_FG_CNTRL);
    result = spe_phy_write(desc, ADDR_FG_CNTRL_RSTRT, val16);
    return result;
}

int spe_phy_frame_gen_set_frame_len(struct spe_phy_desc *desc, uint16_t frame_len)
{
    int    result = 0;

    result = spe_phy_write(desc, ADDR_FG_FRM_LEN, (frame_len << BITP_FG_FRM_LEN_FG_FRM_LEN));
    return result;
}

int spe_phy_frame_gen_set_ifg_len(struct spe_phy_desc *desc, uint16_t ifg_len)
{
    int    result = 0;

    /* Single 16-bit bitfield */
    result = spe_phy_write(desc, ADDR_FG_IFG_LEN, ifg_len);
    return result;
}

int spe_phy_frame_gen_restart(struct spe_phy_desc *desc)
{
    int    result = 0;
    uint16_t            frame_control;
    bool                fg_done;

    /* Before restart, clear FG_DONE explicitly in case it was set before and not cleared */
    /* Discard the read value, only use of the read is to clear FG_DONE */
    result = spe_phy_frame_gen_done(desc, &fg_done);
    if (result != 0)
    {
        return result;
    }
    (void)fg_done;

    result = spe_phy_read(desc, ADDR_FG_CNTRL_RSTRT, &frame_control);
    if (result != 0)
    {
        return result;
    }

    frame_control |= (1 << BITP_FG_CNTRL_RSTRT_FG_RSTRT);
    result = spe_phy_write(desc, ADDR_FG_CNTRL_RSTRT, frame_control);

    return result;
}

int spe_phy_frame_gen_done(struct spe_phy_desc *desc, bool *done)
{
    int    result = 0;
    uint16_t            val16;

    result = spe_phy_read(desc, ADDR_FG_DONE, &val16);
    if (result != 0)
    {
        return result;
    }

    *done = (BITM_FG_DONE_FG_DONE & val16);

    return result;
}

int spe_phy_frame_check_enable(struct spe_phy_desc *desc, bool enable)
{
    int    result = 0;
    uint16_t            val16;

    if (enable)
    {
        val16 = 1 << BITP_FC_EN_FC_EN;
    }
    else
    {
        val16 = 0 << BITP_FC_EN_FC_EN;
    }

    result = spe_phy_write(desc, ADDR_FC_EN, val16);

    return result;
}

int spe_phy_frame_check_source_select(struct spe_phy_desc *desc, enum spe_phy_frame_check_source source)
{
    int    result = 0;

    result = spe_phy_write(desc, ADDR_FC_TX_SEL, (source << BITP_FC_TX_SEL_FC_TX_SEL));

    return result;
}

int spe_phy_frame_check_read_frame_count(struct spe_phy_desc *desc, uint32_t *count)
{
    int    result = 0;
    uint16_t            val16;

    result = spe_phy_read(desc, ADDR_FC_FRM_CNT_H, &val16);
    if (result != 0)
    {
        return result;
    }
    *count = (val16 << 16);

    result = spe_phy_read(desc, ADDR_FC_FRM_CNT_L, &val16);
    if (result != 0)
    {
        return result;
    }
    *count |= val16;

    return result;
}

int spe_phy_frame_check_read_rx_err_count(struct spe_phy_desc *desc, uint16_t *count)
{
    int    result = 0;

    result = spe_phy_read(desc, ADDR_RX_ERR_CNT, count);

    return result;
}

int spe_phy_frame_check_read_error_count(struct spe_phy_desc *desc, struct spe_phy_frame_check_error_counters *count)
{
    int    result = 0;

    result = spe_phy_read(desc, ADDR_FC_LEN_ERR_CNT, &count->LEN_ERR_CNT);
    if (result != 0)
    {
        return result;
    }

    result = spe_phy_read(desc, ADDR_FC_ALGN_ERR_CNT, &count->ALGN_ERR_CNT);
    if (result != 0)
    {
        return result;
    }

    result = spe_phy_read(desc, ADDR_FC_SYMB_ERR_CNT, &count->SYMB_ERR_CNT);
    if (result != 0)
    {
        return result;
    }

    result = spe_phy_read(desc, ADDR_FC_OSZ_CNT, &count->OSZ_CNT);
    if (result != 0)
    {
        return result;
    }

    result = spe_phy_read(desc, ADDR_FC_USZ_CNT, &count->USZ_CNT);
    if (result != 0)
    {
        return result;
    }

    result = spe_phy_read(desc, ADDR_FC_ODD_CNT, &count->ODD_CNT);
    if (result != 0)
    {
        return result;
    }

    result = spe_phy_read(desc, ADDR_FC_ODD_PRE_CNT, &count->ODD_PRE_CNT);
    if (result != 0)
    {
        return result;
    }

    result = spe_phy_read(desc, ADDR_FC_FALSE_CARRIER_CNT, &count->FALSE_CARRIER_CNT);

    return result;
}

/**************************************************/
/***                                            ***/
/***              Static Functions              ***/
/***                                            ***/
/**************************************************/

/*
 * @brief           Check thedevice identity.
 *
 * @param [in]      desc     Device driver handle.
 * @param [out]
 * @return
 *
 * @details         This function is called after a reset event and before the
 *                  initialization of the device is performed.
 *                  It reads the MMD1_DEV_ID1/MMD1_DEV_ID2 registers and compares
 *                  them with expected values. If comparison is not successful,
 *                  return SPE_PHY_UNSUPPORTED_DEVID
 *
 * @sa
 */
static int checkIdentity(struct spe_phy_desc *desc, uint32_t *modelNum, uint32_t *revNum)
{
    int    result = 0;
    uint16_t            val16;

#if defined(MDIO_CL22)
    result = spe_phy_read(desc, ADDR_MI_PHY_ID1, &val16);
#else
      result = spe_phy_read(desc, ADDR_MMD1_DEV_ID1, &val16);
#endif
    if (result != 0)
    {
        return result;
    }
    
    if (val16 != SPE_PHY_DEVID1)
    {
        return SPE_ETH_UNSUPPORTED_DEVICE;
    }

#if defined(MDIO_CL22)
      result = spe_phy_read(desc, ADDR_MI_PHY_ID2, &val16);
#else
      result = spe_phy_read(desc, ADDR_MMD1_DEV_ID2, &val16);
#endif
    if (result != 0)
    {
        return result;
    }

    /* Check if the value of MMD1_DEV_ID2.OUI matches expected value */
    if ((val16 & BITM_MMD1_DEV_ID2_MMD1_DEV_ID2_OUI) != (SPE_PHY_DEVID2_OUI << BITP_MMD1_DEV_ID2_MMD1_DEV_ID2_OUI))
    {
        return SPE_ETH_UNSUPPORTED_DEVICE;
    }

    *modelNum = (uint32_t)((val16 & BITM_MMD1_DEV_ID2_MMD1_MODEL_NUM) >> BITP_MMD1_DEV_ID2_MMD1_MODEL_NUM);
    *revNum = (uint32_t)((val16 & BITM_MMD1_DEV_ID2_MMD1_REV_NUM) >> BITP_MMD1_DEV_ID2_MMD1_REV_NUM);

    return result;
}

#if defined(ADIN1100)
static int waitMdio(struct spe_phy_desc *desc)
{
    int    result = 0;
    int32_t             iter = SPE_PHY_MDIO_POWERUP_ITER;
    uint16_t            val16;

    /* Wait until MDIO interface is up */
    /* The MDIO reads will return all 1s as data values before the interface is up, poll DEVID1 register. */
    do
    {
#if defined(MDIO_CL22)
        result = spe_phy_read(desc, ADDR_MI_PHY_ID1, &val16);
#else
        result = spe_phy_read(desc, ADDR_MMD1_DEV_ID1, &val16);
#endif

    } while ((result == SPE_ETH_COMM_ERROR) && (--iter));

    return result;
}
#endif



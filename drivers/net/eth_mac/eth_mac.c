/***************************************************************************//**
*   @file   eth_mac.c
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

#include "eth_mac.h"

#define PHY_SUBSYS_SHIFT_BITS       (16)
#define MDIO_PHY_PORT1_ADDR         (1)
#define MDIO_PHY_PORT2_ADDR         (2)

/*          Bit shift positions for 32-bit register byte             */
#define BYTE3_SHIFT_BITS    (24)    /* Most significant byte [31:24] */
#define BYTE2_SHIFT_BITS    (16)    /* Upper middle byte [23:16]     */
#define BYTE1_SHIFT_BITS    (8)     /* Lower middle byte [15:8]      */

#define DWORD               (4)

static int     	eth_mac_wait_mdio_ready           (struct eth_mac_desc *desc, uint16_t addr_offset);
static int     	eth_mac_config_register           (struct eth_mac_desc *desc);
static int     	eth_mac_receive_frame             (struct eth_mac_desc *desc, uint32_t port);
static int     	eth_mac_wait_device_ready         (struct eth_mac_desc *desc);
static void    	eth_mac_calculate_fcs             (struct eth_mac_desc *desc, struct eth_mac_buf_desc *buf_desc);
static int     	eth_mac_process_tx_queue          (struct eth_mac_desc *desc, struct eth_mac_status1 status1);
static int     	eth_mac_send_frame                (struct eth_mac_desc *desc, struct eth_mac_frame *frame, struct eth_mac_status1 status1);

/****************************************/
/****************************************/
/***                                  ***/
/***            IRQ Callback          ***/
/***                                  ***/
/**
 * @brief ADIN1110-specific interrupt callback handler
 */
static void mac_callback_adin1110(struct eth_mac_desc *desc,
                                struct eth_mac_status0 status0,
                                struct eth_mac_status1 status1,
                                struct eth_mac_status0 status0_mask,
                                struct eth_mac_status1 status1_mask,
                                uint32_t *irq_mask1)
{
    struct eth_mac_status1        status1_clear;
    struct eth_mac_timestamp_rdy  timestamp_ready;
    uint16_t                	  val16;
    uint32_t                	  rx_count[2] = {0, 0};
    
    (void)status0;
    (void)status1;

    /* If PHYINT is asserted, read PHY status registers (cleared on read) */
    if (status0_mask.PHYINT)
    {
        result = eth_mac_phy_read(desc, 1, ADDR_CRSM_IRQ_STATUS, &val16);
        if (result != 0)
        {
            goto end;
        }

        /* Record state of CRSM_IRQ_STATUS */
        desc->status_registers.p1_status &= 0xFFFF0000;
        desc->status_registers.p1_status |= (val16 & 0x0000FFFF);
        desc->status_registers.p1_status_mask &= 0xFFFF0000;
        desc->status_registers.p1_status_mask |=
            (desc->status_registers.p1_status & desc->phy_irq_mask & 0x0000FFFF);

        result = eth_mac_phy_read(desc, 1, ADDR_PHY_SUBSYS_IRQ_STATUS, &val16);
        if (result != 0)
        {
            goto end;
        }

        /* Record state of PHY_SUBSYS_IRQ_STATUS */
        desc->status_registers.p1_status &= 0x0000FFFF;
        desc->status_registers.p1_status |= ((val16 << 16) & 0xFFFF0000);
        desc->status_registers.p1_status_mask &= 0x0000FFFF;
        desc->status_registers.p1_status_mask |= 
            ((desc->status_registers.p1_status & desc->phy_irq_mask & 0xFFFF0000) << 16);
    }

#if defined(ADIN1110)
    /* Link status and link status change behave differently in ADIN2111 */
    if (status1_mask.LINK_CHANGE)
    {
        if (desc->cb_func[ETH_MAC_EVT_LINK_CHANGE] != NULL)
        {
            enum eth_mac_link_status link_status = 
                (enum eth_mac_link_status)status1.P1_LINK_STATUS;
            desc->cb_func[ETH_MAC_EVT_LINK_CHANGE]
                (desc->cb_param[ETH_MAC_EVT_LINK_CHANGE], 
                 ETH_MAC_EVT_LINK_CHANGE, (void *)&link_status);
        }
    }
#endif
    /* Timestamp availability */
    if (status0_mask.TTSCAA | status0_mask.TTSCAB | status0_mask.TTSCAC)
    {
        if (desc->cb_func[ETH_MAC_EVT_TIMESTAMP_RDY] != NULL)
        {
            timestamp_ready.timestamp_readyA = (bool)status0_mask.TTSCAA;
            timestamp_ready.timestamp_readyB = (bool)status0_mask.TTSCAB;
            timestamp_ready.timestamp_readyC = (bool)status0_mask.TTSCAC;
            desc->cb_func[ETH_MAC_EVT_TIMESTAMP_RDY]
                (desc->cb_param[ETH_MAC_EVT_TIMESTAMP_RDY], 
                 ETH_MAC_EVT_TIMESTAMP_RDY, (void *)&timestamp_ready);
        }
    }

    /* General status callback will fire even if callbacks were executed */ 
    /* for specific status bits                                          */
    if (status0_mask.VALUE32 || status1_mask.VALUE32)
    {
        if (desc->cb_func[ETH_MAC_EVT_STATUS] != NULL)
        {
            desc->cb_func[ETH_MAC_EVT_STATUS]
                (desc->cb_param[ETH_MAC_EVT_STATUS], ETH_MAC_EVT_STATUS, 
                 (void *)&desc->status_registers);
        }
    }

    /* Handle RX/TX ready */
    status1_clear.VALUE32 = status1.VALUE32;
    if (!desc->pending_ctrl)
    {
        /* If RX_RDY is asserted, do not clear TX_RDY, as we */
        /* prioritize reading frames                         */
        if (status1.P1_RX_RDY)
        {
#if ADI_PAUSE_RX_IF_NO_BUFFERS
            if (queue_isEmpty(desc->rx_queue))
            {
                /* P*_RX_RDY interrupt sources stay asserted for as long as  */ 
                /* there is something  to read in RxFIFO, but if there are no*/
                /* buffers to read to, the IRQ will keep  being asserted.    */
                /* To avoid a lockup, the P*_RX_RDY interrupts sources are   */ 
                /* masked until an Rx buffer is submitted to the driver.     */
                *irq_mask1 |= (1 << BITP_MAC_IMASK1_P1_RX_RDY_MASK);
            }
            else
#endif
            {
                /* Received buffer will be read from the FIFO, */
                /* do not clear TX_RDY yet.                    */
                status1_clear.TX_RDY = 0;
            }
        }
    }
    else
    {
        status1_clear.TX_RDY = 0;
    }

    result = eth_mac_write_register(desc, ADDR_MAC_STATUS1, status1_clear.VALUE32);
    if (result != 0)
    {
        goto end;
    }

#if ADI_PAUSE_RX_IF_NO_BUFFERS || ADI_EDGE_SENSITIVE_IRQ
#if ADI_EDGE_SENSITIVE_IRQ
    /* This will update the IRQ status mask and reassert the pending IRQ, if needed when */
    /* the IRQ is edge-triggered. It has no effect when the IRQ is level sensitive. */
    result = eth_mac_write_register(desc, ADDR_MAC_IMASK1, 0xFFFFFFFF);
    if (result != 0)
    {
        goto end;
    }
#endif

    if (ADI_EDGE_SENSITIVE_IRQ || (desc->irq_mask1 != *irq_mask1))
    {
        result = eth_mac_write_register(desc, ADDR_MAC_IMASK1, *irq_mask1);
        if (result != 0)
        {
            goto end;
        }
    }
#endif

    if (desc->pending_ctrl)
    {
	capi_irq_disable(desc->eth_irq);
        goto end;
    }

    /* Handle frame reception */
    if (status1.P1_RX_RDY)
    {
	capi_irq_disable(desc->eth_irq);

       do 
       {
           if (queue_isEmpty(desc->rx_queue))
               break;

            if (status1.P1_RX_RDY_HI && rx_count[0] < 2) { 
                eth_mac_receive_frame(desc, 0); 
                rx_count[0]++;
                status1.TX_RDY = 0;
            } else if (status1.P1_RX_RDY && rx_count[1] < 2) { 
                eth_mac_receive_frame(desc, 0);
                status1.TX_RDY = 0;                
                rx_count[1]++; 
            } else {
                break;
            }

            desc->mac_wait_until_ready(desc->device);

            result = eth_mac_read_register(desc, ADDR_MAC_STATUS1,
                                      &status1.VALUE32);
            if (result != 0)
                goto end;

        } while (status1.P1_RX_RDY || status1.P1_RX_RDY_HI);
    }
    else
    {
        if (status1.TX_RDY)
        {
            if (desc->state == ETH_MAC_STATE_READY)
            {
                result = eth_mac_process_tx_queue(desc,status1);
                if (result != 0)
                {
                    goto end;
                }
            }
            else
            {
                // ADI_HAL_SET_PENDING_IRQ(desc->device);
		stm32_capi_irq_set_pending(ETH_INT_N_IRQn);
            }
        }
    }

    if (!queue_isEmpty(&desc->tx_queue))
    {
        result = eth_mac_process_tx_queue(desc, status1);
        if (result != 0)
        {
            goto end;
        }
    }

end:
    return;
}

/**
 * @brief ADIN2111-specific interrupt callback handler
 */

static void mac_callback_adin2111(struct eth_mac_desc *desc,
                                struct eth_mac_status0 status0,
                                struct eth_mac_status1 status1,
                                struct eth_mac_status0 status0_mask,
                                struct eth_mac_status1 status1_mask,
                                uint32_t *irq_mask1)
{
#if defined(ADIN2111)
    int        			 result = 0;
    uint16_t                	 val16;
    struct eth_mac_status1       status1_clear;
    struct eth_mac_timestamp_rdy timestamp_ready;
    /* Index: 0=P1 normal, 1=P1 high, 2=P2 normal, 3=P2 high */
    uint32_t 			 rx_count[4] = {0, 0, 0, 0};
    
    (void)status0;
    (void)status1;

    /* If PHYINT is asserted for Port 1, read PHY status registers(cleared on read) */
    if (status0_mask.PHYINT)
    {
        result = eth_mac_phy_read(desc, 1, ADDR_CRSM_IRQ_STATUS, &val16);
        if (result != 0)
        {
            goto end;
        }

        /* Record state of CRSM_IRQ_STATUS */
        desc->status_registers.p1_status &= 0xFFFF0000;
        desc->status_registers.p1_status |= (val16 & 0x0000FFFF);
        desc->status_registers.p1_status_mask &= 0xFFFF0000;
        desc->status_registers.p1_status_mask |= 
            (desc->status_registers.p1_status & desc->phy_irq_mask 
             & 0x0000FFFF);

        result = eth_mac_phy_read(desc, 1, ADDR_PHY_SUBSYS_IRQ_STATUS, &val16);
        if (result != 0)
        {
            goto end;
        }

        /* Record state of PHY_SUBSYS_IRQ_STATUS */
        desc->status_registers.p1_status &= 0x0000FFFF;
        desc->status_registers.p1_status |= ((val16 << 16) & 0xFFFF0000);
        desc->status_registers.p1_status_mask &= 0x0000FFFF;
        desc->status_registers.p1_status_mask |= 
            ((desc->status_registers.p1_status & desc->phy_irq_mask 
              & 0xFFFF0000) << 16);
    }

    /* If PHYINT is asserted for Port 2, read PHY status registers */
    if (status1_mask.P2_PHYINT)
    {
        result = eth_mac_phy_read(desc, 2, ADDR_CRSM_IRQ_STATUS, &val16);
        if (result != 0)
        {
            goto end;
        }

        /* Record state of CRSM_IRQ_STATUS */
        desc->status_registers.p2_status &= 0xFFFF0000;
        desc->status_registers.p2_status |= (val16 & 0x0000FFFF);
        desc->status_registers.p2_status_mask &= 0xFFFF0000;
        desc->status_registers.p2_status_mask |= 
            (desc->status_registers.p2_status & desc->phy_irq_mask 
             & 0x0000FFFF);

        result = eth_mac_phy_read(desc, 2, ADDR_PHY_SUBSYS_IRQ_STATUS, &val16);
        if (result != 0)
        {
            goto end;
        }

        /* Record state of PHY_SUBSYS_IRQ_STATUS */
        desc->status_registers.p2_status &= 0x0000FFFF;
        desc->status_registers.p2_status |= ((val16 << 16) & 0xFFFF0000);
        desc->status_registers.p2_status_mask &= 0x0000FFFF;
        desc->status_registers.p2_status_mask |= 
            ((desc->status_registers.p2_status & desc->phy_irq_mask 
              & 0xFFFF0000) << 16);
    }

    /* Timestamp availability flags are different in ADIN2111 */

    if ((status0_mask.TTSCAA | status0_mask.TTSCAB | status0_mask.TTSCAC) | 
        (status1_mask.P2_TTSCAA | status1_mask.P2_TTSCAB | 
         status1_mask.P2_TTSCAC))
    {
        if (desc->cb_func[eth_mac_EVT_TIMESTAMP_RDY] != NULL)
        {
            timestamp_ready.p1TimestampReadyA = (bool)status0_mask.TTSCAA;
            timestamp_ready.p1TimestampReadyB = (bool)status0_mask.TTSCAB;
            timestamp_ready.p1TimestampReadyC = (bool)status0_mask.TTSCAC;
            timestamp_ready.p2TimestampReadyA = (bool)status1_mask.P2_TTSCAA;
            timestamp_ready.p2TimestampReadyB = (bool)status1_mask.P2_TTSCAB;
            timestamp_ready.p2TimestampReadyC = (bool)status1_mask.P2_TTSCAC;
            desc->cb_func[ETH_MAC_EVT_TIMESTAMP_RDY]
                (desc->cb_param[ETH_MAC_EVT_TIMESTAMP_RDY], 
                 ETH_MAC_EVT_TIMESTAMP_RDY, (void *)&timestamp_ready);
        }
    }


    /* General status callback will fire even if callbacks were executed */ 
    /* for specific status bits                                          */
    if (status0_mask.VALUE32 || status1_mask.VALUE32)
    {
        if (desc->cb_func[ETH_MAC_EVT_STATUS] != NULL)
        {
            desc->cb_func[ETH_MAC_EVT_STATUS]
                (desc->cb_param[ETH_MAC_EVT_STATUS], ETH_MAC_EVT_STATUS, 
                 (void *)&desc->status_registers);
        }
    }

    /* Handle RX/TX ready for dual ports */
    status1_clear.VALUE32 = status1.VALUE32;
    if (!desc->pending_ctrl)
    {
        /* If RX_RDY is asserted, do not clear TX_RDY, */
        /* as we prioritize reading frames             */
        if (status1.P1_RX_RDY || status1.P2_RX_RDY)
        {
#if ADI_PAUSE_RX_IF_NO_BUFFERS
            if (queue_isEmpty(desc->rx_queue))
            {
                /* P*_RX_RDY interrupt sources stay asserted for as long as  */ 
                /* there is something  to read in RxFIFO, but if there are no*/
                /* buffers to read to, the IRQ will keep  being asserted.    */
                /* To avoid a lockup, the P*_RX_RDY interrupts sources are   */ 
                /* masked until an Rx buffer is submitted to the driver.     */
                *irq_mask1 |= ((1 << BITP_MAC_IMASK1_P1_RX_RDY_MASK) | 
                              (1 << BITP_MAC_IMASK1_P2_RX_RDY_MASK));
            }
            else
#endif
            {
                /* Received buffer will be read from the FIFO, */
                /* do not clear TX_RDY yet.                    */
                status1_clear.TX_RDY = 0;
            }
        }
    }
    else
    {
        status1_clear.TX_RDY = 0;
    }

    result = eth_mac_write_register(desc, ADDR_MAC_STATUS1, status1_clear.VALUE32);
    if (result != 0)
    {
        goto end;
    }

#if ADI_PAUSE_RX_IF_NO_BUFFERS || ADI_EDGE_SENSITIVE_IRQ
#if ADI_EDGE_SENSITIVE_IRQ
    /* This will update the IRQ status mask and reassert the pending IRQ,*/
    /* if needed when  the IRQ is edge-triggered. It has no effect when  */
    /* the IRQ is level sensitive.                                       */
    result = eth_mac_write_register(desc, ADDR_MAC_IMASK1, 0xFFFFFFFF);
    if (result != 0)
    {
        goto end;
    }
#endif

    if (ADI_EDGE_SENSITIVE_IRQ || (desc->irq_mask1 != *irq_mask1))
    {
        result = eth_mac_write_register(desc, ADDR_MAC_IMASK1, *irq_mask1);
        if (result != 0)
        {
            goto end;
        }
    }
#endif

    if (desc->pending_ctrl)
    {
	capi_irq_disable(desc->eth_irq);
        goto end;
    }

    /* Handle frame reception for dual ports with priority.          */
    /* Drain ALL available frames per datasheet RX step 4 before TX */
    if (status1.P1_RX_RDY || status1.P2_RX_RDY)
    {
	capi_irq_disable(desc->eth_irq);

       do 
       {
           if (queue_isEmpty(desc->rx_queue))
               break;

            if (status1.P1_RX_RDY_HI && rx_count[1] < 2) { 
                eth_mac_receive_frame(desc, 0); 
                rx_count[1]++; 
            } else if (status1.P2_RX_RDY_HI && rx_count[3] < 2) { 
                eth_mac_receive_frame(desc, 1); 
                rx_count[3]++; 
            } else if (status1.P1_RX_RDY && rx_count[0] < 2) { 
                eth_mac_receive_frame(desc, 0); 
                rx_count[0]++; 
            } else if (status1.P2_RX_RDY && rx_count[2] < 2) { 
                eth_mac_receive_frame(desc, 1); 
                rx_count[2]++; 
            } else
                break;

            desc->mac_request_irq_handler(desc->device);

            result = eth_mac_read_register(desc, ADDR_MAC_STATUS1,
                                      &status1.VALUE32);
            if (result != 0)
                goto end;

        } while (status1.P1_RX_RDY || status1.P2_RX_RDY || status1.P1_RX_RDY_HI 
                 || status1.P2_RX_RDY_HI);
    }

    /* TX gets a guaranteed turn after RX FIFO is fully drained */
    if (status1.TX_RDY || !queue_isEmpty(&desc->tx_queue))
    {
        if (desc->state == ETH_MAC_STATE_READY)
        {       
            result = eth_mac_process_tx_queue(desc, status1);
            if (result != 0)
            {
                goto end;
            }
        }
        else
        {
            // ADI_HAL_SET_PENDING_IRQ(desc->device);
	    stm32_capi_irq_set_pending(ETH_INT_N_IRQn);
        }
    }

end:
#endif
    return;
}

/**
 * @brief Main MAC interrupt callback handler
 * Dispatches to device-specific handlers based on ADIN device type 
 */
// static void mac_callback(void *pCBParam, uint32_t Event, void *pArg)
static void mac_callback(void *pCBParam)
{
    struct eth_mac_desc        *desc = (struct eth_mac_desc *)pCBParam;
    int        result;
    struct eth_mac_status0       status0;
    struct eth_mac_status1       status1;
    struct eth_mac_status0       status0_mask;
    struct eth_mac_status1       status1_mask;
    uint32_t                irq_mask1;

    (void)Event;
    (void)pArg;
    
    /* Callback process when Open Alliance SPI is used */
    if(desc->is_oa_spi_enabled) {
        desc->mac_request_irq_handler(desc->device);
        return;
    }
    
    /* Read interrupt status and immediately clear the status bits */
    result = eth_mac_read_register(desc, ADDR_MAC_STATUS0, &status0.VALUE32);
    if (result != 0)
    {
        goto end;
    }
    result = eth_mac_write_register(desc, ADDR_MAC_STATUS0, status0.VALUE32);
    if (result != 0)
    {
        goto end;
    }

    result = eth_mac_read_register(desc, ADDR_MAC_STATUS1, &status1.VALUE32);
    if (result != 0)
    {
        goto end;
    }

    status0_mask.VALUE32 = status0.VALUE32 & ~desc->irq_mask0;
    status1_mask.VALUE32 = status1.VALUE32 & ~desc->irq_mask1;

    desc->status_registers.p1_status_mask = ETH_MAC_PHY_STATUS_INIT_VAL;
    desc->status_registers.p1_status = ETH_MAC_PHY_STATUS_INIT_VAL;
    
    if(desc->device_type == ADIN_2111){
        desc->status_registers.p2_status_mask = ETH_MAC_PHY_STATUS_INIT_VAL;
        desc->status_registers.p2_status = ETH_MAC_PHY_STATUS_INIT_VAL;
    }

    irq_mask1 = desc->irq_mask1;

    /* Call device-specific handler */
    if(desc->device_type == ADIN_2111){
        mac_callback_adin2111(desc, status0, status1, status0_mask, 
                            status1_mask, &irq_mask1);
    } else {
        mac_callback_adin1110(desc, status0, status1, status0_mask, 
                            status1_mask, &irq_mask1);
    }

end:
    if (result == 0)
    {
        desc->irq_mask1 = irq_mask1;
    }
}

static int eth_mac_config_register(struct eth_mac_desc *desc)
{
    int result = 0;
    uint32_t         val32 = 0;
    uint32_t         imask1P1RxIfgErrMask = BITM_MAC_IMASK1_P1_RX_IFG_ERR_MASK;
    uint32_t         imask1SpiErrMask = BITM_MAC_IMASK1_SPI_ERR_MASK;
    uint32_t         imask1RxEccErrMask = BITM_MAC_IMASK1_RX_ECC_ERR_MASK;
    uint32_t         imask1TxEccErrMask = BITM_MAC_IMASK1_TX_ECC_ERR_MASK;
    uint32_t         imask1TxRdyMask = 0;   /* For generic SPI only          */
    uint32_t         imask1P1RxRdyMask = 0; /* For generic SPI only          */    
    uint32_t         imask1P2Txfcsem = 0;   /* For ADIN2111 only             */
    uint32_t         imask1P2RxRdyMask = 0; /*For ADIN2111 & generic SPI only*/
        
    /* Eventually we need to enable at least the PHY_INT in addition to the */
    /* existing interrupt sources.   */

    /* IMASK0 */
    /* All interrupt sources are unmasked, writing them individually for clarity */
    /* Exception is the PHY interrupt, it is too early to enable it here as this */
    /* is run before the PHY initialization.
                                         */
    if (desc == NULL)
    {
        return -EINVAL;
    }

    desc->irq_mask0 = NO_OS_GENMASK(32, 0) // 0xFFFFFFFF;
    desc->irq_mask0 &= ~(BITM_MAC_IMASK0_TXPEM |
                           BITM_MAC_IMASK0_TXBOEM |
                           BITM_MAC_IMASK0_TXBUEM |
                           BITM_MAC_IMASK0_RXBOEM |
                           BITM_MAC_IMASK0_LOFEM |
                           BITM_MAC_IMASK0_HDREM |
                           BITM_MAC_IMASK0_RESETCM |
                           BITM_MAC_IMASK0_TXFCSEM |
                           BITM_MAC_IMASK0_CDPEM);

    result = eth_mac_write_register(desc, ADDR_MAC_IMASK0, desc->irq_mask0);

    /* IMASK1 */
    desc->irq_mask1 = NO_OS_GENMASK(32, 0) // 0xFFFFFFFF;
    
    if(!desc->is_oa_spi_enabled) {
        imask1TxRdyMask = BITM_MAC_IMASK1_TX_RDY_MASK;
        imask1P1RxRdyMask = BITM_MAC_IMASK1_P1_RX_RDY_MASK;
    }

    if (desc->device_type == ADIN_2111) {
        imask1P2Txfcsem = BITM_MAC_IMASK1_P2_TXFCSEM;
    }
     
    if((desc->device_type == ADIN_2111) && (!desc->is_oa_spi_enabled)){
        imask1P2RxRdyMask = BITM_MAC_IMASK1_P2_RX_RDY_MASK;
    }

    desc->irq_mask1 &= ~(imask1TxRdyMask |
                           imask1P1RxRdyMask |
                           imask1P2RxRdyMask |
                           imask1P1RxIfgErrMask |
                           imask1SpiErrMask |
                           imask1RxEccErrMask |
                           imask1TxEccErrMask |
                           imask1P2Txfcsem
                           );

    result = eth_mac_write_register(desc, ADDR_MAC_IMASK1, desc->irq_mask1);
    if (result != 0)
    {
        return result;
    }

    /* Although the callback is registered here,  */
    // result = (int)ADI_HAL_REGISTER_CALLBACK(desc->device, 
                                // (HAL_Callback_t const *)mac_callback, desc);
    result = capi_irq_connect(ETH_INT_N_IRQn, mac_callback, desc);
    if (result != 0)
    {
        return result;
    }

    result = eth_mac_read_register(desc, ADDR_MAC_STATUS0, &val32);
    if (result != 0)
    {
        return result;
    }
    result = eth_mac_read_register(desc, ADDR_MAC_STATUS1, &val32);
    if (result != 0)
    {
        return result;
    }

    /* Configure the MAC to generate and append FCS to the frame, 
        or expect the FCS to be appended by the host */
    // if (desc->fcs_check_en)
    // {
        // result = (int)ADI_HAL_FCS_INIT(desc->device);
        // if (result != 0)
        // {
            // return result;
        // }
    // }

    result = eth_mac_read_register(desc, ADDR_MAC_CONFIG0, &val32);
    if (result != 0)
    {
        return result;
    }

    if (desc->fcs_check_en)
    {
        val32 |= BITM_MAC_CONFIG0_TXFCSVE;

    }
    else
    {
        val32 &= ~BITM_MAC_CONFIG0_TXFCSVE;
    }
    
    result = eth_mac_write_register(desc, ADDR_MAC_CONFIG0, val32);
    if (result != 0)
    {
        return result;
    }

    result = eth_mac_read_register(desc, ADDR_MAC_CONFIG2, &val32);
    if (result != 0)
    {
        return result;
    }

    if (desc->fcs_check_en)
    {
        val32 &= ~BITM_MAC_CONFIG2_CRC_APPEND;
    }
    else
    {
        val32 |= BITM_MAC_CONFIG2_CRC_APPEND;
    }
    
    result = eth_mac_write_register(desc, ADDR_MAC_CONFIG2, val32);
    if (result != 0)
    {
        return result;
    }
     
    if (desc->device_type == ADIN_2111)
    {
        /* Configure RX CRC checking based on FCS mode:
         * - When fcs_check_en = true (host calculates FCS), enable RX CRC checking
         * - When fcs_check_en = false (hardware appends FCS), disable RX CRC checking
         */
        result = eth_mac_write_register(desc, ADDR_MAC_P1_RX_CRC_EN, desc->fcs_check_en);
        if (result != 0)
        {
            return result;
        }

        result = eth_mac_write_register(desc, ADDR_MAC_P2_RX_CRC_EN, desc->fcs_check_en);
        if (result != 0)
        {
            return result;
        }
    }

    return result;
}

/*!
 * @brief Initialize MAC queue with both generic queue init and MAC-specific frame init.
 *
 * @param [in] queue     Pointer to the MAC queue structure to initialize.
 * @param [in] frames    Pointer to the frame array.
 * @param [in] num_entries Number of entries in the frame array.
 *
 * @return 0 if initialization is successful.
 *         -EINVAL if queue or frames is NULL.
 *         Other error codes from queue_init if generic queue initialization fails.
 */
int eth_mac_init_queue(eth_mac_queue *queue, struct eth_mac_frame *frames, uint32_t num_entries)
{
    int result = 0;

    if (queue == NULL || frames == NULL)
    {
        return -EINVAL;
    }

    /* Initialize the generic queue first with proper type casting */
    result = queue_init(queue, (void*)frames, num_entries);
    if (result != 0)
    {
        return result;
    }

    /* MAC-specific initialization for frame structures */
    for (uint32_t index = 0; index < num_entries; index++)
    {
        frames[index].header.VALUE16 = 0x0000;
        frames[index].buf_desc = NULL;
    }
    
    return result;
}

/*
 * @brief Reset MAC IC
 *
 * @param [in] dev - Pointer to the HW driver.
 *
 * @return 0 in case of success, positive error code otherwise.
*/
int eth_mac_reset(struct eth_mac_desc *desc, 
    enum eth_mac_reset_type reset_type)
{
    int    result = 0;
    uint16_t            keys[MAC_RESET_KEY_COUNT];
    uint32_t            startTime = HAL_GetTick();

    if (desc == NULL)
    {
        return -EINVAL;
    }

    if (desc->state == ETH_MAC_STATE_UNINITIALIZED)
    {
        return -EINVAL;
    }

    /* Disable IRQ here to prevent RESETC from being cleared in the IRQ handler */
    /* Because a reset needs to be followed up by reconfiguration of the device */
    /* and a call to SyncConfig(), the IRQ will be enabled by the latter so it  */
    /* can be left in a disabled state by this reset function.                  */
    capi_irq_disable(desc->eth_irq);

    /* When the reset is executed as part of the initialization routine, here   */
    /* is the first register access, and it may not be fully reset/powered up.  */
    /* To prevent a premature exit with an error, the reset is repeated several */
    /* times if the register write is not successful. This is particularly      */
    /* important when using OPEN Alliance protocol, because the comparison      */
    /* between the control header and the echoed control header will fail       */
    /* immediately if the device is sending invalid data (all 0s) to the host.  */
    
    do
    {
        switch (reset_type)
        {
            case ETH_MAC_RESET_TYPE_MAC_ONLY:
                keys[RST_KEY1_IDX] = RST_MAC_ONLY_KEY1;
                keys[RST_KEY2_IDX] = RST_MAC_ONLY_KEY2;

                /* No checking of results, see comment above. */
                result = eth_mac_write_register(desc, ADDR_MAC_SOFT_RST, 
                                           keys[RST_KEY1_IDX]);
                if (result == 0)
                {
                    result = eth_mac_write_register(desc, ADDR_MAC_SOFT_RST, 
                                               keys[RST_KEY2_IDX]);
                }

                break;

            case ETH_MAC_RESET_TYPE_MAC_PHY:

                /* No checking of results, see comment above. */
                result = eth_mac_write_register(desc, ADDR_MAC_RESET, 
                                          (1 << BITP_MAC_RESET_SWRESET));
                break;

            default:
                return -EINVAL;
        }
    } while ((result != 0) && 
             ((HAL_GetTick() - startTime) < COMM_INTERFACE_STARTUP_TIME_MS));

    if (result != 0)
    {
        return -ETIMEDOUT;
    }

    if ((reset_type == ETH_MAC_RESET_TYPE_MAC_ONLY) || 
        (reset_type == ETH_MAC_RESET_TYPE_MAC_PHY))
    {
        /* Wait for MAC reset to finish */
        result = eth_mac_wait_device_ready(desc);
        if (result != 0)
        {   
            return result;
        }
    }

    /* After reset, the configuration is no longer synchronized */
    desc->config_sync = false;
    /* Initialize IRQs and other registers */
    result = eth_mac_config_register(desc); 

    return result;
}



/*
 * @brief Register callback function for MAC.
 *
 * @param [in]      desc     Device driver handle.
 *
 * @return 0 in case of success, positive error code otherwise.
 *
 */
int eth_mac_register_callback(struct eth_mac_desc *desc, 
    struct eth_mac_callback cb_func, enum eth_mac_event cb_event, void *cb_param)
{
    int            result = 0;
    uint32_t                    currentirq_mask1 = 0;
    uint32_t                    currentirq_mask0 = 0;
    bool                        irqMaskModified = false;

    if (desc == NULL)
    {
        return -EINVAL;
    }
    
    currentirq_mask1 = desc->irq_mask1;
    currentirq_mask0 = desc->irq_mask0;
    
    desc->cb_func[cb_event] = cb_func;
    desc->cb_param[cb_event] = cb_param;

    switch (cb_event)
    {
        case ETH_MAC_EVT_LINK_CHANGE:
             /* Link status and link status change behave 
                differently in ADIN2111 */
            if (desc->device_type != ADIN_2111) {
                if (currentirq_mask1 & BITM_MAC_IMASK1_LINK_CHANGE_MASK)
                {
                    currentirq_mask1 &= ~BITM_MAC_IMASK1_LINK_CHANGE_MASK;
                    irqMaskModified = true;
                }
            }

            break;

        case ETH_MAC_EVT_TIMESTAMP_RDY:
            if (desc->timestamp_format == ETH_MAC_TS_FORMAT_NONE)
            {
                return ETH_MAC_NO_TS_FORMAT;
            }

            if (desc->device_type == ADIN_2111) {
                if (currentirq_mask1 & (BITM_MAC_IMASK1_P2_TTSCAAM | 
                                       BITM_MAC_IMASK1_P2_TTSCABM | 
                                       BITM_MAC_IMASK1_P2_TTSCACM))
                {
                    currentirq_mask1 &= ~(BITM_MAC_IMASK1_P2_TTSCAAM | 
                                         BITM_MAC_IMASK1_P2_TTSCABM | 
                                         BITM_MAC_IMASK1_P2_TTSCACM);
                    irqMaskModified = true;
                }
            }

            if (currentirq_mask0 & (BITM_MAC_IMASK0_TTSCAAM | 
                                   BITM_MAC_IMASK0_TTSCABM | 
                                   BITM_MAC_IMASK0_TTSCACM))
            {
                currentirq_mask0 &= ~(BITM_MAC_IMASK0_TTSCAAM | 
                                     BITM_MAC_IMASK0_TTSCABM | 
                                     BITM_MAC_IMASK0_TTSCACM);
                irqMaskModified = true;
            }
            break;

        default:
            irqMaskModified = false;
    }

    if (irqMaskModified)
    {
        result = eth_mac_write_register(desc, ADDR_MAC_IMASK0, 
                                   currentirq_mask0);
        if (result != 0)
        {
            return result;
        }
        result = eth_mac_write_register(desc, ADDR_MAC_IMASK1, 
                                   currentirq_mask1);
        if (result != 0)
        {
            return result;
        }
    }

    desc->irq_mask0 = currentirq_mask0;
    desc->irq_mask1 = currentirq_mask1;

    return result;
}

int eth_mac_read_register(struct eth_mac_desc *desc, 
    uint16_t reg_addr, uint32_t *reg_data)
{
    int    result = 0;
    uint8_t             buf[ETH_MAC_SPI_ACCESS_SIZE];
    uint8_t             backup = 0;

    if (desc == NULL)
    {
        return -EINVAL;
    }

    if (desc->state == ETH_MAC_STATE_UNINITIALIZED)
    {
        result = ETH_MAC_DEVICE_UNINITIALIZED;
        goto end;
    }
    
    /* For OA SPI, wait for any in-progress SPI transaction */
    /* to complete  before attempting register access.               */
    if(desc->is_oa_spi_enabled) {
        result = desc->mac_wait_until_ready(desc->device);
        if (result != 0)
        {
            return result;
        }
    }
    
    result = desc->mac_get_comm_status(desc->device, &backup, true);
    if (result == ETH_MAC_COMM_TIMEOUT)
        goto end;

    if(desc->is_oa_spi_enabled) {
        /* when Open Alliance SPI is used */
        (void)buf;       
        /*TODO:Interface to SPI_Read to access oaStateMachine of OA SPI*/
        result = desc->mac_read(desc->device, 
                                   ETH_MAC_STATE_CONTROL_START, reg_addr, 
                                   reg_data, 1, false);  
        
        /* If an error occurred during OA SPI transaction, reset to a */
        /* known 'good' state. This will allow the driver to recover. */
        result = desc->mac_request_irq_handler(desc->device);
        if (result == 0) {
            desc->state =  ETH_MAC_STATE_READY;
        }
        else
        {   
            desc->state =  ETH_MAC_STATE_READY;
            return result;
        }
    }
    else {      
        /* When generic SPI is used */
        /* TODO: Interface to SPI_Read function *(For Generic SPI)*/
        result = desc->mac_read(desc->device, 
                                   ETH_MAC_STATE_CONTROL_START, reg_addr, (void *)buf, 
                                   ETH_MAC_SPI_ACCESS_SIZE, true);
        
        /* 2 bytes is used for Silicon 1 backward compatibility */
        /* 4 bytes is used for new Silicon                      */
        if(ETH_MAC_SPI_ACCESS_SIZE == 2U) { 
            *reg_data = HTON16((*(uint16_t *)buf));
        }
        else if (ETH_MAC_SPI_ACCESS_SIZE == 4U) {
            *reg_data = HTON32((*(uint32_t *)buf));
        }
        else {
            result = ETH_MAC_PLACEHOLDER_ERROR;
            goto end;
        }
    }


end:

    desc->pending_ctrl = false;

    if (backup)
    {
	capi_irq_enable(desc->eth_irq);
    }

    return result;
}


int eth_mac_write_register(struct eth_mac_desc *desc, 
    uint16_t reg_addr, uint32_t reg_data)
{
    int    result = 0;
    uint8_t             buf[ETH_MAC_SPI_ACCESS_SIZE];
    uint8_t             backup = 0;
    
    if (desc == NULL)
    {
        return -EINVAL;
    }

    if (desc->state == ETH_MAC_STATE_UNINITIALIZED)
    {
        result = ETH_MAC_DEVICE_UNINITIALIZED;
        goto end;
    }
    /*TODO: Interface to SPI_GetStatus*/
    result = desc->mac_get_comm_status(desc->device, &backup, true);
    if (result == ETH_MAC_COMM_TIMEOUT)
        goto end;


    if(desc->is_oa_spi_enabled){ // when Open Alliance SPI is used
        (void)buf;
        /*TODO: Interface to SPI_Write func to access oaStateMachine (OA SPI)*/
        result = desc->mac_write(desc->device,ETH_MAC_STATE_CONTROL_START,
                                     reg_addr, &reg_data, 1, false);

        /* If an error occurred during OA SPI transaction, reset to a */
        /* known 'good' state. This will allow the driver to recover. */
        result = desc->mac_wait_until_ready(desc->device);
        if (result == 0) {
            desc->state =  ETH_MAC_STATE_READY;
        }
        else
        {   
            desc->state =  ETH_MAC_STATE_READY;
            return result;
        }
    }
    else{ //When generic SPI is used
        
        /* 2 bytes is used for Silicon 1 backward compatibility */
        /* 4 bytes is used for new Silicon                      */
        if (ETH_MAC_SPI_ACCESS_SIZE == 2U) {
            *(uint16_t *)buf = HTON16(reg_data);
        }        
        else if (ETH_MAC_SPI_ACCESS_SIZE == 4U){
            *(uint32_t *)buf = HTON32(reg_data);
        }
        else {
            result = ETH_MAC_PLACEHOLDER_ERROR;
            goto end;
        }

        /* TODO: Interface to SPI_Write function (For Generic SPI)*/
        result = desc->mac_write(desc->device,ETH_MAC_STATE_CONTROL_START,
                                    reg_addr, &buf, 
                                    ETH_MAC_SPI_ACCESS_SIZE, true);
    }
    

end:
    desc->pending_ctrl = false;
    if (backup)
    {
	capi_irq_enable(desc->eth_irq);
    }

    return result;
}

static int eth_mac_receive_frame(struct eth_mac_desc *desc, uint32_t port)
{
    int        result = 0;
    eth_mac_queue         *rx_queue = desc->rx_queue;
    struct eth_mac_frame   *pEntries = rx_queue->pEntries;
    struct eth_mac_buf_desc       *pRxBufDesc = pEntries[rx_queue->tail].buf_desc;
    uint32_t                rxSize = 0;
    uint32_t                reg_addr = 0;
    
    if (desc->state == ETH_MAC_STATE_UNINITIALIZED)
    {
        return -EINVAL;
    }
 
    reg_addr = ADDR_MAC_P1_RX_FSIZE;
 
    if (desc->device_type == ADIN_2111) {
        if (port == 1)
        {
            reg_addr = ADDR_MAC_P2_RX_FSIZE;
        }
    }
 
    desc->state = ETH_MAC_STATE_RX_FRAME;
 
    if ((result = eth_mac_read_register(desc,reg_addr,&rxSize)) != 0)
    {
        goto end;
    }
    /* Do not try to read a frame if size is 0 */
    if (!rxSize)
    {
        goto end;
    }
    /* Allow the user to swap in another buffer/buffer desc, 
       with a suitable size for the received frame. */
    if (desc->cb_func[ETH_MAC_EVT_RX_FRAME_RDY] != NULL)
    {
        desc->cb_func[ETH_MAC_EVT_RX_FRAME_RDY](desc->app_dev, 
                                                  rxSize, (void *)pRxBufDesc);
    }
 
    pRxBufDesc->trxSize = rxSize;
 
    /* Odd-size frames need an extra byte */
    if (rxSize & 1)
    {
        rxSize++;
    }
 
    reg_addr = ADDR_MAC_P1_RX;
 
    if (desc->device_type == ADIN_2111) {
        if (port)
        {
            reg_addr = ADDR_MAC_P2_RX;
        }
    }
    /* TODO: Interface to  SPI_Read function (For Generic SPI)*/
    result = desc->mac_read(desc->device, ETH_MAC_STATE_DATA_START,reg_addr,
                               pRxBufDesc->pBuf, rxSize, false);
 
end:
    pEntries[desc->rx_queue->tail].buf_desc = pRxBufDesc;
    rx_queue->pEntries = pEntries;
    desc->rx_queue = rx_queue;
 
    desc->state = ETH_MAC_STATE_READY;
    return result;
}

static int eth_mac_wait_mdio_ready(struct eth_mac_desc *desc, uint16_t addr_offset)
{
    int        result = 0;
    ETH_MAC_MDIOACC_0__t    mdioCmd;
    uint32_t                startTime = HAL_GetTick();
    bool                    mdioRdy = false;

    do
    {
        result = eth_mac_read_register(desc, addr_offset, &mdioCmd.VALUE32);
        if (result != 0)
        {
            break;
        }
        else if (mdioCmd.MDIO_TRDONE)
        {
            mdioRdy = true;
        }
    } while (((result != 0) || !mdioRdy) && 
             ((HAL_GetTick() - startTime) < COMM_INTERFACE_STARTUP_TIME_MS));

    result = ((result == 0) && mdioRdy) ? 0: -ETIMEDOUT;

    return result;
}

static int eth_mac_wait_device_ready(struct eth_mac_desc *desc)
{
    int    result = 0;
    uint32_t            startTime = HAL_GetTick();
    bool                resetDone = false;
    uint32_t            status0;
    bool                commOk = false;
    uint32_t            phyId;

    /* Poll PHYID register to establish the device 
       has been brought up (powered up, out of reset). */

    if (desc == NULL)
    {
        return -EINVAL;
    }
   

    while ((HAL_GetTick() - startTime) < COMM_INTERFACE_STARTUP_TIME_MS)
    {
        result = eth_mac_read_register(desc, ADDR_MAC_PHYID, &phyId);
        if ((result == 0) && (phyId == RSTVAL_MAC_PHYID))
        {
            commOk = true;
            break;
        }
    }

    if (!commOk)
    {   
        return -ETIMEDOUT;
    }

    /* Now we can check RESETC without worrying about status0 
       comming back as all 0xF due to MAC-PHY still in reset. */
    while ((HAL_GetTick() - startTime) < COMM_INTERFACE_STARTUP_TIME_MS)
    {
        result = eth_mac_read_register(desc, ADDR_MAC_STATUS0, &status0);
        if ((result == 0) && 
            ((status0 & BITM_MAC_STATUS0_RESETC) == BITM_MAC_STATUS0_RESETC))
        {
            resetDone = true;
            result = eth_mac_write_register(desc, ADDR_MAC_STATUS0, 
                                       BITM_MAC_STATUS0_RESETC);
            break;
        }
    }
    
    if (!resetDone)
    {
        result = -ETIMEDOUT;
    }
    
    return result;
}


/*
 * @brief PHY Register Read via SPI<->MDIO bridge
 *
 * @param [in] instance - Pointer to the HW driver.
 * @param [in] hw_addr - PHY MDIO Hardware address.
 * @param [in] reg_addr - PHY MDIO Register address to read.
 * @param [out] data - Pointer to the data buffer.
 *
 * @return 0 in case of success, positive error code otherwise.
*/
int eth_mac_phy_read(struct eth_mac_desc *desc,  uint8_t hw_addr, 
    uint32_t reg_addr, uint16_t *reg_data)
{
    int       result = 0;
    ETH_MAC_MDIOACC_0__t   mdioCmd;

    if (desc == NULL)
    {
        return -EINVAL;
    }

    /* Use the first MDIO register for the address operation.   */
    /* MDIO Speed defaults to 2.5MHz per CONFIG2.MSPEED.        */
    mdioCmd.MDIO_DEVAD = DEVTYPE(reg_addr);
    mdioCmd.MDIO_ST = ENUM_MAC_MDIOACC_N__MDIO_ST_CLAUSE45;
    mdioCmd.MDIO_PRTAD = hw_addr;
    mdioCmd.MDIO_OP = ENUM_MAC_MDIOACC_N__MDIO_OP_MD_ADDR;
    mdioCmd.MDIO_DATA = REGADDR(reg_addr);
    mdioCmd.MDIO_TRDONE = 0;
    
    if((result = eth_mac_write_register(desc, ADDR_MAC_MDIOACC_0_, 
        mdioCmd.VALUE32)) != 0)
    {
        return result;
    }

    /* Use the next MDIO register for the read operation.       */
    mdioCmd.MDIO_OP = ENUM_MAC_MDIOACC_N__MDIO_OP_MD_RD;
    mdioCmd.MDIO_DATA = 0;
    mdioCmd.MDIO_TRDONE = 0;
    if ((result = eth_mac_write_register(desc, ADDR_MAC_MDIOACC_1_, 
        mdioCmd.VALUE32)) != 0)
    {
        return result;
    }

    result = eth_mac_wait_mdio_ready(desc, ADDR_MAC_MDIOACC_1_);
    if (result != 0)
    {
        return result;
    }
    result = eth_mac_read_register(desc, ADDR_MAC_MDIOACC_1_, &mdioCmd.VALUE32);
    *reg_data = mdioCmd.MDIO_DATA;

    return result;
}


int eth_mac_phy_write(struct eth_mac_desc *desc,  uint8_t hw_addr, 
    uint32_t reg_addr, uint16_t data)
{
    int       result = 0;
    ETH_MAC_MDIOACC_0__t   mdioCmd;
    
    if (desc == NULL)
    {
        return -EINVAL;
    }

    /* Use the first MDIO register for the address operation.   */
    /* MDIO Speed defaults to 2.5MHz per CONFIG2.MSPEED.        */
    mdioCmd.MDIO_DEVAD = DEVTYPE(reg_addr);
    mdioCmd.MDIO_ST = ENUM_MAC_MDIOACC_N__MDIO_ST_CLAUSE45;
    mdioCmd.MDIO_PRTAD = hw_addr;
    mdioCmd.MDIO_OP = ENUM_MAC_MDIOACC_N__MDIO_OP_MD_ADDR;
    mdioCmd.MDIO_DATA = REGADDR(reg_addr);
    mdioCmd.MDIO_TRDONE = 0;
    if((result = eth_mac_write_register(desc, ADDR_MAC_MDIOACC_0_, 
        mdioCmd.VALUE32)) != 0)
    {
        return result;
    }

    /* Use the next MDIO register for the write operation.      */
    mdioCmd.MDIO_OP = ENUM_MAC_MDIOACC_N__MDIO_OP_MD_WR;
    mdioCmd.MDIO_DATA = data;
    mdioCmd.MDIO_TRDONE = 0;
    if ((result = eth_mac_write_register(desc, ADDR_MAC_MDIOACC_1_, 
        mdioCmd.VALUE32)) != 0)
    {
        return result;
    }

    result = eth_mac_wait_mdio_ready(desc, ADDR_MAC_MDIOACC_1_);
    if (result != 0)
    {
        return result;
    }

    /* Return successful result*/
    return result;
}


static void eth_mac_calculate_fcs(struct eth_mac_desc *desc, 
    struct eth_mac_buf_desc *buf_desc)
{
    if (desc == NULL)
    {
        return;
    }

    if (buf_desc == NULL)
    {
        return;
    }

    uint32_t bufTrxSize = buf_desc->trxSize;
    uint32_t calculatedFcs = 0;
        
    if (desc->fcs_check_en) {            
        /* Calculate and append FCS */
        calculatedFcs = calculate_fcs(&buf_desc->pBuf[0], bufTrxSize);
        /*FCS appended to buffer*/
        memcpy(&buf_desc->pBuf[bufTrxSize], &calculatedFcs, FCS_SIZE);
        
        bufTrxSize += FCS_SIZE;
        
        /*Transaction size increased by FCS_SIZE (4 bytes)*/
        buf_desc->trxSize = bufTrxSize;
    }
}

int eth_mac_submit_tx_buffer(struct eth_mac_desc *desc,
    struct eth_mac_frame_header header, struct eth_mac_buf_desc *buf_desc)
{
    int        result = 0;
    int        commResult;
    struct eth_mac_frame   *pEntries;
    struct eth_mac_status1       status1;
    critical_state_t        basepri;
    bool                    queueIsEmpty;
    
    if (desc == NULL)
        return -EINVAL;

    pEntries = desc->tx_queue.pEntries; 
    
    if (desc->state == ETH_MAC_STATE_UNINITIALIZED)
        return -ENODEV;

    if (buf_desc == NULL)
        return -EINVAL;

    if ((buf_desc->trxSize < MIN_FRAME_SIZE) || 
        (buf_desc->trxSize > MAX_FRAME_SIZE)) {
        result =  ETH_MAC_PARAM_OUT_OF_RANGE;
        goto end;
    }

    /* If FCS is calculated on the host, the buffer 
       needs 4 bytes more to store the FCS. */
    if (desc->fcs_check_en)
    {
        if ((buf_desc->bufSize < buf_desc->trxSize) || 
            (buf_desc->bufSize - buf_desc->trxSize < FCS_SIZE)) {
            result = ETH_MAC_PARAM_OUT_OF_RANGE;
            goto end;
        }
    } 

    basepri = adi_hal_enter_critical_section(ADI_HAL_INT_PRI_DMA_SPI_TX);

    if (queue_isFull(&desc->tx_queue)){
        result = ETH_MAC_QUEUE_FULL;
        goto end;
    }
    
    pEntries[desc->tx_queue.head].header.VALUE16 = header.VALUE16;

    eth_mac_calculate_fcs(desc, buf_desc);
    
    pEntries[desc->tx_queue.head].buf_desc = buf_desc;

    queue_addEntry(&desc->tx_queue);

end:
    queueIsEmpty = queue_isEmpty(&desc->tx_queue);

    adi_hal_exit_critical_section(basepri);
    
    if (!queueIsEmpty) {
        commResult = desc->mac_get_comm_status(desc->device, NULL, false);
        if (commResult == 0)
        {
            eth_mac_process_tx_queue(desc,status1);
        }
    }
    
    return result;
}


int eth_mac_submit_rx_buffer(struct eth_mac_desc *desc, 
    struct eth_mac_buf_desc *buf_desc)
{
    int        result = 0;
    int        commResult;
#if ADI_PAUSE_RX_IF_NO_BUFFERS //Defined in hal_port_specific.h
    uint32_t                rxRdyMask;
#endif
    eth_mac_queue         rcv_queue = {0};
    struct eth_mac_frame   *pEntries;
    eth_mac_State_e         currentState;
    critical_state_t        basepri;

    if (desc == NULL)
    {
        return -EINVAL;
    }
    
    currentState = desc->state;
    
    if (buf_desc == NULL)
    {
        return -EINVAL;
    }
    
    rcv_queue = desc->rx_queue_lp;
    pEntries = rcv_queue.pEntries;
    
    if (currentState == ETH_MAC_STATE_UNINITIALIZED)
    {
        return -ENODEV;
    }

    /* For Rx we only need to check for minimum size. */
    /* The buffer will always have the FCS appended. */
    if (buf_desc->bufSize < MIN_FRAME_SIZE + FCS_SIZE)
    {
        return -ERANGE;
    }

    basepri = adi_hal_enter_critical_section(ADI_HAL_INT_PRI_DMA_SPI_RX);

    if (queue_isFull(&rcv_queue)) {
        adi_hal_exit_critical_section(basepri);
	return -EXFULL;
    }

    pEntries[rcv_queue.head].header.VALUE16 = 0x0000;
    pEntries[rcv_queue.head].buf_desc = buf_desc;

    queue_addEntry(&rcv_queue);

    desc->rx_queue_lp = rcv_queue;

    adi_hal_exit_critical_section(basepri);
    
    if(desc->is_oa_spi_enabled) {
        /* If idle, try to start a data transaction. */
        commResult = desc->mac_get_comm_status(desc->device, NULL, false);
        if (commResult == 0)
        {    
            /*TODO: Interface to SPI_Read to access oaStateMachine of OA SPI*/
            result = desc->mac_read(desc->device,
                                ETH_MAC_STATE_DATA_START, 0, NULL,
                                    0, false);
            return result;
        }
    }
    else { 
        #if ADI_PAUSE_RX_IF_NO_BUFFERS //Defined in hal_port_specific.h
        /* If RX_RDY interrupt sources were masked before   */
        /* due to no available Rx buffers, unmask them now. */

        if (desc->device_type == ADIN_2111) {
            rxRdyMask = ((1 << BITP_MAC_IMASK1_P1_RX_RDY_MASK) | 
                         (1 << BITP_MAC_IMASK1_P2_RX_RDY_MASK));
        }
        else {
            rxRdyMask = (1 << BITP_MAC_IMASK1_P1_RX_RDY_MASK);
        }

        if (desc->irq_mask1 & rxRdyMask)
        {
            desc->irq_mask1 &= ~rxRdyMask;
            // ADI_HAL_SET_PENDING_IRQ(desc->device);
	    stm32_capi_irq_set_pending(ETH_INT_N_IRQn);
        }

        if (result != 0)
        {
            return result;
        }
        #endif //ADI_PAUSE_RX_IF_NO_BUFFERS
    }

    desc->state = currentState;

    /* Return successful result */
    return result;
}

int eth_mac_submit_rx_buffer_hp(struct eth_mac_desc *desc, struct eth_mac_buf_desc *buf_desc)
{
    int        result = 0;
    int        commResult;
#if ADI_PAUSE_RX_IF_NO_BUFFERS //Defined in hal_port_specific.h
    uint32_t                rxRdyMask;
#endif
    eth_mac_queue         rcv_queue = {0};
    struct eth_mac_frame   *pEntries;
    eth_mac_State_e         currentState;
    critical_state_t        basepri;
    
    if (desc == NULL)
    {
        return -EINVAL;
    }
    
    currentState = desc->state;
    
    if (buf_desc == NULL)
    {
        return -EINVAL;
    }

    rcv_queue = desc->rx_queue_hp;
    pEntries = rcv_queue.pEntries;

    if (currentState == ETH_MAC_STATE_UNINITIALIZED)
    {
        return -ENODEV;
    }

    /* For Rx we only need to check for minimum size. */
    /* The buffer will always have the FCS appended. */
    if (buf_desc->bufSize < MIN_FRAME_SIZE + FCS_SIZE)
    {
        return -ERANGE;
    }

    basepri = adi_hal_enter_critical_section(ADI_HAL_INT_PRI_DMA_SPI_RX);

    if (queue_isFull(&rcv_queue)) {
        adi_hal_exit_critical_section(basepri);
        return -EXFULL;
    }

    pEntries[rcv_queue.head].header.VALUE16 = 0x0000;
    pEntries[rcv_queue.head].buf_desc = buf_desc;

    queue_addEntry(&rcv_queue);
    
    desc->rx_queue_hp = rcv_queue;

    adi_hal_exit_critical_section(basepri);

    if(desc->is_oa_spi_enabled)  {
        commResult = desc->mac_get_comm_status(desc->device, NULL, false);
        if (commResult == 0)
        {            
            /*TODO:Interface to SPI_Read to access oaStateMachine  of OA SPI*/
            (void)desc->mac_read(desc->device,
                                    ETH_MAC_STATE_DATA_START, 0, NULL,
                                    0, false);
        }
    }
    else {
        #if ADI_PAUSE_RX_IF_NO_BUFFERS //Defined in hal_port_specific.h
        /* If RX_RDY interrupt sources were masked before   */
        /* due to no available Rx buffers, unmask them now. */

        if (desc->device_type == ADIN_2111) {
            rxRdyMask = ((1 << BITP_MAC_IMASK1_P1_RX_RDY_MASK) | 
                         (1 << BITP_MAC_IMASK1_P2_RX_RDY_MASK));
        }
        else {
            rxRdyMask = (1 << BITP_MAC_IMASK1_P1_RX_RDY_MASK);
        }

        if (desc->irq_mask1 & rxRdyMask)
        {
            desc->irq_mask1 &= ~rxRdyMask;
            // ADI_HAL_SET_PENDING_IRQ(desc->device);
	    stm32_capi_irq_set_pending(ETH_INT_N_IRQn);
        }

        if (result != 0)
        {
            return result;
        }
        #endif
    }
    
    desc->state = currentState;

    return result;
}


/*!
 * @brief           Send frame to the MAC.
 *
 * @param [in]      desc             Device driver handle.
 * @param [in]      frame              Pointer to the frame definition.
 * @param [in]      is_oa_spi_enabled      Enable Open Alliance SPI protocol.
 *
 * @return          Status
 *                  - #0              Call completed successfully.
 *
 * @details         Executes the state machine to start sending a frame to the MAC.
 *
 */
static int eth_mac_send_frame(struct eth_mac_desc *desc, 
    struct eth_mac_frame *frame, ETH_MAC_STATUS1_t status1)
{
    int          result = 0;
    uint16_t                  nBytes;
    uint32_t                  txFifoAvail;
    eth_mac_State_e           stateBackup;

    if (desc->is_oa_spi_enabled) {   
        /*TODO: Interface to SPI_Write func to access oaStateMachine of OA SPI*/
        result = desc->mac_write(desc->device, 
                                    ETH_MAC_STATE_DATA_START, 0,
                                    NULL, 0, false);
        return result;
    }

    /* Generic SPI path */
    stateBackup = desc->state;

    /*TODO: Interface to SPI_GetStatus function */
    if (desc->mac_get_comm_status(desc->device, NULL, false) 
         != 0) {
        result = ETH_MAC_COMM_BUSY;
        goto end;
    }

    desc->state = ETH_MAC_STATE_TX_FRAME;
      
    if (desc->tx_cte && status1.TX_RDY) { 
        nBytes = frame->buf_desc->trxSize + ADI_FRAME_HEADER_SIZE;
        
        result = eth_mac_write_register(desc, ADDR_MAC_TX_FSIZE, nBytes);
        if (result != 0)
            goto end;
    } else {
        result = eth_mac_read_register(desc, ADDR_MAC_TX_SPACE, &txFifoAvail);
        if (result != 0)
            goto end;

        nBytes = frame->buf_desc->trxSize + ADI_FRAME_HEADER_SIZE;

        /* TX_SPACE contains the number of halfwords available in the TxFIFO, 
           multiply by 2 to match the frame size in bytes */
        if (2 * txFifoAvail < nBytes + ETH_MAC_TX_FIFO_BUFFER) {
            result = -ERANGE;
            goto end;
        }

        result = eth_mac_write_register(desc, ADDR_MAC_TX_FSIZE, nBytes);
        if (result != 0)
            goto end;
    }
   
    /*TODO: Interface to SPI_WriteFrame function */
    result = desc->mac_write_frame(desc->device, frame, false);
    if (result != 0)
        goto end;
     
    desc->state = ETH_MAC_STATE_READY;
end: 

    if (result != 0)
        desc->state = stateBackup;

    return result;
}

static int eth_mac_process_tx_queue(struct eth_mac_desc *desc, struct eth_mac_status1 status1)
{
    int        result = 0;
    eth_mac_queue         tx_queue = desc->tx_queue;
    struct eth_mac_frame   *pEntries = tx_queue.pEntries;
    uint32_t                backup;

    critical_state_t basepri = adi_hal_enter_critical_section(
                                                    ADI_HAL_INT_PRI_DMA_SPI_TX);

    backup = capi_irq_get_enable(desc->eth_irq);

    capi_irq_disable(desc->eth_irq);

    bool queueIsEmpty = queue_isEmpty(&desc->tx_queue);

    adi_hal_exit_critical_section(basepri);

    if (!queueIsEmpty) {
        result = desc->mac_get_comm_status(desc->device, NULL, false);        
        if (result == 0)
        {
            result = eth_mac_send_frame(desc, &pEntries[tx_queue.tail],status1);
            if (result != 0)
            {
		capi_irq_enable(desc->eth_irq);

            }
        }  
    }
    else
    {
        if (backup)
        {
	    capi_irq_enable(desc->eth_irq);
        }
    }

    return result;
}

/*
 * @brief  MAC Get Statistics Counters
 *
 * @param [in] dev - Pointer to the HW driver.
 * @param [out] stat - statistics counters.
 *
 * @return 0 in case of success, positive error code otherwise.
*/
int eth_mac_get_stat_counters(struct eth_mac_desc *desc, uint32_t port, struct eth_mac_stat_counter *stat)
{
    int            result      = 0;
    uint16_t                    baseAddr    = 0;

    if (desc == NULL)
    {
        result = -EINVAL;
        goto end;
    }

    if (desc->state == ETH_MAC_STATE_UNINITIALIZED)
    {
        result = -ENODEV;
        goto end;
    }

    if (desc->is_oa_spi_enabled == true) {
        if(desc->device_type == ADIN_2111) {
            if (port) {
                baseAddr = ADDR_MAC_P2_RX_FRM_CNT - ADDR_MAC_P1_RX_FRM_CNT;
            }
            else {
                baseAddr = 0;
            }
        }
    
        result = eth_mac_read_register(desc, baseAddr + ADDR_MAC_P1_RX_FRM_CNT, &stat->RX_FRM_CNT);
        if (result != 0)
        {
            goto end;
        }
        result = eth_mac_read_register(desc, baseAddr + ADDR_MAC_P1_RX_BCAST_CNT, &stat->RX_BCAST_CNT);
        if (result != 0)
        {
            goto end;
        }
        result = eth_mac_read_register(desc, baseAddr + ADDR_MAC_P1_RX_MCAST_CNT, &stat->RX_MCAST_CNT);
        if (result != 0)
        {
            goto end;
        }
        result = eth_mac_read_register(desc, baseAddr + ADDR_MAC_P1_RX_UCAST_CNT, &stat->RX_UCAST_CNT);
        if (result != 0)
        {
            goto end;
        }
        result = eth_mac_read_register(desc, baseAddr + ADDR_MAC_P1_RX_CRC_ERR_CNT, &stat->RX_CRC_ERR_CNT);
        if (result != 0)
        {
            goto end;
        }
        result = eth_mac_read_register(desc, baseAddr + ADDR_MAC_P1_RX_ALGN_ERR_CNT, &stat->RX_ALGN_ERR_CNT);
        if (result != 0)
        {
            goto end;
        }
        result = eth_mac_read_register(desc, baseAddr + ADDR_MAC_P1_RX_LS_ERR_CNT, &stat->RX_LS_ERR_CNT);
        if (result != 0)
        {
            goto end;
        }
        result = eth_mac_read_register(desc, baseAddr + ADDR_MAC_P1_RX_PHY_ERR_CNT, &stat->RX_PHY_ERR_CNT);
        if (result != 0)
        {
            goto end;
        }
        result = eth_mac_read_register(desc, baseAddr + ADDR_MAC_P1_TX_FRM_CNT, &stat->TX_FRM_CNT);
        if (result != 0)
        {
            goto end;
        }
        result = eth_mac_read_register(desc, baseAddr + ADDR_MAC_P1_TX_BCAST_CNT, &stat->TX_BCAST_CNT);
        if (result != 0)
        {
            goto end;
        }
        result = eth_mac_read_register(desc, baseAddr + ADDR_MAC_P1_TX_MCAST_CNT, &stat->TX_MCAST_CNT);
        if (result != 0)
        {
            goto end;
        }
        result = eth_mac_read_register(desc, baseAddr + ADDR_MAC_P1_TX_UCAST_CNT, &stat->TX_UCAST_CNT);
        if (result != 0)
        {
            goto end;
        }
        result = eth_mac_read_register(desc, baseAddr + ADDR_MAC_P1_RX_DROP_FULL_CNT, &stat->RX_DROP_FULL_CNT);
        if (result != 0)
        {
            goto end;
        }
        result = eth_mac_read_register(desc, baseAddr + ADDR_MAC_P1_RX_DROP_FILT_CNT, &stat->RX_DROP_FILT_CNT);
        if (result != 0)
        {
            goto end;
        }
    } else {
            /* Use burst read for Generic SPI. */
            struct eth_mac_stat_counter   locStat;

            if(desc->device_type == ADIN_2111) {
                if (port)
                {
                    baseAddr = ADDR_MAC_P2_RX_FRM_CNT;
                }
                else
                {
                    baseAddr = ADDR_MAC_P1_RX_FRM_CNT;
                }
            } else {
                baseAddr = ADDR_MAC_P1_RX_FRM_CNT;
            }

            result = desc->mac_read(desc->device, ETH_MAC_STATE_CONTROL_START, baseAddr, &locStat, sizeof(struct eth_mac_stat_counter), true);

            stat->RX_FRM_CNT         = HTON32(locStat.RX_FRM_CNT);
            stat->RX_BCAST_CNT       = HTON32(locStat.RX_BCAST_CNT);
            stat->RX_MCAST_CNT       = HTON32(locStat.RX_MCAST_CNT);
            stat->RX_UCAST_CNT       = HTON32(locStat.RX_UCAST_CNT);
            stat->RX_CRC_ERR_CNT     = HTON32(locStat.RX_CRC_ERR_CNT);
            stat->RX_ALGN_ERR_CNT    = HTON32(locStat.RX_ALGN_ERR_CNT);
            stat->RX_LS_ERR_CNT      = HTON32(locStat.RX_LS_ERR_CNT);
            stat->RX_PHY_ERR_CNT     = HTON32(locStat.RX_PHY_ERR_CNT);
            stat->TX_FRM_CNT         = HTON32(locStat.TX_FRM_CNT);
            stat->TX_BCAST_CNT       = HTON32(locStat.TX_BCAST_CNT);
            stat->TX_MCAST_CNT       = HTON32(locStat.TX_MCAST_CNT);
            stat->TX_UCAST_CNT       = HTON32(locStat.TX_UCAST_CNT);
            stat->RX_DROP_FULL_CNT   = HTON32(locStat.RX_DROP_FULL_CNT);
            stat->RX_DROP_FILT_CNT   = HTON32(locStat.RX_DROP_FILT_CNT);
    }

end:
    return result;
}


int eth_mac_add_address_filter(struct eth_mac_desc *desc, 
    uint8_t *mac_addr, uint8_t *mac_addr_mask, uint16_t addr_rule)
{
    int    result = 0;
    uint32_t            addr_index = ETH_MAC_ADDR_NUM_ENTRIES;
    uint32_t            maxAddrIndex = 0;
    uint16_t            addr_offset = 0;
    uint32_t            val32 = 0;

    if (desc == NULL)
    {
        return -EINVAL;
    }

    if (desc->state == ETH_MAC_STATE_UNINITIALIZED)
    {
        return -ENODEV;
    }

    maxAddrIndex = ETH_MAC_ADDR_NUM_ENTRIES;
    if (mac_addr_mask != NULL)
    {
        maxAddrIndex = ETH_MAC_ADDR_MASK_MAX;
    }

    for (uint32_t filterIdx = 0; filterIdx < maxAddrIndex; filterIdx++)
    {
        if (!(desc->addr_filter_active & (1 << filterIdx)))
        {
            addr_index = filterIdx;
            break;
        }
    }

    if (addr_index == maxAddrIndex)
    {
        return -ERANGE;
    }

    addr_offset = addr_index * (ADDR_MAC_ADDR_FILT_UPR_1_ - ADDR_MAC_ADDR_FILT_UPR_0_);

    /* Write MAC address and address rules.                                 */
    /* Note MAC_ADDR_FILT_UPR needs to be written before MAC_ADDR_FILT_LWR. */
    /* Pack: [31:16]=rule, [15:8]=mac[0], [7:0]=mac[1]                      */
    val32 = (addr_rule << BYTE2_SHIFT_BITS) | (mac_addr[0] << BYTE1_SHIFT_BITS) | mac_addr[1];
    result = eth_mac_write_register(desc, addr_offset + ADDR_MAC_ADDR_FILT_UPR_0_, 
                               val32);
    if(result != 0)
    {
        return result;
    }

    val32 = ((mac_addr[2] << BYTE3_SHIFT_BITS) | (mac_addr[3] << BYTE2_SHIFT_BITS) | 
             (mac_addr[4] << BYTE1_SHIFT_BITS) | mac_addr[5]);
    result = eth_mac_write_register(desc, addr_offset + ADDR_MAC_ADDR_FILT_LWR_0_, 
                               val32);
    if(result != 0)
    {
        return result;
    }

    /* Write MAC address mask. Note MAC_ADDR_MSK_UPR               */
    /* needs to be written before MAC_ADDR_MSK_LWR.                */
    /* Allow special case of NULL translating to a mask of all 1s. */
    if (mac_addr_mask == NULL)
    {
        val32 = 0xFFFF;
    }
    else
    {
        val32 = (mac_addr_mask[0] << BYTE1_SHIFT_BITS) | mac_addr_mask[1];
    }
    result = eth_mac_write_register(desc, addr_offset + ADDR_MAC_ADDR_MSK_UPR_0_, 
                               val32);
    if(result != 0)
    {
        return result;
    }

    if (mac_addr_mask == NULL)
    {
        val32 = 0xFFFFFFFF;
    }
    else
    {
        val32 = ((mac_addr_mask[2] << BYTE3_SHIFT_BITS) | 
                 (mac_addr_mask[3] << BYTE2_SHIFT_BITS) | 
                 (mac_addr_mask[4] << BYTE1_SHIFT_BITS) | mac_addr_mask[5]);
    }
    result = eth_mac_write_register(desc, addr_offset + ADDR_MAC_ADDR_MSK_LWR_0_,
                               val32);
    if(result != 0)
    {
        return result;
    }

    /* Mark the address filter entry as used. */
    desc->addr_filter_active |= (1 << addr_index);

    return result;
}


int eth_mac_clr_address_filter(struct eth_mac_desc *desc, 
    uint32_t addr_index)
{
    int        result = 0;
    uint16_t                addr_offset;

    if (desc == NULL)
    {
        return -EINVAL;
    }

    if (desc->state == ETH_MAC_STATE_UNINITIALIZED)
    {
        return -ENODEV;
    }

    if (addr_index >= ETH_MAC_ADDR_NUM_ENTRIES)
    {
        return -ERANGE;
    }

    addr_offset = addr_index * (ADDR_MAC_ADDR_FILT_UPR_1_ - ADDR_MAC_ADDR_FILT_UPR_0_);

    result = eth_mac_write_register(desc, addr_offset + ADDR_MAC_ADDR_FILT_UPR_0_, 0);
    if(result != 0)
    {
        return result;
    }

   result = eth_mac_write_register(desc, addr_offset + ADDR_MAC_ADDR_FILT_LWR_0_, 0);
   if(result != 0)
   {
        return result;
   }

   desc->addr_filter_active &= ~(1 << addr_index);
   return result;
}

int eth_mac_get_link_status(struct eth_mac_desc *desc, uint32_t port, enum eth_mac_link_status *link_status)
{
    int    result = 0;

#if defined(ADIN2111)
    /* For the ADIN2111 we need to read link status from the PHY registers */
    result = -ENOSYS;
#else
    uint32_t            val32;

    (void)port;

    result = eth_mac_read_register(desc, ADDR_MAC_STATUS1, &val32);
    if(result != 0)
    {
        goto end;
    }

    *link_status = (enum eth_mac_link_status)((val32 & BITM_MAC_STATUS1_P1_LINK_status) >> BITP_MAC_status1_P1_LINK_status);

end:
#endif
    return result;
}

int eth_mac_set_chunk_size(struct eth_mac_desc *desc, enum eth_mac_oa_cps cps)
{
    int    result = 0;
    uint16_t            addr = ADDR_MAC_CONFIG0;
    uint32_t            mask = BITM_MAC_CONFIG0_CPS;
    uint32_t            val = 0;

    if (desc == NULL)
    {
        return -EINVAL;
    }

    if (desc->config_sync)
    {
        return -EL2NSYNC;
    }
    
    if ((cps < ETH_MAC_OA_CPS_8BYTE) ||
        (cps > ETH_MAC_OA_CPS_64BYTE))
    {
        return -EINVAL;
    }    

    result = eth_mac_read_register(desc, addr, &val);
    if (result != 0)
    {
        return result;
    }

    val = (val & ~mask) | (cps & mask);

    result = eth_mac_write_register(desc, addr, val);
    if (result != 0)
    {
        return result;
    }

    desc->oa_max_chunk_count = (cps == ETH_MAC_OA_CPS_64BYTE) ? 
                                ADI_OA_MAX_CHUNK64_COUNT : 
                                ADI_OA_MAX_CHUNK_COUNT;

    desc->oa_cps = (uint32_t)cps;

    return result;
}

int eth_mac_get_chunk_size(struct eth_mac_desc *desc, enum eth_mac_oa_cps *pCps)
{
    int    result = 0;
    uint16_t            addr = ADDR_MAC_CONFIG0;
    uint32_t            mask = BITM_MAC_CONFIG0_CPS;
    uint32_t            val = 0;

    if (desc == NULL)
    {
        return -EINVAL;
    }

    if (pCps == NULL)
    {
        return -EINVAL;
    }

    if ((*pCps < ETH_MAC_OA_CPS_8BYTE) ||
        (*pCps > ETH_MAC_OA_CPS_64BYTE))
    {
        return -EINVAL;
    }    

    result = eth_mac_read_register(desc, addr, &val);
    if (result != 0)
    {
        return result;
    }

    val &= mask;
    *pCps = (enum eth_mac_oa_cps)val;


    if (val != desc->oa_cps)
    {
        return -EBADMSG;

    }

    return result;
}

int eth_mac_set_cut_through_mode(struct eth_mac_desc *desc, bool tx_cte, bool rx_cte)
{
    int    result = 0;
    uint16_t            addr = ADDR_MAC_CONFIG0;
    uint32_t            setMask = (rx_cte << BITP_MAC_CONFIG0_RXCTE) | 
                                  (tx_cte << BITP_MAC_CONFIG0_TXCTE);
    uint32_t            clearMask = (BITM_MAC_CONFIG0_RXCTE | 
                                     BITP_MAC_CONFIG0_TXCTE);
    uint32_t            val;
    
    desc->tx_cte = tx_cte;
    
    if (desc != NULL)
    {
        if ((!desc->is_oa_spi_enabled) && rx_cte)
        {
            /* Cut-through on receive is not supported in hardware for Generic SPI */
            return -ENOSYS;
        }

        if (desc->config_sync)
        {
            return -EL2NSYNC;
        }
    }

    result = eth_mac_read_register(desc, addr, &val);
    if (result != 0)
    {
        return result;
    }

    val = (val & ~clearMask) | setMask;

    result = eth_mac_write_register(desc, addr, val);

    return result;
}

int eth_mac_get_cut_through_mode(struct eth_mac_desc *desc, 
    bool *tx_cte, bool *rx_cte)
{
    int    result = 0;
    uint16_t            addr    = ADDR_MAC_CONFIG0;
    uint32_t            rxMask  = BITM_MAC_CONFIG0_RXCTE;
    uint32_t            txMask  = BITM_MAC_CONFIG0_TXCTE;
    uint32_t            val;

    if (desc == NULL)
    {
        return -EINVAL;
    }

    if ((tx_cte == NULL) || (rx_cte == NULL))
    {
        return -EINVAL;
    }

    result = eth_mac_read_register(desc, addr, &val);
    if (result != 0)
    {
        return result;
    }

    *rx_cte = (bool)(val & rxMask);
    *tx_cte = (bool)(val & txMask);

    return result;
}

int eth_mac_set_fifo_sizes(struct eth_mac_desc *desc,
							struct eth_mac_fifo_size write_val)
{
    int    result  = 0;

    if ((desc != NULL) && (desc->config_sync))
    {
        return -EL2NSYNC;
    }

    result = eth_mac_write_register(desc, ADDR_MAC_FIFO_SIZE, write_val);

    return result;
}

int eth_mac_get_fifo_sizes(struct eth_mac_desc *desc,
							struct eth_mac_fifo_size *read_val)
{
    return eth_mac_read_register(desc, ADDR_MAC_FIFO_SIZE, read_val);
}

int eth_mac_clear_fifo(struct eth_mac_desc *desc, 
    enum eth_mac_fifo_clr_mode clear_mode)
{
    int        result  = 0;
    uint16_t                addr    = ADDR_MAC_FIFO_CLR;

    result = eth_mac_write_register(desc, addr, (uint32_t)clear_mode);
    if (result != 0)
    {
        return result;
    }

    return result;
}

int eth_mac_set_promiscuous_mode(struct eth_mac_desc *desc, bool bit_flag)
{
    int    result = 0;
    uint16_t            addr = ADDR_MAC_CONFIG2;
    uint32_t            mask = BITM_MAC_CONFIG2_P1_FWD_UNK2HOST;;
    uint32_t            val;

    result = eth_mac_read_register(desc, addr, &val);
    if (result != 0)
    {
        return result;
    }

    if (bit_flag)
    {
        val |= mask;
    }
    else
    {
        val &= ~mask;
    }

    result = eth_mac_write_register(desc, addr, val);
    if (result != 0)
    {
        return result;
    }

    return result;
}

int eth_mac_get_promiscuous_mode(struct eth_mac_desc *desc, bool *bit_flag)
{
    int    result = 0;
    uint16_t            addr = ADDR_MAC_CONFIG2;
    uint32_t            mask = BITM_MAC_CONFIG2_P1_FWD_UNK2HOST;
    uint32_t            val;

    if (desc == NULL)
    {
        return -EINVAL;
    }

    if (bit_flag == NULL)
    {
        return -EINVAL;
    }

    result = eth_mac_read_register(desc, addr, &val);
    if (result != 0)
    {
        return result;
    }

    *bit_flag = (val & mask) ? true: false;

    return result;
}


int eth_mac_ts_enable(struct eth_mac_desc *desc, enum eth_mac_ts_format format)
{
    int     result = 0;
    uint32_t             tsCfg;
    uint32_t             config0;

    if ((desc != NULL) && (desc->config_sync))
    {
        return -EL2NSYNC;
    }

    result = eth_mac_read_register(desc, ADDR_MAC_TS_CFG, &tsCfg);
    if (result != 0)
    {
        return result;
    }

    result = eth_mac_read_register(desc, ADDR_MAC_CONFIG0, &config0);
    if (result != 0)
    {
        return result;
    }

    config0 |= BITM_MAC_CONFIG0_FTSE;
    switch(format)
    {
        case ETH_MAC_TS_FORMAT_NONE:
            tsCfg &= ~BITM_MAC_TS_CFG_TS_CAPT_FREE_CNT;
            config0 &= ~BITM_MAC_CONFIG0_FTSE;
            break;
        case ETH_MAC_TS_FORMAT_32B_FREE:
            tsCfg |= BITM_MAC_TS_CFG_TS_CAPT_FREE_CNT;
            config0 &= ~BITM_MAC_CONFIG0_FTSS;
            break;
        case ETH_MAC_TS_FORMAT_32B_1588:
            tsCfg &= ~BITM_MAC_TS_CFG_TS_CAPT_FREE_CNT;
            config0 &= ~BITM_MAC_CONFIG0_FTSS;
            break;
        case ETH_MAC_TS_FORMAT_64B_1588:
            tsCfg &= ~BITM_MAC_TS_CFG_TS_CAPT_FREE_CNT;
            config0 |= BITM_MAC_CONFIG0_FTSS;
            break;
        default:
            return -EINVAL;
    }

    result = eth_mac_write_register(desc, ADDR_MAC_CONFIG0, config0);
    if (result != 0)
    {
        return result;
    }

    tsCfg |= BITM_MAC_TS_CFG_TS_EN;
    result = eth_mac_write_register(desc, ADDR_MAC_TS_CFG, tsCfg);
    if (result != 0)
    {
        return result;
    }

    desc->timestamp_format = format;

    return result;
}


int eth_mac_ts_clear(struct eth_mac_desc *desc)
{
    int     result = 0;
    uint32_t             tsCfg;

    result = eth_mac_read_register(desc, ADDR_MAC_TS_CFG, &tsCfg);
    if (result != 0)
    {
        return result;
    }

    tsCfg |= BITM_MAC_TS_CFG_TS_CLR;
    result = eth_mac_write_register(desc, ADDR_MAC_TS_CFG, tsCfg);

    return result;
}

int eth_mac_ts_timer_start(struct eth_mac_desc *desc, 
    struct eth_mac_ts_timer_cfg *timer_config)
{
    int     result = 0;
    uint32_t             remainder16 = 0;
    uint32_t             timeHi = 0;
    uint32_t             timeLo = 0;
    uint32_t             tsCfg = 0;

    if (desc == NULL)
    {
        return -EINVAL;
    }

    if (timer_config == NULL)
    {
        return -EINVAL;
    }

    /* TS_TIMER register values must be greater than
       or equal to ETH_MAC_TS_MIN_NS */
    if (timer_config->startTimeNs < ETH_MAC_TS_MIN_NS)
    {
        timer_config->startTimeNs = ADDR_MAC_TS_NS_CNT;
    }
    if (timer_config->periodNs < (2 * ETH_MAC_TS_MIN_NS))
    {
        timer_config->periodNs = 2 * ETH_MAC_TS_MIN_NS;
    }

    result = eth_mac_read_register(desc, ADDR_MAC_TS_CFG, &tsCfg);
    if (result != 0)
    {
        return result;
    }

    if (!(tsCfg & BITM_MAC_TS_CFG_TS_EN))
    {
        return -ENODATA;
    }

    tsCfg = ((tsCfg & ~BITM_MAC_TS_CFG_TS_TIMER_DEF) | 
            ((uint32_t)timer_config->idleState << BITP_MAC_TS_CFG_TS_TIMER_DEF));
    result = eth_mac_write_register(desc, ADDR_MAC_TS_CFG, tsCfg);
    if (result != 0)
    {
        return result;
    }

    /* Need to use quantization error correction if the period 
       is not a multiple of ETH_MAC_TS_MIN_NS */
    remainder16 = timer_config->periodNs & ETH_MAC_TS_QE_MASK;
    if (remainder16)
    {
        result = eth_mac_write_register(desc, ADDR_MAC_TS_TIMER_QE_CORR, 
                                   remainder16);
        if (result != 0)
        {
            return result;
        }
    }

    timeHi = (uint32_t)((timer_config->periodNs & ~ETH_MAC_TS_QE_MASK) * 
                         timer_config->dutyCycle);
    timeLo = (timer_config->periodNs & ~ETH_MAC_TS_QE_MASK) - timeHi;
    result = eth_mac_write_register(desc, ADDR_MAC_TS_TIMER_HI, 
                               timeHi & ~ETH_MAC_TS_QE_MASK);
    if (result != 0)
    {
        return result;
    }
    result = eth_mac_write_register(desc, ADDR_MAC_TS_TIMER_LO,
                               timeLo & ~ETH_MAC_TS_QE_MASK);
    if (result != 0)
    {
        return result;
    }

    result = eth_mac_write_register(desc, ADDR_MAC_TS_TIMER_START, 
                               timer_config->startTimeNs);

    return result;
}

int eth_mac_ts_timer_stop(struct eth_mac_desc *desc)
{
    int    result = 0;
    uint32_t            val32;

    result = eth_mac_read_register(desc, ADDR_MAC_TS_CFG, &val32);
    if (result != 0)
    {
        return result;
    }

    val32 |= BITM_MAC_TS_CFG_TS_TIMER_STOP;
    result = eth_mac_write_register(desc, ADDR_MAC_TS_CFG, val32);

    return result;
}

int eth_mac_ts_convert(uint32_t timestamp_low_word,
    uint32_t timestamp_high_word, enum eth_mac_ts_format format, 
    struct eth_mac_ts_timespec *timespec)
{
    int result = 0;
    /* Floating point nanoseconds value for free-running counter conversion.  */ 
    /* Double to reduce rounding errors.  */
    double nsecF;

    if (timespec == NULL)
    {
        return -EINVAL;
    }

    switch(format)
    {
        case ETH_MAC_TS_FORMAT_32B_FREE:
            /* Convert LSB to nanoseconds. */
            nsecF = timestamp_low_word * ETH_MAC_TS_FREE_CNT_LSB_IN_NS;
            /* Extract number of whole seconds. */
            timespec->sec = (uint32_t)(nsecF / ETH_MAC_TS_ONE_SECOND_IN_NS);
            /* Subtract off number of whole seconds to get number of */ 
            /* whole nanoseconds. Fractional value is lost. */
            timespec->nsec = (uint32_t)(nsecF - (timespec->sec * 
                                                 ETH_MAC_TS_ONE_SECOND_IN_NS));
            break;
        case ETH_MAC_TS_FORMAT_32B_1588:
            /* Mask and shift to get number of seconds (2 bits). */
            timespec->sec = ((timestamp_low_word & ETH_MAC_TS_1588_SEC_MASK) >>
                              ETH_MAC_TS_1588_SEC_POS);
            /* Mask to get number of nanoseconds (30 bits). */
            timespec->nsec = timestamp_low_word & ETH_MAC_TS_1588_NS_MASK;
            break;
        case ETH_MAC_TS_FORMAT_64B_1588:
            /* All 32b of upper words are whole seconds. */
            timespec->sec = timestamp_high_word;
            /* Mask to get number of nanoseconds (30 bits). */
            timespec->nsec = timestamp_low_word & ETH_MAC_TS_1588_NS_MASK;
            break;
        default:
            result = -ENOEXEC;
            break;
    }

    return result;
}


int eth_mac_ts_get_ext_capt_timestamp(struct eth_mac_desc *desc, 
    struct eth_mac_ts_timespec *captured_timespec)
{
    int result = 0;
    uint32_t timestamp_low_word = 0;
    uint32_t timestamp_high_word = 0;

    if (desc == NULL)
    {
        return -EINVAL;
    }

    if (captured_timespec == NULL)
    {
        return -EINVAL;
    }

    /* 64b timestamp and free-running counter are both always captured. */
    /* Return only one depending on the configured format.              */
    if ((desc->timestamp_format == ETH_MAC_TS_FORMAT_32B_1588) || 
        (desc->timestamp_format == ETH_MAC_TS_FORMAT_64B_1588))
    {
        result = eth_mac_read_register(desc, ADDR_MAC_TS_EXT_CAPT0, 
                                 &timestamp_low_word);
        if (result != 0)
        {
            return result;
        }
        result = eth_mac_read_register(desc, ADDR_MAC_TS_EXT_CAPT1, 
                                 &timestamp_high_word);
        if (result != 0)
        {
            return result;
        }
        result = eth_mac_ts_convert(timestamp_low_word, timestamp_high_word, 
                               ETH_MAC_TS_FORMAT_64B_1588, captured_timespec);
    }
    else if (desc->timestamp_format == ETH_MAC_TS_FORMAT_32B_FREE)
    {
        timestamp_high_word = 0;
        result = eth_mac_read_register(desc, ADDR_MAC_TS_FREECNT_CAPT, 
                                  &timestamp_low_word);
        if (result != 0)
        {
            return result;
        }
        result = eth_mac_ts_convert(timestamp_low_word, timestamp_high_word, 
                               desc->timestamp_format, captured_timespec);
    }
    else
    {
       return -ENOEXEC;
    }

    return result;
}

int eth_mac_ts_get_egress_timestamp(struct eth_mac_desc *desc, 
    enum eth_mac_egress_capture egress_reg, struct eth_mac_ts_timespec *captured_timespec)
{
    int result = 0;
    uint32_t timestamp_low_word = 0;
    uint32_t timestamp_high_word = 0;
    uint32_t reg_addrL = 0;
    uint32_t reg_addrH = 0;

    if (desc == NULL)
    {
        return -EINVAL;
    }

    if (captured_timespec == NULL)
    {
        return -EINVAL;
    }

    switch(egress_reg)
    {
        case ETH_MAC_EGRESS_CAPTURE_A:
            reg_addrL = ADDR_MAC_TTSCAL;
            reg_addrH = ADDR_MAC_TTSCAH;
            break;
        case ETH_MAC_EGRESS_CAPTURE_B:
            reg_addrL = ADDR_MAC_TTSCBL;
            reg_addrH = ADDR_MAC_TTSCBH;
            break;
        case ETH_MAC_EGRESS_CAPTURE_C:
            reg_addrL = ADDR_MAC_TTSCCL;
            reg_addrH = ADDR_MAC_TTSCCH;
            break;
        default:
            result = -ENOEXEC;
            break;
    }

    if (result == 0)
    {
        result = eth_mac_read_register(desc, reg_addrL, &timestamp_low_word);
        if (result != 0)
        {
            return result;
        }
        result = eth_mac_read_register(desc, reg_addrH, &timestamp_high_word);
        if (result != 0)
        {
            return result;
        }
        result = eth_mac_ts_convert(timestamp_low_word, timestamp_high_word, 
                               desc->timestamp_format, captured_timespec);
    }

    return result;
}


int eth_mac_ts_set_timer_absolute(struct eth_mac_desc *desc, 
    uint32_t seconds, uint32_t nanoseconds)
{
    int result = 0;
    uint32_t addend = 0;
    
    if (desc == NULL)
    {
        return -EINVAL;
    }
    
    /* Stop clock while updating. */
    result = eth_mac_read_register(desc, ADDR_MAC_TS_ADDEND, &addend);
    if (result != 0)
    {
        return result;
    }
    result = eth_mac_write_register(desc, ADDR_MAC_TS_ADDEND, 0);
    if (result != 0)
    {
        return result;
    }

    result = eth_mac_write_register(desc, ADDR_MAC_TS_SEC_CNT, seconds);
    if (result != 0)
    {
        return result;
    }

    /* Nanoseconds value must be greater than ETH_MAC_TS_MIN_NS */
    /* and divisible by ETH_MAC_TS_MIN_NS.                      */
    if (nanoseconds < ETH_MAC_TS_MIN_NS)
    {
        nanoseconds = ETH_MAC_TS_MIN_NS;
    }
    /* Nanoseconds value must be less than the number */ 
    /* of nanoseconds in a second.                    */
    if (nanoseconds >= ETH_MAC_TS_ONE_SECOND_IN_NS)
    {
        nanoseconds = ETH_MAC_TS_ONE_SECOND_IN_NS - 1;
    }
    result = eth_mac_write_register(desc, ADDR_MAC_TS_NS_CNT, 
                               nanoseconds & ~ETH_MAC_TS_QE_MASK);
    if (result != 0)
    {
        return result;
    }

    result = eth_mac_write_register(desc, ADDR_MAC_TS_ADDEND, addend);

    return result;
}

int eth_mac_ts_sync_clock(struct eth_mac_desc *desc, 
    int64_t time_error, uint64_t refer_time_ns_diff, uint64_t local_time_ns_diff)
{
    int result = 0;
    int64_t referenceNsDiff = 0;
    int64_t localNsDiff = 0;
    int64_t val64 = 0;
    uint32_t addend = 0;

    if (desc == NULL)
    {
        return -EINVAL;
    }

    /* Adjust to prevent overflow in computation */
    referenceNsDiff = refer_time_ns_diff;
    localNsDiff = local_time_ns_diff;
    while (referenceNsDiff > 0x7FFFFFFF)
    {
        referenceNsDiff >>= 1;
        localNsDiff >>= 1;
    }

    /* Compute syntonization factor. */
    result = eth_mac_read_register(desc, ADDR_MAC_TS_ADDEND, &addend);
    if (result != 0)
    {
        return result;
    }
    
    val64 = (int64_t) addend;
    if ((referenceNsDiff != 0) && (localNsDiff != 0)) {
        val64 = ((referenceNsDiff * val64) / localNsDiff);
    }

    /* Apply correction. */
    val64 += time_error;

    /* Coerce to register limits and write. */
    addend = (uint32_t)val64;
    result = eth_mac_write_register(desc, ADDR_MAC_TS_ADDEND, addend);

    return result;
}


int64_t eth_mac_ts_subtract(struct eth_mac_ts_timespec *ts_a, struct eth_mac_ts_timespec *ts_b)
{
    int64_t tsAns = 0;
    int64_t tsBns = 0;

    if ((ts_a == NULL) || (ts_b == NULL))
    {
        return -EINVAL;
    }

    /* Convert s to ns and add in ns */
    tsAns = ts_a->sec;
    tsAns *= ETH_MAC_TS_ONE_SECOND_IN_NS;
    tsAns += ts_a->nsec;

    /* Convert s to ns and add in ns */
    tsBns = ts_b->sec;
    tsBns *= ETH_MAC_TS_ONE_SECOND_IN_NS;
    tsBns += ts_b->nsec;

    /* Return the difference in ns */
    return tsAns - tsBns;
}

int eth_mac_sync_config(struct eth_mac_desc *desc)
{
    int    result = 0;
    uint32_t            val32 = 0;

    if (desc == NULL)
    {
        return -EINVAL;
    }

    if (desc->config_sync)
    {
        return -EL2NSYNC;
    }
   
    result = eth_mac_read_register(desc, ADDR_MAC_CONFIG0, &val32);
    if (result != 0)
    {
        return result;
    }

    val32 |= BITM_MAC_CONFIG0_SYNC;

    result = eth_mac_write_register(desc, ADDR_MAC_CONFIG0, val32);
    if (result != 0)
    {
        return result;
    }

    /* CONFIG0.SYNC is set, we can now enable the IRQ. */
    capi_irq_enable(desc->eth_irq);

    desc->config_sync = true;

    return result;
}

/*
 * @brief           Calculates the parity over a number of bytes.
 *
 * @param [in]      p           Pointer to the input data.
 * @param [in]      nBytes      Size of the input data, in bytes.
 *
 * @return          Returns 1 if input data has odd parity. Otherwise 0.
 *
 * @details         Calculates the odd parity of the input data. Used for the
 *                  header/footer parity fields defined by the OPEN Alliance specification
 *                  and for timestamp parity checking in both SPI protocols.
 *
 */
uint8_t eth_mac_CalculateParity(uint8_t *p, uint32_t nBytes)
{
    uint32_t i;

    if (p == NULL)
    {
        return -EINVAL;
    }

    uint8_t parity = 0;
    for (i = 0; i < nBytes; i++)
    {
        parity ^= p[i];
    }

    uint8_t prt = 0;
    for (uint32_t i = 0; i < 8; i++)
    {
        prt = prt ^ (parity & 0x1);
        parity >>= 1;
    }

    return prt & 0x1;
}

int eth_mac_set_forward_mode(struct eth_mac_desc *desc, struct eth_mac_forward_mode mode, uint32_t mask)
{
    int    result = 0;
    uint16_t            addr = ADDR_MAC_CONFIG2;
    uint32_t            val;
    uint32_t            setVal;

    if (desc == NULL)
    {
        return -EINVAL;
    }

    result = eth_mac_read_register(desc, addr, &val);
    if (result != 0)
    {
        return result;
    }

    val &= ~mask;
    setVal =    ((uint32_t)mode.P1_FWD_UNK_TO_HOST << BITP_MAC_CONFIG2_P1_FWD_UNK2HOST);
    if (desc->device_type == ADIN_2111){
        
        setVal |=   ((uint32_t)mode.P2_FWD_UNK_TO_HOST << BITP_MAC_CONFIG2_P2_FWD_UNK2HOST) |
                    ((uint32_t)mode.P1_FWD_UNK_TO_P2 << BITP_MAC_CONFIG2_P1_FWD_UNK2P2) |
                    ((uint32_t)mode.P2_FWD_UNK_TO_P1 << BITP_MAC_CONFIG2_P2_FWD_UNK2P1);
    } else {
        setVal = val | (setVal & mask);
    }
    result = eth_mac_write_register(desc, addr, setVal);
    return result;
}

int eth_mac_get_forward_mode(struct eth_mac_desc *desc, struct eth_mac_forward_mode *mode)
{
    int    result = 0;
    uint16_t            addr = ADDR_MAC_CONFIG2;
    uint32_t            val;

    if (desc == NULL)
    {
        return -EINVAL;
    }

    result = eth_mac_read_register(desc, addr, &val);
    if (result != 0)
    {
        return result;
    }

    if (mode == NULL)
    {
        return -EINVAL;
    }

    mode->P1_FWD_UNK_TO_HOST = (val & BITM_MAC_CONFIG2_P1_FWD_UNK2HOST);
    
    if (desc->device_type == ADIN_2111){
        mode->P2_FWD_UNK_TO_HOST = (val & BITM_MAC_CONFIG2_P2_FWD_UNK2HOST);
        mode->P1_FWD_UNK_TO_P2   = (val & BITM_MAC_CONFIG2_P1_FWD_UNK2P2);
        mode->P2_FWD_UNK_TO_P1   = (val & BITM_MAC_CONFIG2_P2_FWD_UNK2P1);
    }

    return result;
}

int eth_mac_init(struct eth_mac_desc **desc, struct eth_mac_init_param *param)
{
    struct eth_mac_desc    *descriptor;

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
	if (param->mem_size < sizeof(struct eth_mac_desc))
	{
		return -EINVAL;
	}

	memset(param->dev_mem, 0, param->mem_size);
	descriptor = (struct eth_mac_desc *)param->dev_mem;
    }
    
    /* Disable IRQ for situations when this is called after device initialization */
    capi_irq_disable(param->eth_irq);
    
    /* Initialize only the specific fields that need to be reset to default values */
    descriptor->state = ETH_MAC_STATE_UNINITIALIZED;
    descriptor->device = param->app_main;

    if (param->app_user != NULL)
    {
        descriptor->app_dev = param->app_user;
    }
    else
    {
        descriptor->app_dev = param->app_main;
    }

    /* Initialize callback function arrays */
    for (uint32_t event_index = 0; event_index < ETH_MAC_EVT_MAX; event_index++)
    {
        descriptor->cb_func[event_index] = NULL;
        descriptor->cb_param[event_index] = NULL;
    }

    /* Initialize control and configuration fields */
    descriptor->pending_ctrl = false;
    descriptor->addr_filter_active = 0;
    descriptor->config_sync = false;
    descriptor->fcs_check_en = param->fcs_check_en;
    descriptor->device_type = param->device_type;
    descriptor->is_oa_spi_enabled = param->is_oa_spi_enabled;
    descriptor->is_rx_queue_hp_enabled = param->is_rx_queue_hp_enabled;
    descriptor->timestamp_format = )param->timestamp_format;
    descriptor->tx_cte = false;
    
    /* Initialize status registers */
    descriptor->status_registers.status0_mask = 0;
    descriptor->status_registers.status0 = 0;
    descriptor->status_registers.status1_mask = 0;
    descriptor->status_registers.status1 = 0;
    descriptor->status_registers.p1_status_mask = ETH_MAC_PHY_STATUS_INIT_VAL;
    descriptor->status_registers.p1_status = ETH_MAC_PHY_STATUS_INIT_VAL;

    if (descriptor->device_type == ADIN_2111) {
        descriptor->status_registers.p2_status_mask = ETH_MAC_PHY_STATUS_INIT_VAL;
        descriptor->status_registers.p2_status = ETH_MAC_PHY_STATUS_INIT_VAL;
    }

    /* Initialize PHY-related fields */
    descriptor->phy_addr = 0;
    descriptor->phy_irq_mask = 0;

    if(descriptor->is_oa_spi_enabled){
        descriptor->oa_cps = ADI_OA_DEFAULT_CPS;
        descriptor->oa_max_chunk_count = ADI_OA_MAX_CHUNK64_COUNT;
    }
    else {
        descriptor->oa_cps = 0;
        descriptor->oa_max_chunk_count = 0;
    }

    /* 
     * Initialize queue structures and frame arrays
     * Note: These need to be zeroed since eth_mac_init_queue expects clean structures
     */
    memset(&descriptor->tx_queue, 0, sizeof(descriptor->tx_queue));
    memset(&descriptor->rx_queue_lp, 0, sizeof(descriptor->rx_queue_lp));
    memset(&descriptor->rx_queue_hp, 0, sizeof(descriptor->rx_queue_hp));
    memset(&descriptor->tx_queue_frames, 0, sizeof(descriptor->tx_queue_frames));
    memset(&descriptor->rx_queue_lp_frames, 0, sizeof(descriptor->rx_queue_lp_frames));
    memset(&descriptor->rx_queue_hp_frames, 0, sizeof(descriptor->rx_queue_hp_frames));

    /* Initialize both queues */
    eth_mac_init_queue(&descriptor->tx_queue, &descriptor->tx_queue_frames[0], 
              TX_QUEUE_NUM_ENTRIES_RAW);
    eth_mac_init_queue(&descriptor->rx_queue_lp, &descriptor->rx_queue_lp_frames[0], 
              RX_QUEUE_NUM_ENTRIES_RAW);
    
    if(descriptor->is_rx_queue_hp_enabled) {
        eth_mac_init_queue(&descriptor->rx_queue_hp, &descriptor->rx_queue_hp_frames[0], 
                  RX_QUEUE_NUM_ENTRIES_RAW);
    }
    
    /* Default Rx queue is always the low priority */
    descriptor->rx_queue = &descriptor->rx_queue_lp;
    
    /* Set function pointers from configuration */
    descriptor->mac_read = param->mac_read;
    descriptor->mac_write = param->mac_write; 
    descriptor->mac_get_comm_status = param->mac_get_comm_status;
    descriptor->mac_write_frame = param->mac_write_frame;
    descriptor->mac_request_irq_handler = param->mac_request_irq_handler;
    descriptor->mac_wait_until_ready = param->mac_wait_until_ready;

    *desc = descriptor;

    return 0;
}

/*
 * @brief           MAC device uninitialization.
 *
 * @param [in]      desc     Device driver handle.
 *
 * @return 0 in case of success, positive error code otherwise.
 *
 */
int eth_mac_remove(struct eth_mac_desc *desc)
{

    if (desc == NULL)
    {
        return -EINVAL;
    }

    capi_irq_disable(desc->eth_irq);
    desc->state = ETH_MAC_STATE_UNINITIALIZED;

    if (desc->dev_mem == NULL)
    {
	no_os_free(desc);
    }
    else
    {
	memset(desc, 0, sizeof(struct eth_mac_desc));
    }

    return 0;
}

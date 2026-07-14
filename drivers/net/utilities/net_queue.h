/*******************************************************************************
*   @file   net_queue.h
*   @brief  Implementation of queue module for adin1110 driver.
*   @author Christine Joy Murillo (Christinejoy.Murillo@analog.com)
********************************************************************************
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
*******************************************************************************/

/** @addtogroup queue QUEUE Definitions
 *  @{
 */

#ifndef NET_QUEUE_H
#define NET_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/*!
 * @brief Queue to hold frames.
 */
struct net_queue
{	
	/*!< Pointer to the entries in the queue */
    void *pEntries;  
	
	/*!< Number of entries that can be held in the queue */  
    uint32_t numEntries; 
	
	/*!< Queue head index */
    volatile uint32_t head;  
	
	/*!< Queue tail index */
    volatile uint32_t tail;       
};


/*!
 * @brief Queue statistics counters.
 */
struct net_queue_stats_counter
{	
	/*!< Number of successful add operations */
    uint32_t enqueueSuccessCount;
	
	/*!< Number of successful remove operations */
    uint32_t dequeueSuccessCount;
	
	/*!< Number of failed add operations */	
    uint32_t enqueueFailCount;
	
	/*!< Number of failed remove operations */    
    uint32_t dequeueFailCount;   
	
	/*!< Total number of add attempts */	
    uint32_t totalEnqueueAttempts; 
};

/* Initializes the queue data structure */
int net_queue_init (struct net_queue *const net_queue_p, void *const pEntries,
	uint32_t numEntries);

/* Gets the current number of frames in the queue */
uint32_t net_queue_get_count (struct net_queue *const net_queue_p);

/* Checks if the queue is full */
bool net_queue_is_full (struct net_queue *const net_queue_p);

/* Checks if the queue is empty */
bool net_queue_is_empty (struct net_queue *const net_queue_p);

/* Gets the number of available entries in the queue */
uint32_t net_queue_get_avail_entries (struct net_queue *const net_queue_p);

/* Adds a frame to the queue by advancing the head pointer */
void net_queue_add_entry (struct net_queue *const net_queue_p);

/* Removes a frame from the queue by advancing the tail pointer */
void net_queue_remove_entry (struct net_queue *const net_queue_p);

/* Gets the tail index of the queue without removing the entry */
uint32_t net_queue_peek (struct net_queue *const net_queue_p);


/*! @endcond */

#ifdef __cplusplus
}
#endif

#endif /* net_queue_H */

/*******************************************************************************
*   @file   net_queue.c
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

#include "net_queue.h"

#define PSEUDO_MODULO(N, D) (((N) < (D)) ? (N) : ((N) - (D)))

static inline uint32_t queue_count(struct net_queue *const queue_p);
static uint32_t queue_available(struct net_queue *const queue_p);
static bool queue_full(struct net_queue *const queue_p);
static bool queue_empty(struct net_queue *const queue_p);
static void queue_add(struct net_queue *const queue_p);
static void queue_remove(struct net_queue *const queue_p);
static uint32_t queue_peek_entry(struct net_queue *const queue_p);


static inline uint32_t queue_count(struct net_queue *const queue_p)
{   
	uint32_t head = queue_p->head;
	uint32_t tail = queue_p->tail;
	uint32_t n = head + queue_p->numEntries - tail;

	return PSEUDO_MODULO(n, queue_p->numEntries);
}

static uint32_t queue_available(struct net_queue *const queue_p)
{  
	return (queue_p->numEntries - 1) - queue_count(queue_p);
}

static bool queue_full(struct net_queue *const queue_p)
{   
	return (queue_p->numEntries - 1) == queue_count(queue_p);
}

static bool queue_empty(struct net_queue *const queue_p)
{
	uint32_t head = queue_p->head;
	uint32_t tail = queue_p->tail;    
	return head == tail;
}

static void queue_add(struct net_queue *const queue_p)
{       
	uint32_t n = queue_p->head + 1;
	queue_p->head = PSEUDO_MODULO(n, queue_p->numEntries);
}

static void queue_remove(struct net_queue *const queue_p)
{
	uint32_t n = queue_p->tail + 1;
	queue_p->tail = PSEUDO_MODULO(n, queue_p->numEntries);
}

static uint32_t queue_peek_entry(struct net_queue *const queue_p)
{
	return queue_p->tail;
}



/****************************************/
/****************************************/
/***                                  ***/
/***             Queue  API           ***/
/***                                  ***/
/****************************************/
/****************************************/

/*!
 * @brief Initializes the queue data structure.
 *
 * @param [in] queue_p     Pointer to the queue data structure to initialize.
 * @param [in] pEntries    Pointer to the array of entries for the queue.
 * @param [in] numEntries  Number of entries in the queue.
 *
 * @return 0 if initialization is successful.
 *         -EINVAL for invalid parameters.
 */
int net_queue_init(struct net_queue *const queue_p, 
	void *const pEntries, uint32_t numEntries)
{   
	if (queue_p == NULL || pEntries == NULL || numEntries == 0)
		return -EINVAL;
    
	/* This is the _RAW value */
	queue_p->pEntries = pEntries;
	queue_p->numEntries = numEntries;
	queue_p->head = 0;
	queue_p->tail = 0;

	return 0;
}

/*!
 * @brief Gets the current number of frames in the queue.
 * 
 * @param [in] queue_p   Constant pointer to the queue data structure.
 * 
 * @return Current count of frames in the queue
 */
uint32_t net_queue_get_count(struct net_queue *const queue_p)
{
	return queue_count(queue_p);
}

/*!
 * @brief Checks if the queue is full.
 * 
 * @param [in] queue_p   Constant pointer to the queue data structure.
 * 
 * @return true if the queue is full.
 *         false if the queue is not full.
 */
bool net_queue_is_full(struct net_queue *const queue_p)
{              
	return queue_full(queue_p);
}

/*!
 * @brief Checks if the queue is empty.
 * 
 * @param [in] queue_p   Constant pointer to the queue data structure.
 * 
 * @return true if the queue is empty.
 *         false if the queue is not empty.
 */
bool net_queue_is_empty(struct net_queue *const queue_p)
{
	return queue_empty(queue_p);
}

/*!
 * @brief Gets the number of available slots in the queue.
 * 
 * @param [in] queue_p   Constant pointer to the queue data structure.
 * 
 * @return Number of available slots in the queue.
 */
uint32_t net_queue_get_avail_entries(struct net_queue *const queue_p)
{   
	return queue_available(queue_p);
}

/*!
 * @brief Adds a frame to the queue by advancing the head pointer.
 * 
 * @param [in] queue_p   Constant pointer to the queue data structure.
 * 
 */
void net_queue_add_entry(struct net_queue *const queue_p)
{   
	queue_add(queue_p);
}

/*!
 * @brief Removes a frame from the queue by advancing the tail pointer.
 * 
 * @param [in] queue_p   Constant pointer to the queue data structure.
 * 
 */
void net_queue_remove_entry(struct net_queue *const queue_p)
{   
	queue_remove(queue_p);
}

/*!
 * @brief Gets the tail index of the queue without removing the entry.
 * 
 * @param [in] queue_p   Constant pointer to the queue data structure.
 * 
 * @return Tail index of the queue entry to peek at.
 *         Returns 0 if queue is empty (which is the valid tail position).
 */
uint32_t net_queue_peek(struct net_queue *const queue_p)
{   
	if (queue_empty(queue_p))
		return 0;
	else
		return queue_peek_entry(queue_p);
}


/*
 * Copyright (c) 2004, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

/**
 * \file
 * Linked list library implementation.
 *
 * \author Adam Dunkels <adam@sics.se>
 *
 */

/**
 * \addtogroup list
 * @{
 */

#include "jbx-list.h"

#define NULL 0

struct jbx_list {
  struct jbx_list *next;
};

/*---------------------------------------------------------------------------*/
/**
 * Initialize a list.
 *
 * This function initalizes a list. The list will be empty after this
 * function has been called.
 *
 * \param list The list to be initialized.
 */
void
jbx_list_init(jbx_list_t jbx_list)
{
  *jbx_list = NULL;
}
/*---------------------------------------------------------------------------*/
/**
 * Get a pointer to the first element of a list.
 *
 * This function returns a pointer to the first element of the
 * list. The element will \b not be removed from the list.
 *
 * \param list The list.
 * \return A pointer to the first element on the list.
 *
 * \sa list_tail()
 */
void *
jbx_list_head(jbx_list_t jbx_list)
{
  return *jbx_list;
}
/*---------------------------------------------------------------------------*/
/**
 * Duplicate a list.
 *
 * This function duplicates a list by copying the list reference, but
 * not the elements.
 *
 * \note This function does \b not copy the elements of the list, but
 * merely duplicates the pointer to the first element of the list.
 *
 * \param dest The destination list.
 * \param src The source list.
 */
void
jbx_list_copy(jbx_list_t dest, jbx_list_t src)
{
  *dest = *src;
}
/*---------------------------------------------------------------------------*/
/**
 * Get the tail of a list.
 *
 * This function returns a pointer to the elements following the first
 * element of a list. No elements are removed by this function.
 *
 * \param list The list
 * \return A pointer to the element after the first element on the list.
 *
 * \sa list_head()
 */
void *
jbx_list_tail(jbx_list_t jbx_list)
{
  struct jbx_list *l;
  
  if(*jbx_list == NULL) {
    return NULL;
  }
  
  for(l = *jbx_list; l->next != NULL; l = l->next);
  
  return l;
}
/*---------------------------------------------------------------------------*/
/**
 * Add an item at the end of a list.
 *
 * This function adds an item to the end of the list.
 *
 * \param list The list.
 * \param item A pointer to the item to be added.
 *
 * \sa list_push()
 *
 */
void
jbx_list_add(jbx_list_t jbx_list, void *item)
{
  struct jbx_list *l;

  /* Make sure not to add the same element twice */
  jbx_list_remove(jbx_list, item);

  ((struct jbx_list *)item)->next = NULL;
  
  l = jbx_list_tail(jbx_list);

  if(l == NULL) {
    *jbx_list = item;
  } else {
    l->next = item;
  }
}
/*---------------------------------------------------------------------------*/
/**
 * Add an item to the start of the list.
 */
void
jbx_list_push(jbx_list_t jbx_list, void *item)
{
  /*  struct list *l;*/

  /* Make sure not to add the same element twice */
  jbx_list_remove(jbx_list, item);

  ((struct jbx_list *)item)->next = *jbx_list;
  *jbx_list = item;
}
/*---------------------------------------------------------------------------*/
/**
 * Remove the last object on the list.
 *
 * This function removes the last object on the list and returns it.
 *
 * \param list The list
 * \return The removed object
 *
 */
void *
jbx_list_chop(jbx_list_t jbx_list)
{
  struct jbx_list *l, *r;
  
  if(*jbx_list == NULL) {
    return NULL;
  }
  if(((struct jbx_list *)*jbx_list)->next == NULL) {
    l = *jbx_list;
    *jbx_list = NULL;
    return l;
  }
  
  for(l = *jbx_list; l->next->next != NULL; l = l->next);

  r = l->next;
  l->next = NULL;
  
  return r;
}
/*---------------------------------------------------------------------------*/
/**
 * Remove the first object on a list.
 *
 * This function removes the first object on the list and returns a
 * pointer to it.
 *
 * \param list The list.
 * \return Pointer to the removed element of list.
 */
/*---------------------------------------------------------------------------*/
void *
jbx_list_pop(jbx_list_t jbx_list)
{
  struct jbx_list *l;
  l = *jbx_list;
  if(*jbx_list != NULL) {
    *jbx_list = ((struct jbx_list *)*jbx_list)->next;
  }

  return l;
}
/*---------------------------------------------------------------------------*/
/**
 * Remove a specific element from a list.
 *
 * This function removes a specified element from the list.
 *
 * \param list The list.
 * \param item The item that is to be removed from the list.
 *
 */
/*---------------------------------------------------------------------------*/
void
jbx_list_remove(jbx_list_t jbx_list, void *item)
{
  struct jbx_list *l, *r;
  
  if(*jbx_list == NULL) {
    return;
  }
  
  r = NULL;
  for(l = *jbx_list; l != NULL; l = l->next) {
    if(l == item) {
      if(r == NULL) {
	/* First on jbx_list */
	*jbx_list = l->next;
      } else {
	/* Not first on jbx_list */
	r->next = l->next;
      }
      l->next = NULL;
      return;
    }
    r = l;
  }
}
/*---------------------------------------------------------------------------*/
/**
 * Get the length of a list.
 *
 * This function counts the number of elements on a specified list.
 *
 * \param list The list.
 * \return The length of the list.
 */
/*---------------------------------------------------------------------------*/
int
jbx_list_length(jbx_list_t jbx_list)
{
  struct jbx_list *l;
  int n = 0;

  for(l = *jbx_list; l != NULL; l = l->next) {
    ++n;
  }

  return n;
}
/*---------------------------------------------------------------------------*/
/**
 * \brief      Insert an item after a specified item on the jbx_list
 * \param list The list
 * \param previtem The item after which the new item should be inserted
 * \param newitem  The new item that is to be inserted
 * \author     Adam Dunkels
 *
 *             This function inserts an item right after a specified
 *             item on the list. This function is useful when using
 *             the list module to ordered lists.
 *
 *             If previtem is NULL, the new item is placed at the
 *             start of the list.
 *
 */
void
jbx_list_insert(jbx_list_t jbx_list, void *previtem, void *newitem)
{
  if(previtem == NULL) {
    jbx_list_push(jbx_list, newitem);
  } else {
  
    ((struct jbx_list *)newitem)->next = ((struct jbx_list *)previtem)->next;
    ((struct jbx_list *)previtem)->next = newitem;
  }
}
/*---------------------------------------------------------------------------*/
/**
 * \brief      Get the next item following this item
 * \param item A list item
 * \returns    A next item on the list
 *
 *             This function takes a list item and returns the next
 *             item on the list, or NULL if there are no more items on
 *             the list. This function is used when iterating through
 *             lists.
 */
void *
jbx_list_item_next(void *item)
{
  return item == NULL? NULL: ((struct jbx_list *)item)->next;
}
/*---------------------------------------------------------------------------*/
/** @} */

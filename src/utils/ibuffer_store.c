/*
 * This file is part of the Yices SMT Solver.
 * Copyright (C) 2023 SRI International.
 *
 * Yices is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Yices is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Yices.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stddef.h>
#include "mt/thread_macros.h"
#include "utils/dl_lists.h"
#include "utils/memalloc.h"
#include "utils/int_vectors.h"

/********************
 * INTEGER BUFFERS  *
 *******************/

/*
 * List of available buffers.
 */
typedef struct {
  dl_list_t header;
  ivector_t buffer;
} ibuffer_elem_t;

static YICES_THREAD_LOCAL dl_list_t ibuffer_list;

/*
 * Get header of buffer b, assuming b is embedded into a ibuffer_elem_t
 */
static inline dl_list_t *ibuffer_header(ivector_t *b) {
  return (dl_list_t *)((char *)b - offsetof(ibuffer_elem_t, buffer));
}

/* 
 * Get buffer of header l
 */
static inline ivector_t *ibuffer(dl_list_t *l) {
  return &((ibuffer_elem_t *)l)->buffer;
}

void init_ibuffer_store(void) {
  clear_list(&ibuffer_list);
}

/*
 * Empty the list of buffers
 */
void delete_ibuffer_store(void) {
  dl_list_t *elem, *aux;

  elem = ibuffer_list.next;
  while (elem != &ibuffer_list) {
    aux = elem->next;
    delete_ivector(ibuffer(elem));
    safe_free(elem);
    elem = aux;
  }
  clear_list(&ibuffer_list);
}

/*
 * Allocate an ibuffer.
 */
ivector_t *alloc_ibuffer(uint32_t n) {
  dl_list_t *elem;

  if (empty_list(&ibuffer_list)) {
    elem = (dl_list_t *) safe_malloc(sizeof(ibuffer_elem_t));
    init_ivector(ibuffer(elem),n);
  } else {
    elem = ibuffer_list.next;
    list_remove(elem);
    resize_ivector(ibuffer(elem), n);
  }
    
  return ibuffer(elem);
}

/*
 * Release buffer b, returning it to the free list.
 */
void free_ibuffer(ivector_t *b) {
  dl_list_t *elem;

  ivector_reset(b);
  elem = ibuffer_header(b);
  /* Make this the first buffer in the list so that it will be
     immediately reused when a new buffer is requested. */
  list_insert_next(&ibuffer_list, elem);
}

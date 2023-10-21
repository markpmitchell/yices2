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
#include "utils/ptr_vectors.h"

/********************
 * POINTER BUFFERS  *
 *******************/

/*
 * List of available buffers.
 */
typedef struct {
  dl_list_t header;
  pvector_t buffer;
} pbuffer_elem_t;

static YICES_THREAD_LOCAL dl_list_t pbuffer_list;

/*
 * Get header of buffer b, assuming b is embedded into a pbuffer_elem_t
 */
static inline dl_list_t *pbuffer_header(pvector_t *b) {
  return (dl_list_t *)((char *)b - offsetof(pbuffer_elem_t, buffer));
}

/* 
 * Get buffer of header l
 */
static inline pvector_t *pbuffer(dl_list_t *l) {
  return &((pbuffer_elem_t *)l)->buffer;
}

void init_pbuffer_store(void) {
  clear_list(&pbuffer_list);
}

/*
 * Empty the list of buffers
 */
void delete_pbuffer_store(void) {
  dl_list_t *elem, *aux;

  elem = pbuffer_list.next;
  while (elem != &pbuffer_list) {
    aux = elem->next;
    delete_pvector(pbuffer(elem));
    safe_free(elem);
    elem = aux;
  }
  clear_list(&pbuffer_list);
}

/*
 * Allocate an pbuffer.
 */
pvector_t *alloc_pbuffer(uint32_t n) {
  dl_list_t *elem;

  if (empty_list(&pbuffer_list)) {
    elem = (dl_list_t *) safe_malloc(sizeof(pbuffer_elem_t));
    init_pvector(pbuffer(elem),n);
  } else {
    elem = pbuffer_list.next;
    list_remove(elem);
    resize_pvector(pbuffer(elem), n);
  }
    
  return pbuffer(elem);
}

/*
 * Release buffer b, returning it to the free list.
 */
void free_pbuffer(pvector_t *b) {
  dl_list_t *elem;

  pvector_reset(b);
  elem = pbuffer_header(b);
  /* Make this the first buffer in the list so that it will be
     immediately reused when a new buffer is requested. */
  list_insert_next(&pbuffer_list, elem);
}

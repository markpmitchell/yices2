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

/*
 * THREAD-LOCAL STORE OF POINTER BUFFERS
 *
 * Maintain a store of integer buffers. Each thread has its own buffer
 * store. Therefore, a buffer must be allocated and released by the
 * same thread.
 */

#ifndef __PBUFFER_STORE_H
#define __PBUFFER_STORE_H

#include "utils/ptr_vectors.h"

/*
 * Initialize the buffer store. This function must be called in each
 * thread prior to allocating a buffer.
 */
extern void init_pbuffer_store(void);

/*
 * Release resources in the buffer store. This function must be called
 * by each thread in order to avoid resource leaks. After calling this
 * function, the buffer store must be re-initialized before further
 * use.
 */
extern void delete_pbuffer_store(void);

/*
 * Return a new, initialized buffer with room for N entries.
 */
extern pvector_t *alloc_pbuffer(uint32_t n);

/*
 * Release a buffer.
 */
extern void free_pbuffer(pvector_t *b);

#endif /* __PBUFFER_STORE_H */

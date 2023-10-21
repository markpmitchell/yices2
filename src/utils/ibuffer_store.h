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
 * THREAD-LOCAL STORE OF INTEGER BUFFERS
 *
 * Maintain a store of integer buffers. Each thread has its own buffer
 * store. Therefore, a buffer must be allocated and released by the
 * same thread.
 */

#ifndef __IBUFFER_STORE_H
#define __IBUFFER_STORE_H

#include "utils/int_vectors.h"

/*
 * Initialize the buffer store. This function must be called in each
 * thread prior to allocating a buffer.
 */
extern void init_ibuffer_store(void);

/*
 * Release resources in the buffer store. This function must be called
 * by each thread in order to avoid resource leaks. After calling this
 * function, the buffer store must be re-initialized before further
 * use.
 */
extern void delete_ibuffer_store(void);

/*
 * Return a new, initialized buffer with room for N elements.
 */
extern ivector_t *alloc_ibuffer(uint32_t n);

/*
 * Release a buffer.
 */
extern void free_ibuffer(ivector_t *b);

#endif /* __IBUFFER_STORE_H */

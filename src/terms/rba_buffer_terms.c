/*
 * This file is part of the Yices SMT Solver.
 * Copyright (C) 2017 SRI International.
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
 * ARITHMETIC OPERATIONS INVOLVING RED-BLACK BUFFERS AND TERMS
 */

#include <assert.h>

#include "terms/rba_buffer_terms.h"
#include "utils/pbuffer_store.h"


/*
 * Add t to buffer b
 * - t must be an arithmetic term
 * - b->ptbl and table->pprods must be equal
 */
void rba_buffer_add_term(rba_buffer_t *b, term_table_t *table, term_t t) {
  pvector_t *pbuffer;
  polynomial_t *p;
  int32_t i;

  assert(b->ptbl == table->pprods);
  assert(pos_term(t) && good_term(table, t) && is_arithmetic_term(table, t));

  i = index_of(t);
  switch (table->kind[i]) {
  case POWER_PRODUCT:
    rba_buffer_add_pp(b, pprod_for_idx(table, i));
    break;

  case ARITH_CONSTANT:
    rba_buffer_add_const(b, rational_for_idx(table, i));
    break;

  case ARITH_POLY:
    p = polynomial_for_idx(table, i);
    pbuffer = pprods_for_poly(table, p);
    rba_buffer_add_monarray(b, p->mono, (pprod_t **) pbuffer->data);
    free_pbuffer(pbuffer);
    break;

  default:
    rba_buffer_add_var(b, t);
    break;
  }
}


/*
 * Subtract t from buffer b
 * - t must be an arithmetic term
 * - b->ptbl and table->pprods must be equal
 */
void rba_buffer_sub_term(rba_buffer_t *b, term_table_t *table, term_t t) {
  pvector_t *pbuffer;
  polynomial_t *p;
  int32_t i;

  assert(b->ptbl == table->pprods);
  assert(pos_term(t) && good_term(table, t) && is_arithmetic_term(table, t));

  i = index_of(t);
  switch (table->kind[i]) {
  case POWER_PRODUCT:
    rba_buffer_sub_pp(b, pprod_for_idx(table, i));
    break;

  case ARITH_CONSTANT:
    rba_buffer_sub_const(b, rational_for_idx(table, i));
    break;

  case ARITH_POLY:
    p = polynomial_for_idx(table, i);
    pbuffer = pprods_for_poly(table, p);
    rba_buffer_sub_monarray(b, p->mono, (pprod_t **) pbuffer->data);
    free_pbuffer(pbuffer);
    break;

  default:
    rba_buffer_sub_var(b, t);
    break;
  }
}


/*
 * Multiply b by t
 * - t must be an arithmetic term
 * - b->ptbl and table->pprods must be equal
 */
void rba_buffer_mul_term(rba_buffer_t *b, term_table_t *table, term_t t) {
  pvector_t *pbuffer;
  polynomial_t *p;
  int32_t i;

  assert(b->ptbl == table->pprods);
  assert(pos_term(t) && good_term(table, t) && is_arithmetic_term(table, t));

  i = index_of(t);
  switch (table->kind[i]) {
  case POWER_PRODUCT:
    rba_buffer_mul_pp(b, pprod_for_idx(table, i));
    break;

  case ARITH_CONSTANT:
    rba_buffer_mul_const(b, rational_for_idx(table, i));
    break;

  case ARITH_POLY:
    p = polynomial_for_idx(table, i);
    pbuffer = pprods_for_poly(table, p);
    rba_buffer_mul_monarray(b, p->mono, (pprod_t **) pbuffer->data);
    free_pbuffer(pbuffer);
    break;

  default:
    rba_buffer_mul_var(b, t);
    break;
  }
}


/*
 * Add t * a to b
 * - t must be an arithmetic term
 * - b->ptbl and table->pprods must be equal
 */
void rba_buffer_add_const_times_term(rba_buffer_t *b, term_table_t *table, rational_t *a, term_t t) {
  rational_t q;
  pvector_t *pbuffer;
  polynomial_t *p;
  int32_t i;

  assert(b->ptbl == table->pprods);
  assert(pos_term(t) && good_term(table, t) && is_arithmetic_term(table, t));

  i = index_of(t);
  switch (table->kind[i]) {
  case POWER_PRODUCT:
    rba_buffer_add_mono(b, a, pprod_for_idx(table, i));
    break;

  case ARITH_CONSTANT:
    q_init(&q);
    q_set(&q, a);
    q_mul(&q, rational_for_idx(table, i));
    rba_buffer_add_const(b, &q);
    q_clear(&q);
    break;

  case ARITH_POLY:
    p = polynomial_for_idx(table, i);
    pbuffer = pprods_for_poly(table, p);
    rba_buffer_add_const_times_monarray(b, p->mono,
					(pprod_t **) pbuffer->data, a);
    free_pbuffer(pbuffer);
    break;

  default:
    rba_buffer_add_varmono(b, a, t);
    break;
  }
}



/*
 * Multiply b by t^d
 * - t must be an arithmetic term
 * - p->ptbl and table->pprods must be equal
 */
void rba_buffer_mul_term_power(rba_buffer_t *b, term_table_t *table, term_t t, uint32_t d) {
  rba_buffer_t aux;
  rational_t q;
  pvector_t *pbuffer;
  polynomial_t *p;
  pprod_t *r;
  int32_t i;

  assert(b->ptbl == table->pprods);
  assert(pos_term(t) && good_term(table, t) && is_arithmetic_term(table, t));

  i = index_of(t);
  switch (table->kind[i]) {
  case POWER_PRODUCT:
    r = pprod_exp(b->ptbl, pprod_for_idx(table, i), d); // r = t^d
    rba_buffer_mul_pp(b, r);
    break;

  case ARITH_CONSTANT:
    q_init(&q);
    q_set_one(&q);
    q_mulexp(&q, rational_for_idx(table, i), d); // q = t^d
    rba_buffer_mul_const(b, &q);
    q_clear(&q);
    break;

  case ARITH_POLY:
    p = polynomial_for_idx(table, i);
    pbuffer = pprods_for_poly(table, p);
    init_rba_buffer(&aux, b->ptbl);
    rba_buffer_mul_monarray_power(b, p->mono, (pprod_t **) pbuffer->data,
				  d, &aux);
    delete_rba_buffer(&aux);
    free_pbuffer(pbuffer);
    break;

  default:
    r = pprod_varexp(b->ptbl, t, d);
    rba_buffer_mul_pp(b, r);
    break;
  }
}

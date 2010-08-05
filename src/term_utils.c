/*
 * UTILITIES FOR SIMPLIFYING TERMS
 */


#include <assert.h>

#include "memalloc.h"
#include "bv64_constants.h"
#include "int_array_sort.h"
#include "int_vectors.h"
#include "int_hash_sets.h"
#include "term_utils.h"




/********************
 *  FINITE DOMAINS  *
 *******************/

/*
 * Build a domain descriptor that contains a[0 ... n-1]
 */
static finite_domain_t *make_finite_domain(term_t *a, uint32_t n) {
  finite_domain_t *tmp;
  uint32_t i;

  assert(n <= MAX_FINITE_DOMAIN_SIZE);
  tmp = (finite_domain_t *) safe_malloc(sizeof(finite_domain_t) + n * sizeof(term_t));
  tmp->nelems = n;
  for (i=0; i<n; i++) {
    tmp->data[i] = a[i];
  }

  return tmp;
}


/*
 * Add all elements of dom that are not in cache into vector v
 * - also store them in the cache
 */
static void add_domain(int_hset_t *cache, ivector_t *v, finite_domain_t *dom) {
  uint32_t i, n;
  term_t t;

  n = dom->nelems;
  for (i=0; i<n; i++) {
    t = dom->data[i];
    if (int_hset_add(cache, t)) {
      ivector_push(v, t);
    }
  }
}


/*
 * Recursively collect all constant terms reachable from t
 * - add all terms visited to hset 
 * - add all constants to vector v
 */
static void collect_finite_domain(term_table_t *tbl, int_hset_t *cache, ivector_t *v, term_t t) {
  special_term_t *d;

  if (int_hset_add(cache, t)) {
    // t not visited yet
    if (term_kind(tbl, t) == ITE_SPECIAL) {
      d = ite_special_desc(tbl, t);
      if (d->extra != NULL) {
	add_domain(cache, v, d->extra);
      } else {
	collect_finite_domain(tbl, cache, v, d->body.arg[1]);
	collect_finite_domain(tbl, cache, v, d->body.arg[2]);
      }
    } else {
      // t must be a constant, not already in v
      assert(term_kind(tbl, t) == ARITH_CONSTANT ||
	     term_kind(tbl, t) == BV64_CONSTANT ||
	     term_kind(tbl, t) == BV_CONSTANT);
      ivector_push(v, t);
    }
  }
}


/*
 * Build the domain for (ite c t1 t2)
 * - d must be the composite descriptor for (ite c t1 t2)
 */
static finite_domain_t *build_ite_finite_domain(term_table_t *tbl, composite_term_t *d) {
  int_hset_t cache;
  ivector_t buffer;
  finite_domain_t *dom;

  assert(d->arity == 3);
  
  init_int_hset(&cache, 32);
  init_ivector(&buffer, 20);

  collect_finite_domain(tbl, &cache, &buffer, d->arg[1]);  // then part
  collect_finite_domain(tbl, &cache, &buffer, d->arg[2]);  // else part

  int_array_sort(buffer.data, buffer.size);
  dom = make_finite_domain(buffer.data, buffer.size); 

  delete_ivector(&buffer);
  delete_int_hset(&cache);
  
  return dom;
}


/*
 * Get the finite domain of term t
 */
finite_domain_t *special_ite_get_finite_domain(term_table_t *tbl, term_t t) {
  special_term_t *d;

  d = ite_special_desc(tbl, t);
  if (d->extra == NULL) {
    d->extra = build_ite_finite_domain(tbl, &d->body);
  }
  return d->extra;
}



/*
 * Check whether u belongs to the finite domain of term t 
 * - t must be a special if-then-else
 */
bool term_is_in_finite_domain(term_table_t *tbl, term_t t, term_t u) {
  finite_domain_t *dom;
  uint32_t l, h, k;

  dom = special_ite_get_finite_domain(tbl, t);
  assert(dom->nelems >= 2);

  // binary search
  l = 0;
  h = dom->nelems;
  for (;;) {
    k = (l + h)/2; // no overflow possible since l+h < MAX_FINITE_DOMAIN_SIZE
    assert(l <= k && k < h && h <= dom->nelems);
    if (k == l) break;
    if (dom->data[k] > u) {
      h = k;
    } else {
      l = k;
    }
  }

  assert(l == k && k+1 == h);

  return dom->data[k] == u;
}



/*
 * Check whether two finite domains are disjoint.
 */
static bool disjoint_finite_domains(finite_domain_t *d1, finite_domain_t *d2) {
  uint32_t i1, i2, n1, n2;
  term_t t1, t2;

  assert(d1->nelems > 0 && d2->nelems > 0);

  n1 = d1->nelems;
  n2 = d2->nelems;
  i1 = 0;
  i2 = 0;
  t1 = d1->data[0];
  t2 = d2->data[0];
  for (;;) {
    if (t1 == t2) return false;
    if (t1 < t2) {
      i1 ++;
      if (i1 == n1) break;
      t1 = d1->data[i1];
    } else {
      i2 ++;
      if (i2 == n2) break;
      t2 = d2->data[i2];
    }
  }

  return true;
}



/*
 * Check whether t and u have disjoint finite domains
 * - both t and u must be special if-then-else
 * - the domains of t and u are computed if needed.
 */
bool terms_have_disjoint_finite_domains(term_table_t *tbl, term_t t, term_t u) {
  finite_domain_t *d1, *d2;

  d1 = special_ite_get_finite_domain(tbl, t);
  d2 = special_ite_get_finite_domain(tbl, u);
  return disjoint_finite_domains(d1, d2);
}



/*
 * FINITE RATIONAL DOMAIN
 */

/*
 * Check whether all elements in domain d are >= 0
 * - d must be an arithmetic domain (i.e., all elements in d are rational constants)
 */
static bool finite_domain_is_nonneg(term_table_t *tbl, finite_domain_t *d) {
  uint32_t i, n;
  term_t t;

  n = d->nelems;
  for (i=0; i<n; i++) {
    t = d->data[i];
    if (q_is_neg(rational_term_desc(tbl, t))) {
      return false;
    }
  }

  return true;
}


/*
 * Check whether all elements in domain d are negative
 * - d must be an arithmetic domain
 */
static bool finite_domain_is_neg(term_table_t *tbl, finite_domain_t *d) {
  uint32_t i, n;
  term_t t;

  n = d->nelems;
  for (i=0; i<n; i++) {
    t = d->data[i];
    if (q_is_nonneg(rational_term_desc(tbl, t))) {
      return false;
    }
  }

  return true;
}


/*
 * Check whether all elements in t's domain are non-negative
 * - t must be a special if-then-else of arithmetic type
 * - the domain of t is computed if required
 */
bool term_has_nonneg_finite_domain(term_table_t *tbl, term_t t) {
  finite_domain_t *d;

  d = special_ite_get_finite_domain(tbl, t);
  return finite_domain_is_nonneg(tbl, d);
}


/*
 * Check whether all elements in t's domain are negative
 * - t must be a special if-then-else term of arithemtic type
 * - the domain of t is computed if rrequired
 */
bool term_has_negative_finite_domain(term_table_t *tbl, term_t t) {
  finite_domain_t *d;

  d = special_ite_get_finite_domain(tbl, t);
  return finite_domain_is_neg(tbl, d);
}







/***********************************
 *  OPERATIONS ON BIT ARRAY TERMS  *
 **********************************/

/*
 * Upper/lower bound on a bitarray interpreted as an unsigned integer.
 *   a = a[0] + 2 a[1] + ... + 2^(n-1) a[n-1], with 0 <= a[i] <= 1
 * upper bound: replace a[i] by 1 if a[i] != 0
 * lower bound: replace a[i] by 0 if a[i] != 1
 */
static void bitarray_upper_bound_unsigned(composite_term_t *a, bvconstant_t *c) {
  uint32_t i, n;

  assert(a->arity > 0);

  n = a->arity;
  bvconstant_set_all_one(c, n); // c := 0b1...1 (n bits)
  for (i=0; i<n; i++) {
    if (a->arg[i] == false_term) {
      bvconst_clr_bit(c->data, i);
    }
  }
}

static void bitarray_lower_bound_unsigned(composite_term_t *a, bvconstant_t *c) {
  uint32_t i, n;

  assert(a->arity > 0);

  n = a->arity;
  bvconstant_set_all_zero(c, n); // c := 0b0...0 (n bits)
  for (i=0; i<n; i++) {
    if (a->arg[i] == true_term) {
      bvconst_set_bit(c->data, i);
    }
  }
}


/*
 * Upper/lower bound on a bitarray interpreted as a signed integer.
 *   a = a[0] + 2 a[1] + ... + 2^(n-2) a[n-2] - 2^(n-1) a[n-1]
 * upper bound: 
 *   for i=0 to n-2, replace a[i] by 1 if a[i] != 0 
 *   replace the sign bit a[n-1] by 0 unless a[n-1] = 1.
 * lower bound: 
 *   for i=0 to n-2, replace a[i] by 0 if a[i] != 1
 *   replace the sign bit a[n-1] by 1 unless a[n-1] = 0.
 */
static void bitarray_upper_bound_signed(composite_term_t *a, bvconstant_t *c) {
  uint32_t i, n;

  assert(a->arity > 0);

  n = a->arity;
  bvconstant_set_all_one(c, n);

  for (i=0; i<n-1; i++) {
    if (a->arg[i] == false_term) {
      bvconst_clr_bit(c->data, i);
    }
  }

  if (a->arg[i] != true_term) {
    bvconst_clr_bit(c->data, i);
  }
}


static void bitarray_lower_bound_signed(composite_term_t *a, bvconstant_t *c) {
  uint32_t i, n;

  assert(a->arity > 0);

  n = a->arity;
  bvconstant_set_all_zero(c, n);

  for (i=0; i<n-1; i++) {
    if (a->arg[i] == true_term) {
      bvconst_set_bit(c->data, i);
    }
  }

  if (a->arg[i] != false_term) {
    bvconst_set_bit(c->data, i);
  }
}




/*
 * BOUNDS FOR ARRAYS OF 1 TO 64BITS
 */

/*
 * Upper/lower bound on a bitarray interpreted as an unsigned integer.
 *   a = a[0] + 2 a[1] + ... + 2^(n-1) a[n-1], with 0 <= a[i] <= 1
 * upper bound: replace a[i] by 1 if a[i] != 0
 * lower bound: replace a[i] by 0 if a[i] != 1
 */
static uint64_t bitarray_upper_bound_unsigned64(composite_term_t *a) {
  uint64_t c;
  uint32_t i, n;

  assert(0 < a->arity && a->arity <= 64);

  n = a->arity;
  c = mask64(n); // c = 0001...1 (n lower bits set)
  for (i=0; i<n; i++) {
    if (a->arg[i] == false_term) {
      c = clr_bit64(c, i);
    }
  }

  assert(c == norm64(c, n));

  return c;
}

static uint64_t bitarray_lower_bound_unsigned64(composite_term_t *a) {
  uint64_t c;
  uint32_t i, n;

  assert(0 < a->arity && a->arity <= 64);
  
  n = a->arity;
  c = 0;
  for (i=0; i<n; i++) {
    if (a->arg[i] == true_term) {
      c = set_bit64(c, i);
    }
  }

  assert(c == norm64(c, n));

  return c;
}


/*
 * Upper/lower bound on a bitarray interpreted as a signed integer.
 *   a = a[0] + 2 a[1] + ... + 2^(n-2) a[n-2] - 2^(n-1) a[n-1]
 * upper bound: 
 *   for i=0 to n-2, replace a[i] by 1 if a[i] != 0 
 *   replace the sign bit a[n-1] by 0 unless a[n-1] = 1.
 * lower bound: 
 *   for i=0 to n-2, replace a[i] by 0 if a[i] != 1
 *   replace the sign bit a[n-1] by 1 unless a[n-1] = 0.
 */
static uint64_t bitarray_upper_bound_signed64(composite_term_t *a) {
  uint64_t c;
  uint32_t i, n;

  assert(0 < a->arity && a->arity <= 64);

  n = a->arity;
  c = mask64(n); // c = 0001...1
  for (i=0; i<n-1; i++) {
    if (a->arg[i] == false_term) {
      c = clr_bit64(c, i);
    }
  }

  if (a->arg[i] != true_term) {
    c = clr_bit64(c, i); // clear the sign bit
  }

  return c;
}


static uint64_t bitarray_lower_bound_signed64(composite_term_t *a) {
  uint64_t c;
  uint32_t i, n;

  assert(0 < a->arity && a->arity <= 64);

  n = a->arity;
  c = 0;

  for (i=0; i<n-1; i++) {
    if (a->arg[i] == true_term) {
      c = set_bit64(c, i);
    }
  }

  if (a->arg[i] != false_term) {
    c = set_bit64(c, i); // set the sign bit
  }

  return c;
}




/*
 * DISEQUALITY CHECKS
 */

/*
 * Disequality check between two bit arrays
 * - a and b must have the same arity
 * - all components must be boolean
 *
 * TODO?: improve this.
 * - we could try to see that (l l) can't be equal to (u (not u))
 */
static bool disequal_bitarrays(composite_term_t *a, composite_term_t *b) {
  uint32_t i, n;

  assert(a->arity == b->arity);

  n = a->arity;
  for (i=0; i<n; i++) {
    if (opposite_bool_terms(a->arg[i], b->arg[i])) return true;
  }

  return false;
}


/*
 * Disequality check between bit array a and small constant c
 * - both must have the same bit size
 */
static bool disequal_bitarray_bvconst64(composite_term_t *a, bvconst64_term_t *c) {
  uint32_t i, n;

  assert(a->arity == c->bitsize && 0 < a->arity && a->arity <= 64);

  n = a->arity;
  for (i=0; i<n; i++) {
    if (index_of(a->arg[i]) == bool_const) {
      assert(a->arg[i] == true_term || a->arg[i] == false_term);
      if (a->arg[i] != bool2term(tst_bit64(c->value, i))) {
	return true;
      }
    }
  }

  return false;
}


/*
 * Disequality check between bit array a and bv-constant c
 * - both must have the same bit size
 */
static bool disequal_bitarray_bvconst(composite_term_t *a, bvconst_term_t *c) {
  uint32_t i, n;

  assert(a->arity == c->bitsize && 64 < a->arity);

  n = a->arity;
  for (i=0; i<n; i++) {
    if (index_of(a->arg[i]) == bool_const) {
      assert(a->arg[i] == true_term || a->arg[i] == false_term);
      if (a->arg[i] != bool2term(bvconst_tst_bit(c->data, i))) {
	return true;
      }
    }
  }

  return false;
}





/******************************
 *  CHECKS FOR DISEQUALITIES  *
 *****************************/

/*
 * Base cases:
 * - x and y are both CONSTANT_TERM
 * - x and y are boolean and x = (not y).
 */
static inline bool disequal_constant_terms(term_t x, term_t y) {
  return x != y;
}

static inline bool disequal_boolean_terms(term_t x, term_t y) {
  return opposite_bool_terms(x, y);
}



/*
 * Test whether x can't be an integer
 * - incomplete
 */
static bool is_non_integer_term(term_table_t *tbl, term_t x) {
  return term_kind(tbl, x) == ARITH_CONSTANT && !q_is_integer(rational_term_desc(tbl, x));
}


/*
 * Arithmetic: x and y are both arithmetic terms
 *
 * The conversion of arith_buffer to terms ensures that polynomial
 * terms are not constant and not of the form 1.x for some term x.
 *
 * We deal with simple cases:
 * - x is integer and y is not (or conversely)
 * - both x and y are constant 
 * - both x and y are polynomials
 * - x is a polynomial and y is not a constant (i.e., y may occur as a variable in x)
 * - y is a polynomial and x is not a constant
 *
 * TODO? we could do more when (x - y) is a polynomial with integer variables.
 */
bool disequal_arith_terms(term_table_t *tbl, term_t x, term_t y) {
  term_kind_t kx, ky;

  if (is_integer_term(tbl, x) && is_non_integer_term(tbl, y)) {
    return true;
  }

  if (is_integer_term(tbl, y) && is_non_integer_term(tbl, x)) {
    return true;
  }

  kx = term_kind(tbl, x);
  ky = term_kind(tbl, y);

  if (kx == ARITH_CONSTANT && ky == ARITH_CONSTANT) {
    return x != y; // because of hash consing.
  }
  
  if (kx == ARITH_CONSTANT && ky == ITE_SPECIAL) {
    return ! term_is_in_finite_domain(tbl, y, x);
  }

  if (kx == ITE_SPECIAL && ky == ARITH_CONSTANT) {
    return !term_is_in_finite_domain(tbl, x, y);
  }

  if (kx == ITE_SPECIAL && ky == ITE_SPECIAL) {
    return terms_have_disjoint_finite_domains(tbl, x, y);
  }

  if (kx == ARITH_POLY && ky == ARITH_POLY) {
    return disequal_polynomials(poly_term_desc(tbl, x), poly_term_desc(tbl, y));
  }

  if (kx == ARITH_POLY && ky != ARITH_CONSTANT) {
    return polynomial_is_const_plus_var(poly_term_desc(tbl, x), y);
  }

  if (ky == ARITH_POLY && kx != ARITH_CONSTANT) {
    return polynomial_is_const_plus_var(poly_term_desc(tbl, y), x);
  } 

  return false;
}




/*
 * Bitvectors: x and y are bitvector terms of 1 to 64 bits
 */
static bool disequal_bv64_terms(term_table_t *tbl, term_t x, term_t y) {
  term_kind_t kx, ky;

  kx = term_kind(tbl, x);
  ky = term_kind(tbl, y);

  if (kx == ky) {
    if (kx == BV64_CONSTANT) {
      return x != y;
    }

    if (kx == BV64_POLY) {
      return disequal_bvpoly64(bvpoly64_term_desc(tbl, x), bvpoly64_term_desc(tbl, y));
    }

    if (kx == BV_ARRAY) {
      return disequal_bitarrays(bvarray_term_desc(tbl, x), bvarray_term_desc(tbl, y));
    }

    if (kx == ITE_SPECIAL) {
      return terms_have_disjoint_finite_domains(tbl, x, y);
    }

  } else {
   
    if (kx == BV64_CONSTANT && ky == BV_ARRAY) {
      return disequal_bitarray_bvconst64(bvarray_term_desc(tbl, y), bvconst64_term_desc(tbl, x));
    }

    if (ky == BV64_CONSTANT && kx == BV_ARRAY) {
      return disequal_bitarray_bvconst64(bvarray_term_desc(tbl, x), bvconst64_term_desc(tbl, y));
    }

    if (kx == BV64_CONSTANT && ky == ITE_SPECIAL) {
      return !term_is_in_finite_domain(tbl, y, x);
    }

    if (ky == BV64_CONSTANT && kx == ITE_SPECIAL) {
      return !term_is_in_finite_domain(tbl, x, y);
    }

    if (kx == BV64_POLY && ky != BV64_CONSTANT) {
      return bvpoly64_is_const_plus_var(bvpoly64_term_desc(tbl, x), y);
    }

    if (ky == BV64_POLY && kx != BV64_CONSTANT) {
      return bvpoly64_is_const_plus_var(bvpoly64_term_desc(tbl, y), x);
    }

  }

  return false;
}


/*
 * x and y are two bitvectors of more than 64bits
 */
static bool disequal_bv_terms(term_table_t *tbl, term_t x, term_t y) {
  term_kind_t kx, ky;

  kx = term_kind(tbl, x);
  ky = term_kind(tbl, y);

  if (kx == ky) {
    if (kx == BV_CONSTANT) {
      return x != y;
    }

    if (kx == BV_POLY) {
      return disequal_bvpoly(bvpoly_term_desc(tbl, x), bvpoly_term_desc(tbl, y));
    }

    if (kx == BV_ARRAY) {
      return disequal_bitarrays(bvarray_term_desc(tbl, x), bvarray_term_desc(tbl, y));
    }

    if (kx == ITE_SPECIAL) {
      return terms_have_disjoint_finite_domains(tbl, x, y);
    }

  } else {
   
    if (kx == BV_CONSTANT && ky == BV_ARRAY) {
      return disequal_bitarray_bvconst(bvarray_term_desc(tbl, y), bvconst_term_desc(tbl, x));
    }

    if (ky == BV_CONSTANT && kx == BV_ARRAY) {
      return disequal_bitarray_bvconst(bvarray_term_desc(tbl, x), bvconst_term_desc(tbl, y));
    }

    if (kx == BV_CONSTANT && ky == ITE_SPECIAL) {
      return !term_is_in_finite_domain(tbl, y, x);
    }

    if (ky == BV_CONSTANT && kx == ITE_SPECIAL) {
      return !term_is_in_finite_domain(tbl, x, y);
    }

    if (kx == BV_POLY && ky != BV_CONSTANT) {
      return bvpoly_is_const_plus_var(bvpoly_term_desc(tbl, x), y);
    }

    if (ky == BV_POLY && kx != BV_CONSTANT) {
      return bvpoly_is_const_plus_var(bvpoly_term_desc(tbl, y), x);
    }

  }

  return false;
}


/*
 * Generic form for two bitvector terms x and y
 */
bool disequal_bitvector_terms(term_table_t *tbl, term_t x, term_t y) {
  assert(is_bitvector_term(tbl, x) && is_bitvector_term(tbl, y) &&
	 term_bitsize(tbl, x) == term_bitsize(tbl, y));
  
  if (term_bitsize(tbl, x) <= 64) {
    return disequal_bv64_terms(tbl, x, y);
  } else {
    return disequal_bv_terms(tbl, x, y);
  }
}


/*
 * Tuple terms x and y are trivially distinct if they have components 
 * x_i and y_i that are trivially distinct.
 */
static bool disequal_tuple_terms(term_table_t *tbl, term_t x, term_t y) {
  composite_term_t *tuple_x, *tuple_y;
  uint32_t i, n;

  tuple_x = tuple_term_desc(tbl, x);
  tuple_y = tuple_term_desc(tbl, y);

  n = tuple_x->arity;
  assert(n == tuple_y->arity);
  for (i=0; i<n; i++) {
    if (disequal_terms(tbl, tuple_x->arg[i], tuple_y->arg[i])) {
      return true;
    }
  }
  return false;
}


/*
 * (update f x1 ... xn a) is trivially distinct from (update f x1 ... xn b)
 * if a is trivially distinct from b.
 */
static bool disequal_update_terms(term_table_t *tbl, term_t x, term_t y) {
  composite_term_t *update_x, *update_y;
  int32_t i, n;

  assert(term_type(tbl, x) == term_type(tbl, y));

  update_x = update_term_desc(tbl, x);
  update_y = update_term_desc(tbl, y);

  n = update_x->arity;
  assert(n == update_y->arity && n > 0);
  for (i=0; i<n-1; i++) {
    if (update_x->arg[i] != update_y->arg[i]) return false;
  }

  return disequal_terms(tbl, update_x->arg[i], update_y->arg[i]);
}


/*
 * Top level check: x and y must be valid terms of compatible types
 */
bool disequal_terms(term_table_t *tbl, term_t x, term_t y) {
  term_kind_t kind;

  if (is_boolean_term(tbl, x)) {
    assert(is_boolean_term(tbl, y));
    return disequal_boolean_terms(x, y);
  }

  if (is_arithmetic_term(tbl, x)) {
    assert(is_arithmetic_term(tbl, y));
    return disequal_arith_terms(tbl, x, y);
  }

  if (is_bitvector_term(tbl, x)) {
    assert(is_bitvector_term(tbl, y) && term_bitsize(tbl, x) == term_bitsize(tbl, y));
    if (term_bitsize(tbl, x) <= 64) {
      return disequal_bv64_terms(tbl, x, y);
    } else {
      return disequal_bv_terms(tbl, x, y);
    }
  }

  kind = term_kind(tbl, x);
  if (kind != term_kind(tbl, y)) return false;

  switch (kind) {
  case CONSTANT_TERM:
    return disequal_constant_terms(x, y);
  case TUPLE_TERM:
    return disequal_tuple_terms(tbl, x, y);
  case UPDATE_TERM:
    return disequal_update_terms(tbl, x, y);
  default:
    return false;
  }
}



// check whether a[i] cannot be equal to b[i] for one i
bool disequal_term_arrays(term_table_t *tbl, uint32_t n, term_t *a, term_t *b) {
  uint32_t i;

  for (i=0; i<n; i++) {
    if (disequal_terms(tbl, a[i], b[i])) return true;
  }

  return false;
}

// check whether all elements of a are disequal
// this is expensive: quadratic cost, but should fail quickly on most examples
bool pairwise_disequal_terms(term_table_t *tbl, uint32_t n, term_t *a) {
  uint32_t i, j;

  for (i=0; i<n; i++) {
    for (j=i+1; j<n; j++) {
      if (! disequal_terms(tbl, a[i], a[j])) return false;
    }
  }

  return true;
}




/********************************
 *  BOUNDS ON ARITHMETIC TERMS  *
 *******************************/

/*
 * Check whether t is non-negative. This is incomplete and 
 * deals only with simple cases. 
 * - return true if the checks can determine that t >= 0
 * - return false otherwise
 */
bool arith_term_is_nonneg(term_table_t *tbl, term_t t) {
  assert(is_arithmetic_term(tbl, t));

  switch (term_kind(tbl, t)) {
  case ARITH_CONSTANT:
    return q_is_nonneg(rational_term_desc(tbl, t));

  case ITE_SPECIAL:
    return term_has_nonneg_finite_domain(tbl, t);

  case ARITH_POLY:
    return polynomial_is_nonneg(poly_term_desc(tbl, t));

  default:
    return false;    
  }
}


/*
 * Check whether t is negative (incomplete)
 * - return true if the checks succeed and determine that t < 0
 * - return false otherwise
 */
bool arith_term_is_negative(term_table_t *tbl, term_t t) {
  assert(is_arithmetic_term(tbl, t));

  switch (term_kind(tbl, t)) {
  case ARITH_CONSTANT:
    return q_is_neg(rational_term_desc(tbl, t));

  case ITE_SPECIAL:
    return term_has_negative_finite_domain(tbl, t);

  case ARITH_POLY:
    return polynomial_is_neg(poly_term_desc(tbl, t));

  default:
    return false;    
  }
}





/*******************************
 *  BOUNDS ON BITVECTOR TERMS  *
 ******************************/

/*
 * Copy a bitvector constant a into c
 */
static inline void copy_bvconst_term(bvconst_term_t *a, bvconstant_t *c) {
  assert(a->bitsize > 0);
  bvconstant_copy(c, a->bitsize, a->data);
}

static void copy_bvconst64_term(bvconst64_term_t *a, bvconstant_t *c) {
  uint32_t aux[2];


  aux[0] = (uint32_t) a->value; // lower-order word
  aux[1] = (uint32_t) (a->value >> 32); // high order word  (unused if bitsize <= 32)
  bvconstant_copy(c, a->bitsize, aux);
}


/*
 * Upper bound on t, interpreted as an unsigned integer
 * - copy the result in c
 */
void upper_bound_unsigned(term_table_t *tbl, term_t t, bvconstant_t *c) {
  uint32_t n;

  assert(is_bitvector_term(tbl, t));

  switch (term_kind(tbl, t)) {
  case BV64_CONSTANT:
    copy_bvconst64_term(bvconst64_term_desc(tbl, t), c);
    break;

  case BV_CONSTANT:
    copy_bvconst_term(bvconst_term_desc(tbl, t), c);
    break;

  case BV_ARRAY:
    bitarray_upper_bound_unsigned(bvarray_term_desc(tbl, t), c);
    break;

  default:
    n = term_bitsize(tbl, t);
    bvconstant_set_all_one(c, n);
    break;
  }  
}



/*
 * Lower bound on t, interpreted as an unsigned integer
 * - copy the result in c
 */
void lower_bound_unsigned(term_table_t *tbl, term_t t, bvconstant_t *c) {
  uint32_t n;

  assert(is_bitvector_term(tbl, t));

  switch (term_kind(tbl, t)) {
  case BV64_CONSTANT:
    copy_bvconst64_term(bvconst64_term_desc(tbl, t), c);
    break;

  case BV_CONSTANT:
    copy_bvconst_term(bvconst_term_desc(tbl, t), c);
    break;

  case BV_ARRAY:
    bitarray_lower_bound_unsigned(bvarray_term_desc(tbl, t), c);
    break;

  default:
    n = term_bitsize(tbl, t);
    bvconstant_set_all_zero(c, n);
    break;
  }  
}


/*
 * Upper bound on t, interpreted as a signed integer
 * - copy the result in c
 */
void upper_bound_signed(term_table_t *tbl, term_t t, bvconstant_t *c) {
  uint32_t n;

  assert(is_bitvector_term(tbl, t));

  switch (term_kind(tbl, t)) {
  case BV64_CONSTANT:
    copy_bvconst64_term(bvconst64_term_desc(tbl, t), c);
    break;

  case BV_CONSTANT:
    copy_bvconst_term(bvconst_term_desc(tbl, t), c);
    break;

  case BV_ARRAY:
    bitarray_upper_bound_signed(bvarray_term_desc(tbl, t), c);
    break;

  default:
    n = term_bitsize(tbl, t);
    assert(n > 0);
    bvconstant_set_all_one(c, n);
    bvconst_clr_bit(c->data, n-1); // clear the sign bit
    break;
  }  
}


/*
 * Lower bound on t, interpreted as a signed integer
 * - copy the result in c
 */
void lower_bound_signed(term_table_t *tbl, term_t t, bvconstant_t *c) {
  uint32_t n;

  assert(is_bitvector_term(tbl, t));

  switch (term_kind(tbl, t)) {
  case BV64_CONSTANT:
    copy_bvconst64_term(bvconst64_term_desc(tbl, t), c);
    break;

  case BV_CONSTANT:
    copy_bvconst_term(bvconst_term_desc(tbl, t), c);
    break;

  case BV_ARRAY:
    bitarray_lower_bound_signed(bvarray_term_desc(tbl, t), c);
    break;

  default:
    n = term_bitsize(tbl, t);
    assert(n > 0);
    bvconstant_set_all_zero(c, n);
    bvconst_set_bit(c->data, n-1); // set the sign bit
    break;
  }  
}




/*
 * BOUNDS FOR VECTORS OF 1 TO 64 BITS
 */

/*
 * Upper bound on t, interpreted as an unsigned integer
 */
uint64_t upper_bound_unsigned64(term_table_t *tbl, term_t t) {
  uint64_t c;
  uint32_t n;

  assert(is_bitvector_term(tbl, t));

  switch (term_kind(tbl, t)) {
  case BV64_CONSTANT:
    c = bvconst64_term_desc(tbl, t)->value;
    break;

  case BV_ARRAY:
    c = bitarray_upper_bound_unsigned64(bvarray_term_desc(tbl, t));
    break;

  default:
    n = term_bitsize(tbl, t);
    assert(1 <= n && n <= 64);
    c = mask64(n);
    break;
  }

  return c;
}


/*
 * Lower bound on t, interpreted as an unsigned integer
 */
uint64_t lower_bound_unsigned64(term_table_t *tbl, term_t t) {
  uint64_t c;

  assert(is_bitvector_term(tbl, t));

  switch (term_kind(tbl, t)) {
  case BV64_CONSTANT:
    c = bvconst64_term_desc(tbl, t)->value;
    break;

  case BV_ARRAY:
    c = bitarray_lower_bound_unsigned64(bvarray_term_desc(tbl, t));
    break;

  default:
    c = 0;
    break;
  }

  return c;
}


/*
 * Upper bound on t, interpreted as a signed integer
 */ 
uint64_t upper_bound_signed64(term_table_t *tbl, term_t t) {
  uint64_t c;
  uint32_t n;

  assert(is_bitvector_term(tbl, t));

  switch (term_kind(tbl, t)) {
  case BV64_CONSTANT:
    c = bvconst64_term_desc(tbl, t)->value;
    break;

  case BV_ARRAY:
    c = bitarray_upper_bound_signed64(bvarray_term_desc(tbl, t));
    break;

  default:
    n = term_bitsize(tbl, t);
    c = max_signed64(n);
    break;
  }

  return c;
}


/*
 * Lower bound on t, interpreted as a signed integer
 */
uint64_t lower_bound_signed64(term_table_t *tbl, term_t t) {
  uint64_t c;
  uint32_t n;

  assert(is_bitvector_term(tbl, t));

  switch (term_kind(tbl, t)) {
  case BV64_CONSTANT:
    c = bvconst64_term_desc(tbl, t)->value;
    break;

  case BV_ARRAY:
    c = bitarray_lower_bound_signed64(bvarray_term_desc(tbl, t));
    break;

  default:
    n = term_bitsize(tbl, t);
    c = min_signed64(n);
    break;
  }

  return c;
}




/*
 * Get bit i of term t:
 * - return NULL_TERM if the bit can't be determined
 * - return true or false if t is a bitvector constant
 * - return b_i if t is (bv-array b_0 .. b_i ...)
 *
 * t must be a bitvector term of size > i
 */
term_t extract_bit(term_table_t *tbl, term_t t, uint32_t i) {
  uint32_t *d;
  uint64_t c;  
  term_t bit;

  assert(is_bitvector_term(tbl, t) && term_bitsize(tbl, t) > i);

  switch (term_kind(tbl, t)) {
  case BV64_CONSTANT:
    c = bvconst64_term_desc(tbl, t)->value;
    bit = bool2term(tst_bit64(c, i));
    break;

  case BV_CONSTANT:
    d = bvconst_term_desc(tbl, t)->data;
    bit = bool2term(bvconst_tst_bit(d, i));
    break;

  case BV_ARRAY:
    bit = bvarray_term_desc(tbl, t)->arg[i];
    break;

  default:
    bit = NULL_TERM;
    break;
  }

  return bit;
}







/*
 * UNIT-TYPE REPRESENTATIVES
 */

/*
 * Representative of a singleton type tau:
 * - for scalar type: the unique constant of that type
 * - for function type: an uninterpreted term (denoting the constant function)
 * - for tuple type: (tau_1 ... tau_n)
 *   representative = (tuple (rep tau_1) ... (rep tau_n))
 */

/*
 * Tuple of representative terms.
 */
static term_t make_tuple_rep(term_table_t *table, tuple_type_t *d) {
  term_t aux[8];
  term_t *a;
  term_t t;
  uint32_t i, n;

  n = d->nelem;
  a = aux;
  if (n > 8) {
    a = (term_t *) safe_malloc(n * sizeof(term_t));
  }

  for (i=0; i<n; i++) {
    a[i] = get_unit_type_rep(table, d->elem[i]);
  }
  t = tuple_term(table, n, a);

  if (n > 8) {
    safe_free(a);
  }

  return t;
}

/*
 * Return the term representative for unit type tau.
 * - search the table of unit-types first
 * - create a new term if there's no entry for tau in that table.
 */
term_t get_unit_type_rep(term_table_t *table, type_t tau) {
  type_table_t *types;
  term_t t;

  assert(is_unit_type(table->types, tau));
  
  t = unit_type_rep(table, tau);
  if (t == NULL_TERM) {
    types = table->types;
    switch (type_kind(types, tau)) {
    case SCALAR_TYPE:
      assert(scalar_type_cardinal(types, tau) == 1);
      t = constant_term(table, tau, 0);
      break;

    case TUPLE_TYPE:
      t = make_tuple_rep(table, tuple_type_desc(types, tau));
      break;

    case FUNCTION_TYPE:
      t = new_uninterpreted_term(table, tau);
      break;

    default:
      assert(false);
      break;
    }
    add_unit_type_rep(table, tau, t);
  }

  return t;
}



#ifndef THREADS_REAL_NUM_H
#define THREADS_REAL_NUM_H

/* Fixed point real arithmetic */

static const int32_t F = 1<<14;
static inline int f_to_int (int32_t);
static inline int32_t int_to_f (int);
static inline int32_t f_add (int32_t, int32_t);
static inline int32_t f_sub (int32_t, int32_t);
static inline int32_t f_mul (int32_t, int32_t);
static inline int32_t f_div (int32_t, int32_t);

static inline int
f_to_int (int32_t x)
{
  if (x >= 0)
    return (int)((x + F/2)/F);
  else
    return (int)((x - F/2)/F);
}

static inline int32_t
int_to_f (int n)
{
  return (int32_t)(n*F);
}

static inline int32_t
f_add (int32_t x, int32_t y)
{
  return x + y;
}

static inline int32_t
f_sub (int32_t x, int32_t y)
{
  return x - y;
}

static inline int32_t
f_mul (int32_t x, int32_t y)
{
  return (int32_t)(((int64_t) x) * y / F);
}

static inline int32_t
f_div (int32_t x, int32_t y)
{
  ASSERT(y != 0)
  return (int32_t)(((int64_t) x) * F / y);
}

#endif

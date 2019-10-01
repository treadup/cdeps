#include "arithmetic.h"
#include "polynomial.h"

double quadratic(double a, double b, double c, double x) {
  return add(add(mul(mul(a, x), x), mul(b, x)), c);
}

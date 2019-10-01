#include <stdio.h>
#include "math/polynomial.h"

int main() {
    double a = 3.0;
    double b = 4.0;
    double c = 5.0;
    double x = 7.0;

    printf("The value at %.1f is %.1f", x, quadratic(a, b, c, x));

    return 0;
}

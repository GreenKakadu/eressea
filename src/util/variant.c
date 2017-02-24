#include <platform.h>
#include "variant.h"

#include <assert.h>
#include <limits.h>

const variant frac_zero = { .sa = { 0, 1 } };
const variant frac_one = { .sa = { 1, 1 } };

static int gcd(int a, int b) {
    const int primes[] = { 3, 5, 7, 11, 13, 17, 19, 23, 0 };
    int i = 0, g = 1, p = 2;
    while (p && p <= a && p <= b) {
        if (a % p == 0 && b % p == 0) {
            a /= p;
            b /= p;
            g *= p;
        }
        else {
            p = primes[i++];
        }
    }
    return g;
}

static int lcm(int a, int b) {
    int x;
    if (a == b) return a;
    x = (a * b);
    x = (x > 0 ? x : -x) / gcd(a, b);
    return x;
}

bool frac_equal(variant a, variant b) {
    return frac_sign(frac_sub(a, b)) == 0;
}

variant frac_make(int num, int den)
{
    variant v;
    int g = gcd(num, den);
    num /= g;
    den /= g;
    assert(num >= SHRT_MIN && num <= SHRT_MAX);
    assert(den >= SHRT_MIN && den <= SHRT_MAX);
    v.sa[0] = (short)num;
    v.sa[1] = (short)den;
    return v;
}

variant frac_add(variant a, variant b)
{
    int num, den;
    den = lcm(a.sa[1], b.sa[1]);
    num = a.sa[0] * den / a.sa[1] + b.sa[0] * den / b.sa[1];
    return frac_make(num, den);
}

variant frac_sub(variant a, variant b)
{
    b.sa[0] = -b.sa[0];
    return frac_add(a, b);
}

variant frac_mul(variant a, variant b)
{
    return frac_make(a.sa[0] * b.sa[0], a.sa[1] * b.sa[1]);
}

variant frac_div(variant a, variant b)
{
    return frac_make(a.sa[0] * b.sa[1], a.sa[1] * b.sa[0]);
}

int frac_sign(variant a) {
    if (a.sa[0] == 0) return 0;
    if (a.sa[0] > 0 && a.sa[1] > 0) return 1;
    if (a.sa[0] < 0 && a.sa[1] < 0) return 1;
    return -1;
}

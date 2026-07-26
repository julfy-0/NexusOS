#include "mathutils.h"
#include <math.h>

long gcd(long a, long b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b != 0) {
        long t = b;
        b = a % b;
        a = t;
    }
    return a;
}

long lcm(long a, long b) {
    if (a == 0 || b == 0) return 0;
    long g = gcd(a, b);
    long result = (a / g) * b;
    return result < 0 ? -result : result;
}

int is_prime(long n) {
    if (n < 2) return 0;
    if (n == 2) return 1;
    if (n % 2 == 0) return 0;

    for (long i = 3; i * i <= n; i += 2) {
        if (n % i == 0) return 0;
    }
    return 1;
}

long pow_mod(long base, long exp, long mod) {
    if (mod == 1) return 0;

    long result = 1;
    base = base % mod;
    if (base < 0) base += mod;

    while (exp > 0) {
        if (exp & 1) {
            result = (result * base) % mod;
        }
        exp >>= 1;
        base = (base * base) % mod;
    }
    return result;
}

double mean(const double *values, size_t n) {
    if (!values || n == 0) return 0.0;

    double sum = 0.0;
    for (size_t i = 0; i < n; i++) sum += values[i];
    return sum / (double)n;
}

double stddev(const double *values, size_t n) {
    if (!values || n == 0) return 0.0;

    double m = mean(values, n);
    double sq_sum = 0.0;
    for (size_t i = 0; i < n; i++) {
        double diff = values[i] - m;
        sq_sum += diff * diff;
    }
    return sqrt(sq_sum / (double)n);
}

double min_value(const double *values, size_t n) {
    if (!values || n == 0) return 0.0;

    double m = values[0];
    for (size_t i = 1; i < n; i++) {
        if (values[i] < m) m = values[i];
    }
    return m;
}

double max_value(const double *values, size_t n) {
    if (!values || n == 0) return 0.0;

    double m = values[0];
    for (size_t i = 1; i < n; i++) {
        if (values[i] > m) m = values[i];
    }
    return m;
}

double clamp(double value, double lo, double hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

double lerp(double a, double b, double t) {
    return a + (b - a) * t;
}

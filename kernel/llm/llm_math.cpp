#include "llm_math.hpp"
#include <stdint.h>

namespace {
constexpr double LN2 = 0.6931471805599453094;
constexpr double PI_2 = 1.5707963267948966192;
inline double bits_to_double(uint64_t u) { double d; __builtin_memcpy(&d, &u, 8); return d; }
inline uint64_t double_to_bits(double d) { uint64_t u; __builtin_memcpy(&u, &d, 8); return u; }

// e^x for |x| <= ~708 via x = k*ln2 + r, |r| <= ln2/2, degree-11 Taylor polynomial of e^r.
double exp_d(double x) {
    if (x != x) return x;
    if (x > 700.0) return bits_to_double(0x7FF0000000000000ull);  // +inf
    if (x < -700.0) return 0.0;
    const double kd = x / LN2;
    const int64_t k = static_cast<int64_t>(kd < 0 ? kd - 0.5 : kd + 0.5);
    const double r = x - static_cast<double>(k) * LN2;
    double p = 1.0 / 39916800.0;  // 1/11!
    const double inv_fact[] = {1.0 / 3628800.0, 1.0 / 362880.0, 1.0 / 40320.0, 1.0 / 5040.0, 1.0 / 720.0,
                               1.0 / 120.0, 1.0 / 24.0, 1.0 / 6.0, 0.5, 1.0, 1.0};
    for (double c : inv_fact) p = p * r + c;
    return bits_to_double(double_to_bits(p) + (static_cast<uint64_t>(k) << 52));
}
// ln(x) for x > 0: x = m * 2^e, m in [sqrt(1/2), sqrt(2)); ln(m) = 2*atanh((m-1)/(m+1)).
double log_d(double x) {
    if (!(x > 0.0)) return x == 0.0 ? -bits_to_double(0x7FF0000000000000ull) : bits_to_double(0x7FF8000000000000ull);
    uint64_t u = double_to_bits(x);
    int64_t e = static_cast<int64_t>((u >> 52) & 0x7FF) - 1023;
    u = (u & 0x000FFFFFFFFFFFFFull) | 0x3FF0000000000000ull;
    double m = bits_to_double(u);
    if (m > 1.4142135623730951) { m *= 0.5; ++e; }
    const double s = (m - 1.0) / (m + 1.0), s2 = s * s;
    double t = 0.0;
    for (int n = 41; n >= 3; n -= 2) t = (t + 1.0 / n) * s2;
    return 2.0 * s * (1.0 + t) + static_cast<double>(e) * LN2;
}
// sin/cos of r in [-pi/4, pi/4] by Taylor series (double).
double sin_poly(double r) { const double r2 = r * r; double t = 0.0; const double f[] = {1.0 / 6227020800.0, -1.0 / 39916800.0, 1.0 / 362880.0, -1.0 / 5040.0, 1.0 / 120.0, -1.0 / 6.0};
    for (double c : f) t = (t + c) * r2; return r + r * t; }
double cos_poly(double r) { const double r2 = r * r; double t = 0.0; const double f[] = {1.0 / 479001600.0, -1.0 / 3628800.0, 1.0 / 40320.0, -1.0 / 720.0, 1.0 / 24.0, -0.5};
    for (double c : f) t = (t + c) * r2; return 1.0 + t; }
// Reduce x to quadrant q and r in [-pi/4, pi/4] (two-constant Cody-Waite; fine for |x| < 1e5).
void reduce(double x, int64_t& q, double& r) {
    const double kd = x / PI_2;
    q = static_cast<int64_t>(kd < 0 ? kd - 0.5 : kd + 0.5);
    constexpr double PI_2_HI = 1.5707963267341256, PI_2_LO = 6.07710050650619224932e-11;
    r = (x - static_cast<double>(q) * PI_2_HI) - static_cast<double>(q) * PI_2_LO;
}
}

extern "C" {
float pg_sqrtf(float x) { float r; asm("sqrtss %1, %0" : "=x"(r) : "x"(x)); return r; }
float pg_expf(float x) { return static_cast<float>(exp_d(x)); }
float pg_logf(float x) { return static_cast<float>(log_d(x)); }
float pg_powf(float base, float exponent) {
    if (exponent == 0.0f) return 1.0f;
    if (base == 1.0f) return 1.0f;
    if (base <= 0.0f) return base == 0.0f ? 0.0f : static_cast<float>(bits_to_double(0x7FF8000000000000ull));  // NaN: only positive bases are used
    return static_cast<float>(exp_d(static_cast<double>(exponent) * log_d(base)));
}
float pg_sinf(float x) {
    int64_t q; double r; reduce(x, q, r);
    switch (q & 3) { case 0: return float(sin_poly(r)); case 1: return float(cos_poly(r)); case 2: return float(-sin_poly(r)); default: return float(-cos_poly(r)); }
}
float pg_cosf(float x) {
    int64_t q; double r; reduce(x, q, r);
    switch (q & 3) { case 0: return float(cos_poly(r)); case 1: return float(-sin_poly(r)); case 2: return float(-cos_poly(r)); default: return float(sin_poly(r)); }
}
}

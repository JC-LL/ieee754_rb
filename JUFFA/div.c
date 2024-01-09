#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#define TEST_FP32_DIV     (0) /* 0: binary64 division; 1: binary32 division */
#define PURELY_RANDOM     (1)
#define PATTERN_BASED     (2)
#define TEST_MODE         (PATTERN_BASED)
#define ITO_TAKAGI_YAJIMA (1) /* more accurate recip. starting approximation */

uint32_t float_as_uint32 (float a)
{
    uint32_t r;
    memcpy (&r, &a, sizeof r);
    return r;
}

float uint32_as_float (uint32_t a)
{
    float r;
    memcpy (&r, &a, sizeof r);
    return r;
}

uint32_t umul32hi (uint32_t a, uint32_t b)
{
    return (uint32_t)(((uint64_t)a*b) >> 32);
}

int clz32 (uint32_t a)
{
    uint32_t r = 32;
    if (a >= 0x00010000) { a >>= 16; r -= 16; }
    if (a >= 0x00000100) { a >>=  8; r -=  8; }
    if (a >= 0x00000010) { a >>=  4; r -=  4; }
    if (a >= 0x00000004) { a >>=  2; r -=  2; }
    r -= a - (a & (a >> 1));
    return r;
}

#if ITO_TAKAGI_YAJIMA
/* Masayuki Ito, Naofumi Takagi, Shuzo Yajima, "Efficient Initial Approximation
   for Multiplicative Division and Square Root by a Multiplication with Operand
   Modification". IEEE Transactions on Computers, Vol. 46, No. 4, April 1997,
   pp. 495-498.
*/
#define LOG2_TAB_ENTRIES (6)
#define TAB_ENTRIES      (1 << LOG2_TAB_ENTRIES)
#define TAB_ENTRY_BITS   (16)
/* approx. err. ~= 5.146e-5 */
const uint16_t b1tab [64] =
{
    0xfc0f, 0xf46b, 0xed20, 0xe626, 0xdf7a, 0xd918, 0xd2fa, 0xcd1e,
    0xc77f, 0xc21b, 0xbcee, 0xb7f5, 0xb32e, 0xae96, 0xaa2a, 0xa5e9,
    0xa1d1, 0x9dde, 0x9a11, 0x9665, 0x92dc, 0x8f71, 0x8c25, 0x88f6,
    0x85e2, 0x82e8, 0x8008, 0x7d3f, 0x7a8e, 0x77f2, 0x756c, 0x72f9,
    0x709b, 0x6e4e, 0x6c14, 0x69eb, 0x67d2, 0x65c8, 0x63cf, 0x61e3,
    0x6006, 0x5e36, 0x5c73, 0x5abd, 0x5913, 0x5774, 0x55e1, 0x5458,
    0x52da, 0x5166, 0x4ffc, 0x4e9b, 0x4d43, 0x4bf3, 0x4aad, 0x496e,
    0x4837, 0x4708, 0x45e0, 0x44c0, 0x43a6, 0x4293, 0x4187, 0x4081
};
#else // ITO_TAKAGI_YAJIMA
#define LOG2_TAB_ENTRIES (7)
#define TAB_ENTRIES      (1 << LOG2_TAB_ENTRIES)
#define TAB_ENTRY_BITS   (8)
/* approx. err. ~= 5.585e-3 */
const uint8_t rcp_tab [TAB_ENTRIES] =
{
    0xff, 0xfd, 0xfb, 0xf9, 0xf7, 0xf5, 0xf4, 0xf2,
    0xf0, 0xee, 0xed, 0xeb, 0xe9, 0xe8, 0xe6, 0xe4,
    0xe3, 0xe1, 0xe0, 0xde, 0xdd, 0xdb, 0xda, 0xd8,
    0xd7, 0xd5, 0xd4, 0xd3, 0xd1, 0xd0, 0xcf, 0xcd,
    0xcc, 0xcb, 0xca, 0xc8, 0xc7, 0xc6, 0xc5, 0xc4,
    0xc2, 0xc1, 0xc0, 0xbf, 0xbe, 0xbd, 0xbc, 0xbb,
    0xba, 0xb9, 0xb8, 0xb7, 0xb6, 0xb5, 0xb4, 0xb3,
    0xb2, 0xb1, 0xb0, 0xaf, 0xae, 0xad, 0xac, 0xab,
    0xaa, 0xa9, 0xa8, 0xa8, 0xa7, 0xa6, 0xa5, 0xa4,
    0xa3, 0xa3, 0xa2, 0xa1, 0xa0, 0x9f, 0x9f, 0x9e,
    0x9d, 0x9c, 0x9c, 0x9b, 0x9a, 0x99, 0x99, 0x98,
    0x97, 0x97, 0x96, 0x95, 0x95, 0x94, 0x93, 0x93,
    0x92, 0x91, 0x91, 0x90, 0x8f, 0x8f, 0x8e, 0x8e,
    0x8d, 0x8c, 0x8c, 0x8b, 0x8b, 0x8a, 0x89, 0x89,
    0x88, 0x88, 0x87, 0x87, 0x86, 0x85, 0x85, 0x84,
    0x84, 0x83, 0x83, 0x82, 0x82, 0x81, 0x81, 0x80
};
#endif // ITO_TAKAGI_YAJIMA

#define FP32_MANT_BITS       (23)
#define FP32_EXPO_BITS       (8)
#define FP32_SIGN_MASK       (0x80000000u)
#define FP32_MANT_MASK       (0x007fffffu)
#define FP32_EXPO_MASK       (0x7f800000u)
#define FP32_MAX_EXPO        (0xff)
#define FP32_EXPO_BIAS       (127)
#define FP32_INFTY           (0x7f800000u)
#define FP32_QNAN_BIT        (0x00400000u)
#define FP32_QNAN_INDEFINITE (0xffc00000u)
#define FP32_MANT_INT_BIT    (0x00800000u)

uint32_t fp32_div_core (uint32_t x, uint32_t y)
{
    uint32_t expo_x, expo_y, expo_r, sign_r;
    uint32_t abs_x, abs_y, f, l, p, r, z, s;
    int x_is_zero, y_is_zero, normalization_shift;

    expo_x = (x & FP32_EXPO_MASK) >> FP32_MANT_BITS;
    expo_y = (y & FP32_EXPO_MASK) >> FP32_MANT_BITS;
    sign_r = (x ^ y) & FP32_SIGN_MASK;

    abs_x = x & ~FP32_SIGN_MASK;
    abs_y = y & ~FP32_SIGN_MASK;
    x_is_zero = (abs_x == 0);
    y_is_zero = (abs_y == 0);

    if ((expo_x == FP32_MAX_EXPO) || (expo_y == FP32_MAX_EXPO) ||
        x_is_zero || y_is_zero) {
        int x_is_nan = (abs_x >  FP32_INFTY);
        int x_is_inf = (abs_x == FP32_INFTY);
        int y_is_nan = (abs_y >  FP32_INFTY);
        int y_is_inf = (abs_y == FP32_INFTY);
        if (x_is_nan) {
            r = x | FP32_QNAN_BIT;
        } else if (y_is_nan) {
            r = y | FP32_QNAN_BIT;
        } else if ((x_is_zero && y_is_zero) || (x_is_inf && y_is_inf)) {
            r = FP32_QNAN_INDEFINITE;
        } else if (x_is_zero || y_is_inf) {
            r = sign_r;
        } else if (y_is_zero || x_is_inf) {
            r = sign_r | FP32_INFTY;
        }
    } else {
        /* normalize any subnormals */
        if (expo_x == 0) {
            s = clz32 (abs_x) - FP32_EXPO_BITS;
            x = x << s;
            expo_x = expo_x - (s - 1);
        }
        if (expo_y == 0) {
            s = clz32 (abs_y) - FP32_EXPO_BITS;
            y = y << s;
            expo_y = expo_y - (s - 1);
        }
        //
        expo_r = expo_x - expo_y + FP32_EXPO_BIAS;
        /* extract mantissas */
        x = x & FP32_MANT_MASK;
        y = y & FP32_MANT_MASK;
#if ITO_TAKAGI_YAJIMA
        /* initial approx based on 6 most significant stored mantissa bits */
        r = b1tab [y >> (FP32_MANT_BITS - LOG2_TAB_ENTRIES)];
        /* make implicit integer bit of mantissa explicit */
        x = x | FP32_MANT_INT_BIT;
        y = y | FP32_MANT_INT_BIT;
        r = r * ((y ^ ((1u << (FP32_MANT_BITS - LOG2_TAB_ENTRIES)) - 1)) >>
                 (FP32_MANT_BITS + 1 + TAB_ENTRY_BITS - 32));
        /* pre-scale y for more efficient fixed-point computation */
        z = y << FP32_EXPO_BITS;
#else // ITO_TAKAGI_YAJIMA
        /* initial approx based on 7 most significant stored mantissa bits */
        r = rcp_tab [y >> (FP32_MANT_BITS - LOG2_TAB_ENTRIES)];
        /* make implicit integer bit of mantissa explicit */
        x = x | FP32_MANT_INT_BIT;
        y = y | FP32_MANT_INT_BIT;
        /* pre-scale y for more efficient fixed-point computation */
        z = y << FP32_EXPO_BITS;
        /* first NR iteration r1 = 2*r0-y*r0*r0 */
        f = r * r;
        p = umul32hi (z, f << (32 - 2 * TAB_ENTRY_BITS));
        r = (r << (32 - TAB_ENTRY_BITS)) - p;
#endif // ITO_TAKAGI_YAJIMA
        /* second NR iteration: r2 = r1*(2-y*r1) */
        p = umul32hi (z, r << 1);
        f = 0u - p;
        r = umul32hi (f, r << 1);
        /* compute quotient as wide product x*(1/y) = x*r */
        l = x * (r << 1);
        r = umul32hi (x, r << 1);
        /* normalize mantissa to be in [1,2) */
        normalization_shift = (r & FP32_MANT_INT_BIT) == 0;
        expo_r -= normalization_shift;
        r = (r << normalization_shift) | ((l >> 1) >> (32 - 1 - normalization_shift));
        if ((expo_r > 0) && (expo_r < FP32_MAX_EXPO)) { /* result is normal */
            int32_t rem_raw, rem_inc, inc;
            /* align x with product y*quotient */
            x = x << (FP32_MANT_BITS + normalization_shift + 1);
            /* compute product y*quotient */
            y = y << 1;
            p = y * r;
            /* compute x - y*quotient, for both raw and incremented quotient */
            rem_raw = x - p;
            rem_inc = rem_raw - y;
            /* round to nearest: tie case _cannot_ occur */
            inc = abs (rem_inc) < abs (rem_raw);
            /* build final results from sign, exponent, mantissa */
            r = sign_r | (((expo_r - 1) << FP32_MANT_BITS) + r + inc);
        } else if ((int)expo_r >= FP32_MAX_EXPO) { /* result overflowed */
            r = sign_r | FP32_INFTY;
        } else { /* result underflowed */
            int denorm_shift = 1 - expo_r;
            if (denorm_shift > (FP32_MANT_BITS + 1)) { /* massive underflow */
                r = sign_r;
            } else {
                int32_t rem_raw, rem_inc, inc;
                /* denormalize quotient */
                r = r >> denorm_shift;
                /* align x with product y*quotient */
                x = x << (FP32_MANT_BITS + normalization_shift + 1 - denorm_shift);
                /* compute product y*quotient */
                y = y << 1;
                p = y * r;
                /* compute x - y*quotient, for both raw & incremented quotient*/
                rem_raw = x - p;
                rem_inc = rem_raw - y;
                /* round to nearest or even: tie case _can_ occur */
                inc = ((abs (rem_inc) < abs (rem_raw)) ||
                       (abs (rem_inc) == abs (rem_raw) && (r & 1)));
                /* build final result from sign and mantissa */
                r = sign_r | (r + inc);
            }
        }
    }
    return r;
}

float fp32_div (float a, float b)
{
    uint32_t x, y, r;
    x = float_as_uint32 (a);
    y = float_as_uint32 (b);
    r = fp32_div_core (x, y);
    return uint32_as_float (r);
}

uint64_t double_as_uint64 (double a)
{
    uint64_t r;
    memcpy (&r, &a, sizeof r);
    return r;
}

double uint64_as_double (uint64_t a)
{
    double r;
    memcpy (&r, &a, sizeof r);
    return r;
}

uint64_t umul64hi (uint64_t a, uint64_t b)
{
    uint64_t a_lo = (uint64_t)(uint32_t)a;
    uint64_t a_hi = a >> 32;
    uint64_t b_lo = (uint64_t)(uint32_t)b;
    uint64_t b_hi = b >> 32;
    uint64_t p0 = a_lo * b_lo;
    uint64_t p1 = a_lo * b_hi;
    uint64_t p2 = a_hi * b_lo;
    uint64_t p3 = a_hi * b_hi;
    uint32_t cy = (uint32_t)(((p0 >> 32) + (uint32_t)p1 + (uint32_t)p2) >> 32);
    return p3 + (p1 >> 32) + (p2 >> 32) + cy;
}

int clz64 (uint64_t a)
{
    uint64_t r = 64;
    if (a >= 0x100000000ULL) { a >>= 32; r -= 32; }
    if (a >= 0x000010000ULL) { a >>= 16; r -= 16; }
    if (a >= 0x000000100ULL) { a >>=  8; r -=  8; }
    if (a >= 0x000000010ULL) { a >>=  4; r -=  4; }
    if (a >= 0x000000004ULL) { a >>=  2; r -=  2; }
    r -= a - (a & (a >> 1));
    return r;
}


#define FP64_MANT_BITS       (52)
#define FP64_EXPO_BITS       (11)
#define FP64_EXPO_MASK       (0x7ff0000000000000ULL)
#define FP64_SIGN_MASK       (0x8000000000000000ULL)
#define FP64_MANT_MASK       (0x000fffffffffffffULL)
#define FP64_MAX_EXPO        (0x7ff)
#define FP64_EXPO_BIAS       (1023)
#define FP64_INFTY           (0x7ff0000000000000ULL)
#define FP64_QNAN_BIT        (0x0008000000000000ULL)
#define FP64_QNAN_INDEFINITE (0xfff8000000000000ULL)
#define FP64_MANT_INT_BIT    (0x0010000000000000ULL)

uint64_t fp64_div_core (uint64_t x, uint64_t y)
{
    uint64_t expo_x, expo_y, expo_r, sign_r;
    uint64_t abs_x, abs_y, f, l, p, r, z, s;
    int x_is_zero, y_is_zero, normalization_shift;

    expo_x = (x & FP64_EXPO_MASK) >> FP64_MANT_BITS;
    expo_y = (y & FP64_EXPO_MASK) >> FP64_MANT_BITS;
    sign_r = (x ^ y) & FP64_SIGN_MASK;

    abs_x = x & ~FP64_SIGN_MASK;
    abs_y = y & ~FP64_SIGN_MASK;
    x_is_zero = (abs_x == 0);
    y_is_zero = (abs_y == 0);

    if ((expo_x == FP64_MAX_EXPO) || (expo_y == FP64_MAX_EXPO) ||
        x_is_zero || y_is_zero) {
        int x_is_nan = (abs_x >  FP64_INFTY);
        int x_is_inf = (abs_x == FP64_INFTY);
        int y_is_nan = (abs_y >  FP64_INFTY);
        int y_is_inf = (abs_y == FP64_INFTY);
        if (x_is_nan) {
            r = x | FP64_QNAN_BIT;
        } else if (y_is_nan) {
            r = y | FP64_QNAN_BIT;
        } else if ((x_is_zero && y_is_zero) || (x_is_inf && y_is_inf)) {
            r = FP64_QNAN_INDEFINITE;
        } else if (x_is_zero || y_is_inf) {
            r = sign_r;
        } else if (y_is_zero || x_is_inf) {
            r = sign_r | FP64_INFTY;
        }
    } else {
        /* normalize any subnormals */
        if (expo_x == 0) {
            s = clz64 (abs_x) - FP64_EXPO_BITS;
            x = x << s;
            expo_x = expo_x - (s - 1);
        }
        if (expo_y == 0) {
            s = clz64 (abs_y) - FP64_EXPO_BITS;
            y = y << s;
            expo_y = expo_y - (s - 1);
        }
        //
        expo_r = expo_x - expo_y + FP64_EXPO_BIAS;
        /* extract mantissas */
        x = x & FP64_MANT_MASK;
        y = y & FP64_MANT_MASK;
#if ITO_TAKAGI_YAJIMA
        /* initial approx based on 6 most significant stored mantissa bits */
        r = b1tab [y >> (FP64_MANT_BITS - LOG2_TAB_ENTRIES)];
        /* make implicit integer bit of mantissa explicit */
        x = x | FP64_MANT_INT_BIT;
        y = y | FP64_MANT_INT_BIT;
        r = r * ((y ^ ((1ULL << (FP64_MANT_BITS - LOG2_TAB_ENTRIES)) - 1)) >>
                 (FP64_MANT_BITS + 1 + TAB_ENTRY_BITS - 64));
        /* pre-scale y for more efficient fixed-point computation */
        z = y << FP64_EXPO_BITS;
#else // ITO_TAKAGI_YAJIMA
        /* initial approx based on 7 most significant stored mantissa bits */
        r = rcp_tab [y >> (FP64_MANT_BITS - LOG2_TAB_ENTRIES)];
        /* make implicit integer bit of mantissa explicit */
        x = x | FP64_MANT_INT_BIT;
        y = y | FP64_MANT_INT_BIT;
        /* pre-scale y for more efficient fixed-point computation */
        z = y << FP64_EXPO_BITS;
        /* first NR iteration r1 = 2*r0-y*r0*r0 */
        f = r * r;
        p = umul64hi (z, f << (64 - 2 * TAB_ENTRY_BITS));
        r = (r << (64 - TAB_ENTRY_BITS)) - p;
#endif // ITO_TAKAGI_YAJIMA
        /* second NR iteration: r2 = r1*(2-y*r1) */
        p = umul64hi (z, r << 1);
        f = 0u - p;
        r = umul64hi (f, r << 1);
        /* third NR iteration: r3 = r2*(2-y*r2) */
        p = umul64hi (z, r << 1);
        f = 0u - p;
        r = umul64hi (f, r << 1);
        /* compute quotient as wide product x*(1/y) = x*r */
        l = x * (r << 1);
        r = umul64hi (x, r << 1);
        /* normalize mantissa to be in [1,2) */
        normalization_shift = (r & FP64_MANT_INT_BIT) == 0;
        expo_r -= normalization_shift;
        r = (r << normalization_shift) | ((l >> 1) >> (64 - 1 - normalization_shift));
        if ((expo_r > 0) && (expo_r < FP64_MAX_EXPO)) { /* result is normal */
            int64_t rem_raw, rem_inc;
            int inc;
            /* align x with product y*quotient */
            x = x << (FP64_MANT_BITS + 1 + normalization_shift);
            /* compute product y*quotient */
            y = y << 1;
            p = y * r;
            /* compute x - y*quotient, for both raw and incremented quotient */
            rem_raw = x - p;
            rem_inc = rem_raw - y;
            /* round to nearest: tie case _cannot_ occur */
            inc = llabs (rem_inc) < llabs (rem_raw);
            /* build final results from sign, exponent, mantissa */
            r = sign_r | (((expo_r - 1) << FP64_MANT_BITS) + r + inc);
        } else if ((int)expo_r >= FP64_MAX_EXPO) { /* result overflowed */
            r = sign_r | FP64_INFTY;
        } else { /* result underflowed */
            int denorm_shift = 1 - expo_r;
            if (denorm_shift > (FP64_MANT_BITS + 1)) { /* massive underflow */
                r = sign_r;
            } else {
                int64_t rem_raw, rem_inc;
                int inc;
                /* denormalize quotient */
                r = r >> denorm_shift;
                /* align x with product y*quotient */
                x = x << (FP64_MANT_BITS + 1 + normalization_shift - denorm_shift);
                /* compute product y*quotient */
                y = y << 1;
                p = y * r;
                /* compute x - y*quotient, for both raw & incremented quotient*/
                rem_raw = x - p;
                rem_inc = rem_raw - y;
                /* round to nearest or even: tie case _can_ occur */
                inc = ((llabs (rem_inc) < llabs (rem_raw)) ||
                       (llabs (rem_inc) == llabs (rem_raw) && (r & 1)));
                /* build final result from sign and mantissa */
                r = sign_r | (r + inc);
            }
        }
    }
    return r;
}

double fp64_div (double a, double b)
{
    uint64_t x, y, r;
    x = double_as_uint64 (a);
    y = double_as_uint64 (b);
    r = fp64_div_core (x, y);
    return uint64_as_double (r);
}

#if TEST_FP32_DIV

// Fixes via: Greg Rose, KISS: A Bit Too Simple. http://eprint.iacr.org/2011/007
static uint32_t kiss_z=362436069,kiss_w=521288629, kiss_jsr=362436069,kiss_jcong=123456789;
#define znew (kiss_z=36969*(kiss_z&0xffff)+(kiss_z>>16))
#define wnew (kiss_w=18000*(kiss_w&0xffff)+(kiss_w>>16))
#define MWC  ((znew<<16)+wnew)
#define SHR3 (kiss_jsr^=(kiss_jsr<<13),kiss_jsr^=(kiss_jsr>>17),kiss_jsr^=(kiss_jsr<<5))
#define CONG (kiss_jcong=69069*kiss_jcong+13579)
#define KISS ((MWC^CONG)+SHR3)

uint32_t v[8192];

int main (void)
{
    uint64_t count = 0;
    float a, b, res, ref;
    uint32_t ires, iref, diff;
    uint32_t i, j, patterns, idx = 0, nbrBits = sizeof (v[0]) * CHAR_BIT;

    printf ("testing fp32 division\n");

    /* pattern class 1: 2**i */
    for (i = 0; i < nbrBits; i++) {
        v [idx] = ((uint32_t)1 << i);
        idx++;
    }
    /* pattern class 2: 2**i-1 */
    for (i = 0; i < nbrBits; i++) {
        v [idx] = (((uint32_t)1 << i) - 1);
        idx++;
    }
    /* pattern class 3: 2**i+1 */
    for (i = 0; i < nbrBits; i++) {
        v [idx] = (((uint32_t)1 << i) + 1);
        idx++;
    }
    /* pattern class 4: 2**i + 2**j */
    for (i = 0; i < nbrBits; i++) {
        for (j = 0; j < nbrBits; j++) {
            v [idx] = (((uint32_t)1 << i) + ((uint32_t)1 << j));
            idx++;
        }
    }
    /* pattern class 5: 2**i - 2**j */
    for (i = 0; i < nbrBits; i++) {
        for (j = 0; j < nbrBits; j++) {
            v [idx] = (((uint32_t)1 << i) - ((uint32_t)1 << j));
            idx++;
        }
    }
    /* pattern class 6: MAX_UINT/(2**i+1) rep. blocks of i zeros an i ones */
    for (i = 0; i < nbrBits; i++) {
        v [idx] = ((~(uint32_t)0) / (((uint32_t)1 << i) + 1));
        idx++;
    }
    patterns = idx;
    /* pattern class 6: one's complement of pattern classes 1 through 5 */
    for (i = 0; i < patterns; i++) {
        v [idx] = ~v [i];
        idx++;
    }
    /* pattern class 7: two's complement of pattern classes 1 through 5 */
    for (i = 0; i < patterns; i++) {
        v [idx] = ~v [i] + 1;
        idx++;
    }
    patterns = idx;

#if ITO_TAKAGI_YAJIMA
    printf ("initial reciprocal based on method of Ito, Takagi, and Yajima\n");
#else
    printf ("initial reciprocal based on straight 8-bit table\n");
#endif
#if TEST_MODE == PURELY_RANDOM
    printf ("using purely random test vectors\n");
#elif TEST_MODE == PATTERN_BASED
    printf ("using pattern-based test vectors\n");
    printf ("#patterns = %u\n", patterns);
#endif // TEST_MODE

    do {
#if TEST_MODE == PURELY_RANDOM
        a = uint32_as_float (KISS);
        b = uint32_as_float (KISS);
#elif TEST_MODE == PATTERN_BASED
        a = uint32_as_float ((v [KISS % patterns] & FP32_MANT_MASK) | (KISS & ~FP32_MANT_MASK));
        b = uint32_as_float ((v [KISS % patterns] & FP32_MANT_MASK) | (KISS & ~FP32_MANT_MASK));
#endif // TEST_MODE
        ref = a / b;
        res = fp32_div (a, b);
        ires = float_as_uint32 (res);
        iref = float_as_uint32 (ref);

        diff = (ires > iref) ? (ires - iref) : (iref - ires);
        if (diff) {
            printf ("a=% 15.6a  b=% 15.6a  res=% 15.6a  ref=% 15.6a\n", a, b, res, ref);
            return EXIT_FAILURE;
        }

        count++;
        if ((count & 0xffffff) == 0) {
            printf ("\r%llu", count);
        }
    } while (1);
    return EXIT_SUCCESS;
}

#else /* TEST_FP32_DIV */

/*
  https://groups.google.com/forum/#!original/comp.lang.c/qFv18ql_WlU/IK8KGZZFJx4J
*/
static uint64_t kiss64_x = 1234567890987654321ULL;
static uint64_t kiss64_c = 123456123456123456ULL;
static uint64_t kiss64_y = 362436362436362436ULL;
static uint64_t kiss64_z = 1066149217761810ULL;
static uint64_t kiss64_t;

#define MWC64  (kiss64_t = (kiss64_x << 58) + kiss64_c, \
                kiss64_c = (kiss64_x >> 6), kiss64_x += kiss64_t, \
                kiss64_c += (kiss64_x < kiss64_t), kiss64_x)
#define XSH64  (kiss64_y ^= (kiss64_y << 13), kiss64_y ^= (kiss64_y >> 17), \
                kiss64_y ^= (kiss64_y << 43))
#define CNG64  (kiss64_z = 6906969069ULL * kiss64_z + 1234567ULL)
#define KISS64 (MWC64 + XSH64 + CNG64)

uint64_t v[32768];

int main (void)
{
    uint64_t ires, iref, diff, count = 0;
    double a, b, res, ref;
    uint32_t i, j, patterns, idx = 0, nbrBits = sizeof (v[0]) * CHAR_BIT;

    printf ("testing fp64 division\n");

    /* pattern class 1: 2**i */
    for (i = 0; i < nbrBits; i++) {
        v [idx] = ((uint64_t)1 << i);
        idx++;
    }
    /* pattern class 2: 2**i-1 */
    for (i = 0; i < nbrBits; i++) {
        v [idx] = (((uint64_t)1 << i) - 1);
        idx++;
    }
    /* pattern class 3: 2**i+1 */
    for (i = 0; i < nbrBits; i++) {
        v [idx] = (((uint64_t)1 << i) + 1);
        idx++;
    }
    /* pattern class 4: 2**i + 2**j */
    for (i = 0; i < nbrBits; i++) {
        for (j = 0; j < nbrBits; j++) {
            v [idx] = (((uint64_t)1 << i) + ((uint64_t)1 << j));
            idx++;
        }
    }
    /* pattern class 5: 2**i - 2**j */
    for (i = 0; i < nbrBits; i++) {
        for (j = 0; j < nbrBits; j++) {
            v [idx] = (((uint64_t)1 << i) - ((uint64_t)1 << j));
            idx++;
        }
    }
    /* pattern class 6: MAX_UINT/(2**i+1) rep. blocks of i zeros an i ones */
    for (i = 0; i < nbrBits; i++) {
        v [idx] = ((~(uint64_t)0) / (((uint64_t)1 << i) + 1));
        idx++;
    }
    patterns = idx;
    /* pattern class 6: one's complement of pattern classes 1 through 5 */
    for (i = 0; i < patterns; i++) {
        v [idx] = ~v [i];
        idx++;
    }
    /* pattern class 7: two's complement of pattern classes 1 through 5 */
    for (i = 0; i < patterns; i++) {
        v [idx] = ~v [i] + 1;
        idx++;
    }
    patterns = idx;

#if ITO_TAKAGI_YAJIMA
    printf ("initial reciprocal based on method of Ito, Takagi, and Yajima\n");
#else
    printf ("initial reciprocal based on straight 8-bit table\n");
#endif
#if TEST_MODE == PURELY_RANDOM
    printf ("using purely random test vectors\n");
#elif TEST_MODE == PATTERN_BASED
    printf ("using pattern-based test vectors\n");
    printf ("#patterns = %u\n", patterns);
#endif // TEST_MODE

    do {
#if TEST_MODE == PURELY_RANDOM
        a = uint64_as_double (KISS64);
        b = uint64_as_double (KISS64);
#elif TEST_MODE == PATTERN_BASED
        a = uint64_as_double ((v [KISS64 % patterns] & FP64_MANT_MASK) | (KISS64 & ~FP64_MANT_MASK));
        b = uint64_as_double ((v [KISS64 % patterns] & FP64_MANT_MASK) | (KISS64 & ~FP64_MANT_MASK));
#endif // TEST_MODE

        ref = a / b;
        res = fp64_div (a, b);
        ires = double_as_uint64(res);
        iref = double_as_uint64(ref);

        diff = (ires > iref) ? (ires - iref) : (iref - ires);
        if (diff) {
            printf ("a=% 23.13a  b=% 23.13a  res=% 23.13a  ref=% 23.13a\n", a, b, res, ref);
            return EXIT_FAILURE;
        }

        count++;
        if ((count & 0xffffff) == 0) {
            printf ("\r%llu", count);
        }
    } while (1);
    return EXIT_SUCCESS;
}
#endif /* TEST_FP32_DIV */

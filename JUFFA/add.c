#include <cstdint>
#include <cstring>

#define RND_VARIANT (3)  // 1 ... 3
#define CLZ_VARIANT (4)  // 1 ... 5

int clz (uint32_t a);

uint32_t fp32_add(uint32_t a, uint32_t b)
{
    /* sort arguments myuint32_t fp32_add_rn_kernel (uint32_t a, uint32_t b) magnitude. Larger one becomes a, smaller becomes b */
    if ((b << 1) > (a << 1)) {
        uint32_t t = a;
        a = b;
        b = t;
    }

    uint32_t expo_a_m1 = ((a >> 23) & 0xff) - 1;
    uint32_t expo_b_m1 = ((b >> 23) & 0xff) - 1;

    if ((expo_a_m1 >= 0xfe) || (expo_b_m1 >= 0xfe)) {
        uint32_t am = a << 1;
        uint32_t bm = b << 1;
        if (bm == 0) {                // b is zero
            if (am == 0) a = a & b;   // a is also zero
            if (am > 0xff000000) a = a | 0x00400000; // a is NaN, quieten it
            return a;
        } else if ((expo_b_m1 > 0xfe) &&  // b is subnormal
                   (expo_a_m1 != 0xfe)) { // a is not NaN or INF
            int shift = clz (bm) - 8;
            bm = bm << shift;
            expo_b_m1 = ~shift;
            b = (b & 0x80000000) | bm;
            if (expo_a_m1 > 0xfe) { // a is subnormal
                int shift = clz (am) - 8;
                am = am << shift;
                expo_a_m1 = ~shift;
                a = (a & 0x80000000) | am;
            }
        } else if (am > 0xff000000) {     // a is a NaN
            return a | 0x00400000;
        } else if (bm > 0xff000000) {     // b is a NaN
            return b | 0x00400000;
        } else if ((am & bm) == 0xff000000) { // a is INF and b is INF
            return a | ((a ^ b) ? 0xffc00000 : 0);
        } else if ((am == 0) || (bm == 0xff000000)) { // a is zero or b is INF
            return b;
        } else if ((bm == 0) || (am == 0xff000000)) { // b is zero or a is INF
            return a;
        }
    }
    uint32_t expo_diff = expo_a_m1 - expo_b_m1;
    uint32_t sign_a = a & 0x80000000;
    uint32_t r;
    if (expo_diff <= 25) {
        uint32_t mant_a = (a & 0x00ffffff) | 0x00800000; // add in implict bit
        uint32_t mant_b = (b & 0x00ffffff) | 0x00800000; // add in implict bit
        uint32_t rndstk = expo_diff ? (mant_b << (32 - expo_diff)) : 0;
        if ((int32_t)(a ^ b) < 0) { // effective subtraction
            rndstk = 0 - rndstk;
            mant_a = mant_a - (mant_b >> expo_diff) - (rndstk != 0);
            if (!(mant_a & 0x00800000)) { /* cancellation of leading bits */
                if ((mant_a | rndstk) == 0) {
                    return mant_a; /* addends cancel completely */
                }
                /* renormalize */
                uint32_t shift = clz (mant_a) - 8;
                mant_a = (mant_a << shift) | (rndstk >> (32 - shift));
                rndstk <<= shift;
                expo_a_m1 -= shift;
            }
        } else {
            mant_a = mant_a + (mant_b >> expo_diff);
            if (mant_a & 0x01000000) { // significand overflow, renormalize
                rndstk = (rndstk >> 1) | (mant_a << 31);
                mant_a = mant_a >> 1;
                expo_a_m1 += 1;
            }
        }
        if (expo_a_m1 < 0xfe) {
            /* normal: round to nearest or even */
#if RND_VARIANT == 1
            r = (expo_a_m1 << 23) + mant_a +
                ((rndstk | (mant_a & 1)) > 0x80000000);
#elif RND_VARIANT == 2
            r = (expo_a_m1 << 23) + mant_a + (rndstk >= 0x80000000);
            if (rndstk == 0x80000000) r = r & ~1; // tie case
#elif RND_VARIANT == 3
            r = (expo_a_m1 << 23) + mant_a +
                ((rndstk == 0x80000000) ? (mant_a & 1) : (rndstk >> 31));
#else // RND_VARIANT
#error unsupported RND_VARIANT
#endif // RND_VARIANT
        } else if (expo_a_m1 > 0xfe) { // underflow
            int denorm_shift = 0 - expo_a_m1;
            r = mant_a >> denorm_shift;
        } else { // overflow
            r = 0x7f800000;
        }
    } else {
        r = a;
    }
    r = sign_a | r;
    return r;
}

int clz (uint32_t a)
{
#if CLZ_VARIANT == 1
    static const uint8_t clz_tab[32] = {
        31, 22, 30, 21, 18, 10, 29,  2, 20, 17, 15, 13, 9,  6, 28, 1,
        23, 19, 11,  3, 16, 14,  7, 24, 12,  4,  8, 25, 5, 26, 27, 0
    };
    a |= a >> 16;
    a |= a >> 8;
    a |= a >> 4;
    a |= a >> 2;
    a |= a >> 1;
    return clz_tab [0x07c4acddu * a >> 27] + (!a);
#elif CLZ_VARIANT == 2
    uint32_t b;
    int n = 31 + (!a);
    if ((b = (a & 0xffff0000u))) { n -= 16;  a = b; }
    if ((b = (a & 0xff00ff00u))) { n -=  8;  a = b; }
    if ((b = (a & 0xf0f0f0f0u))) { n -=  4;  a = b; }
    if ((b = (a & 0xccccccccu))) { n -=  2;  a = b; }
    if ((    (a & 0xaaaaaaaau))) { n -=  1;         }
    return n;
#elif CLZ_VARIANT == 3
    int n = 0;
    if (!(a & 0xffff0000u)) { n |= 16;  a <<= 16; }
    if (!(a & 0xff000000u)) { n |=  8;  a <<=  8; }
    if (!(a & 0xf0000000u)) { n |=  4;  a <<=  4; }
    if (!(a & 0xc0000000u)) { n |=  2;  a <<=  2; }
    if ((int32_t)a >= 0) n++;
    if ((int32_t)a == 0) n++;
    return n;
#elif CLZ_VARIANT == 4
    uint32_t b;
    int n = 32;
    if ((b = (a >> 16))) { n = n - 16;  a = b; }
    if ((b = (a >>  8))) { n = n -  8;  a = b; }
    if ((b = (a >>  4))) { n = n -  4;  a = b; }
    if ((b = (a >>  2))) { n = n -  2;  a = b; }
    if ((b = (a >>  1))) return n - 2;
    return n - a;
#elif CLZ_VARIANT == 5
    return __builtin_clz (a);
#else
#error unknown CLZ_VARIANT
#endif
}

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

float fp32_add_rn (float a, float b)
{
    uint32_t ai, bi;

    ai = float_as_uint32 (a);
    bi = float_as_uint32 (b);
    return uint32_as_float (fp32_add_rn_kernel (ai, bi));
}

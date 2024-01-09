#define RND_VARIANT (3)     // 1 ... 3

#define FLOAT_MANT_BITS    (23)
#define FLOAT_EXPO_BITS    (8)
#define FLOAT_EXPO_BIAS    (127)
#define FLOAT_MANT_MASK    (~((~0u) << (FLOAT_MANT_BITS+1))) /* incl. integer bit */
#define EXPO_ADJUST        (1)   /* adjustment for performance reasons */
#define MIN_NORM_EXPO      (1)   /* minimum biased exponent of normals */
#define MAX_NORM_EXPO      (254) /* maximum biased exponent of normals */
#define INF_EXPO           (255) /* biased exponent of infinities */
#define EXPO_MASK          (~((~0u) << FLOAT_EXPO_BITS))
#define FLOAT_SIGN_MASK    (0x80000000u)
#define FLOAT_IMPLICIT_BIT (1 << FLOAT_MANT_BITS)
#define RND_BIT_SHIFT      (31)
#define RND_BIT_MASK       (1 << RND_BIT_SHIFT)
#define FLOAT_INFINITY     (0x7f800000)
#define FLOAT_INDEFINITE   (0xffc00000u)
#define MANT_LSB           (0x00000001)
#define FLOAT_QNAN_BIT     (0x00400000)
#define MAX_SHIFT          (FLOAT_MANT_BITS + 2)

uint32_t fp32_mul_core (uint32_t a, uint32_t b)
{
    uint64_t prod;
    uint32_t expoa, expob, manta, mantb, shift;
    uint32_t r, signr, expor, mantr_hi, mantr_lo;

    /* split arguments into sign, exponent, significand */
    expoa = ((a >> FLOAT_MANT_BITS) & EXPO_MASK) - EXPO_ADJUST;
    expob = ((b >> FLOAT_MANT_BITS) & EXPO_MASK) - EXPO_ADJUST;
    manta = (a | FLOAT_IMPLICIT_BIT) & FLOAT_MANT_MASK;
    mantb = (b | FLOAT_IMPLICIT_BIT) & FLOAT_MANT_MASK;
    /* result sign bit: XOR sign argument signs */
    signr = (a ^ b) & FLOAT_SIGN_MASK;
    if ((expoa >= (MAX_NORM_EXPO - EXPO_ADJUST)) || /* at least one argument is special */
        (expob >= (MAX_NORM_EXPO - EXPO_ADJUST))) {
        if ((a & ~FLOAT_SIGN_MASK) > FLOAT_INFINITY) { /* a is NaN */
            /* return quietened NaN */
            return a | FLOAT_QNAN_BIT;
        }
        if ((b & ~FLOAT_SIGN_MASK) > FLOAT_INFINITY) { /* b is NaN */
            /* return quietened NaN */
            return b | FLOAT_QNAN_BIT;
        }
        if ((a & ~FLOAT_SIGN_MASK) == 0) { /* a is zero */
            /* return NaN if b is infinity, else zero */
            return (expob != (INF_EXPO - EXPO_ADJUST)) ? signr : FLOAT_INDEFINITE;
        }
        if ((b & ~FLOAT_SIGN_MASK) == 0) { /* b is zero */
            /* return NaN if a is infinity, else zero */
            return (expoa != (INF_EXPO - EXPO_ADJUST)) ? signr : FLOAT_INDEFINITE;
        }
        if (((a & ~FLOAT_SIGN_MASK) == FLOAT_INFINITY) || /* a or b infinity */
            ((b & ~FLOAT_SIGN_MASK) == FLOAT_INFINITY)) {
            return signr | FLOAT_INFINITY;
        }
        if ((int32_t)expoa < (MIN_NORM_EXPO - EXPO_ADJUST)) { /* a is subnormal */
            /* normalize significand of a */
            manta = a & FLOAT_MANT_MASK;
            expoa++;
            do {
                manta = 2 * manta;
                expoa--;
            } while (manta < FLOAT_IMPLICIT_BIT);
        } else if ((int32_t)expob < (MIN_NORM_EXPO - EXPO_ADJUST)) { /* b is subnormal */
            /* normalize significand of b */
            mantb = b & FLOAT_MANT_MASK;
            expob++;
            do {
                mantb = 2 * mantb;
                expob--;
            } while (mantb < FLOAT_IMPLICIT_BIT);
        }
    }
    /* result exponent: add argument exponents and adjust for biasing */
    expor = expoa + expob - FLOAT_EXPO_BIAS + 2 * EXPO_ADJUST;
    mantb = mantb << FLOAT_EXPO_BITS; /* preshift to align result signficand */
    /* result significand: multiply argument significands */
    prod = (uint64_t)manta * mantb;
    mantr_hi = (uint32_t)(prod >> 32);
    mantr_lo = (uint32_t)(prod >>  0);
    /* normalize significand */
    if (mantr_hi < FLOAT_IMPLICIT_BIT) {
        mantr_hi = (mantr_hi << 1) | (mantr_lo >> (32 - 1));
        mantr_lo = (mantr_lo << 1);
        expor--;
    }
    if (expor <= (MAX_NORM_EXPO - EXPO_ADJUST)) { /* normal, may overflow to infinity during rounding */
        /* combine biased exponent, sign and signficand */
        r = (expor << FLOAT_MANT_BITS) + signr + mantr_hi;
        /* round result to nearest or even; overflow to infinity possible */
#if RND_VARIANT == 1
        r = r + ((mantr_lo | (mantr_hi & MANT_LSB)) > RND_BIT_MASK);
#elif RND_VARIANT == 2
        r = r + (mantr_lo >= RND_BIT_MASK);
        if (mantr_lo == 0x80000000) r = r & ~MANT_LSB; // tie case
#elif RND_VARIANT == 3
        r = r + ((mantr_lo == RND_BIT_MASK) ? (mantr_hi & MANT_LSB) : (mantr_lo >> RND_BIT_SHIFT));
#else // RND_VARIANT
#error unsupported RND_VARIANT
#endif // RND_VARIANT
    } else if ((int32_t)expor > (MAX_NORM_EXPO - EXPO_ADJUST)) { /* overflow */
        /* return infinity */
        r = signr | FLOAT_INFINITY;
    } else { /* underflow */
        /* return zero, normal, or smallest subnormal */
        shift = 0 - expor;
        if (shift > MAX_SHIFT) shift = MAX_SHIFT;
        /* denormalize significand */
        mantr_lo = mantr_hi << (32 - shift) | (mantr_lo ? 1 : 0);
        mantr_hi = mantr_hi >> shift;
        /* combine sign and signficand; biased exponent known to be zero */
        r = mantr_hi + signr;
        /* round result to nearest or even */
#if RND_VARIANT == 1
        r = r + ((mantr_lo | (mantr_hi & MANT_LSB)) > RND_BIT_MASK);
#elif RND_VARIANT == 2
        r = r + (mantr_lo >= RND_BIT_MASK);
        if (mantr_lo == 0x80000000) r = r & ~MANT_LSB; // tie case
#elif RND_VARIANT == 3
        r = r + ((mantr_lo == RND_BIT_MASK) ? (mantr_hi & MANT_LSB) : (mantr_lo >> RND_BIT_SHIFT));
#else // RND_VARIANT
#error unsupported RND_VARIANT
#endif // RND_VARIANT
     }
    return r;
}

uint32_t float_as_uint (float a)
{
    uint32_t r;
    memcpy (&r, &a, sizeof r);
    return r;
}

float uint_as_float (uint32_t a)
{
    float r;
    memcpy (&r, &a, sizeof r);
    return r;
}

float fp32_mul (float a, float b)
{
    return uint_as_float (fp32_mul_core (float_as_uint (a), float_as_uint (b)));
}

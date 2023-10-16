
/*
 chacha-merged.c version 20080118
 D. J. Bernstein
 Public domain.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "crypto_stream_chacha20.h"
#include "private/common.h"
#include "utils.h"

#include "../stream_chacha20.h"
#include "chacha20_ref.h"

#include <flexos/isolation.h>

struct chacha_ctx {
    uint32_t input[16];
};

typedef struct chacha_ctx chacha_ctx;

#define U32C(v) (v##U)

#define U32V(v) ((uint32_t)(v) &U32C(0xFFFFFFFF))

#define ROTATE(v, c) (ROTL32(v, c))
#define XOR(v, w) ((v) ^ (w))
#define PLUS(v, w) (U32V((v) + (w)))
#define PLUSONE(v) (PLUS((v), 1))

#define QUARTERROUND(a, b, c, d) \
    a = PLUS(a, b);              \
    d = ROTATE(XOR(d, a), 16);   \
    c = PLUS(c, d);              \
    b = ROTATE(XOR(b, c), 12);   \
    a = PLUS(a, b);              \
    d = ROTATE(XOR(d, a), 8);    \
    c = PLUS(c, d);              \
    b = ROTATE(XOR(b, c), 7);

static void
chacha_keysetup(chacha_ctx *ctx, const uint8_t *k)
{
    ctx->input[0]  = U32C(0x61707865);
    ctx->input[1]  = U32C(0x3320646e);
    ctx->input[2]  = U32C(0x79622d32);
    ctx->input[3]  = U32C(0x6b206574);
    ctx->input[4]  = LOAD32_LE(k + 0);
    ctx->input[5]  = LOAD32_LE(k + 4);
    ctx->input[6]  = LOAD32_LE(k + 8);
    ctx->input[7]  = LOAD32_LE(k + 12);
    ctx->input[8]  = LOAD32_LE(k + 16);
    ctx->input[9]  = LOAD32_LE(k + 20);
    ctx->input[10] = LOAD32_LE(k + 24);
    ctx->input[11] = LOAD32_LE(k + 28);
}

static void
chacha_ivsetup(chacha_ctx *ctx, const uint8_t *iv, const uint8_t *counter)
{
    ctx->input[12] = counter == NULL ? 0 : LOAD32_LE(counter + 0);
    ctx->input[13] = counter == NULL ? 0 : LOAD32_LE(counter + 4);
    ctx->input[14] = LOAD32_LE(iv + 0);
    ctx->input[15] = LOAD32_LE(iv + 4);
}

static void
chacha_ietf_ivsetup(chacha_ctx *ctx, const uint8_t *iv, const uint8_t *counter)
{
    ctx->input[12] = counter == NULL ? 0 : LOAD32_LE(counter);
    ctx->input[13] = LOAD32_LE(iv + 0);
    ctx->input[14] = LOAD32_LE(iv + 4);
    ctx->input[15] = LOAD32_LE(iv + 8);
}


static void
chacha20_encrypt_bytes_morello(void *__capability ctx1, uint8_t *__capability m, uint8_t *__capability c,
                       unsigned long long bytes)
{
    uint32_t x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14,
        x15;
    uint32_t j0, j1, j2, j3, j4, j5, j6, j7, j8, j9, j10, j11, j12, j13, j14,
        j15;
    uint8_t     *__capability ctarget = NULL;
    uint8_t      tmp[64];
    unsigned int i;

    chacha_ctx *__capability ctx = ctx1;

    if (!bytes) {
        return; /* LCOV_EXCL_LINE */
    }
    j0  = (uint32_t)ctx->input[0];
    j1  = (uint32_t)ctx->input[1];
    j2  = (uint32_t)ctx->input[2];
    j3  = (uint32_t)ctx->input[3];
    j4  = (uint32_t)ctx->input[4];
    j5  = (uint32_t)ctx->input[5];
    j6  = (uint32_t)ctx->input[6];
    j7  = (uint32_t)ctx->input[7];
    j8  = (uint32_t)ctx->input[8];
    j9  = (uint32_t)ctx->input[9];
    j10 = (uint32_t)ctx->input[10];
    j11 = (uint32_t)ctx->input[11];
    j12 = (uint32_t)ctx->input[12];
    j13 = (uint32_t)ctx->input[13];
    j14 = (uint32_t)ctx->input[14];
    j15 = (uint32_t)ctx->input[15];

    for (;;) {
        if (bytes < 64) {
            memset(tmp, 0, 64);
            for (i = 0; i < bytes; ++i) {
                tmp[i] = m[i];
            }
            m       = tmp;
            ctarget = c;
            c       = tmp;
        }
        x0  = j0;
        x1  = j1;
        x2  = j2;
        x3  = j3;
        x4  = j4;
        x5  = j5;
        x6  = j6;
        x7  = j7;
        x8  = j8;
        x9  = j9;
        x10 = j10;
        x11 = j11;
        x12 = j12;
        x13 = j13;
        x14 = j14;
        x15 = j15;
        for (i = 20; i > 0; i -= 2) {
            QUARTERROUND(x0, x4, x8, x12)
            QUARTERROUND(x1, x5, x9, x13)
            QUARTERROUND(x2, x6, x10, x14)
            QUARTERROUND(x3, x7, x11, x15)
            QUARTERROUND(x0, x5, x10, x15)
            QUARTERROUND(x1, x6, x11, x12)
            QUARTERROUND(x2, x7, x8, x13)
            QUARTERROUND(x3, x4, x9, x14)
        }
        x0  = PLUS(x0, j0);
        x1  = PLUS(x1, j1);
        x2  = PLUS(x2, j2);
        x3  = PLUS(x3, j3);
        x4  = PLUS(x4, j4);
        x5  = PLUS(x5, j5);
        x6  = PLUS(x6, j6);
        x7  = PLUS(x7, j7);
        x8  = PLUS(x8, j8);
        x9  = PLUS(x9, j9);
        x10 = PLUS(x10, j10);
        x11 = PLUS(x11, j11);
        x12 = PLUS(x12, j12);
        x13 = PLUS(x13, j13);
        x14 = PLUS(x14, j14);
        x15 = PLUS(x15, j15);

        uint32_t x0_1;
        uint32_t x1_1;
        uint32_t x2_1;
        uint32_t x3_1;
        uint32_t x4_1;
        uint32_t x5_1;
        uint32_t x6_1;
        uint32_t x7_1;
        uint32_t x8_1;
        uint32_t x9_1;
        uint32_t x10_1;
        uint32_t x11_1;
        uint32_t x12_1;
        uint32_t x13_1;
        uint32_t x14_1;
        uint32_t x15_1;

        uint8_t x_ptr1 = (__cheri_fromcap const uint8_t*)m + 0;
        uint8_t x_ptr2 = (__cheri_fromcap const uint8_t*)m + 4;
        uint8_t x_ptr3 = (__cheri_fromcap const uint8_t*)m + 8;
        uint8_t x_ptr4 = (__cheri_fromcap const uint8_t*)m + 12;
        uint8_t x_ptr5 = (__cheri_fromcap const uint8_t*)m + 16;
        uint8_t x_ptr6 = (__cheri_fromcap const uint8_t*)m + 20;
        uint8_t x_ptr7 = (__cheri_fromcap const uint8_t*)m + 24;
        uint8_t x_ptr8 = (__cheri_fromcap const uint8_t*)m + 28;
        uint8_t x_ptr9 = (__cheri_fromcap const uint8_t*)m + 32;
        uint8_t x_ptr10 = (__cheri_fromcap const uint8_t*)m + 36;
        uint8_t x_ptr11 = (__cheri_fromcap const uint8_t*)m + 40;
        uint8_t x_ptr12 = (__cheri_fromcap const uint8_t*)m + 44;
        uint8_t x_ptr13 = (__cheri_fromcap const uint8_t*)m + 48;
        uint8_t x_ptr14 = (__cheri_fromcap const uint8_t*)m + 52;
        uint8_t x_ptr15 = (__cheri_fromcap const uint8_t*)m + 56;
        uint8_t x_ptr16 = (__cheri_fromcap const uint8_t*)m + 60;

        __flexos_morello_gate1_rword_i(1, 0, x0_1, load32_le, x_ptr1);
        __flexos_morello_gate1_rword_i(1, 0, x1_1, load32_le, x_ptr2);
        __flexos_morello_gate1_rword_i(1, 0, x2_1, load32_le, x_ptr3);
        __flexos_morello_gate1_rword_i(1, 0, x3_1, load32_le, x_ptr4);
        __flexos_morello_gate1_rword_i(1, 0, x4_1, load32_le, x_ptr5);
        __flexos_morello_gate1_rword_i(1, 0, x5_1, load32_le, x_ptr6);
        __flexos_morello_gate1_rword_i(1, 0, x6_1, load32_le, x_ptr7);
        __flexos_morello_gate1_rword_i(1, 0, x7_1, load32_le, x_ptr8);
        __flexos_morello_gate1_rword_i(1, 0, x8_1, load32_le, x_ptr9);
        __flexos_morello_gate1_rword_i(1, 0, x9_1, load32_le, x_ptr10);
        __flexos_morello_gate1_rword_i(1, 0, x10_1, load32_le, x_ptr11);
        __flexos_morello_gate1_rword_i(1, 0, x11_1, load32_le, x_ptr12);
        __flexos_morello_gate1_rword_i(1, 0, x12_1, load32_le, x_ptr13);
        __flexos_morello_gate1_rword_i(1, 0, x13_1, load32_le, x_ptr14);
        __flexos_morello_gate1_rword_i(1, 0, x14_1, load32_le, x_ptr15);
        __flexos_morello_gate1_rword_i(1, 0, x15_1, load32_le, x_ptr16);

        x0  = XOR(x0, x0_1);
        x1  = XOR(x1, x1_1);
        x2  = XOR(x2, x2_1);
        x3  = XOR(x3, x3_1);
        x4  = XOR(x4, x4_1);
        x5  = XOR(x5, x5_1);
        x6  = XOR(x6, x6_1);
        x7  = XOR(x7, x7_1);
        x8  = XOR(x8, x8_1);
        x9  = XOR(x9, x9_1);
        x10 = XOR(x10, x10_1);
        x11 = XOR(x11, x11_1);
        x12 = XOR(x12, x12_1);
        x13 = XOR(x13, x13_1);
        x14 = XOR(x14, x14_1);
        x15 = XOR(x15, x15_1);


        j12 = PLUSONE(j12);
        /* LCOV_EXCL_START */
        if (!j12) {
            j13 = PLUSONE(j13);
        }
        /* LCOV_EXCL_STOP */
        uint64_t ptr1 = ((__cheri_fromcap uint8_t*)c) + 0;
        uint64_t ptr2 = (__cheri_fromcap uint8_t*)c + 4;
        uint64_t ptr3 = (__cheri_fromcap uint8_t*)c + 8;
        uint64_t ptr4 = (__cheri_fromcap uint8_t*)c + 12;
        uint64_t ptr5 = (__cheri_fromcap uint8_t*)c + 16;
        uint64_t ptr6 = (__cheri_fromcap uint8_t*)c + 20;
        uint64_t ptr7 = (__cheri_fromcap uint8_t*)c + 24;
        uint64_t ptr8 = (__cheri_fromcap uint8_t*)c + 28;
        uint64_t ptr9 = (__cheri_fromcap uint8_t*)c + 32;
        uint64_t ptr10 = (__cheri_fromcap uint8_t*)c + 36;
        uint64_t ptr11 = (__cheri_fromcap uint8_t*)c + 40;
        uint64_t ptr12 = (__cheri_fromcap uint8_t*)c + 44;
        uint64_t ptr13 = (__cheri_fromcap uint8_t*)c + 48;
        uint64_t ptr14 = (__cheri_fromcap uint8_t*)c + 52;
        uint64_t ptr15 = (__cheri_fromcap uint8_t*)c + 56;
        uint64_t ptr16 = (__cheri_fromcap uint8_t*)c + 60;

        __flexos_morello_gate2_ii(1, 0, store32_le, ptr1, x0);
        __flexos_morello_gate2_ii(1, 0, store32_le, ptr2, x1);
        __flexos_morello_gate2_ii(1, 0, store32_le, ptr3, x2);
        __flexos_morello_gate2_ii(1, 0, store32_le, ptr4, x3);
        __flexos_morello_gate2_ii(1, 0, store32_le, ptr5, x4);
        __flexos_morello_gate2_ii(1, 0, store32_le, ptr6, x5);
        __flexos_morello_gate2_ii(1, 0, store32_le, ptr7, x6);
        __flexos_morello_gate2_ii(1, 0, store32_le, ptr8, x7);
        __flexos_morello_gate2_ii(1, 0, store32_le, ptr9, x8);
        __flexos_morello_gate2_ii(1, 0, store32_le, ptr10, x9);
        __flexos_morello_gate2_ii(1, 0, store32_le, ptr11, x10);
        __flexos_morello_gate2_ii(1, 0, store32_le, ptr12, x11);
        __flexos_morello_gate2_ii(1, 0, store32_le, ptr13, x12);
        __flexos_morello_gate2_ii(1, 0, store32_le, ptr14, x13);
        __flexos_morello_gate2_ii(1, 0, store32_le, ptr15, x14);
        __flexos_morello_gate2_ii(1, 0, store32_le, ptr16, x15);

        if (bytes <= 64) {
            if (bytes < 64) {
                for (i = 0; i < (unsigned int) bytes; ++i) {
                    ctarget[i] = c[i]; /* ctarget cannot be NULL */
                }
            }
            ctx->input[12] = j12;
            ctx->input[13] = j13;

            return;
        }
        bytes -= 64;
        c += 64;
        m += 64;
    }
}

void
chacha20_encrypt_bytes(chacha_ctx *ctx, const uint8_t *m, uint8_t *c,
                       unsigned long long bytes)
{
    chacha_ctx *__capability ctx_cap = (chacha_ctx* __capability)ctx;
    __flexos_morello_gate4_variant1(0, 1, chacha20_encrypt_bytes_morello, ctx_cap, (uint8_t *__capability)m, (uint8_t *__capability)c, bytes);
}


// static void
// chacha20_encrypt_bytes(chacha_ctx *ctx, const uint8_t *m, uint8_t *c,
//                        unsigned long long bytes)
// {
//     uint32_t x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14,
//         x15;
//     uint32_t j0, j1, j2, j3, j4, j5, j6, j7, j8, j9, j10, j11, j12, j13, j14,
//         j15;
//     uint8_t     *ctarget = NULL;
//     uint8_t      tmp[64];
//     unsigned int i;

//     // uk_pr_crit("ctx->input: %p\n", ctx->input);
//     // uk_pr_crit("ctx->input[0]: %d\n", ctx->input[0]);
//     // uk_pr_crit("ctx: %p\n", ctx);

//     // uk_pr_crit("after pass %p\n", m);

//     if (!bytes) {
//         return; /* LCOV_EXCL_LINE */
//     }
//     j0  = ctx->input[0];
//     j1  = ctx->input[1];
//     j2  = ctx->input[2];
//     j3  = ctx->input[3];
//     j4  = ctx->input[4];
//     j5  = ctx->input[5];
//     j6  = ctx->input[6];
//     j7  = ctx->input[7];
//     j8  = ctx->input[8];
//     j9  = ctx->input[9];
//     j10 = ctx->input[10];
//     j11 = ctx->input[11];
//     j12 = ctx->input[12];
//     j13 = ctx->input[13];
//     j14 = ctx->input[14];
//     j15 = ctx->input[15];

//     for (;;) {
//         if (bytes < 64) {
//             memset(tmp, 0, 64);
//             for (i = 0; i < bytes; ++i) {
//                 tmp[i] = m[i];
//             }
//             m       = tmp;
//             ctarget = c;
//             c       = tmp;

//             // uk_pr_crit("different now? %p\n", m);
//         }
//         x0  = j0;
//         x1  = j1;
//         x2  = j2;
//         x3  = j3;
//         x4  = j4;
//         x5  = j5;
//         x6  = j6;
//         x7  = j7;
//         x8  = j8;
//         x9  = j9;
//         x10 = j10;
//         x11 = j11;
//         x12 = j12;
//         x13 = j13;
//         x14 = j14;
//         x15 = j15;
//         for (i = 20; i > 0; i -= 2) {
//             QUARTERROUND(x0, x4, x8, x12)
//             QUARTERROUND(x1, x5, x9, x13)
//             QUARTERROUND(x2, x6, x10, x14)
//             QUARTERROUND(x3, x7, x11, x15)
//             QUARTERROUND(x0, x5, x10, x15)
//             QUARTERROUND(x1, x6, x11, x12)
//             QUARTERROUND(x2, x7, x8, x13)
//             QUARTERROUND(x3, x4, x9, x14)
//         }
//         x0  = PLUS(x0, j0);
//         x1  = PLUS(x1, j1);
//         x2  = PLUS(x2, j2);
//         x3  = PLUS(x3, j3);
//         x4  = PLUS(x4, j4);
//         x5  = PLUS(x5, j5);
//         x6  = PLUS(x6, j6);
//         x7  = PLUS(x7, j7);
//         x8  = PLUS(x8, j8);
//         x9  = PLUS(x9, j9);
//         x10 = PLUS(x10, j10);
//         x11 = PLUS(x11, j11);
//         x12 = PLUS(x12, j12);
//         x13 = PLUS(x13, j13);
//         x14 = PLUS(x14, j14);
//         x15 = PLUS(x15, j15);

//         x0  = XOR(x0, LOAD32_LE(m + 0));
//         x1  = XOR(x1, LOAD32_LE(m + 4));
//         x2  = XOR(x2, LOAD32_LE(m + 8));
//         x3  = XOR(x3, LOAD32_LE(m + 12));
//         x4  = XOR(x4, LOAD32_LE(m + 16));
//         x5  = XOR(x5, LOAD32_LE(m + 20));
//         x6  = XOR(x6, LOAD32_LE(m + 24));
//         x7  = XOR(x7, LOAD32_LE(m + 28));
//         x8  = XOR(x8, LOAD32_LE(m + 32));
//         x9  = XOR(x9, LOAD32_LE(m + 36));
//         x10 = XOR(x10, LOAD32_LE(m + 40));
//         x11 = XOR(x11, LOAD32_LE(m + 44));
//         x12 = XOR(x12, LOAD32_LE(m + 48));
//         x13 = XOR(x13, LOAD32_LE(m + 52));
//         x14 = XOR(x14, LOAD32_LE(m + 56));
//         x15 = XOR(x15, LOAD32_LE(m + 60));

//         // uint32_t x00 = LOAD32_LE(m+0);

//         //uk_pr_crit("x0 load: %d\n", x00);
//         uk_pr_crit("x0: %d\n", x0);
//         uk_pr_crit("j0: %d\n", j0);
//         uk_pr_crit("tmp[0]: %d\n", tmp[0]);
        
//         // uk_pr_crit("values %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n", x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15);

//         j12 = PLUSONE(j12);
//         /* LCOV_EXCL_START */
//         if (!j12) {
//             j13 = PLUSONE(j13);
//         }
//         /* LCOV_EXCL_STOP */

//         STORE32_LE(c + 0, x0);
//         STORE32_LE(c + 4, x1);
//         STORE32_LE(c + 8, x2);
//         STORE32_LE(c + 12, x3);
//         STORE32_LE(c + 16, x4);
//         STORE32_LE(c + 20, x5);
//         STORE32_LE(c + 24, x6);
//         STORE32_LE(c + 28, x7);
//         STORE32_LE(c + 32, x8);
//         STORE32_LE(c + 36, x9);
//         STORE32_LE(c + 40, x10);
//         STORE32_LE(c + 44, x11);
//         STORE32_LE(c + 48, x12);
//         STORE32_LE(c + 52, x13);
//         STORE32_LE(c + 56, x14);
//         STORE32_LE(c + 60, x15);

//         uk_pr_crit("x0: %d\n", x0);

//         if (bytes <= 64) {
//             if (bytes < 64) {
//                 for (i = 0; i < (unsigned int) bytes; ++i) {
//                     ctarget[i] = c[i]; /* ctarget cannot be NULL */
//                 }
//             }
//             ctx->input[12] = j12;
//             ctx->input[13] = j13;

//             uk_pr_crit("ctarget[0]: %d\n", c[0]);
//             return;
//         }
//         bytes -= 64;
//         c += 64;
//         m += 64;
//     }

// }

static int
stream_ref(unsigned char *c, unsigned long long clen, const unsigned char *n,
           const unsigned char *k)
{
    struct chacha_ctx ctx;

    if (!clen) {
        return 0;
    }
    COMPILER_ASSERT(crypto_stream_chacha20_KEYBYTES == 256 / 8);
    chacha_keysetup(&ctx, k);
    chacha_ivsetup(&ctx, n, NULL);
    memset(c, 0, clen);
    chacha20_encrypt_bytes(&ctx, c, c, clen);
    sodium_memzero(&ctx, sizeof ctx);

    return 0;
}

static int
stream_ietf_ext_ref(unsigned char *c, unsigned long long clen,
                    const unsigned char *n, const unsigned char *k)
{
    struct chacha_ctx ctx;

    if (!clen) {
        return 0;
    }
    COMPILER_ASSERT(crypto_stream_chacha20_KEYBYTES == 256 / 8);
    chacha_keysetup(&ctx, k);
    chacha_ietf_ivsetup(&ctx, n, NULL);
    memset(c, 0, clen);
    chacha20_encrypt_bytes(&ctx, c, c, clen);
    sodium_memzero(&ctx, sizeof ctx);

    return 0;
}

static int
stream_ref_xor_ic(unsigned char *c, const unsigned char *m,
                  unsigned long long mlen, const unsigned char *n, uint64_t ic,
                  const unsigned char *k)
{
    struct chacha_ctx ctx;
    uint8_t           ic_bytes[8];
    uint32_t          ic_high;
    uint32_t          ic_low;

    if (!mlen) {
        return 0;
    }
    ic_high = U32V(ic >> 32);
    ic_low  = U32V(ic);
    STORE32_LE(&ic_bytes[0], ic_low);
    STORE32_LE(&ic_bytes[4], ic_high);
    chacha_keysetup(&ctx, k);
    chacha_ivsetup(&ctx, n, ic_bytes);
    chacha20_encrypt_bytes(&ctx, m, c, mlen);
    sodium_memzero(&ctx, sizeof ctx);
    return 0;
}

static int
stream_ietf_ext_ref_xor_ic(unsigned char *c, const unsigned char *m,
                           unsigned long long mlen, const unsigned char *n,
                           uint32_t ic, const unsigned char *k)
{
    struct chacha_ctx ctx;
    uint8_t           ic_bytes[4];

    if (!mlen) {
        return 0;
    }
    STORE32_LE(ic_bytes, ic);
    chacha_keysetup(&ctx, k);
    chacha_ietf_ivsetup(&ctx, n, ic_bytes);
    chacha20_encrypt_bytes(&ctx, m, c, mlen);
    sodium_memzero(&ctx, sizeof ctx);

    return 0;
}

struct crypto_stream_chacha20_implementation
    crypto_stream_chacha20_ref_implementation = {
        SODIUM_C99(.stream =) stream_ref,
        SODIUM_C99(.stream_ietf_ext =) stream_ietf_ext_ref,
        SODIUM_C99(.stream_xor_ic =) stream_ref_xor_ic,
        SODIUM_C99(.stream_ietf_ext_xor_ic =) stream_ietf_ext_ref_xor_ic
    };

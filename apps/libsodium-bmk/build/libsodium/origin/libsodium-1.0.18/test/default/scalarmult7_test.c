
#define TEST_NAME "scalarmult7"
#include "cmptest.h"

static unsigned char p1[32] = {
    0x72, 0x20, 0xf0, 0x09, 0x89, 0x30, 0xa7, 0x54, 0x74, 0x8b, 0x7d,
    0xdc, 0xb4, 0x3e, 0xf7, 0x5a, 0x0d, 0xbf, 0x3a, 0x0d, 0x26, 0x38,
    0x1a, 0xf4, 0xeb, 0xa4, 0xa9, 0x8e, 0xaa, 0x9b, 0x4e, 0xea
};

static unsigned char p2[32] = {
    0x85, 0x20, 0xf0, 0x09, 0x89, 0x30, 0xa7, 0x54, 0x74, 0x8b, 0x7d,
    0xdc, 0xb4, 0x3e, 0xf7, 0x5a, 0x0d, 0xbf, 0x3a, 0x0d, 0x26, 0x38,
    0x1a, 0xf4, 0xeb, 0xa4, 0xa9, 0x8e, 0xaa, 0x9b, 0x4e, 0x6a
};

static unsigned char scalar[32];
static unsigned char out1[32];
static unsigned char out2[32];

int
scalarmult7_xmain(void)
{
    int ret;

    scalar[0] = 1U;
    ret       = crypto_scalarmult_curve25519(out1, scalar, p1);
    assert(ret == 0);
    ret = crypto_scalarmult_curve25519(out2, scalar, p2);
    assert(ret == 0);
    printf("%d\n", !!memcmp(out1, out2, 32));

    return 0;
}

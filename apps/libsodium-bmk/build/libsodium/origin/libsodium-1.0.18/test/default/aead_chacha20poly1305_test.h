#ifndef __AEAD_CHACHA20POLY1305_TEST_H__
#define __AEAD_CHACHA20POLY1305_TEST_H__

int aead_chacha20poly1305_xmain(void);

#define aead_chacha20poly1305_res "\
,0xe3,0xe4,0x46,0xf7,0xed,0xe9,0xa1,0x9b\n\
,0x62,0xa4,0x67,0x7d,0xab,0xf4,0xe3,0xd2\n\
,0x4b,0x87,0x6b,0xb2,0x84,0x75,0x38,0x96\n\
,0xe1,0xd6\n\
,0xe3,0xe4,0x46,0xf7,0xed,0xe9,0xa1,0x9b\n\
,0x62,0xa4,0x69,0xe7,0x78,0x9b,0xcd,0x95\n\
,0x4e,0x65,0x8e,0xd3,0x84,0x23,0xe2,0x31\n\
,0x61,0xdc\n\
,0xe3,0xe4,0x46,0xf7,0xed,0xe9,0xa1,0x9b\n\
,0x62,0xa4,0x69,0xe7,0x78,0x9b,0xcd,0x95\n\
,0x4e,0x65,0x8e,0xd3,0x84,0x23,0xe2,0x31\n\
,0x61,0xdc\n\
,0xd3,0x1a,0x8d,0x34,0x64,0x8e,0x60,0xdb\n\
,0x7b,0x86,0xaf,0xbc,0x53,0xef,0x7e,0xc2\n\
,0xa4,0xad,0xed,0x51,0x29,0x6e,0x08,0xfe\n\
,0xa9,0xe2,0xb5,0xa7,0x36,0xee,0x62,0xd6\n\
,0x3d,0xbe,0xa4,0x5e,0x8c,0xa9,0x67,0x12\n\
,0x82,0xfa,0xfb,0x69,0xda,0x92,0x72,0x8b\n\
,0x1a,0x71,0xde,0x0a,0x9e,0x06,0x0b,0x29\n\
,0x05,0xd6,0xa5,0xb6,0x7e,0xcd,0x3b,0x36\n\
,0x92,0xdd,0xbd,0x7f,0x2d,0x77,0x8b,0x8c\n\
,0x98,0x03,0xae,0xe3,0x28,0x09,0x1b,0x58\n\
,0xfa,0xb3,0x24,0xe4,0xfa,0xd6,0x75,0x94\n\
,0x55,0x85,0x80,0x8b,0x48,0x31,0xd7,0xbc\n\
,0x3f,0xf4,0xde,0xf0,0x8e,0x4b,0x7a,0x9d\n\
,0xe5,0x76,0xd2,0x65,0x86,0xce,0xc6,0x4b\n\
,0x61,0x16,0x1a,0xe1,0x0b,0x59,0x4f,0x09\n\
,0xe2,0x6a,0x7e,0x90,0x2e,0xcb,0xd0,0x60\n\
,0x06,0x91\n\
,0xd3,0x1a,0x8d,0x34,0x64,0x8e,0x60,0xdb\n\
,0x7b,0x86,0xaf,0xbc,0x53,0xef,0x7e,0xc2\n\
,0xa4,0xad,0xed,0x51,0x29,0x6e,0x08,0xfe\n\
,0xa9,0xe2,0xb5,0xa7,0x36,0xee,0x62,0xd6\n\
,0x3d,0xbe,0xa4,0x5e,0x8c,0xa9,0x67,0x12\n\
,0x82,0xfa,0xfb,0x69,0xda,0x92,0x72,0x8b\n\
,0x1a,0x71,0xde,0x0a,0x9e,0x06,0x0b,0x29\n\
,0x05,0xd6,0xa5,0xb6,0x7e,0xcd,0x3b,0x36\n\
,0x92,0xdd,0xbd,0x7f,0x2d,0x77,0x8b,0x8c\n\
,0x98,0x03,0xae,0xe3,0x28,0x09,0x1b,0x58\n\
,0xfa,0xb3,0x24,0xe4,0xfa,0xd6,0x75,0x94\n\
,0x55,0x85,0x80,0x8b,0x48,0x31,0xd7,0xbc\n\
,0x3f,0xf4,0xde,0xf0,0x8e,0x4b,0x7a,0x9d\n\
,0xe5,0x76,0xd2,0x65,0x86,0xce,0xc6,0x4b\n\
,0x61,0x16,0x6a,0x23,0xa4,0x68,0x1f,0xd5\n\
,0x94,0x56,0xae,0xa1,0xd2,0x9f,0x82,0x47\n\
,0x72,0x16\n\
,0xd3,0x1a,0x8d,0x34,0x64,0x8e,0x60,0xdb\n\
,0x7b,0x86,0xaf,0xbc,0x53,0xef,0x7e,0xc2\n\
,0xa4,0xad,0xed,0x51,0x29,0x6e,0x08,0xfe\n\
,0xa9,0xe2,0xb5,0xa7,0x36,0xee,0x62,0xd6\n\
,0x3d,0xbe,0xa4,0x5e,0x8c,0xa9,0x67,0x12\n\
,0x82,0xfa,0xfb,0x69,0xda,0x92,0x72,0x8b\n\
,0x1a,0x71,0xde,0x0a,0x9e,0x06,0x0b,0x29\n\
,0x05,0xd6,0xa5,0xb6,0x7e,0xcd,0x3b,0x36\n\
,0x92,0xdd,0xbd,0x7f,0x2d,0x77,0x8b,0x8c\n\
,0x98,0x03,0xae,0xe3,0x28,0x09,0x1b,0x58\n\
,0xfa,0xb3,0x24,0xe4,0xfa,0xd6,0x75,0x94\n\
,0x55,0x85,0x80,0x8b,0x48,0x31,0xd7,0xbc\n\
,0x3f,0xf4,0xde,0xf0,0x8e,0x4b,0x7a,0x9d\n\
,0xe5,0x76,0xd2,0x65,0x86,0xce,0xc6,0x4b\n\
,0x61,0x16,0x6a,0x23,0xa4,0x68,0x1f,0xd5\n\
,0x94,0x56,0xae,0xa1,0xd2,0x9f,0x82,0x47\n\
,0x72,0x16\n\
"

#endif /* __AEAD_CHACHA20POLY1305_TEST_H__ */

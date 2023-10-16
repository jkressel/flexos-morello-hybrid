#ifndef __SODIUM_UTILS_TEST_H__
#define __SODIUM_UTILS_TEST_H__

int sodium_utils_xmain(void);

#define sodium_utils_res "\
0\n\
0\n\
-1\n\
0\n\
0\n\
0\n\
0\n\
0\n\
010000000000000000000000000000000000000000000000\n\
000000000000000000000000000000000000000000000000\n\
010100000000000000000000000000000000000000000000\n\
020000000000000000000000000000000000000000000000\n\
0001ff000000000000000000000000000000000000000000\n\
0\n\
0\n\
000000000000fffefefefefefefefefefefefefefefefefe\n\
00000000000000000000fffefefefefefefefefefefefefe\n\
00000000000000000000000000000000000000000000fffe\n\
fcfffffffffffbfdfefefefefefefefefefefefefefefefe\n\
fcfffffffffffffffffffbfdfefefefefefefefefefefefe\n\
fcfffffffffffffffffffffffffffffffffffffffffffbfd\n\
fcfffffffffffffffffffffffffffffffffffffffffffbfd\n\
fcfffffffffffffffffffffffffffffffffffffffffffbfd\n\
fcfffffffffffffffffffffffffffffffffffffffffffbfd\n\
fcfffffffffffffffffffffffffffffffffffffffffffbfd\n\
"

#endif /* __SODIUM_UTILS_TEST_H__ */

#ifndef __CODECS_TEST_H__
#define __CODECS_TEST_H__

int codecs_xmain(void);

#define codecs_res "\
30313233343536373839414243444546\n\
bin2hex(..., guard_page, 0):\n\
bin2hex(..., \"\", 0):\n\
4:cafe6942\n\
dt1: 11\n\
4:cafe6942\n\
dt2: 2\n\
dt3: 11\n\
dt4: 11\n\
dt5: 11\n\
dt6: 11\n\
+/DxMDEyMzQ1Njc4OUFCQ0RFRmFi\n\
+/DxMDEyMzQ1Njc4OUFCQ0RFRmFiYw\n\
-_DxMDEyMzQ1Njc4OUFCQ0RFRmFi\n\
-_DxMDEyMzQ1Njc4OUFCQ0RFRmFiYw\n\
\n\
YQ==\n\
YWI=\n\
YWJj\n\
\n\
YQ\n\
YWI\n\
YWJj\n\
[]\n\
[BpcyBhIGpvdXJu\n\
ZXkgaW50by Bzb3VuZA==]\n\
[This is a journey into sound]\n\
[This is a journ]\n\
[\n\
ZXkgaW50by Bzb3VuZA==]\n\
"

#endif /* __CODECS_TEST_H__ */

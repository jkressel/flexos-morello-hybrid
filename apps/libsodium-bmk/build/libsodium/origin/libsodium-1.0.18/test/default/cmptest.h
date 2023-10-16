
#ifndef __CMPTEST_H__
#define __CMPTEST_H__

#ifdef NDEBUG
#/**/undef/**/ NDEBUG
#endif

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "sodium.h"
#include "quirks.h"

#ifdef __EMSCRIPTEN__
# undef TEST_SRCDIR
# define TEST_SRCDIR "/test-data"
#endif
#ifndef TEST_SRCDIR
# define TEST_SRCDIR "."
#endif

#define TEST_NAME_RES TEST_NAME ".res"
#define TEST_NAME_OUT TEST_SRCDIR "/" TEST_NAME ".exp"

#ifdef HAVE_ARC4RANDOM
# undef rand
# define rand(X) arc4random(X)
#endif

extern FILE *fp_res;
extern unsigned char *guard_page;

#undef  printf
#define printf(...) fprintf(fp_res, __VA_ARGS__)

#endif

/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Glue code for libsodium tests
 *
 * Authors: Michalis Pappas <michalis.pappas@opensynergy.com>
 *
 * Copyright (c) 2021 OpenSynergy GmbH. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sodium.h>
#include <stdio.h>
#include <uk/plat/console.h>
#include <uk/test.h>
#include <uk_sodium_test_defs.h>

#define RES_BUF_LEN_MAX 44 * 1024 /* > largest .exp file in tests/default/ */

FILE *fp_res;
unsigned char *guard_page;

static char exp_buf[RES_BUF_LEN_MAX];
static char res_buf[RES_BUF_LEN_MAX];

/* Replacement for libsodium's cmptest function (test/default/cmptest.h) */
int uk_sodium_cmptest(struct uk_sodium_test *test)
{
	FILE *fp_exp = NULL;
	unsigned char *gp = NULL;
	int c;
	int res = 0;

	if (sodium_init() == -1) {
		perror("sodium_init()");
		res = -1;
		goto out;
	}

	/* Prepare guard page */
	if (!(gp = (unsigned char *) sodium_malloc(0))) {
		perror("sodium_malloc()");
		res = -1;
		goto out;
	}
	guard_page = gp + 1;

	/* Prepare expected result buffer */
	if (!(fp_exp = fmemopen(test->exp, sizeof(exp_buf), "r"))) {
		perror("fmemopen");
		res = -1;
		goto out;
	}
	fprintf(fp_exp, "%s", test->exp);

	/* Prepare actual result buffer */
	if (!(fp_res = fmemopen(res_buf, sizeof(res_buf), "w+"))) {
		perror("fmemopen");
		res = -1;
		goto out;
	}

	/* Execute test */
	if (test->func() != 0) {
		res = -1;
		goto out;
	}

	/* Compare */
	rewind(fp_exp);
	rewind(fp_res);
	while ((c = fgetc(fp_res)) != EOF) {
		if (c != fgetc(fp_exp)) {
			res = -1;
			goto out;
		}
	}

out:
	if (gp)
		sodium_free(gp);

	if (fp_res)
		fclose(fp_res);

	if (fp_exp)
		fclose(fp_exp);

	return res;
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_aead_aes256gcm)
{
	struct uk_sodium_test aead_aes256gcm = {
		.name = "aead_aes256gcm",
		.exp  = aead_aes256gcm_res,
		.func = aead_aes256gcm_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&aead_aes256gcm));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_aead_aes256gcm2)
{
	struct uk_sodium_test aead_aes256gcm2 = {
		.name = "aead_aes256gcm2",
		.exp  = aead_aes256gcm2_res,
		.func = aead_aes256gcm2_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&aead_aes256gcm2));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_aead_chacha20poly1305)
{
	struct uk_sodium_test aead_chacha20poly1305 = {
		.name = "aead_chacha20poly1305",
		.exp  = aead_chacha20poly1305_res,
		.func = aead_chacha20poly1305_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&aead_chacha20poly1305));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_aead_chacha20poly13052)
{
	struct uk_sodium_test aead_chacha20poly13052 = {
		.name = "aead_chacha20poly13052",
		.exp  = aead_chacha20poly13052_res,
		.func = aead_chacha20poly13052_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&aead_chacha20poly13052));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_aead_xchacha20poly1305)
{
	struct uk_sodium_test aead_xchacha20poly1305 = {
		.name = "aead_xchacha20poly1305",
		.exp  = aead_xchacha20poly1305_res,
		.func = aead_xchacha20poly1305_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&aead_xchacha20poly1305));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_auth)
{
	struct uk_sodium_test auth = {
		.name = "auth",
		.exp  = auth_res,
		.func = auth_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&auth));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_auth2)
{
	struct uk_sodium_test auth2 = {
		.name = "auth2",
		.exp  = auth2_res,
		.func = auth2_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&auth2));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_auth3)
{
	struct uk_sodium_test auth3 = {
		.name = "auth3",
		.exp  = auth3_res,
		.func = auth3_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&auth3));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_auth5)
{
	struct uk_sodium_test auth5 = {
		.name = "auth5",
		.exp  = auth5_res,
		.func = auth5_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&auth5));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_auth6)
{
	struct uk_sodium_test auth6 = {
		.name = "auth6",
		.exp  = auth6_res,
		.func = auth6_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&auth6));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_auth7)
{
	struct uk_sodium_test auth7 = {
		.name = "auth7",
		.exp  = auth7_res,
		.func = auth7_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&auth7));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_box)
{
	struct uk_sodium_test box = {
		.name = "box",
		.exp  = box_res,
		.func = box_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&box));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_box2)
{
	struct uk_sodium_test box2 = {
		.name = "box2",
		.exp  = box2_res,
		.func = box2_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&box2));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_box7)
{
	struct uk_sodium_test box7 = {
		.name = "box7",
		.exp  = box7_res,
		.func = box7_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&box7));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_box8)
{
	struct uk_sodium_test box8 = {
		.name = "box8",
		.exp  = box8_res,
		.func = box8_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&box8));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_box_easy)
{
	struct uk_sodium_test box_easy = {
		.name = "box_easy",
		.exp  = box_easy_res,
		.func = box_easy_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&box_easy));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_box_easy2)
{
	struct uk_sodium_test box_easy2 = {
		.name = "box_easy2",
		.exp  = box_easy2_res,
		.func = box_easy2_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&box_easy2));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_box_seal)
{
	struct uk_sodium_test box_seal = {
		.name = "box_seal",
		.exp  = box_seal_res,
		.func = box_seal_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&box_seal));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_box_seed)
{
	struct uk_sodium_test box_seed = {
		.name = "box_seed",
		.exp  = box_seed_res,
		.func = box_seed_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&box_seed));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_chacha20)
{
	struct uk_sodium_test chacha20 = {
		.name = "chacha20",
		.exp  = chacha20_res,
		.func = chacha20_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&chacha20));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_codecs)
{
	struct uk_sodium_test codecs = {
		.name = "codecs",
		.exp  = codecs_res,
		.func = codecs_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&codecs));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_core1)
{
	struct uk_sodium_test core1 = {
		.name = "core1",
		.exp  = core1_res,
		.func = core1_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&core1));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_core2)
{
	struct uk_sodium_test core2 = {
		.name = "core2",
		.exp  = core2_res,
		.func = core2_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&core2));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_core3)
{
	struct uk_sodium_test core3 = {
		.name = "core3",
		.exp  = core3_res,
		.func = core3_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&core3));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_core4)
{
	struct uk_sodium_test core4 = {
		.name = "core4",
		.exp  = core4_res,
		.func = core4_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&core4));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_core5)
{
	struct uk_sodium_test core5 = {
		.name = "core5",
		.exp  = core5_res,
		.func = core5_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&core5));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_core6)
{
	struct uk_sodium_test core6 = {
		.name = "core6",
		.exp  = core6_res,
		.func = core6_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&core6));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_ed25519_convert)
{
	struct uk_sodium_test ed25519_convert = {
		.name = "ed25519_convert",
		.exp  = ed25519_convert_res,
		.func = ed25519_convert_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&ed25519_convert));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_generichash)
{
	struct uk_sodium_test generichash = {
		.name = "generichash",
		.exp  = generichash_res,
		.func = generichash_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&generichash));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_generichash2)
{
	struct uk_sodium_test generichash2 = {
		.name = "generichash2",
		.exp  = generichash2_res,
		.func = generichash2_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&generichash2));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_generichash3)
{
	struct uk_sodium_test generichash3 = {
		.name = "generichash3",
		.exp  = generichash3_res,
		.func = generichash3_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&generichash3));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_hash)
{
	struct uk_sodium_test hash = {
		.name = "hash",
		.exp  = hash_res,
		.func = hash_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&hash));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_hash3)
{
	struct uk_sodium_test hash3 = {
		.name = "hash3",
		.exp  = hash3_res,
		.func = hash3_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&hash3));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_kdf)
{
	struct uk_sodium_test kdf = {
		.name = "kdf",
		.exp  = kdf_res,
		.func = kdf_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&kdf));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_keygen)
{
	struct uk_sodium_test keygen = {
		.name = "keygen",
		.exp  = keygen_res,
		.func = keygen_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&keygen));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_kx)
{
	struct uk_sodium_test kx = {
		.name = "kx",
		.exp  = kx_res,
		.func = kx_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&kx));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_metamorphic)
{
	struct uk_sodium_test metamorphic = {
		.name = "metamorphic",
		.exp  = metamorphic_res,
		.func = metamorphic_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&metamorphic));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_misuse)
{
	struct uk_sodium_test misuse = {
		.name = "misuse",
		.exp  = misuse_res,
		.func = misuse_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&misuse));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_onetimeauth)
{
	struct uk_sodium_test onetimeauth = {
		.name = "onetimeauth",
		.exp  = onetimeauth_res,
		.func = onetimeauth_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&onetimeauth));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_onetimeauth2)
{
	struct uk_sodium_test onetimeauth2 = {
		.name = "onetimeauth2",
		.exp  = onetimeauth2_res,
		.func = onetimeauth2_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&onetimeauth2));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_onetimeauth7)
{
	struct uk_sodium_test onetimeauth7 = {
		.name = "onetimeauth7",
		.exp  = onetimeauth7_res,
		.func = onetimeauth7_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&onetimeauth7));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_pwhash_argon2i)
{
	struct uk_sodium_test pwhash_argon2i = {
		.name = "pwhash_argon2i",
		.exp  = pwhash_argon2i_res,
		.func = pwhash_argon2i_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&pwhash_argon2i));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_pwhash_argon2id)
{
	struct uk_sodium_test pwhash_argon2id = {
		.name = "pwhash_argon2id",
		.exp  = pwhash_argon2id_res,
		.func = pwhash_argon2id_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&pwhash_argon2id));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_randombytes)
{
	struct uk_sodium_test randombytes = {
		.name = "randombytes",
		.exp  = randombytes_res,
		.func = randombytes_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&randombytes));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_scalarmult)
{
	struct uk_sodium_test scalarmult = {
		.name = "scalarmult",
		.exp  = scalarmult_res,
		.func = scalarmult_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&scalarmult));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_scalarmult2)
{
	struct uk_sodium_test scalarmult2 = {
		.name = "scalarmult2",
		.exp  = scalarmult2_res,
		.func = scalarmult2_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&scalarmult2));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_scalarmult5)
{
	struct uk_sodium_test scalarmult5 = {
		.name = "scalarmult5",
		.exp  = scalarmult5_res,
		.func = scalarmult5_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&scalarmult5));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_scalarmult6)
{
	struct uk_sodium_test scalarmult6 = {
		.name = "scalarmult6",
		.exp  = scalarmult6_res,
		.func = scalarmult6_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&scalarmult6));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_scalarmult7)
{
	struct uk_sodium_test scalarmult7 = {
		.name = "scalarmult7",
		.exp  = scalarmult7_res,
		.func = scalarmult7_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&scalarmult7));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_scalarmult8)
{
	struct uk_sodium_test scalarmult8 = {
		.name = "scalarmult8",
		.exp  = scalarmult8_res,
		.func = scalarmult8_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&scalarmult8));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_secretbox)
{
	struct uk_sodium_test secretbox = {
		.name = "secretbox",
		.exp  = secretbox_res,
		.func = secretbox_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&secretbox));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_secretbox2)
{
	struct uk_sodium_test secretbox2 = {
		.name = "secretbox2",
		.exp  = secretbox2_res,
		.func = secretbox2_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&secretbox2));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_secretbox7)
{
	struct uk_sodium_test secretbox7 = {
		.name = "secretbox7",
		.exp  = secretbox7_res,
		.func = secretbox7_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&secretbox7));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_secretbox8)
{
	struct uk_sodium_test secretbox8 = {
		.name = "secretbox8",
		.exp  = secretbox8_res,
		.func = secretbox8_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&secretbox8));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_secretbox_easy)
{
	struct uk_sodium_test secretbox_easy = {
		.name = "secretbox_easy",
		.exp  = secretbox_easy_res,
		.func = secretbox_easy_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&secretbox_easy));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_secretbox_easy2)
{
	struct uk_sodium_test secretbox_easy2 = {
		.name = "secretbox_easy2",
		.exp  = secretbox_easy2_res,
		.func = secretbox_easy2_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&secretbox_easy2));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_secretstream)
{
	struct uk_sodium_test secretstream = {
		.name = "secretstream",
		.exp  = secretstream_res,
		.func = secretstream_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&secretstream));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_shorthash)
{
	struct uk_sodium_test shorthash = {
		.name = "shorthash",
		.exp  = shorthash_res,
		.func = shorthash_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&shorthash));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_sign)
{
	struct uk_sodium_test sign = {
		.name = "sign",
		.exp  = sign_res,
		.func = sign_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&sign));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_sodium_core)
{
	struct uk_sodium_test sodium_core = {
		.name = "sodium_core",
		.exp  = sodium_core_res,
		.func = sodium_core_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&sodium_core));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_sodium_utils)
{
	struct uk_sodium_test sodium_utils = {
		.name = "sodium_utils",
		.exp  = sodium_utils_res,
		.func = sodium_utils_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&sodium_utils));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_sodium_utils2)
{
	struct uk_sodium_test sodium_utils2 = {
		.name = "sodium_utils2",
		.exp  = sodium_utils2_res,
		.func = sodium_utils2_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&sodium_utils2));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_sodium_utils3)
{
	struct uk_sodium_test sodium_utils3 = {
		.name = "sodium_utils3",
		.exp  = sodium_utils3_res,
		.func = sodium_utils3_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&sodium_utils3));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_sodium_version)
{
	struct uk_sodium_test sodium_version = {
		.name = "sodium_version",
		.exp  = sodium_version_res,
		.func = sodium_version_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&sodium_version));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_stream)
{
	struct uk_sodium_test stream = {
		.name = "stream",
		.exp  = stream_res,
		.func = stream_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&stream));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_stream2)
{
	struct uk_sodium_test stream2 = {
		.name = "stream2",
		.exp  = stream2_res,
		.func = stream2_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&stream2));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_stream3)
{
	struct uk_sodium_test stream3 = {
		.name = "stream3",
		.exp  = stream3_res,
		.func = stream3_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&stream3));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_stream4)
{
	struct uk_sodium_test stream4 = {
		.name = "stream4",
		.exp  = stream4_res,
		.func = stream4_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&stream4));
}

UK_TESTCASE(libsodium_minimal_testsuite, uktest_test_verify1)
{
	struct uk_sodium_test verify1 = {
		.name = "verify1",
		.exp  = verify1_res,
		.func = verify1_xmain
	};
	UK_TEST_EXPECT_ZERO(uk_sodium_cmptest(&verify1));
}

#if defined(CONFIG_LIBSODIUM_TEST_MINIMAL)
uk_testsuite_register(libsodium_minimal_testsuite, NULL);
#endif

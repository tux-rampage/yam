#if !defined(PROTO_AMISSL_H) && !defined(AMISSL_COMPILE)
#include <proto/amissl.h>
#endif
/* ====================================================================
 * Copyright (c) 1999-2015 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */


#ifndef HEADER_COMP_H
# define HEADER_COMP_H

# include <openssl/opensslconf.h>

# ifndef OPENSSL_NO_COMP
# include <openssl/crypto.h>
# ifdef  __cplusplus
extern "C" {
# endif



COMP_CTX *COMP_CTX_new(COMP_METHOD *meth);
const COMP_METHOD *COMP_CTX_get_method(const COMP_CTX *ctx);
int COMP_CTX_get_type(const COMP_CTX* comp);
int COMP_get_type(const COMP_METHOD *meth);
const char *COMP_get_name(const COMP_METHOD *meth);
void COMP_CTX_free(COMP_CTX *ctx);

int COMP_compress_block(COMP_CTX *ctx, unsigned char *out, int olen,
                        unsigned char *in, int ilen);
int COMP_expand_block(COMP_CTX *ctx, unsigned char *out, int olen,
                      unsigned char *in, int ilen);

COMP_METHOD *COMP_zlib(void);

#if OPENSSL_API_COMPAT < 0x10100000L
#define COMP_zlib_cleanup() while(0) continue
#endif

# ifdef HEADER_BIO_H
#  ifdef ZLIB
const BIO_METHOD *BIO_f_zlib(void);
#  endif
# endif

/* BEGIN ERROR CODES */
/*
 * The following lines are auto generated by the script mkerr.pl. Any changes
 * made after this point may be overwritten when the script is next run.
 */
void ERR_load_COMP_strings(void);

/* Error codes for the COMP functions. */

/* Function codes. */
# define COMP_F_BIO_ZLIB_FLUSH                            99
# define COMP_F_BIO_ZLIB_NEW                              100
# define COMP_F_BIO_ZLIB_READ                             101
# define COMP_F_BIO_ZLIB_WRITE                            102

/* Reason codes. */
# define COMP_R_ZLIB_DEFLATE_ERROR                        99
# define COMP_R_ZLIB_INFLATE_ERROR                        100
# define COMP_R_ZLIB_NOT_SUPPORTED                        101

#ifdef  __cplusplus
}
# endif
# endif

#endif

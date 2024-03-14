/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __MBEDTLS_USER_CONFIG_H__
#define __MBEDTLS_USER_CONFIG_H__

// Defines which cannot be specified in prj.conf
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_X509_CREATE_C
#define MBEDTLS_X509_CRT_WRITE_C
#define MBEDTLS_X509_CRL_PARSE_C
#define MBEDTLS_SSL_KEEP_PEER_CERTIFICATE
#define MBEDTLS_PSA_CRYPTO_STORAGE_C

#endif /* __MBEDTLS_USER_CONFIG_H__ */

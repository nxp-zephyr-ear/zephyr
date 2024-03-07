/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// This is a workaround for a known issue when using TF-M PSA as backend for mbedTLS.
// There is a partial workaround in Zephyr TF-M, but it fails for our example, since we 
// actually need to link it when using the PK module of mbedTLS.
// See: https://github.com/zephyrproject-rtos/trusted-firmware-m/commit/e99f0c8d2e8f3d7733137422eeb5636b24541107
// Upstream issue: https://github.com/Mbed-TLS/mbedtls/issues/8602
//
// Will be fixed upstream in the next mbedTLS release: https://github.com/Mbed-TLS/mbedtls/pull/8607

#include <psa/crypto.h>

psa_algorithm_t psa_get_key_enrollment_algorithm(
const psa_key_attributes_t *attributes)
{
    return psa_get_key_algorithm(attributes);
}

/**
 *  Copyright 2023-2024 NXP
 *
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  @file  main.c
 *  @brief main file
 */

#include <stdio.h>

#include "fsl_common.h"
#include "fsl_device_registers.h"
#include "els_pkc_examples.h"

#include <mcux_els.h>
#include <mcux_pkc.h>

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/

/******************************************************************************/
/******************** CRYPTO_InitHardware **************************************/
/******************************************************************************/
/*!
 * @brief Main function
 */
int main(void)
{
	uint8_t pass = 0;
	uint8_t fail = 0;
	printf("\r\n============================\r\n");
	printf("\rELS hash example\r");
	printf("\r\n============================\r\n");

	printf("SHA224 one shot:");
	if (mcuxClHashModes_sha224_oneshot_example() == true) {
		pass++;
		printf("pass \r\n");
	} else {
		fail++;
		printf("fail \r\n");
	}

	printf("SHA256 one shot:");
	if (mcuxClHashModes_sha256_oneshot_example() == true) {
		pass++;
		printf("pass \r\n");
	} else {
		fail++;
		printf("fail \r\n");
	}

	printf("SHA256 streaming example:");
	if (mcuxClHashModes_sha256_streaming_example() == true) {
		pass++;
		printf("pass \r\n");
	} else {
		fail++;
		printf("fail \r\n");
	}

	printf("SHA256 long message example:");
	if (mcuxClHashModes_sha256_longMsgOneshot_example() == true) {
		pass++;
		printf("pass \r\n");
	} else {
		fail++;
		printf("fail \r\n");
	}

	printf("SHA384 one shot:");
	if (mcuxClHashModes_sha384_oneshot_example() == true) {
		pass++;
		printf("pass \r\n");
	} else {
		fail++;
		printf("fail \r\n");
	}

	printf("SHA512 one shot:");
	if (mcuxClHashModes_sha512_oneshot_example() == true) {
		pass++;
		printf("pass \r\n");
	} else {
		fail++;
		printf("fail \r\n");
	}

	printf("\r\n============================\r\n");
	printf("\rELS PKC asymmetric cipher example\r");
	printf("\r\n============================\r\n");

	printf("PKC ECC keygen sign verify:");
	if (mcuxClEls_Ecc_Keygen_Sign_Verify_example() == true) {
		pass++;
		printf("pass \r\n");
	} else {
		fail++;
		printf("fail \r\n");
	}

	printf("PKC RSA no-verify:");
	if (mcuxClRsa_verify_NoVerify_example() == true) {
		pass++;
		printf("pass \r\n");
	} else {
		fail++;
		printf("fail \r\n");
	}

	printf("PKC RSA sign no-encode:");
	if (mcuxClRsa_sign_NoEncode_example() == true) {
		pass++;
		printf("pass \r\n");
	} else {
		fail++;
		printf("fail \r\n");
	}

	printf("PKC RSA-PSS sign SHA256:");
	if (mcuxClRsa_sign_pss_sha2_256_example() == true) {
		pass++;
		printf("pass \r\n");
	} else {
		fail++;
		printf("fail \r\n");
	}

	printf("PKC RSA-PSS verify SHA256:");
	if (mcuxClRsa_verify_pssverify_sha2_256_example() == true) {
		pass++;
		printf("pass \r\n");
	} else {
		fail++;
		printf("fail \r\n");
	}

	printf("PKC ECC Curve25519:");
	if (mcuxClEcc_Mont_Curve25519_example() == true) {
		pass++;
		printf("pass \r\n");
	} else {
		fail++;
		printf("fail \r\n");
	}

	printf("TLS Master session keys:");
	if (mcuxClEls_Tls_Master_Key_Session_Keys_example() == true) {
		pass++;
		printf("pass \r\n");
	} else {
		fail++;
		printf("fail \r\n");
	}

	printf("\r\n============================\r\n");
	printf("\rELS PKC common example\r");
	printf("\r\n============================\r\n");

	printf("ELS get info:");
	if (mcuxClEls_Common_Get_Info_example() == true) {
		pass++;
		printf("pass \r\n");
	} else {
		fail++;
		printf("fail \r\n");
	}

	printf("RNG PRNG random:");
	if (mcuxClEls_Rng_Prng_Get_Random_example() == true) {
		pass++;
		printf("pass \r\n");
	} else {
		fail++;
		printf("fail \r\n");
	}

	printf("Flow protection:");
	if (mcuxCsslFlowProtection_example() == true) {
		pass++;
		printf("pass \r\n");
	} else {
		fail++;
		printf("fail \r\n");
	}

	printf("Memory data invariant compare:");
	if (data_invariant_memory_compare() == EXIT_CODE_OK) {
		pass++;
		printf("pass \r\n");
	} else {
		fail++;
		printf("fail \r\n");
	}

	printf("Memory data invariant copy:");
	if (data_invariant_memory_compare() == EXIT_CODE_OK) {
		pass++;
		printf("pass \r\n");
	} else {
		fail++;
		printf("fail \r\n");
	}

	printf("Key component operations:");
	if (mcuxClKey_example() == true) {
		pass++;
		printf("pass \r\n");
	} else {
		fail++;
		printf("fail \r\n");
	}

	printf("ELS power down wake-up init:");
	if (ELS_PowerDownWakeupInit(ELS) == kStatus_Success) {
		pass++;
		printf("pass \r\n");
	} else {
		fail++;
		printf("fail \r\n");
	}

	printf("PKC power down wake-up init:");
	if (PKC_PowerDownWakeupInit(PKC) == kStatus_Success) {
		pass++;
		printf("pass \r\n");
	} else {
		fail++;
		printf("fail \r\n");
	}

	printf("\r\n============================\r\n");
	printf("\rELS symmetric cipher example\r");
	printf("\r\n============================\r\n");

	printf("AES128-CBC encryption:");
	if (mcuxClEls_Cipher_Aes128_Cbc_Encrypt_example() == true) {
		pass++;
		printf("pass \r\n");
	} else {
		fail++;
		printf("fail \r\n");
	}

	printf("AES128-ECB encryption:");
	if (mcuxClEls_Cipher_Aes128_Ecb_Encrypt_example() == true) {
		pass++;
		printf("pass \r\n");
	} else {
		fail++;
		printf("fail \r\n");
	}

	printf("CMAC AES128:");
	if (mcuxClMacModes_cmac_oneshot_example() == true) {
		pass++;
		printf("pass \r\n");
	} else {
		fail++;
		printf("fail \r\n");
	}

	printf("\r\n============================\r\n");
	printf("RESULT: ");
	if (fail == 0) {
		printf("All %d test PASS!!\r\n", pass);
	} else {
		printf("%d / %d test PASSED, %d FAILED!!\r\n", pass, pass + fail, fail);
	}

	printf("ELS-PKC stand-alone examples END \r\n");
	return 0;
}

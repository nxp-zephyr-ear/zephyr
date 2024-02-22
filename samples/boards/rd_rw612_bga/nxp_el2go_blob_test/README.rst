.. nxp_el2go_blob_test:

RD-RW612-BGA NXP EL2GO Blob Test Application
#######################################

Overview
********

This is an application which imports and validates EL2GO blobs and their usage with PSA.
Note: You will first need to replace the placeholder blobs with real ones from EL2GO.

The source code for this application can be found at:
:zephyr_file:`modules/hal/nxp/mcux/middleware/nxp_iot_agent/ex/src/apps/psa_examples/el2go_blob_test`.

Requirements
************

- Micro USB cable
- RD-RW61X-BGA board
- Personal Computer

Building, Flashing and Running
******************************

.. zephyr-app-commands::
   :zephyr-app: samples/boards/rd_rw612_bga/nxp_el2go_blob_test
   :board: rd_rw612_bga_ns
   :goals: build flash
   :compact:

Sample Output
=============

.. code-block:: console

    [WRN] This device was provisioned with dummy keys. This device is NOT SECURE
    [Sec Thread] Secure image initializing!
    Booting TF-M v1.8.0
    [INF][Crypto] Provisioning entropy seed... complete.
    *** Booting Zephyr OS build zephyr-v3.5.0-5365-g9d2cefa7fd2f ***
    
    #### Start EL2GO blob tests ####
    Running test suite INTERNAL (EL2GO_BLOB_TEST_INTERNAL_10XX)
    > Executing test EL2GO_BLOB_TEST_INTERNAL_1000 
      Description: 'Internal AES128 CIPHER CTR'
      Placeholder blob
      Test EL2GO_BLOB_TEST_INTERNAL_1000 - SKIPPED
    [...]
    > Executing test EL2GO_BLOB_TEST_INTERNAL_1031 
      Description: 'Internal HMAC256 KDF HKDFSHA256'
      Placeholder blob
      Test EL2GO_BLOB_TEST_INTERNAL_1031 - SKIPPED
    15 of 15 SKIPPED
    Test suite INTERNAL (EL2GO_BLOB_TEST_INTERNAL_10XX) - PASSED
    Running test suite EXTERNAL (EL2GO_BLOB_TEST_EXTERNAL_2XXX)
    > Executing test EL2GO_BLOB_TEST_EXTERNAL_2000 
      Description: 'External BIN1B EXPORT NONE'
      Placeholder blob
      Test EL2GO_BLOB_TEST_EXTERNAL_2000 - SKIPPED
    [...]
    > Executing test EL2GO_BLOB_TEST_EXTERNAL_219D 
      Description: 'External RSA4096 NONE NONE'
      Placeholder blob
      Test EL2GO_BLOB_TEST_EXTERNAL_219D - SKIPPED
    190 of 190 SKIPPED
    Test suite EXTERNAL (EL2GO_BLOB_TEST_EXTERNAL_2XXX) - PASSED
    
    #### Summary ####
    Test suite INTERNAL (EL2GO_BLOB_TEST_INTERNAL_10XX) - PASSED
    Test suite EXTERNAL (EL2GO_BLOB_TEST_EXTERNAL_2XXX) - PASSED
    
    #### EL2GO blob tests finished ####

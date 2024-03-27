.. _nxp_el2go_claimcode_encryption:

RD-RW612-BGA EL2GO Claimcode Encryption Application
#######################################

Overview
********

This demo demonstrates how to create and store the encrypted claim code blob on MCU devices;
the example is meant to be used together with the EdgeLock 2GO Agent example, which needs to be downloaded
afterwards. It will read the claim code blob and provide it to the EdgeLock 2GO service during device connection;
the EdgeLock 2GO service will decrypt and validate the blob and claim the device if the corresponding claim code
will be found in one of the device groups.

The source code for this sample application can be found at:
:zephyr_file:`modules/hal/nxp/mcux/middleware/nxp_iot_agent/ex/src/apps/el2go_claimcode_encryption.c`.

Prerequisites
*************
- Active EdgeLock 2GO account (https://www.nxp.com/products/security-and-authentication/secure-service-2go-platform/edgelock-2go:EDGELOCK-2GO)
- Any Serial communicator

Setup of the EdgeLock 2GO platform
**********************************
The documentation which explains how to setup the EdgeLock 2GO Account to
- Create device group
- Create and copy claim code for device group
- Get EdgeLock 2GO claim code public key (RW61x_Claiming_Public_Key)
can be found under the EdgeLock 2GO account under the Documentation tab.

Requirements
************

- Micro USB cable
- RD-RW61X-BGA board
- Personal Computer

Prepare the Demo
****************
1.  Provide the plain claim code string as created on EdgeLock 2GO service:

    in file modules/hal/nxp/mcux/middleware/nxp_iot_agent/ex/inc/iot_agent_demo_config.h
    #define IOT_AGENT_CLAIMCODE_STRING "insert_claimcode_from_el2go";

2.  Provide the EdgeLock 2GO claim code public key as read from EdgeLock 2GO service
    (in the format of a hexadecimal byte array): 

    in file modules/hal/nxp/mcux/middleware/nxp_iot_agent/inc/nxp_iot_agent_config_credentials.h
    #define NXP_IOT_AGENT_CLAIMCODE_KEY_AGREEMENT_PUBLIC_KEY_PROD
    #define NXP_IOT_AGENT_CLAIMCODE_KEY_AGREEMENT_PUBLIC_KEY_PROD_SIZE

3.  [Optional] The Flash address where the claim code will be written is by default located
    at 0x084A0000; this can be change by manipulating the following variable. Same address must
    be then applied also in the EdgeLock 2GO Agent example:

    in file modules/hal/nxp/mcux/middleware/nxp_iot_agent/ex/src/apps/el2go_claimcode_encryption.c
    #define CLAIM_CODE_INFO_ADDRESS

4.  [Optional] In case the example will be used in a device with secure boot mode enabled.
    The generated binary must be signed with the correct with OEM key.
    Additionaly if the example is supposed to run in the OEM CLOSED life cycle, the image needs to be encrypted
    with the OEM FW encryption key and loaded as an SB3.1 container (details on how to execute these steps can be found
    in the Application note AN13813 - Secure boot on RW61x downloadable from link
    https://www.nxp.com/products/wireless-connectivity/wi-fi-plus-bluetooth-plus-802-15-4/
wireless-mcu-with-integrated-tri-radio-1x1-wi-fi-6-plus-bluetooth-low-energy-5-3-802-15-4:RW612
    under Secure file section).

5.  Compile the project using the dedicated toolchain

6.  Connect a micro USB cable between the PC host and the MCU-Link USB port (J7) on the board.
7.  Open a serial terminal with the following settings:
    - 115200 baud rate
    - 8 data bits
    - No parity
    - One stop bit
    - No flow control
8.  Download the program to the target board. In case the image is signed the base address needs to be adjusted
    to 0x08001000.

Building, Flashing and Running
******************************

.. zephyr-app-commands::
   :zephyr-app: samples/boards/rd_rw612_bga/nxp_el2go_claimcode_encryption
   :board: rd_rw612_bga
   :goals: build flash
   :compact:

Flashing
********

  .. code-block:: console

    # You can find "zephyr.bin" and "zephyr.elf" under the "build/zephyr/" path.
    $ cd build/zephyr
    $ loadbin C:xxx\build\zephyr.bin, 0x08000000

Sample Output
=============

.. code-block:: console

Enabling ELS... done
Generating random ECC keypair... done
Calculating shared secret... done
Creating claimcode blob... done
claimcode (*): *** dynamic data ***
claimcode (*): *** dynamic data ***
claimcode (*): *** dynamic data ***
claimcode information written to flash at address 0x84a0000

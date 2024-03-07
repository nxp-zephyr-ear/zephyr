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
:zephyr_file:`modules/hal/nxp/mcux/middleware/nxp_iot_agent/ex/src/apps`.
Requirements
************

- Micro USB cable
- RD-RW61X-BGA board
- Personal Computer

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
claimcode information written to flash at address 0x 84a0000

.. _nxp_el2go_import_blob:

RD-RW612-BGA NXP EL2GO Import Blob Test Sample
##################################

Overview
********

With this application, EL2GO Secure Objects are imported from flash.

The source code for this application can be found at:
:zephyr_file:`modules/hal/nxp/mcux/middleware/nxp_iot_agent/ex/src/apps/psa_examples/el2go_import_blob`.

Requirements
************

- Micro USB cable
- RD-RW61X-BGA board
- Personal Computer

Building, Flashing and Running
******************************

.. zephyr-app-commands::
   :zephyr-app: samples/boards/rd_rw612_bga/nxp_el2go_import_blob
   :board: rd_rw612_bga_ns
   :goals: build flash
   :compact:

Sample Output
=============

.. code-block:: console

    X blob(s) imported from flash successfully
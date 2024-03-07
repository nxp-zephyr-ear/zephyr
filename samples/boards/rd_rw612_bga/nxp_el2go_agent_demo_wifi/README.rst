.. nxp_el2go_agent_demo_wifi:

RD-RW612-BGA NXP EL2GO Agent Demo Application (Wi-Fi)
#######################################

Overview
********

The EL2GO Agent Demo connects your device to an EL2GO endpoint via Wi-Fi and provisions any objects assigned to it.
Note: You need to at least set the defines EDGELOCK2GO_HOSTNAME, AP_SSID and AP_PASSWORD for it to work.

The source code for this application can be found at:
:zephyr_file:`modules/hal/nxp/mcux/middleware/nxp_iot_agent`.

Requirements
************

- Micro USB cable
- RD-RW61X-BGA board
- Personal Computer

Building, Flashing and Running
******************************

.. zephyr-app-commands::
   :zephyr-app: samples/boards/rd_rw612_bga/nxp_el2go_agent_demo_wifi
   :board: rd_rw612_bga_ns
   :goals: build flash
   :compact:

Sample Output
=============

.. code-block:: console

    [WRN] This device was provisioned with dummy keys. This device is NOT SECURE
    [Sec Thread] Secure image initializing!
    Booting TF-M v2.0.0
    Creating an empty ITS flash layout.
    Creating an empty PS flash layout.
    [INF][Crypto] Provisioning entropy seed... complete.
    MAC Address: MY_MAC_ADDRESS 
    PKG_TYPE: BGA
    Set BGA tx power table data 
    *** Booting Zephyr OS build RW-v3.6.0-441-g02fee8bf698d ***
    Connecting to SSID 'WIFI SSID' ...
    Using IPv4 address 172.20.10.2 @ Gateway 172.20.10.1 (DHCP)
    Using WIFI 6 (802.11ax/HE) @ 5GHz (Channel 149, -63 dBm)
    Successfully connected to WIFI
    Performance timing: DEVICE_INIT_TIME : 13784ms
    Start
    UID in hex format: MY_UUID
    UID in decimal format: MY_DECIMAL_UUID
    Updating device configuration from [MY_EL2GO_ID.device-link.edgelock2go.com]:[443].

.. nxp_el2go_agent_demo_wifi:

RD-RW612-BGA NXP EL2GO Agent Demo Application (Wi-Fi)
#######################################

Overview
********

This demo demonstrates how to use the EdgeLock 2GO service for provisioning keys and certificates into the MCU device.
Those keys and certificates can then be used to establish mutual-authenticated TLS connections to cloud services such as AWS or Azure.

The demo supports two modes for registering device to the EdgeLock 2GO service:
- UUID registration: at the start the demo is printing the UUID which can be used for registering it on the service
- Claiming: the EdgeLock 2GO Claim Code Encryption must be run in front, which will store the claim code blob
in the Flash memory. The EdgeLock 2GO Agent example will present the claim code to the EdgeLock 2GO service
and automatically register the device.

The device on which the example is running must have the secure boot enabled, otherwise the blob verification and
decryption keys can't be derived.

The source code for this application can be found at:
:zephyr_file:`modules/hal/nxp/mcux/middleware/nxp_iot_agent`.

Prerequisites
*************
- Active EdgeLock 2GO account (https://www.nxp.com/products/security-and-authentication/secure-service-2go-platform/edgelock-2go:EDGELOCK-2GO)
- Any Serial communicator

Setup of the EdgeLock 2GO platform
**********************************
The documentation which explains how to setup the EdgeLock 2GO Account to
- Create device group and whitelist device UUID
- Create and copy claim code for device group
- Create Secure Object
- Assign Secure Objects to device
can be found under the EdgeLock 2GO account under the Documentation tab.

Prepare the Demo
****************
1.  Provide the EdgeLock 2GO URL for the account (is in Admin settings section)

    in file modules/hal/nxp/mcux/middleware/nxp_iot_agent/inc/nxp_iot_agent_config.h
    #define EDGELOCK2GO_HOSTNAME

2.  Provide the wifi access point credentials

    in file modules/hal/nxp/mcux/middleware/nxp_iot_agent/ex/src/network/iot_agent_network_zephyr_wifi.c
    #define AP_SSID
    #define AP_PASSWORD

3.  [Optional] Only in case of claiming registration method the set to 1 the macro

    in file modules/hal/nxp/mcux/middleware/nxp_iot_agent/ex/inc/iot_agent_demo_config.h
    #define IOT_AGENT_CLAIMCODE_INJECT_ENABLE     1

    The Flash address from where the claim code will be read from Flash is set
    at 0x084A0000; this can be change by manipulating the following variable. The value should
    match the one used in EdgeLock 2GO Claim Code Encryption

    in file mmodules/hal/nxp/mcux/middleware/nxp_iot_agent/ex/src/utils/iot_agent_claimcode_inject.c
    #define CLAIM_CODE_INFO_ADDRESS

4.  [Optional] Only in case the user wants to use provisioned ECC key pairs and corresponding X.509 certificates
    to execute TLS mutual-authentication and MQTT message exchange with AWS and/or Azure clouds, set to 1 the macro:

    in file modules/hal/nxp/mcux/middleware/nxp_iot_agent/ex/inc/iot_agent_demo_config.h
    #define IOT_AGENT_COS_OVER_RTP_ENABLE     1

    In same file the following macros should be set to the object ID as defined on EdgeLock 2GO service:
    #define $SERVER$_SERVICE_KEY_PAIR_ID
    #define $SERVER$_SERVICE_DEVICE_CERT_ID

    Setting of other macros is server dependent and the meaning can be found on AWS/Azure documentation.
    By default the demo is executing the connection to both clouds when IOT_AGENT_COS_OVER_RTP_ENABLE is enabled;
    To disable one of two comment in the file modules/hal/nxp/mcux/middleware/nxp_iot_agent/ex/src/iot_agent_demo.c
    the calls to the service APIs under section "trigger MQTT connection RTP"

5.  [Optional] In order to maximize the TF-M ITS performance, the maximum supported blob size is set to 2908 bytes. In case
    the user wants to support bigger blobs (8K is the maximum size supported by PSA), he needs to change the following two variables:

    in file modules/tee/tf-m/trusted-firmware-m/platform/ext/target/nxp/$board$/config_tfm_target.h
    #define ITS_MAX_ASSET_SIZE                     3 * 0xC00

    in file modules/tee/tf-m/trusted-firmware-m/platform/ext/target/nxp/$board$/partition/flash_layout.h
    #define TFM_HAL_ITS_SECTORS_PER_BLOCK   (3)

6.  To correctly run the example, the secure boot mode on the device needs to be enabled. The bootheader needs to be removed
    from the SPE image, it has to be merged with the NSPE image and the resulting image must be signed with the OEM key.
    Additionaly, if the example is supposed to run in the OEM CLOSED life cycle, the image needs to be encrypted with
    the OEM FW encryption key and loaded as an SB3.1 container.
    Details on how to execute these steps can be found in the Application note AN13813 "Secure boot on RW61x", downloadable from
    https://www.nxp.com/products/wireless-connectivity/wi-fi-plus-bluetooth-plus-802-15-4/wireless-mcu-with-integrated-tri-radio-1x1-wi-fi-6-plus-bluetooth-low-energy-5-3-802-15-4:RW612
    in the "Secure Files" section.

7.  Connect a micro USB cable between the PC host and the MCU-Link USB port (J7) on the board.
8.  Open a serial terminal with the following settings:
    - 115200 baud rate
    - 8 data bits
    - No parity
    - One stop bit
    - No flow control
9. Download the Wifi FW as described in modules/hal/nxp/mcux/mcux-sdk/components/conn_fwloader/readme_rc.txt
10. Download the program to the target board. In case the image is signed the base address for downloading
    needs to be adjusted to 0x08001000.

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

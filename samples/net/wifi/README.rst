.. zephyr:code-sample:: wifi-shell
   :name: Wi-Fi shell
   :relevant-api: net_stats

   Test Wi-Fi functionality using the Wi-Fi shell module.

Overview
********

This sample allows testing Wi-Fi drivers for various boards by
enabling the Wi-Fi shell module that provides a set of commands:
scan, connect, and disconnect.  It also enables the net_shell module
to verify net_if settings.

Building and Running
********************

Verify the board and chip you are targeting provide Wi-Fi support.

For instance you can use TI's CC3220 by selecting the cc3220sf_launchxl board.

.. zephyr-app-commands::
   :zephyr-app: samples/net/wifi
   :board: cc3220sf_launchxl
   :goals: build
   :compact:

Sample console interaction
==========================

.. code-block:: console

   shell> wifi scan
   Scan requested
   shell>
   Num  | SSID                             (len) | Chan | RSSI | Sec
   1    | kapoueh!                         8     | 1    | -93  | WPA/WPA2
   2    | mooooooh                         8     | 6    | -89  | WPA/WPA2
   3    | Ap-foo blob..                    13    | 11   | -73  | WPA/WPA2
   4    | gksu                             4     | 1    | -26  | WPA/WPA2
   ----------
   Scan request done

   shell> wifi connect "gksu" 4 SecretStuff
   Connection requested
   shell>
   Connected
   shell>

Building, Flashing and Running
******************************

Verify the board and chip you are targeting provide Wi-Fi support.

For instance you can use Redfinch RW610.

.. build-wifi-example::
   :zephyr-app: samples/net/wifi
   :board: rd_rw612_bga
   :goals: build
   :compact:

You can find “zephyr.bin” and “zephyr.elf” under the "build/zephyr/" path.

Flashing:
.. flash-wifi-image::
   # Then you can download zephyr.bin to 0x08000000 with JLINK.
   # The CMD to write CPU3 image to flash in J-link window:
   $ loadbin C:xxx\zephyr.bin,0x08000000

For CPU1 and CPU2 image, please refer to modules/hal/nxp/mcux/components/fw_bin/readme.txt 

Sample console interaction
==========================

.. code-block:: console

   uart:~$ wlansh scan
   Scan scheduled...
   Command wlan-scan
   10 networks found:
   30:5F:77:7B:BC:ED  "Test_5G" Infra
        mode: 802.11AX
        channel: 40
        rssi: -49 dBm
        security: WPA2
        WMM: YES
        802.11V: YES
        802.11W: NA
        WPS: YES, Session: Not active
   ...
   uart:~$ wlansh wlan-add testSTA ssid Test_5G wpa2 123456789
   uart:~$ wlansh wlan-connect testSTA
   Connecting to network...
   ...
   app_cb: WLAN: connected to network
   ...

wifidriver APIs User Manual
===========================

1. AP setup
(1) Add AP
For static IP address assignment:
uart:~$ wlansh wlan-add testAP ssid FD-2 role uap ip:192.168.50.1,192.168.50.1,255.255.255.0 channel 6 wpa2 123456789

For Open mode assignment:
uart:~$ wlansh wlan-add testAP ssid FD-2 role uap ip:192.168.50.1,192.168.50.1,255.255.255.0

For WPA3 mode assignment:
uart:~$ wlansh wlan-add testAP ssid FD-2 role uap ip:192.168.50.1,192.168.50.1,255.255.255.0 wpa3 sae 123456789 mfpc 1 mfpr 1

For AP that supports up to 802.11ax:
uart:~$ wlansh wlan-add testAP ssid FD-2 role uap ip:192.168.50.1,192.168.50.1,255.255.255.0 channel 6 capa 11ax

For AP that supports up to 802.11ac:
uart:~$ wlansh wlan-add testAP ssid FD-2 role uap ip:192.168.50.1,192.168.50.1,255.255.255.0 channel 6 capa 11ac

For AP that supports up to 11n:
uart:~$ wlansh wlan-add testAP ssid FD-2 role uap ip:192.168.50.1,192.168.50.1,255.255.255.0 channel 6 capa 11n

List network:
uart:~$ wlansh wlan-start-network testAP

(2) Start AP
Start the AP whose profile_name is testAP:
uart:~$ wlansh wlan-start-network testAP

Get the AP channel:
uart:~$ wlansh wlan-get-uap-channel

Get wlan info:
uart:~$ wlansh wlan-info
Station not connected
uAP started as:
"testap2g"
        SSID: TEST-AP2g
        BSSID: 00:50:43:02:34:99
        mode: 802.11AX
        channel: 6
        role: uAP
        security: none
        wifi capability: 11ax
        user configure: 11ax

        IPv4 Address
        address: STATIC
                IP:             192.168.10.1
                gateway:        192.168.10.1
                netmask:        255.255.255.0

        IPv6 Addresses
        Link-Local   :  fe80::250:43ff:fe02:3499 (Preferred)
        rssi threshold: 0
Command wlan-info

Get wlan stat:
uart:~$ wlansh wlan-stat
Station disconnected (Active)
uAP started (Active)
Command wlan-stat

After STA associate to AP, list sta:
uart:~$ wlansh wlan-get-uap-sta-list
Number of STA = 1

STA 1 information:
=====================
MAC Address: C2:10:0B:BF:C6:2F
Power mfg status: power save
Rssi : -29 dBm

Command wlan-get-uap-sta-list

(3) Stop AP
Start the AP whose profile_name is testAP:
uart:~$ wlansh wlan-stop-network testap2g

(4) Remove AP network
Remove AP network:
uart:~$ wlansh wlan-remove testap2g

2. STA setup
AP information:
  < ssid>: FD-2
  <security>: WPA2-Personal, 123456789
  <channel>: 6

(1) Add STA
STA information:
  <profile_name>: testSTA
  <ssid>: FD-2
  <ip_addr>: 192.168.50.104
  <gateway_ip>: 192.168.50.1
  <channel_number>: 6

For static IP address assignment:
uart:~$ wlansh wlan-add testSTA ssid FD-2 ip:192.168.50.104,192.168.50.1,255.255.255.0 channel 6 wpa2 123456789

For DHCP IP address assignment:
uart:~$ wlansh wlan-add testSTA ssid FD-2 wpa2 123456789

For Open mode assignment:
uart:~$ wlansh wlan-add testSTA ssid FD-2

For WPA WPA2 mix mode assignment:
uart:~$ wlansh wlan-add test_sta ssid long2g wpa 0123456789 wpa2 0123456789

For WPA3 mode assignment:
uart:~$ wlansh wlan-add testSTA ssid FD-2 wpa3 sae 123456789 mfpc 1 mfpr 1

List network:
uart:~$ wlansh wlan-list
1 network:
"test_sta"
        SSID: long2g
        BSSID: 00:00:00:00:00:00
        channel: (Auto)
        role: Infra

        RSSI: -26dBm
        security: WPA/WPA2 Mixed
        Proactive Key Caching: Disabled

        IPv6 Addresses

        rssi threshold: 0
Command wlan-list

(2) Connection
Connect to the AP(eg. FD-2)
uart:~$ wlansh wlan-connect test
Connecting to network...
Use 'wlan-stat' for current connection status.
Command wlan-connect
Info: ml: SME: Trying to authenticate with 7c:10:c9:4e:42:50 (SSID='FD-2' freq=2462 MHz)
Info: ml: Trying to associate with 7c:10:c9:4e:42:50 (SSID='FD-2' freq=2462 MHz)
Info: ml: Associated with 7c:10:c9:4e:42:50
Info: ml: CTRL-EVENT-CONNECTED - Connection to 7c:10:c9:4e:42:50 completed [id=0 id_str=]
Info: ml: CTRL-EVENT-SUBNET-STATUS-UPDATE status=0
========================================
app_cb: WLAN: received event 1
========================================
app_cb: WLAN: authenticated to network
[00:00:07.197,898] <inf> net_stats: Global statistics:
[00:00:07.205,900] <inf> net_stats: IPv4 recv      1    sent    0       drop    1       forwarded       0
[00:00:07.215,803] <inf> net_stats: IP vhlerr      0    hblener 0       lblener 0
[00:00:07.225,213] <inf> net_stats: IP fragerr     0    chkerr  0       protoer 0
[00:00:07.234,531] <inf> net_stats: ICMP recv      0    sent    0       drop    0
[00:00:07.243,429] <inf> net_stats: ICMP typeer    0    chkerr  0
[00:00:07.251,876] <inf> net_stats: UDP recv       0    sent    0       drop    0
[00:00:07.260,749] <inf> net_stats: UDP chkerr     0
[00:00:07.268,408] <inf> net_stats: TCP bytes recv 0    sent    0
[00:00:07.276,690] <inf> net_stats: TCP seg recv   0    sent    0       drop    0
[00:00:07.285,571] <inf> net_stats: TCP seg resent 0    chkerr  0       ackerr  0
[00:00:07.294,820] <inf> net_stats: TCP seg rsterr 0    rst     0       re-xmit 0
[00:00:07.303,877] <inf> net_stats: TCP conn drop  0    connrst 0
[00:00:07.312,405] <inf> net_stats: Bytes received 50
[00:00:07.320,148] <inf> net_stats: Bytes sent     0
[00:00:07.327,807] <inf> net_stats: Processing err 0
[00:00:12.602,604] <inf> net_dhcpv4: Received: 192.168.50.65
========================================
app_cb: WLAN: received event 0
========================================
app_cb: WLAN: connected to network
Connected to following BSS:
SSID = [FD-2]
IPv4 Address: [192.168.50.65]

Get wlan info:
uart:~$ wlansh wlan-info
Station connected to:
"test_sta"
        SSID: long2g
        BSSID: 90:8D:78:25:04:E8
        channel: 6
        role: Infra

        RSSI: -26dBm
        security: WPA2
        Proactive Key Caching: Disabled

        IPv4 Address
        address: DHCP
                IP:             192.168.0.109
                gateway:        192.168.0.1
                netmask:        255.255.255.0

        IPv6 Addresses
        rssi threshold: 0
uAP not started
Command wlan-info

Get wlan stat:
uart:~$ wlansh wlan-stat
Station connected (Active)
uAP stopped
Command wlan-stat

(3) Reassociate
Do reassociate after connected:
uart:~$ wlansh wlan-reassociate
Reassociating to network...
Use 'wlan-stat' for current connection status.
Command wlan-reassociate
Info: ml: SME: Trying to authenticate with 90:8d:78:25:04:e8 (SSID='long2g' freq=2437 MHz)
Info: ml: Trying to associate with 90:8d:78:25:04:e8 (SSID='long2g' freq=2437 MHz)
Info: ml: Associated with 90:8d:78:25:04:e8
Info: ml: CTRL-EVENT-CONNECTED - Connection to 90:8d:78:25:04:e8 completed [id=0 id_str=]
Info: ml: CTRL-EVENT-SUBNET-STATUS-UPDATE status=0
========================================
app_cb: WLAN: received event 1
========================================
app_cb: WLAN: authenticated to network
========================================
app_cb: WLAN: received event 0
========================================
app_cb: WLAN: connected to network
Connected to following BSS:
SSID = [long2g]
IPv4 Address: [192.168.0.109]

(4) Disconnect
Disconnect:
uart:~$ wlansh wlan-disconnect
Info: ml: CTRL-EVENT-DISCONNECTED bssid=90:8d:78:25:04:e8 reason=3 locally_generated=1
Info: ml: CTRL-EVENT-DSCP-POLICY clear_all
========================================
app_cb: WLAN: received event 11
========================================
app_cb: disconnected
Command wlan-disconnect

(5) Remove STA network
Remove STA network:
uart:~$ wlansh wlan-remove test_sta
Removed "test_sta"
Command wlan-remove
uart:~$
uart:~$ wlansh wlan-list
0 networks
Command wlan-list

3. Set/Get MAC
Set MAC address:
uart:~$ wlansh wlan-set-mac 00:50:43:02:33:99
Command wlan-set-mac

Get MAC address:
uart:~$ wlansh wlan-mac
MAC address
STA MAC Address: 00:50:43:02:33:99
uAP MAC Address: 00:50:43:02:34:99
Command wlan-mac

4. IEEE Power Save
Before testing the IEEE PS mode, you need to connect to an AP.
#wlan-add test ssid net-2g ip:192.168.0.44,192.168.0.1,255.255.255.0 channel 1
#wlan-connect test

1) Enable IEEE Power Save
#wlan-ieee-ps 1
When no data traffic or no cmd dnld, FW may enter ieee ps mode.
Check mlan_adap.ps_state value is PS_STATE_SLEEP, then FW entered power save.

Check wlan-stat:
uart:~$ wlansh wlan-stat
Station connected (IEEE ps)
uAP stopped
Command wlan-stat

2) Disable IEEE Power Save
#wlan-ieee-ps 0
Check wlan-stat:
uart:~$ wlansh wlan-stat
Station disconnected (Active)
uAP stopped
Command wlan-stat

5. Deep Sleep Power Save
STA should not connect to an AP for Deep sleep power save mode.
1) Enable Deep Sleep Power Save
uart:~$ wlansh wlan-deep-sleep-ps 1
When no cmd dnld, FW may enter ieee ps mode.
Check mlan_adap.ps_state value is PS_STATE_SLEEP, then FW entered power save.

Check wlan-stat:
uart:~$ wlansh wlan-stat
Station disconnected (Deep sleep)
uAP stopped
Command wlan-stat

2) Disable Deep Sleep Power Save
uart:~$ wlansh wlan-deep-sleep-ps 0

Check wlan-stat:
uart:~$ wlansh wlan-stat
Station disconnected (Active)
uAP stopped
Command wlan-stat

6. WNM PS Mode Test
(1) Testing Environment Setup
Two equipments are required:
1) Wi-Fi board as STA(RW610)
2) Wi-Fi board as uAP(RW610), use embedded wifi_cli image.

(2) WNM PS mode test
1) Start AP
# wlan-add test ssid WNMtest ip:192.168.10.1,192.168.10.1,255.255.255.0 role uap channel 6
#  wlan-start-network test
2) Start STA and connect to AP
Start STA
# wlan-add test ssid WNMtest ip:192.168.10.2,192.168.10.1,255.255.255.0 channel 6
Connect to AP
# wlan-connect test
3) Enable WNM PS mode
Before enable WNM PS mode, need to disable IEEE PS mode.
# wlan-ieee-ps 0
Enable WNM PS and set sleep_interval to 20.
# wlan-wnm-ps 1 6
4) Disable WNM PS mode
# wlan-wnm-ps 0
5) Re-enable WNM PS mode
# wlan-wnm-ps 1 15
(3) Sniffer log
After STA connected to AP, STA would send out WNM-Sleep Mode request to ask for enter WNM-Sleep Mode, AP would send WNM-Sleep Mode Response to STA.
When user tried to disable WNM PS mode, STA would send out WNM-Sleep Mode request to ask for exit WNM-Sleep mode, AP would send WNM-Sleep Mode response to STA. And if RSN is used without management frame protection and a valid PTK is configured for the STA, AP shall send the current GTK to the STA using a group key handshake.

7. Max Clients Count Configuration
Usage:
wlan-set-max-clients-count  <max_clients_count>
Example:
#wlan-set-max-clients-count  16

8. RTS & Fragment Test
Disable AMPDU before the RTS and Fragment test.
(1) RTS Test
1) Start AP
# wlan-add test ssid AX6wpa3 role uap ip:192.168.0.1,192.168.0.1,255.255.255.0 channel 1
# wlan-start-network test
2) Enable RTS
Issue below command to enable RTS:
# wlan-rts <sta/uap> <rts threshold>
For example, enable rts and set rts threshold to 400:
# wlan-rts uap 400
3) Capture info from WireShark
NOTE：STA is similar to AP

(2) Fragment Test
1) Start AP
# wlan-add test ssid AX6wpa3 role uap ip:192.168.0.1,192.168.0.1,255.255.255.0 channel 1
# wlan-start-network test
2) Enable Fragment
Issue below command to enable Fragment:
# wlan-frag <sta/uap> <fragment threshold>
For example, enable fragment and set fragment threshold to 300:
# wlan-frag uap 300
3) STA ping AP
You need to specify the size of the ping packet. If the size of packet is too small, maybe you can't see the Fragment phenomenon.
e.g. set size to 1300.
#ping -c 3 -s 1300 192.168.0.1
4) Capture info from WireShark
You can see that the ping reply is divided into five Fragments. Because the AP threshold of  fragment is 300, when AP sends ping reply to STA, the size is over 300.
If this is the last fragment, the flag of More Fragments is set to 0, otherwise it is set to 1.
The Sequence number of the same packet is the same, but the Fragment number is different.
Note：STA is similar to AP

9. Scan Command
The scan command is used to scan the visible access points.
Scan for nearby APs
uart:~$ wlansh wlan-scan
Scan scheduled...
Command wlan-scan
2 networks found:

C8:9E:43:5A:6D:A9  "lhx_ap_roam" Infra
        mode: 802.11AX
        channel: 6
        rssi: -30 dBm
        security: WPA3 SAE
        WMM: YES
        802.11K: YES
        802.11V: YES
        802.11W: Capable, Required
        WPS: YES, Session: Not active

04:A1:51:AB:07:1F  "xue-2g" Infra
        mode: 802.11N
        channel: 7
        rssi: -38 dBm
        security: WPA2
        WMM: YES
        802.11W: NA
        WPS: YES, Session: Not active

Use wlan-scan-opt to scan with specific conditions.

Scan with specific ssid:
uart:~$ wlansh wlan-scan-opt ssid NXPOPEN
Scan for ssid "NXPOPEN" scheduled...
Command wlan-scan-opt
7 networks found:
1C:6A:7A:87:FF:B1  "NXPOPEN" Infra
        mode: 802.11N
        channel: 1
        rssi: -36 dBm
        security: WPA2
        WMM: YES
        802.11K: YES
        802.11V: YES
        802.11W: NA
        WPS: NO
F8:C2:88:74:92:52  "NXPOPEN" Infra
        mode: 802.11N
        channel: 1
        rssi: -67 dBm
        security: WPA2
        WMM: YES
        802.11K: YES
        802.11V: YES
        802.11W: NA
        WPS: NO
24:16:9D:3E:61:61  "NXPOPEN" Infra
        mode: 802.11N
        channel: 11
        rssi: -61 dBm
        security: WPA2
        WMM: YES
        802.11K: YES
        802.11V: YES
        802.11W: NA
        WPS: NO
24:16:9D:3E:61:6E  "NXPOPEN" Infra
        mode: 802.11AC
        channel: 52
        rssi: -56 dBm
        security: WPA2
        WMM: YES
        802.11K: YES
        802.11V: YES
        802.11W: NA
        WPS: NO
24:16:9D:A6:82:CE  "NXPOPEN" Infra
        mode: 802.11AC
        channel: 64
        rssi: -83 dBm
        security: WPA2
        WMM: YES
        802.11K: YES
        802.11V: YES
        802.11W: NA
        WPS: NO
1C:6A:7A:87:FF:BE  "NXPOPEN" Infra
        mode: 802.11AC
        channel: 161
        rssi: -32 dBm
        security: WPA2
        WMM: YES
        802.11K: YES
        802.11V: YES
        802.11W: NA
        WPS: NO
F8:C2:88:74:92:5D  "NXPOPEN" Infra
        mode: 802.11AC
        channel: 161
        rssi: -75 dBm
        security: WPA2
        WMM: YES
        802.11K: YES
        802.11V: YES
        802.11W: NA
        WPS: NO

Scan with specific ssid and channel:
uart:~$ wlansh wlan-scan-opt ssid NXPOPEN channel 1
Scan for ssid "NXPOPEN" scheduled...
Command wlan-scan-opt
2 networks found:
1C:6A:7A:87:FF:B1  "NXPOPEN" Infra
        mode: 802.11N
        channel: 1
        rssi: -46 dBm
        security: WPA2
        WMM: YES
        802.11K: YES
        802.11V: YES
        802.11W: NA
        WPS: NO
F8:C2:88:74:92:52  "NXPOPEN" Infra
        mode: 802.11N
        channel: 1
        rssi: -66 dBm
        security: WPA2
        WMM: YES
        802.11K: YES
        802.11V: YES
        802.11W: NA
        WPS: NO

Scan with specific ssid and rssi threshold better than -50db:
uart:~$ wlansh wlan-scan-opt ssid NXPOPEN rssi_threshold -50
Scan for ssid "NXPOPEN" scheduled...
Command wlan-scan-opt
2 networks found:
1C:6A:7A:87:FF:B1  "NXPOPEN" Infra
        mode: 802.11N
        channel: 1
        rssi: -38 dBm
        security: WPA2
        WMM: YES
        802.11K: YES
        802.11V: YES
        802.11W: NA
        WPS: NO
1C:6A:7A:87:FF:BE  "NXPOPEN" Infra
        mode: 802.11AC
        channel: 161
        rssi: -32 dBm
        security: WPA2
        WMM: YES
        802.11K: YES
        802.11V: YES
        802.11W: NA
        WPS: NO

Scan with specific bssid:
uart:~$ wlansh wlan-scan-opt bssid 1C:6A:7A:87:FF:B1
Scan for bssid 1C:6A:7A:87:FF:B1 scheduled...
Command wlan-scan-opt
1 network found:
1C:6A:7A:87:FF:B1  "NXPOPEN" Infra
        mode: 802.11N
        channel: 1
        rssi: -42 dBm
        security: WPA2
        WMM: YES
        802.11K: YES
        802.11V: YES
        802.11W: NA
        WPS: NO

10. Ping Command
Please wait DHCP success before ping command.
DHCP success has following log:
Connected to following BSS:
SSID = [TPAX5G]
IPv4 Address: [192.168.1.103]

Ping command brief:
net ping [-s <packet_size>] [-c <packet_count>] <ip_address>

The default (packet_count: 3) :
uart:~$ net ping 192.168.50.132

Specify package size:
uart:~$ net ping -s 100 192.168.50.132

Specify packet count:
uart:~$ net ping -c 10 192.168.50.132

11. zperf
(1) TCP iperf
Redfinch connects to AP.
As iperf client (TCP TX)
uart:~$ zperf tcp upload 192.168.50.132 5001 10 1470 114M

As iperf server (TCP RX)
uart:~$ zperf tcp download 5001

Stop TCP iperf server
uart:~$ zperf tcp download stop

(2) UDP iperf
Redfinch connects to AP.
As iperf client (UDP TX)
uart:~$ zperf udp upload -a 192.168.50.132 5001 10 1470 114M

As iperf server (UDP RX)
uart:~$ zperf udp download 5001

Stop UDP iperf server
uart:~$ zperf udp download stop

Bind to the specified interface
Check the interface name
uart:~$ device list
- clkctl@21000 (READY)
- clkctl@1000 (READY)
- hsgpio@1 (READY)
- hsgpio@0 (READY)
- random@14000 (READY)
- flexcomm@109000 (READY)
  requires: clkctl@21000
- ua (READY)
- ml (READY)

Start iperf client(UDP TX), and bind to “ua” interface
uart:~$ zperf udp upload -a 192.168.50.132 5001 10 1470 114M ua

12. 802.1X (WPA2/3-Enterprise) test
(1) Testing Environment Setup
Two boards are required:
1) Wi-Fi board as STA(RW610)
2) Wi-Fi board as AP(RW610)

(2) CA and Key files
NOTE:
1) the default key size is rsa:3072 in ca-cert.h  client-cert.h  client-key.h  dh-param.h  server-cert.h  server-key.h
2) If macro CONFIG_WIFI_USB_FILE_ACCESS is defined, CA & key will only be read from USB. If not defined, will only read the default CA & Key. (Currently, zephyr RW610 does not support reading CA and key files from USB.)

(3) Prepare and convert CA and Key files
If you need to use new CA and Key files, please follow the steps below.
Get CA and Key from Radius (Hostapd in Linux)
The following is an example files, please refer to the CA & Key files provided by your RADIUS server.

Convert CA format form PEM to DER.
openssl x509 -outform der -in cas.pem -out ca.der
openssl x509 -outform der -in wifiuser.pem -out client.der

Convert RSA Key format to DER.
openssl rsa -in wifiuser.key -outform DER -out client_key.der

Copy the contents of the ca.der file to array ca_der in “wifi_nxp\certs\ca-cert.h”.
Copy the contents of the client.der file to array client_der in “wifi_nxp\certs\client-cert.h”.
Copy the contents of the client_key.der file to array client_key_der in “wifi_nxp\certs\client-key.h”.

(4) Test commands
client:
wlansh wlan-set-mac 00:50:43:02:11:22
(Currently, zephyr RW610 does not support reading CA and key files from USB.)
#NOTE: please confirm the file name
wlan-read-usb-file ca-cert 1:/hca.der
wlan-read-usb-file client-cert 1:/hcl.der
wlan-read-usb-file client-key 1:/hck.der

server:
wlansh wlan-set-mac 00:50:43:02:11:33
(Currently, zephyr RW610 does not support reading CA and key files from USB.)
#NOTE: please confirm the file name
wlan-read-usb-file ca-cert 1:/hca.der
wlan-read-usb-file server-cert 1:/hsc.der
wlan-read-usb-file server-key 1:/hsk.der

1) EAP_TLS:
UAP WPA2:
wlansh wlan-set-mac 00:50:43:02:11:22
wlansh wlan-add eap_tls_test ssid EapNet_AP ip:192.168.10.1,192.168.10.1,255.255.255.0 channel 149 role uap eap-tls id client1 key_passwd whatever
wlansh wlan-start-network eap_tls_test

STA WPA2:
wlansh wlan-set-mac 00:50:43:02:11:33
wlansh wlan-add eap_tls_test ssid EapNet_AP eap-tls aid client1 key_passwd whatever
wlansh wlan-connect eap_tls_test

UAP WPA3 suite B:
wlansh wlan-set-mac 00:50:43:02:11:22
wlansh wlan-add eap_tls_test ssid EapNet_AP ip:192.168.10.1,192.168.10.1,255.255.255.0 channel 149 role uap wpa3-sb eap-tls id client1 key_passwd whatever mfpc 1 mfpr 1
wlansh wlan-start-network eap_tls_test

STA WPA3 suite B:
wlansh wlan-set-mac 00:50:43:02:11:33
wlansh wlan-add eap_tls_test ssid EapNet_AP wpa3-sb eap-tls aid client1 key_passwd whatever mfpc 1 mfpr 1
wlansh wlan-connect eap_tls_test

UAP WPA3 suite B-192:
wlansh wlan-set-mac 00:50:43:02:11:22
wlansh wlan-add eap_tls_test ssid EapNet_AP ip:192.168.10.1,192.168.10.1,255.255.255.0 channel 149 role uap gmc 4096 wpa3-sb-192 eap-tls id client1 key_passwd whatever mfpc 1 mfpr 1
wlansh wlan-start-network eap_tls_test

STA WPA3 suite B-192:
wlansh wlan-set-mac 00:50:43:02:11:33
wlansh wlan-add eap_tls_test ssid EapNet_AP gmc 4096 wpa3-sb-192 eap-tls aid client1 key_passwd whatever mfpc 1 mfpr 1
wlansh wlan-connect eap_tls_test

2) PEAPv0-MSCHAPv2
UAP WPA2:
wlansh wlan-add peap_test ssid EapNet_AP ip:192.168.10.1,192.168.10.1,255.255.255.0 channel 149 role uap eap-peap-mschapv2 ver 0 aid client1 id nxp pass wireless key_passwd whatever
wlansh wlan-start-network peap_test

STA WPA2:
wlansh  wlan-set-mac 00:50:43:02:11:33
wlansh wlan-add peap_test ssid EapNet_AP eap-peap-mschapv2 ver 0 aid client1 id nxp pass wireless key_passwd whatever
wlansh wlan-connect peap_test

UAP WPA3 suite B:
wlansh wlan-add peap_test ssid EapNet_AP ip:192.168.10.1,192.168.10.1,255.255.255.0 channel 149 role uap wpa3-sb eap-peap-mschapv2 ver 0 aid client1 id nxp pass wireless key_passwd whatever mfpc 1 mfpr 1
wlansh wlan-start-network peap_test
STA WPA3 suite B:
wlansh  wlan-set-mac 00:50:43:02:11:33
wlansh wlan-add peap_test ssid EapNet_AP wpa3-sb eap-peap-mschapv2 ver 0 aid client1 id nxp pass wireless key_passwd whatever mfpc 1 mfpr 1
wlansh wlan-connect peap_test

UAP WPA3 suite B-192:
wlansh wlan-add peap_test ssid EapNet_AP ip:192.168.10.1,192.168.10.1,255.255.255.0 channel 149 role uap gmc 4096 wpa3-sb-192 eap-peap-mschapv2 ver 0 aid client1 id nxp pass wireless key_passwd whatever mfpc 1 mfpr 1
wlansh wlan-start-network peap_test
STA WPA3 suite B-192:
wlansh wlan-set-mac 00:50:43:02:11:33
wlansh wlan-add peap_test ssid EapNet_AP gmc 4096 wpa3-sb-192 eap-peap-mschapv2 ver 0 aid client1 id nxp pass wireless key_passwd whatever mfpc 1 mfpr 1
wlansh wlan-connect peap_test

3) PEAPv1-GTC
UAP WPA2:
wlansh wlan-add eap_gtc_test ssid EapNet_AP ip:192.168.10.1,192.168.10.1,255.255.255.0 channel 149 role uap eap-peap-gtc aid client1 id nxp pass wireless key_passwd whatever
wlansh wlan-start-network eap_gtc_test
STA WPA2:
wlansh wlan-set-mac 00:50:43:02:11:33
wlansh wlan-add eap_gtc_test ssid EapNet_AP eap-peap-gtc aid client1 id nxp pass wireless key_passwd whatever
wlansh wlan-connect eap_gtc_test

UAP WPA3 suite B:
wlansh wlan-add eap_gtc_test ssid EapNet_AP ip:192.168.10.1,192.168.10.1,255.255.255.0 channel 149 role uap wpa3-sb eap-peap-gtc aid client1 id nxp pass wireless key_passwd whatever mfpc 1 mfpr 1
wlansh wlan-start-network eap_gtc_test
STA WPA3 suite B:
wlansh wlan-set-mac 00:50:43:02:11:33
wlansh wlan-add eap_gtc_test ssid EapNet_AP wpa3-sb eap-peap-gtc aid client1 id nxp pass wireless key_passwd whatever mfpc 1 mfpr 1
wlansh wlan-connect eap_gtc_test

UAP WPA3 suite B-192:
wlansh wlan-add eap_gtc_test ssid EapNet_AP ip:192.168.10.1,192.168.10.1,255.255.255.0 channel 149 role uap gmc 4096 wpa3-sb-192 eap-peap-gtc aid client1 id nxp pass wireless key_passwd whatever mfpc 1 mfpr 1
wlansh wlan-start-network eap_gtc_test
STA WPA3 suite B-192:
wlansh wlan-set-mac 00:50:43:02:11:33
wlansh wlan-add eap_gtc_test ssid EapNet_AP gmc 4096 wpa3-sb-192 eap-peap-gtc aid client1 id nxp pass wireless key_passwd whatever mfpc 1 mfpr 1
wlansh wlan-connect eap_gtc_test

4) EAP-TTLS-MSCHAPv2
UAP WPA2:
wlansh wlan-add eap_ttls_test ssid EapNet_AP ip:192.168.10.1,192.168.10.1,255.255.255.0 channel 149 role uap eap-ttls-mschapv2 aid client1 id nxp pass wireless key_passwd whatever
wlansh wlan-start-network eap_ttls_test
STA WPA2:
wlansh wlan-set-mac 00:50:43:02:11:33
wlansh wlan-add eap_ttls_test ssid EapNet_AP eap-ttls-mschapv2 aid client1 id nxp pass wireless key_passwd whatever
wlansh wlan-connect eap_ttls_test

UAP WPA3 suite B:
wlansh wlan-add eap_ttls_test ssid EapNet_AP ip:192.168.10.1,192.168.10.1,255.255.255.0 channel 149 role uap wpa3-sb eap-ttls-mschapv2 aid client1 id nxp pass wireless key_passwd whatever mfpc 1 mfpr 1
wlansh wlan-start-network eap_ttls_test
STA WPA3 suite B:
wlansh wlan-set-mac 00:50:43:02:11:33
wlansh wlan-add eap_ttls_test ssid EapNet_AP wpa3-sb eap-ttls-mschapv2 aid client1 id nxp pass wireless key_passwd whatever mfpc 1 mfpr 1
wlansh wlan-connect eap_ttls_test

UAP WPA3 suite B-192:
wlansh wlan-add eap_ttls_test ssid EapNet_AP ip:192.168.10.1,192.168.10.1,255.255.255.0 channel 149 role uap gmc 4096 wpa3-sb-192 eap-ttls-mschapv2 aid client1 id nxp pass wireless key_passwd whatever mfpc 1 mfpr 1
wlansh wlan-start-network eap_ttls_test
STA WPA3 suite B-192:
wlansh wlan-set-mac 00:50:43:02:11:33
wlansh wlan-add eap_ttls_test ssid EapNet_AP gmc 4096 wpa3-sb-192 eap-ttls-mschapv2 aid client1 id nxp pass wireless key_passwd whatever mfpc 1 mfpr 1
wlansh wlan-connect eap_ttls_test

13. Tx ampdu prot mode(force RTS)
This command is used to set either RTS/CTS or CTS2SELF protection mechanism in MAC, for aggregated Tx QoS data frames. RTS/CTS is enabled by default.
(1) Usage:
wlansh wlan-tx-ampdu-prot-mode <mode>
<mode>: 0 - Set RTS/CTS mode
                  1 - Set CTS2SELF mode
                  2 - Disable Protection mode
                  3 - Set Dynamic RTS/CTS mode
NOTE:
- 0: force RTS/CTS.
- 1: force RTS to SELF.
- 3: fw will send RTS when CCA is high.

(2) Example:
Get currently set protection mode for TX AMPDU.
# wlansh wlan-tx-ampdu-prot-mode
Set protection mode for TX AMPDU to CTS2SELF.
# wlansh wlan-tx-ampdu-prot-mode 1

14. Set RSSI low threshold
This command is used to set the RSSI threshold for 11k, 11v, 11r or roaming case, default value is -70 dBm.
(1) Usage:
wlansh wlan-rssi-low-threshold <threshold_value>
(2) Example:
Set the RSSI threshold as -60 dBm.
# wlansh wlan-rssi-low-threshold 60

15. Set/Get Rx Abort Configuration
This command is used to set/get static rx abort config for pkt having weaker RSSI than threshold. This threshold will be overwritten on starting. dynamic rx abort cfg ext.
Usage:
Get rx  abort config:
wlansh wlan-rx-abort-cfg
set rx  abort config:
wlansh wlan-rx-abort-cfg  <enable>   <rssi_threshold>
Options:
<enable>: 0 – disable
          1 – enable
<rssi_threshold> : weak RSSI pkt threshold in dBm (absolute value, default = 70)
For exanples:
wlansh wlan-rx-abort-cfg         -- Get current rx abort config and command usage
wlansh wlan-rx-abort-cfg 1 40    -- Enable rx abort and set weak RSSI threshold to –40 dBm
wlansh wlan-rx-abort-cfg 0       -- Disbale rx abort

16. Set/Get RX Abort Configuration ext
This command is used to set/get dynamic rx abort config for pkt having  weaker RSSI than threshold. Threshold value is in absolute value of rssi in dBm.
If set ceil_rssi_thresh to 0xff, firmware will set ceil RSSI threshold based on  EDMAC status. Set ceil RSSI threshold to EDMAC value if EDMAC is enabled otherwise set ceil RSSI threshold to default value(-62dBm). Firmware will set ceil RSSI threshold to the opposite of this value if ceil_rssi_thresh is a valid value(rather than 0xff). If set floor_rssi_thresh to 0xff, firmware will set floor RSSI threshold to default value(-82 dbm). If floor_rssi_thresh is valid(rather than 0xff), firmware will set floor RSSI threshold to the opposite of this value.
Compare the value of ‘[min RSSI among all connected peers] – margin’ and ceil RSSI threshold, select the minimum value and compare it with floor RSSI threshold. If the value of floor RSSI threshold is larger, dynamic rx abort RSSI threshold will be updated based on floor RSSI threshold, otherwise update dynamic rx abort RSSI threshold with another value.
If hardware receives a packet which RSSI is less than dynamic rx abort RSSI threshold, hardware will ignore it rather than pull up CCA to block TX.

Usage:
Get Dynamic rx abort cfg:
wlansh wlan-get-rx-abort-cfg-ext
Set Dynamic rx abort cfg:
wlansh wlan-set-rx-abort-cfg-ext enable <enable> margin <margin> ceil <ceil_thresh> floor <floor_thresh>
Options:
    enable <enable>
              0 -- Disable Rx abort
              1 -- Enable Rx abort of pkt having weak RSSI
    margin <margin>
              rssi margin in dBm (absolute val)
              (default = 10)
    ceil <ceil_rssi_thresh>
              ceiling weak RSSI pkt threshold in dBm (absolute val)
              (default = 62)
    floor <floor_rssi_thresh>
              floor weak RSSI pkt threshold in dBm (absolute val)
              (default = 82)

For example:
wlansh wlan-get-rx-abort-cfg-ext
  Display current rx abort configuration
wlansh wlan-set-rx-abort-cfg-ext enable 1 margin 5 ceil 40 floor 70
  Enable dynamic rx abort,set margin to -5 dBm
  set ceil RSSI Threshold to -40 dBm and set floor RSSI threshold to -70 dbm
wlansh wlan-set-rx-abort-cfg-ext enable 1
  Don't set RSSI margin, drive will set defult RSSI margin threshold value.
  Don't set ceil RSSI threshold, driver will set default ceil RSSI threshold value.
  Don't set floor RSSI threshold, driver will set default floor RSSI threshold value.
wlansh wlan-set-rx-abort-cfg-ext enable 1 ceil 255
  Don't set RSSI margin, drive will set defult RSSI margin threshold value.
  Input ceil RSSI threshold to 0xff, set ceil value to default based on EDMAC enabled or disabled status.
  In this case, don't set floor RSSI threshold.
wlansh wlan-set-rx-abort-cfg-ext enable 0
  Disable dynamic rx abort

17. CCK Desenses Configuration
This command is used to configure CCK (802.11b) Desensitization RSSI threshold. All CCK traffic beyond this threshold will be ignored, resulting in higher Tx throughput. Threshold value is in absolute value of rssi in dBm. In dynamic and enhanced modes, cck desense will be turned on only inpresence of an active connection and the effective CCK desense RSSI threshold will be updated every rateadapt interval, based on: min{ceil_thresh, [min RSSI among all connected peers] - margin} Further, for dynamic enhanced mode, CCK desense will be turned on/off based on environment noise condition and ongoing Tx traffic rate. In this mode, CCK desense will also be turned off periodically in order to allow 802.11b Rx frames from Ext-AP, if rx rssi becomes weaker than the current threshold. Turn on and off intervals are specified in terms of rateadapt intervals. Please note that in this mode, if dynamic Rx Abort is enabled, then it will turn on/off in sync with cck desense.
Usage:
wlansh wlan-cck-desense-cfg [mode] [margin] [ceil_thresh] [num_on_intervals] [num_off_intervals]

Options:
        [mode] : 0 - Disable cck desense
                 1 - Enable dynamic cck desense mode
                 2 - Enable dynamic enhanced cck desense mode
        [margin] : rssi margin in dBm (absolute value, default = 10)
        [ceil_thresh] : ceiling weak RSSI pkt threshold in dBm (absolute value, default = 70)
        [num_on_intervals] : number of rateadapt intervals to keep cck desense "on" [for mode 2 only] (default = 20)
        [num_off_intervals]: number of rateadapt intervals to keep cck desense "off" [for mode 2 only] (default = 3)

Examples:
wlansh wlan-cck-desense-cfg -- Display current cck desense configuration and command usage.
wlansh wlan-cck-desense_cfg 1 10 50 -- Set dynamic mode, margin to -10 dBm and ceil RSSI threshold to -50 dBm
wlansh wlan-cck-desense-cfg 2 5 60 -- Set dynamic enhanced mode, set margin to -5 dBm, set ceil RSSI threshold to -60 dBm and retain previous num on/off interval setting.
wlansh wlan-cck-desense-cfg 0 -- Disable cck desense

18. Wi-Fi Protected Setup(WPS) test
(1) EXT-AP configuration
SCBT-AP as an example, enter the Web-GUI and select “WiFi Protected Setup” and press “START”.
(2) Redfinch STA configuration(role: STA enrollee)
1) Enrollee PBC usage:
wlansh wlan-start-wps-pbc
Example:
When EXT-AP starts WPS-PBC, STA just needs to enter cmd "wlansh wlan-start-wps-pbc" to start this connection.

2) Enrollee PIN usage:
wlansh wlan-generate-wps-pin
wlansh wlan-start-wps-pin <8 digit pin>
Example:
Use the cmd “wlansh wlan-generate-wps-pin” to generate 8 digit pin code, then EXT-AP starts WPS-PIN with the specific pin code and STA just needs to enter cmd " wlansh wlan-start-wps-pin <8 digit pin>" to start this connection.

3) Cancel WPS session usage:
wlansh wlan-wps-cancel
Example:
When WPS session starts and before connected to EXT-AP, user can enter cmd “wlansh wlan-wps-cancel” to cancel the WPS session.

(3) Redfinch AP configuration (role: uap registrar):
1) Refer to chapter “3.1 AP setup” to add a uap profile/interface with secure WPA2.
2) Registrar PBC usage:
wlansh wlan-start-ap-wps-pbc
Example:
When Redfinch uap starts WPS-PBC, press the "WPS-PBC" icon with an android phone that supports WPS2.0.
3) Registrar PIN usage:
wlansh wlan-start-ap-wps-pin <8 digit pin>
Example:
First, use an android phone that supports WPS2.0, press "WPS-PIN" to get the pin code of the enrollee side, and then input the pin code to the back of the Redfinch cmd " wlansh wlan-start-ap-wps-pin ".
4) Cancel registrar WPS session usage:
wlansh wlan-wps-ap-cancel
Example:
When WPS session starts and before STA connected, user can enter cmd “wlansh wlan-wps-ap-cancel” to cancel the WPS session.

19. Set/Get TSP Configuration
This command is used to set/get Thermal Safeguard Protection (TSP) configuration.
Usage:
Get TSP configuration:
wlansh wlan-get-tsp-cfg
Set TSP configuration:
wlansh wlan-set-tsp-cfg enable <enable> backoff <backoff> high <highThreshold> low <lowThreshold>
               <enable>: 0 -- disable   1 -- enable
               <backoff>: power backoff [0...10]dB
               <highThreshold>: High power Threshold [0...300]°C
               <lowThreshold>: Low power Threshold [0...300]°C
               High Threshold must be greater than Low Threshold
Example:
wlansh wlan-set-tsp-cfg enable 1 backoff 5 high 230 low 100  - - Set TSP values
wlansh wlan-get-tsp-cfg                                                                   - - Get TSP values
TSP Configuration:
        Enable TSP Algorithm: 1
                   0: disable 1: enable
        Power Management Backoff: 5 dB
        Low Power BOT Threshold: 100 °C
        High Power BOT Threshold: 230 °C

20. Get the signal info
This command gets the last and average value of RSSI, SNR and NF of Beacon and Data.
Note: This command is available only when STA is connected.
1) Usage:
wlansh wlan-get-signal
2) Example:
wlansh wlan-get-signal

21. IPS enable or disable
This command is used to enable and disable the IPS (IEEE PS mode power optimization).
Usage:
wlansh wlan-set-ips option <enable>
option:
          enable/disable
Example:
wlansh wlan-set-ips 0(disable the IPS)
wlansh wlan-set-ips 1(enable the IPS)

22. Set 802.11 AX OBSS Narrow Bandwidth RU Tolerance Time
In uplink transmission, AP sends a trigger frame to all the stations that will be involved in the upcoming transmission, and then these stations transmit Trigger-based(TB) PPDU in response to the trigger frame.
If STA connects to AP which channel is set to 100,STA doesn't support 26 tones RU.
1) Usage
wlansh wlan-set-toltime <value>
value: [1…3600]
Tolerance Time is in unit of seconds.
STA periodically check AP's beacon for ext cap bit79 (OBSS Narrow bandwidth RU in ofdma tolerance support) and set 20 tone RU tolerance time if ext cap bit79 is not set.
2) Example
Set AP to channel 100 and AX only mode. AP should support OBSS.

Before connect to AP, set tolerance time:
wlansh wlan-set-toltime 8

Connected to AP and wait for 8s, enter
wlansh wlan-mem-access 0x401051d8

The bit4 of this register should be set 1.

23. Set/Get MMSF config
These commands are used to specify/get 11ax density config.
Usage:
Set:
wlansh wlan-set-mmsf <enable> <Denisty> <MMSF>
Get:
wlansh wlan-get-mmsf
enable:
              0: disable
              1: enable
Density:
              Range: 0x0~0xFF. Default value is 0x30.
MMSF:
              Range: 0x0~0xFF. Default value is  0x6.

Example:
# wlansh wlan-set-mmsf 1 0x34 0x12
Success to set MMSF config.
# wlansh wlan-get-mmsf
MMSF configure:
Enable MMSF: Enable
Density: 0x34
MMSF: 0x12

24. Set/Get Turbo mode
These commands are used to set/get STA/UAP turbo mode.
Usage:
Get STA/UAP current turbo mode:
wlansh wlan-get-turbo-mode  <STA/UAP>
Set STA/UAP turbo mode:
wlansh wlan-set-turbo-mode <STA/UAP>  <mode>
mode:
0: Disable turbo mode
1: Turbo mode 1
2: Turbo mode 2
3: Turbo mode 3

Example:
# wlansh wlan-get-turbo-mode STA
  STA turbo mode: 3
# wlansh wlan-add sta ssid test
# wlansh wlan-connect sta
# wlansh wlan-get-turbo-mode UAP
   UAP turbo mode: 3
# wlansh wlan-set-turbo-mode UAP 2
  Set UAP turbo mode to 2
# wlansh wlan-get-turbo-mode UAP
   UAP turbo mode: 2

25. Set country code
1) Usage:
wlan-set-country <country_code_str 3 bytes>
Country Code Options:
  WW  (World Wide Safe)
  US  (US FCC)
  CA  (IC Canada)
  SG  (Singapore)
  EU  (ETSI)
  AU  (Australia)
  KR  (Republic Of Korea)
  FR  (France)
  JP  (Japan)
  CN  (China)
2) Example
# wlansh wlan-set-country US
  Set country code US is successful
  Command wlan-set-country

26. Set duty cycle
1) Usage:
Set single ant duty cycle:
wlan-single-ant-duty-cycle <enable/disable> [<Ieee154Duration> <TotalDuration>]
Options:
<enable/disable>:
  1 - Enable
  0 - Disable
<Ieee154Duration>: Enter value in Units (1Unit = 1ms), no more than TotalDuration
<TotalDuration>: Enter value in Units (1Unit = 1ms), total duty cycle time
Ieee154Duration should not equal to TotalDuration-Ieee154Duration

Set dual ant duty cycle:
wlan-dual-ant-duty-cycle <enable/disable>
[<Ieee154Duration> <TotalDuration> <Ieee154FarRangeDuration>]
Options:
<enable/disable>:
  1 - Enable
  0 - Disable
<Ieee154Duration>: Enter value in Units (1Unit = 1ms), no more than TotalDuration
<TotalDuration>: Enter value in Units (1Unit = 1ms), total duty cycle time
<Ieee154FarRangeDuration>: Enter value in Units (1Unit = 1ms)
Ieee154Duration, TotalDuration - Ieee154Duration and Ieee154FarRangeDuration should not equal to each other

2) Example:
Enable single-ant-duty-cycle or dual-ant-duty-cycle
uart:~$ wlansh wlan-single-ant-duty-cycle 1 32 62
Set single ant duty cycle successfully
Command wlan-single-ant-duty-cycle
uart:~$ wlansh wlan-dual-ant-duty-cycle 1 5 35 32
Set dual ant duty cycle successfully
Command wlan-dual-ant-duty-cycle

Disable single-ant-duty-cycle or dual-ant-duty-cycle
uart:~$ wlansh wlan-single-ant-duty-cycle 0
uart:~$ wlansh wlan-dual-ant-duty-cycle 0

27. Set/Get channel list
This cmd is used to set/get 2G/5G channel list configuration.
1) Usage:
wlan-set-chanlist
wlan-get-chanlist

2)Example
Set channel list,
# wlansh wlan-set-chanlist
--------------------------------------------------------------------------------
Number of channels configured: 39
ChanNum: 1      ChanFreq: 2412  Active
ChanNum: 2      ChanFreq: 2417  Active
ChanNum: 3      ChanFreq: 2422  Active
ChanNum: 4      ChanFreq: 2427  Active
ChanNum: 5      ChanFreq: 2432  Active
ChanNum: 6      ChanFreq: 2437  Active
ChanNum: 7      ChanFreq: 2442  Active
ChanNum: 8      ChanFreq: 2447  Active
ChanNum: 9      ChanFreq: 2452  Active
ChanNum: 10     ChanFreq: 2457  Active
ChanNum: 11     ChanFreq: 2462  Active
ChanNum: 12     ChanFreq: 2467  Passive
ChanNum: 13     ChanFreq: 2472  Passive
ChanNum: 14     ChanFreq: 2484  Passive
ChanNum: 36     ChanFreq: 5180  Active
ChanNum: 40     ChanFreq: 5200  Active
ChanNum: 44     ChanFreq: 5220  Active
ChanNum: 48     ChanFreq: 5240  Active
ChanNum: 52     ChanFreq: 5260  Passive
ChanNum: 56     ChanFreq: 5280  Passive
ChanNum: 60     ChanFreq: 5300  Passive
ChanNum: 64     ChanFreq: 5320  Passive
ChanNum: 100    ChanFreq: 5500  Passive
ChanNum: 104    ChanFreq: 5520  Passive
ChanNum: 108    ChanFreq: 5540  Passive
ChanNum: 112    ChanFreq: 5560  Passive
ChanNum: 116    ChanFreq: 5580  Passive
ChanNum: 120    ChanFreq: 5600  Passive
ChanNum: 124    ChanFreq: 5620  Passive
ChanNum: 128    ChanFreq: 5640  Passive
ChanNum: 132    ChanFreq: 5660  Passive
ChanNum: 136    ChanFreq: 5680  Passive
ChanNum: 140    ChanFreq: 5700  Passive
ChanNum: 144    ChanFreq: 5720  Passive
ChanNum: 149    ChanFreq: 5745  Passive
ChanNum: 153    ChanFreq: 5765  Passive
ChanNum: 157    ChanFreq: 5785  Passive
ChanNum: 161    ChanFreq: 5805  Passive
ChanNum: 165    ChanFreq: 5825  Passive
Command wlan-set-chanlist

Get channel list,
# wlansh wlan-get-chanlist
--------------------------------------------------------------------------------
Number of channels configured: 36
ChanNum: 1      ChanFreq: 2412  Active
ChanNum: 2      ChanFreq: 2417  Active
ChanNum: 3      ChanFreq: 2422  Active
ChanNum: 4      ChanFreq: 2427  Active
ChanNum: 5      ChanFreq: 2432  Active
ChanNum: 6      ChanFreq: 2437  Active
ChanNum: 7      ChanFreq: 2442  Active
ChanNum: 8      ChanFreq: 2447  Active
ChanNum: 9      ChanFreq: 2452  Active
ChanNum: 10     ChanFreq: 2457  Active
ChanNum: 11     ChanFreq: 2462  Active
ChanNum: 36     ChanFreq: 5180  Active
ChanNum: 40     ChanFreq: 5200  Active
ChanNum: 44     ChanFreq: 5220  Active
ChanNum: 48     ChanFreq: 5240  Active
ChanNum: 52     ChanFreq: 5260  Passive
ChanNum: 56     ChanFreq: 5280  Passive
ChanNum: 60     ChanFreq: 5300  Passive
ChanNum: 64     ChanFreq: 5320  Passive
ChanNum: 100    ChanFreq: 5500  Passive
ChanNum: 104    ChanFreq: 5520  Passive
ChanNum: 108    ChanFreq: 5540  Passive
ChanNum: 112    ChanFreq: 5560  Passive
ChanNum: 116    ChanFreq: 5580  Passive
ChanNum: 120    ChanFreq: 5600  Passive
ChanNum: 124    ChanFreq: 5620  Passive
ChanNum: 128    ChanFreq: 5640  Passive
ChanNum: 132    ChanFreq: 5660  Passive
ChanNum: 136    ChanFreq: 5680  Passive
ChanNum: 140    ChanFreq: 5700  Passive
ChanNum: 144    ChanFreq: 5720  Passive
ChanNum: 149    ChanFreq: 5745  Active
ChanNum: 153    ChanFreq: 5765  Active
ChanNum: 157    ChanFreq: 5785  Activ
ChanNum: 161    ChanFreq: 5805  Active
ChanNum: 165    ChanFreq: 5825  Active
Command wlan-get-chanlist

28. Set 11AX OMI Value
This command is used to set 802.11 AX OMI value.
1) Usage:
wlan-set-tx-omi <interface> <tx-omi> <tx-option> <num_data_pkts>
omi:
         Bit 0-2: Rx NSS
         Bit 3-4: Channel Width. 0: 20MHz  1: 40MHz  2: 80MHz
         Bit 5  : UL MU Disable
         Bit 6-8: Tx NSTS (applies to client mode only)
         Bit 9  : ER SU Disable
         Bit 10 : DL MU-MIMO Resound Recommendation
         Bit 11 : DL MU Data Disable
         Example : For 1x1 SoC, to set bandwidth,
           20M, tx-omi = 0x00
           40M, tx-omi = 0x08
           80M, tx-omi = 0x10
tx_option:
          0: send OMI in NULL Data
          1: send OMI in QoS Data
          0xff: send OMI in either QoS Data or NULL Data
num_data_pkts:
         Minimum value is 1
         Maximum value is 16
         num_data_pkts is applied only if OMI is sent in QoS data frame
         It specifies the number of consecutive data frames containing the OMI
Pls enter decimal value. Set omi value after connecting to AP.

2) Example
uart:~$ wlansh wlan-add s1 ssid TEST  wpa3 sae 1234567890 mfpc 1 mfpr 1
uart:~$ wlansh wlan-connect  s1
uart:~$ wlansh wlan-set-tx-omi sta 0x0820 0 1   (set bit 5 and bit 11 to 1, sent OMI in QoS NULL)

TX OMI: 0x820 set
TX OPTION: 0x0 set
TX NUM_DATA_PKTS: 0x1 set
Command wlan-set-tx-omi

The value of OMI set corresponds to the value of HT Control IE of the packets.
When you set tx_option to 1 or 255, you should find HTC IE from Qos data packets. There are many types of Qos data, you can ping to uAP and capture Qos data packets.

29. Get PMF configuration
This command is used to get PMF configuration.
STA:
1) Usage:
wlan-get-pmfcfg
2) Example
uart:~$ wlansh wlan-add s1 ssid TEST  wpa3 sae 1234567890 mfpc 1 mfpr 1
uart:~$ wlansh wlan-connect  s1
uart:~$ wlansh wlan-get-pmfcfg
Management Frame Protection Capability: Yes
Management Frame Protection: Required
Command wlan-get-pmfcfg

uAP:
1) Usage:
wlan-uap-get-pmfcfg
2) Example
uart:~$ wlansh wlan-add uap_5g ssid zephyr_ap role uap ip:192.168.19.1,192.168.19.1,255.255.255.0 channel 149 wpa3 sae 123456789 mfpc 1 mfpr 1
uart:~$ wlansh wlan-connect  uap_5g
uart:~$ wlansh wlan-uap-get-pmfcfg
Uap Management Frame Protection Capability: Yes
Uap Management Frame Protection: Required
Command wlan-uap-get-pmfcfg

30. Set/get ed mac mode
1) Usage:
If enable the Energy Detect adaptivity mode, and configure the energy detect threshold, then FW will not sending packets, until the neighbor energy is lower than threshold.
Set ed mac mode:
wlan-set-ed-mac-mode <interface> <ed_ctrl_2g> <ed_offset_2g> <ed_ctrl_5g> <ed_offset_5g>
Options:
<interface>:
  # 0       - for STA
  # 1       - for uAP
<ed_ctrl_2g>
  # 0       - disable EU adaptivity for 2.4GHz band
  # 1       - enable EU adaptivity for 2.4GHz band
<ed_offset_2g>
  # 0       - Default Energy Detect threshold
  # ed_threshold = ed_base - ed_offset_2g
  # e.g., if ed_base default is -62dBm, ed_offset_2g is 0x8, then ed_threshold is -70dBm
ed_ctrl_5g
  # 0       - disable EU adaptivity for 5GHz band
  # 1       - enable EU adaptivity for 5GHz band
ed_offset_5g
  # 0       - Default Energy Detect threshold
  # ed_threshold = ed_base - ed_offset_5g
  # e.g., if ed_base default is -62dBm, ed_offset_5g is 0x8, then ed_threshold is -70dBm
Get ed mac mode:
wlan-get-ed-mac-mode <interface>
Options:
<interface>:
  # 0       - for STA
  # 1       - for uAP

2) Example:
For STA, enable EU adaptivity for both 2.4GHz band and 5GHz band, and set the ed_threshold to -70 dBm.
uart:~$ wlansh wlan-set-ed-mac-mode 0 1 0x8 1 0x8
ED MAC MODE settings configuration successful
Command wlan-set-ed-mac-mode
For uAP, disable EU adaptivity for both 2.4GHz band and 5GHz band.
uart:~$ wlansh wlan-set-ed-mac-mode 1 0 0 0 0
ED MAC MODE settings configuration successful
Command wlan-set-ed-mac-mode
Get the EU adaptivity of STA,
uart:~$ wlansh wlan-get-ed-mac-mode 0
EU adaptivity for 2.4GHz band : Enabled
Energy Detect threshold offset : 0X8
EU adaptivity for 5GHz band : Enabled
Energy Detect threshold offset : 0X8
Command wlan-get-ed-mac-mode

31. Fix rate Test
(1) Fix Rate Config Commands Brief
This command is used to set/get the transmit data rate.
Below commands are used to fix rate:
1) #wlan-set-txratecfg <sta/uap> <format> <index> <nss> <rate_setting>
This command is used to set tx rate.
<sta/uap> - This parameter specifies the bss type.
<format> - This parameter specifies the data rate format used in this command.
  0: 	LG
  1: 	HT
  2: 	VHT
  3: 	HE
  0xff: 	Auto
<index> - This parameter specifies the rate or MCS index.
Different format has different rate scope, you can find it from the help info of the wlan-set-txratecfg command.
<nss> - This parameter specifies the NSS. It is valid for VHT or HE.
       Note: this parameter must be set to 1.
<rate_setting> - This parameter can only specifies the GI types now.
Bit 5-6:
For HT:
  0 = normal
  1 = Short GI
For VHT:
  01 = Short GI
  11 = Short GI and Nsym mod 10=9
  00 = otherwise
For HE, currently only set for GI types:
  0 = 1xHELTF + GI0.8us
  1 = 2xHELTF + GI0.8us
  2 = 2xHELTF + GI1.6us
  3 = 4xHELTF + GI0.8us if DCM = 1 and STBC = 1
      4xHELTF + GI3.2us, otherwise
Note:
Parameter <rate_setting> is optional. If <rate_setting> is not given, it will be set as 0xffff.
If bss type is STA, the data rate can be set only after association. If bss type is uAP, the data rate can be set only after starting uAP.
If you want to test the case where uAP and STA exist at the same time, you must start the uAP firstly, and then connect STA to other AP. But the channel of uAP must be the same as the channel of STA.

2) #wlan-get-txratecfg <sta/uap>
This command is used to get current tx rate configuration.

3) #wlan-get-data-rate <sta/uap>
This command is used to get current transmit data rate, it includes TX rate and RX rate.

(2) Examples
For example:
#wlansh wlan-set-txratecfg sta 3 9 1 0x0020	: set STA 11AX fixed TX rate to HE, MCS9, and LTF+GI size 1
In the raditap header, HE information component is included.
And MCS index is 9, LTF symbol size is 2x, GI type is 0.8us.

32. STA DTIM manual setting
(1) DTIM verification
AP parameters：
Beacon Interval: 100ms
DTIM period: 1
NOTE: You should pay attention to whether the AP's beacon cycle is stable.
1) Start AP
Recommend the use of third-party AP.(DTIM = 1)
2) Set DTIM
If you want to change the DTIM, you can use the command below:
# wlansh wlan-set-multiple-dtim <value>
This command is to set multiple_dtim to modify Next Wakeup RX Beacon Time and will take effect after enter power save mode by command wlan-ieee-ps 1
Next Wakeup RX Beacon Time = DTIM * BeaconPeriod * multiple_dtim
Note: range of multiple_dtim is [1,20]
3) Add STA and connection
# wlansh wlan-add test ssid net-2g ip:192.168.0.44,192.168.0.1,255.255.255.0 channel 1
# wlansh wlan-connect test
4) Enter IEEE-PS mode
#wlansh wlan-ieee-ps 1
5) AP ping STA & Capture packets
Open Wireshark and Capture, channel 1.
AP ping STA:
# net ping 192.168.0.44
6) Capture Results (DTIM period = 4)
STA sleep time = (AP DTIM period * STA DTIM period) * Beacon Interval
For example:
STA sleep time = (1 * 4) * 100 = 400ms
The STA wakes up every 400ms. At this time, there are two situations:
If there is data buffered in the AP, the STA will wake up, send a Qos Null packet, and then receive the data.
If there is  no data buffered in the AP, the STA will sleep fast.

Number  Time         STA       status Beacon      Remark
                               (Associtation ID)
1       24.127140s   wake up   0x01               STA will send a Qos Null packet(PWR MGT = 0), and receive the data.
2       24.536535s   wake up   None               STA will sleep fast.
3       24.946406s   wake up   0x01               STA will send a Qos Null packet(PWR MGT = 0), and receive the data.
4       25.355906S   wake up   None               STA will sleep fast.

When the STA sends out the Qos Null(PWR MGT = 0) packet, it will receive the AP's ping packet. After reply, it will send a Qos Null (PWR MGT = 1) packet, and the STA will go to sleep.
NOTE: In the current DTIM cycle, the sleep time of the STA is equal to 400ms minus the time consumed by the above operations.

33. wlan-eu-crypto-rc4
This command is used to verify Algorithm RC4 encryption and decryption.
1) Usage
Algorithm RC4 encryption and decryption verification
wlan-eu-crypto-rc4 <EncDec>
EncDec: 0-Decrypt, 1-Encrypt
2) Example
uart:~$ wlansh wlan-eu-crypto-rc4 1
uart:~$ wlansh wlan-eu-crypto-rc4 0

34. wlan-eu-crypto-aes-wrap
This command is used to verify Algorithm AES-WRAP encryption and decryption.
1) Usage
Algorithm AES-WRAP encryption and decryption verification
wlan-eu-crypto-aes-wrap <EncDec>
EncDec: 0-Decrypt, 1-Encrypt
2) Example
uart:~$ wlansh wlan-eu-crypto-aes-wrap 1
uart:~$ wlansh wlan-eu-crypto-aes-wrap 0

35. wlan-eu-crypto-aes-ecb
This command is used to verify Algorithm AES-ECB encryption and decryption.
1) Usage
Algorithm AES-ECB encryption and decryption verification
wlan-eu-crypto-aes-ecb <EncDec>
EncDec: 0-Decrypt, 1-Encrypt
2) Example
uart:~$ wlansh wlan-eu-crypto-aes-ecb 1
uart:~$ wlansh wlan-eu-crypto-aes-ecb 0

36. wlan-eu-crypto-ccmp-128
This command is used to verify Algorithm AES-CCMP-128 encryption and decryption.
1) Usage
Algorithm AES-CCMP-128 encryption and decryption verification
wlan-eu-crypto-ccmp-128 <EncDec>
EncDec: 0-Decrypt, 1-Encrypt
2) Example
uart:~$ wlansh wlan-eu-crypto-ccmp-128 1
uart:~$ wlansh wlan-eu-crypto-ccmp-128 0

37. wlan-eu-crypto-ccmp-256
This command is used to verify Algorithm AES-CCMP-256 encryption and decryption.
1) Usage
Algorithm AES-CCMP-256 encryption and decryption verification
wlan-eu-crypto-ccmp-256 <EncDec>
EncDec: 0-Decrypt, 1-Encrypt
2) Example
uart:~$ wlansh wlan-eu-crypto-ccmp-256 1
uart:~$ wlansh wlan-eu-crypto-ccmp-256 0

38. wlan-eu-crypto-gcmp-128
This command is used to verify Algorithm AES-GCMP-128 encryption and decryption.
1) Usage
Algorithm AES-GCMP-128 encryption and decryption verification
wlan-eu-crypto-gcmp-128 <EncDec>
EncDec: 0-Decrypt, 1-Encrypt
2) Example
uart:~$ wlansh wlan-eu-crypto-gcmp-128 1
uart:~$ wlansh wlan-eu-crypto-gcmp-128 0

39. wlan-eu-crypto-gcmp-256
This command is used to verify Algorithm AES-GCMP-256 encryption and decryption.
1) Usage
Algorithm AES-GCMP-256 encryption and decryption verification
wlan-eu-crypto-gcmp-256 <EncDec>
EncDec: 0-Decrypt, 1-Encrypt
2) Example
uart:~$ wlansh wlan-eu-crypto-gcmp-256 1
uart:~$ wlansh wlan-eu-crypto-gcmp-256 0

40. wifi packets statistics
This command is used to get wifi packets statistics.
1) Usage
wlan-get-log <sta/uap> <ext>
2) Example
get uap packets statistics
uart:~$ wlansh wlan-get-log uap
get sta packet statistics
uart:~$ wlansh wlan-get-log sta
get extended package statistic
uart:~$ wlansh wlan-get-log uap ext
uart:~$ wlansh wlan-get-log sta ext

41. TX Pert
This command is used to track Tx packet error ratio.
1) Usage
    wlan-tx-pert <0/1> <STA/AP> <p:tx_pert_check_period> <r:tx_pert_check_ratio> <n:tx_pert_check_num>
Options:
    <0/1>: Disable/enable Tx Pert tracking.
    <STA/UAP>: User needs to indicate which interface this tracking for.
    <p>: Tx Pert check period. Unit is second.
    <r>: Tx Pert ratio threshold (unit 10%). (Fail TX packet)/(Total TX packets). The default value is 5.
    <n>: A watermark of check number (default 5). Fw will start tracking Tx Pert after sending n packets.
Example:
    wlan-tx-pert 1 UAP 5 3 5
Note:
    Please verify by iperf or ping
    When the traffic quality is good enough, it will not be triggered
2) Example
uart:~$ wlansh wlan-tx-pert 1 STA 5 1 1
3) Result
If the Tx packet error ratio reaches or exceeds the threshold that user configured, firmware will upload event to report current Tx error ratio. The host will print out the report as well as user configuration like below:
current PER is 60%
User configure:
       tx_pert_check_period : 5 sec
       tx_pert_check_ratio  : 1%
       tx_pert_check_num    : 1

42. Roaming Test
1)	Testing Environment Setup
Two APs are needed. AP1 and AP2 should have same SSID and different mac address and same security settings. If AP1 and AP2 are both secured, they should have same security mode and same passphrase.
2)	Enable Roaming
Issue below command to enable roaming:
wlan-roaming <0/1> <rssi_threshold>
Example:
    wlan-roaming 1 40
3)	Connect to AP1
Remove the antenna of AP2 to make sure our device will always connect to AP1 for the first time.
Connect to AP1 with below commands:
uart:~$ wlansh wlan-add ROAMING ssid <SSID>
uart:~$ wlansh wlan-connect ROAMING
Use below command to check AP1 info after connection is done:
uart:~$ wlansh wlan-info
4)	Decrease Signal of AP1
Attach antenna of AP2 and decrease signal of AP1. If the rssi of AP1 is below the rssi_threshold, fw will send RSSI_LOW event to driver and roaming process will be triggered.
5)	Check Roaming result
If roaming is successful, you will find connection success log like below:
========================================
app_cb: WLAN: received event 0
========================================
app_cb: WLAN: connected to network
Connected to following BSS:
SSID = [roaming_test], IP = [192.168.3.137]
Use below command to check detailed info of current connection:
uart:~$ wlansh wlan-info
If roam to AP2 successfully, the BSS info should be updated to info of AP2. If roaming is failed, our device will keep the connection to AP1 and BSS info will not be changed.

43. wlan-set-uap-hidden-ssid
This command is used to set uap hidden ssid.
1) Usage
wlan-set-uap-hidden-ssid <0/1/2>
Usage: wlan-set-uap-hidden-ssid <0/1/2>
Error: 0: broadcast SSID in beacons.
1: send empty SSID (length=0) in beacons.
2: clear SSID (ACSII 0), but keep the original length
2) Example
1. broadcast SSID in beacons
uart:~$ wlansh wlan-set-uap-hidden-ssid 0
2. send empty SSID (length=0) in beacons
uart:~$ wlansh wlan-set-uap-hidden-ssid 1
3. clear SSID (ACSII 0), but keep the original length
uart:~$ wlansh wlan-set-uap-hidden-ssid 2
Check SSID in beacon packets.

44. wlan-set-scan-interval
This command is used to set scan interval.
1) Usage
wlan-set-scan-interval <scan_int: in seconds>
2) Example
Connect to an external AP, and then turn off the AP, check the scan interval on STA
uart:~$ wlansh wlan-set-scan-interval 5
uart:~$ wlansh wlan-set-scan-interval 120

45. wlan-test-mode
(1) wlan-set-rf-test-mode
1) Example
uart:~$ wlansh wlan-set-rf-test-mode

(2) wlan-set-rf-tx-antenna
1) Usage
wlan-set-rf-tx-antenna <antenna>
antenna: 1=Main, 2=Aux
2) Example
uart:~$ wlansh wlan-set-rf-test-mode
uart:~$ wlansh wlan-set-rf-tx-antenna 1

(3) wlan-get-rf-tx-antenna
1) Example
uart:~$ wlansh wlan-set-rf-test-mode
uart:~$ wlansh wlan-get-rf-tx-antenna

(4) wlan-set-rf-rx-antenna
1) Usage
wlan-set-rf-rx-antenna <antenna>
antenna: 1=Main, 2=Aux
2) Example
uart:~$ wlansh wlan-set-rf-test-mode
uart:~$ wlansh wlan-set-rf-rx-antenna 1

(5) wlan-get-rf-rx-antenna
1) Example
uart:~$ wlansh wlan-set-rf-test-mode
uart:~$ wlansh wlan-get-rf-rx-antenna

(6) wlan-set-rf-band
1) Usage
wlan-set-rf-band <band>
band: 0=2.4G, 1=5G
2) Example
uart:~$ wlansh wlan-set-rf-test-mode
uart:~$ wlansh wlan-set-rf-band 1

(7) wlan-get-rf-band
1) Example
uart:~$ wlansh wlan-set-rf-test-mode

(8) wlan-set-rf-bandwidth
1) Usage
wlan-set-bandwidth <bandwidth>
        <bandwidth>:
                0: 20MHz
                1: 40MHz
                4: 80MHz
2) Example
uart:~$ wlansh wlan-set-rf-test-mode
uart:~$ wlansh wlan-set-rf-bandwidth 0

(9) wlan-get-rf-bandwidth
1) Example
uart:~$ wlansh wlan-set-rf-test-mode
uart:~$ wlansh wlan-get-rf-bandwidth

(10) wlansh wlan-set-rf-channel
1) Usage
wlan-set-rf-channel <channel>
2) Example
uart:~$ wlansh wlan-set-rf-test-mode
uart:~$ wlansh wlan-set-rf-channel 36

(11) wlan-get-rf-channel
1) Example
uart:~$ wlansh wlan-set-rf-test-mode
uart:~$ wlansh wlan-get-rf-channel

(12) wlan-set-rf-radio-mode
1) Usage
wlan-set-rf-radio-mode <radio_mode>
0: set the radio in power down mode
3: sets the radio in 5GHz band, 1X1 mode(path A)
11: sets the radio in 2.4GHz band, 1X1 mode(path A)
2) Example
uart:~$ wlansh wlan-set-rf-test-mode
uart:~$ wlansh wlan-set-rf-radio-mode 3

(13) wlan-get-rf-radio-mode
1) Example
uart:~$ wlansh wlan-set-rf-test-mode
uart:~$ wlansh wlan-get-rf-radio-mode

(14) wlansh wlan-set-rf-tx-power
1) Usage
wlan-set-rf-tx-power <tx_power> <modulation> <path_id>
Power       (0 to 20 dBm)
Modulation  (0: CCK, 1:OFDM, 2:MCS)
Path ID     (0: PathA, 1:PathB, 2:PathA+B)
2) Example
uart:~$ wlansh wlan-set-rf-test-mode
uart:~$ wlansh wlan-set-rf-tx-power 0 0 0

(15) wlan-set-rf-tx-cont-mode
1) Usage
wlan-set-rf-tx-cont-mode <enable_tx> <cw_mode> <payload_pattern> <cs_mode> <act_sub_ch> <tx_rate>
Enable                (0:disable, 1:enable)
Continuous Wave Mode  (0:disable, 1:enable)
Payload Pattern       (0 to 0xFFFFFFFF) (Enter hexadecimal value)
CS Mode               (Applicable only when continuous wave is disabled) (0:disable, 1:enable)
Active SubChannel     (0:low, 1:upper, 3:both)
Tx Data Rate          (Rate Index corresponding to legacy/HT/VHT rates)

To Disable:
  In Continuous Wave Mode:
    Step1: wlan-set-rf-tx-cont-mode 0 1 0 0 0 0
    Step2: wlan-set-rf-tx-cont-mode 0
  In none continuous Wave Mode:
    Step1: wlan-set-rf-tx-cont-mode 0

2) Example
uart:~$ wlansh wlan-set-rf-test-mode
uart:~$ wlansh wlan-set-rf-tx-cont-mode 1 1 0 0 0 0

(16) wlan-set-rf-tx-frame
1) Usage
wlan-set-rf-tx-frame <start> <data_rate> <frame_pattern> <frame_len> <adjust_burst_sifs> <burst_sifs_in_us> <short_preamble> <act_sub_ch> <short_gi> <adv_coding> <tx_bf> <gf_mode> <stbc> <bssid>
Enable                 (0:disable, 1:enable)
Tx Data Rate           (Rate Index corresponding to legacy/HT/VHT rates)
Payload Pattern        (0 to 0xFFFFFFFF) (Enter hexadecimal value)
Payload Length         (1 to 0x400) (Enter hexadecimal value)
Adjust Burst SIFS3 Gap (0:disable, 1:enable)
Burst SIFS in us       (0 to 255us)
Short Preamble         (0:disable, 1:enable)
Active SubChannel      (0:low, 1:upper, 3:both)
Short GI               (0:disable, 1:enable)
Adv Coding             (0:disable, 1:enable)
Beamforming            (0:disable, 1:enable)
GreenField Mode        (0:disable, 1:enable)
STBC                   (0:disable, 1:enable)
BSSID                  (xx:xx:xx:xx:xx:xx)

To Disable:
wlan-set-rf-tx-frame 0

2) Example
uart:~$ wlansh wlan-set-rf-test-mode
uart:~$ wlansh wlan-set-rf-tx-frame 1 7 2730 256 0 0 0 0 0 0 0 0 0 ad:ad:23:12:45:57

(17) wlan-set-rf-trigger-frame-cfg
1) Usage
wlan-set-rf-trigger-frame-cfg <Enable_tx> <Standalone_hetb> <FRAME_CTRL_TYPE> <FRAME_CTRL_SUBTYPE> <FRAME_DURATION><TriggerType> <UlLen> <MoreTF> <CSRequired> <UlBw> <LTFType> <LTFMode><LTFSymbol> <UlSTBC> <LdpcESS> <ApTxPwr> <PreFecPadFct> <PeDisambig> <SpatialReuse><Doppler> <HeSig2> <AID12> <RUAllocReg> <RUAlloc> <UlCodingType> <UlMCS> <UlDCM><SSAlloc> <UlTargetRSSI> <MPDU_MU_SF> <TID_AL> <AC_PL> <Pref_AC>
Enable_tx                   (Enable/Disable trigger frame transmission)
Standalone_hetb             (Enable/Disable Standalone HE TB support.)
FRAME_CTRL_TYPE             (Frame control type)
FRAME_CTRL_SUBTYPE          (Frame control subtype)
FRAME_DURATION              (Max Duration time)
TriggerType                 (Identifies the Trigger frame variant and its encoding)
UlLen                       (Indicates the value of the L-SIG LENGTH field of the solicited HE TB PPDU)
MoreTF                      (Indicates whether a subsequent Trigger frame is scheduled for transmission)
CSRequired                  (Required to use ED to sense the medium and to consider the medium state and the NAV in determining whether to respond)
UlBw                        (Indicates the bandwidth in the HE-SIG-A field of the HE TB PPDU)
LTFType                     (Indicates the LTF type of the HE TB PPDU response)
LTFMode                     (Indicates the LTF mode for an HE TB PPDU)
LTFSymbol                   (Indicates the number of LTF symbols present in the HE TB PPDU)
UlSTBC                      (Indicates the status of STBC encoding for the solicited HE TB PPDUs)
LdpcESS                     (Indicates the status of the LDPC extra symbol segment)
ApTxPwr                     (Indicates the AP抯 combined transmit power at the transmit antenna connector of all the antennas used to transmit the triggering PPDU)
PreFecPadFct                (Indicates the pre-FEC padding factor)
PeDisambig                  (Indicates PE disambiguity)
SpatialReuse                (Carries the values to be included in the Spatial Reuse fields in the HE-SIG-A field of the solicited HE TB PPDUs)
Doppler                     (Indicate that a midamble is present in the HE TB PPDU)
HeSig2                      (Carries the value to be included in the Reserved field in the HE-SIG-A2 subfield of the solicited HE TB PPDUs)
AID12                       (If set to 0 allocates one or more contiguous RA-RUs for associated STAs)
RUAllocReg                  (RUAllocReg)
RUAlloc                     (Identifies the size and the location of the RU)
UlCodingType                (Indicates the code type of the solicited HE TB PPDU)
UlMCS                       (Indicates the HE-MCS of the solicited HE TB PPDU)
UlDCM                       (Indicates DCM of the solicited HE TB PPDU)
SSAlloc                     (Indicates the spatial streams of the solicited HE TB PPDU)
UlTargetRSSI                (Indicates the expected receive signal power)
MPDU_MU_SF                  (Used for calculating the value by which the minimum MPDU start spacing is multiplied)
TID_AL                      (Indicates the MPDUs allowed in an A-MPDU carried in the HE TB PPDU and the maximum number of TIDs that can be aggregated by the STA in the A-MPDU)
AC_PL                       (Reserved)
Pref_AC                     (Indicates the lowest AC that is recommended for aggregation of MPDUs in the A-MPDU contained in the HE TB PPDU sent as a response to the Trigger frame)

2) Example
uart:~$ wlansh wlan-set-rf-test-mode
uart:~$ wlansh wlan-set-rf-radio-mode 3
uart:~$ wlansh wlan-set-rf-band 1
uart:~$ wlansh wlan-set-rf-bandwidth 0
uart:~$ wlansh wlan-set-rf-channel 36
uart:~$ wlansh wlan-set-rf-trigger-frame-cfg 1 0 1 2 5484 0 256 0 0 0 1 0 0 0 1 60 1 0 65535 0 511 5 0 61 0 0 0 0 90 0 0 0 0

(18) wlan-set-rf-he-tb-tx
1) Usage
wlan-set-rf-he-tb-tx <enable> <qnum> <uint16_t aid> <axq_mu_timer> <tx_power>
Enable           (Enable/Disable trigger response mode)
qnum             (AXQ to be used for the trigger response frame)
aid              (AID of the peer to which response is to be generated)
axq_mu_timer     (MU timer for the AXQ on which response is sent)
tx_power         (TxPwr to be configured for the response)
Command wlan-set-rf-he-tb-tx
2) Example
uart:~$ wlansh wlan-set-rf-test-mode
uart:~$ wlansh wlan-set-rf-radio-mode 3
uart:~$ wlansh wlan-set-rf-band 1
uart:~$ wlansh wlan-set-rf-bandwidth 0
uart:~$ wlansh wlan-set-rf-channel 36
uart:~$ wlansh wlan-set-rf-he-tb-tx 1 1 5 400 -7

(19) wlan-get-and-reset-rf-per
1) Example
uart:~$ wlansh wlan-set-rf-test-mode
uart:~$ wlansh wlan-get-and-reset-rf-per

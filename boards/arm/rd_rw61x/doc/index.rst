.. _rd_rw612_bga_evk:

NXP RD-RW612-BGA
################

Overview
********

The RW612 is a highly integrated, low-power tri-radio wireless MCU with an
integrated 260 MHz ARM Cortex-M33 MCU and Wi-Fi 6 + Bluetooth Low Energy (LE) 5.3 / 802.15.4
radios designed for a broad array of applications, including connected smart home devices,
gaming controllers, enterprise and industrial automation, smart accessories and smart energy.

The RW612 MCU subsystem includes 1.2 MB of on-chip SRAM and a high-bandwidth Quad SPI interface
with an on-the-fly decryption engine for securely accessing off-chip XIP flash.

The advanced design of the RW612 delivers tight integration, low power and highly secure
operation in a space- and cost-efficient wireless MCU requiring only a single 3.3 V power supply.

Hardware
********

- 260 MHz ARM Cortex-M33, tri-radio cores for Wifi 6 + BLE 5.3 + 802.15.4
- 1.2 MB on-chip SRAM

Supported Features
==================

+-----------+------------+-----------------------------------+
| Interface | Controller | Driver/Component                  |
+===========+============+===================================+
| NVIC      | on-chip    | nested vector interrupt controller|
+-----------+------------+-----------------------------------+
| SYSTICK   | on-chip    | systick                           |
+-----------+------------+-----------------------------------+
| OS_TIMER  | on-chip    | os timer                          |
+-----------+------------+-----------------------------------+
| MCI_IOMUX | on-chip    | pinmux                            |
+-----------+------------+-----------------------------------+
| GPIO      | on-chip    | gpio                              |
+-----------+------------+-----------------------------------+
| USART     | on-chip    | serial                            |
+-----------+------------+-----------------------------------+
| I2C       | on-chip    | i2c                               |
+-----------+------------+-----------------------------------+
| SPI       | on-chip    | spi                               |
+-----------+------------+-----------------------------------+
| I2S       | on-chip    | i2s                               |
+-----------+------------+-----------------------------------+
| PWM       | on-chip    | pwm                               |
+-----------+------------+-----------------------------------+
| WDT       | on-chip    | watchdog                          |
+-----------+------------+-----------------------------------+
| USB       | on-chip    | usb                               |
+-----------+------------+-----------------------------------+
| ADC       | on-chip    | adc                               |
+-----------+------------+-----------------------------------+
| TRNG      | on-chip    | entropy                           |
+-----------+------------+-----------------------------------+
| ACOMP     | on-chip    | sensor                            |
+-----------+------------+-----------------------------------+
| DAC       | on-chip    | dac                               |
+-----------+------------+-----------------------------------+
| ENET      | on-chip    | ethernet                          |
+-----------+------------+-----------------------------------+
| CTIMER    | on-chip    | counter                           |
+-----------+------------+-----------------------------------+
| LCDIC     | on-chip    | mipi-dbi                          |
+-----------+------------+-----------------------------------+

The default configuration can be found in the defconfig file:

   ``boards/arm/rd_rw61x/rd_rw612_bga_defconfig/``

Other hardware features are not currently supported

Programming and Debugging
*************************

Build and flash applications as usual (see :ref:`build_an_application` and
:ref:`application_run` for more details).

Configuring a Debug Probe
=========================

A debug probe is used for both flashing and debugging the board. This board is
configured by default to use the LPC-Link2 and JLink Firmware.

Configuring a Console
=====================

Connect a USB cable from your PC to J7, and use the serial terminal of your choice
(minicom, putty, etc.) with the following settings:

- Speed: 115200
- Data: 8 bits
- Parity: None
- Stop bits: 1

Flashing
========

Here is an example for the :ref:`hello_world` application. This example uses the
:ref:`jlink-debug-host-tools` as default.

.. zephyr-app-commands::
   :zephyr-app: samples/hello_world
   :board: rd_rw612_bga
   :goals: flash

Open a serial terminal, reset the board (press the RESET button), and you should
see the following message in the terminal:

.. code-block:: console

   ***** Booting Zephyr OS v3.4.0 *****
   Hello World! rd_rw612_bga

Debugging
=========

Here is an example for the :ref:`hello_world` application. This example uses the
:ref:`jlink-debug-host-tools` as default.

.. zephyr-app-commands::
   :zephyr-app: samples/hello_world
   :board: rd_rw612_bga
   :goals: debug

Open a serial terminal, step through the application in your debugger, and you
should see the following message in the terminal:

.. code-block:: console

   ***** Booting Zephyr OS zephyr-v3.4.0 *****
   Hello World! rd_rw612_bga

Attached Display
================

The RW61x board is configured to drive an ST7796S based display controller,
with a FT7401 touch IC. The display can be driven via the LCDIC, or Flexcomm
SPI.

When driving the display via the LCDIC, the following board modifications
must be made:

* Populate R125, R123, R12, R124, R15, R243, R239, R236, R233, R286, R246

* Remove R9, R11, R20, R19, R242, R241, R237, R235, R245

When driving the display via the LCDIC, use the following connections:

+--------+--------+----------------------------------+
| Pin #  | Signal | Header                           |
+========+========+==================================+
| 1      | VDD    | J5.8 (+3.3V)                     |
+--------+--------+----------------------------------+
| 2      | RST    | J6.8 (LCD_SPI_RESETN)            |
+--------+--------+----------------------------------+
| 3      | SDO    | J5.5 (SPI_MISO)                  |
+--------+--------+----------------------------------+
| 4      | CS     | J5.3 (LCD_SPI_SS)                |
+--------+--------+----------------------------------+
| 5      | SCLK   | J5.6 (LCD_SPI_SCK)               |
+--------+--------+----------------------------------+
| 6      | GND    | J5.7 (GND)                       |
+--------+--------+----------------------------------+
| 7      | MOSI   | J5.4 (LCD_SPI_SDIO)              |
+--------+--------+----------------------------------+
| 8      | CD     | J5.1 (LCD_SPI_DC)                |
+--------+--------+----------------------------------+
| 9      | TE     | J5.2 (LCD_SPI_TE, not enabled)   |
+--------+--------+----------------------------------+

When driving the display via the Flexcomm SPI, set the following jumpers:

* JP19

* JP49 (connect pins 1-2)

use the following connections:

+-------+--------+---------------+
| Pin # | Signal | Header        |
+=======+========+===============+
| 1     | VDD    | J13.7 (+3.3V) |
+-------+--------+---------------+
| 2     | RST    | J11.2 (INT)   |
+-------+--------+---------------+
| 3     | SDO    | J13.5 (MISO)  |
+-------+--------+---------------+
| 4     | CS     | J13.3 (CS)    |
+-------+--------+---------------+
| 5     | SCLK   | J13.4 (SCK)   |
+-------+--------+---------------+
| 6     | GND    | J13.8 (GND)   |
+-------+--------+---------------+
| 7     | MOSI   | J13.6 (MOSI)  |
+-------+--------+---------------+
| 8     | CD     | J11.1 (PWM)   |
+-------+--------+---------------+

The touch controller requires the following connections:

* Populate JP3 and JP50

+--------+--------+---------------+
| Pin #  | Signal | Header        |
+========+========+===============+
| 1      | VDD    | J8.2 (+3.3V)  |
+--------+--------+---------------+
| 2      | IOVDD  | J8.4 (+3.3V)  |
+--------+--------+---------------+
| 3      | SCL    | J5.10 (SCL)   |
+--------+--------+---------------+
| 4      | SDA    | J5.9 (SDA)    |
+--------+--------+---------------+
| 5      | INT    | Not Connected |
+--------+--------+---------------+
| 6      | RST    | J6.4 (D3)     |
+--------+--------+---------------+
| 7      | GND    | J8.6 (GND)    |
+--------+--------+---------------+
| 8      | GND    | J8.7 (GND)    |
+--------+--------+---------------+


How to use Ethernet
===================

By default, the standard configuration of the board does not support the use of ethernet.
In order to use ethernet on this board, the following modification needs to be done:

- Load R485, R486, R487, R488, R489, R491, R490, R522, R521, R520, R524, R523, R508, R505
- Remove R518, R507, R506

This rework will affect peripherals (such as RTC) that use the XTAL32K clock because the
ethernet phy shares pins with the XTAL32K clock source.

To build, use the board target `rd_rw612_bga_ethernet`.

Resources
=========

.. _RW612 Website:
   https://www.nxp.com/products/wireless-connectivity/wi-fi-plus-bluetooth-plus-802-15-4/wireless-mcu-with-integrated-tri-radiobr1x1-wi-fi-6-plus-bluetooth-low-energy-5-3-802-15-4:RW612

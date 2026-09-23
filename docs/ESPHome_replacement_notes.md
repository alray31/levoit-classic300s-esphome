<!--
Mirrored from https://github.com/Taxom/levoit-classic-300s-uart-protocol
by Maxim Pivovarov / MaxPi ("Taxom"), licensed CC BY-NC-SA 4.0.
Mirrored here for convenience/offline reference; this file is not
original work of this repo. See ../README.md for full attribution.
-->

# ESPHome replacement-controller notes

These notes summarize the replacement-controller tests performed for the Levoit Classic 300S UART protocol project.

This file is not required to understand the protocol, but it records practical implementation details that were useful for replacing the original WiFi/ESP module.

> This repository documents the protocol and hardware notes. It does not currently include ready-to-flash ESPHome YAML files or firmware binaries.

## Tested hardware variants

### External diagnostic controller

```text
Board: ESP32-C3 Super Mini
UART to humidifier MCU:
  ESP32-C3 RX GPIO20 <- humidifier MCU TX
  ESP32-C3 TX GPIO21 -> humidifier MCU RX
UART: 9600 8N1
```

Electrical warning for external controllers:

```text
The MCU-board header is 5 V.
The UART levels observed at the MCU-board header are 5 V logic.
The original module board contains the level conversion/protection.
External 3.3 V controllers must use level shifting or other input protection.
```

During diagnostics, the external ESP32-C3 was connected through a transistor level shifter. Directly connecting the humidifier MCU TX line from the MCU-board header to a 3.3 V ESP RX pin is not recommended.

### Original ESP32-SOLO module replacement

The original WiFi module in the first tested unit uses an ESP32 single-core module.

```text
Chip detected by esptool: ESP32-S0WD revision v1.0
Original module class: ESP32-SOLO-1C / single-core ESP32
Flash size: 4 MB
MAC: device-specific, omitted
```

Important: the original ESP32-SOLO module board is not just an ESP32 module. In the tested unit it also provides the interface electronics between the 5 V MCU-board header and the 3.3 V ESP32-SOLO GPIOs.

If replacing this board with a different ESP board, replicate the required level shifting/protection.

Working A5 UART pins on the original ESP32-SOLO module:

```text
ESP32 GPIO16 = RX from humidifier MCU TX
ESP32 GPIO17 = TX to humidifier MCU RX
UART: 9600 8N1
```

Flashing/backup UART:

```text
ESP32 GPIO3 = U0RXD <- USB-UART TX
ESP32 GPIO1 = U0TXD -> USB-UART RX
GPIO0       = BOOT strap low for UART bootloader mode
EN/RST      = reset
GND         = common ground
3.3V only
```

Example ESPHome `esp32:` block used for the ESP32-SOLO variant:

```yaml
esp32:
  board: esp32dev
  variant: esp32
  flash_size: 4MB
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_FREERTOS_UNICORE: y
```

Example A5 UART block:

```yaml
uart:
  id: a5_uart_bus
  rx_pin: GPIO16
  tx_pin: GPIO17
  baud_rate: 9600
```

### Newer ESP32-C3-SOLO-1 module replacement

A newer Levoit Classic 300S unit was observed with an ESP32-C3-SOLO-1 module instead of ESP32-SOLO-1C.

Observed `esptool flash-id` information:

```text
Chip type: ESP32-C3 (QFN32) revision v0.4
Features: Wi-Fi, BT 5 (LE), Single Core, 160 MHz, Embedded Flash 4 MB (XMC)
Crystal: 40 MHz
Flash size: 4 MB
MAC: device-specific, omitted
```

The carrier board layout appeared equivalent to the ESP32-SOLO version, but the module pin mapping differs because the module itself is ESP32-C3.

Working A5 UART pins on the ESP32-C3-SOLO-1 module:

```text
ESP32-C3 GPIO18 = RX from humidifier MCU TX
ESP32-C3 GPIO19 = TX to humidifier MCU RX
UART: 9600 8N1
```

Observed mapping basis:

```text
Old ESP32-SOLO board connection:
  module pin 27 -> GPIO16
  module pin 28 -> GPIO17

New ESP32-C3-SOLO-1 board connection:
  module pin 27 -> GPIO18
  module pin 28 -> GPIO19
```

Flashing/backup UART for ESP32-C3:

```text
ESP32-C3 GPIO20 = U0RXD <- USB-UART TX
ESP32-C3 GPIO21 = U0TXD -> USB-UART RX
ESP32-C3 GPIO9  = BOOT strap low for UART bootloader mode
EN/RST          = reset
GND             = common ground
3.3V only
```

Example ESPHome `esp32:` block used for the ESP32-C3-SOLO-1 variant:

```yaml
esp32:
  board: esp32-c3-devkitm-1
  variant: esp32c3
  flash_size: 4MB
  framework:
    type: esp-idf
```

Example A5 UART block:

```yaml
uart:
  id: a5_uart_bus
  rx_pin: GPIO18
  tx_pin: GPIO19
  baud_rate: 9600
```

ESPHome may warn that GPIO18/GPIO19 are used by the ESP32-C3 USB-Serial-JTAG interface. In this module installation they are used as GPIO/UART pins to the humidifier MCU, while flashing/logging is done through UART0 on GPIO20/GPIO21. This warning is expected for this design.

## Original firmware backup

Before replacing the original firmware, make a private full-flash backup of the original ESP module.

Do **not** publish this backup: the flash image may contain device-specific data such as WiFi credentials, tokens, calibration/NVS data, MAC-related records, and pairing/provisioning state.

For a 4 MB ESP32-SOLO flash chip, read the full image twice and compare the files:

```powershell
python -m esptool --chip esp32 --port COMx --baud 460800 --before no-reset --after no-reset read-flash 0x000000 0x400000 backup_1.bin
python -m esptool --chip esp32 --port COMx --baud 460800 --before no-reset --after no-reset read-flash 0x000000 0x400000 backup_2.bin
cmd /c fc /b backup_1.bin backup_2.bin
Get-FileHash .\backup_1.bin -Algorithm SHA256
```

For a 4 MB ESP32-C3 flash chip, use `--chip esp32c3`:

```powershell
python -m esptool --chip esp32c3 --port COMx --baud 460800 --before no-reset --after no-reset read-flash 0x000000 0x400000 backup_1.bin
python -m esptool --chip esp32c3 --port COMx --baud 460800 --before no-reset --after no-reset read-flash 0x000000 0x400000 backup_2.bin
cmd /c fc /b backup_1.bin backup_2.bin
Get-FileHash .\backup_1.bin -Algorithm SHA256
```

A correct full backup for a 4 MB module should be:

```text
4,194,304 bytes
```

The SHA256 value is intentionally not documented here because it is device-specific and not useful to other users.

Note: repeated backups may differ if the stock firmware boots between reads and updates the NVS/WiFi area. A stable backup can be produced by keeping the chip in bootloader mode and using `--before no-reset --after no-reset` for repeated reads.

## ESPHome entity design notes

### Stop At Target vs Target Stop Active

Do not bind the `Stop At Target` switch directly to STATUS `p[10]`.

Updated interpretation:

```text
Stop At Target command:
  01 E5 A5 00 00 = behavior OFF
  01 E5 A5 00 01 = behavior ON

STATUS p[10]:
  0 = target stop is not currently active
  1 = MCU is currently inhibiting/stopping output due to target condition
```

Recommended ESPHome/HA design:

```text
Stop At Target
  command/desired behavior switch, preferably optimistic or preference-backed

Target Stop Active
  binary sensor from p[10]
  shows the actual MCU output-inhibit state
```

This avoids the misleading UI state where the command behavior is enabled, but the humidifier has not yet reached the humidity threshold.

### Error field

Expose `p[19]` as a diagnostic text sensor.

Recommended mapping:

```text
0x00 = OK
0x01 = E1, observed; likely tank Hall sensor / tank-presence fault
0x02 = E2 candidate / unconfirmed
other = UNKNOWN(0xXX)
```

### Tank and water text states

The following user-facing naming was found clearer than alarm-style naming:

```text
Tank:  installed / removed
Water: OK / empty
```

Instead of:

```text
Tank Removed: OK / problem
Water Empty: OK / problem
```

### Mode selection

A single select/dropdown is preferred over separate mode buttons:

```text
Mode: AUTO / MANUAL / SLEEP
```

This avoids clutter and prevents separate mode-button entities from creating meaningless history graphs.

## Recovery behavior implemented in the tested ESPHome replacement

The MCU reports physical power-button long-press events as short `A5-02 len=5` packets.

Replacement behavior tested:

```text
5-second hold threshold + release event:
  reboot ESP / reconnect WiFi

15-second hold threshold:
  start ESP recovery AP
```

This uses the existing humidifier front-panel power button and does not require adding a separate physical recovery button to the ESP module.

## mDNS / hostname note

The device can work normally in Home Assistant even if `.local` name resolution is broken on some clients, provided the IP address is reachable and the ESPHome native API connection works.

If `.local` does not resolve but the device works by IP, the issue is likely network/mDNS/DNS behavior rather than the UART replacement logic.

A normal DNS name such as `levoit-classic300s.home.arpa` can be used if managed by the local DNS server.

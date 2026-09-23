<!--
Mirrored from https://github.com/Taxom/levoit-classic-300s-uart-protocol
by Maxim Pivovarov / MaxPi ("Taxom"), licensed CC BY-NC-SA 4.0.
Mirrored here for convenience/offline reference; this file is not
original work of this repo. See ../README.md for full attribution.
-->

# Hardware component notes

These are hardware observations collected during the Levoit Classic 300S UART reverse-engineering work.

This file is not a complete schematic. It documents visible/candidate ICs and relevant interface behavior.

## Board-level architecture

Practical architecture observed during reverse engineering:

```text
Front panel / sensors / humidifier control
        |
        v
Main MCU board  <--- 5 V UART / power header --->  WiFi/ESP module board
        |
        v
Display/key controller and front-panel LEDs/buttons
```

The original WiFi/ESP module board is not only a radio module. It also contains interface/protection circuitry between the humidifier MCU board and the ESP module.

## Main MCU candidate: SinOne SC92F84A family

The main humidifier controller appears to be a SinOne `SC92F84A*` family MCU.

Relevant datasheet-level capabilities of this family:

```text
Core:        high-speed 1T 8051-compatible MCU
Flash:       16 KB class, depending on exact suffix
SRAM:        1 KB class
EEPROM:      128 bytes independent EEPROM
Touch keys:  dual-mode capacitive TouchKey module
ADC:         12-bit ADC
PWM:         six 10-bit PWM outputs
Timers:      three timer/counters
Interfaces:  UART and SSI, where SSI can be used as UART/SPI/TWI depending on configuration
```

This matches the kind of job the MCU performs in the humidifier:

```text
- front-panel touch/button handling
- tank/water/sensor input handling
- fan/mist/display decisions
- UART protocol endpoint for the WiFi/ESP module
```

## Display/key controller: AiP650E

The display/key controller is an `AiP650E` or compatible part.

Relevant observed/datasheet-level behavior:

```text
Function:  LED display controller/driver with keyboard scan
Interface: 2-wire serial interface
Display:   8SEG x 4DIG class LED drive
Brightness: hardware brightness modulation supported by the controller
```

Important practical note:

The display controller appears to support hardware brightness control, but the stock humidifier MCU UART protocol currently exposes only binary display control:

```text
01 05 A1 00 00 = display OFF
01 05 A1 00 64 = display ON
```

Intermediate display values sent through the `A5` UART protocol were rejected by the MCU with `A5-52` NACK responses.

Therefore the limitation appears to be in the stock MCU firmware / exposed UART API, not necessarily in the display driver hardware.

Possible deeper reverse-engineering path:

```text
MCU <-> AiP650E bus sniffing or MITM could reveal the native display commands.
```

This is separate from the WiFi-module UART replacement project and is not required for Home Assistant integration.

## WiFi/ESP module variants

Two module variants were observed/tested.

### Original ESP32-SOLO-1C / ESP32-S0WD module

```text
Chip class: ESP32-SOLO-1C / ESP32-S0WD
Core:       single-core classic ESP32 / Xtensa
Flash:      4 MB observed
A5 UART:    GPIO16/GPIO17 on the original module board
```

### Newer ESP32-C3-SOLO-1 module

```text
Chip class: ESP32-C3-SOLO-1
Core:       single-core ESP32-C3 / RISC-V
Flash:      embedded 4 MB XMC observed
A5 UART:    GPIO18/GPIO19 on the original module board
```

Observed flashing UART for ESP32-C3:

```text
GPIO20 = U0RXD
GPIO21 = U0TXD
GPIO9  = BOOT strap low for download mode
```

## 5 V header warning

The humidifier MCU-board header carries 5 V power and 5 V UART logic.

The original WiFi/ESP module board provides the required interface electronics. If replacing the original module board with a generic development board, add proper level shifting/protection.

A 3.3 V ESP RX pin should not be connected directly to the 5 V MCU-board TX signal.

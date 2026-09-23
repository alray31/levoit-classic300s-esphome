# Levoit Classic 300S -- ESPHome replacement firmware

Custom ESPHome external component that replaces the stock WiFi/ESP module
firmware on a Levoit Classic 300S humidifier, talking directly to the
appliance MCU over its proprietary `A5` UART protocol. Gives you local
control in Home Assistant with no cloud/VeSync dependency.

## Credit

The `A5` UART protocol itself was **not** reverse-engineered in this repo --
it was recovered by **Maxim Pivovarov / MaxPi ("Taxom")** through live UART
sniffing and active replacement-controller testing, and published at
[Taxom/levoit-classic-300s-uart-protocol](https://github.com/Taxom/levoit-classic-300s-uart-protocol)
under CC BY-NC-SA 4.0. That repo documents the protocol and hardware notes
but does not include a ready-to-flash ESPHome component -- this repo is the
ESPHome component built on top of that documented protocol (original code,
not copied from the source repo).

If you republish or build on this, keep crediting the original protocol
research -- it's the hard part.

## What this gives you

- Sensors: current humidity, target humidity, temperature, night light
  level, output level
- Binary sensors: power, tank removed, water empty, mist active, display,
  target-stop-active
- Switches: power, display, stop-at-target
- Numbers: manual mist level (1-9), auto target humidity, sleep target
  humidity, night light (0-100)
- Select: mode (auto / manual / sleep)
- Diagnostic text sensors: mode (raw), error code, last raw status frame
- Automation triggers for the physical power button's long-press events
  (`on_power_button_hold_5s`, `on_power_button_hold_15s`) so you can script
  your own reboot / recovery behavior

## Before you touch anything: hardware variant

Levoit shipped this model with **two different WiFi modules** over time:

| | ESP32-SOLO-1C (original) | ESP32-C3-SOLO-1 (newer) |
|---|---|---|
| YAML to use | `classic300s_esp32solo.yaml` | `classic300s_esp32c3solo.yaml` |
| A5 bus pins | GPIO16 (RX) / GPIO17 (TX) | GPIO18 (RX) / GPIO19 (TX) |
| Flashing pins | GPIO3 (RX) / GPIO1 (TX), boot=GPIO0 | GPIO21 (RX) / GPIO20 (TX), boot=GPIO9 |

Check which one you have with `esptool.py flash_id` (or the module's own
printed markings) **before** picking a YAML file. Flashing the wrong pin
mapping won't damage anything, it just won't talk to the appliance MCU.

Full protocol details, status byte map, and hardware notes: see
[`docs/A5_UART_protocol.md`](docs/A5_UART_protocol.md) (mirrored from the
Taxom repo, see credit above) and the project's own research notes.

## Before you flash: back up the stock firmware

Do a **full flash dump**, not just the app partition, and keep it private
(it contains your WiFi credentials and VeSync pairing data):

```
esptool.py --port /dev/ttyUSBx flash_id
esptool.py --chip esp32 --port /dev/ttyUSBx --baud 460800 --before no-reset --after no-reset read-flash 0x000000 0x400000 backup.bin
```

(use `--chip esp32c3` for the ESP32-C3-SOLO-1 variant). Read it twice and
diff/hash the two files to make sure the backup is stable before you flash
over it -- see the source protocol repo's `ESPHome_replacement_notes.md` for
why (the stock firmware can update its own NVS data between reads).

## Install

1. `cp secrets.yaml.example secrets.yaml` and fill in your WiFi credentials,
   an API encryption key, an OTA password, and a fallback AP password.
2. Pick the YAML file matching your hardware variant (see table above).
3. First flash has to be over the serial/UART0 programming pins (no OTA yet
   since the stock firmware isn't ESPHome):

   ```
   esphome run classic300s_esp32solo.yaml --device /dev/ttyUSBx
   ```

   (or `classic300s_esp32c3solo.yaml`, `esphome upload` if you just want to
   flash without the log console).
4. After that, OTA updates work normally over WiFi.

`common_entities.yaml` holds every entity definition (shared by both
hardware-variant files via `packages:`), so you only need to edit the
`esp32:` / `uart:` blocks in the top-level file if you want to tweak pins.

## Design notes worth knowing before you wire up automations

- **Stop At Target vs Target Stop Active**: these are two different things.
  The `Stop At Target` switch is the behavior you command; `Target Stop
  Active` (binary sensor) is the MCU's live report of whether output is
  currently being inhibited by the target condition. Don't conflate them in
  your dashboard.
- **Mode + target/level are the same command.** Changing the `Auto Target
  Humidity` or `Sleep Target Humidity` number, or the `Manual Mist Level`
  number, also switches the appliance into that mode (matching stock
  behavior, which always sends a sync preamble before a mode-establishing
  command). If you just want to *adjust* the target while already in that
  mode, that's fine -- it's idempotent -- just know it's not a "set value
  without changing mode" control.
- **Physical front-panel button presses are not commands we send** -- the
  MCU reports them via status frames, and this component listens
  continuously and updates accordingly. If you see state change without an
  ESPHome log line for a command, that's the physical panel being used.
- Target humidity range on the `number` entities is set to a conservative
  30-80% (see `components/lv_classic300s_humidifier/number.py`) since the
  exact MCU-enforced bounds aren't documented; widen it if your unit accepts
  more.

## Status of this build

Config validated with `esphome config` against ESPHome 2026.6.5 for both
hardware variants (clean, no errors) and code generation (`esphome compile`
up through C++ source generation) completed successfully. The final
toolchain compile step (downloading the ESP-IDF build tools and invoking
gcc) could not be completed in the sandbox this was built in, due to a
network/TLS limitation of that environment unrelated to this code -- so the
very first `esphome run`/`esphome compile` you do will be the first real
end-to-end compile. If it turns up an error, it's most likely a small,
fixable C++ issue -- report it back with the compiler output.

The protocol documentation this is built on marks several fields as
`candidate` / `UNKNOWN` / not fully confirmed (see
`docs/A5_UART_protocol.md`) -- notably the exact meaning of some `01 29 A1`
startup variants, the E2 error code, and whether the stored Stop-At-Target
setting is exposed anywhere in the 20-byte status payload. Treat anything
derived from those fields as best-effort.

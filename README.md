# Levoit Classic 300S -- ESPHome replacement firmware

Custom ESPHome external component that replaces the stock WiFi/ESP module
firmware on a Levoit Classic 300S humidifier, talking directly to the
appliance MCU over its proprietary `A5` UART protocol. Gives you local
control in Home Assistant with no cloud/VeSync dependency.

<img width="1254" height="1254" alt="image" src="https://github.com/user-attachments/assets/942bcc66-45e4-4f88-a31d-c65c1e05ad7c" />



## Table of contents

1. [Credit](#credit)
2. [How it works](#how-it-works)
3. [What you get](#what-you-get)
4. [Identify your hardware variant](#identify-your-hardware-variant)
5. [What you'll need](#what-youll-need)
6. [Wiring](#wiring)
7. [Step 1 -- Back up the stock firmware](#step-1----back-up-the-stock-firmware)
8. [Step 2 -- Configure secrets](#step-2----configure-secrets)
9. [Step 3 -- Flash the ESPHome firmware](#step-3----flash-the-esphome-firmware)
10. [Step 4 -- Add it to Home Assistant](#step-4----add-it-to-home-assistant)
11. [Step 5 -- Reassemble and test on mains power](#step-5----reassemble-and-test-on-mains-power)
12. [Quick install via the Home Assistant ESPHome Dashboard](#quick-install-via-the-home-assistant-esphome-dashboard)
13. [Design notes worth knowing before you wire up automations](#design-notes-worth-knowing-before-you-wire-up-automations)
14. [Status of this build](#status-of-this-build)

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

## How it works

The Classic 300S has two separate MCUs on two separate boards:

- **The appliance board**, which reads the humidity/tank sensors, drives the
  mist output and the front-panel LEDs/buttons, and runs the actual
  humidifier logic. This board is untouched by this project.
- **The WiFi module** (an ESP32-SOLO-1C or ESP32-C3-SOLO-1, depending on
  when your unit was made), a small daughterboard that plugs into the
  appliance board and normally runs Levoit's stock firmware, talking to the
  appliance board over a simple UART link (the `A5` protocol) and to
  VeSync's cloud over WiFi.

This project replaces **only the WiFi module's firmware**. The new firmware
speaks the same `A5` UART protocol the appliance board already expects, so
from the appliance board's point of view nothing has changed -- it's still
being asked to turn mist on, report humidity, etc. What's different is that
the WiFi module now exposes all of that directly to Home Assistant over the
local network (ESPHome's native API), with no VeSync account, app, or
internet dependency involved at all. Physical front-panel controls (buttons,
display) keep working exactly as before, since those are handled entirely
by the appliance board -- the WiFi module only listens in and relays state.

Because it's a straight firmware swap on the existing module, no soldering
onto the appliance board itself, no wiring changes to the mist/sensor
hardware, and no 3D-printed parts are needed -- just reprogramming the WiFi
module's flash over its existing programming pins.

## What you get

- Sensors: current humidity, target humidity, temperature, night light
  level, output level
- Binary sensors: power, tank removed, water empty, mist active, display,
  target-stop-active, needs cleaning, replace water
- Switches: power, display, stop-at-target, night light notifications
- Numbers: manual mist level (1-9), auto target humidity, sleep target
  humidity
- Select: mode (auto / manual / sleep)
- Light: night light, as a real dimmable light entity (on/off + 0-100%),
  see below
- Diagnostic sensors: WiFi signal, uptime, IP address, connected SSID
- Diagnostic text sensors: mode (raw), error code, last raw status frame
- A restart button and a "reset cleaning reminder" button
- Automation triggers for the physical power button's long-press events
  (`on_power_button_hold_5s`, `on_power_button_hold_15s`) so you can script
  your own reboot / recovery behavior

### Night Light doubling as a status light

Two independent switches control this, so you can enable either one, both,
or neither -- there's no single master switch:

- **Night Light Problem Notification** -- when on, Night Light **flashes**
  whenever Water Empty or Tank Removed is true **and the appliance is
  powered on** (highest priority -- something needs your attention). The
  Power check is deliberate: with Power off, pulling the tank out to refill
  or clean it is routine, not a problem, so it's excluded on purpose --
  otherwise Night Light would flash the whole time the tank is out for a
  refill.
- **Night Light Operating Notification** -- when on, Night Light does a
  **slow breathing pulse (0->100%->0%)** while Mist Active is true and
  there's no problem

If a problem is active and Problem Notification is on, that always wins
over the breathing pulse, regardless of Operating Notification. With both
switches off, Night Light is a plain, manually-controlled light untouched
by any of this. With at least one switch on, Night Light gets reclaimed by
whichever of the above currently applies, and forced off when neither does
(so manual control while at least one of these is on gets overridden).

The logic lives in `common_entities.yaml`, in one script,
`apply_night_light_state` -- tweak the flash speed, breathing speed, or
priority order there if you want something different. While an effect is
running, the Night Light *entity* in Home Assistant doesn't flicker between
the in-between brightness/on-off values each effect tick produces -- it
only updates when an effect starts/stops or you control the light
manually, so its state history stays meaningful. The physical light still
pulses/flashes for real; only the HA-facing state is held steady.

Two things worth knowing about how this interacts with the appliance
itself, both confirmed by testing on real hardware:

- **The front-panel display wakes up momentarily on *any* command sent to
  the appliance over the A5 bus** -- this is the appliance's own MCU
  behavior, not something this firmware does on purpose. Since the
  breathing/flashing effects above are continuously sending real commands
  to physically drive the Night Light LED, the display will stay lit for
  as long as a notification effect is active, even if you've turned
  **Display** off. There's no way to suppress this from the WiFi module
  side -- it's baked into the main appliance board's firmware, which this
  project doesn't touch.
- The breathing/flashing effects send an actual UART command on every
  update tick (there's no local GPIO LED for Night Light -- it's a remote
  light on the appliance MCU), so their update interval doubles as a
  command rate on the same bus used for status polling. Sending them too
  fast (the original design used 40ms/250ms ticks) can starve the MCU's
  replies to periodic status requests, which then freezes Tank
  Removed/Water Empty/Mist Active at stale values for as long as an effect
  is running -- so notifications stop reacting to real changes until
  something forces a fresh check (like toggling the Notifications switch
  itself). If you tune the effect speeds in `common_entities.yaml`, keep
  this in mind; there's no precisely documented "safe" rate for the
  appliance MCU, so treat any change as something to retest, not just
  something to eyeball.

### Maintenance reminders

Two independent binary sensors, both diagnostic-only (they don't change
anything on their own -- wire up your own HA automation/notification if you
want to be alerted):

- **Needs Cleaning** turns on once roughly **3 days (72h) of cumulative
  misting time** have accumulated since the last reset -- a rough proxy for
  "the tank/wick have been used enough that it's worth cleaning them."
  Press the **Reset Cleaning Reminder** button after cleaning to zero the
  counter. This tracks *actual running time*, not calendar time, so a unit
  that's barely used will take much longer than 3 calendar days to trip it.
- **Replace Water** turns on when the appliance **hasn't misted at all in
  roughly 3 calendar days (72h)** -- water that's been sitting unused in the
  tank that long is worth replacing before the next use. It clears itself
  automatically the moment it's used again.

Both rely on the `time:` component (synced from Home Assistant over the
API) and a couple of `globals:` counters -- see `common_entities.yaml` if
you want to change the 72h threshold for either one.

## Identify your hardware variant

Levoit shipped this model with **two different WiFi modules** over time:

| | ESP32-SOLO-1C (original) | ESP32-C3-SOLO-1 (newer) |
|---|---|---|
| Core | Xtensa, single-core | RISC-V, single-core |
| YAML to use | `classic300s_esp32solo.yaml` | `classic300s_esp32c3solo.yaml` |
| A5 bus pins (to appliance board) | GPIO16 (RX) / GPIO17 (TX) | GPIO18 (RX) / GPIO19 (TX) |
| Flashing/programming pins | GPIO3 (RX) / GPIO1 (TX), boot=GPIO0 | GPIO21 (RX) / GPIO20 (TX), boot=GPIO9 |

You can usually tell from the module's own silkscreen markings, but the
sure way is to connect just power + the programming UART pins (see
[Wiring](#wiring)) and run:

```
esptool.py --port COMx flash_id
```

(`COMx` on Windows, `/dev/ttyUSBx` on Linux/Mac). It will print the chip
type (`ESP32-D0WD`/`ESP32-S0WD` vs `ESP32-C3`) before you've committed to
any pin mapping. Flashing the wrong pin mapping for the A5 bus won't damage
anything -- the firmware just won't be able to talk to the appliance board
until you fix it.

The header pads on the module are usually unpopulated (no pins soldered) --
you'll need to solder your own wires or pins to them to connect anything.

## What you'll need

- A Philips #2 screwdriver
- A T20 screwdriver
- A USB-UART TTL adapter (3.3V logic level -- **not** 5V) plugged into your
  computer
- An external 3.3V power supply for the module during flashing (a bench
  supply, or a second USB-UART adapter used only for its 3V3/GND pins,
  works fine -- you do **not** need to power the appliance from mains for
  any of this)
- A way to make temporary(pen probes with BDM frame) or soldered connections to 6 pads on the module:
  **3V3, GND, EN, IO0 (boot strap), RX, TX**
- [`esptool`](https://github.com/espressif/esptool) and
  [ESPHome](https://esphome.io/) installed on your computer (`pip install
  esptool esphome`)

This guide documents the setup actually used to develop and test this
project: **6 wires soldered directly onto the module's unpopulated header
pads** (GND, 3V3, EN, RX, TX, IO0), an external power supply for 3V3/GND,
and a USB-UART TTL adapter for RX/TX with its ground tied to the same
ground as the power supply. Soldering permanent wires instead of using
pogo-pin test probes makes bootloader entry (see below) much less fiddly,
since EN and IO0 don't need to be shared/reused with other pins mid-procedure.

## Wiring

1. Remove the 7 Philips #2 screw (4 hidden under rubber pad) and T20 screw, remove the botttom cover:

<img width="1446" height="1012" alt="image" src="https://github.com/user-attachments/assets/5d8eafbc-4652-44e1-bfd4-66283950d92f" />


2. **Remove the 2 philips screw securing the the WiFi/Display module assembly bracket. Disconnect all cables fro the Wifi/Display module assembly. Remove the module from the appliance** so you can access its
   header pads directly.

<img width="1335" height="1025" alt="image" src="https://github.com/user-attachments/assets/225527d6-6916-48c4-b71c-8f478ca88de9" />
<img width="1876" height="865" alt="image" src="https://github.com/user-attachments/assets/df920f5d-4602-48f1-ab15-1a5b3ab0eef7" />

   
3. **Solder 6 wires** to the module's pads: `3V3`, `GND`, `EN`, `IO0`, `RX`,
   `TX`. `EN` and `IO0` are strapping pins used only to enter bootloader
   mode -- once you're done flashing you can leave them unconnected in
   normal use.
4. **Power**: connect your external 3.3V supply's `3V3` and `GND` outputs
   to the module's `3V3`/`GND` wires. Do **not** power the module from the
   USB-UART adapter's own 3V3/5V pin -- use a proper external supply.
5. **Data**: connect the USB-UART adapter's `RX` to the module's `TX`, and
   the adapter's `TX` to the module's `RX` (crossed, as usual for UART).
6. **Ground**: connect the USB-UART adapter's `GND` to the same ground as
   the power supply/module. This is easy to forget since the adapter isn't
   powering anything, but without a common ground reference the serial
   link will be unreliable or fail outright ("Invalid head of packet",
   "serial noise or corruption" errors from esptool).
7. Double-check the supply is actually outputting **3.3V, not 5V**, before
   connecting anything -- 5V on these pins can damage the module.

<img width="954" height="702" alt="image" src="https://github.com/user-attachments/assets/ef56d672-f113-40ef-a77d-9d6663cfd570" />
<img width="998" height="2160" alt="20260923_151726" src="https://github.com/user-attachments/assets/1753b62b-0d05-4323-9845-d35abcd01483" />


With that done, you have independent, always-available access to `EN` and
`IO0` for bootloader entry, without needing to borrow/share pins with
anything else.

### Entering bootloader mode

The ESP32(-C3) only enters its UART bootloader (needed for `esptool` to
talk to it) if `IO0` is held low at the moment of reset. With `EN` and
`IO0` both wired out, the sequence is:

1. With the module already powered (3V3/GND connected) and `IO0` **not**
   yet grounded, touch `IO0` to `GND` and keep it there.
2. While `IO0` is still held to `GND`, briefly touch `EN` to `GND` as well
   (a fraction of a second) to reset the chip, then release `EN`.
3. Keep `IO0` held to `GND` for about another second after releasing `EN`,
   then release `IO0`.

The chip is now in bootloader mode and stays there indefinitely (no
timeout) until the next reset or power loss -- you can run multiple
`esptool` commands back-to-back without repeating this.

To leave bootloader mode and boot your newly-flashed firmware normally,
reset again with `IO0` **not** grounded: just pulse `EN` to `GND` and
release it (or power-cycle the module).

## Step 1 -- Back up the stock firmware

Always do this before flashing anything, even if you don't think you'll
ever want to go back. Do a **full flash dump**, not just the app partition,
and keep the resulting file **private** -- it contains your WiFi
credentials and VeSync pairing data in plain text:

```
esptool.py --port COMx flash_id
esptool.py --chip esp32 --port COMx --baud 460800 --before no-reset --after no-reset read-flash 0x000000 0x400000 backup1.bin
esptool.py --chip esp32 --port COMx --baud 460800 --before no-reset --after no-reset read-flash 0x000000 0x400000 backup2.bin
```

(use `--chip esp32c3` for the ESP32-C3-SOLO-1 variant; enter bootloader
mode once before the first command -- `--before no-reset --after no-reset`
keeps the chip in bootloader between commands so you don't need to repeat
the EN/IO0 sequence for the second read).

Read it **twice** into two separate files and compare their hashes:

```
certutil -hashfile backup1.bin SHA256      (Windows)
sha256sum backup1.bin backup2.bin          (Linux/Mac)
```

They should match exactly. If they don't, the stock firmware likely
touched its own NVS data between reads -- redo both reads. Once you have a
verified, matching pair, you have a safe way back to stock.

## Step 2 -- Configure secrets

1. `cp secrets.yaml.example secrets.yaml` (this file is gitignored -- never
   commit it or share it).
2. Fill in the 5 values:
   - `wifi_ssid` / `wifi_password` -- your WiFi network
   - `api_encryption_key` -- a random 32-byte base64 key, e.g. generate one
     with `python3 -c "import os, base64; print(base64.b64encode(os.urandom(32)).decode())"`
   - `ota_password` -- any password of your choosing, protects future OTA
     updates
   - `ap_password` -- any password of at least 8 characters, for the
     fallback WiFi AP the device creates if it can't reach your network

## Step 3 -- Flash the ESPHome firmware

1. Pick the YAML file matching your hardware variant (see
   [Identify your hardware variant](#identify-your-hardware-variant)).
2. Enter bootloader mode (see [Wiring](#wiring) above).
3. Flash over the same serial connection used for the backup -- this first
   flash has to be wired, since the stock firmware isn't ESPHome and can't
   do an OTA handoff:

   ```
   esphome run classic300s_esp32c3solo.yaml --device COMx
   ```

   (or `classic300s_esp32solo.yaml` for the older variant; use `esphome
   upload` instead of `run` if you don't want the log console -- note that
   UART logging is disabled in this config on purpose, since UART0 is
   shared with the flashing pins on the C3, so you won't see live logs
   over serial either way).

   The very first run also downloads and caches the ESP-IDF build
   toolchain, which can take a few minutes depending on your connection --
   subsequent builds are much faster.
4. Once flashing finishes, esptool's own "reset" at the end does nothing
   useful here (`EN`/`IO0` aren't wired to the adapter's RTS/DTR lines), so
   the chip is likely still sitting in the bootloader rather than running
   your new firmware. **Boot it normally**: with `IO0` *not* grounded,
   pulse `EN` to `GND` and release it (or power-cycle the module).
5. Give it 30-60 seconds to connect to WiFi, then find its IP address --
   check your router's/access point's client list for a device named after
   whatever you set under `esphome: name:` (`levoit-classic300s` by
   default), or try reaching `levoit-classic300s.local` directly.
6. After this first wired flash, all future updates can be done over WiFi
   (`esphome run classic300s_esp32c3solo.yaml`, no `--device` needed) --
   no more re-opening the appliance.

## Step 4 -- Add it to Home Assistant

Home Assistant should auto-discover the device via mDNS within a minute or
two of it joining your network:

1. **Settings -> Devices & services** -- look for a "Discovered" card for
   your device's name.
2. Click **Configure**, paste in the `api_encryption_key` from your
   `secrets.yaml` when asked.

If nothing is discovered automatically, add it manually instead:
**Settings -> Devices & services -> Add integration -> ESPHome**, enter the
IP address (or `<name>.local`), port `6053`, and the same encryption key.

All entities appear grouped under a single device once connected.

## Step 5 -- Reassemble and test on mains power

Once you've confirmed the device connects to WiFi and shows up in Home
Assistant, reinstall the WiFi module back into the appliance (A5 bus wires
reconnected to the appliance board), reassemble the housing, and power the
appliance from mains as normal. Within a few seconds of the appliance
board powering up, the firmware's status-request/status-response exchange
should populate all the sensors and binary sensors with real values, and
commands sent from Home Assistant should be reflected on the physical
front panel.

## Quick install via the Home Assistant ESPHome Dashboard

If you just want to add this device from the ESPHome Dashboard built into
Home Assistant without cloning this whole repo, create a new device with a
minimal YAML that pulls the component and the shared entities straight from
GitHub:

```yaml
esphome:
  name: levoit-classic300s
  friendly_name: Levoit Classic 300S

esp32:
  board: esp32-c3-devkitm-1     # or esp32dev for the ESP32-SOLO-1C variant
  variant: esp32c3              # remove this line for the ESP32-SOLO-1C variant
  flash_size: 4MB
  framework:
    type: esp-idf

logger:
  baud_rate: 0   # UART0 is shared with the A5 bus / flashing pins on the C3

api:
  encryption:
    key: !secret api_encryption_key

ota:
  - platform: esphome
    password: !secret ota_password

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  ap:
    ssid: "Levoit Classic 300S Fallback"
    password: !secret ap_password

captive_portal:

external_components:
  - source: github://alray31/levoit-classic300s-esphome@main
    components: [lv_classic300s_humidifier]

uart:
  id: a5_uart_bus
  rx_pin: GPIO18   # GPIO16 on the ESP32-SOLO-1C variant
  tx_pin: GPIO19   # GPIO17 on the ESP32-SOLO-1C variant
  baud_rate: 9600

packages:
  common: github://alray31/levoit-classic300s-esphome/common_entities.yaml@main
```

You'll still need a `secrets.yaml` next to this file with the same 5 keys as
`secrets.yaml.example`. ESPHome downloads and caches `components/` and
`common_entities.yaml` from GitHub at compile time, so nothing else needs to
live locally -- and future fixes/features pushed to this repo show up on
your next compile automatically.

Both `external_components:` and `packages:` cache the GitHub clone for
**24 hours by default** (`refresh: 1d`). If you're actively iterating on
this repo and want your next compile to pick up a change you just pushed
immediately, add `refresh: 0s` to the `external_components:` entry, and
expand `packages:` to its long form to do the same:

```yaml
external_components:
  - source: github://alray31/levoit-classic300s-esphome@main
    components: [lv_classic300s_humidifier]
    refresh: 0s

packages:
  common:
    url: https://github.com/alray31/levoit-classic300s-esphome
    files: [common_entities.yaml]
    ref: main
    refresh: 0s
```

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
- **The front-panel display wakes up briefly on any command sent to the
  appliance**, regardless of which entity that command came from -- this is
  the appliance MCU's own behavior (see
  [Night Light doubling as a status light](#night-light-doubling-as-a-status-light)),
  not something this firmware controls. If Display is off and something
  else you control (the Night Light notification switches especially,
  since an active effect sends commands continuously) is sending commands,
  expect the display to stay lit for as long as that keeps happening.
- Target humidity range on the `number` entities is set to a conservative
  30-80% (see `components/lv_classic300s_humidifier/number.py`) since the
  exact MCU-enforced bounds aren't documented; widen it if your unit accepts
  more.
- The maintenance reminders (`Needs Cleaning`, `Replace Water`) are purely
  diagnostic -- nothing in this firmware acts on them automatically. Add
  your own Home Assistant automation on those binary sensors if you want a
  notification.

## Status of this build

Flashed and running on a real ESP32-C3-SOLO-1 unit. Core entities (sensors,
binary sensors, switches, numbers, select) are confirmed working
end-to-end, including reassembly onto the appliance and communication over
the real A5 bus.

The Night Light `light` entity and its notification effects
(breathing/flashing) are also flash-tested and working now, after a few
rounds of fixes driven by real-hardware testing:

- The original effect update rates (40ms/250ms) sent UART commands fast
  enough to starve the appliance's replies to periodic status polls,
  freezing Tank Removed/Water Empty/Mist Active at stale values for as long
  as an effect ran -- so notifications never reacted to real changes on
  their own. Slowed down (150ms for the breathing pulse, 400ms for the
  error flash) to leave the bus enough room.
- Turning the notification switch on while the humidifier was already
  running did nothing until some unrelated sensor changed state, because
  of an ESPHome ordering quirk: a template switch's `turn_on_action` runs
  *before* the switch publishes its own new state, so a script checking
  that switch's state from inside its own `turn_on_action` would read the
  stale (still-off) value. Fixed by having each switch mirror its own state
  into a `globals:` bool (synchronously, no publish-timing issue) instead
  of the script reading `switch.is_on:` -- see
  [Night Light doubling as a status light](#night-light-doubling-as-a-status-light).
  This also made it straightforward to split what was originally one
  "Night Light Notifications" switch into the two independent switches
  described there, since each just flips its own global.
- The Night Light entity in Home Assistant no longer flickers through every
  in-between brightness/on-off value the effects produce -- only the
  physical light does. See the same section above.

The maintenance-reminder sensors (`Needs Cleaning`, `Replace Water`) and the
WiFi diagnostic sensors have passed config validation and C++ code
generation but have not yet been separately confirmed on real hardware
beyond what's needed for the fixes above.

The protocol documentation this is built on marks several fields as
`candidate` / `UNKNOWN` / not fully confirmed (see
`docs/A5_UART_protocol.md`) -- notably the exact meaning of some `01 29 A1`
startup variants, the E2 error code, and whether the stored Stop-At-Target
setting is exposed anywhere in the 20-byte status payload. Treat anything
derived from those fields as best-effort.

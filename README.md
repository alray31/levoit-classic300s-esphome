# Levoit Classic 300S -- ESPHome replacement firmware / micrologiciel de remplacement ESPHome

🇬🇧 **[Read in English](#english)**  |  🇫🇷 **[Lire en français](#français)**

---

## English

Custom ESPHome external component that replaces the stock WiFi/ESP module
firmware on a Levoit Classic 300S humidifier, talking directly to the
appliance MCU over its proprietary `A5` UART protocol. Gives you local
control in Home Assistant with no cloud/VeSync dependency.

<img width="1254" height="1254" alt="image" src="https://github.com/user-attachments/assets/942bcc66-45e4-4f88-a31d-c65c1e05ad7c" />

### Table of contents

1. [Credit](#credit)
2. [How it works](#how-it-works)
3. [What you get](#what-you-get)
4. [Optional: a native `humidifier` card via a companion HACS integration](#optional-a-native-humidifier-card-via-a-companion-hacs-integration)
5. [Identify your hardware variant](#identify-your-hardware-variant)
6. [What you'll need](#what-youll-need)
7. [Wiring](#wiring)
8. [Step 1 -- Back up the stock firmware](#step-1----back-up-the-stock-firmware)
9. [Step 2 -- Configure secrets](#step-2----configure-secrets)
10. [Step 3 -- Flash the ESPHome firmware](#step-3----flash-the-esphome-firmware)
11. [Step 4 -- Add it to Home Assistant](#step-4----add-it-to-home-assistant)
12. [Step 5 -- Reassemble and test on mains power](#step-5----reassemble-and-test-on-mains-power)
13. [Quick install via the Home Assistant ESPHome Dashboard](#quick-install-via-the-home-assistant-esphome-dashboard)
14. [Design notes worth knowing before you wire up automations](#design-notes-worth-knowing-before-you-wire-up-automations)
15. [Status of this build](#status-of-this-build)

### Credit

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

### How it works

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

### What you get

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

#### Night Light doubling as a status light

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

#### Maintenance reminders

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

### Optional: a native `humidifier` card via a companion HACS integration

The entities above expose Mode, the humidity/mist targets, and Power as
separate `select` / `number` / `switch` entities -- accurate, but it means
several separate rows/cards in Lovelace instead of the single familiar
"humidifier" widget (power toggle + Auto/Sleep/Manual dropdown + one target
dial) most native Home Assistant humidifier integrations give you.

To get that, there's a separate companion project:
[**alray31/levoit-classic300s-humidifier-bridge**](https://github.com/alray31/levoit-classic300s-humidifier-bridge)
-- a small HACS integration that combines this firmware's `select`/
`number`/`switch` entities into one native `humidifier` entity, set up
entirely through the UI (Settings -> Devices & services -> Add integration;
it auto-detects your device's entities and pre-fills the form, no YAML to
write).

**Why this needed its own integration instead of a plain `template:`
humidifier:** Home Assistant's built-in `template` integration has no
`humidifier` platform at all -- unlike `switch`, `light`, `select`, `number`,
etc., which all have one -- so a pure-YAML template humidifier isn't
possible for this domain. [Generic
Hygrostat](https://www.home-assistant.io/integrations/generic_hygrostat/)
was also considered and rejected: it makes its own on/off decisions from a
humidity sensor via its own hysteresis logic, which would fight with the
Auto/Sleep band logic already running on the appliance's own MCU. A
third-party HACS template humidifier exists too, but its min/max range is
fixed at setup time, so it can't give Manual mode its own 1-9 dial range
the way this bridge does. The bridge repo's README covers this reasoning
in full.

This is entirely **optional** -- the raw select/number/switch entities from
this firmware work perfectly well on their own. The bridge doesn't talk to
the appliance or this firmware directly either; it only relays
`switch`/`select`/`number` service calls to entities that already exist, so
it's maintained as a separate repo rather than folded into this one.

### Identify your hardware variant

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

### What you'll need

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

### Wiring

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

#### Entering bootloader mode

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

### Step 1 -- Back up the stock firmware

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

### Step 2 -- Configure secrets

1. `cp secrets.yaml.example secrets.yaml` (this file is gitignored -- never
   commit it or share it).
2. Fill in the 4 values:
   - `wifi_ssid` / `wifi_password` -- your WiFi network
   - `api_encryption_key` -- a random 32-byte base64 key, e.g. generate one
     with `python3 -c "import os, base64; print(base64.b64encode(os.urandom(32)).decode())"`.
     Also protects future OTA updates -- `ota:` reuses this same key instead
     of a separate password.
   - `ap_password` -- any password of at least 8 characters, for the
     fallback WiFi AP the device creates if it can't reach your network

### Step 3 -- Flash the ESPHome firmware

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

### Step 4 -- Add it to Home Assistant

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

### Step 5 -- Reassemble and test on mains power

Once you've confirmed the device connects to WiFi and shows up in Home
Assistant, reinstall the WiFi module back into the appliance (A5 bus wires
reconnected to the appliance board), reassemble the housing, and power the
appliance from mains as normal. Within a few seconds of the appliance
board powering up, the firmware's status-request/status-response exchange
should populate all the sensors and binary sensors with real values, and
commands sent from Home Assistant should be reflected on the physical
front panel.

### Quick install via the Home Assistant ESPHome Dashboard

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
    encryption:   # reuses the api: key above -- no separate OTA password

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

### Design notes worth knowing before you wire up automations

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

### Status of this build

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
- **`Water Empty` (and potentially other sensors) could stay stuck at
  "Unknown" in HA forever.** Every field decoded from a status frame only
  called `publish_state()` when it differed from the last known value, to
  avoid re-publishing on every 15s poll -- but that comparison started
  against a compile-time default (`false`/`0`), so a field whose actual
  first real reading matched that default (Water Empty is normally
  `false`, i.e. water present -- the common case) never got a first
  publish, and sat at "Unknown" indefinitely unless it happened to change
  at least once. Fixed in `lv_classic300s_humidifier.cpp`
  (`handle_status_payload_`) by forcing one publish per field on the very
  first successfully parsed status frame regardless of its value.

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

---

## Français

Composant externe ESPHome personnalisé qui remplace le micrologiciel
d'origine du module WiFi/ESP d'un humidificateur Levoit Classic 300S, en
communiquant directement avec le MCU de l'appareil via son protocole UART
propriétaire `A5`. Vous donne un contrôle local dans Home Assistant, sans
dépendance au cloud/VeSync.

<img width="1254" height="1254" alt="image" src="https://github.com/user-attachments/assets/942bcc66-45e4-4f88-a31d-c65c1e05ad7c" />

### Table des matières

1. [Crédit](#crédit)
2. [Comment ça fonctionne](#comment-ça-fonctionne)
3. [Ce que vous obtenez](#ce-que-vous-obtenez)
4. [Optionnel : une carte `humidifier` native via une intégration HACS complémentaire](#optionnel--une-carte-humidifier-native-via-une-intégration-hacs-complémentaire)
5. [Identifier votre variante matérielle](#identifier-votre-variante-matérielle)
6. [Ce dont vous aurez besoin](#ce-dont-vous-aurez-besoin)
7. [Câblage](#câblage)
8. [Étape 1 -- Sauvegarder le micrologiciel d'origine](#étape-1----sauvegarder-le-micrologiciel-dorigine)
9. [Étape 2 -- Configurer les secrets](#étape-2----configurer-les-secrets)
10. [Étape 3 -- Flasher le micrologiciel ESPHome](#étape-3----flasher-le-micrologiciel-esphome)
11. [Étape 4 -- L'ajouter à Home Assistant](#étape-4----lajouter-à-home-assistant)
12. [Étape 5 -- Réassembler et tester sur secteur](#étape-5----réassembler-et-tester-sur-secteur)
13. [Installation rapide via le tableau de bord ESPHome de Home Assistant](#installation-rapide-via-le-tableau-de-bord-esphome-de-home-assistant)
14. [Notes de conception à connaître avant de créer des automatisations](#notes-de-conception-à-connaître-avant-de-créer-des-automatisations)
15. [État de cette version](#état-de-cette-version)

### Crédit

Le protocole UART `A5` lui-même n'a **pas** été rétro-ingénié dans ce dépôt
-- il a été découvert par **Maxim Pivovarov / MaxPi (« Taxom »)** par
écoute UART en direct et par des tests actifs avec un contrôleur de
remplacement, puis publié sur
[Taxom/levoit-classic-300s-uart-protocol](https://github.com/Taxom/levoit-classic-300s-uart-protocol)
sous licence CC BY-NC-SA 4.0. Ce dépôt documente le protocole et donne des
notes matérielles, mais ne contient pas de composant ESPHome prêt à
flasher -- ce dépôt-ci est le composant ESPHome construit à partir de ce
protocole documenté (code original, non copié du dépôt source).

Si vous republiez ce projet ou vous en inspirez, continuez de créditer la
recherche originale sur le protocole -- c'est la partie difficile.

### Comment ça fonctionne

Le Classic 300S possède deux MCU distincts sur deux cartes séparées :

- **La carte de l'appareil**, qui lit les capteurs d'humidité/de réservoir,
  pilote la sortie de brume et les LED/boutons du panneau avant, et exécute
  la logique réelle de l'humidificateur. Cette carte n'est pas touchée par
  ce projet.
- **Le module WiFi** (un ESP32-SOLO-1C ou un ESP32-C3-SOLO-1, selon la date
  de fabrication de votre unité), une petite carte fille qui se branche sur
  la carte de l'appareil et exécute normalement le micrologiciel d'origine
  de Levoit, communiquant avec la carte de l'appareil via une simple
  liaison UART (le protocole `A5`) et avec le cloud de VeSync via WiFi.

Ce projet remplace **uniquement le micrologiciel du module WiFi**. Le
nouveau micrologiciel parle le même protocole UART `A5` que la carte de
l'appareil attend déjà, donc du point de vue de la carte de l'appareil,
rien n'a changé -- on continue de lui demander d'activer la brume, de
rapporter l'humidité, etc. Ce qui change, c'est que le module WiFi expose
maintenant tout cela directement à Home Assistant via le réseau local
(l'API native d'ESPHome), sans aucune dépendance à un compte VeSync, à
l'application, ou à Internet. Les contrôles physiques du panneau avant
(boutons, écran) continuent de fonctionner exactement comme avant, puisqu'ils
sont entièrement gérés par la carte de l'appareil -- le module WiFi ne fait
qu'écouter et relayer l'état.

Comme il s'agit d'un simple remplacement de micrologiciel sur le module
existant, aucune soudure sur la carte de l'appareil elle-même, aucun
changement de câblage sur le matériel de brume/capteurs, et aucune pièce
imprimée en 3D n'est nécessaire -- il suffit de reprogrammer la mémoire
flash du module WiFi via ses broches de programmation existantes.

### Ce que vous obtenez

- Sensors (capteurs) : humidité actuelle, humidité cible, température,
  niveau de veilleuse, niveau de sortie
- Binary sensors (capteurs binaires) : power (alimentation), tank removed
  (bac retiré), water empty (eau vide), mist active (brume active), display
  (écran), target-stop-active (arrêt-cible actif), needs cleaning
  (nettoyage requis), replace water (remplacer l'eau)
- Switches (interrupteurs) : power, display, stop-at-target, notifications
  de la Night Light
- Numbers (nombres) : niveau de brume manuel (1-9), humidité cible en mode
  auto, humidité cible en mode sommeil
- Select (sélecteur) : mode (auto / manual / sleep)
- Light (lumière) : night light, en tant que vraie entité lumière gradable
  (on/off + 0-100 %), voir ci-dessous
- Sensors diagnostiques : signal WiFi, uptime, adresse IP, SSID connecté
- Text sensors diagnostiques : mode (brut), code d'erreur, dernière trame
  de statut brute
- Un bouton restart et un bouton « reset cleaning reminder »
- Des déclencheurs d'automatisation pour les appuis longs sur le bouton
  d'alimentation physique (`on_power_button_hold_5s`,
  `on_power_button_hold_15s`) pour que vous puissiez scripter votre propre
  comportement de redémarrage/récupération

#### Night Light comme indicateur de statut

Deux interrupteurs indépendants contrôlent ce comportement, vous pouvez
donc activer l'un, l'autre, les deux, ou aucun -- il n'y a pas
d'interrupteur maître unique :

- **Night Light Problem Notification** -- quand activé, la Night Light
  **flashe** dès que Water Empty ou Tank Removed est vrai **et que
  l'appareil est allumé** (priorité la plus haute -- quelque chose demande
  votre attention). La vérification de Power est volontaire : quand Power
  est éteint, retirer le bac pour le remplir ou le nettoyer est une
  opération courante, pas un problème -- elle est donc exclue exprès,
  sinon la Night Light flasherait tout le temps que le bac est sorti pour
  un remplissage.
- **Night Light Operating Notification** -- quand activé, la Night Light
  fait un **pulse de respiration lent (0->100 %->0 %)** tant que Mist
  Active est vrai et qu'il n'y a pas de problème.

Si un problème est actif et que Problem Notification est activé, cela
l'emporte toujours sur le pulse de respiration, peu importe l'état de
Operating Notification. Avec les deux interrupteurs désactivés, la Night
Light est une simple lumière contrôlée manuellement, non touchée par tout
ceci. Avec au moins un interrupteur activé, la Night Light est reprise par
celui des deux comportements qui s'applique actuellement, et forcée à off
quand aucun des deux ne s'applique (donc le contrôle manuel, tant qu'au
moins un de ces interrupteurs est activé, se fait écraser).

La logique se trouve dans `common_entities.yaml`, dans un seul script,
`apply_night_light_state` -- ajustez-y la vitesse du flash, la vitesse de
la respiration, ou l'ordre de priorité si vous voulez un comportement
différent. Pendant qu'un effet tourne, l'*entité* Night Light dans Home
Assistant ne clignote pas à travers toutes les valeurs intermédiaires de
luminosité/on-off que chaque tick de l'effet produit -- elle ne se met à
jour que lorsqu'un effet démarre/s'arrête ou que vous contrôlez la lumière
manuellement, donc son historique d'état reste pertinent. La lumière
physique continue, elle, de vraiment pulser/flasher ; seul l'état visible
côté HA reste stable.

Deux choses à savoir sur la façon dont ceci interagit avec l'appareil
lui-même, toutes deux confirmées par des tests sur du matériel réel :

- **L'écran du panneau avant se rallume momentanément à la réception de
  *n'importe quelle* commande envoyée à l'appareil via le bus A5** -- c'est
  un comportement du MCU de l'appareil lui-même, pas quelque chose que ce
  micrologiciel fait exprès. Comme les effets de respiration/flash
  ci-dessus envoient continuellement de vraies commandes pour piloter
  physiquement la LED de la Night Light, l'écran restera allumé tant qu'un
  effet de notification est actif, même si vous avez éteint **Display**.
  Il n'y a aucun moyen de supprimer ce comportement du côté du module WiFi
  -- c'est intégré au micrologiciel de la carte principale de l'appareil,
  que ce projet ne touche pas.
- Les effets de respiration/flash envoient une vraie commande UART à
  chaque tick de mise à jour (il n'y a pas de LED GPIO locale pour la
  Night Light -- c'est une lumière distante sur le MCU de l'appareil),
  donc leur intervalle de mise à jour fait aussi office de débit de
  commandes sur ce même bus. Les envoyer trop vite (la conception
  d'origine utilisait des ticks de 40 ms/250 ms) peut affamer les réponses
  du MCU aux requêtes de statut périodiques, ce qui gèle alors Tank
  Removed/Water Empty/Mist Active à des valeurs périmées tant qu'un effet
  tourne -- les notifications cessent alors de réagir aux vrais
  changements jusqu'à ce que quelque chose force une nouvelle vérification
  (comme basculer l'interrupteur de notifications lui-même). Si vous
  ajustez la vitesse des effets dans `common_entities.yaml`, gardez ceci en
  tête ; il n'existe pas de débit « sûr » précisément documenté pour le
  MCU de l'appareil, donc traitez tout changement comme quelque chose à
  retester, pas juste à évaluer à l'œil.

#### Rappels d'entretien

Deux binary_sensors indépendants, tous deux purement diagnostiques (ils ne
changent rien d'eux-mêmes -- ajoutez votre propre automatisation/
notification HA si vous voulez être alerté) :

- **Needs Cleaning** s'active après environ **3 jours (72h) de temps de
  brumisation cumulé** depuis la dernière remise à zéro -- une
  approximation grossière de « le réservoir/la mèche ont assez servi pour
  valoir la peine d'être nettoyés ». Appuyez sur le bouton **Reset Cleaning
  Reminder** après le nettoyage pour remettre le compteur à zéro. Ceci suit
  le temps de fonctionnement *réel*, pas le temps calendaire, donc une
  unité peu utilisée prendra bien plus de 3 jours calendaires avant de se
  déclencher.
- **Replace Water** s'active quand l'appareil **n'a pas du tout brumisé
  depuis environ 3 jours calendaires (72h)** -- de l'eau qui stagne dans le
  réservoir depuis aussi longtemps vaut la peine d'être remplacée avant la
  prochaine utilisation. Il se réinitialise automatiquement dès que
  l'appareil est réutilisé.

Les deux s'appuient sur le composant `time:` (synchronisé depuis Home
Assistant via l'API) et quelques compteurs `globals:` -- voir
`common_entities.yaml` si vous voulez changer le seuil de 72h pour l'un ou
l'autre.

### Optionnel : une carte `humidifier` native via une intégration HACS complémentaire

Les entités ci-dessus exposent le mode, les cibles d'humidité/brume et
l'alimentation comme des entités `select` / `number` / `switch` séparées --
précis, mais cela donne plusieurs lignes/cartes distinctes dans Lovelace
plutôt que le widget « humidificateur » unique et familier (bouton power +
menu déroulant Auto/Sleep/Manual + un seul cadran de cible) qu'offrent la
plupart des intégrations d'humidificateur natives de Home Assistant.

Pour obtenir cela, il existe un projet complémentaire séparé :
[**alray31/levoit-classic300s-humidifier-bridge**](https://github.com/alray31/levoit-classic300s-humidifier-bridge)
-- une petite intégration HACS qui combine les entités `select`/`number`/
`switch` de ce micrologiciel en une seule vraie entité `humidifier`,
configurable entièrement via l'interface (Paramètres -> Appareils et
services -> Ajouter une intégration ; elle détecte automatiquement les
entités de votre appareil et pré-remplit le formulaire, aucun YAML à
écrire).

**Pourquoi une intégration à part plutôt qu'un simple `template:`
humidifier :** l'intégration `template` intégrée à Home Assistant n'a
tout simplement aucune plateforme `humidifier` -- contrairement à
`switch`, `light`, `select`, `number`, etc., qui en ont toutes une --
donc un template humidifier en YAML pur n'est pas possible pour ce
domaine. [Generic
Hygrostat](https://www.home-assistant.io/integrations/generic_hygrostat/)
a aussi été envisagé, puis écarté : il prend lui-même ses décisions
on/off à partir d'un capteur d'humidité via sa propre logique
d'hystérésis, ce qui entrerait en conflit avec la logique Auto/Sleep déjà
gérée par le MCU de l'appareil. Il existe aussi un template humidifier
HACS tiers, mais sa plage min/max est fixée à la configuration, donc il
ne peut pas donner au mode Manuel sa propre plage de cadran 1-9 comme le
fait cette passerelle. Le README du dépôt de la passerelle détaille ce
raisonnement en entier.

Ceci est entièrement **optionnel** -- les entités select/number/switch
brutes de ce micrologiciel fonctionnent très bien seules. La passerelle
ne parle ni à l'appareil ni à ce micrologiciel directement non plus ;
elle ne fait que relayer des appels de service `switch`/`select`/`number`
vers des entités qui existent déjà, d'où le fait qu'elle soit maintenue
comme un dépôt séparé plutôt qu'intégrée à celui-ci.

### Identifier votre variante matérielle

Levoit a livré ce modèle avec **deux modules WiFi différents** au fil du
temps :

| | ESP32-SOLO-1C (original) | ESP32-C3-SOLO-1 (plus récent) |
|---|---|---|
| Cœur | Xtensa, mono-cœur | RISC-V, mono-cœur |
| YAML à utiliser | `classic300s_esp32solo.yaml` | `classic300s_esp32c3solo.yaml` |
| Broches du bus A5 (vers la carte de l'appareil) | GPIO16 (RX) / GPIO17 (TX) | GPIO18 (RX) / GPIO19 (TX) |
| Broches de flashage/programmation | GPIO3 (RX) / GPIO1 (TX), boot=GPIO0 | GPIO21 (RX) / GPIO20 (TX), boot=GPIO9 |

Vous pouvez généralement le déterminer à partir des inscriptions
sérigraphiées sur le module, mais la méthode sûre est de connecter
uniquement l'alimentation + les broches UART de programmation (voir
[Câblage](#câblage)) et d'exécuter :

```
esptool.py --port COMx flash_id
```

(`COMx` sous Windows, `/dev/ttyUSBx` sous Linux/Mac). Cela affichera le
type de puce (`ESP32-D0WD`/`ESP32-S0WD` contre `ESP32-C3`) avant même que
vous ne vous engagiez dans un mapping de broches. Flasher le mauvais
mapping de broches pour le bus A5 n'endommagera rien -- le micrologiciel
ne pourra simplement pas parler à la carte de l'appareil tant que vous ne
l'aurez pas corrigé.

Les pastilles de connexion du module ne sont généralement pas peuplées
(aucune broche soudée) -- vous devrez y souder vos propres fils ou broches
pour connecter quoi que ce soit.

### Ce dont vous aurez besoin

- Un tournevis Philips #2
- Un tournevis T20
- Un adaptateur USB-UART TTL (niveau logique 3,3 V -- **pas** 5 V) branché
  à votre ordinateur
- Une alimentation externe 3,3 V pour le module pendant le flashage (une
  alimentation de laboratoire, ou un second adaptateur USB-UART utilisé
  uniquement pour ses broches 3V3/GND, fait très bien l'affaire -- vous
  n'avez **pas** besoin d'alimenter l'appareil sur secteur pour tout ceci)
- Un moyen de faire des connexions temporaires (sondes à pointe avec cadre
  BDM) ou soudées à 6 pastilles du module :
  **3V3, GND, EN, IO0 (strap de boot), RX, TX**
- [`esptool`](https://github.com/espressif/esptool) et
  [ESPHome](https://esphome.io/) installés sur votre ordinateur (`pip
  install esptool esphome`)

Ce guide documente le montage réellement utilisé pour développer et tester
ce projet : **6 fils soudés directement sur les pastilles non peuplées du
module** (GND, 3V3, EN, RX, TX, IO0), une alimentation externe pour
3V3/GND, et un adaptateur USB-UART TTL pour RX/TX avec sa masse reliée à la
même masse que l'alimentation. Souder des fils permanents plutôt que
d'utiliser des sondes de test à ressort rend l'entrée en mode bootloader
(voir plus bas) beaucoup moins délicate, puisque EN et IO0 n'ont pas besoin
d'être partagés/réutilisés avec d'autres broches en cours de procédure.

### Câblage

1. Retirez les 7 vis Philips #2 (4 cachées sous le pad en caoutchouc) et
   la vis T20, retirez le couvercle du bas :

<img width="1446" height="1012" alt="image" src="https://github.com/user-attachments/assets/5d8eafbc-4652-44e1-bfd4-66283950d92f" />

2. **Retirez les 2 vis Philips fixant le support de l'ensemble module
   WiFi/écran. Débranchez tous les câbles de l'ensemble module WiFi/écran.
   Retirez le module de l'appareil** pour accéder directement à ses
   pastilles de connexion.

<img width="1335" height="1025" alt="image" src="https://github.com/user-attachments/assets/225527d6-6916-48c4-b71c-8f478ca88de9" />
<img width="1876" height="865" alt="image" src="https://github.com/user-attachments/assets/df920f5d-4602-48f1-ab15-1a5b3ab0eef7" />

3. **Soudez 6 fils** aux pastilles du module : `3V3`, `GND`, `EN`, `IO0`,
   `RX`, `TX`. `EN` et `IO0` sont des broches de configuration (strapping
   pins) utilisées uniquement pour entrer en mode bootloader -- une fois le
   flashage terminé, vous pouvez les laisser non connectées en usage
   normal.
4. **Alimentation** : connectez les sorties `3V3` et `GND` de votre
   alimentation externe 3,3 V aux fils `3V3`/`GND` du module. N'alimentez
   **pas** le module depuis la broche 3V3/5V propre de l'adaptateur
   USB-UART -- utilisez une véritable alimentation externe.
5. **Données** : connectez le `RX` de l'adaptateur USB-UART au `TX` du
   module, et le `TX` de l'adaptateur au `RX` du module (croisé, comme
   d'habitude pour l'UART).
6. **Masse** : connectez le `GND` de l'adaptateur USB-UART à la même masse
   que l'alimentation/le module. C'est facile à oublier puisque l'adaptateur
   n'alimente rien, mais sans référence de masse commune, la liaison série
   sera peu fiable ou échouera carrément (erreurs « Invalid head of
   packet », « serial noise or corruption » d'esptool).
7. Vérifiez bien que l'alimentation fournit réellement **3,3 V, pas 5 V**,
   avant de connecter quoi que ce soit -- 5 V sur ces broches peut
   endommager le module.

<img width="954" height="702" alt="image" src="https://github.com/user-attachments/assets/ef56d672-f113-40ef-a77d-9d6663cfd570" />
<img width="998" height="2160" alt="20260923_151726" src="https://github.com/user-attachments/assets/1753b62b-0d05-4323-9845-d35abcd01483" />

Une fois cela fait, vous avez un accès indépendant et toujours disponible
à `EN` et `IO0` pour l'entrée en mode bootloader, sans avoir besoin
d'emprunter/partager des broches avec autre chose.

#### Entrer en mode bootloader

L'ESP32(-C3) n'entre dans son bootloader UART (nécessaire pour qu'`esptool`
puisse lui parler) que si `IO0` est maintenu bas au moment du reset. Avec
`EN` et `IO0` tous deux câblés en sortie, la séquence est :

1. Le module déjà alimenté (3V3/GND connectés) et `IO0` **pas encore**
   relié à la masse, touchez `IO0` à `GND` et maintenez-le là.
2. Pendant que `IO0` est toujours maintenu à `GND`, touchez brièvement `EN`
   à `GND` aussi (une fraction de seconde) pour réinitialiser la puce, puis
   relâchez `EN`.
3. Maintenez `IO0` à `GND` encore environ une seconde après avoir relâché
   `EN`, puis relâchez `IO0`.

La puce est maintenant en mode bootloader et y reste indéfiniment (pas de
timeout) jusqu'au prochain reset ou à la prochaine coupure d'alimentation
-- vous pouvez exécuter plusieurs commandes `esptool` à la suite sans
répéter cette séquence.

Pour quitter le mode bootloader et démarrer normalement votre
micrologiciel fraîchement flashé, réinitialisez à nouveau avec `IO0`
**non** relié à la masse : impulsez simplement `EN` vers `GND` et relâchez
(ou coupez/rétablissez l'alimentation du module).

### Étape 1 -- Sauvegarder le micrologiciel d'origine

Faites toujours ceci avant de flasher quoi que ce soit, même si vous
pensez ne jamais vouloir revenir en arrière. Faites un **dump flash
complet**, pas seulement la partition app, et gardez le fichier résultant
**privé** -- il contient vos identifiants WiFi et les données d'appairage
VeSync en clair :

```
esptool.py --port COMx flash_id
esptool.py --chip esp32 --port COMx --baud 460800 --before no-reset --after no-reset read-flash 0x000000 0x400000 backup1.bin
esptool.py --chip esp32 --port COMx --baud 460800 --before no-reset --after no-reset read-flash 0x000000 0x400000 backup2.bin
```

(utilisez `--chip esp32c3` pour la variante ESP32-C3-SOLO-1 ; entrez en
mode bootloader une fois avant la première commande -- `--before no-reset
--after no-reset` maintient la puce en bootloader entre les commandes pour
ne pas avoir à répéter la séquence EN/IO0 pour la seconde lecture).

Lisez-la **deux fois** dans deux fichiers séparés et comparez leurs
empreintes :

```
certutil -hashfile backup1.bin SHA256      (Windows)
sha256sum backup1.bin backup2.bin          (Linux/Mac)
```

Elles doivent correspondre exactement. Si ce n'est pas le cas, le
micrologiciel d'origine a probablement modifié ses propres données NVS
entre les deux lectures -- refaites les deux lectures. Une fois que vous
avez une paire vérifiée et identique, vous disposez d'un moyen sûr de
revenir à l'origine.

### Étape 2 -- Configurer les secrets

1. `cp secrets.yaml.example secrets.yaml` (ce fichier est dans le
   .gitignore -- ne le committez ni ne le partagez jamais).
2. Remplissez les 4 valeurs :
   - `wifi_ssid` / `wifi_password` -- votre réseau WiFi
   - `api_encryption_key` -- une clé base64 aléatoire de 32 octets, par
     exemple générée avec `python3 -c "import os, base64;
     print(base64.b64encode(os.urandom(32)).decode())"`. Protège aussi les
     futures mises à jour OTA -- `ota:` réutilise cette même clé au lieu
     d'un mot de passe séparé.
   - `ap_password` -- un mot de passe d'au moins 8 caractères, pour le
     point d'accès WiFi de secours que l'appareil crée s'il ne peut pas
     joindre votre réseau

### Étape 3 -- Flasher le micrologiciel ESPHome

1. Choisissez le fichier YAML correspondant à votre variante matérielle
   (voir [Identifier votre variante matérielle](#identifier-votre-variante-matérielle)).
2. Entrez en mode bootloader (voir [Câblage](#câblage) ci-dessus).
3. Flashez via la même connexion série utilisée pour la sauvegarde -- ce
   premier flashage doit se faire par fil, puisque le micrologiciel
   d'origine n'est pas ESPHome et ne peut pas faire de bascule OTA :

   ```
   esphome run classic300s_esp32c3solo.yaml --device COMx
   ```

   (ou `classic300s_esp32solo.yaml` pour la variante plus ancienne ;
   utilisez `esphome upload` au lieu de `run` si vous ne voulez pas la
   console de logs -- notez que le logging UART est désactivé exprès dans
   cette config, puisque UART0 est partagé avec les broches de flashage
   sur le C3, donc vous ne verrez pas de logs en direct via le port série
   de toute façon).

   La toute première exécution télécharge et met aussi en cache la chaîne
   de compilation ESP-IDF, ce qui peut prendre quelques minutes selon
   votre connexion -- les compilations suivantes sont bien plus rapides.
4. Une fois le flashage terminé, le « reset » propre d'esptool à la fin ne
   fait rien d'utile ici (`EN`/`IO0` ne sont pas câblés aux lignes RTS/DTR
   de l'adaptateur), donc la puce est probablement toujours en bootloader
   plutôt qu'en train d'exécuter votre nouveau micrologiciel. **Démarrez-la
   normalement** : avec `IO0` *non* relié à la masse, impulsez `EN` vers
   `GND` et relâchez (ou coupez/rétablissez l'alimentation du module).
5. Laissez-lui 30 à 60 secondes pour se connecter au WiFi, puis trouvez
   son adresse IP -- consultez la liste des clients de votre routeur/point
   d'accès pour un appareil nommé d'après ce que vous avez défini sous
   `esphome: name:` (`levoit-classic300s` par défaut), ou essayez de
   joindre directement `levoit-classic300s.local`.
6. Après ce premier flashage filaire, toutes les mises à jour futures
   peuvent se faire par WiFi (`esphome run classic300s_esp32c3solo.yaml`,
   sans `--device`) -- plus besoin de rouvrir l'appareil.

### Étape 4 -- L'ajouter à Home Assistant

Home Assistant devrait découvrir automatiquement l'appareil via mDNS dans
la minute ou les deux minutes suivant sa connexion à votre réseau :

1. **Paramètres -> Appareils et services** -- cherchez une carte
   « Découvert » pour le nom de votre appareil.
2. Cliquez sur **Configurer**, collez la `api_encryption_key` de votre
   `secrets.yaml` quand demandé.

Si rien n'est découvert automatiquement, ajoutez-le manuellement à la
place : **Paramètres -> Appareils et services -> Ajouter une intégration
-> ESPHome**, entrez l'adresse IP (ou `<name>.local`), le port `6053`, et
la même clé de chiffrement.

Toutes les entités apparaissent regroupées sous un seul appareil une fois
connecté.

### Étape 5 -- Réassembler et tester sur secteur

Une fois que vous avez confirmé que l'appareil se connecte au WiFi et
apparaît dans Home Assistant, réinstallez le module WiFi dans l'appareil
(fils du bus A5 reconnectés à la carte de l'appareil), réassemblez le
boîtier, et alimentez l'appareil sur secteur normalement. En quelques
secondes après la mise sous tension de la carte de l'appareil, l'échange
requête-statut/réponse-statut du micrologiciel devrait peupler tous les
sensors et binary_sensors avec de vraies valeurs, et les commandes
envoyées depuis Home Assistant devraient se refléter sur le panneau avant
physique.

### Installation rapide via le tableau de bord ESPHome de Home Assistant

Si vous voulez simplement ajouter cet appareil depuis le tableau de bord
ESPHome intégré à Home Assistant sans cloner tout ce dépôt, créez un
nouvel appareil avec un YAML minimal qui tire le composant et les entités
partagées directement depuis GitHub :

```yaml
esphome:
  name: levoit-classic300s
  friendly_name: Levoit Classic 300S

esp32:
  board: esp32-c3-devkitm-1     # ou esp32dev pour la variante ESP32-SOLO-1C
  variant: esp32c3              # supprimez cette ligne pour la variante ESP32-SOLO-1C
  flash_size: 4MB
  framework:
    type: esp-idf

logger:
  baud_rate: 0   # UART0 est partagé avec le bus A5 / les broches de flashage sur le C3

api:
  encryption:
    key: !secret api_encryption_key

ota:
  - platform: esphome
    encryption:   # réutilise la clé de api: ci-dessus -- pas de mot de passe OTA séparé

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
  rx_pin: GPIO18   # GPIO16 pour la variante ESP32-SOLO-1C
  tx_pin: GPIO19   # GPIO17 pour la variante ESP32-SOLO-1C
  baud_rate: 9600

packages:
  common: github://alray31/levoit-classic300s-esphome/common_entities.yaml@main
```

Il vous faudra quand même un `secrets.yaml` à côté de ce fichier avec les 5
mêmes clés que `secrets.yaml.example`. ESPHome télécharge et met en cache
`components/` et `common_entities.yaml` depuis GitHub à la compilation,
donc rien d'autre n'a besoin d'être local -- et les futurs correctifs/
fonctionnalités poussés sur ce dépôt apparaissent automatiquement à votre
prochaine compilation.

`external_components:` et `packages:` mettent tous deux en cache le clone
GitHub pendant **24 heures par défaut** (`refresh: 1d`). Si vous itérez
activement sur ce dépôt et voulez que votre prochaine compilation
récupère immédiatement un changement que vous venez de pousser, ajoutez
`refresh: 0s` à l'entrée `external_components:`, et développez `packages:`
vers sa forme longue pour faire de même :

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

### Notes de conception à connaître avant de créer des automatisations

- **Stop At Target contre Target Stop Active** : ce sont deux choses
  différentes. L'interrupteur `Stop At Target` est le comportement que
  vous commandez ; `Target Stop Active` (binary_sensor) est le rapport en
  direct du MCU indiquant si la sortie est actuellement inhibée par la
  condition de cible. Ne les confondez pas dans votre tableau de bord.
- **Mode + cible/niveau sont la même commande.** Changer le nombre `Auto
  Target Humidity` ou `Sleep Target Humidity`, ou le nombre `Manual Mist
  Level`, bascule aussi l'appareil dans ce mode (comme le comportement
  d'origine, qui envoie toujours un préambule de synchronisation avant une
  commande établissant un mode). Si vous voulez simplement *ajuster* la
  cible pendant que vous êtes déjà dans ce mode, ce n'est pas un problème
  -- c'est idempotent -- sachez juste que ce n'est pas un contrôle
  « changer la valeur sans changer le mode ».
- **Les appuis physiques sur les boutons du panneau avant ne sont pas des
  commandes que nous envoyons** -- le MCU les rapporte via des trames de
  statut, et ce composant écoute en continu et se met à jour en
  conséquence. Si vous voyez un changement d'état sans ligne de log
  ESPHome pour une commande, c'est le panneau physique qui est utilisé.
- **L'écran du panneau avant se rallume brièvement à la réception de
  n'importe quelle commande envoyée à l'appareil**, peu importe de quelle
  entité vient cette commande -- c'est un comportement propre au MCU de
  l'appareil (voir
  [Night Light comme indicateur de statut](#night-light-comme-indicateur-de-statut)),
  pas quelque chose que ce micrologiciel contrôle. Si Display est éteint
  et que quelque chose d'autre que vous contrôlez (les interrupteurs de
  notification de la Night Light en particulier, puisqu'un effet actif
  envoie des commandes en continu) envoie des commandes, attendez-vous à
  ce que l'écran reste allumé tant que cela continue.
- La plage d'humidité cible sur les entités `number` est fixée à une
  valeur prudente de 30-80 % (voir
  `components/lv_classic300s_humidifier/number.py`) puisque les bornes
  exactes imposées par le MCU ne sont pas documentées ; élargissez-la si
  votre unité en accepte davantage.
- Les rappels d'entretien (`Needs Cleaning`, `Replace Water`) sont
  purement diagnostiques -- rien dans ce micrologiciel n'agit dessus
  automatiquement. Ajoutez votre propre automatisation Home Assistant sur
  ces binary_sensors si vous voulez une notification.

### État de cette version

Flashé et fonctionnel sur une unité ESP32-C3-SOLO-1 réelle. Les entités
principales (sensors, binary_sensors, switches, numbers, select) sont
confirmées fonctionnelles de bout en bout, y compris le réassemblage sur
l'appareil et la communication sur le vrai bus A5.

L'entité `light` Night Light et ses effets de notification (respiration/
flash) sont maintenant, elles aussi, flash-testées et fonctionnelles,
après plusieurs rounds de correctifs guidés par des tests sur du matériel
réel :

- Les débits de mise à jour d'origine des effets (40 ms/250 ms) envoyaient
  des commandes UART assez vite pour affamer les réponses de l'appareil
  aux polls de statut périodiques, gelant Tank Removed/Water Empty/Mist
  Active à des valeurs périmées tant qu'un effet tournait -- les
  notifications ne réagissaient donc jamais aux vrais changements
  d'elles-mêmes. Ralenti (150 ms pour le pulse de respiration, 400 ms pour
  le flash d'erreur) pour laisser assez de marge au bus.
- Activer l'interrupteur de notification pendant que l'humidificateur
  tournait déjà ne faisait rien tant qu'un autre capteur ne changeait pas
  d'état, à cause d'une particularité d'ordonnancement d'ESPHome : le
  `turn_on_action` d'un interrupteur template s'exécute *avant* que
  l'interrupteur ne publie son propre nouvel état, donc un script
  vérifiant l'état de cet interrupteur depuis son propre `turn_on_action`
  lisait la valeur périmée (encore éteinte). Corrigé en faisant refléter à
  chaque interrupteur son propre état dans un booléen `globals:` (de façon
  synchrone, sans souci de timing de publication) plutôt que le script ne
  lise `switch.is_on:` -- voir
  [Night Light comme indicateur de statut](#night-light-comme-indicateur-de-statut).
  Cela a aussi permis de scinder facilement ce qui était à l'origine un
  seul interrupteur « Night Light Notifications » en les deux
  interrupteurs indépendants décrits là-bas, puisque chacun ne fait plus
  que basculer son propre booléen.
- L'entité Night Light dans Home Assistant ne clignote plus à travers
  chaque valeur intermédiaire de luminosité/on-off que les effets
  produisent -- seule la lumière physique le fait. Voir la même section
  ci-dessus.
- **`Water Empty` (et potentiellement d'autres capteurs) pouvait rester
  bloqué à « Unknown » dans HA indéfiniment.** Chaque champ décodé d'une
  trame de statut n'appelait `publish_state()` que s'il différait de la
  dernière valeur connue, pour éviter de republier à chaque poll de 15s --
  mais cette comparaison partait d'une valeur par défaut au compile-time
  (`false`/`0`), donc un champ dont la vraie première lecture correspondait
  à ce défaut (Water Empty vaut normalement `false`, c.-à-d. eau présente
  -- le cas courant) ne recevait jamais de première publication, et
  restait à « Unknown » indéfiniment tant qu'il n'avait pas changé au
  moins une fois. Corrigé dans `lv_classic300s_humidifier.cpp`
  (`handle_status_payload_`) en forçant une publication par champ dès la
  toute première trame de statut correctement décodée, peu importe sa
  valeur.

Les sensors de rappels d'entretien (`Needs Cleaning`, `Replace Water`) et
les sensors diagnostiques WiFi ont passé la validation de config et la
génération de code C++, mais n'ont pas encore été confirmés séparément sur
du matériel réel au-delà de ce qui était nécessaire pour les correctifs
ci-dessus.

La documentation du protocole sur laquelle ce projet est construit marque
plusieurs champs comme `candidate` / `UNKNOWN` / non entièrement confirmés
(voir `docs/A5_UART_protocol.md`) -- notamment le sens exact de certaines
variantes de démarrage `01 29 A1`, le code d'erreur E2, et si le réglage
Stop-At-Target stocké est exposé quelque part dans la charge utile de
statut de 20 octets. Traitez tout ce qui dérive de ces champs comme
approximatif.

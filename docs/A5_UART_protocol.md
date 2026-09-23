<!--
Mirrored from https://github.com/Taxom/levoit-classic-300s-uart-protocol
by Maxim Pivovarov / MaxPi ("Taxom"), licensed CC BY-NC-SA 4.0.
Mirrored here for convenience/offline reference; this file is not
original work of this repo. See ../README.md for full attribution.
-->

# A5 UART protocol notes for Levoit Classic 300S MCU / WiFi-module replacement

Version: draft 2026-06-16

Purpose: packet/status/control description recovered from UART sniffing and active replacement-controller tests.

## IMPORTANT

This is a working protocol map from captured logs. Most user-facing commands are confirmed. Unknown or not fully proven fields are explicitly marked as `UNKNOWN`, `candidate`, or `observed`.

## UART

```text
Baud rate: 9600
Format: 8N1, no flow control
Logical directions:
  WiFi/ESP -> MCU : control commands
  MCU -> WiFi/ESP : replies, full status packets, and short events
```

Tested original ESP32-SOLO replacement UART pins:

```text
Original ESP32-SOLO-1C GPIO16 = UART RX from humidifier MCU TX
Original ESP32-SOLO-1C GPIO17 = UART TX to humidifier MCU RX
```

Tested newer ESP32-C3-SOLO-1 replacement UART pins:

```text
ESP32-C3-SOLO-1 GPIO18 = UART RX from humidifier MCU TX
ESP32-C3-SOLO-1 GPIO19 = UART TX to humidifier MCU RX
```

### Electrical level warning

In the tested unit, the header on the humidifier MCU board carries 5 V power and 5 V UART logic levels.

The level conversion/protection is on the original WiFi/ESP module board, not on the MCU header itself. Therefore, if an external ESP32/ESP32-C3/ESP8266 or other 3.3 V controller is connected in place of the original ESP module board, the UART lines must be level-shifted or otherwise protected.

Do not connect a 3.3 V ESP RX pin directly to the MCU-board 5 V UART TX line.

The external ESP32-C3 diagnostic controller used during reverse engineering was connected through a transistor level shifter.

## Frame format

All captured `A5` frames use this structure:

```text
offset  size  description
0x00    1     0xA5 magic/header
0x01    1     frame type
0x02    1     packet id
0x03    1     payload length low byte
0x04    1     payload length high byte
0x05    1     checksum
0x06    N     payload
```

Total frame size:

```text
total_length = payload_length + 6
```

Checksum rule:

```text
sum(all frame bytes including checksum) & 0xFF == 0xFF
```

When building a frame:

```text
checksum = 0xFF - (sum(all bytes except checksum) & 0xFF)
```

Known frame types:

```text
A5-22 = WiFi/ESP -> MCU command
A5-12 = MCU -> WiFi/ESP reply / ACK / status reply
A5-52 = MCU -> WiFi/ESP NACK / rejected command
A5-02 = MCU -> WiFi/ESP full status or short event
```

Packet id notes:

- `A5-22` command and `A5-12` reply usually share the same id.
- `A5-02` full STATUS id is not a reliable monotonic packet counter.
- `A5-02` id may jump during events, boot sync, or command-triggered status.
- Therefore an `A5-02` packet id gap does not necessarily mean UART packet loss.

## Confirmed WiFi/ESP -> MCU commands, `A5-22`

The following sections list **payload only**. A full command frame is:

```text
A5 22 <id> <len-lo> <len-hi> <checksum> <payload...>
```

### 1. Power / main switch

```text
01 00 A0 00 00 = power OFF
01 00 A0 00 01 = power ON
```

Related STATUS field:

```text
p[7] = power state
  0x00 = OFF
  0x01 = ON
```

### 2. Night light / night lamp

```text
01 03 A0 00 01 00 = night light OFF / 0%
01 03 A0 00 01 32 = night light LOW / 50%
01 03 A0 00 01 64 = night light HIGH / 100%
```

Confirmed: intermediate/custom values are accepted.

Observed accepted examples:

```text
01 03 A0 00 01 0F = custom 15%
01 03 A0 00 01 14 = custom 20%
01 03 A0 00 01 1E = custom 30%
01 03 A0 00 01 46 = custom 70%
```

Related STATUS field:

```text
p[18] = night light brightness
  0x00 = OFF
  0x32 = LOW / 50%
  0x64 = HIGH / 100%
  other 0..100 values may be reported as custom brightness
```

### 3. Display / front-panel display

```text
01 05 A1 00 00 = display OFF / brightness 0
01 05 A1 00 64 = display ON / brightness 100%
```

Confirmed: display appears binary through the stock MCU UART protocol.

Intermediate values tested were rejected.

Observed rejected examples:

```text
01 05 A1 00 19 -> A5-52 NACK code 0x34
01 05 A1 00 32 -> A5-52 NACK code 0x34
01 05 A1 00 4B -> A5-52 NACK code 0x34
```

Related STATUS field:

```text
p[11] = display brightness/state
  0x00 = display OFF
  0x64 = display ON / 100%
```

Hardware note: the AiP650E display/key controller appears to support hardware brightness control, but the stock MCU UART protocol currently exposes only binary display ON/OFF.

### 4. Timer indicator / timer icon

```text
01 6A A2 00 00 = timer icon OFF
01 6A A2 00 01 = timer icon ON
```

Important: this does **not** set timer duration. Timer duration is not sent to the MCU. It is counted by the WiFi/Tuya module or by replacement firmware.

Observed timer behavior:

```text
When timer starts:
  WiFi/ESP -> MCU: 01 6A A2 00 01  // show timer icon

When timer expires:
  WiFi/ESP -> MCU: 01 00 A0 00 01  // power ON, for ON timer
  OR
  WiFi/ESP -> MCU: 01 00 A0 00 00  // power OFF, for OFF timer

  WiFi/ESP -> MCU: 01 6A A2 00 00  // hide timer icon
```

For replacement firmware: implement timer duration on the ESP side. Use the MCU only for timer icon and final power command.

### 5. Manual mode / mist level

```text
01 60 A2 00 00 01 xx = Manual mode, selected mist level xx
```

Known and tested values:

```text
01 = level 1 / minimum
05 = level 5 / middle
09 = level 9 / maximum
```

Likely valid range:

```text
01..09
```

Stock WiFi usually sends this before mode changes:

```text
01 29 A1 00 01 00 00 00 00 00
```

Then sends the actual manual command:

```text
01 60 A2 00 00 01 xx
```

Related STATUS fields:

```text
p[16] = 0x01 means MANUAL
p[17] = selected manual level, e.g. 0x01 / 0x05 / 0x09
p[12] = actual mist/output active, 0x00 / 0x01
```

Important replacement-firmware note:

```text
p[17] has different meaning outside MANUAL mode.
Do not overwrite a stored manual-level setting from p[17] while in AUTO/SLEEP.
```

### 6. Auto mode / target humidity band

```text
01 80 40 00 TT LL HH 09 05 01 = Auto mode, humidity target/band
```

Where:

```text
TT = target humidity
LL = lower threshold
HH = upper threshold
```

Observed pattern:

```text
LL = target - 5
HH = target + 5
```

Examples:

```text
1E 19 23 = target 30%, low 25%, high 35%
21 1C 26 = target 33%, low 28%, high 38%
33 2E 38 = target 51%, low 46%, high 56%
3F 3A 44 = target 63%, low 58%, high 68%
47 42 4C = target 71%, low 66%, high 76%
```

Stock WiFi usually sends this before mode changes:

```text
01 29 A1 00 01 00 00 00 00 00
```

Then sends:

```text
01 80 40 00 TT LL HH 09 05 01
```

Related STATUS fields:

```text
p[16] = 0x00 means AUTO
p[13] = target humidity
p[14] = current humidity
p[17] = auto output state/level
p[12] = actual mist/output active
p[10] = target stop active / output inhibited by target condition
```

### 7. Sleep mode / sleep humidity band

```text
01 82 40 00 TT LL HH 09 05 01 = Sleep mode, humidity target/band
```

Where:

```text
TT = target humidity
LL = lower threshold
HH = upper threshold
```

Example:

```text
33 2E 38 = target 51%, low 46%, high 56%
```

Related STATUS fields:

```text
p[16] = 0x02 means SLEEP
p[13] = target humidity
p[14] = current humidity
p[17] = sleep/auto output state/level
p[12] = actual mist/output active
p[10] = target stop active / output inhibited by target condition
```

### 8. Stop at target / Auto-off behavior command

App setting name: `auto-off`.

Practical protocol meaning: `Stop At Target` behavior.

```text
01 E5 A5 00 00 = Stop At Target behavior OFF
                  When target is reached/exceeded, do not fully stop;
                  keep minimum/maintain output behavior.

01 E5 A5 00 01 = Stop At Target behavior ON
                  When target condition is active, stop output.
```

Important update: the 20-byte STATUS packet does **not** appear to expose the stored Stop At Target setting directly.

The previously suspected field `p[10]` is better interpreted as:

```text
p[10] = Target Stop Active / output inhibited by target condition
  0x00 = not currently stopped by target condition
  0x01 = currently stopped/inhibited by target condition
```

This means the following states are valid and expected:

```text
Stop At Target behavior is ON, but humidity is below the upper threshold:
  p[10] = 0
  mist may remain ON

Stop At Target behavior is ON and humidity is above the upper threshold:
  p[10] = 1
  mist is stopped/inhibited
```

Observed behavior confirming the new interpretation:

```text
Target 60%, humidity 56%, Stop At Target command sent:
  p[10]=0, mist=ON

Target later lowered to 45%, humidity 57%:
  p[10]=1, mist=OFF
```

This is distinct from the hysteresis/band itself, which is set by:

```text
01 80 40 00 TT LL HH 09 05 01
01 82 40 00 TT LL HH 09 05 01
```

### 9. Request full status

```text
01 84 40 00 = request full status
```

MCU replies with `A5-12`, payload length `20`, status-like payload:

```text
01 84 40 00 0D 00 01 ...
```

Use this at startup for synchronization.

### 10. Sync / mode-change preamble / internal state

Main observed sync/preamble command:

```text
01 29 A1 00 01 00 00 00 00 00
```

Observed:

- before Auto -> Manual
- before Manual -> Auto
- before Auto/Manual/Sleep mode changes
- MCU replies with short ACK: `01 29 A1 00`

Working interpretation:

```text
01 29 A1 = sync / mode-change preamble / internal state command
```

For replacement firmware: send this before switching Auto/Manual/Sleep to match stock behavior.

Additional cold boot variants observed:

```text
01 29 A1 00 00 4B 00 4B 00 00
01 29 A1 00 00 7D 00 7D 00 00
01 29 A1 00 01 7D 00 7D 00 00
```

Exact meaning: `UNKNOWN`. Likely startup/internal sync or state restore after mains power-up.

## STATUS packets, MCU -> WiFi/ESP, `A5-02`

`A5-02` is not always a full 20-byte status.

Observed forms:

```text
A5-02 len=20 = full STATUS payload
A5-02 len=5  = short MCU event / notification
```

### Full STATUS packet

Format:

```text
A5 02 <id> 14 00 <checksum> <20-byte payload>
```

STATUS payload is 20 bytes:

```text
p00 p01 p02 p03 p04 p05 p06 p07 p08 p09 p10 p11 p12 p13 p14 p15 p16 p17 p18 p19
```

### Full STATUS payload map

```text
p[0] = 0x01, UNKNOWN / constant

p[1] = 0x85, UNKNOWN / status object id candidate
       Note: A5-12 full status reply to 01 84 40 uses 0x84 instead of 0x85.

p[2] = 0x40, UNKNOWN / object id part candidate
p[3] = 0x00, UNKNOWN / constant
p[4] = 0x0D, UNKNOWN / constant
p[5] = 0x00, UNKNOWN / constant
p[6] = 0x01, UNKNOWN / constant

p[7] = Power / main power state
       0x00 = OFF
       0x01 = ON

p[8] = Tank removed
       0x00 = tank installed
       0x01 = tank removed

p[9] = Water empty / low water alarm
       0x00 = water OK
       0x01 = water empty / low

p[10] = Target Stop Active / auto output inhibit
        0x00 = not currently stopped by target condition
        0x01 = currently stopped/inhibited by target condition

        Important: this is not the stored Stop At Target setting itself.
        It is an active state/result reported by the MCU.

p[11] = Display brightness/state
        0x00 = display OFF
        0x64 = display ON / 100%

p[12] = Actual mist/output active
        0x00 = mist/output OFF or blocked
        0x01 = mist/output ON / active

p[13] = Target humidity, percent
        Example: 0x2D = 45%

p[14] = Current humidity, percent
        Example: 0x42 = 66%

p[15] = Temperature, degrees Celsius
        Confirmed by cold and warm sensor tests.
        Observed low clamp around 10°C on this unit/sensor.
        Example: 0x16 = 22°C

p[16] = Work mode
        0x00 = AUTO
        0x01 = MANUAL
        0x02 = SLEEP

p[17] = Selected level / output level / mode state
        MANUAL: selected mist level, e.g. 0x01 / 0x05 / 0x09
        AUTO/SLEEP: automatic output level/state, e.g. 0x00 / 0x01 / 0x05 / 0x09

p[18] = Night light brightness
        0x00 = OFF
        0x32 = LOW / 50%
        0x64 = HIGH / 100%
        Other 0..100 custom values accepted/reported.

p[19] = Error / fault code candidate
        0x00 = OK / no error
        0x01 = E1 observed on the front-panel display;
               likely tank Hall sensor / tank-presence fault,
               observed while manipulating the tank magnet
        0x02 = E2 candidate / unconfirmed
        other = UNKNOWN / not yet decoded
```

### Confirmed STATUS behavior notes

Power:

```text
p[7]=0 -> power OFF
p[7]=1 -> power ON
```

Tank removed:

```text
p[8]=1 indicates tank removed.
Tank removed is an output interlock, not a command interlock.
The MCU accepts mode/manual-level commands while the tank is removed.
Actual mist output remains OFF while tank is removed.
When the tank is installed again, output may resume using the stored mode/level.
```

Water empty:

```text
p[9]=1 indicates low water / water-empty alarm.
Mist-related commands may be rejected with A5-52 code 0x14 while water-empty is active.
Replacement firmware may skip mist-related mode/level commands while water-empty is active.
```

Mist output:

```text
p[12] is actual physical/active output state.
p[17] is selected level or mode output state/level.
```

Manual level cache:

```text
In MANUAL mode, p[17] is selected manual level.
In AUTO/SLEEP mode, p[17] is mode output state/level.
Replacement firmware should not overwrite stored manual level from p[17] outside MANUAL mode.
```

Target stop active:

```text
p[10] is an active/inhibited state, not a stored setting.
For a replacement firmware UI, prefer:
  - Stop At Target: command/desired behavior switch
  - Target Stop Active: status/binary sensor from p[10]
```

### STATUS examples

Power ON, tank installed, water OK, display ON, auto mode, target stop active, no mist, night light OFF:

```text
01 85 40 00 0D 00 01 01 00 00 01 64 00 33 42 16 00 00 00 00
```

Decoded:

```text
power=ON tank=installed water=OK targetStopActive=ON display=ON mistOutput=OFF target=51% humidity=66% temp=22C mode=AUTO level=0 night=OFF error=OK
```

Manual max, tank installed, water OK, mist visible:

```text
01 85 40 00 0D 00 01 01 00 00 00 64 01 32 41 16 01 09 00 00
```

Decoded:

```text
power=ON tank=installed water=OK targetStopActive=OFF display=ON mistOutput=ON target=50% humidity=65% temp=22C mode=MANUAL selectedLevel=9 night=OFF error=OK
```

Tank removed, water OK, manual max selected, output blocked:

```text
01 85 40 00 0D 00 01 01 01 00 00 64 00 32 3F 16 01 09 00 00
```

Decoded:

```text
power=ON tank=removed water=OK display=ON mistOutput=OFF mode=MANUAL selectedLevel=9 error=OK
```

Water empty, power OFF:

```text
01 85 40 00 0D 00 01 00 00 01 00 64 00 2D 42 16 01 05 00 00
```

Decoded:

```text
power=OFF tank=installed water=EMPTY display=ON mistOutput=OFF target=45% humidity=66% temp=22C mode=MANUAL selectedLevel=5 error=OK
```

Observed E1 / tank Hall sensor candidate:

```text
p[19] = 0x01
```

Decoded:

```text
error=E1, likely tank Hall sensor / tank-presence fault candidate
```

## Short MCU events, `A5-02 len=5`

Observed format:

```text
A5 02 <id> 05 00 <checksum> 01 XX D1 00 YY
```

Confirmed physical power-button hold events:

```text
01 02 D1 00 02
  Power button hold reached about 5 seconds.
  Original firmware meaning: Wi-Fi pairing / reconnect threshold.
  Observed physical effect: WiFi indicator starts blinking.

01 02 D1 00 03
  Power button released after 5-second hold.
  Likely pairing/reconnect hold-completed release event.

01 03 D1 00 04
  Power button hold reached about 15 seconds.
  Original firmware meaning: factory reset / disconnect from WiFi and VeSync.
  Observed physical effect: WiFi indicator turns off.
```

Replacement ESPHome behavior tested in this project:

```text
5s threshold event + release event -> reboot ESP
15s threshold event -> cancel pending reboot and start recovery AP
```

## MCU -> WiFi/ESP REPLY, `A5-12`

### Short ACK

Most commands return short ACK:

```text
A5 12 <id> 04 00 <checksum> 01 xx yy 00
```

Examples:

```text
Command payload: 01 00 A0 00 01
Reply payload:   01 00 A0 00

Command payload: 01 03 A0 00 01 64
Reply payload:   01 03 A0 00

Command payload: 01 60 A2 00 00 01 09
Reply payload:   01 60 A2 00
```

Important: ACK means the command was recognized at protocol level. It does not always mean that every user-visible state changed immediately.

### Full status reply

Request:

```text
WiFi/ESP -> MCU: 01 84 40 00
```

MCU reply:

```text
A5-12 with len=20: 01 84 40 00 0D 00 01 ...
```

Useful for startup sync.

## MCU -> WiFi/ESP NACK / rejected command, `A5-52`

Observed format:

```text
A5 52 <id> 04 00 <checksum> 01 xx yy <code>
```

Known observed examples:

```text
01 05 A1 34
  Display command rejected with unsupported intermediate brightness.
  Observed for display values other than 0x00 or 0x64.
  Interpretation: unsupported value/range.

01 60 A2 14
  Manual level command rejected while current state did not allow mist output.
  Observed while power was OFF and water-empty alarm was active.
  Interpretation: state/interlock rejection candidate.
```

Known/observed error codes:

```text
0x34 = unsupported value/range candidate
0x14 = command rejected due to current state/interlock candidate
```

## Startup / cold boot behavior

Observed after mains power is applied:

1. WiFi module sends timer icon OFF:

```text
01 6A A2 00 00
```

2. WiFi module sends request full status:

```text
01 84 40 00
```

3. MCU replies with `A5-12` full status.
4. WiFi module sends several `01 29 A1` startup sync variants:

```text
01 29 A1 00 00 4B 00 4B 00 00
01 29 A1 00 00 7D 00 7D 00 00
01 29 A1 00 01 7D 00 7D 00 00
```

5. MCU emits `A5-02` STATUS.

Physical button behavior:

```text
When the user presses the physical power button, there may be no WiFi/ESP->MCU command.
The MCU changes state internally and sends A5-02 STATUS.
Replacement firmware must always listen for MCU-generated state changes.
```

## Recommended init for replacement ESP

Minimum startup sequence:

1. Init UART `9600 8N1`.
2. Start A5 parser.
3. `nextPacketId = 1` or any uint8 counter.
4. Send timer icon OFF:

```text
01 6A A2 00 00
```

5. Send request full status:

```text
01 84 40 00
```

6. Wait for `A5-12` status reply and/or `A5-02` status.
7. Publish state to Home Assistant / MQTT / web UI.
8. Keep listening permanently because physical panel buttons are handled by MCU.

Mode switch recommendation:

```text
Send sync preamble:
  01 29 A1 00 01 00 00 00 00 00

Then send one of:
  Manual: 01 60 A2 00 00 01 xx
  Auto:   01 80 40 00 TT LL HH 09 05 01
  Sleep:  01 82 40 00 TT LL HH 09 05 01
```

If device is power OFF and a mode command is requested, replacement firmware may first send power ON, then the mode command.

## Command frame build example

Pseudo C/C++:

```cpp
uint8_t nextPacketId = 1;

void sendA5Command(const uint8_t* payload, uint16_t len) {
  uint8_t frame[64];
  uint8_t id = nextPacketId++;

  frame[0] = 0xA5;
  frame[1] = 0x22;
  frame[2] = id;
  frame[3] = len & 0xFF;
  frame[4] = len >> 8;
  frame[5] = 0x00;  // checksum placeholder

  memcpy(&frame[6], payload, len);

  uint16_t total = len + 6;
  uint16_t sum = 0;
  for (int i = 0; i < total; i++) {
    if (i != 5) sum += frame[i];
  }
  frame[5] = 0xFF - (sum & 0xFF);

  uart_write_bytes(UART_NUM_1, (const char*) frame, total);
}
```

Example payloads:

```cpp
const uint8_t powerOff[]        = {0x01,0x00,0xA0,0x00,0x00};
const uint8_t powerOn[]         = {0x01,0x00,0xA0,0x00,0x01};
const uint8_t displayOff[]      = {0x01,0x05,0xA1,0x00,0x00};
const uint8_t displayOn[]       = {0x01,0x05,0xA1,0x00,0x64};
const uint8_t timerIconOff[]    = {0x01,0x6A,0xA2,0x00,0x00};
const uint8_t requestStatus[]   = {0x01,0x84,0x40,0x00};
const uint8_t stopAtTargetOn[]  = {0x01,0xE5,0xA5,0x00,0x01};
const uint8_t stopAtTargetOff[] = {0x01,0xE5,0xA5,0x00,0x00};
```

## Remaining unknown / not fully closed

1. STATUS `p[0]..p[6]`
   - Mostly constant/service prefix.
   - Useful to log, not necessary for user-level control.

2. Stored Stop At Target setting
   - Command is confirmed.
   - `p[10]` is now interpreted as active target-stop/inhibit state, not stored setting.
   - The stored setting itself may not be directly exposed in the 20-byte STATUS packet.

3. STATUS `p[19]`
   - `0x00` = OK.
   - `0x01` = E1 observed; likely tank Hall sensor / tank-presence fault.
   - `0x02` = E2 candidate / unconfirmed.
   - Other values unknown.

4. Exact meaning of all `01 29 A1` variants
   - Mode-change sync is understood practically.
   - Startup variants are not fully understood.

5. Whether `01 29 A1` preamble is strictly required
   - Stock module sends it before mode changes.
   - Replacement firmware should copy stock behavior unless further testing proves it unnecessary.

## Short command summary

```text
Power:
  01 00 A0 00 00 = OFF
  01 00 A0 00 01 = ON

Night light:
  01 03 A0 00 01 xx = brightness xx, 0..100
  examples: 00 = OFF, 32 = LOW / 50%, 64 = HIGH / 100%

Display:
  01 05 A1 00 00 = OFF
  01 05 A1 00 64 = ON

Timer icon:
  01 6A A2 00 00 = OFF
  01 6A A2 00 01 = ON

Manual:
  01 29 A1 00 01 00 00 00 00 00 = mode sync/preamble
  01 60 A2 00 00 01 xx = manual level xx

Auto:
  01 29 A1 00 01 00 00 00 00 00 = mode sync/preamble
  01 80 40 00 TT LL HH 09 05 01 = auto target/band

Sleep:
  01 29 A1 00 01 00 00 00 00 00 = mode sync/preamble
  01 82 40 00 TT LL HH 09 05 01 = sleep target/band

Stop at target / Auto-off behavior command:
  01 E5 A5 00 00 = OFF, keep minimum/maintain
  01 E5 A5 00 01 = ON, stop output when target-stop condition is active

Request status:
  01 84 40 00 = request full status

Sync/internal:
  01 29 A1 ... = sync/mode preamble/internal state
```

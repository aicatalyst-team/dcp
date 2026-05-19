# DCP recipes

Ready-to-flash skeletons for the five protocol shapes you'll most
commonly want. Each one teaches a different slice of DCP and runs on
an ESP32-class MCU. Hardware is cheap enough that you can buy the
whole bench for ~¥40.

| Recipe | What you'll learn | Approx. hardware cost |
|---|---|---|
| [`relay_switch`](#relay_switch) | bool param · idempotent · capability · non-idempotent pulse | ¥5 (5V relay module) |
| [`sensor_dht22`](#sensor_dht22) | typed read intent with units · unsolicited event push · device-tracked config | ¥10 (DHT22) |
| [`stepper_motor`](#stepper_motor) | int + duration params · `dry_run` as preview · non-idempotent move | ¥10 (28BYJ-48 + ULN2003) |
| [`encoder_input`](#encoder_input) | **event-only device** — zero intents | ¥3 (KY-040) |
| [`door_lock`](#door_lock) | capability + `dry_run` as **real safety story** · string return types · state-change events | ¥10 (SG90 servo) |

Each recipe is a pair: a manifest (`examples/<name>_manifest.yaml`)
and a firmware sketch (`firmware/esp32/examples/<name>/<name>.ino`).
All five cross-compile clean on the full
[ESP family build matrix](../README.md#cross-compile-clean-across-the-esp-family-xtensa--risc-v--esp8266).

## Quick start (any recipe)

```bash
# 1. Wire your hardware per the comments at the top of the .ino
# 2. Flash:
arduino-cli compile --upload -p COM5 --fqbn esp32:esp32:esp32 \
    --library firmware/esp32 firmware/esp32/examples/relay
# 3. Run the bridge:
dcp serve examples/relay_manifest.yaml --serial COM5
# 4. Point your MCP client (Claude Desktop / Ollama / etc.) at dcp.
```

---

## relay_switch

A single-channel 5V relay (door buzzer, appliance switch, fan).

- 3 intents: `set_relay(state)`, `read_relay()`, `pulse(duration)`
- The `set_relay` intent is **idempotent** — setting "on" twice still
  leaves it on. `pulse` is **non-idempotent** (two calls = two pulses)
  and the manifest declares that explicitly.
- Both `set_relay` and `pulse` are gated on `relay.write`; `read_relay`
  only needs `relay.read`. A read-only session token cannot switch the
  appliance.

**Files**: [`examples/relay_manifest.yaml`](../examples/relay_manifest.yaml) ·
[`firmware/esp32/examples/relay/relay.ino`](../firmware/esp32/examples/relay/relay.ino)

---

## sensor_dht22

A DHT22 temperature + humidity sensor that PUSHES events when
temperature crosses a configurable threshold — no polling needed
from the LLM.

- 2 read intents (`read_temperature`, `read_humidity`) with explicit
  `unit: celsius` / `unit: percent` declared in the manifest, so the
  LLM tool schema surfaces them and stops asking "Fahrenheit?"
- 1 write intent (`set_alert_threshold`) that mutates device state
  used by the event loop
- 1 event (`threshold_exceeded`) emitted on rising edge of "above
  threshold," with payload `{temperature, threshold}`

The DHT22 driver call is stubbed (`read_dht_*()` returns mock data) so
the example runs headlessly during compile-test. Drop in your
favourite library (e.g. Adafruit DHT) at those two function bodies.

**Files**: [`examples/sensor_dht22_manifest.yaml`](../examples/sensor_dht22_manifest.yaml) ·
[`firmware/esp32/examples/sensor_dht22/sensor_dht22.ino`](../firmware/esp32/examples/sensor_dht22/sensor_dht22.ino)

---

## stepper_motor

A 28BYJ-48 stepper driven by a ULN2003 board — curtain, valve, dial,
small actuator, etc.

- 3 intents: `step(direction, count, speed_rpm)`, `read_position()`,
  `home()`
- This is where **dry_run gets genuinely useful**: an LLM can ask
  "what step position would this move end at?" and get
  `{would_move_to: 2700, from: 1234}` without spinning the shaft.
  Saves wear, time, and lets the LLM reason about reachability.
- `step` and `home` are **non-idempotent** (each call advances state).
  The manifest declares that explicitly so the LLM knows not to retry.
- Half-step coil sequence implemented inline; coils released after
  every move to avoid overheating.

**Files**: [`examples/stepper_manifest.yaml`](../examples/stepper_manifest.yaml) ·
[`firmware/esp32/examples/stepper/stepper.ino`](../firmware/esp32/examples/stepper/stepper.ino)

---

## encoder_input

A KY-040 rotary encoder with integrated push button.

This is the canonical **"the device has no intents"** pattern. The
LLM does not command the encoder; it gets notified when the user
turns or clicks. The `intents:` list in the manifest is literally
empty, the bindings array in the firmware is literally empty, and the
firmware only ever calls `send_event()`.

Useful for: volume knobs, menu wheels, RPM counters, manual override
inputs — anything where the LLM should react to user input but never
drive that input itself.

- 3 events: `encoder_turned(delta, position)`, `button_pressed`,
  `button_long_press(held_ms)`
- Polled in `loop()` with 5 ms debounce on CLK and 20 ms on the
  button; long-press fires after 1 s of continuous press.

**Files**: [`examples/encoder_manifest.yaml`](../examples/encoder_manifest.yaml) ·
[`firmware/esp32/examples/encoder/encoder.ino`](../firmware/esp32/examples/encoder/encoder.ino)

---

## door_lock

The capability + dry_run safety story made concrete: a servo (or
solenoid) actuated door lock.

- The capability name is **`lock.admin`**, not `lock.write` — the
  manifest itself signals "this is dangerous." A reviewer reading the
  manifest immediately sees that this intent is in a separate trust
  tier from "read the temperature."
- `dry_run` is the safety primitive in action: an LLM can be
  ALLOWED to call `unlock` with `__dry_run__=true` (cheap,
  reversible — returns "would transition locked → unlocked") but the
  REAL unlock requires the `lock.admin` capability, which the
  operator typically mints out of band with a short TTL:

  ```bash
  dcp token mint --caps lock.read,lock.admin --ttl 300
  ```

- The Bridge in the recommended `dcp serve --grant lock.read`
  configuration grants **only read** by default. Admin must be
  presented as a signed token per call.
- A `state_changed(from, to)` event fires on any transition,
  regardless of cause — so manual key turns are observable too.

**Files**: [`examples/door_lock_manifest.yaml`](../examples/door_lock_manifest.yaml) ·
[`firmware/esp32/examples/door_lock/door_lock.ino`](../firmware/esp32/examples/door_lock/door_lock.ino)

---

## Adding your own recipe

The protocol is the same regardless of device. Use one of the above
as your starting template, change the manifest to declare your
intents and events, and replace the firmware handlers with your
hardware-specific code. The full add-a-feature walkthrough is in
[`ADDING_FEATURES.md`](ADDING_FEATURES.md).

If you build a recipe for a piece of hardware that isn't covered
here (BME280, NeoPixel strip, ultrasonic sensor, GPS module,
PCA9685, ADS1115…), please open a PR — the cookbook grows by
contribution.

## Footprint stays flat

Adding a recipe doesn't grow the DCP layer. All five recipes above
land at the same ~290 KB / ~22.5 KB on ESP32-WROOM-32 — the
variation is in the example sketch's own logic, not in any per-recipe
protocol cost. The intent table is `switch(intent_id) → handler`, and
each intent adds one row and one function. There is no plugin loader,
no runtime registration, no per-handler dispatcher overhead.

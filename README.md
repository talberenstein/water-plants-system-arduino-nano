# sketch_sep15a — Riego Automático por Umbral (Arduino Nano)

Automatic threshold-based irrigation controller for an Arduino Nano. It reads a
soil moisture sensor and drives a relay-controlled water pump, with hysteresis
so the pump doesn't chatter on and off near the threshold, plus a manual
Serial override and a hard safety cutoff.

## What it does

- Every 500 ms, reads the soil moisture sensor on `A0`.
- Uses **two thresholds (hysteresis)** instead of one:
  - `UMBRAL_SECO = 650` — reading at or above this turns the pump **ON**.
  - `UMBRAL_HUMEDO = 400` — reading at or below this turns the pump **OFF**.
  - Between 400 and 650 is a dead zone: the pump just keeps doing whatever it
    was already doing. This prevents rapid on/off flicker when the reading
    hovers near a single threshold.
- The pump is a relay on pin `7` (`HIGH` = on, `LOW` = off).
- **Safety cutoff:** the pump is forcibly turned off after `MAX_RIEGO_MS`
  (15 seconds) of continuous run time, regardless of sensor state, in case the
  sensor is disconnected/misreading or stuck logic would otherwise run it
  forever.
- **Manual override via Serial** (9600 baud): send `ON` or `OFF` (case
  insensitive) to force the pump. The automatic logic can still turn it back
  off (if it becomes wet) or back on (if it's still dry on the next reading
  cycle) — this is intentional, automatic mode always has the final say.
- Status lines are printed to Serial every reading cycle:
  `Humedad (raw): <value>  | Bomba: ON/OFF`.

## Hardware (current)

| Component | Arduino Nano pin | Notes |
|---|---|---|
| Soil moisture sensor (analog out) | `A0` | Raw reading 0–1023. In the Wokwi simulation this is a potentiometer standing in for the real sensor. |
| Relay module (pump) | `D7` | `HIGH` energizes the relay in this wiring. |

⚠️ **`UMBRAL_SECO` / `UMBRAL_HUMEDO` are placeholder values** tuned for the
Wokwi potentiometer's full 0–1023 sweep. With a real capacitive/resistive soil
sensor you must recalibrate: dip it in dry substrate to find the "dry" raw
value, and in freshly-watered substrate to find the "wet" raw value, then
replace these two constants.

## Serial commands

| Command | Effect |
|---|---|
| `ON` | Force the pump on (no-op if already on) |
| `OFF` | Force the pump off (no-op if already off) |

## Planned hardware additions

A physical **DHT22 ambient humidity/temperature sensor** and **3 small DC
motors used as independent liquid dosing pumps** (driven through H-bridge
motor driver modules) are planned next. See
[`specs/001-humidity-sensor-and-dosing-motors/`](specs/001-humidity-sensor-and-dosing-motors/)
for the spec-driven plan (`spec.md`, `plan.md`, `tasks.md`) before any of that
code is written.

## File structure

```
sketch_sep15a.ino    — the sketch (single file, no external libraries yet)
specs/                — spec-driven-development docs for upcoming features
  001-humidity-sensor-and-dosing-motors/
    spec.md            — what & why (requirements)
    plan.md             — how (architecture, pin map, BOM)
    tasks.md             — ordered implementation checklist
```

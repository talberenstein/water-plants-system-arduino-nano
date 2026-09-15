# Plan: Ambient Humidity Sensor + 3 Dosing Pumps

Implements the requirements in [`spec.md`](spec.md). This is the *how*:
hardware, pin map, architecture, and validation approach.

## 1. Bill of materials (new)

| Qty | Part | Purpose |
|---|---|---|
| 1 | DHT22 / AM2302 module (with pull-up, 3-pin breakout) | Ambient temp + RH |
| 2 | Dual H-bridge motor driver module (L298N, L293D, or TB6612FNG) | 4 channels total; 3 used for dosing pumps, 1 spare |
| 3 | Small DC dosing pumps (peristaltic or diaphragm, 5–12 V per pump spec) | Liquid dosing |
| 1 | External power supply sized for pump stall current × 3 (+ margin) | Motors MUST NOT be powered from the Nano's 5 V/USB rail |
| — | Common ground wire between Nano, driver boards, and pump PSU | Required — a shared ground reference between logic and motor power is mandatory for the driver's logic-level inputs to work |
| 3+ | Flyback/freewheeling diodes | Only if not already integrated on the chosen driver board (most L298N/L293D breakouts already include them) |

**Power warning:** the water pump relay already isolates the Nano from pump
current. Dosing pump motors must be driven the same way — never tie a
motor's power directly to the Nano's 5V pin. Confirm current draw of the
chosen pumps against the driver module's continuous-current rating before
wiring.

## 2. Pin map (Arduino Nano)

| Signal | Pin | Type | Notes |
|---|---|---|---|
| Relay (water pump) | `D7` | digital out | existing, unchanged |
| Soil moisture sensor | `A0` | analog in | existing, unchanged |
| DHT22 data | `D2` | digital, 1-wire | needs `#include <DHT.h>` (Adafruit DHT sensor library + Adafruit Unified Sensor dependency) |
| Dosing pump 1 enable | `D9` (PWM-capable) | digital/PWM out | driver channel A, board 1 |
| Dosing pump 2 enable | `D10` (PWM-capable) | digital/PWM out | driver channel B, board 1 |
| Dosing pump 3 enable | `D11` (PWM-capable) | digital/PWM out | driver channel A, board 2 |

**Design decision — direction pins hardwired, not MCU-driven:** dosing pumps
only ever need to push liquid one direction. To keep the pin count down
(3 new digital pins total instead of 3 enable + 6 direction pins), each
driver channel's `IN1`/`IN2` (or equivalent) pair is hardwired directly to
`+5V`/`GND` (fixed polarity) rather than routed to the Nano, and only the
channel's `ENA`/`ENB` PWM-enable pin is driven by the Nano. This is
sufficient for on/off + optional speed/flow control via PWM duty cycle. If a
future spec needs reversible motors, this decision must be revisited.

Pins `D9`–`D11` are chosen because they're PWM-capable, leaving `D3`, `D5`,
`D6` free for future expansion (e.g. the spare driver channel, or a 4th
motor).

## 3. Software architecture

### 3.1 New library dependency

- `DHT sensor library` (Adafruit) + its dependency `Adafruit Unified Sensor`,
  installed via Arduino Library Manager. This is the only new dependency;
  motor driver channels are plain `digitalWrite`/`analogWrite`, no library
  needed.

### 3.2 New constants

```cpp
// --- DHT22 ambiente ---
#define DHT_PIN 2
#define DHT_TIPO DHT22
const unsigned long INTERVALO_DHT_MS = 2000UL; // minimo del DHT22

// --- Bombas de dosificacion ---
const int PIN_DOSIS[3] = {9, 10, 11};
const unsigned long MAX_DOSIS_MS = 10000UL; // corte de seguridad por bomba
```

### 3.3 New state

```cpp
DHT dht(DHT_PIN, DHT_TIPO);
unsigned long tUltimaLecturaDHT = 0;

struct BombaDosis {
  bool encendida = false;
  unsigned long tInicio = 0;
  unsigned long duracionMs = 0;
};
BombaDosis dosis[3];
```

This mirrors the existing `bombaEncendida` / `tInicioRiego` pattern but keyed
per-pump in an array/struct, per FR-9 (fully independent state — no shared
variables with the irrigation logic).

### 3.4 New functions

- `void encenderDosis(int idx, unsigned long duracionMs, const char* motivo)`
  — clamps `duracionMs` to `MAX_DOSIS_MS` (FR-7), sets `analogWrite`/
  `digitalWrite` HIGH on `PIN_DOSIS[idx]`, records `tInicio`/`duracionMs`,
  logs to Serial including the clamped value if it was clamped.
- `void apagarDosis(int idx, const char* motivo)` — mirrors `apagarBomba`.
- `void leerDHT()` — called on its own interval (`INTERVALO_DHT_MS`), reads
  temp/humidity, checks `isnan()` on both, prints either the reading or a
  `>> DHT22 read error` line (FR-3), never touches irrigation state.
- Serial command parsing extended to recognize `DOSE1`, `DOSE2`, `DOSE3`
  prefixes (case-insensitive per FR-10) followed by either a number
  (milliseconds) or the literal `OFF`, alongside the existing `ON`/`OFF`
  parsing for the water pump. Keep the existing `cmd == "ON"` / `"OFF"`
  branches untouched; add new branches, don't restructure them.

### 3.5 `loop()` integration

Add, without touching the existing irrigation block:

1. Extend the Serial-command block to also match `DOSE<n> ...` commands.
2. A new "safety cutoff" block, parallel to the existing
   `MAX_RIEGO_MS` check, iterating `dosis[0..2]` and calling `apagarDosis`
   for any pump whose `millis() - tInicio >= duracionMs` (this doubles as
   both "timed dose completed" and "safety cutoff" — a dose's own requested
   duration *is* its cutoff, already clamped to `MAX_DOSIS_MS` at start
   time, satisfying FR-7/FR-9 without a second timer).
3. A new interval-gated block calling `leerDHT()` on `INTERVALO_DHT_MS`,
   parallel to (not replacing) the existing `INTERVALO_LECTURA_MS`
   soil-moisture block. Keep them as two separate `if` blocks with their
   own `tUltimaLectura*` variables — do not merge intervals, since DHT22
   (2000 ms) and the soil sensor (500 ms) have different minimum sampling
   rates (FR-1).

### 3.6 Failure isolation (FR-3, G4)

- DHT read failures only affect the DHT-reporting block; they must not
  `return` out of `loop()` or otherwise skip the irrigation/dosing blocks
  below them.
- Dosing pump logic must not read `A0` or touch `bombaEncendida` /
  `tInicioRiego` at all — verified by code review before merge (no shared
  identifiers between the two subsystems apart from `millis()`).

## 4. Current/power budget check (SC-5)

Before final wiring, sum the stall current of all 3 dosing pumps + the
water pump relay's coil current, and confirm the chosen external PSU and
each driver module's continuous-current rating covers running all 4 motors
concurrently (worst case: irrigation pump ON at the same moment all 3
dosing pumps are commanded). If the PSU can't cover worst-case concurrent
draw, either upsize the PSU or add a software interlock limiting how many
motors run at once — treat that as a follow-up spec if it comes up, it is
out of scope for v1 per the non-goals.

## 5. Testing / validation plan

1. **Simulation first (Wokwi):** add a virtual DHT22 and 3 virtual DC
   motors (or LEDs standing in for them, as the existing potentiometer
   stands in for the soil sensor) to validate logic/timing before touching
   real hardware.
2. **Serial command tests:** exercise `DOSE1 500`, `DOSE2 15000` (expect
   clamp to `MAX_DOSIS_MS` and a log line saying so), `DOSE3 OFF`
   mid-run, and confirm existing `ON`/`OFF` still work unchanged.
3. **Fault injection:** physically unplug the DHT22 data line mid-run;
   confirm Serial shows the error line and the water pump / dosing pumps
   are unaffected (SC-2).
4. **Physical flow calibration (per pump, per liquid):** run each pump for
   a fixed duration (e.g. 5000 ms) into a graduated container, measure mL
   dispensed, compute mL/ms. Record these three calibration constants in
   code comments (mirroring the existing `UMBRAL_SECO`/`UMBRAL_HUMEDO`
   calibration-values-are-yours-to-set pattern) — this is what lets the
   grower convert "I want 10 mL of nutrient B" into a millisecond duration
   for the `DOSE2 <ms>` command.
5. **Concurrent load test:** trigger the water pump and all 3 dosing pumps
   at once, confirm no brownout/reset of the Nano (SC-5).

## 6. Rollout steps

1. Wire DHT22 to `D2` + 5V/GND; verify readings via a minimal test sketch
   before merging into the main sketch.
2. Wire one driver module (2 channels) for dosing pumps 1–2, verify each
   independently with a test sketch (`analogWrite` a channel, confirm
   motor spins, confirm it stops).
3. Wire the second driver module (1 of 2 channels used) for dosing pump 3.
4. Merge DHT + dosing pump code into `sketch_sep15a.ino` per §3.
5. Run the full testing/validation plan in §5 before relying on this for
   unattended operation.
6. Update `README.md`'s hardware table and pin map once wiring is final.

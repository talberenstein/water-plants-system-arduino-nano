# Spec: Ambient Humidity Sensor + 3 Dosing Pumps

**Status:** Draft — not yet implemented
**Feature branch/dir:** `001-humidity-sensor-and-dosing-motors`
**Depends on:** current `sketch_sep15a.ino` (soil-moisture-triggered irrigation
pump on relay pin 7)

## 1. Problem statement

The current sketch only measures soil moisture and controls one water pump.
Two capabilities are missing:

1. There is no visibility into **ambient air conditions** (temperature /
   relative humidity) around the plant, which matters for diagnosing why
   soil dries out fast (heat) or stays wet (poor air circulation).
2. There is no way to **dose liquids** (e.g. nutrient solutions, pH
   adjusters) automatically or on demand — only water via the single
   irrigation pump.

This feature adds one **DHT22** ambient temperature/humidity sensor and
**3 small DC motors used as independent dosing pumps**, driven through
H-bridge motor driver modules.

## 2. Goals

- G1: Read and report ambient temperature and relative humidity on a fixed
  interval, alongside the existing soil-moisture reading.
- G2: Control 3 dosing pumps independently, each capable of running for a
  caller-specified duration (a manual "dose N for M ms" command).
- G3: Each dosing pump has its own hard safety cutoff (max continuous run
  time), mirroring the existing `MAX_RIEGO_MS` pattern for the water pump.
- G4: None of the above may destabilize or block the existing irrigation
  logic — irrigation must keep working exactly as it does today even if the
  DHT sensor fails or a dosing pump is mid-run.

## 3. Non-goals (out of scope for v1)

- Automatic dosing triggered by nutrient concentration / pH / EC feedback —
  no such sensor exists in this project. (See Open Question OQ-1.)
- Reversing dosing pump direction (they only need to push liquid one way).
- A `STATUS` query command, logging to SD card, or a UI beyond Serial text.
- Flow-rate accuracy better than "time-based dosing calibrated by the
  user" (i.e. no flow meter/feedback loop).

## 4. User scenarios

- **US1 — Passive monitoring:** As the grower, I want the Serial monitor to
  also show ambient temperature and humidity every cycle, so I can correlate
  soil drying speed with air conditions.
- **US2 — Manual dosing:** As the grower, I want to send a Serial command to
  run a specific dosing pump for a specific duration (e.g. "give pump 2 a
  3-second dose of nutrient B"), so I can dose nutrients on demand during
  setup/testing without rewiring anything.
- **US3 — Scheduled dosing (optional, see OQ-1):** As the grower, I want to
  optionally configure each dosing pump to run automatically every N hours
  for M ms, so routine nutrient dosing doesn't require me to be present.
- **US4 — Fail-safe:** As the grower, I want a dosing pump that's
  accidentally left/commanded on to shut itself off after a bounded max
  time, so a mistake (or a bug) can't drain an entire nutrient reservoir or
  flood the mix tank.
- **US5 — Sensor fault tolerance:** As the grower, if the DHT22 is
  unplugged or gives a bad reading, I want the sketch to print a clear error
  and keep running (irrigation + dosing must be unaffected).

## 5. Functional requirements

Numbered so `tasks.md` can reference them (`FR-1`, `FR-2`, …).

- **FR-1:** The system MUST read temperature and relative humidity from a
  DHT22 sensor no more often than once every 2000 ms (DHT22 datasheet
  minimum sampling interval).
- **FR-2:** The system MUST print the DHT22 reading (temp °C, RH %) to
  Serial on the same cadence it currently prints the soil-moisture line.
- **FR-3:** If a DHT22 read fails (library returns `NaN`), the system MUST
  print a distinct error message (e.g. `>> DHT22 read error`) and MUST NOT
  use the stale/invalid value for any decision, and MUST NOT stop or delay
  the soil-moisture/irrigation loop.
- **FR-4:** The system MUST support 3 independently addressable dosing
  pumps (index 1–3), each mapped to its own driver-enable pin.
- **FR-5:** The system MUST accept a Serial command of the form
  `DOSE<n> <ms>` (e.g. `DOSE1 3000`) that runs dosing pump `n` for `ms`
  milliseconds, then automatically turns it off.
- **FR-6:** The system MUST accept `DOSE<n> OFF` to immediately stop dosing
  pump `n` before its timed run completes.
- **FR-7:** Each dosing pump MUST have its own configurable maximum run
  time (`MAX_DOSIS_MS`, default 10000 ms.) A `DOSE<n> <ms>` request longer
  than the max MUST be clamped to the max (not rejected silently — print
  what was actually applied).
- **FR-8:** Starting a dose on pump `n` that is already running MUST
  restart its timer with the new duration (last command wins), not stack
  durations.
- **FR-9:** Dosing pump state/timers MUST be fully independent per pump and
  independent of the existing water-pump (`bombaEncendida`) state — no
  shared variables, no shared safety-cutoff timer.
- **FR-10:** All new Serial commands MUST be case-insensitive and MUST NOT
  break the existing `ON` / `OFF` commands for the water pump.

### Optional / conditional on OQ-1 resolution

- **FR-11 (optional):** If scheduled dosing is enabled for a pump, the
  system MUST automatically start that pump's configured dose every
  configured interval, using the same per-pump max-run safety cutoff as
  manual doses.

## 6. Key entities / state

- `DHT22` ambient sensor → `temperaturaAire` (°C, float), `humedadAire` (%RH,
  float), `tUltimaLecturaDHT` (timestamp).
- `DosingPump[3]` → `encendida` (bool), `tInicio` (timestamp),
  `duracionSolicitadaMs` (unsigned long, clamped to `MAX_DOSIS_MS`), driver
  enable pin.

## 7. Success criteria

- SC-1: Serial output shows a valid temp/RH line at least once every 2–3 s
  during normal operation with the DHT22 connected.
- SC-2: Unplugging the DHT22 mid-run produces error lines but soil-moisture
  readings and pump ON/OFF logic continue unaffected (verified in a test
  run of ≥ 60 s).
- SC-3: `DOSE1 500`, `DOSE2 500`, `DOSE3 500` each independently start and
  stop their own motor at the right time, verified by Serial log
  timestamps within ±50 ms of requested duration.
- SC-4: A dose requested for longer than `MAX_DOSIS_MS` is capped, and the
  pump shuts off at the cap even if no `OFF` command is sent.
- SC-5: Running all 3 dosing pumps and the water pump simultaneously does
  not brown out the Arduino Nano (verified with the final power-supply
  design from `plan.md`).

## 8. Open questions / assumptions

- **OQ-1 (assumption made):** No EC/pH/flow sensor exists in this project,
  so v1 dosing is **manual-command + optional fixed time-of-interval
  schedule**, not feedback-controlled. If real nutrient-concentration
  feedback is wanted later, that's a separate future spec (needs an EC or
  pH probe).
- **OQ-2:** Exact dosing pump liquids/purpose (which pump does what) is up
  to the grower's setup; the spec treats all 3 pumps symmetrically/generic.
- **OQ-3:** Real-world mL-per-second flow rate per pump is a physical
  calibration the grower must do after wiring (see `plan.md` §6); the spec
  only guarantees time-accuracy of the run, not volume accuracy.

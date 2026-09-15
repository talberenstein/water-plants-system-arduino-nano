# Tasks: Ambient Humidity Sensor + 3 Dosing Pumps

Ordered checklist. Each task references the `FR-*`/`SC-*` it satisfies from
[`spec.md`](spec.md); implementation details are in [`plan.md`](plan.md).
Check items off as completed; don't reorder phases — later phases assume
earlier ones are wired/working.

## Phase 0 — Hardware wiring

- [ ] T0.1 Wire DHT22 data → `D2`, VCC → 5V, GND → GND (add pull-up per
      module datasheet if the breakout doesn't include one).
- [ ] T0.2 Wire driver module 1 (channels for dosing pumps 1 & 2):
      `ENA`→`D9`, `ENB`→`D10`, `IN1..IN4` hardwired per `plan.md` §2,
      motor power from external PSU, **grounds tied together** (Nano GND ↔
      driver GND ↔ PSU GND).
- [ ] T0.3 Wire driver module 2 (channel for dosing pump 3): `ENA`→`D11`,
      same hardwiring/grounding pattern.
- [ ] T0.4 Verify current/power budget per `plan.md` §4 before powering
      anything on.

## Phase 1 — DHT22 integration

- [ ] T1.1 Install `DHT sensor library` + `Adafruit Unified Sensor` via
      Library Manager.
- [ ] T1.2 Flash a minimal standalone test sketch reading the DHT22 and
      printing temp/RH to Serial; confirm sane values before touching the
      main sketch. *(Not merged — throwaway verification.)*
- [ ] T1.3 Add DHT constants/state/`leerDHT()` to `sketch_sep15a.ino` per
      `plan.md` §3.2–3.4. Satisfies FR-1, FR-2.
- [ ] T1.4 Add `isnan()` guard + error line in `leerDHT()`. Satisfies FR-3.
- [ ] T1.5 Wire `leerDHT()` into `loop()` on its own `INTERVALO_DHT_MS`
      gate, as a block separate from the existing soil-moisture block.
      Satisfies FR-1 (2000 ms minimum), G4 (no interference).

## Phase 2 — Dosing pump control

- [ ] T2.1 Add `PIN_DOSIS[3]`, `MAX_DOSIS_MS`, `BombaDosis dosis[3]` per
      `plan.md` §3.2–3.3.
- [ ] T2.2 Implement `encenderDosis(idx, duracionMs, motivo)` with clamping
      to `MAX_DOSIS_MS` and a Serial log line noting if clamped. Satisfies
      FR-4, FR-7.
- [ ] T2.3 Implement `apagarDosis(idx, motivo)`. Satisfies FR-4.
- [ ] T2.4 Implement the restart-on-repeat-command behavior (starting a
      dose on an already-running pump resets its timer/duration rather
      than stacking). Satisfies FR-8.
- [ ] T2.5 Add the per-pump elapsed-time check in `loop()` that
      auto-turns-off a pump when `millis() - tInicio >= duracionMs`.
      Satisfies FR-7 (acts as the safety cutoff), FR-9 (fully independent
      of irrigation timers — confirm no shared variables with
      `bombaEncendida`/`tInicioRiego`).

## Phase 3 — Serial command extension

- [ ] T3.1 Extend the Serial command parser to recognize `DOSE1`, `DOSE2`,
      `DOSE3` prefixes (case-insensitive, reuse the existing
      `cmd.toUpperCase()`), each followed by either an integer
      (milliseconds) or the literal `OFF`. Satisfies FR-5, FR-6, FR-10.
- [ ] T3.2 Confirm the existing `ON`/`OFF` branches for the water pump are
      untouched and still work after the parser extension (regression
      check for FR-10).

## Phase 4 — Testing & validation

- [ ] T4.1 Wokwi simulation pass with virtual DHT22 + motor stand-ins,
      exercising all Serial commands (`plan.md` §5.1–5.2).
- [ ] T4.2 Fault-injection test: disconnect DHT22 mid-run, confirm
      irrigation + dosing pumps unaffected and error line appears.
      Satisfies SC-2.
- [ ] T4.3 Per-pump timing accuracy test: `DOSE<n> 500` for each pump,
      confirm actual on-duration within ±50 ms via Serial timestamps.
      Satisfies SC-3.
- [ ] T4.4 Clamp test: `DOSE<n> <value greater than MAX_DOSIS_MS>`,
      confirm it's capped and logged, and the pump shuts off at the cap
      without an explicit `OFF`. Satisfies SC-4.
- [ ] T4.5 Concurrent load test: trigger water pump + all 3 dosing pumps
      simultaneously, confirm no Nano brownout/reset. Satisfies SC-5.
- [ ] T4.6 Physical flow calibration per pump/liquid (measure mL per fixed
      run duration); record resulting mL/ms constants as code comments,
      same spirit as the existing `UMBRAL_SECO`/`UMBRAL_HUMEDO` comment.

## Phase 5 — Documentation

- [ ] T5.1 Update `README.md`'s hardware table and pin map to match the
      final as-wired configuration (pins may differ from the plan if
      wiring constraints forced changes — document what was *actually*
      built).
- [ ] T5.2 Add the calibration constants/notes from T4.6 to the sketch's
      top-of-file comment block, matching the existing calibration-warning
      style already in `sketch_sep15a.ino`.

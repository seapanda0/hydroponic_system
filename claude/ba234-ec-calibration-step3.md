# EC Sensor — Single-Point Software Calibration Algorithm

## Goal

Calibrate raw EC readings from the sensor against a **1413 µS/cm KCl reference
standard**, targeting **±0.1 mS/cm (±100 µS/cm)** accuracy around the hydroponic
operating range (~0.5–3 mS/cm). Assumes a function `read_ec()` already exists
that returns one raw EC reading in µS/cm, and `read_temp()` that returns
solution temperature in °C.

## Calibration Routine

### 1. Temperature gate

Read temperature. If outside **24.0–26.0 °C**, warn the operator and wait or
abort (configurable). Reference standards are rated at 25 °C; calibrating there
removes any mismatch between the sensor's temperature-compensation curve and KCl.

### 2. Stabilization + sampling

- Discard the first **5 readings** (probe settling)
- Collect **N = 25 readings** at 2 s intervals (~50 s total)
- Compute mean and standard deviation of the raw EC values

### 3. Quality checks (abort if any fail)

- Relative std dev (σ/mean) must be **< 2%** — otherwise readings are unstable
  (air bubbles, drift, poor electrode contact)
- Mean raw EC must be within **±30% of 1413 µS/cm** — otherwise something is
  grossly wrong (wrong solution, dirty/damaged probe, missing zero-cal)
- Optional robustness: drop readings > 3σ from the median, or use the trimmed
  mean of the middle 80% instead of a plain mean

### 4. Compute and persist the correction factor

```
k = 1413.0 / EC_raw_mean
```

- Sanity-bound `k` to **0.7–1.3**; refuse to store values outside that range
- Store in non-volatile memory (EEPROM/flash):
  - `k`
  - calibration timestamp (or boot count if no RTC)
  - raw mean and std dev at calibration (for later drift diagnostics)
  - struct version byte + CRC over the stored record
- On boot: load and CRC-validate the record; if invalid or missing, fall back
  to `k = 1.0` and flag an "uncalibrated" state

### 5. Apply correction at runtime

All reported EC values:

```
EC_true = k * EC_raw        // µS/cm
```

### 6. Operator feedback

- On success: report raw mean, σ, computed `k`, and the corrected reading
- On failure: report which quality check failed and the suggested remedy

## Nice-to-haves

- **Two-point mode**: accept a second standard (e.g. 84 or 2764 µS/cm) and fit
  `EC_true = a * EC_raw + b`. Store `(a, b)`; single-point mode is then `(k, 0)`.
- **Drift warning**: at recalibration, if the previously-corrected reading in
  standard solution differs from 1413 by > 5%, log it — indicates electrode
  fouling.

## Out of Scope

- Sensor communication / frame parsing (provided by `read_ec()` / `read_temp()`)
- The chip's built-in zero-cal and salinity-cal commands
- Dosing control logic

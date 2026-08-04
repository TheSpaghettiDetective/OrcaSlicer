# Reduce stringing

Fix fine strands of plastic between separated parts of a print, caused by
filament oozing during travel moves.

## Check first

1. Read the current values before changing anything:
   `settings_get_values` for `nozzle_temperature`, `retraction_length`,
   `retraction_speed`, `z_hop`, `wipe`, `travel_speed`.
2. Wet filament strings regardless of settings. If the filament is PETG, TPU,
   or nylon and has been out of sealed storage for days, recommend drying
   before spending time on settings.
3. If retraction has never been calibrated for this printer/filament, prefer
   `calibration_generate type=retraction` over guessing.

## Changes, in order of likely payoff

1. **Lower `nozzle_temperature`** in 5 °C steps, staying inside the
   filament's rated range. Hotter plastic oozes more. Most impactful single
   change for PETG.
2. **Increase `retraction_length`.** Direct-drive extruders: typical
   0.5–2 mm. Bowden: 3–6 mm. Increase in 0.5 mm steps; too much causes
   clogs/grinding, so do not exceed roughly double the current value without
   a calibration print.
3. **Increase `retraction_speed`** toward 40–60 mm/s if currently below.
4. **Raise `travel_speed`** so the nozzle spends less time over gaps
   (150+ mm/s where the printer allows).
5. **Enable `wipe`** so the nozzle wipes on the perimeter before traveling.
6. **Reduce `z_hop`** to 0 if enabled and stringing is the complaint — z-hop
   trades stringing for collision safety; only keep it if the print needs it.
7. **Enable `retract_when_changing_layer`** and, if infill travel is the
   source, disable `reduce_infill_retraction` so infill moves also retract.

Apply one or two changes per iteration with `settings_set_values`, re-slice,
and if the user can test-print, judge on a small two-column tower rather than
the full model.

## Tradeoffs

- Lower temperature can weaken layer bonding — do not combine with a
  strength requirement without checking `guides_get id=increase-strength`.
- More retraction slows the print slightly and risks clogs on soft filaments
  (TPU wants *less* retraction, not more).

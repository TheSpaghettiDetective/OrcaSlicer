# Tuning guides

Guides are the server's domain knowledge: markdown documents retrieved via
`guides_search` / `guides_get`. They replace what a naive design would expose
as `workflow_*` tools (`workflow_optimize_for_strength`,
`workflow_tune_petg_stringing`, ...).

Why documents instead of tools:

- The right changes depend on geometry, printer, filament, and what the user
  actually wants. A fixed server-side bundle of setting changes is opaque and
  often wrong. A guide lets the model reason with the knowledge and apply
  only what fits, via `settings_set_values`.
- Guides are reviewable and editable by maintainers without touching server
  code, and the model's resulting changes are visible in tool calls instead
  of hidden inside a workflow implementation.

The exception is `calibration_generate`: generating a calibration print is a
deterministic OrcaSlicer feature, so it is a tool. Each calibration guide
tells the model how to interpret the printed result and which key to update.

## Format rules

- Filename is the guide id: `reduce-stringing.md` → id `reduce-stringing`.
- Start with a one-line summary (used as the `guides_search` result line).
- Reference settings by exact registry key in backticks so the model can go
  straight to `settings_get_values` / `settings_set_values`.
- Give ranges and tradeoffs, not single magic numbers.
- Order steps by likelihood of fixing the problem, cheapest first.
- Keep each guide under ~150 lines; split rather than grow.

See [reduce-stringing.md](reduce-stringing.md) for the reference example.

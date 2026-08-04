# Settings registry

The registry is the server-side index behind `settings_search`,
`settings_get_schema`, and validation in `settings_set_values`. It is **not**
shipped in tool descriptions — the model retrieves only the records it needs.

## Source of truth

Generated at build time from the option definitions in
`src/libslic3r/PrintConfig.cpp` (each `def()` already carries label, tooltip,
type, unit, min/max, enum values, and mode). Do not hand-maintain a parallel
list; hand-maintained fields (tags, related keys, dependency rules) live in a
small overlay file merged with the generated data.

## Record format

```json
{
  "key": "support_top_z_distance",
  "label": "Top Z distance",
  "scope": "process",
  "type": "float",
  "unit": "mm",
  "default": 0.2,
  "min": 0,
  "max": 1,
  "enum": null,
  "overridable_at": ["project", "plate", "object"],
  "description": "Gap between the top of supports and the object surface. Larger = easier removal, worse overhang surface.",
  "tags": ["support", "surface-quality", "removal"],
  "related": ["support_bottom_z_distance", "support_interface_spacing"],
  "dependencies": [
    { "rule": "requires", "key": "enable_support", "value": true, "note": "Has no effect unless supports are enabled." }
  ]
}
```

Field notes:

- `scope` — which preset type owns the key: `process`, `filament`, `printer`.
- `overridable_at` — layers where `settings_set_values` may write it. Printer
  keys are typically not overridable per-object.
- `enum` — for enum types, `[{value, label}]`; `min`/`max` null.
- `description` — one or two sentences, tooltip-derived, states the tradeoff.
- `tags` + `label` + `description` feed the `settings_search` index (keyword
  plus embedding search; exact-key and substring matches rank first).
- `dependencies` — machine-checkable rules used for `SETTING_CONFLICT` errors
  and `side_effects` reporting. Kinds: `requires` (no effect without X),
  `forces` (setting this changes X), `excludes` (cannot combine with X).

## Value resolution (layering)

Effective value for a key at a given object:

```
preset (printer/filament/process, from disk)
  ← project override        (settings_set_values scope=project)
    ← plate override        (scope=plate)
      ← object modifier     (scope=object)
```

The most specific layer wins. `settings_get_values` reports both the effective
value and the winning layer as `source`; when `source` is `preset` it also
names the preset. `settings_set_values` never writes presets on disk — the
narrowest persistent thing it touches is the session project, which the user
can save or discard.

## Session dirty-state

The server tracks a settings generation counter. `slice_run` records the
counter; `slice_analyze` and `gcode_export` compare against it and fail with
`SLICE_STALE` when settings changed after the last slice. This prevents the
agent from reporting numbers or exporting G-code that doesn't reflect its own
edits.

In the future interactive backend the same counter invalidates on user edits
in the GUI, so the agent's stale reads fail loudly instead of silently
diverging from what the user sees.

# Error conventions

Every tool error is a JSON object with three fields. The `suggestion` field is
mandatory because with hundreds of interdependent settings, error messages are
the main way the server teaches the model to make the next call correctly.

```json
{
  "code": "UNKNOWN_KEY",
  "message": "No setting named 'support_z_distance'.",
  "suggestion": "Did you mean one of: support_top_z_distance, support_bottom_z_distance? Use settings_search to browse further."
}
```

## Rules

1. **Unknown key → nearest matches.** Never just "not found". Return up to 5
   closest registry keys and point to `settings_search`.
2. **Invalid value → the valid space.** Include the type, unit, and min/max or
   the full enum list with labels. Example: `"sparse_infill_pattern must be
   one of: grid, gyroid, honeycomb, crosshatch, ... (got 'diagonal')"`.
3. **Conflict → what and why.** When a combination is rejected or would be
   force-changed, name both settings and the rule. Example: `"spiral_mode
   requires wall_loops=1 and top_shell_layers=0; pass dry_run=true to preview
   the forced changes, or set them explicitly."`
4. **Wrong layer → where it lives.** Setting a printer-scope key with
   scope='object' fails with which scopes are legal for that key.
5. **State errors → the missing step.** `NOT_SLICED` and `SLICE_STALE` name
   the tool to call (`slice_run`). `NO_PROJECT` names `project_open`.
6. **Transactional failures report everything at once.** If 3 of 8 keys in a
   `settings_set_values` call are bad, return all 3 errors in one response so
   the model fixes the batch in one retry, not three.

## Error codes

| Code | Meaning | Typical suggestion content |
|---|---|---|
| `NO_PROJECT` | No project open in this session | Call `project_open` |
| `FILE_NOT_FOUND` | Path in `project_open`/`gcode_export` invalid | Check path; server's working dir |
| `UNKNOWN_KEY` | Setting key not in registry | Nearest matches + `settings_search` |
| `INVALID_VALUE` | Value fails type/range/enum check | Valid range or enum list |
| `SETTING_CONFLICT` | Combination rejected or would force changes | Conflicting keys + rule + `dry_run` hint |
| `INVALID_SCOPE` | Key cannot be overridden at requested layer | Legal scopes for the key |
| `UNKNOWN_TARGET` | `plate_id`/`object_id` doesn't exist | Call `project_inspect` |
| `NOT_SLICED` | Analyze/export before any slice | Call `slice_run` |
| `SLICE_STALE` | Settings changed since last slice | Call `slice_run` again |
| `SLICE_FAILED` | Slicing aborted | Slicer error text + `slice_analyze detail=warnings` if partial |
| `UNKNOWN_GUIDE` | Guide id not found | Nearest ids + `guides_search` |
| `BACKEND_UNSUPPORTED` | Tool not available in this backend mode | Reported by `diagnostics_get` |
| `INTERNAL` | Unexpected slicer/server failure | `diagnostics_get log_lines=100` |

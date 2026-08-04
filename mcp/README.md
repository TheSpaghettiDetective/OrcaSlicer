# OrcaSlicer MCP Interface (design)

This folder is the interface design for an OrcaSlicer MCP server: the tool
definitions an MCP client would see from `tools/list`, plus the conventions the
server must follow. It is a specification, not an implementation.

## Design principles

1. **Few generic tools over a searchable settings registry.** OrcaSlicer has
   hundreds of settings. They are exposed as *data* behind four tools
   (`settings_search`, `settings_get_schema`, `settings_get_values`,
   `settings_set_values`), never as one tool per setting. The registry format
   is in [docs/settings-registry.md](docs/settings-registry.md).

2. **Data-returning tools, not MCP resources.** Most MCP clients treat
   resources as user-attached context, not something the model fetches on its
   own. Everything the model needs on demand (settings index, slice report,
   tuning guides) is behind a tool that returns data.

3. **Tuning knowledge is documentation, not tools.** There is no
   `workflow_optimize_for_strength` tool. Advice like "make this stronger" or
   "reduce stringing" depends on geometry, printer, and filament; baking it
   into server code makes it opaque and often wrong. Instead, guides are
   markdown the model retrieves with `guides_search` / `guides_get` and then
   applies with its own judgment via `settings_set_values`. The one exception
   is calibration (`calibration_generate`), because generating a calibration
   print is a deterministic, procedural OrcaSlicer feature.

4. **Every read tool controls its output size.** `project_inspect` and
   `slice_analyze` take a `detail` parameter and default to the smallest
   useful response. Response size is a bigger token problem than catalog size.

5. **Errors teach.** An unknown key returns nearest matches; an out-of-range
   value returns the valid range; a conflicting combination says what
   conflicts and why. Conventions in [docs/errors.md](docs/errors.md).

6. **Tool names use underscores only.** Anthropic's API rejects tool names
   containing dots, so `settings.set_values` style is out.

## Tool inventory (13 tools)

| Tool | File | Purpose |
|---|---|---|
| `project_open` | [tools/project.json](tools/project.json) | Open a .3mf/.stl/.step/.obj file |
| `project_inspect` | [tools/project.json](tools/project.json) | Plates, objects, active presets, warnings |
| `settings_search` | [tools/settings.json](tools/settings.json) | Find setting keys by natural language |
| `settings_get_schema` | [tools/settings.json](tools/settings.json) | Full schema for specific keys |
| `settings_get_values` | [tools/settings.json](tools/settings.json) | Effective values + which layer set them |
| `settings_set_values` | [tools/settings.json](tools/settings.json) | Batch mutation with validation and dry-run |
| `slice_run` | [tools/slice.json](tools/slice.json) | Slice plate(s) |
| `slice_analyze` | [tools/slice.json](tools/slice.json) | Time, filament, per-feature stats, warnings |
| `gcode_export` | [tools/slice.json](tools/slice.json) | Export G-code or sliced .3mf |
| `calibration_generate` | [tools/calibration.json](tools/calibration.json) | Generate flow/PA/temp/retraction calibration prints |
| `guides_search` | [tools/guides.json](tools/guides.json) | Find tuning/troubleshooting guides |
| `guides_get` | [tools/guides.json](tools/guides.json) | Retrieve one guide's full text |
| `diagnostics_get` | [tools/diagnostics.json](tools/diagnostics.json) | Version, backend mode, log tail |

Definition files use the MCP `tools/list` result shape (`name`, `title`,
`description`, `inputSchema`, `annotations`) so they can be served verbatim.

## The settings layering model

OrcaSlicer resolves a setting through layers: **preset** (printer / filament /
process) → **project override** → **per-plate** → **per-object modifier**.
An agent that edits the wrong layer makes changes that don't take effect or
that leak beyond what the user asked for. Therefore:

- `settings_get_values` always returns the *effective* value **and** the
  `source` layer that produced it.
- `settings_set_values` requires an explicit `scope` and returns
  `side_effects`: any other settings OrcaSlicer force-changed as a consequence
  (e.g. enabling `spiral_mode` alters wall/infill settings).

Details in [docs/settings-registry.md](docs/settings-registry.md).

## Two backends, phased

**Phase 1 — headless (buildable today).** Backed by the existing CLI
(`src/OrcaSlicer.cpp` supports `slice`, `export_gcode`, `export_3mf`,
`load_settings`, `load_filaments`, `arrange`, `orient`, `info`,
`export_settings`). The server holds a session: an opened project plus pending
setting overrides, materialized into CLI invocations. All 13 tools work in
this mode; `diagnostics_get` reports `backend: "headless"`.

**Phase 2 — interactive (requires app changes).** Controlling a *running*
OrcaSlicer GUI (live project state, preview capture, undo, reacting to user
edits) has no existing API; it needs an embedded server inside the app. Tools
like `preview_capture` and `operation_undo` are deliberately **absent** from
this design until that backend exists. Do not spec tools the backend can't
honestly implement.

## Deliberately not in this design

- One tool per setting (catalog bloat, poor tool selection).
- `workflow_*` advice tools (reasoning belongs to the model; see
  [docs/guides/README.md](docs/guides/README.md)).
- MCP resources as the primary data channel (weak client support).
- GUI-control tools without a GUI-control backend.

## Validation plan

Before implementation settles, run realistic tasks against a prototype
("this benchy strings, fix it and reslice"; "make this bracket stronger
without doubling print time"; "calibrate PA for this filament") and let the
transcripts drive changes to schemas, defaults, and error messages. The tool
*list* is designed top-down; the tool *details* must be tuned bottom-up.

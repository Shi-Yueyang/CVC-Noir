---
name: create-dataplug
description: Generate dataplug.bin test data with controlled wheel-diameter values for simulator scenarios. Use when creating deterministic dataplug.bin input for local simulator tests or when validating PDA/dataplug parsing with varied wheel diameters.
---

# create-dataplug

## Purpose

Generate `dataplug.bin` test data with controlled wheel-diameter values for simulator scenarios.

## When to use

- You need a deterministic `dataplug.bin` for local simulator tests.
- A task changes PDA/dataplug parsing or behavior and needs reproducible input data.
- You need to vary wheel diameters quickly for validation.

## When not to use

- You need to modify `simu_config.json` itself (use configuration docs/process instead).
- You need non-diameter binary payload formats not supported by this helper.

## Script

- Script path: `skills/create-dataplug/create_dataplug.py`

## Inputs

- `--diameter` / `-d` (one or more integers)
  - Default: `1050 1050 1050`
- `--output-dir` / `-o` (directory)
  - Default: current directory

## Command templates

### Default output in current directory

```powershell
python skills/create-dataplug/create_dataplug.py
```

### Custom diameters and output folder

```powershell
python skills/create-dataplug/create_dataplug.py -d 1048 1050 1052 -o Plug
```

## Expected output

- Creates `<output-dir>/dataplug.bin`.
- Writes each diameter as little-endian unsigned 16-bit values in sequence.
- Prints a summary line with values and output path.

## Verification

- Confirm file exists at the expected path.
- Confirm file length is `2 * number_of_diameter_values` bytes.
- Use the file in simulator test flow and verify behavior via logs/results.

## Safety and constraints

- Do not overwrite unrelated files manually; this script writes only `dataplug.bin` in target directory.
- Keep test artifacts scoped to test folders unless a task explicitly requires committed data updates.
- If this script is used as part of task verification, report command, inputs, and observed result in final response.

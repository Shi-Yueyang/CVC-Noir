# Testing Guide

## Purpose

Every change must be validated before completion.
Testing depth should match change impact, but at minimum include a successful build and targeted behavior verification when execution is feasible.

## Environment prerequisites

- Build using Visual Studio developer environment:
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat`
- Run commands from repository root.

## Minimum required checks per task

1. Build check
- Build affected target/configuration.
- Ensure no new compile errors.

2. Runtime sanity check (when runnable)
- Start simulator for tested configuration.
- Verify startup succeeds and no immediate crash/assert appears.

3. Targeted behavior check
- Execute scenario tied to changed area.
- Use logs/outputs/script results as evidence.

## Standard test workflow (REQUIRED)

Complete all steps and mark checklist before finishing.

1. ☐ **Prepare test config**
   - Disable unrelated launcher entries in `simu_config.json` when external dependencies are not needed.
   - Keep test-only config changes reversible.

2. ☐ **Build**
   - Build using the same variant impacted by the change.

3. ☐ **Run simulator**
   - Launch executable from output directory.
   - Confirm initialization logs for touched modules.

4. ☐ **Run scenario scripts** (when applicable)
   - Protocol/framing checks: `skills/tcp_framing_test.py`.
   - Dataplug/PDA preparation: `skills/create-dataplug/create_dataplug.py`.
   - C++ unit tests live in `tests/` (each `test_*.cpp` is a CMake target); run with `ctest --test-dir build`.
   - Add focused skills under `skills/` when needed.

5. ☐ **Verify evidence**
   - Confirm expected behavior from logs/outputs.
   - Confirm no new unexpected warnings/errors in touched paths.

6. ☐ **CLEANUP (MANDATORY)**
   - MUST remove temporary diagnostic logs/prints.
   - MUST revert test-only config edits unless explicitly requested as final output.
   - MUST delete test output files (stdout.txt, stderr.txt, temporary .log files).
   - MUST confirm workspace is clean before submitting.

## Change-based test matrix

1. Config parsing changes
- Test valid and invalid/missing input for changed fields.
- Verify fallback/default behavior.

2. Session/transport changes
- Verify session open/send/receive behavior for touched types.
- Validate framing and endpoints.

3. Launcher/startup/shutdown changes
- Verify launch behavior and auto-close behavior.

4. PDA/data handling changes
- Verify expected read/write behavior and file updates.
- Ensure unrelated data files are untouched.

5. Logging changes
- Verify expected module and level output.
- Ensure no excessive noisy output introduced.

6. PXI `from_pxi` / `to_pxi` array schema changes
- Verify `from_pxi` is parsed as an array of mapping rules.
- Verify each `from_pxi` rule's `total_length`, `high_value`, and `entries` are loaded correctly.
- Verify the `from_pxi` rule whose `total_length` exactly matches the incoming packet size is selected.
- Verify a warning is logged and `from_pxi` mapping is skipped when no rule matches, while `from_internal` still publishes.
- Verify the legacy single-object `from_pxi` format is rejected with an error log.
- Verify `to_pxi` accepts both a single object and an array of rules.
- Verify each `to_pxi` rule's `length` discriminator, `total_length`, `high_value`, and `entries` are loaded correctly.
- Verify the `to_pxi` rule whose `length` matches `vob_data.Length` is selected and the packet is padded to that rule's `total_length`.
- Verify a warning is logged and the VOB message is skipped when no `to_pxi` rule matches.
- Verify the legacy single-object `to_pxi` format still works (treated as a one-rule array with `length` defaulting to `total_length`).
- Verify `port_names` is parsed when present: `input` and `output` sub-objects populate the respective name maps.
- Verify partial naming: ports listed in `port_names` show their name in trace logs; unlisted ports fall back to numeric index.
- Verify missing `port_names`, `input`, or `output` does not error and all ports in that direction show as indices.
- Verify invalid keys (non-numeric/negative) and non-string values are skipped with a warning log.
- Verify trace log lines (`in: HIGH[...] LOW[...]` / `out: HIGH[...] LOW[...]`) are emitted at trace level with correct HIGH/LOW split (`PortValue == 1` is HIGH) and ascending port order.
- Verify `trace_unnamed_ports` defaults to `true` when omitted (unnamed ports shown as indices).
- Verify `trace_unnamed_ports: false` omits unnamed ports from both HIGH/LOW groups while named ports still appear.

## Pre-completion checklist

Before marking task complete, verify ALL items:

- [ ] **Cleanup completed** - No temporary test files remain in workspace (stdout.txt, stderr.txt, *.log in non-standard locations, test artifacts)
- [ ] **Config reverted** - All test-only configuration changes restored to original state
- [ ] **Diagnostics removed** - No temporary logging/print statements left in code
- [ ] **Build artifacts clean** - No temporary build outputs committed or left behind

## Pass criteria

All must be true:

1. **Cleanup verified** - Pre-completion checklist fully satisfied.
2. Build succeeds for tested target.
3. Startup/shutdown behavior is normal for scenario.
4. Target behavior is confirmed with concrete evidence.
5. No new unintended errors in touched path.
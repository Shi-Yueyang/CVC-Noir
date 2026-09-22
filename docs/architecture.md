# Architecture

## Purpose

cvc-noir is a host-side simulator for the 300C embedded system.

- `Source/300C_SIMU` contains simulator runtime, host integrations, and platform-side adapters.
- Domain logic (e.g. `Source/ATP_CODE`) contains embedded C code, managed externally by each team.

This document defines top-level boundaries so changes stay safe and localized.

## Runtime model

### Entry and loop

- Entry point: `Source/300C_SIMU/main.cpp`.
- Startup flow:
  - resolve config path: optional `argv[1]`, otherwise default `simu_config.json`
  - load the resolved config (logger init, subsystem init via `Initial(...)`)
  - launch configured external processes via `AppLauncher`
- Main cycle:
  - `SyncInput(...)`
  - `MAIN_SAFETY_F_ProcessInData()`
  - `SyncOutput(...)`
- Shutdown flow:
  - close auto-launched processes
  - shutdown logger

### Orchestration

- `Source/300C_SIMU/BSW_CODE/Initial.cpp` drives configuration-based initialization:
  - session initialization
  - board status parsing
  - PDA storage parsing
  - ASW init and plug init

### Interface boundary

- `Source/300C_SIMU/ASW_300C/interface_p2a.h` is the host/platform boundary.
- ATP-facing behavior should pass through defined interfaces rather than simulator internal types.

## Dependency isolation

### Required direction

- Simulator runtime modules under `Source/300C_SIMU` must not add direct dependencies on domain logic internals.
- Coupling should remain at interface/protocol/config boundaries.

### Allowed coupling

- Stable interface-level contracts already used by the build.
- Configuration-driven behavior from `simu_config.json`.

### Disallowed coupling

- Importing domain logic private/internal implementation types into simulator runtime modules.
- Cross-layer shortcuts that bypass boundary interfaces.

## Change scope policy

- Prefer edits in `Source/300C_SIMU` and config/docs.
- Do not make functional edits in domain logic folders unless explicitly requested.
- Temporary diagnostic logging is allowed during investigation and must be removed before finalizing.
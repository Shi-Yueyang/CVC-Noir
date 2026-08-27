# Documentation Map

## Purpose

This file defines where canonical project guidance lives and how conflicts are resolved.

## Canonical topics

- `docs/architecture.md`: project architecture and dependency boundaries.
- `docs/configuration.md`: `simu_config.json` configuration specification.
- `docs/testing.md`: test strategy, required checks, and pass criteria.
- `docs/behavior-guidelines.md`: coding behavior and quality expectations.
- `docs/workflow.md`: execution lifecycle and final report format.

## Module index

Module-local READMEs contain API behavior, examples, and connection details for their respective components. Keep implementation-specific details in these files to avoid duplication in `docs/`.

| Module | Path | Scope |
|---|---|---|
| Pub_Comm | `Source/300C_SIMU/BSW_CODE/Pub_Comm/README.md` | Connection factory and transport implementations (UDP, TCP, serial, dummy). |
| PDA APIs | `Source/300C_SIMU/ASW_300C/README.md` | Persistent Data Area (PDA) storage API reference. |

## Adapter files

- `README.md`: human quick overview, build quickstart, links to canonical docs.
- `AGENTS.md`: generic agent adapter; routes to canonical docs under `docs/`.

## Precedence rules

1. Files under `docs/` are source of truth by topic.
2. Adapter files summarize and route, they do not redefine canonical topic rules.
3. If conflict exists, topic files under `docs/` win.

## Ownership and updates

- Update only the topic file that owns the changed content.
- Do not duplicate topic content across files; link instead.
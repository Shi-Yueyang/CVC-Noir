# Agent Adapter

This file is a generic adapter for coding agents.
Canonical project guidance lives under `docs/`.

## Canonical document map

- `docs/doc-map.md`: ownership, precedence, and navigation.
- `docs/architecture.md`: top-level architecture and isolation boundaries.
- `docs/configuration.md`: `simu_config.json` specification.
- `docs/testing.md`: mandatory testing flow and evidence requirements.
- `docs/behavior-guidelines.md`: coding behavior and quality rules.
- `docs/workflow.md`: execution workflow and final reporting contract.

please read all these docs

## Skills

Project skills live under `skills/` and are registered for agent discovery via
`kilo.json` (`skills.paths`). Each skill is a folder containing a `SKILL.md`
(with `name` + `description` frontmatter) plus its script(s).

- `skills/auto_click/`: drive one or more Windows GUI apps with ordered
  clicks/typing via `pyautogui` using window-relative coordinates (deps in
  `skills/requirements.txt`, venv at `skills/.venv/`).
- `skills/create-dataplug/`: generate `dataplug.bin` test data with controlled
  wheel-diameter values.

To add a new skill, create `skills/<name>/SKILL.md` and its script. Keep all
automation/tooling scripts under `skills/`.

## Precedence

1. Topic files under `docs/` are the source of truth.
2. Adapter files summarize and route only.
3. If any conflict appears, `docs/` wins.

## Non-negotiable constraints

- `Source/300C_SIMU` must not add direct dependencies on domain logic internals.
- Prefer edits in simulator/runtime/config/docs scope unless explicitly requested otherwise.
- Do not make functional edits in domain logic folders unless explicitly requested.
- Temporary debug instrumentation is allowed for diagnosis but must be removed before finalizing.
- Run build and relevant tests before completing a task whenever feasible; report limits if full validation is blocked.

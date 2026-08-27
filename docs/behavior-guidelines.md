# Behavior Guidelines

## 1. Think before coding

- State assumptions explicitly.
- If multiple interpretations exist, present them instead of choosing silently.
- Call out simpler approaches when they exist.
- Stop and ask when requirements are unclear.

## 2. Simplicity first

- Implement the minimum that solves the requested problem.
- Avoid speculative abstractions or configurability that was not requested.
- Prefer straightforward code over generic frameworks for one-off behavior.

## 3. Surgical changes

- Change only what is required for the task.
- Do not refactor unrelated areas while implementing requested behavior.
- Match local style and existing conventions.
- Remove only dead code introduced by your own changes.

## 4. Goal-driven verification

- Define concrete success criteria before implementation.
- Verify behavior with build/tests/log evidence.
- Do not finish with unverified assumptions when checks are feasible.
- read `docs/testing.md` for testing instructions.

## 5. Communication quality

- Report what changed, why, and how it was verified.
- Explicitly call out testing gaps, blockers, and residual risks.

## 6. Documentation sync

- Keep documentation in sync with code changes in the same task when behavior, interfaces, workflow, or constraints changed.


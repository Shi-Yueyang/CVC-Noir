# Workflow

## Standard execution flow

1. Understand the request and boundaries
- Confirm scope from `docs/architecture.md`.
- Identify exact modules/files to touch.

2. Define verifiable goals
- Convert request into concrete checks.
- Prefer checks that can be proven by build/test/log output.

3. Implement minimally
- Keep edits directly tied to requested behavior.
- Avoid unrelated refactors.

4. Validate
- Build affected target(s).
- Run relevant test scenarios from `docs/testing.md`.
- Collect evidence from logs/output.

5. Finalize
- Remove temporary diagnostic instrumentation.
- Revert temporary test-only config changes unless they are part of requested output.

## Final report contract

Final report should include:

1. What changed.
2. Build/test commands executed.
3. Evidence summary from outputs/logs.
4. What was not tested.
5. Residual risks or blockers.
---
name: unitlab-release
description: Use for unitlab release preparation, validation planning, focused diffs, changelog/release notes, versioning checks, commit message selection, package build verification, and pre-merge quality gates.
---

# unitlab Release

## Scope

Use this skill when preparing a focused change for commit/release, validating package readiness, writing changelog or release notes, selecting commit messages, or checking that diffs are safe and scoped.

## Release Rules

- Keep commits focused and separable.
- Prefer conventional commit subjects with accurate scope.
- Do not hide unrelated failures; report them separately.
- Do not broaden validation without a reason.
- Verify docs, tests, and type-check/build status match the behavioral risk.

## Pre-Commit Checklist

- Worktree changes are scoped to the requested slice.
- Public API changes are approved and documented.
- Relevant docs are updated or `docs: not needed` is recorded.
- Targeted tests pass.
- Package-level type-check/build passes when code changed.
- `git diff --check` passes.
- Suggested commit message describes behavior, not implementation trivia.

## Validation Selection

- Docs-only: `git diff --check` is usually enough.
- Single composable/component: targeted Vitest + package type-check.
- Protocol/backend behavior: focused service tests + type-check/build.
- Performance slice: targeted tests plus benchmark/perf trace where available.
- Release prep: package build/type-check and changelog/release note review.

## Reporting

Return concise status, validation run, unresolved issues, and suggested commit message.

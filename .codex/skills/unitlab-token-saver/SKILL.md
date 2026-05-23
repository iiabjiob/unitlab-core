---
name: unitlab-token-saver
description: Minimize unnecessary output during long unitlab refactors, repetitive engineering tasks, validation-heavy workflows, and cost-sensitive Codex sessions. Use when Codex should stay concise, make focused code changes, run targeted validation, and report only the essential result.
---

# unitlab Token Saver

## Operating Rules

- Work in small slices: audit, plan, implement, validate.
- Prefer direct code changes over narration.
- Keep scope bounded; avoid unrelated packages and broad refactors.
- Preserve public APIs unless the task explicitly requires a change.
- If the task needs a larger architectural shift, stop and propose it first.

## Validation

- Run the smallest relevant check first.
- Prefer targeted tests, type-checks, or builds over full monorepo validation.
- If validation fails for unrelated reasons, report that separately and do not expand scope.

## Reporting

- Keep user-facing output compact.
- Do not narrate exploration or implementation steps.
- Return only the essential result, validation, unresolved issues, and a concise commit message when asked for completion.

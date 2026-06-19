# Independent Review Notes

This directory is the landing zone for independent reviewer feedback.

Use it for external/human review of an iteration, screenshot, diagnosis, plan
change, or phase gate. Keep reviewer text separate from iteration scoring so the
progress file stays readable and the original critique remains auditable.

## File Naming

Use:

```text
YYYY-MM-DD-<target-slug>.md
```

Examples:

```text
2026-06-19-a2-track-adapts-v1.md
2026-06-20-stage-b-sky-gate.md
```

## Required Workflow

1. Reviewer writes the full opinion in a review file.
2. Agent adds or updates one line in `docs/plans/photoreal-progress.md` under
   `## 独立外审 / Reviewer Notes`.
3. Agent copies blocking action items into `## NEEDS-HUMAN / Blocked 待办`.
4. Agent copies accepted process-level decisions into `## 决策记录`.
5. Agent does not mark a phase Done while any relevant review has
   `Verdict: FAIL` or `Verdict: NEEDS-HUMAN` with unresolved blocking issues.

## Template

```md
# <Review Title>

- Date:
- Reviewer:
- Review target:
- Reviewed artifacts:
- Verdict: PASS | FAIL | NEEDS-HUMAN | COMMENT
- Related phase/task:

## Summary

## Blocking Issues

1.

## Non-Blocking Suggestions

1.

## Required Next Action

## Agent Disposition

- Status: Open | Accepted | Rejected | Superseded | Done
- Progress link:
- Follow-up evidence:
```

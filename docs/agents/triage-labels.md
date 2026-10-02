# Triage Labels

The skills speak in terms of five canonical triage roles. This file maps those roles to the actual label strings used in this repo's issue tracker.

| Label in mattpocock/skills | Label in our tracker | Meaning                                  |
| -------------------------- | -------------------- | ---------------------------------------- |
| `needs-triage`             | `needs-triage`       | Maintainer needs to evaluate this issue  |
| `needs-info`               | `needs-info`         | Waiting on reporter for more information |
| `ready-for-agent`          | `ready-for-agent`    | Fully specified, ready for an AFK agent  |
| `ready-for-human`          | `ready-for-human`    | Requires human implementation            |
| `wontfix`                  | `wontfix`            | Will not be actioned                     |

When a skill mentions a role (e.g. "apply the AFK-ready triage label"), use the corresponding label string from this table.

Edit the right-hand column to match whatever vocabulary you actually use.

## Provision labels before first use

Label assignment does not create repository labels. Before using the triage or Wayfinding workflows, a maintainer with permission to manage labels must run the following Bash block. It creates only missing labels, preserves existing colors and descriptions (including `wontfix`), and is safe to rerun. Stop and ask a maintainer if listing or creating labels fails.

```bash
set -e
repo=open-source-space-foundation/proves-core-reference
existing_labels=$(gh api --paginate "repos/$repo/labels" --jq '.[].name')

while IFS='|' read -r name color description; do
  if ! grep -Fqx -- "$name" <<< "$existing_labels"; then
    gh label create "$name" -R "$repo" --color "$color" --description "$description"
  fi
done <<'LABELS'
needs-triage|FBCA04|Maintainer needs to evaluate this issue
needs-info|D876E3|Waiting on reporter for more information
ready-for-agent|0E8A16|Fully specified, ready for an AFK agent
ready-for-human|1D76DB|Requires human implementation
wontfix|FFFFFF|This will not be worked on
wayfinder:map|5319E7|Wayfinding map with notes, decisions, and open questions
wayfinder:research|D4C5F9|Wayfinding research ticket
wayfinder:prototype|C5DEF5|Wayfinding prototype ticket
wayfinder:grilling|FEF2C0|Wayfinding design clarification ticket
wayfinder:task|BFDADC|Wayfinding implementation task
LABELS
```

Verify the labels with `gh label list -R open-source-space-foundation/proves-core-reference --limit 1000` before assigning them.

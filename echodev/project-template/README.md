# Unreal-Echo Project Template

A comprehensive project management template for tracking the development of the Unreal-Echo neuro-symbolic AGI across multiple GitHub organizations.

## Overview

This template provides:

- **Issue Templates**: Structured templates for Epics, Phases, and Features
- **Label Configuration**: Consistent labeling across organizations
- **Milestone Definitions**: Release milestones aligned with the roadmap
- **Issue Generation Scripts**: Automated issue creation from roadmap data
- **Cross-Org Synchronization**: Progress tracking across multiple organizations
- **GitHub Actions Workflows**: Automated progress reporting

## Directory Structure

```
project-template/
├── .github/
│   ├── ISSUE_TEMPLATE/
│   │   ├── epic.yml          # Epic issue template
│   │   ├── phase.yml         # Phase issue template
│   │   └── feature.yml       # Feature issue template
│   └── workflows/
│       └── sync-progress.yml # Automated sync workflow
├── config/
│   ├── roadmap-schema.json   # JSON schema for roadmap data
│   ├── roadmap-data.json     # Complete roadmap with all epics
│   ├── labels.json           # Label definitions
│   ├── milestones.json       # Milestone definitions
│   └── organizations.json    # Cross-org configuration
├── scripts/
│   ├── generate-issues.py    # Issue generation script
│   └── sync-orgs.py          # Cross-org sync script
└── README.md                 # This file
```

## Quick Start

### 1. Generate Issues for a Single Epic

```bash
# Dry run to preview
python scripts/generate-issues.py \
  --roadmap config/roadmap-data.json \
  --epic E1 \
  --org orgitcog \
  --repo u9n \
  --dry-run

# Actually create issues
python scripts/generate-issues.py \
  --roadmap config/roadmap-data.json \
  --epic E1 \
  --org orgitcog \
  --repo u9n
```

### 2. Generate All Issues

```bash
python scripts/generate-issues.py \
  --roadmap config/roadmap-data.json \
  --org orgitcog \
  --repo u9n
```

### 3. Create Labels and Milestones

```bash
# Create labels
python scripts/generate-issues.py \
  --roadmap config/roadmap-data.json \
  --org orgitcog \
  --repo u9n \
  --create-labels \
  --labels-config config/labels.json

# Create milestones
python scripts/generate-issues.py \
  --roadmap config/roadmap-data.json \
  --org orgitcog \
  --repo u9n \
  --create-milestones \
  --milestones-config config/milestones.json
```

### 4. Generate Cross-Org Progress Report

```bash
python scripts/sync-orgs.py \
  --config config/organizations.json \
  --action report \
  --output progress-report.md
```

### 5. Create Tracking Issue

```bash
python scripts/sync-orgs.py \
  --config config/organizations.json \
  --action create-tracking-issue \
  --target-org orgitcog \
  --target-repo u9n
```

### 6. Sync Labels Between Repos

```bash
python scripts/sync-orgs.py \
  --config config/organizations.json \
  --action sync-labels \
  --source-org orgitcog \
  --source-repo u9n \
  --target-org cogpy \
  --target-repo cogplan9
```

## Configuration

### Adding a New Organization

Edit `config/organizations.json`:

```json
{
  "organizations": [
    {
      "name": "NewOrg",
      "github_org": "neworg",
      "role": "contributor",
      "repos": ["repo1", "repo2"],
      "responsibilities": ["Component X", "Component Y"]
    }
  ]
}
```

### Adding a New Epic

Edit `config/roadmap-data.json` following the schema in `config/roadmap-schema.json`.

### Customizing Labels

Edit `config/labels.json` to add or modify labels.

## GitHub Actions

The template includes a workflow that:

- Runs weekly on Mondays at 9 AM UTC
- Generates a progress report
- Can be manually triggered for specific actions

### Required Secrets

- `CROSS_ORG_TOKEN`: A GitHub PAT with access to all participating organizations

## Issue Hierarchy

```
Epic (E1, E2, ...)
└── Phase (1.1, 1.2, ...)
    └── Feature (F1.1.1, F1.1.2, ...)
        └── Task (implementation details)
```

## Labels

| Category | Labels |
|----------|--------|
| Hierarchy | `epic`, `phase`, `feature`, `task`, `roadmap` |
| Component | `neural-core`, `symbolic-engine`, `integration`, ... |
| Priority | `priority: critical/high/medium/low` |
| Status | `status: planned/in-progress/review/blocked/completed` |
| Effort | `effort: 1/2/3/5/8/13/21` (Fibonacci points) |

## Milestones

| Milestone | Target | Description |
|-----------|--------|-------------|
| M1: Foundation Complete | 2026-06-30 | Core neural-symbolic bridge |
| M2: Embodiment Alpha | 2026-10-31 | Basic avatar control |
| M3: Cognitive Core Beta | 2027-04-30 | Full cognitive architecture |
| M4: Emotional Intelligence | 2027-08-31 | Digital endocrine system |
| M5: Gaming Mastery Alpha | 2027-12-31 | Game learning capability |
| M6: Social Intelligence | 2028-04-30 | Theory of Mind |
| M7: Self-Aware AGI | 2028-08-31 | Meta-cognitive capabilities |
| M8: Production Release 1.0 | 2028-12-31 | Full production readiness |

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

## License

MIT License - See LICENSE file for details.

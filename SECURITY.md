# Security Policy

## Overview

This repository (un9n) implements the Deep Tree Echo cognitive architecture for embodied AI avatars using Unreal Engine 5.x. Security is a priority for all project code.

## High-Privileged Callback Policy

This project's custom code does **NOT** use any of the following high-privileged editor/build callbacks:

| Callback | Risk | Status |
|----------|------|--------|
| `FEditorDelegates::OnAssetPostImport` | Auto-executes with editor privileges during asset import | **Not used** |
| `FEditorDelegates::OnAssetPreImport` | Auto-executes with editor privileges before asset import | **Not used** |
| `UBT PreBuildSteps` / `PostBuildSteps` | Executes arbitrary commands during build | **Not used** |

These callbacks exist only in the vendored Unreal Engine source (`Engine/` directory), which is standard Epic Games code and not modified by this project.

### Why These Are Restricted

These callbacks can be automatically invoked by the engine during normal import/build workflows, and logic implemented in them runs with elevated editor privileges. If abused, they could execute malicious code without explicit developer action.

### Policy

Any future use of these callbacks in project code (`DeepTreeEcho/`, `UnrealEcho/`, `ReservoirEcho/`, `Source/`) **requires**:
1. Explicit security review by a project maintainer
2. Clear documentation of why the callback is necessary
3. Input validation and sandboxing where applicable
4. PR approval from at least two reviewers

## Reporting Security Issues

If you discover a security vulnerability in this project's code, please report it responsibly:

1. **Do not** open a public GitHub issue for security vulnerabilities
2. Email the maintainers directly or use GitHub's private vulnerability reporting feature
3. Include detailed reproduction steps and impact assessment

For vulnerabilities in the vendored Unreal Engine source, please report to Epic Games at security@epicgames.com or via their [HackerOne program](https://hackerone.com/epicgames).

## Scope

This security policy covers the project's custom source code:
- `DeepTreeEcho/` — Core cognitive architecture
- `UnrealEcho/` — Unreal Engine cognitive components
- `ReservoirEcho/` — Echo State Network library
- `MetaHuman-DNA-Calibration/` — MetaHuman rig system
- `Source/` — UE module build definitions

The `Engine/` directory contains unmodified Unreal Engine 5.3 source governed by Epic Games' security policies.
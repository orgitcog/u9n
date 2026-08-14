# Contributing to un9n (Deep Tree Echo)

Thank you for your interest in contributing to the Deep Tree Echo cognitive architecture project.

## Getting Started

1. Fork the repository
2. Create a feature branch from `main`
3. Make your changes following the guidelines below
4. Submit a pull request

## Code Style

- Follow Unreal Engine 5.x C++ conventions
- Use `UCLASS`, `USTRUCT`, `UPROPERTY`, `UFUNCTION` macros appropriately
- Include `.generated.h` headers for reflection
- Disable unity builds in new modules for better debugging (`bUseUnity = false`)

## Security Guidelines

### Prohibited Patterns (Require Security Review)

The following patterns are **prohibited without explicit security review** by a project maintainer:

#### High-Privileged Editor Callbacks

Do NOT bind to these delegates in project code without review:

```cpp
// PROHIBITED - Auto-executes with editor privileges during asset import
GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetPostImport.AddUObject(...);
GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetPreImport.AddUObject(...);

// PROHIBITED - Legacy delegate form
FEditorDelegates::OnAssetPostImport.AddRaw(...);
FEditorDelegates::OnAssetPreImport.AddRaw(...);
```

**Why:** These callbacks execute automatically when assets are imported into the editor. Code bound to them runs with full editor privileges and can perform arbitrary operations without user consent.

#### Build Step Injection

Do NOT add `PreBuildSteps` or `PostBuildSteps` to any `.Target.cs` file:

```csharp
// PROHIBITED - Executes arbitrary commands during build
PreBuildSteps.Add("...");
PostBuildSteps.Add("...");
```

**Why:** Build steps execute arbitrary shell commands during the UBT build process. They run with the full privileges of the build user and can modify the system without developer awareness.

#### Other High-Risk Patterns

- `FCoreDelegates::OnPreExit` with external network calls
- `FModuleManager` callbacks that download/execute external code
- Any callback that writes to system paths outside the project directory

### If You Need These Patterns

If your contribution genuinely requires one of these patterns:

1. Open an issue describing the use case before implementing
2. Document why no safer alternative exists
3. Add input validation and scope-limiting guards
4. Ensure the PR is reviewed by at least two maintainers
5. Add inline comments explaining the security implications

## Pull Request Process

1. Ensure your code compiles without errors
2. Add appropriate documentation for new components
3. Update `CLAUDE.md` if you add new modules or change architecture
4. Security-sensitive changes require two approvals
5. Include test coverage for new cognitive components

## Module Structure

When adding a new cognitive component:

1. Create header in the appropriate `DeepTreeEcho/` subdirectory
2. Inherit from `UActorComponent`
3. Define state struct with `USTRUCT(BlueprintType)`
4. Implement in corresponding `.cpp` file
5. Register with `DeepTreeEchoCore` for tick updates

## Questions?

Open a discussion issue if you have questions about contributing.

## Learned User Preferences
- Do not commit `Engine/`, `.cursor/`, or `videosrc/` unless explicitly asked.
- Never force-push `origin/main`.
- Use Windows PowerShell for shell commands: no bash `&&` chaining or bash heredocs.

## Learned Workspace Facts
- Local `main` tracks `origin/dte-nest4-echoself-autonomy`; `origin/main` is an unrelated GitHub history.
- DeepTreeEcho uses nest-4 (OEIS A000081) 9-term shells with EchoSelf autonomy; AutonomyPipeline InputDim stays 38.
- Reconcile `origin/main` with a PR overlay (delete Windows `(2)` duplicate files, then overlay nest-4/EchoSelf sources), not an unrelated-history merge.
- EchoSelf compiles in the DeepTreeEcho module (`DEEPTREEECHO_API`), not as a separate game-target module.

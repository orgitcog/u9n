// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "uLang/Toolchain/Toolchain.h"
#include "uLang/Common/Containers/SharedPointer.h"
#include "uLang/Common/Containers/Array.h"
#include "uLang/CompilerPasses/IPostIrFilter.h"
#include "uLang/CompilerPasses/IPostSemanticAnalysisFilter.h"
#include "uLang/Diagnostics/Diagnostics.h"

#define UE_API VERSECOMPILER_API

namespace uLang
{
// Forward declarations
class CUTF8StringView;

struct SToolchainOverrides
{
    TOptional<TSPtr<IParserPass>>                      Parser;
    TOptional<TSRefArray<IPostVstFilter>>              PostVstFilters;
    TOptional<TSPtr<ISemanticAnalyzerPass>>            SemanticAnalyzer;
    TOptional<TSRefArray<IPostSemanticAnalysisFilter>> PostSemanticAnalysisFilters;
    TOptional<TSRefArray<IPostIrFilter>>               PostIrFilters;
    TOptional<TSPtr<IAssemblerPass>>                   Assembler;

    TOptional<TSRefArray<IPreParseInjection>>         PreParseInjections;
    TOptional<TSRefArray<IPostParseInjection>>        PostParseInjections;
    TOptional<TSRefArray<IPreSemAnalysisInjection>>   PreSemAnalysisInjections;
    TOptional<TSRefArray<IIntraSemAnalysisInjection>> IntraSemAnalysisInjections;
    TOptional<TSRefArray<IPostSemAnalysisInjection>>  PostSemAnalysisInjections;
    TOptional<TSRefArray<IPreTranslateInjection>>     PreTranslateInjections;
    TOptional<TSRefArray<IPreLinkInjection>>          PreLinkInjections;
};

struct SBuildManagerParams
{
    // For the pieces of this that are set, the toolchain will be constructed
    // using those specified parts -- for the other toolchain pieces, the
    // build-manager will perform auto-discovery to fill the rest out.
    SToolchainOverrides _ToolchainOverrides;

    //SToolchainParams _ToolchainParams;
    TSPtr<CSemanticProgram> _ExistingProgram; // Optional existing program
};

class CProgramBuildManager : public CSharedMix
{
public:
    UE_API CProgramBuildManager(const SBuildManagerParams& Params);
    UE_API ~CProgramBuildManager();

    UE_API void SetSourceProject(const TSRef<CSourceProject>& Project);
    UE_API void AddSourceSnippet(const TSRef<ISourceSnippet>& Snippet, const CUTF8StringView& PackageName, const CUTF8StringView& PackageVersePath);
    UE_API void RemoveSourceSnippet(const TSRef<ISourceSnippet>& Snippet);
    UE_API const CSourceProject::SPackage& FindOrAddSourcePackage(const CUTF8StringView& PackageName, const CUTF8StringView& PackageVersePath);

    UE_API SBuildResults Build(const SBuildParams& Params, TSRef<CDiagnostics> Diagnostics);

    const TSRef<CToolchain>& GetToolchain() const               { return _Toolchain; }
    const SProgramContext& GetProgramContext() const            { return _ProgramContext; }
    const TSRef<CSourceProject>& GetSourceProject() const       { return _SourceProject; }
    const TUPtr<SPackageUsage>& GetPackageUsage() const         { return _PackageUsage; }
    TArray<FSolLocalizationInfo> TakeLocalizationInfo()         { return _Toolchain->TakeLocalizationInfo(); }
    TArray<FSolLocalizationInfo> TakeStringInfo()               { return _Toolchain->TakeStringInfo(); }

    UE_API SBuildResults BuildProject(const CSourceProject& SourceProject, const SBuildContext& BuildContext);
    UE_API ECompilerResult ParseSnippet(const uLang::TSRef<Verse::Vst::Snippet>& OutVst, const CUTF8StringView& TextSnippet, const SBuildContext& BuildContext);
    UE_API ECompilerResult SemanticAnalyzeVst(TOptional<TSRef<CSemanticProgram>>& OutProgram, const TSRef<Verse::Vst::Project>& Vst, const SBuildContext& BuildContext);
    UE_API ECompilerResult IrGenerateProgram(const TSRef<CSemanticProgram>& Program, const SBuildContext& BuildContext);
    UE_API ECompilerResult AssembleProgram(const TSRef<CSemanticProgram>& Program, const SBuildContext& BuildContext);
    UE_API ELinkerResult Link(const SBuildContext& BuildContext);

    UE_API void ResetSemanticProgram();
    const TSPtr<Verse::Vst::Project>& GetProjectVst() const { return _Toolchain->GetProjectVst(); }
    void SetProjectVst(const TSRef<Verse::Vst::Project>& NewProject)
    {
        return _Toolchain->SetProjectVst(NewProject);
    }

    UE_API void EnablePackageUsage(bool bEnable = true);

private:
    UE_API void OnBuildDiagnostic(const TSRef<SGlitch>& Diagnostic);
    UE_API void OnBuildStatistic(const SBuildEventInfo& EventInfo);

    TSRef<CToolchain> _Toolchain;
    SProgramContext _ProgramContext;
    TSRef<CSourceProject> _SourceProject;
    TUPtr<SPackageUsage> _PackageUsage;
    bool bEnablePackageUsage = false;
};

} // namespace uLang

#undef UE_API

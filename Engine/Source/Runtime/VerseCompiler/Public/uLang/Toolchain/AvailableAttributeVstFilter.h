// Copyright Epic Games, Inc. All Rights Reserved.
// uLang Version attribute filter 

#pragma once

#include "uLang/Common/Containers/SharedPointer.h"
#include "uLang/CompilerPasses/IPostVstFilter.h"

#define UE_API VERSECOMPILER_API

namespace uLang
{

struct SBuildVersionInfo
{
    uint32_t UploadedAtFNVersion;
};

// Filter out any symbol with a version attribute that would exclude it from the compile
class CAvailableAttributeVstFilter final : public IPostVstFilter
{
public:
    CAvailableAttributeVstFilter() {}

    //~ Begin IPostVstFilter interface
    virtual void Filter(const TSRef<Verse::Vst::Snippet>& VstSnippet, const SBuildContext& BuildContext) override
    {
        StaticFilter(VstSnippet.As<Verse::Vst::Node>(), BuildContext);
    }
    //~ End IPostVstFilter interface

    static UE_API void StaticFilter(const TSRef<Verse::Vst::Node>& VstNode, const SBuildContext& BuildContext);

private:
    static UE_API void StaticFilterHelper(const TSRef<Verse::Vst::Node>& VstNode,
        const SBuildContext& BuildContext,
        const SBuildVersionInfo& BuildVersion);
};
}

#undef UE_API

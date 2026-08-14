// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "uLang/CompilerPasses/IParserPass.h"

#define UE_API VERSECOMPILER_API

namespace uLang
{
class CParserPass : public IParserPass
{
public:
    //~ Begin IParserPass interface
    UE_API virtual void ProcessSnippet(const uLang::TSRef<Verse::Vst::Snippet>& OutVst, const CUTF8StringView& TextSnippet, const SBuildContext& BuildContext, const uint32_t VerseVersion, const uint32_t UploadedAtFNVersion) const override;
    //~ End IParserPass interface
};

} // namespace uLang

#undef UE_API

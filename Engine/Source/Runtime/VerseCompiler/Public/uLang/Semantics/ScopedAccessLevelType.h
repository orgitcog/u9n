// Copyright Epic Games, Inc. All Rights Reserved.
// uLang Compiler Public API

#pragma once

#include "uLang/Semantics/SemanticTypes.h"
#include "uLang/Semantics/SemanticClass.h"

#define UE_API VERSECOMPILER_API

namespace uLang
{
    class CScopedAccessLevelDefinition;
    class CExprScopedAccessLevelDefinition;

    /**
     * AccessLevelDefinition type
     **/
    // HACK! This is convoluted, but attributes need to be CClass types because the semantic attribute processing demands it right now
    // HACK! CClass expects its associated definition to be a CClassDefinition type, so our CScopedAccessLevelDefinition also needs to be a CClassDefinition type
    // HACK! Ordinarily, we could just use CClassDefinition directly without this extra child type, except the CClassDefinition linkage to the AST demands
    // HACK! that the CExpr* type be CExprClassDefinition even though it ultimately relaxes to CExpressionBase.
    class CScopedAccessLevelDefinition : public CClassDefinition
    {
    public:
        UE_API CScopedAccessLevelDefinition(TOptional<CSymbol> ClassName, CScope& EnclosingScope);

        TArray<const CScope*> _Scopes;
        bool _IsAnonymous;

        // CDefinition interface glue.
        UE_API void SetAstNode(CExprScopedAccessLevelDefinition* AstNode);
        UE_API CExprScopedAccessLevelDefinition* GetAstNode() const;

        UE_API void SetIrNode(CExprScopedAccessLevelDefinition* AstNode);
        UE_API CExprScopedAccessLevelDefinition* GetIrNode(bool bForce = false) const;

        UE_API virtual CUTF8String AsCodeRecursive(ETypeSyntaxPrecedence OuterPrecedence, TArray<const CFlowType*>& VisitedFlowTypes, bool bLinkable, ETypeStringFlag Flag) const override;
    };
}

#undef UE_API

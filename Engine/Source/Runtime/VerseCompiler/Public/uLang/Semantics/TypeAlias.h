// Copyright Epic Games, Inc. All Rights Reserved.
// uLang Compiler Public API

#pragma once

#include "uLang/Common/Common.h"
#include "uLang/Semantics/Definition.h"
#include "uLang/Semantics/Expression.h"
#include "uLang/Semantics/SemanticProgram.h"
#include "uLang/Semantics/SemanticTypes.h"

namespace uLang
{
class CTypeAlias;
enum class EPathMode : uint8_t;

/** Encodes a type alias as a non-normal CTypeBase. */
class CAliasType : public CTypeBase
{
public:

    CAliasType(CTypeAlias& Definition, const CTypeBase* AliasedType) : CTypeBase(AliasedType->GetProgram()), _Definition(Definition), _AliasedType(AliasedType)
    {}

    CTypeAlias& GetDefinition() const { return _Definition; }
    const CTypeBase* GetAliasedType() const { return _AliasedType; }

    // CTypeBase interface.
    virtual const CNormalType& GetNormalType() const override
    {
        return _AliasedType->GetNormalType();
    }

    virtual const CAliasType* AsAliasType() const override
    {
        return this;
    }

    virtual bool CanBeCustomAccessorDataType() const override { return _AliasedType->CanBeCustomAccessorDataType(); }
    virtual bool CanBePredictsVarDataType() const override { return _AliasedType->CanBePredictsVarDataType(); }

    virtual CUTF8String AsCodeRecursive(ETypeSyntaxPrecedence OuterPrecedence, TArray<const CFlowType*>& VisitedFlowTypes, bool bLinkable, ETypeStringFlag Flag) const override
    {
        return _AliasedType->AsCodeRecursive(OuterPrecedence, VisitedFlowTypes, bLinkable, Flag);
    }

private:

    CTypeAlias& _Definition;
    const CTypeBase* _AliasedType;
};

/** Maps a name to a type */
class CTypeAlias : public CDefinition
{
public:
    static const CDefinition::EKind StaticDefinitionKind = CDefinition::EKind::TypeAlias;

    CTypeAlias(const CSymbol& Name, CScope& EnclosingScope)
        : CDefinition(StaticDefinitionKind, EnclosingScope, Name)
    {
    }

    bool IsInitialized() const
    {
        return _TypeType != nullptr;
    }

    const CTypeType* GetTypeType() const
    {
        return _TypeType;
    }

    const CTypeBase* GetType() const
    {
        if (!_PositiveAliasType)
        {
            return nullptr;
        }
        return _PositiveAliasType.Get();
    }
    
    const CTypeBase* GetPositiveAliasedType() const
    {
        ULANG_ASSERT(_PositiveAliasType);
        return _PositiveAliasType->GetAliasedType();
    }

    void InitType(const CTypeBase* NegativeAliasedType, const CTypeBase* PositiveAliasedType)
    {
        ULANG_ASSERT(!_TypeType);
        _PositiveAliasType = TURef<CAliasType>::New(*this, PositiveAliasedType);
        const CAliasType* NegativeAliasType = _PositiveAliasType.Get();
        if (PositiveAliasedType != NegativeAliasedType)
        {
            _NegativeAliasType = TURef<CAliasType>::New(*this, NegativeAliasedType);
            NegativeAliasType = _NegativeAliasType.Get();
        }
        _TypeType = &_EnclosingScope.GetProgram().GetOrCreateTypeType(NegativeAliasType, _PositiveAliasType.Get());
    }

    // CDefinition interface.
    void SetAstNode(CExprTypeAliasDefinition* AstNode) { CDefinition::SetAstNode(AstNode); }
    CExprTypeAliasDefinition* GetAstNode() const { return static_cast<CExprTypeAliasDefinition*>(CDefinition::GetAstNode()); }

    void SetIrNode(CExprTypeAliasDefinition* AstNode) { CDefinition::SetIrNode(AstNode); }
    CExprTypeAliasDefinition* GetIrNode(bool bForce = false) const { return static_cast<CExprTypeAliasDefinition*>(CDefinition::GetIrNode(bForce)); }

    virtual bool IsPersistenceCompatConstraint() const override { return false; }

    CUTF8String GetPath(const uLang::UTF8Char SeparatorChar, const uLang::EPathMode Mode) const
    {
        CUTF8String EnclosingScopePath = _EnclosingScope.GetScopePath(SeparatorChar, Mode);
        if (EnclosingScopePath.IsEmpty())
        {
            return AsNameStringView();
        }
        return CUTF8String("%s%c%s", *EnclosingScopePath, SeparatorChar, AsNameCString());
    }

private:

    const CTypeType* _TypeType{ nullptr };

    TUPtr<CAliasType> _NegativeAliasType;
    TUPtr<CAliasType> _PositiveAliasType;
};
}

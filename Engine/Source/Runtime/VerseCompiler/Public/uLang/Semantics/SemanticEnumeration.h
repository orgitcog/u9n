// Copyright Epic Games, Inc. All Rights Reserved.
// uLang Compiler Public API

#pragma once

#include "uLang/Common/Common.h"
#include "uLang/Semantics/Attributable.h"
#include "uLang/Semantics/Definition.h"
#include "uLang/Semantics/MemberOrigin.h"
#include "uLang/Semantics/SemanticScope.h"
#include "uLang/Semantics/SemanticTypes.h"
#include "uLang/Semantics/SmallDefinitionArray.h"
#include "uLang/Semantics/VisitStamp.h"

#define UE_API VERSECOMPILER_API

namespace uLang
{
class CEnumeration;
class CExprEnumLiteral;
class CExprEnumDefinition;

/**
 * Description for a single enumerator
 **/
class CEnumerator : public CDefinition
{
public:

    static constexpr EKind StaticDefinitionKind = EKind::Enumerator;

    /// The integer value denoting this enumerator for native representation.
    const int32_t _IntValue;

    /// Type this enumerator belongs to
    CEnumeration* _Enumeration{nullptr};

    UE_API CEnumerator(CEnumeration& Enumeration, const CSymbol& Name, int32_t Value);

    UE_API CUTF8String AsCode() const;

    // CDefinition interface.
    UE_API void SetAstNode(CExprEnumLiteral* AstNode);
    UE_API CExprEnumLiteral* GetAstNode() const;

    UE_API void SetIrNode(CExprEnumLiteral* AstNode);
    UE_API CExprEnumLiteral* GetIrNode(bool bForce = false) const;

    UE_API virtual bool IsPersistenceCompatConstraint() const override;
};


/**
 * Enumeration type
 * @jira SOL-1013 : Make enums derive from Class?
 **/
class CEnumeration : public CDefinition, public CLogicalScope, public CNominalType
{
public:
    static const ETypeKind StaticTypeKind = ETypeKind::Enumeration;
    static const CDefinition::EKind StaticDefinitionKind = CDefinition::EKind::Enumeration;

    CAttributable _EffectAttributable;

    UE_API CEnumeration(const CSymbol& Name, CScope& EnclosingScope);

    UE_API CEnumerator& CreateEnumerator(const CSymbol& EnumeratorName, int32_t Value);

    // CTypeBase interface.
    using CTypeBase::GetProgram;
    UE_API virtual SmallDefinitionArray FindTypeMember(const CSymbol& Name, EMemberOrigin Origin, const SQualifier& Qualifier = SQualifier::Unknown(), VisitStampType VisitStamp = CScope::GenerateNewVisitStamp()) const override;
    virtual EComparability GetComparability() const override { return EComparability::ComparableAndHashable; }
    UE_API virtual bool IsPersistable() const override;
    virtual bool CanBeCustomAccessorDataType() const override { return true; };

    // CNominalType interface.
    virtual const CDefinition* Definition() const override { return this; }

    // CScope interface.
    virtual CSymbol GetScopeName() const override { return GetName(); }
    virtual const CTypeBase* ScopeAsType() const override { return this; }
    virtual const CDefinition* ScopeAsDefinition() const override { return this; }
    UE_API virtual SAccessLevel GetDefaultDefinitionAccessLevel() const override;

    // CDefinition interface.
    UE_API void SetAstNode(CExprEnumDefinition* AstNode);
    UE_API CExprEnumDefinition* GetAstNode() const;

    UE_API void SetIrNode(CExprEnumDefinition* AstNode);
    UE_API CExprEnumDefinition* GetIrNode(bool bForce = false) const;

    virtual const CLogicalScope* DefinitionAsLogicalScopeNullable() const override { return this; }

    virtual bool IsPersistenceCompatConstraint() const override { return IsPersistable(); }

    bool IsOpen() const { return GetOpenness() == EEnumOpenness::Open; }
    bool IsClosed() const { return GetOpenness() == EEnumOpenness::Closed; }

    enum class EEnumOpenness
    {
        Closed,
        Open,
        Invalid,
    };

    UE_API EEnumOpenness GetOpenness() const;
};

}

#undef UE_API

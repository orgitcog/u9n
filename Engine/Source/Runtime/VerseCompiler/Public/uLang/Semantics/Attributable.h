// Copyright Epic Games, Inc. All Rights Reserved.
// uLang Compiler Public API

#pragma once

#include "uLang/Common/Common.h"
#include "uLang/Common/Containers/SharedPointerArray.h"
#include "uLang/Common/Text/UTF8String.h"

#define UE_API VERSECOMPILER_API

namespace uLang
{
class CClass;
class CExpressionBase;
class CFunction;
class CSemanticProgram;

struct SAttribute
{
    enum class EType
    {
        Attribute, // Attribute which is used with prefix @attr syntax
        Specifier  // Attribute which is used with suffix <spec> syntax
    };

    TSRef<CExpressionBase> _Expression;
    EType _Type;

    VERSECOMPILER_API TOptional<CUTF8String> GetTextValue() const;
};

VERSECOMPILER_API bool IsAttributeHack(const SAttribute&, const CClass* AttributeClass, const CSemanticProgram&);
VERSECOMPILER_API bool IsAttributeHack(const SAttribute&, const CFunction* AttributeFunction, const CSemanticProgram&);

template <typename TIterator, typename ClassOrFunction>
TIterator FindAttributeHack(TIterator First, TIterator Last, const ClassOrFunction* AttributeClassOrFunction, const CSemanticProgram& Program)
{
    for (; First != Last; ++First)
    {
        if (IsAttributeHack(*First, AttributeClassOrFunction, Program))
        {
            break;
        }
    }
    return First;
}

/// Base class for everything that can have attributes attached to it (classes, expressions, etc.)
class CAttributable
{
public:
    TArray<SAttribute> _Attributes;

    bool HasAttributes() const { return _Attributes.Num() > 0; }
    UE_API bool HasAttributeClass(const CClass* AttributeClass, const CSemanticProgram& Program) const;

    UE_API int32_t GetAttributeClassCount(const CClass* AttributeClass, const CSemanticProgram& Program) const;

    UE_API TArray<const CExpressionBase*> GetAttributesWithAttribute(const CClass* AttributeClass, const CSemanticProgram& Program) const;

    UE_API const CExpressionBase* FindAttributeExpr(const CClass* AttributeClass, const CSemanticProgram& Program) const;
    UE_API const TArray<CExpressionBase*> FindAttributeExprs(const CClass* AttributeClass, const CSemanticProgram& Program) const;

    UE_API TOptional<SAttribute> FindAttribute(const CClass* AttributeClass, const CSemanticProgram& Program) const;
    UE_API TArray<SAttribute> FindAttributes(const CClass* AttributeClass, const CSemanticProgram& Program) const;

    UE_API void AddAttributeClass(const CClass* AttributeClass);
    UE_API void AddAttribute(SAttribute Attribute);
    UE_API void RemoveAttributeClass(const CClass* AttributeClass, const CSemanticProgram& Program);

    // @HACK: SOL-972, We need full proper support for compile-time evaluation of attribute types
    UE_API TOptional<CUTF8String> GetAttributeTextValue(const CClass* AttributeClass, const CSemanticProgram& Program) const;
    static UE_API TOptional<CUTF8String> GetAttributeTextValue(const TArray<SAttribute>& Attributes, const CClass* AttributeClass, const CSemanticProgram& Program);

    // Specifies the attribute scope for expressions (e.g. should only respect class-scoped attributes, etc.)
    enum class EAttributableScope : uint8_t
    {
        Module,
        Class,
        Struct,
        Data,
        Function,
        Enum,
        Enumerator,
        AttributeClass,
        Interface,
        Expression,
        TypeDefinition,
        ScopedAccessLevel,
        ClassTypeFunction,
        AttributeClassTypeFunction,
        InterfaceTypeFunction,
    };

    CAttributable& operator=(const CAttributable& Other) = delete;
    CAttributable& operator=(CAttributable&& Other) = delete;

    UE_API bool HasAttributeClassHack(const CClass* AttributeClass, const CSemanticProgram&) const;
    UE_API bool HasAttributeFunctionHack(const CFunction* AttributeFunction, const CSemanticProgram&) const;

private:
    UE_API TOptional<int32_t> FindAttributeImpl(const CClass* AttributeClass, const CSemanticProgram& Program) const;
    UE_API TArray<int32_t> FindAttributesImpl(const CClass* AttributeClass, const CSemanticProgram& Program) const;
};
}

#undef UE_API

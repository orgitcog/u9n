// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_BPVM
#include "Containers/ContainersFwd.h"
#include "Containers/StringFwd.h"
#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/DynamicallyTypedValue.h"
#include <atomic>

// Forward declarations.
class UObject;

namespace UE::Verse
{
using ::GetTypeHash;

#define VERSE_ENUM_RUNTIME_TYPE_KINDS(v) \
	v(Dynamic)                           \
	v(Logic)                             \
	v(Float)                             \
	v(Char8)                             \
	v(Char32)                            \
	v(Int64)                             \
	v(Rational)                          \
	v(String)                            \
	v(Class)                             \
	v(Object)                            \
	v(Function)                          \
	v(Option)                            \
	v(Reference)                         \
	v(Array)                             \
	v(Map)                               \
	v(Tuple)                             \
	v(Struct)                            \
	v(Enumeration)

struct FRuntimeType : FDynamicallyTypedValueType
{
	enum class EKind : uint8
	{
#define VISIT_KIND(Name) Name,
		VERSE_ENUM_RUNTIME_TYPE_KINDS(VISIT_KIND)
#undef VISIT_KIND
	};

	const EKind Kind;

	FRuntimeType(EKind InKind, SIZE_T InNumBytes, uint8 InMinAlignmentLogTwo, EContainsReferences InContainsReferences)
		: FDynamicallyTypedValueType(InNumBytes, InMinAlignmentLogTwo, InContainsReferences)
		, Kind(InKind)
	{
		checkf(!(GetNumBytes() & (GetMinAlignment() - 1)), TEXT("Misaligned runtime type: kind %u, %zu bytes, %u alignment"), static_cast<uint32>(InKind), GetNumBytes(), GetMinAlignment());
	}

	virtual ~FRuntimeType() {}

	virtual void AppendDiagnosticString(FUtf8StringBuilderBase& Builder, const void* Data, uint32 RecursionDepth) const = 0;

	virtual void MarkReachable(FReferenceCollector& Collector) override
	{
		bIsReachable.store(true, std::memory_order_relaxed);
	}
	void UnmarkReachable()
	{
		bIsReachable.store(false, std::memory_order_relaxed);
	}
	bool IsReachable() const
	{
		return bIsReachable.load(std::memory_order_relaxed);
	}

	friend uint32 GetTypeHash(const FRuntimeType& Type)
	{
		return PointerHash(&Type);
	}
	friend uint32 GetTypeHash(const TArray<FRuntimeType*>& Types)
	{
		uint32 CombinedHash = ::GetTypeHash(Types.Num());
		for (FRuntimeType* Type : Types)
		{
			CombinedHash = HashCombine(CombinedHash, GetTypeHash(*Type));
		}
		return CombinedHash;
	}
	friend bool operator==(FRuntimeType& Lhs, FRuntimeType& Rhs)
	{
		return &Lhs == &Rhs;
	}

	virtual bool AreEquivalent(const void* DataA, const FRuntimeType& TypeB, const void* DataB) const = 0;
	virtual void ExportValueToText(FString& OutputString, const void* Data, const void* DefaultData, UObject* Parent, UObject* ExportRootScope) const = 0;
	virtual bool ImportValueFromText(const TCHAR*& InputString, void* Data, UObject* Parent, FOutputDevice* ErrorText) const = 0;

	virtual void InstanceSubobjects(void* Data, void const* DefaultData, TNotNull<UObject*> Owner, FObjectInstancingGraph* InstanceGraph) const {}

	virtual bool IsValid(const void* Data) const { return true; }

	virtual bool HasIntrusiveUnsetOptionalState() const { return false; }
	virtual void InitializeIntrusiveUnsetOptionalValue(void* Data) const { check(false); }
	virtual bool IsIntrusiveOptionalValueSet(const void* Data) const
	{
		check(false);
		return true;
	}

private:
	std::atomic<bool> bIsReachable{false};
};

template <typename>
struct TRuntimeTypeTraits;

#define VERSE_DEFINE_NONPARAMETRIC_RUNTIME_TYPE(RuntimeTypeType, CType) \
	template <>                                                         \
	struct TRuntimeTypeTraits<CType>                                    \
	{                                                                   \
		static RuntimeTypeType& GetType()                               \
		{                                                               \
			return RuntimeTypeType::Get();                              \
		}                                                               \
	};

// Implement the static Get functions for the global types.
#define VERSE_IMPLEMENT_GLOBAL_RUNTIME_TYPE(RuntimeTypeClass) \
	RuntimeTypeClass& RuntimeTypeClass::Get()                 \
	{                                                         \
		static RuntimeTypeClass StaticType;                   \
		return StaticType;                                    \
	}

COREUOBJECT_API bool AreEquivalent(const FRuntimeType& TypeA, const void* DataA, const FDynamicallyTypedValue& ValueB);

struct FRuntimeTypeDynamic : FRuntimeType
{
	static COREUOBJECT_API FRuntimeTypeDynamic& Get();

	// Verse::FRuntimeType interface.
	COREUOBJECT_API virtual void AppendDiagnosticString(FUtf8StringBuilderBase& Builder, const void* Data, uint32 RecursionDepth) const override;

	// FDynamicallyTypedValueType interface.
	COREUOBJECT_API virtual void MarkValueReachable(void* Data, FReferenceCollector& Collector) const override;

	COREUOBJECT_API virtual void InitializeValue(void* Data) const override;
	COREUOBJECT_API virtual void InitializeValueFromCopy(void* DestData, const void* SourceData) const override;
	COREUOBJECT_API virtual void DestroyValue(void* Data) const override;

	COREUOBJECT_API virtual void SerializeValue(FStructuredArchive::FSlot Slot, void* Data, const void* DefaultData) const override;

	COREUOBJECT_API virtual void ExportValueToText(FString& OutputString, const void* Data, const void* DefaultData, UObject* Parent, UObject* ExportRootScope) const override;
	COREUOBJECT_API virtual bool ImportValueFromText(const TCHAR*& InputString, void* Data, UObject* Parent, FOutputDevice* ErrorText) const override;

	COREUOBJECT_API virtual uint32 GetValueHash(const void* Data) const override;
	COREUOBJECT_API virtual bool AreIdentical(const void* DataA, const void* DataB) const override;
	COREUOBJECT_API virtual bool AreEquivalent(const void* DataA, const FRuntimeType& TypeB, const void* DataB) const override;

	COREUOBJECT_API virtual void InstanceSubobjects(void* Data, void const* DefaultData, TNotNull<UObject*> Owner, FObjectInstancingGraph* InstanceGraph) const override;

	COREUOBJECT_API virtual bool IsValid(const void* Data) const;

private:
	FRuntimeTypeDynamic()
		: FRuntimeType(
			FRuntimeType::EKind::Dynamic,
			sizeof(FDynamicallyTypedValue),
			UE_FORCE_CONSTEVAL(FMath::ConstExprCeilLogTwo(alignof(FDynamicallyTypedValue))),
			EContainsReferences::Maybe)
	{
	}
};
VERSE_DEFINE_NONPARAMETRIC_RUNTIME_TYPE(FRuntimeTypeDynamic, FDynamicallyTypedValue)

} // namespace UE::Verse
#endif // WITH_VERSE_BPVM

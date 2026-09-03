// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "Math/MathFwd.h"
#include "Serialization/ArchiveCrc32.h"

class FHLODHashBuilder : public FArchiveCrc32
{
public:
	/** Push a new context, logging with correct indentation */
	void PushContext(const TCHAR* Context);

	/** Pop context, ensuring indentation decreases */
	void PopContext();

	template <typename TParamType>
	typename TEnableIf<TIsIntegral<TParamType>::Value, FArchive&>::Type operator<<(TParamType InValue)
	{
		FArchive& Self = *this;
		return Self << InValue;
	}

	FArchive& operator<<(FTransform InTransform);
	FArchive& operator<<(class UMaterialInterface* InMaterialInterface);
	FArchive& operator<<(class UTexture* InTexture);
	FArchive& operator<<(class UStaticMesh* InStaticMesh);
	FArchive& operator<<(class USkinnedAsset* InSkinnedAsset);

	//~ Begin FArchive Interface
	virtual FArchive& operator<<(class UObject*& Object) override;
	//~ End FArchive Interface

	void LogContext(const TCHAR* Context, bool bOutputHash);

	// For visibility of the overloads we don't override
	using FArchiveCrc32::operator<<;

private:
	int32 IndentationLevel = 0; // Track indentation depth
};

// Templated operator overloads for handling arrays
template <typename TElementType, typename TAllocatorType>
FArchive& operator<<(FHLODHashBuilder& Ar, const TArray<TElementType, TAllocatorType>& InArray)
{
	TArray<TElementType, TAllocatorType>& ArrayMutable = const_cast<TArray<TElementType, TAllocatorType>&>(InArray);
	FArchive& Self = Ar;
	return Self << ArrayMutable;
}

template <typename TElementType, typename TAllocatorType>
FArchive& operator<<(FHLODHashBuilder& Ar, TArray<TElementType, TAllocatorType>& InArray)
{
	FArchive& Self = Ar;
	return Self << InArray;
}

class FHLODHashContext
{
public:
	template <typename... TArgs>
	explicit FHLODHashContext(UE::Core::TCheckedFormatString<FString::FmtCharType, TArgs...> Format, TArgs... Args)
		: Context(FString::Printf(Format, Args...)) {}

	explicit FHLODHashContext(FName InName)
		: Context(InName.ToString()) {}

	const TCHAR* GetContext() const { return *Context; }

	/** Overload for FHLODHashContext to log structured properties */
	friend FArchive& operator<<(FArchive& Ar, FHLODHashContext InContext)
	{
		static_cast<FHLODHashBuilder&>(Ar).LogContext(InContext.GetContext(), true);
		return Ar;
	}

private:
	FString Context;
};

class FHLODHashScope
{
public:
	FHLODHashScope(FHLODHashBuilder& InBuilder, const TCHAR* Format)
		: Builder(InBuilder)
	{
		Builder.PushContext(Format);
	}

	template <typename... Args>
	FHLODHashScope(FHLODHashBuilder& InBuilder, const TCHAR* Format, Args... Arguments)
		: Builder(InBuilder)
	{
		Builder.PushContext(*FString::Printf(Format, Arguments...));
	}

	~FHLODHashScope()
	{
		Builder.PopContext();
	}

private:
	FHLODHashBuilder& Builder;
};

#endif

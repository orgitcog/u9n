// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructUtils/PropertyBag.h"
#include "AnimNextDataInterfacePayload.generated.h"

class UAnimNextAnimGraphSettings;

// Combined native & non-native payloads, for supplying to a data interface instance
USTRUCT()
struct FAnimNextDataInterfacePayload
{
	GENERATED_BODY()

	FAnimNextDataInterfacePayload() = default;

	FAnimNextDataInterfacePayload(const FAnimNextDataInterfacePayload& InOther)
	{
		OwnedPayload = InOther.OwnedPayload;
		OwnedNativePayloads = InOther.OwnedNativePayloads;
		NonOwnedPayloads = InOther.NonOwnedPayloads;
		bCombinedPayloadsDirty = true;
	}

	FAnimNextDataInterfacePayload& operator=(const FAnimNextDataInterfacePayload& InOther)
	{
		OwnedPayload = InOther.OwnedPayload;
		OwnedNativePayloads = InOther.OwnedNativePayloads;
		NonOwnedPayloads = InOther.NonOwnedPayloads;
		bCombinedPayloadsDirty = true;
		return *this;
	}

	FAnimNextDataInterfacePayload(FAnimNextDataInterfacePayload&& InOther)
	{
		OwnedPayload = MoveTemp(InOther.OwnedPayload);
		OwnedNativePayloads = MoveTemp(InOther.OwnedNativePayloads);
		NonOwnedPayloads = MoveTemp(InOther.NonOwnedPayloads);
		bCombinedPayloadsDirty = true;

		InOther.CombinedPayloads.Empty();
		InOther.bCombinedPayloadsDirty = false;
	}

	FAnimNextDataInterfacePayload& operator=(FAnimNextDataInterfacePayload&& InOther)
	{
		OwnedPayload = MoveTemp(InOther.OwnedPayload);
		OwnedNativePayloads = MoveTemp(InOther.OwnedNativePayloads);
		NonOwnedPayloads = MoveTemp(InOther.NonOwnedPayloads);
		bCombinedPayloadsDirty = true;

		InOther.CombinedPayloads.Empty();
		InOther.bCombinedPayloadsDirty = false;
		return *this;
	}

	// Get all payloads
	TArrayView<FStructView> Get()
	{
		UpdateCombinedPayloads();
		return CombinedPayloads;
	}

	// Set payload
	void Set(FInstancedPropertyBag&& InPayload)
	{
		OwnedPayload = MoveTemp(InPayload);
		bCombinedPayloadsDirty = true;
	}

	// Set payload
	void Set(const FInstancedPropertyBag& InPayload)
	{
		OwnedPayload = InPayload;
		bCombinedPayloadsDirty = true;
	}

	// Set native payloads
	void SetNative(TArray<FInstancedStruct>&& InNativePayloads)
	{
		OwnedNativePayloads = MoveTemp(InNativePayloads);
		bCombinedPayloadsDirty = true;
	}

	// Append native payloads
	void AppendNative(const TArray<FInstancedStruct>& InNativePayloads)
	{
		OwnedNativePayloads.Append(InNativePayloads);
		bCombinedPayloadsDirty = true;
	}

	// Add a native payload
	void AddNative(FInstancedStruct&& InNativePayload)
	{
		OwnedNativePayloads.Add(MoveTemp(InNativePayload));
		bCombinedPayloadsDirty = true;
	}

	// Add a native payload
	void AddNative(const FInstancedStruct& InNativePayload)
	{
		OwnedNativePayloads.Add(InNativePayload);
		bCombinedPayloadsDirty = true;
	}

	// Add non-owned payload
	void AddStructView(const FStructView& InPayload)
	{
		NonOwnedPayloads.Add(InPayload);
		bCombinedPayloadsDirty = true;
	}

	// Append non-owned payloads
	void AppendStructView(const TArray<FStructView>& InPayloads)
	{
		NonOwnedPayloads.Append(InPayloads);
		bCombinedPayloadsDirty = true;
	}

	const FInstancedPropertyBag& GetPayload() const
	{
		return OwnedPayload;
	}

	TConstArrayView<FInstancedStruct> GetNativePayloads() const
	{
		return OwnedNativePayloads;
	}

	TConstArrayView<FStructView> GetNonOwnedPayloads() const
	{
		return NonOwnedPayloads;
	}
	
#if WITH_EDITOR
	// Details customization support
	static FName GetOwnedPayloadPropertyName()
	{
		return GET_MEMBER_NAME_CHECKED(FAnimNextDataInterfacePayload, OwnedPayload);
	}

	// Details customization support
	static FName GetOwnedNativePayloadsPropertyName()
	{
		return GET_MEMBER_NAME_CHECKED(FAnimNextDataInterfacePayload, OwnedNativePayloads);
	}
#endif

private:
	// Lazily refresh combined payloads
	void UpdateCombinedPayloads()
	{
		if(bCombinedPayloadsDirty)
		{
			CombinedPayloads.Reset();
			CombinedPayloads.Reserve(OwnedNativePayloads.Num() + NonOwnedPayloads.Num() + 1);
			if(OwnedPayload.IsValid())
			{
				CombinedPayloads.Add(OwnedPayload.GetMutableValue());
			}
			for(FInstancedStruct& InstancedStruct : OwnedNativePayloads)
			{
				CombinedPayloads.Add(InstancedStruct);
			}
			for(FStructView& StructView : NonOwnedPayloads)
			{
				CombinedPayloads.Add(StructView);
			}

			bCombinedPayloadsDirty = false;
		}
	}

private:
	friend UAnimNextAnimGraphSettings;

	// Single payload used for data-driven variable bindings
	UPROPERTY(EditAnywhere, Category = Payload)
	FInstancedPropertyBag OwnedPayload;

	// Multiple payloads used for native bindings
	UPROPERTY(EditAnywhere, Category = Payload)
	TArray<FInstancedStruct> OwnedNativePayloads;

	// Externally-owned payloads used to avoid copying constant data
	TArray<FStructView> NonOwnedPayloads;

	// Combined view of all payloads used for binding
	TArray<FStructView> CombinedPayloads;

	// dirty flag for lazy rebuilds of CombinedPayloads
	bool bCombinedPayloadsDirty = false;
};
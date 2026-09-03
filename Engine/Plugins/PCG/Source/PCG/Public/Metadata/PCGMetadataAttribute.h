// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGMetadataCommon.h"

#include "Utils/PCGValueRange.h"

#define UE_API PCG_API

class FPCGMetadataDomain;
class UPCGMetadata;

namespace PCGMetadataAttributeConstants
{
	const FName LastAttributeName = TEXT("@Last");
	const FName LastCreatedAttributeName = TEXT("@LastCreated");
	const FName SourceAttributeName = TEXT("@Source");
	const FName SourceNameAttributeName = TEXT("@SourceName");
}

class FPCGMetadataAttributeBase
{
public:
	FPCGMetadataAttributeBase() = default;
	UE_API FPCGMetadataAttributeBase(FPCGMetadataDomain* InMetadata, FName InName, const FPCGMetadataAttributeBase* InParent, bool bInAllowsInterpolation);

	UE_DEPRECATED(5.6, "Use the version with FPCGMetadataDomain")
	UE_API FPCGMetadataAttributeBase(UPCGMetadata* InMetadata, FName InName, const FPCGMetadataAttributeBase* InParent, bool bInAllowsInterpolation);

	virtual ~FPCGMetadataAttributeBase() = default;
	
	UE_API virtual void Serialize(FPCGMetadataDomain* InMetadata, FArchive& InArchive);

	UE_DEPRECATED(5.6, "Use the version with FPCGMetadataDomain")
	UE_API virtual void Serialize(UPCGMetadata* InMetadata, FArchive& InArchive);

	/** Unparents current attribute by flattening the values, entries, etc. */
	virtual void Flatten() = 0;
	/** Unparents current attribute by flattening the values, entries, etc while only keeping the entries referenced in InEntryKeysToKeep. There must be NO invalid entry keys. */
	virtual void FlattenAndCompress(const TArrayView<const PCGMetadataEntryKey>& InEntryKeysToKeep) = 0;

	/** Remove all entries, values and parenting. */
	virtual void Reset() = 0;
	
	UE_API const UPCGMetadata* GetMetadata() const;
	const FPCGMetadataDomain* GetMetadataDomain() const { return Metadata; }
	
	int16 GetTypeId() const { return TypeId; }

	virtual FPCGMetadataAttributeBase* Copy(FName NewName, FPCGMetadataDomain* InMetadata, bool bKeepParent, bool bCopyEntries = true, bool bCopyValues = true) const = 0;

	UE_DEPRECATED(5.6, "Use the version with FPCGMetadataDomain")
	virtual FPCGMetadataAttributeBase* Copy(FName NewName, UPCGMetadata* InMetadata, bool bKeepParent, bool bCopyEntries = true, bool bCopyValues = true) const = 0;
	
	virtual FPCGMetadataAttributeBase* CopyToAnotherType(int16 Type) const = 0;

	virtual PCGMetadataValueKey GetValueKeyOffsetForChild() const = 0;
	virtual void SetValue(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttribute, PCGMetadataEntryKey InEntryKey) = 0;
	virtual void SetZeroValue(PCGMetadataEntryKey ItemKey) = 0;
	virtual void AccumulateValue(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, float Weight) = 0;
	virtual void SetWeightedValue(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttribute, const TArrayView<const TPair<PCGMetadataEntryKey, float>>& InWeightedKeys) = 0;
	virtual void SetValue(PCGMetadataEntryKey ItemKey, const FPCGMetadataAttributeBase* InAttributeA, PCGMetadataEntryKey InEntryKeyA, const FPCGMetadataAttributeBase* InAttributeB, PCGMetadataEntryKey InEntryKeyB, EPCGMetadataOp Op) = 0;
	virtual bool IsEqualToDefaultValue(PCGMetadataValueKey ValueKey) const = 0;
	/** In the case of multi entry attribute and after some operations, we might have a single entry attribute with a default value that is different than the first entry. Use this function to fix that. Only valid if there is one and only one value. */
	virtual void SetDefaultValueToFirstEntry() = 0;

	virtual bool UsesValueKeys() const = 0;
	virtual bool AreValuesEqualForEntryKeys(PCGMetadataEntryKey EntryKey1, PCGMetadataEntryKey EntryKey2) const = 0;
	virtual bool AreValuesEqual(PCGMetadataValueKey ValueKey1, PCGMetadataValueKey ValueKey2) const = 0;

	UE_API void SetValueFromValueKey(PCGMetadataEntryKey EntryKey, PCGMetadataValueKey ValueKey, bool bResetValueOnDefaultValueKey = false);
	UE_API PCGMetadataValueKey GetValueKey(PCGMetadataEntryKey EntryKey) const;
	UE_API bool HasNonDefaultValue(PCGMetadataEntryKey EntryKey) const;
	UE_API void ClearEntries();

	/** Bulk getter, to lock in read only once per parent. */
	UE_API void GetValueKeys(const TArrayView<const PCGMetadataEntryKey>& EntryKeys, TArray<PCGMetadataValueKey>& OutValueKeys) const;

	/** Bulk getter, to lock in read only once per parent. */
	UE_API void GetValueKeys(TConstPCGValueRange<PCGMetadataEntryKey> EntryKeys, TArray<PCGMetadataValueKey>& OutValueKeys) const;

	/** Optimized version that take ownership on the Entries passed.*/
	UE_API void GetValueKeys(TArrayView<PCGMetadataEntryKey> EntryKeys, TArray<PCGMetadataValueKey>& OutValueKeys) const;

	/** Optimized version that take ownership on the Entries passed.*/
	UE_API void GetValueKeys(TPCGValueRange<PCGMetadataEntryKey> EntryKeys, TArray<PCGMetadataValueKey>& OutValueKeys) const;

	/** Bulk setter to lock in write only once. */
	UE_API void SetValuesFromValueKeys(const TArrayView<const TTuple<PCGMetadataEntryKey, PCGMetadataValueKey>>& EntryValuePairs, bool bResetValueOnDefaultValueKey = true);

	/** Two arrays version of bulk setter to lock in write only once. Both arrays must be the same size. */
	UE_API void SetValuesFromValueKeys(const TArrayView<const PCGMetadataEntryKey>& EntryKeys, const TArrayView<const PCGMetadataValueKey>& ValueKeys, bool bResetValueOnDefaultValueKey = true);
	UE_API void SetValuesFromValueKeys(const TArrayView<const PCGMetadataEntryKey* const>& EntryKeys, const TArrayView<const PCGMetadataValueKey>& ValueKeys, bool bResetValueOnDefaultValueKey = true);

	bool AllowsInterpolation() const { return bAllowsInterpolation; }

	int32 GetNumberOfEntries() const { return EntryToValueKeyMap.Num(); }
	int32 GetNumberOfEntriesWithParents() const { return EntryToValueKeyMap.Num() + (Parent ? Parent->GetNumberOfEntries() : 0); }

	// This call is not thread safe
	const TMap<PCGMetadataEntryKey, PCGMetadataValueKey>& GetEntryToValueKeyMap_NotThreadSafe() const { return EntryToValueKeyMap; }

	const FPCGMetadataAttributeBase* GetParent() const { return Parent; }

	/** Returns true if for valid attribute names, which are alphanumeric with some special characters allowed. */
	static UE_API bool IsValidName(const FString& Name);
	static UE_API bool IsValidName(const FName& Name);

	/** Replaces any invalid characters in name with underscores. Returns true if Name was changed. */
	static UE_API bool SanitizeName(FString& InOutName);

private:
	// Unsafe version, needs to be write lock protected.
	UE_API void SetValueFromValueKey_Unsafe(PCGMetadataEntryKey EntryKey, PCGMetadataValueKey ValueKey, bool bResetValueOnDefaultValueKey, bool bAllowInvalidEntries = false);

	// Gather the value keys for the list of entry keys.
	// Because we need to update the entry keys if we look for the value keys in the parent, but because the EntryKeys are coming from the outside, we can't modify them.
	// (as entry keys need to be put into the parent referential when looking for value keys). 
	// So we will copy internally the EntryKeys to modify them (and only once) if we are not owner of the memory.
	UE_API void GetValueKeys_Internal(TConstPCGValueRange<PCGMetadataEntryKey> EntryKeys, TArrayView<PCGMetadataValueKey> OutValueKeys, TBitArray<>& UnsetValues, bool bOwnerOfEntryKeysView = false) const;

protected:
	TMap<PCGMetadataEntryKey, PCGMetadataValueKey> EntryToValueKeyMap;
	mutable FRWLock EntryMapLock;

	FPCGMetadataDomain* Metadata = nullptr;
	const FPCGMetadataAttributeBase* Parent = nullptr;
	int16 TypeId = 0;
	bool bAllowsInterpolation = false;

public:
	FName Name = NAME_None;
	PCGMetadataAttributeKey AttributeId = -1;
};

#undef UE_API

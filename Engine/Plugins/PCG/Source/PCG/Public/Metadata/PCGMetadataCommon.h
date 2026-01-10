// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "PCGMetadataCommon.generated.h"

typedef int64 PCGMetadataEntryKey;
typedef int32 PCGMetadataAttributeKey;
typedef int32 PCGMetadataValueKey;

const PCGMetadataEntryKey PCGInvalidEntryKey = -1;
const PCGMetadataEntryKey PCGFirstEntryKey = 0;
const PCGMetadataValueKey PCGDefaultValueKey = -1;
const PCGMetadataValueKey PCGNotFoundValueKey = -2;

UENUM()
enum class EPCGMetadataOp : uint8
{
	/** Take the minimum value. */
	Min,
	/** Take the maximum value. */
	Max,
	/** Subtract the values. */
	Sub,
	/** Add the values. */
	Add,
	/** Multiply the values. */
	Mul,
	/** Divide the values. */
	Div,
	/** Pick the source (first) value. */
	SourceValue,
	/** Pick the target (second) value. */
	TargetValue
};

UENUM()
enum class EPCGMetadataFilterMode : uint8
{
	/** The listed attributes will be unchanged by the projection and will not be added from the target data. */
	ExcludeAttributes,
	/** Only the listed attributes will be changed by the projection or added from the target data. */
	IncludeAttributes,
};

UENUM()
enum class EPCGMetadataDomainFlag : uint8
{
	/** Depends on the data. Should map to the same concept before multi-domain metadata. */
	Default = 0,
	
	/** Metadata at the data domain. */
	Data = 1,

	/** Metadata on elements like points on point data and entries on param data. */
	Elements = 2,
	
	/** For invalid domain. */
	Invalid = 254,

	/** For data that can have more domains. */
	Custom = 255
};

USTRUCT(BlueprintType)
struct FPCGMetadataDomainID
{
	GENERATED_BODY();

	FPCGMetadataDomainID() = default;
	
	FPCGMetadataDomainID(EPCGMetadataDomainFlag InFlag, int32 InCustomFlag = -1, FName InDebugName = NAME_None)
		: Flag(InFlag)
		, CustomFlag(InCustomFlag)
		, DebugName(InDebugName)
	{
		check(CustomFlag == -1 || InFlag == EPCGMetadataDomainFlag::Custom);
	}

	bool operator==(const FPCGMetadataDomainID& Other) const
	{
		return Flag == Other.Flag && CustomFlag == Other.CustomFlag;
	}

	bool operator<(const FPCGMetadataDomainID& Other) const
	{
		return Flag < Other.Flag || (Flag == Other.Flag && CustomFlag < Other.CustomFlag);
	}

	friend uint32 GetTypeHash(const FPCGMetadataDomainID& Item)
	{
		return HashCombine(static_cast<uint32>(Item.Flag), static_cast<uint32>(Item.CustomFlag));
	}

	friend FArchive& operator<<(FArchive& Ar, FPCGMetadataDomainID& Item)
	{
		Ar << Item.Flag;
		Ar << Item.CustomFlag;
		return Ar;
	}

	bool IsDefault() const { return Flag == EPCGMetadataDomainFlag::Default; }
	bool IsValid() const { return Flag != EPCGMetadataDomainFlag::Invalid; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	EPCGMetadataDomainFlag Flag = EPCGMetadataDomainFlag::Default;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	int32 CustomFlag = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FName DebugName = NAME_None;
};

namespace PCGMetadataDomainID
{
	static const FPCGMetadataDomainID Default{EPCGMetadataDomainFlag::Default, -1, TEXT("Default")};
	static const FPCGMetadataDomainID Elements{EPCGMetadataDomainFlag::Elements, -1, TEXT("Elements")};
	static const FPCGMetadataDomainID Data{EPCGMetadataDomainFlag::Data, -1, TEXT("Data")};
	static const FPCGMetadataDomainID Invalid{EPCGMetadataDomainFlag::Invalid, -1, TEXT("Invalid")};
}

USTRUCT(BlueprintType)
struct FPCGAttributeIdentifier
{
	GENERATED_BODY();

	FPCGAttributeIdentifier() = default;

	// Needs to be backward compatible with FNames (and everything that can be constructed into a FName).
	template <typename T = FName, std::enable_if_t<std::is_constructible_v<FName, T>, bool> = true>
	FPCGAttributeIdentifier(const T& InName, FPCGMetadataDomainID InMetadataDomainID = PCGMetadataDomainID::Default)
		: Name(InName)
		, MetadataDomain(InMetadataDomainID)
	{}

	template <typename T = FName, std::enable_if_t<std::is_constructible_v<FName, T>, bool> = true>
	FPCGAttributeIdentifier& operator=(const T& InName)
	{
		Name = FName(InName);
		return *this;
	}

	UE_DEPRECATED(5.6, "Explicitly use the Name field.")
	operator FName() const
	{
		return Name;
	}

	bool operator==(const FPCGAttributeIdentifier& Other) const
	{
		return Name == Other.Name && MetadataDomain == Other.MetadataDomain;
	}

	friend uint32 GetTypeHash(const FPCGAttributeIdentifier& Item)
	{
		return HashCombine(GetTypeHash(Item.Name), GetTypeHash(Item.MetadataDomain));
	}
	
	friend FArchive& operator<<(FArchive& Ar, FPCGAttributeIdentifier& Item)
	{
		Ar << Item.Name;
		Ar << Item.MetadataDomain;
		return Ar;
	}
	
	static TSet<FPCGAttributeIdentifier> TransformNameSet(const TSet<FName>& InContainer)
	{
		TSet<FPCGAttributeIdentifier> OutContainer;
		OutContainer.Reserve(InContainer.Num());
		Algo::Transform(InContainer, OutContainer, [](const FName& InAttributeName) { return InAttributeName;});
		return OutContainer;
	}

	template <typename AllocatorType>
	static TArray<FPCGAttributeIdentifier, AllocatorType> TransformNameArray(const TArray<FName, AllocatorType>& InContainer)
	{
		TArray<FPCGAttributeIdentifier, AllocatorType> OutContainer;
		OutContainer.Reserve(InContainer.Num());
		Algo::Transform(InContainer, OutContainer, [](const FName& InAttributeName) { return InAttributeName;});
		return OutContainer;
	}

	template <typename Container>
	static TMap<FPCGMetadataDomainID, TSet<FName>> SplitByDomain(const Container& InContainer)
	{
		TMap<FPCGMetadataDomainID, TSet<FName>> OutMap;
		for (const FPCGAttributeIdentifier& It : InContainer)
		{
			OutMap.FindOrAdd(It.MetadataDomain).Add(It.Name);
		}

		return OutMap;
	}

	FString ToString() const
	{
		if (!MetadataDomain.IsValid())
		{
			return TEXT("INVALID");
		}
		else if (MetadataDomain.IsDefault())
		{
			return Name.ToString();
		}
		else
		{
			return FString::Printf(TEXT("%s.%s"), *MetadataDomain.DebugName.ToString(), *Name.ToString());
		}
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	FName Name = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "", meta = (ShowOnlyInnerProperties))
	FPCGMetadataDomainID MetadataDomain;
};


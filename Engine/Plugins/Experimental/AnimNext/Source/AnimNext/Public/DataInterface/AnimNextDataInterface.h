// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextRigVMAsset.h"
#include "AnimNextDataInterface.generated.h"

#define UE_API ANIMNEXT_API

class UAnimNextDataInterfaceFactory;
class UAnimNextDataInterface;

namespace UE::AnimNext
{
	struct FInjectionInfo;
}

namespace UE::AnimNext::Tests
{
	class FDataInterfaceCompile;
}

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

// Information about an implemented interface
USTRUCT()
struct FAnimNextImplementedDataInterface
{
	GENERATED_BODY()

	// Reference to the implemented data interface asset
	UPROPERTY()
	TObjectPtr<const UAnimNextDataInterface> DataInterface;

	// Native struct used for communication with the data interface
	UPROPERTY()
	TObjectPtr<const UScriptStruct> NativeInterface;

	// Index of the first variable that implements the interface
	UPROPERTY()
	int32 VariableIndex = INDEX_NONE;

	// Number of variables that implement the interface
	UPROPERTY()
	int32 NumVariables = 0;

	// Whether to automatically bind this interface to any host data interface 
	UPROPERTY()
	bool bAutoBindToHost = false;
};

// Data interfaces provide a set of named data that is shared between AnimNext assets and used for communication between assets and functional units
UCLASS(MinimalAPI, BlueprintType)
class UAnimNextDataInterface : public UAnimNextRigVMAsset
{
	GENERATED_BODY()

public:
	UE_API UAnimNextDataInterface(const FObjectInitializer& ObjectInitializer);

	// Get all the implemented interfaces
	UE_API TConstArrayView<FAnimNextImplementedDataInterface> GetImplementedInterfaces() const;

	// Find an implemented interface
	UE_API const FAnimNextImplementedDataInterface* FindImplementedInterface(const UAnimNextDataInterface* InDataInterface) const;

private:
	friend class UAnimNextDataInterfaceFactory;
	friend class UE::AnimNext::Tests::FDataInterfaceCompile;
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend UE::AnimNext::FInjectionInfo;

	// Information about implemented interfaces. Note this includes the 'self' interface (first), if any public variables are specified.
	UPROPERTY()
	TArray<FAnimNextImplementedDataInterface> ImplementedInterfaces;

	// The variable index of the default injection site
	UPROPERTY()
	int32 DefaultInjectionSiteIndex;
};

#undef UE_API

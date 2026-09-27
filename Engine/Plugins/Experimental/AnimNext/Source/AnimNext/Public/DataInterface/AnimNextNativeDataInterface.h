// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextNativeDataInterface.generated.h"

class UAnimNextDataInterface;

// Base structure used to implement a native interface into AnimNext assets
USTRUCT()
struct FAnimNextNativeDataInterface
{
	GENERATED_BODY()

	FAnimNextNativeDataInterface() = default;
	virtual ~FAnimNextNativeDataInterface() = default;

	// Context passed to Initialize()
	struct FBindToFactoryObjectContext
	{
		// If this native interface was created as a result of a factory, then this is the object that the factor used as a source. Can be nullptr.
		const UObject* FactoryObject = nullptr;

		// The data interface that this native interface was constructed for
		const UAnimNextDataInterface* DataInterface = nullptr;
	};

	// Initialize this native interface during factory object creation.
	// Called once on worker threads to set up this native interface's properties after it has been default-constructed.
	// The data interface instance that it will manage has not yet been constructed at this point.
	virtual void BindToFactoryObject(const FBindToFactoryObjectContext& InContext) {}
};

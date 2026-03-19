// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "ChaosVDCustomIconDataStorageFactory.generated.h"

/**
 * 
 */
UCLASS()
class UChaosVDCustomIconDataStorageFactory : public UEditorDataStorageFactory
{
	GENERATED_BODY()
	
	virtual ~UChaosVDCustomIconDataStorageFactory() override = default;

	virtual void RegisterQueries(UE::Editor::DataStorage::ICoreProvider& DataStorage) override;
};

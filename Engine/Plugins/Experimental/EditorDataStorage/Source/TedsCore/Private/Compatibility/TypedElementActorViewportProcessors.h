// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementActorViewportProcessors.generated.h"

UCLASS()
class UActorViewportDataStorageFactory : public UEditorDataStorageFactory
{
	GENERATED_BODY()

public:
	~UActorViewportDataStorageFactory() override = default;

	void RegisterQueries(UE::Editor::DataStorage::ICoreProvider& DataStorage) override;

private:
	void RegisterOutlineColorColumnToActor(UE::Editor::DataStorage::ICoreProvider& DataStorage);
	void RegisterOverlayColorColumnToActor(UE::Editor::DataStorage::ICoreProvider& DataStorage);
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Interfaces/TypedElementDataStorageFactory.h"

#include "ObjectPackagePathToColumnQueries.generated.h"

namespace UE::Editor::DataStorage
{
	class ICoreProvider;
} // namespace UE::Editor::DataStorage

UCLASS()
class UTypedElementUObjectPackagePathFactory : public UEditorDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTypedElementUObjectPackagePathFactory() override = default;

	void RegisterQueries(UE::Editor::DataStorage::ICoreProvider& DataStorage) override;

private:
	void RegisterTryAddPackageRef(UE::Editor::DataStorage::ICoreProvider& DataStorage);
	UE::Editor::DataStorage::QueryHandle TryAddPackageRef = UE::Editor::DataStorage::InvalidQueryHandle;
};

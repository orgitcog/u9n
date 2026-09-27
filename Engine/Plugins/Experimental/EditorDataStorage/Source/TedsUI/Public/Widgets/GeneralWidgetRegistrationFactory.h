// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "UObject/ObjectMacros.h"

#include "GeneralWidgetRegistrationFactory.generated.h"

UCLASS()
class TEDSUI_API UGeneralWidgetRegistrationFactory : public UEditorDataStorageFactory
{
	GENERATED_BODY()

public:
	static const FName LargeCellPurpose;
	static const FName HeaderPurpose;

	~UGeneralWidgetRegistrationFactory() override = default;

	void RegisterWidgetPurposes(UE::Editor::DataStorage::IUiProvider& DataStorageUi) const override;
};

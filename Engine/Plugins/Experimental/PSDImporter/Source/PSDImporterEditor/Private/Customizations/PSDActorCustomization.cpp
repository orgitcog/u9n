// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/PSDActorCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"

#define LOCTEXT_NAMESPACE "PSDActorCustomization"

namespace UE::PSDImporterEditor
{

TSharedRef<IDetailCustomization> FPSDActorCustomization::MakeInstance()
{
	return MakeShared<FPSDActorCustomization>();
}

void FPSDActorCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	InDetailBuilder.EditCategory(TEXT("PSD"), LOCTEXT("PSD", "PSD"))
		.SetSortOrder(2000);
}

} // UE::PSDImporterEditor

#undef LOCTEXT_NAMESPACE

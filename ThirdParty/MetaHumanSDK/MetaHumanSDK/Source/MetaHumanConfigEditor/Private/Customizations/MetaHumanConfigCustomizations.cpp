// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanConfigCustomizations.h"
#include "MetaHumanConfig.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DetailLayoutBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "Dialogs/Dialogs.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "MetaHumanAnimator"

namespace
{
	TAutoConsoleVariable<bool> CVarConfigAllowCustomization
	{
		TEXT("mh.Config.AllowCustomization"),
		false,
		TEXT("Enables the customization of configs"),
		ECVF_Default
	};
}

TSharedRef<IDetailCustomization> FMetaHumanConfigCustomization::MakeInstance()
{
	return MakeShared<FMetaHumanConfigCustomization>();
}

void FMetaHumanConfigCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	UMetaHumanConfig* Config = nullptr;

	// Get the config object that we're building the details panel for.
	if (!InDetailBuilder.GetSelectedObjects().IsEmpty())
	{
		Config = Cast<UMetaHumanConfig>(InDetailBuilder.GetSelectedObjects()[0].Get());
	}

	bool bAllowCustomization = CVarConfigAllowCustomization->GetBool();

	TSharedRef<IPropertyHandle> NameProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanConfig, Name));
	IDetailPropertyRow* NameRow = InDetailBuilder.EditDefaultProperty(NameProperty);
	check(NameRow);

	TSharedPtr<SWidget> NameWidget, ValueWidget;
	NameRow->GetDefaultWidgets(NameWidget, ValueWidget);

	NameRow->CustomWidget()
		.NameContent()
		[
			NameWidget.ToSharedRef()
		]
		.ValueContent()
		.MinDesiredWidth(250.0f)
		.MaxDesiredWidth(0.0f)
		[
			SNew(SBox)
			.IsEnabled_Lambda([bAllowCustomization]() 
			{ 
				return bAllowCustomization;
			})
			[
				ValueWidget.ToSharedRef()
			]
		];

	TSharedRef<IPropertyHandle> VersionProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanConfig, Version));
	IDetailPropertyRow* VersionRow = InDetailBuilder.EditDefaultProperty(VersionProperty);
	check(VersionRow);

	VersionRow->GetDefaultWidgets(NameWidget, ValueWidget);

	VersionRow->CustomWidget()
		.NameContent()
		[
			NameWidget.ToSharedRef()
		]
		.ValueContent()
		.MinDesiredWidth(250.0f)
		.MaxDesiredWidth(0.0f)
		[
			SNew(SBox)
			.IsEnabled_Lambda([bAllowCustomization]()
			{
				return bAllowCustomization;
			})
			[
				ValueWidget.ToSharedRef()
			]
		];

	if (bAllowCustomization)
	{
		IDetailCategoryBuilder& ParamsCategory = InDetailBuilder.EditCategory(TEXT("Parameters"));
		ParamsCategory.SetSortOrder(1000);

		IDetailCategoryBuilder& InitCategory = InDetailBuilder.EditCategory(TEXT("Initialization"));
		InitCategory.SetSortOrder(1001);

		FDetailWidgetRow& InitRow = InitCategory.AddCustomRow(LOCTEXT("MHConfig_Initialize", "Initialize"));

		InitRow
			.NameContent()
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(LOCTEXT("MHConfig_Initialize_CreateFromDirectory", "Create from file directory"))
			]
			.ValueContent()
			.MinDesiredWidth(250.0f)
			.MaxDesiredWidth(0.0f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.Text(LOCTEXT("FromFile", "..."))
				.OnClicked_Lambda([Config]()
				{
					FString Directory;
					if (PromptUserForDirectory(Directory, TEXT("Config directory"), TEXT("")))
					{
						Config->ReadFromDirectory(Directory);
					}

					return FReply::Handled();
				})
			];
	}
}

#undef LOCTEXT_NAMESPACE
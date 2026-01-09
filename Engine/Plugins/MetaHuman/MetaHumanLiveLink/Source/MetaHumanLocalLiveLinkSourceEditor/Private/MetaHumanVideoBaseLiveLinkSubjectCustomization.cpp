// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanVideoBaseLiveLinkSubjectCustomization.h"
#include "MetaHumanVideoBaseLiveLinkSubjectSettings.h"
#include "MetaHumanVideoBaseLiveLinkSubjectMonitorWidget.h"
#include "MetaHumanLocalLiveLinkSubjectMonitorWidget.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"

#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MetaHumanVideoBaseLiveLinkSource"



TSharedRef<IDetailCustomization> FMetaHumanVideoBaseLiveLinkSubjectCustomization::MakeInstance()
{
	return MakeShared<FMetaHumanVideoBaseLiveLinkSubjectCustomization>();
}

void FMetaHumanVideoBaseLiveLinkSubjectCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	InDetailBuilder.GetObjectsBeingCustomized(Objects);

	check(Objects.Num() == 1);
	UMetaHumanVideoBaseLiveLinkSubjectSettings* Settings = Cast<UMetaHumanVideoBaseLiveLinkSubjectSettings>(Objects[0]);

	if (!Settings->bIsLiveProcessing)
	{
		return;
	}

	IDetailCategoryBuilder& MonitorCategory = InDetailBuilder.EditCategory("Image", LOCTEXT("Image", "Image"), ECategoryPriority::Important);

	TSharedPtr<SMetaHumanLocalLiveLinkSubjectMonitorWidget> LocalLiveLinkSubjectMonitorWidget = SNew(SMetaHumanLocalLiveLinkSubjectMonitorWidget, Settings);

	MonitorCategory.AddCustomRow(LOCTEXT("Image", "Image"))
		.WholeRowContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.MaxHeight(200)
			[
				SNew(SMetaHumanVideoBaseLiveLinkSubjectMonitorWidget, Settings, LocalLiveLinkSubjectMonitorWidget)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				LocalLiveLinkSubjectMonitorWidget.ToSharedRef()
			]
		];

	TSharedRef<IPropertyHandle> FocalLengthProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanVideoBaseLiveLinkSubjectSettings, FocalLength));
	IDetailPropertyRow* FocalLengthRow = InDetailBuilder.EditDefaultProperty(FocalLengthProperty);
	check(FocalLengthRow);

	TSharedPtr<SWidget> NameWidget, ValueWidget;
	FocalLengthRow->GetDefaultWidgets(NameWidget, ValueWidget);

	FocalLengthRow->CustomWidget()
		.NameContent()
		[
			NameWidget.ToSharedRef()
		]
		.ValueContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ToolTipText_Lambda([Settings]()
			{
				if (Settings->FocalLength < 0)
				{
					return LOCTEXT("FocalNotSetTooltip", "Focal length is set when a Head Translation Neutral is captured");
				}
				else
				{
					return FText::FromString(FString::Printf(TEXT("%.2f pixels"), Settings->FocalLength));
				}
			})
			.Text_Lambda([Settings]()
			{
				if (Settings->FocalLength < 0)
				{ 
					return LOCTEXT("FocalNotSet", "Not Set");
				}
				else
				{
					return FText::FromString(FString::Printf(TEXT("%.2f px"), Settings->FocalLength));
				}
			})
		];

	TSharedRef<IPropertyHandle> ResProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanVideoBaseLiveLinkSubjectSettings, Resolution));
	IDetailPropertyRow* ResRow = InDetailBuilder.EditDefaultProperty(ResProperty);
	check(ResRow);

	ResRow->GetDefaultWidgets(NameWidget, ValueWidget);

	ResRow->CustomWidget()
		.NameContent()
		[
			NameWidget.ToSharedRef()
		]
		.ValueContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text_Lambda([Settings]()
			{
				return FText::FromString(Settings->Resolution);
			})
		];

	TSharedRef<IPropertyHandle> DroppingProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanVideoBaseLiveLinkSubjectSettings, Dropping));
	IDetailPropertyRow* DroppingRow = InDetailBuilder.EditDefaultProperty(DroppingProperty);
	check(DroppingRow);

	DroppingRow->GetDefaultWidgets(NameWidget, ValueWidget);

	DroppingRow->CustomWidget()
		.NameContent()
		[
			NameWidget.ToSharedRef()
		]
		.ValueContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text_Lambda([Settings]()
			{
				return FText::FromString(Settings->Dropping);
			})
		];
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCharacterEditorBlendToolView.h"

#include "AssetThumbnail.h"
#include "ClassIconFinder.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Engine/Texture2D.h"
#include "ImageCoreUtils.h"
#include "ImageUtils.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "Slate/DeferredCleanupSlateBrush.h"
#include "Tools/MetaHumanCharacterEditorFaceEditingTools.h"
#include "Tools/MetaHumanCharacterEditorBodyEditingTools.h"
#include "Tools/MetaHumanCharacterEditorToolTargetUtil.h"
#include "UI/Widgets/SMetaHumanCharacterEditorBlendToolPanel.h"
#include "UI/Widgets/SMetaHumanCharacterEditorToolPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "SWarningOrErrorBox.h"
#include "ObjectTools.h"

#define LOCTEXT_NAMESPACE "SMetaHumanCharacterEditorBlendToolView"

namespace UE::MetaHuman::Private
{
	UTexture2D* ThumbnailToTexture(const FObjectThumbnail* InThumbnailObject)
	{
		if (InThumbnailObject)
		{
			return UTexture2D::CreateTransient(
				InThumbnailObject->GetImageWidth(),
				InThumbnailObject->GetImageHeight(),
				FImageCoreUtils::GetPixelFormatForRawImageFormat(InThumbnailObject->GetImage().Format),
				NAME_None,
				InThumbnailObject->GetUncompressedImageData()); // This will decompress thumbnail image data, if necessary
		}

		return nullptr;
	}

	UTexture2D* LoadThumbnailAsTextureFromAssetData(const FAssetData& InAssetData, EMetaHumanCharacterThumbnailCameraPosition InCameraPosition)
	{
		UTexture2D* Texture = nullptr;
		const FString ObjectPath = InAssetData.GetObjectPathString();
		const FName ThumbnailPath = UMetaHumanCharacter::GetThumbnailPathInPackage(ObjectPath, InCameraPosition);
		FThumbnailMap ThumbnailMap;
		ThumbnailTools::ConditionallyLoadThumbnailsForObjects({ ThumbnailPath }, ThumbnailMap);

		if (FObjectThumbnail* ThumbnailObject = ThumbnailMap.Find(ThumbnailPath))
		{
			return ThumbnailToTexture(ThumbnailObject);
		}

		return Texture;
	}
}

FName SMetaHumanCharacterEditorHeadBlendToolView::HeadBlendAssetsSlotName = FName(TEXT("Head Blend"));
FName SMetaHumanCharacterEditorBodyBlendToolView::BodyBlendAssetsSlotName = FName(TEXT("Body Blend"));

void SMetaHumanCharacterEditorBlendToolView::Construct(const FArguments& InArgs, UMetaHumanCharacterEditorMeshBlendTool* InTool)
{
	SMetaHumanCharacterEditorToolView::Construct(SMetaHumanCharacterEditorToolView::FArguments(), InTool);
}

UInteractiveToolPropertySet* SMetaHumanCharacterEditorBlendToolView::GetToolProperties() const
{
	UInteractiveToolPropertySet* ToolProperties = nullptr;
	
	if (const UMetaHumanCharacterEditorFaceBlendTool* FaceBlendTool = Cast<UMetaHumanCharacterEditorFaceBlendTool>(Tool); IsValid(FaceBlendTool))
	{
		ToolProperties = FaceBlendTool->GetFaceToolHeadParameterProperties();
	}
	else if (const UMetaHumanCharacterEditorBodyBlendTool* BodyBlendTool = Cast<UMetaHumanCharacterEditorBodyBlendTool>(Tool); IsValid(BodyBlendTool))
	{
		ToolProperties = BodyBlendTool->GetBodyParameterProperties();
	}

	return ToolProperties;
}

void SMetaHumanCharacterEditorBlendToolView::OnBlendToolItemDropped(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent, int32 InItemIndex)
{
	// TODO: implementing the logic to work when an item is dropped in the blend tool
	UMetaHumanCharacterEditorMeshBlendTool* BlendTool = Cast<UMetaHumanCharacterEditorMeshBlendTool>(Tool);
	TSharedPtr<FAssetDragDropOp> AssetDragDropOperation = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
	if (AssetDragDropOperation.IsValid())
	{
		const TArray<FAssetData>& AssetsData = AssetDragDropOperation->GetAssets();
		if (!AssetsData.IsEmpty())
		{
			const FAssetData& DroppedAssetData = AssetsData[0];
			if (UMetaHumanCharacter* Character = Cast<UMetaHumanCharacter>(DroppedAssetData.GetAsset()))
			{
				BlendTool->AddMetaHumanCharacterPreset(Character, InItemIndex);
			}
		}
	}
}

void SMetaHumanCharacterEditorBlendToolView::OnBlendToolItemDeleted(int32 InItemIndex)
{
	UMetaHumanCharacterEditorMeshBlendTool* BlendTool = Cast<UMetaHumanCharacterEditorMeshBlendTool>(Tool);
	if (BlendTool)
	{
		BlendTool->RemoveMetaHumanCharacterPreset(InItemIndex);
	}
}

void SMetaHumanCharacterEditorBlendToolView::OnBlendToolItemActivated(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item)
{
	UMetaHumanCharacterEditorMeshBlendTool* BlendTool = Cast<UMetaHumanCharacterEditorMeshBlendTool>(Tool);
	if (BlendTool && Item.IsValid())
	{
		const FAssetData AssetData = Item->AssetData;
		if (UMetaHumanCharacter* Character = Cast<UMetaHumanCharacter>(AssetData.GetAsset()))
		{
			BlendTool->BlendToMetaHumanCharacterPreset(Character);
		}
	}
}

void SMetaHumanCharacterEditorHeadBlendToolView::Construct(const FArguments& InArgs, UMetaHumanCharacterEditorMeshBlendTool* InTool)
{
	SMetaHumanCharacterEditorToolView::Construct(SMetaHumanCharacterEditorToolView::FArguments(), InTool);
}

void SMetaHumanCharacterEditorHeadBlendToolView::MakeToolView()
{
	if (ToolViewScrollBox.IsValid())
	{
		ToolViewScrollBox->AddSlot()
			.VAlign(VAlign_Top)
			[
				SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Bottom)
					.Padding(4.f)
					.AutoHeight()
					[
						CreateManipulatorsViewSection()
					]

					+ SVerticalBox::Slot()
					.Padding(4.f)
					.AutoHeight()
					[
						CreateBlendToolViewBlendPanelSection()
					]

					+ SVerticalBox::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Bottom)
					.Padding(4.f)
					.AutoHeight()
					[
						CreateHeadParametersViewSection()
					]
			];
	}
}

TSharedRef<SWidget> SMetaHumanCharacterEditorHeadBlendToolView::CreateBlendToolViewBlendPanelSection()
{
	UMetaHumanCharacterEditorMeshBlendTool* BlendTool = Cast<UMetaHumanCharacterEditorMeshBlendTool>(Tool);
	UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(BlendTool->GetTarget());
	if (!IsValid(BlendTool))
	{
		return SNullWidget::NullWidget;
	}

	const TSharedRef<SWidget> BlendToolSectionWidget =
		SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("HeadBlendSectionLabel", "Blend Preset Selection"))
		.Content()
		[
			SNew(SVerticalBox)

			// Presets tile view section
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(4.f)
			.AutoHeight()
			[
				SAssignNew(BlendToolPanel, SMetaHumanCharacterEditorBlendToolPanel, Character)
				.VirtualFolderSlotName(HeadBlendAssetsSlotName)
				.OnItemDropped(this, &SMetaHumanCharacterEditorBlendToolView::OnBlendToolItemDropped)
				.OnItemDeleted(this, &SMetaHumanCharacterEditorBlendToolView::OnBlendToolItemDeleted)
				.OnItemActivated(this, &SMetaHumanCharacterEditorBlendToolView::OnBlendToolItemActivated)
				.OnOverrideItemThumbnail(this, &SMetaHumanCharacterEditorHeadBlendToolView::OnOverrideItemThumbnailBrush)
			]
		];

	return BlendToolSectionWidget;
}

TSharedRef<SWidget> SMetaHumanCharacterEditorHeadBlendToolView::CreateManipulatorsViewSection()
{
	const UMetaHumanCharacterEditorFaceBlendTool* FaceTool = Cast<UMetaHumanCharacterEditorFaceBlendTool>(Tool);
	if (!IsValid(FaceTool))
	{
		return SNullWidget::NullWidget;
	}

	UMetaHumanCharacterEditorMeshEditingToolProperties* ManipulatorProperties = Cast<UMetaHumanCharacterEditorMeshEditingToolProperties>(FaceTool->GetMeshEditingToolProperties());
	UMetaHumanCharacterEditorFaceBlendToolProperties* FaceBlendToolProperties = Cast<UMetaHumanCharacterEditorFaceBlendToolProperties>(FaceTool->GetBlendToolProperties());
	if (!IsValid(ManipulatorProperties) || !IsValid(FaceBlendToolProperties))
	{
		return SNullWidget::NullWidget;
	}
	

	FProperty* SizeProperty = UMetaHumanCharacterEditorMeshEditingToolProperties::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorMeshEditingToolProperties, Size));
	FProperty* SymmetricProperty = UMetaHumanCharacterEditorMeshEditingToolProperties::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorMeshEditingToolProperties, bSymmetricModeling));
	FProperty* BlendOptionsProperty = UMetaHumanCharacterEditorFaceBlendToolProperties::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorFaceBlendToolProperties, BlendOptions));

	return SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("HeadBlendToolManipulatorSection", "Manipulator"))
		.Content()
		[
			SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					CreatePropertySpinBoxWidget(LOCTEXT("HeadManipulatorSize", "Size").ToString(), SizeProperty, ManipulatorProperties)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					CreatePropertyCheckBoxWidget(LOCTEXT("HeadManipulatorSymmetricManipulation", "Symmetric Manipulation").ToString(), SymmetricProperty, ManipulatorProperties)
				]
				
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					CreatePropertyComboBoxWidget<EBlendOptions>(TEXT("Blend Space"), FaceBlendToolProperties->BlendOptions, BlendOptionsProperty, FaceBlendToolProperties)
				]
		];
}

TSharedRef<SWidget> SMetaHumanCharacterEditorHeadBlendToolView::CreateHeadParametersViewSection()
{
	const UMetaHumanCharacterEditorFaceTool* FaceTool = Cast<UMetaHumanCharacterEditorFaceTool>(Tool);
	if (!IsValid(FaceTool))
	{
		return SNullWidget::NullWidget;
	}

	UMetaHumanCharacterEditorFaceEditingToolHeadParameterProperties* HeadParameterProperties = Cast<UMetaHumanCharacterEditorFaceEditingToolHeadParameterProperties>(FaceTool->GetFaceToolHeadParameterProperties());
	if (!HeadParameterProperties)
	{
		return SNullWidget::NullWidget;
	}

	FProperty* GlobalDeltaProperty = UMetaHumanCharacterEditorFaceEditingToolHeadParameterProperties::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorFaceEditingToolHeadParameterProperties, GlobalDelta));
	FProperty* HeadScaleProperty = UMetaHumanCharacterEditorFaceEditingToolHeadParameterProperties::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorFaceEditingToolHeadParameterProperties, HeadScale));

	return SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("HeadBlendToolHeadParametersSection", "Head Parameters"))
		.Content()
		[
			SNew(SVerticalBox)
				// Global delta
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					CreatePropertySpinBoxWidget(TEXT("Global Delta"), GlobalDeltaProperty, HeadParameterProperties)
				]

				// Head size
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					CreatePropertySpinBoxWidget(TEXT("Head Scale"), HeadScaleProperty, HeadParameterProperties)
				]
				+ SVerticalBox::Slot()
				.Padding(4.f)
				[
					SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "FlatButton.Default")
						.ForegroundColor(FLinearColor::White)
						.OnClicked(this, &SMetaHumanCharacterEditorHeadBlendToolView::OnResetButtonClicked)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.ToolTipText(LOCTEXT("ResetFaceToolTip", "Reverts the face back to default."))
						[
							SNew(STextBlock)
								.Text(LOCTEXT("ResetFace", "Reset Head Parameters"))
								.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						]
				]
				+ SVerticalBox::Slot()
				.Padding(4.f)
				[
					SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "FlatButton.Default")
						.ForegroundColor(FLinearColor::White)
						.OnClicked(this, &SMetaHumanCharacterEditorHeadBlendToolView::OnResetNeckButtonClicked)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.ToolTipText(LOCTEXT("ResetFaceNeckToolTip", "Reverts the neck region and aligns it to the body."))
						[
							SNew(STextBlock)
								.Text(LOCTEXT("ResetFaceNeck", "Align Neck to Body"))
								.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						]
				]
		];
}

void SMetaHumanCharacterEditorHeadBlendToolView::OnOverrideItemThumbnailBrush(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item) const
{
	if (!Item.IsValid())
	{
		return;
	}

	if (UTexture2D* Texture = UE::MetaHuman::Private::LoadThumbnailAsTextureFromAssetData(Item->AssetData, EMetaHumanCharacterThumbnailCameraPosition::Face))
	{
		Item->ThumbnailImageOverride = FDeferredCleanupSlateBrush::CreateBrush(Texture);
	}
}

FReply SMetaHumanCharacterEditorHeadBlendToolView::OnResetButtonClicked() const
{
	Cast<UMetaHumanCharacterEditorFaceTool>(Tool)->ResetFace();
	return FReply::Handled();
}

FReply SMetaHumanCharacterEditorHeadBlendToolView::OnResetNeckButtonClicked() const
{
	Cast<UMetaHumanCharacterEditorFaceTool>(Tool)->ResetFaceNeck();
	return FReply::Handled();
}



void SMetaHumanCharacterEditorBodyBlendToolView::Construct(const FArguments& InArgs, UMetaHumanCharacterEditorMeshBlendTool* InTool)
{
	SMetaHumanCharacterEditorToolView::Construct(SMetaHumanCharacterEditorToolView::FArguments(), InTool);
}

void SMetaHumanCharacterEditorBodyBlendToolView::MakeToolView()
{
	if (ToolViewScrollBox.IsValid())
	{
		ToolViewScrollBox->AddSlot()
			.VAlign(VAlign_Top)
			[
				SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.Padding(4.f)
					.AutoHeight()
					[
						CreateManipulatorsViewSection()
					]

					+ SVerticalBox::Slot()
					.Padding(4.f)
					.AutoHeight()
					[
						CreateBlendToolViewBlendPanelSection()
					]

					+ SVerticalBox::Slot()
					.Padding(4.f)
					.AutoHeight()
					[
						CreateBodyParametersViewSection()
					]
			];
	}
}

TSharedRef<SWidget> SMetaHumanCharacterEditorBodyBlendToolView::CreateManipulatorsViewSection()
{
	const UMetaHumanCharacterEditorMeshEditingTool* MeshTool = Cast<UMetaHumanCharacterEditorMeshEditingTool>(Tool);
	if (!IsValid(MeshTool))
	{
		return SNullWidget::NullWidget;
	}

	UMetaHumanCharacterEditorMeshEditingToolProperties* ManipulatorProperties = Cast<UMetaHumanCharacterEditorMeshEditingToolProperties>(MeshTool->GetMeshEditingToolProperties());
	UMetaHumanCharacterEditorBodyBlendToolProperties* BodyBlendToolProperties = GetBodyBlendToolProperties();
	if (!IsValid(ManipulatorProperties) || !IsValid(BodyBlendToolProperties))
	{
		return SNullWidget::NullWidget;
	}

	FProperty* SizeProperty = UMetaHumanCharacterEditorMeshEditingToolProperties::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorMeshEditingToolProperties, Size));
	FProperty* BlendOptionsProperty = UMetaHumanCharacterEditorBodyBlendToolProperties::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorBodyBlendToolProperties, BlendOptions));
	
	return SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("BodyBlendToolManipulatorSection", "Manipulator"))
		.Content()
		[
			SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					CreatePropertySpinBoxWidget(LOCTEXT("BodyManipulatorSize", "Size").ToString(), SizeProperty, ManipulatorProperties)
				]

				+ SVerticalBox::Slot()
				.MinHeight(24.f)
				.Padding(2.f, 0.f)
				.AutoHeight()
				[
					CreatePropertyComboBoxWidget<EBodyBlendOptions>(TEXT("Blend Type"), BodyBlendToolProperties->BlendOptions, BlendOptionsProperty, BodyBlendToolProperties)
				]
		];
}

TSharedRef<SWidget> SMetaHumanCharacterEditorBodyBlendToolView::CreateBlendToolViewBlendPanelSection()
{
	UMetaHumanCharacterEditorMeshBlendTool* BlendTool = Cast<UMetaHumanCharacterEditorMeshBlendTool>(Tool);
	UMetaHumanCharacter* Character = UE::ToolTarget::GetTargetMetaHumanCharacter(BlendTool->GetTarget());

	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SMetaHumanCharacterEditorToolPanel)
			.Label(LOCTEXT("BodyBlendSectionLabel", "Blend Preset Selection"))
			.Visibility(this, &SMetaHumanCharacterEditorBodyBlendToolView::GetBodyBlendSubToolVisibility)
			.Content()
			[
				SNew(SVerticalBox)

				// Presets tile view section
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.Padding(4.f)
				.AutoHeight()
				[
					SAssignNew(BlendToolPanel, SMetaHumanCharacterEditorBlendToolPanel, Character)
					.VirtualFolderSlotName(BodyBlendAssetsSlotName)
					.OnItemDropped(this, &SMetaHumanCharacterEditorBlendToolView::OnBlendToolItemDropped)
					.OnItemDeleted(this, &SMetaHumanCharacterEditorBlendToolView::OnBlendToolItemDeleted)
					.OnItemActivated(this, &SMetaHumanCharacterEditorBlendToolView::OnBlendToolItemActivated)
					.OnOverrideItemThumbnail(this, &SMetaHumanCharacterEditorBodyBlendToolView::OnOverrideItemThumbnailBrush)
					.OnFilterAssetData(this, &SMetaHumanCharacterEditorBodyBlendToolView::OnFilterAddAssetDataToAssetView)
				]
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SWarningOrErrorBox)
			.AutoWrapText(false)
			.Visibility(this,  &SMetaHumanCharacterEditorBodyBlendToolView::GetFixedBodyWarningVisibility)
			.MessageStyle(EMessageStyle::Warning)
			.Message(LOCTEXT("MetaHumanBodyBlendFixedWarning", "This Asset uses a Fixed Body type, Fixed Body types can't\n"
															"be modified or used for blending without fitting them first to\n"
															"the Parametric Model. This is an approximation and can\n"
															"result in some visual differences."))
		]
		+ SVerticalBox::Slot()
		.Padding(4.f)
		.AutoHeight()
		[
			SNew(SMetaHumanCharacterEditorToolPanel)
			.Label(LOCTEXT("BlendToolFixedBodyTypeLabel", "Fixed Body Type"))
			.Visibility(this,  &SMetaHumanCharacterEditorBodyBlendToolView::GetFixedBodyWarningVisibility)
			.Content()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(4.f)
				[
					SNew(SButton)
					.OnClicked(this, &SMetaHumanCharacterEditorBodyBlendToolView::OnPerformParametricFitButtonClicked)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("PerformParametricFit", "Perform Parametric Fit"))
					]
				]
			]
		];
}

TSharedRef<SWidget> SMetaHumanCharacterEditorBodyBlendToolView::CreateBodyParametersViewSection()
{
	const UMetaHumanCharacterEditorBodyBlendTool* BodyBlendTool = Cast<UMetaHumanCharacterEditorBodyBlendTool>(Tool);
	if (!IsValid(BodyBlendTool))
	{
		return SNullWidget::NullWidget;
	}

	UMetaHumanCharacterEditorBodyParameterProperties* BodyParameterProperties = Cast<UMetaHumanCharacterEditorBodyParameterProperties>(BodyBlendTool->GetBodyParameterProperties());
	if (!IsValid(BodyParameterProperties))
	{
		return SNullWidget::NullWidget;
	}

	FProperty* GlobalDeltaProperty = UMetaHumanCharacterEditorBodyParameterProperties::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorBodyParameterProperties, GlobalDelta));

	return SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("BodyBlendToolBodyParametersSection", "Body Parameters"))
		.Visibility(this, &SMetaHumanCharacterEditorBodyBlendToolView::GetBodyBlendSubToolVisibility)
		.Content()
		[
			SNew(SVerticalBox)
				// Global delta
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					CreatePropertySpinBoxWidget(TEXT("Global Delta"), GlobalDeltaProperty, BodyParameterProperties)
				]

				+ SVerticalBox::Slot()
				.Padding(4.f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "FlatButton.Default")
					.ForegroundColor(FLinearColor::White)
					.OnClicked(this, &SMetaHumanCharacterEditorBodyBlendToolView::OnResetButtonClicked)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.ToolTipText(LOCTEXT("ResetBodyToolTip", "Reverts the body back to default."))
					[
						SNew(STextBlock)
							.Text(LOCTEXT("ResetBody", "Reset Body Parameters"))
							.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					]
				]
		];
}

void SMetaHumanCharacterEditorBodyBlendToolView::OnOverrideItemThumbnailBrush(TSharedPtr<FMetaHumanCharacterAssetViewItem> Item) const
{
	if (!Item.IsValid())
	{
		return;
	}

	if (UTexture2D* Texture = UE::MetaHuman::Private::LoadThumbnailAsTextureFromAssetData(Item->AssetData, EMetaHumanCharacterThumbnailCameraPosition::Body))
	{
		Item->ThumbnailImageOverride = FDeferredCleanupSlateBrush::CreateBrush(Texture);
	}
}

bool SMetaHumanCharacterEditorBodyBlendToolView::OnFilterAddAssetDataToAssetView(const FAssetData& AssetData) const
{
	// Do not add fixed body types to body blend asset view
	FName FixedBodyTypePropertyName = GET_MEMBER_NAME_CHECKED(UMetaHumanCharacter, bFixedBodyType);
	bool bFixedBodyType = false;
	AssetData.GetTagValue(FixedBodyTypePropertyName, bFixedBodyType);
	return bFixedBodyType;
}

UMetaHumanCharacterEditorBodyBlendToolProperties* SMetaHumanCharacterEditorBodyBlendToolView::GetBodyBlendToolProperties() const
{
	UMetaHumanCharacterEditorMeshBlendTool* BlendTool = Cast<UMetaHumanCharacterEditorMeshBlendTool>(Tool);
	if (!IsValid(BlendTool))
	{
		return nullptr;
	}
	return Cast<UMetaHumanCharacterEditorBodyBlendToolProperties>(BlendTool->GetBlendToolProperties());
}

EVisibility SMetaHumanCharacterEditorBodyBlendToolView::GetBodyBlendSubToolVisibility() const
{
	UMetaHumanCharacterEditorBodyBlendToolProperties* BodyBlendToolProperties = GetBodyBlendToolProperties();
	if (IsValid(BodyBlendToolProperties))
	{
		if (!BodyBlendToolProperties->IsFixedBodyType())
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Collapsed;
}

EVisibility SMetaHumanCharacterEditorBodyBlendToolView::GetFixedBodyWarningVisibility() const
{
	UMetaHumanCharacterEditorBodyBlendToolProperties* BodyBlendToolProperties = GetBodyBlendToolProperties();
	if (IsValid(BodyBlendToolProperties))
	{
		if (BodyBlendToolProperties->IsFixedBodyType())
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Collapsed;
}

FReply SMetaHumanCharacterEditorBodyBlendToolView::OnPerformParametricFitButtonClicked() const
{
	UMetaHumanCharacterEditorBodyBlendToolProperties* BodyBlendToolProperties = GetBodyBlendToolProperties();
	BodyBlendToolProperties->PerformParametricFit();
	return FReply::Handled();
}

FReply SMetaHumanCharacterEditorBodyBlendToolView::OnResetButtonClicked() const
{
	const UMetaHumanCharacterEditorBodyBlendTool* BodyBlendTool = Cast<UMetaHumanCharacterEditorBodyBlendTool>(Tool);
	if (IsValid(BodyBlendTool))
	{
		UMetaHumanCharacterEditorBodyParameterProperties* BodyParameterProperties = Cast<UMetaHumanCharacterEditorBodyParameterProperties>(BodyBlendTool->GetBodyParameterProperties());
		BodyParameterProperties->ResetBody();
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPerformanceCustomizations.h"
#include "MetaHumanPerformance.h"
#include "MetaHumanPerformanceLog.h"
#include "SMetaHumanCameraCombo.h"
#include "FrameRangeArrayBuilder.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DetailLayoutBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "ControlRigBlueprint.h"
#include "Animation/Skeleton.h"
#include "AssetThumbnail.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/IConsoleManager.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "MetaHumanPerformance"

TSharedRef<IDetailCustomization> FMetaHumanPerformanceCustomization::MakeInstance()
{
	return MakeShared<FMetaHumanPerformanceCustomization>();
}

void FMetaHumanPerformanceCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	UMetaHumanPerformance* Performance = nullptr;

	// Get the performance object that we're building the details panel for.
	if (!InDetailBuilder.GetSelectedObjects().IsEmpty())
	{
		Performance = Cast<UMetaHumanPerformance>(InDetailBuilder.GetSelectedObjects()[0].Get());
	}
	else
	{
		return;
	}

	TSharedPtr<SWidget> NameWidget;
	TSharedPtr<SWidget> ValueWidget;

	TSharedRef<IPropertyHandle> CameraProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanPerformance, Camera));
	IDetailPropertyRow* CameraRow = InDetailBuilder.EditDefaultProperty(CameraProperty);
	check(CameraRow);

	CameraRow->GetDefaultWidgets(NameWidget, ValueWidget);

	TSharedRef<SMetaHumanCameraCombo> CameraCombo = SNew(SMetaHumanCameraCombo, &Performance->CameraNames, &Performance->Camera, Performance, CameraProperty.ToSharedPtr());
	Performance->OnSourceDataChanged().AddSP(CameraCombo, &SMetaHumanCameraCombo::HandleSourceDataChanged);

	CameraRow->CustomWidget()
		.NameContent()
		[
			NameWidget.ToSharedRef()
		]
		.ValueContent()
		.MinDesiredWidth(250.0f)
		.MaxDesiredWidth(0.0f)
		[
			CameraCombo
		];

	TSharedRef<IPropertyHandle> SkipPreviewProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanPerformance, bSkipPreview));
	IDetailPropertyRow* SkipPreviewRow = InDetailBuilder.EditDefaultProperty(SkipPreviewProperty);
	check(SkipPreviewRow);

	SkipPreviewRow->GetDefaultWidgets(NameWidget, ValueWidget);

	SkipPreviewRow->CustomWidget()
		.NameContent()
		[
			NameWidget.ToSharedRef()
		]
		.ValueContent()
		.MinDesiredWidth(250.0f)
		.MaxDesiredWidth(0.0f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([Performance]()
			{
				return (Performance && Performance->bSkipPreview && Performance->SolveType != ESolveType::Preview) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([SkipPreviewProperty](ECheckBoxState InState)
			{
				SkipPreviewProperty->SetValue(InState == ECheckBoxState::Checked);
			})
			.IsEnabled_Lambda([Performance, SkipPreviewProperty]()
			{
				return Performance && Performance->CanEditChange(SkipPreviewProperty->GetProperty());
			})
			.ToolTipText_Lambda([SkipPreviewProperty]()
			{ 
				FText BoolValueText;
				SkipPreviewProperty->GetValueAsDisplayText(BoolValueText);
				return BoolValueText;
			})
		];

	TSharedRef<IPropertyHandle> SkipTongueSolveProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanPerformance, bSkipTongueSolve));
	IDetailPropertyRow* SkipTongueSolveRow = InDetailBuilder.EditDefaultProperty(SkipTongueSolveProperty);
	check(SkipTongueSolveRow);

	SkipTongueSolveRow->GetDefaultWidgets(NameWidget, ValueWidget);

	SkipTongueSolveRow->CustomWidget()
		.NameContent()
		[
			NameWidget.ToSharedRef()
		]
		.ValueContent()
		.MinDesiredWidth(250.0f)
		.MaxDesiredWidth(0.0f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([Performance]()
			{
				return (Performance && Performance->bSkipTongueSolve && Performance->GetAudioForProcessing()) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([SkipTongueSolveProperty](ECheckBoxState InState)
			{
				SkipTongueSolveProperty->SetValue(InState == ECheckBoxState::Checked);
			})
			.IsEnabled_Lambda([Performance, SkipTongueSolveProperty]()
			{
				return Performance && Performance->CanEditChange(SkipTongueSolveProperty->GetProperty());
			})
			.ToolTipText_Lambda([SkipTongueSolveProperty]()
			{
				FText BoolValueText;
				SkipTongueSolveProperty->GetValueAsDisplayText(BoolValueText);
				return BoolValueText;
			})
		];


	TSharedRef<IPropertyHandle> SkipPerVertexSolveProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanPerformance, bSkipPerVertexSolve));
	IDetailPropertyRow* SkipPerVertexSolveRow = InDetailBuilder.EditDefaultProperty(SkipPerVertexSolveProperty);
	check(SkipPerVertexSolveRow);

	SkipPerVertexSolveRow->GetDefaultWidgets(NameWidget, ValueWidget);

	SkipPerVertexSolveRow->CustomWidget()
		.NameContent()
		[
			NameWidget.ToSharedRef()
		]
		.ValueContent()
		.MinDesiredWidth(250.0f)
		.MaxDesiredWidth(0.0f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([Performance]()
			{
				return (Performance && Performance->bSkipPerVertexSolve && Performance->FootageCaptureData) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([SkipPerVertexSolveProperty](ECheckBoxState InState)
			{
				SkipPerVertexSolveProperty->SetValue(InState == ECheckBoxState::Checked);
			})
			.IsEnabled_Lambda([Performance, SkipPerVertexSolveProperty]()
			{
				return Performance && Performance->CanEditChange(SkipPerVertexSolveProperty->GetProperty());
			})
			.ToolTipText_Lambda([SkipPerVertexSolveProperty]()
			{
				FText BoolValueText;
				SkipPerVertexSolveProperty->GetValueAsDisplayText(BoolValueText);
				return BoolValueText;
			})
		];

	TSharedRef<IPropertyHandle> UserExcludedFramesProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanPerformance, UserExcludedFrames));
	TSharedRef<IPropertyHandle> ProcessingExcludedFramesProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanPerformance, ProcessingExcludedFrames));

	IDetailCategoryBuilder& ExcludedFramesCategory = InDetailBuilder.EditCategory(UserExcludedFramesProperty->GetDefaultCategoryName());

	ExcludedFramesCategory.AddCustomBuilder(MakeShareable(new FFrameRangeArrayBuilder(UserExcludedFramesProperty, Performance->UserExcludedFrames, &Performance->OnGetCurrentFrame())), false);
	ExcludedFramesCategory.AddCustomBuilder(MakeShareable(new FFrameRangeArrayBuilder(ProcessingExcludedFramesProperty, Performance->ProcessingExcludedFrames)), false);

	// Update edit conditions based on data input type.
	// Need to set an edit condition when input type uses a head movement reference frame. But need to use edit condition hides to hide properties when using other types.
	bool bShowHeadMovementReferenceFrameDetails = (Performance->InputType == EDataInputType::DepthFootage || Performance->InputType == EDataInputType::MonoFootage);
	
	TSharedRef<IPropertyHandle> HeadMovementReferenceFrameProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanPerformance, HeadMovementReferenceFrame));
	TSharedRef<IPropertyHandle> AutoChooseReferenceFrameProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanPerformance, bAutoChooseHeadMovementReferenceFrame));
	IDetailPropertyRow* HeadMovementReferenceFrameRow = InDetailBuilder.EditDefaultProperty(HeadMovementReferenceFrameProperty);
	IDetailPropertyRow* AutoChooseReferenceFrameRow = InDetailBuilder.EditDefaultProperty(AutoChooseReferenceFrameProperty);

	if (bShowHeadMovementReferenceFrameDetails)
	{
		HeadMovementReferenceFrameRow->EditCondition(TAttribute<bool>::CreateLambda([Performance] { return !Performance->bAutoChooseHeadMovementReferenceFrame; }), {});
		AutoChooseReferenceFrameRow->EditCondition(TAttribute<bool>::CreateLambda([] { return true; }), {});

		HeadMovementReferenceFrameRow->EditConditionHides(false);
		AutoChooseReferenceFrameRow->EditConditionHides(false);
	}
	else
	{
		HeadMovementReferenceFrameRow->EditCondition(TAttribute<bool>::CreateLambda([] { return false; }), {});
		AutoChooseReferenceFrameRow->EditCondition(TAttribute<bool>::CreateLambda([] { return false; }), {});

		HeadMovementReferenceFrameRow->EditConditionHides(true);
		AutoChooseReferenceFrameRow->EditConditionHides(true);
	}

	const bool bShowNeutralPoseCalibrationDetails = Performance->InputType == EDataInputType::MonoFootage;
	TSharedRef<IPropertyHandle> NeutralPoseCalibrationEnabledProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanPerformance, bNeutralPoseCalibrationEnabled));
	TSharedRef<IPropertyHandle> NeutralPoseCalibrationFrameProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanPerformance, NeutralPoseCalibrationFrame));
	TSharedRef<IPropertyHandle> NeutralPoseCalibrationAlphaProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanPerformance, NeutralPoseCalibrationAlpha));
	TSharedRef<IPropertyHandle> NeutralPoseCalibrationCurvesProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanPerformance, NeutralPoseCalibrationCurves));
	IDetailPropertyRow* NeutralPoseCalibrationEnabledRow = InDetailBuilder.EditDefaultProperty(NeutralPoseCalibrationEnabledProperty);
	IDetailPropertyRow* NeutralPoseCalibrationFrameRow = InDetailBuilder.EditDefaultProperty(NeutralPoseCalibrationFrameProperty);
	IDetailPropertyRow* NeutralPoseCalibrationAlphaRow = InDetailBuilder.EditDefaultProperty(NeutralPoseCalibrationAlphaProperty);
	IDetailPropertyRow* NeutralPoseCalibrationCurvesRow = InDetailBuilder.EditDefaultProperty(NeutralPoseCalibrationCurvesProperty);

	if (bShowNeutralPoseCalibrationDetails)
	{
		NeutralPoseCalibrationEnabledRow->EditCondition(true, {});
		NeutralPoseCalibrationFrameRow->EditCondition(TAttribute<bool>::CreateLambda([Performance] { return Performance->bNeutralPoseCalibrationEnabled; }), {});
		NeutralPoseCalibrationAlphaRow->EditCondition(TAttribute<bool>::CreateLambda([Performance] { return Performance->bNeutralPoseCalibrationEnabled; }), {});
		NeutralPoseCalibrationCurvesRow->EditCondition(TAttribute<bool>::CreateLambda([Performance] { return Performance->bNeutralPoseCalibrationEnabled; }), {});

		NeutralPoseCalibrationEnabledRow->EditConditionHides(false);
		NeutralPoseCalibrationFrameRow->EditConditionHides(false);
		NeutralPoseCalibrationAlphaRow->EditConditionHides(false);
		NeutralPoseCalibrationCurvesRow->EditConditionHides(false);
	}
	else
	{
		NeutralPoseCalibrationEnabledRow->EditCondition(false, {});
		NeutralPoseCalibrationFrameRow->EditCondition(false, {});
		NeutralPoseCalibrationAlphaRow->EditCondition(false, {});
		NeutralPoseCalibrationCurvesRow->EditCondition(false, {});

		NeutralPoseCalibrationEnabledRow->EditConditionHides(true);
		NeutralPoseCalibrationFrameRow->EditConditionHides(true);
		NeutralPoseCalibrationAlphaRow->EditConditionHides(true);
		NeutralPoseCalibrationCurvesRow->EditConditionHides(true);
	}

	bool bShowAudioChannelDetail = Performance->InputType == EDataInputType::Audio && !Performance->bRealtimeAudio;
	TSharedRef<IPropertyHandle> AudioChannelProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanPerformance, AudioChannelIndex));
	IDetailPropertyRow* AudioChannelRow = InDetailBuilder.EditDefaultProperty(AudioChannelProperty);

	if (bShowAudioChannelDetail)
	{
		AudioChannelRow->EditCondition(TAttribute<bool>::CreateLambda([Performance] { return !Performance->bDownmixChannels; }), {});
		AudioChannelRow->EditConditionHides(false);
	}
	else
	{
		AudioChannelRow->EditCondition(TAttribute<bool>::CreateLambda([] { return false; }), {});
		AudioChannelRow->EditConditionHides(true);
	}

	IDetailCategoryBuilder& DataCategory = InDetailBuilder.EditCategory(TEXT("Data"));
	IDetailCategoryBuilder& VisualizationCategory = InDetailBuilder.EditCategory(TEXT("Visualization"));
	IDetailCategoryBuilder& ProcessingCategory = InDetailBuilder.EditCategory(TEXT("Processing Parameters"));
	IDetailCategoryBuilder& DiagnosticsCategory = InDetailBuilder.EditCategory(TEXT("Processing Diagnostics"));

	DataCategory.SetSortOrder(1000);
	VisualizationCategory.SetSortOrder(1001);
	ProcessingCategory.SetSortOrder(1002);
	ExcludedFramesCategory.SetSortOrder(1003);
	DiagnosticsCategory.SetSortOrder(1004);

	TSharedRef<IPropertyHandle> ControlRigClassProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanPerformance, ControlRigClass));

	IDetailPropertyRow& ControlRigClassRow = InDetailBuilder.AddPropertyToCategory(ControlRigClassProperty);
	constexpr int32 NumImagesInPool = 16;
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool = MakeShared<FAssetThumbnailPool>(16);

	ControlRigClassRow.GetDefaultWidgets(NameWidget, ValueWidget);

	ControlRigClassRow.CustomWidget()
		.NameContent()
		[
			NameWidget.ToSharedRef()
		]
		.ValueContent()
		.MinDesiredWidth(250.0f)
		.MaxDesiredWidth(0.0f)
		[
			SNew(SObjectPropertyEntryBox)
			.PropertyHandle(ControlRigClassProperty)
			.DisplayThumbnail(true)
			.ThumbnailPool(ThumbnailPool)
			.AllowCreate(false)
			.AllowClear(false)
			.AllowedClass(UControlRigBlueprint::StaticClass())
			.OnObjectChanged_Lambda([ControlRigClassProperty](const FAssetData& InAssetData)
			{
				if (InAssetData.IsValid())
				{ 
					if (InAssetData.IsInstanceOf(UControlRigBlueprint::StaticClass()))
					{
						ControlRigClassProperty->SetValue(Cast<UControlRigBlueprint>(InAssetData.GetAsset())->GetControlRigClass());
					}
					else if (InAssetData.IsInstanceOf(URigVMBlueprintGeneratedClass::StaticClass()))
					{
						ControlRigClassProperty->SetValue(Cast<URigVMBlueprintGeneratedClass>(InAssetData.GetAsset()));
					}
					else
					{
						UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Unsupported ControlRig class"));
					}
				}
				else
				{
					UE_LOG(LogMetaHumanPerformance, Warning, TEXT("Invalid ControlRig asset"));
				}
			})
			.OnShouldFilterAsset(this, &FMetaHumanPerformanceCustomization::ShouldFilterControlRigAsset)
		];

	TSharedRef<IPropertyHandle> FocalLengthProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaHumanPerformance, FocalLength));
	IDetailPropertyRow& FocalLengthRow = InDetailBuilder.AddPropertyToCategory(FocalLengthProperty);

	FocalLengthRow.GetDefaultWidgets(NameWidget, ValueWidget);

	FocalLengthRow.CustomWidget()
		.NameContent()
		[
			NameWidget.ToSharedRef()
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 5, 0, 0)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ToolTipText_Lambda([Performance]()
				{
					if (Performance->FocalLength < 0)
					{
						return LOCTEXT("FocalNotSetTooltip", "Focal length is set when the \"Estimate\" button is pressed");
					}
					else
					{
						return FText::FromString(FString::Printf(TEXT("%.2f pixels"), Performance->FocalLength));
					}
				})
				.Text_Lambda([Performance]()
				{
					if (Performance->FocalLength < 0)
					{ 
						return LOCTEXT("FocalNotSet", "Not Set");
					}
					else
					{
						return FText::FromString(FString::Printf(TEXT("%.2f px"), Performance->FocalLength));
					}
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(10, 0, 0, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("FocalEstimate", "Estimate"))
				.IsEnabled_Lambda([Performance]()
				{
					return Performance->CanProcess();
				})
				.OnClicked_Lambda([Performance]()
				{
					FString ErrorMessage;
					if (!Performance->EstimateFocalLength(ErrorMessage))
					{
						FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(TEXT("Failed to estimate focal length:\n%s"), *ErrorMessage)));
					}

					return FReply::Handled();
				})
			]
		];
}

bool FMetaHumanPerformanceCustomization::ShouldFilterControlRigAsset(const FAssetData& InAssetData) const
{
	const IAssetRegistry& AssetRegistry = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	// Using Tags to ready properties from the asset without actually loading it. This allows the dropdown to load
	// without the penalty hit of loading every control rig asset in memory
	const FAssetDataTagMapSharedView::FFindTagResult Tag = InAssetData.TagsAndValues.FindTag(TEXT("SupportedEventNames"));
	if (Tag.IsSet())
	{
		TArray<FString> SupportedEventNames;
		constexpr bool bCullEmpty = true;
		Tag.GetValue().ParseIntoArray(SupportedEventNames, TEXT(","), bCullEmpty);

		bool bSupportsBackwardsSolve = false;

		for (const FString& EventName : SupportedEventNames)
		{
			if (EventName.Contains(TEXT("Backwards Solve")) || EventName.Contains(TEXT("Inverse")))
			{
				bSupportsBackwardsSolve = true;
			}
		}

		return !bSupportsBackwardsSolve;
	}

	// Returning true means the asset will not be displayed in the dropdown menu
	return true;
}

#undef LOCTEXT_NAMESPACE

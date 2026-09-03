// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEditorViewportToolBarBase.h"
#include "SEditorViewport.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "EditorViewportClient.h"

#define LOCTEXT_NAMESPACE "DataflowEditorViewportToolBarBase"

TSharedRef<SWidget> SDataflowEditorViewportToolBarBase::GenerateCameraSpeedSettingsMenu() const
{
	// This comes from STransformViewportToolBar::FillCameraSpeedMenu
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Menu.Background")))
		[
			SNew( SVerticalBox )
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding( FMargin(8.0f, 2.0f, 60.0f, 2.0f) )
			.HAlign( HAlign_Left )
			[
				SNew( STextBlock )
				.Text( LOCTEXT("MouseSettingsCamSpeed", "Camera Speed")  )
				.Font( FAppStyle::GetFontStyle( TEXT( "MenuItem.Font" ) ) )
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding( FMargin(8.0f, 4.0f) )
			[	
				SNew( SHorizontalBox )
				+SHorizontalBox::Slot()
				.FillWidth(1)
				.Padding( FMargin(0.0f, 2.0f) )
				[
					SNew( SBox )
					.MinDesiredWidth(220)
					[
						SNew(SSlider)
						.Value(this, &SDataflowEditorViewportToolBarBase::GetCamSpeedSliderPosition)
						.OnValueChanged(this, &SDataflowEditorViewportToolBarBase::OnSetCamSpeed)
					]
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( 8.0f, 2.0f, 0.0f, 2.0f)
				[
					SNew( SBox )
					.WidthOverride(40)
					[
						SNew( STextBlock )
						.Text(this, &SDataflowEditorViewportToolBarBase::GetCameraSpeedLabel)
						.Font( FAppStyle::GetFontStyle( TEXT( "MenuItem.Font" ) ) )
					]
				]
			] // Camera Speed Scalar
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(8.0f, 2.0f, 60.0f, 2.0f))
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MouseSettingsCamSpeedScalar", "Camera Speed Scalar"))
				.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(8.0f, 4.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1)
				.Padding(FMargin(0.0f, 2.0f))
				[
					SNew(SSpinBox<float>)
					.MinValue(1.0f)
 					.MaxValue(std::numeric_limits<float>::max())
					.MinSliderValue(1.0f)
					.MaxSliderValue(128.0f)
					.Value(this, &SDataflowEditorViewportToolBarBase::GetCamSpeedScalarBoxValue)
					.OnValueChanged(this, &SDataflowEditorViewportToolBarBase::OnSetCamSpeedScalarBoxValue)
					.ToolTipText(LOCTEXT("CameraSpeedScalar_ToolTip", "Scalar to increase camera movement range"))
				]
			]
		];
}

FText SDataflowEditorViewportToolBarBase::GetCameraSpeedLabel() const
{
	const float CameraSpeed = GetViewportClient().GetCameraSpeed();
	FNumberFormattingOptions FormattingOptions = FNumberFormattingOptions::DefaultNoGrouping();
	FormattingOptions.MaximumFractionalDigits = CameraSpeed > 1 ? 1 : 3;
	return FText::AsNumber(CameraSpeed, &FormattingOptions);
}

float SDataflowEditorViewportToolBarBase::GetCamSpeedSliderPosition() const
{
	return (GetViewportClient().GetCameraSpeedSetting() - 1) / ((float)FEditorViewportClient::MaxCameraSpeeds - 1);
}

void SDataflowEditorViewportToolBarBase::OnSetCamSpeed(float NewValue) const
{
	const int32 OldSpeedSetting = GetViewportClient().GetCameraSpeedSetting();
	const int32 NewSpeedSetting = NewValue * ((float)FEditorViewportClient::MaxCameraSpeeds - 1) + 1;

	if (OldSpeedSetting != NewSpeedSetting)
	{
		GetViewportClient().SetCameraSpeedSetting(NewSpeedSetting);
	}
}

float SDataflowEditorViewportToolBarBase::GetCamSpeedScalarBoxValue() const
{
	return GetViewportClient().GetCameraSpeedScalar();
}

void SDataflowEditorViewportToolBarBase::OnSetCamSpeedScalarBoxValue(float NewValue) const
{
	GetViewportClient().SetCameraSpeedScalar(NewValue);
}

TSharedRef<SWidget> SDataflowEditorViewportToolBarBase::GenerateOptionsMenu() const
{
	GetInfoProvider().OnFloatingButtonClicked();
	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();

	const bool bIsPerspective = GetViewportClient().GetViewportType() == LVT_Perspective;

	constexpr bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder OptionsMenuBuilder(bInShouldCloseWindowAfterMenuSelection, ViewportRef->GetCommandList());
	{
		OptionsMenuBuilder.BeginSection("DataflowEditorViewportViewportOptions", LOCTEXT("OptionsMenuHeader", "Viewport Options"));
		{
			if (bIsPerspective)
			{
				OptionsMenuBuilder.AddWidget(GenerateFOVMenu(), LOCTEXT("FOVAngle", "Field of View (H)"));
			}

			OptionsMenuBuilder.AddSubMenu(
				LOCTEXT("CameraSpeedSettings", "Camera Speed Settings"),
				LOCTEXT("CameraSpeedSettingsToolTip", "Adjust camera speed settings"),
				FNewMenuDelegate::CreateLambda([this](FMenuBuilder& MenuBuilder)
					{
						MenuBuilder.AddWidget(GenerateCameraSpeedSettingsMenu(), FText());
					}
				));

		}
		OptionsMenuBuilder.EndSection();

		ExtendOptionsMenu(OptionsMenuBuilder);
	}

	return OptionsMenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE

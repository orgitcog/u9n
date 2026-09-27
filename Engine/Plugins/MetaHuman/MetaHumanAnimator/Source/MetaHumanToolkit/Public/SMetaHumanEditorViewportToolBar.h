// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SViewportToolBar.h"
#include "SEditorViewportViewMenuContext.h"
#include "MetaHumanABCommandList.h"

#include "Widgets/SWidget.h"
#include "Widgets/Input/SSpinBox.h"

#include "SMetaHumanEditorViewportToolBar.generated.h"

/** A delegate that is executed when adding menu content. */
DECLARE_DELEGATE_OneParam(FOnCamSpeedChanged, int32);
DECLARE_DELEGATE_OneParam(FOnCamSpeedScalarChanged, float);
DECLARE_DELEGATE_TwoParams(FOnGetABMenuContents, enum class EABImageViewMode InABViewMode, FMenuBuilder& InMenuBuilder)

class METAHUMANTOOLKIT_API SMetaHumanEditorViewportToolBar
	: public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanEditorViewportToolBar) {}

		SLATE_ARGUMENT(TSharedPtr<class FUICommandList>, ViewportCommandList)

		SLATE_ARGUMENT(FMetaHumanABCommandList, ABCommandList)

		SLATE_ARGUMENT(TSharedPtr<class FMetaHumanEditorViewportClient>, ViewportClient)

		SLATE_EVENT(FOnGetABMenuContents, OnGetABMenuContents)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	EVisibility GetShowAVisibility() const;
	EVisibility GetShowBVisibility() const;

private:
	TSharedRef<SWidget> CreateViewMenuWidget(enum class EABImageViewMode InViewMode);
	TSharedRef<SWidget> CreateViewMixToggleWidget();
	TSharedRef<SWidget> CreateABToggleWidget();
	TSharedRef<SWidget> CreateCameraOptionsToolbarButtonWidget();
	TSharedRef<SWidget> FillDisplayOptionsForViewMenu(enum class EABImageViewMode InViewMode);

	/** View Mix */
	void EnterSingleScreenView() {}
	void EnterMultiScreenView() {}
	void EnterSplitScreenView() {}

	bool MultiScreenViewIsChecked() const { return false; }
	bool SplitScreenViewIsChecked() const { return false; }
	bool SingleViewIsChecked() const { return false; }

	/** Field of View options */
	bool CanChangeFOV() const;
	TOptional<float> GetFOVValue() const;
	void HandleFOVValueChanged(float InNewValue);

	/** Depth data */
	bool CanChangeFootageDepthData() const;
	TOptional<float> GetFootageDepthDataNear() const;
	void HandleFootageDepthDataNearChanged(float InNewValue);
	TOptional<float> GetFootageDepthDataFar() const;
	void HandleFootageDepthDataFarChanged(float InNewValue);

private:

	static const FMargin ToolbarSlotPadding;

	TSharedPtr<class SHorizontalBox> ToolbarMenuHorizontalBox;
	TSharedPtr<class FUICommandList> ViewportCommandList;
	FMetaHumanABCommandList ABCommandList;
	TSharedPtr<class FMetaHumanEditorViewportClient> ViewportClient;

	FOnGetABMenuContents OnGetABMenuContentsDelegate;

	/** Camera speed Label callback */
	FText GetCameraSpeedLabel() const;

	/** Creates the widget to display the camera controls */
	TSharedRef<SWidget> CreateCameraOptionsDropDownMenuWidget();

	/** Returns the current camera speed setting */
	float GetCamSpeedSliderPosition() const;

	/**
	 * Sets new camera speed
	 *
	 * @Param	NewValue	Value to set camera speed too
	 */
	void OnSetCamSpeed(float NewValue);

	/** Returns the current camera speed scalar setting */
	float GetCamSpeedScalarBoxValue() const;

	/**
	* Sets new camera speed scalar
	*
	* @Param	NewValue	Value to set for the camera speed scalar
	*/
	void OnSetCamSpeedScalarBoxValue(float NewValue);

	/** Camera speed scalar Label callback */
	FText GetCameraSpeedScalarLabel() const;

	/** Called when the camera speed is changed */
	FOnCamSpeedChanged OnCamSpeedChanged;
	FOnCamSpeedScalarChanged OnCamSpeedScalarChanged;

	/** Reference to the camera slider used to display current camera speed */
	TSharedPtr<class SSlider> CamSpeedSlider;

	/** Reference to the camera spinbox used to display current camera speed scalar */
	mutable TSharedPtr<SSpinBox<float>> CamSpeedScalarBox;

	FText GetABToggleButtonATooltip() const;
	FText GetABToggleButtonBTooltip() const;
	FText GetABToggleButtonTooltip(FText InDefaultTooltipText) const;
};

UCLASS()
class UMetaHumanEditorViewportViewMenuContext
	: public UEditorViewportViewMenuContext
{
	GENERATED_BODY()

public:

	TWeakPtr<const class SMetaHumanViewportViewMenu> MetaHumanViewportViewMenu;
};

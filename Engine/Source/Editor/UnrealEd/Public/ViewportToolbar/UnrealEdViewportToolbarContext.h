// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineBaseTypes.h"
#include "Framework/Docking/TabManager.h"
#include "ShowFlags.h"
#include "Templates/SharedPointer.h"
#include "UObject/Object.h"

#include "UnrealEdViewportToolbarContext.generated.h"

class FAssetEditorToolkit;
class IPreviewProfileController;
enum ERotationGridMode : int;

namespace UE::UnrealEd
{

DECLARE_DELEGATE_RetVal_OneParam(bool, IsViewModeSupportedDelegate, EViewModeIndex);

enum EHidableViewModeMenuSections : uint8;
DECLARE_DELEGATE_RetVal_OneParam(bool, DoesViewModeMenuShowSectionDelegate, EHidableViewModeMenuSections);

}

class SEditorViewport;

UCLASS()
class UNREALED_API UUnrealEdViewportToolbarContext : public UObject
{
	GENERATED_BODY()

public:
	TWeakPtr<SEditorViewport> Viewport;
	UE::UnrealEd::IsViewModeSupportedDelegate IsViewModeSupported;
	UE::UnrealEd::DoesViewModeMenuShowSectionDelegate DoesViewModeMenuShowSection;

	/**
	 * Whether the current editor should show menu entries to select between coordinate systems (e.g. Local vs World)
	 */
	bool bShowCoordinateSystemControls = true;

	/**
	 * Add flags to this array to remove them from the Show Menu
	 */
	TArray<FEngineShowFlags::EShowFlag> ExcludedShowMenuFlags;

	/**
	 * Can be used to retrieve data e.g. TabManager
	 */
	TWeakPtr<FAssetEditorToolkit> AssetEditorToolkit;

	/**
	 * Identifier for the Preview Settings Tab
	 */
	FTabId PreviewSettingsTabId;
	
	virtual TSharedPtr<IPreviewProfileController> GetPreviewProfileController() const;
	
	bool bShowSurfaceSnap = true;
	
	virtual void RefreshViewport();
	
	virtual FText GetGridSnapLabel() const;
	virtual TArray<float> GetGridSnapSizes() const;
	virtual bool IsGridSnapSizeActive(int32 GridSizeIndex) const;
	virtual void SetGridSnapSize(int32 GridSizeIndex);
	
	virtual FText GetRotationSnapLabel() const;
	virtual bool IsRotationSnapActive(int32 RotationIndex, ERotationGridMode RotationMode) const;
	virtual void SetRotationSnapSize(int32 RotationIndex, ERotationGridMode RotationMode);
	
	virtual FText GetScaleSnapLabel() const;
	virtual TArray<float> GetScaleSnapSizes() const;
	virtual bool IsScaleSnapActive(int32 ScaleIndex) const;
	virtual void SetScaleSnapSize(int32 ScaleIndex);
};

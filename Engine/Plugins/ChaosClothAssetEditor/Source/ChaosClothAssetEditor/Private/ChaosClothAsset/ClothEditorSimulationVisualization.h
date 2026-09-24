// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/BitArray.h"
#include "Templates/SharedPointer.h"

class FMenuBuilder;
class UChaosClothComponent;
class FPrimitiveDrawInterface;
class FCanvas;
class FSceneView;
class STextComboBox;
class FDataflowSimulationViewportClient;

namespace ESelectInfo
{
enum Type : int;
}

namespace UE::Chaos::ClothAsset
{

class FChaosClothAssetEditor3DViewportClient;

class FClothEditorSimulationVisualization
{
public:
	FClothEditorSimulationVisualization();
	   
	void ExtendViewportShowMenu(FMenuBuilder& MenuBuilder, const TSharedRef<FChaosClothAssetEditor3DViewportClient>& ViewportClient);
	void ExtendViewportShowMenu(FMenuBuilder& MenuBuilder, const TSharedRef<FDataflowSimulationViewportClient>& ViewportClient);

	void DebugDrawSimulation(const UChaosClothComponent* ClothComponent, FPrimitiveDrawInterface* PDI);
	void DebugDrawSimulationTexts(const UChaosClothComponent* ClothComponent, FCanvas* Canvas, const FSceneView* SceneView);
	FText GetDisplayString(const UChaosClothComponent* ClothComponent) const;
	void RefreshMenusForClothComponent(const UChaosClothComponent* ClothComponent);

	// WeightMaps 
	const FString* GetCurrentlySelectedWeightMap() const { return WeightMapSelection.CurrentlySelectedName.Get(); }
	void ExtendViewportShowMenuWeightMapSelector(FMenuBuilder& MenuBuilder)
	{
		ExtendViewportShowMenuNameSelector(MenuBuilder, WeightMapSelection);
	}
	// Morph Targets
	const FString* GetCurrentlySelectedMorphTarget() const { return MorphTargetSelection.CurrentlySelectedName.Get(); }
	void ExtendViewportShowMenuMorphTargetSelector(FMenuBuilder& MenuBuilder)
	{
		ExtendViewportShowMenuNameSelector(MenuBuilder, MorphTargetSelection);
	}
	// Normals
	void ExtendViewportShowMenuPointNormalsLength(FMenuBuilder& MenuBuilder)
	{
		ExtendViewportShowMenuSpinBox(MenuBuilder, PointNormalLength, 0.f, FLT_MAX, 0.f, 40.f);
	}
	void ExtendViewportShowMenuAnimatedNormalsLength(FMenuBuilder& MenuBuilder)
	{
		ExtendViewportShowMenuSpinBox(MenuBuilder, AnimatedNormalLength, 0.f, FLT_MAX, 0.f, 40.f);
	}
	void ExtendViewportShowMenuAerodynamicsLengthScale(FMenuBuilder& MenuBuilder)
	{
		ExtendViewportShowMenuSpinBox(MenuBuilder, AerodynamicsLengthScale, 0.f, FLT_MAX, 0.f, 40.f);
	}
	float GetPointNormalLength() const { return PointNormalLength; }
	float GetAnimatedNormalLength() const { return AnimatedNormalLength; }
	float GetAerodynamicsLengthScale() const { return AerodynamicsLengthScale; }

private:
	struct FNameSelectionData
	{
		TSharedPtr<STextComboBox> Selector;
		TArray<TSharedPtr<FString>> Names;
		TSharedPtr<FString> CurrentlySelectedName;
	};


	void ExtendViewportShowMenu(FMenuBuilder& MenuBuilder, const TFunction<UChaosClothComponent*()>& GetClothComponentFunc);

	/** Return whether or not - given the current enabled options - the simulation should be disabled. */
	bool ShouldDisableSimulation() const;
	/** Show/hide all cloth sections for the specified mesh compoment. */
	void ShowClothSections(UChaosClothComponent* ClothComponent, bool bIsClothSectionsVisible) const;
	void ExtendViewportShowMenuSpinBox(FMenuBuilder& MenuBuilder, float& Value, const float MinValue, const float MaxValue, const float MinSliderValue, const float MaxSliderValue);
	void ExtendViewportShowMenuNameSelector(FMenuBuilder& MenuBuilder, FNameSelectionData& SelectionData);

private:
	/** Flags used to store the checked status for the visualization options. */
	TBitArray<> Flags;
	FNameSelectionData WeightMapSelection;
	FNameSelectionData MorphTargetSelection;
	float PointNormalLength = 20.f;
	float AnimatedNormalLength = 20.f;
	float AerodynamicsLengthScale = 10.f;
	
};
} // namespace UE::Chaos::ClothAsset

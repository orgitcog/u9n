// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Input/Reply.h"
#include "Layout/Visibility.h"

class UMLDeformerModel;
class UMLDeformerTrainingDataProcessorSettings;
class USkeleton;
struct FAssetData;

namespace UE::MLDeformer::TrainingDataProcessor
{
	/**
	 * The detail customization class for the UMLDeformerTrainingDataProcessorSettings class.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API FTrainingDataProcessorSettingsDetailCustomization final : public IDetailCustomization
	{
	public:
		static TSharedRef<IDetailCustomization> MakeInstance();

		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	private:
		EVisibility GetNoInputBonesWarningVisibility() const;
		EVisibility GetNoFramesWarningVisibility() const;
		EVisibility GetSkeletonMismatchErrorVisibility() const;
		
		FReply OnCreateNewButtonClicked() const;
		int32 GetTotalNumInputFrames() const;
		bool FilterAnimSequences(const FAssetData& AssetData) const;
		
		static void Refresh(IDetailLayoutBuilder* DetailBuilder);
		static FString FindDefaultAnimSequencePath(const UMLDeformerModel* Model);

	private:
		TWeakObjectPtr<UMLDeformerTrainingDataProcessorSettings> TrainingDataProcessorSettings;
	};
} // namespace UE::MLDeformer::TrainingDataProcessor

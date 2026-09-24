// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "MLDeformerModel.h"
#include "Misc/NotifyHook.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;

namespace UE::MLDeformer::TrainingDataProcessor
{
	/**
	 * The main widget for the training data processor tool.
	 * This widget is basically what's inside the tab when this tool opens.
	 * It contains a detail view and generate button.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API STrainingDataProcessorMainWidget final : public SCompoundWidget, public FEditorUndoClient, public FNotifyHook
	{
	public:
		SLATE_BEGIN_ARGS(STrainingDataProcessorMainWidget) { }
			SLATE_ARGUMENT(TObjectPtr<UMLDeformerModel>, Model)
		SLATE_END_ARGS()

		virtual ~STrainingDataProcessorMainWidget() override;
		void Construct(const FArguments& InArgs);

		//~ Begin FEditorUndoClient overrides.
		virtual void PostUndo(bool bSuccess) override;
		virtual void PostRedo(bool bSuccess) override;
		//~ End FEditorUndoClient overrides.

		//~ Begin FNotifyHook overrides.
		virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;
		//~ End FNotifyHook overrides.

	private:
		FReply OnGenerateButtonClicked() const;
		bool IsValidConfiguration() const;
		bool IsObjectOfInterest(UObject* Object) const;
		void OnAssetModified(UObject* Object) const;
		void Refresh() const;

	private:
		/** The details view that shows the properties of our UMLDeformerTrainingDataProcessorSettings. */
		TSharedPtr<IDetailsView> DetailsView;

		/** A pointer to our model. */
		TObjectPtr<UMLDeformerModel> Model;

		/** The delegate that handles when an object got modified (any object). */
		FDelegateHandle ObjectModifiedHandle;
	};
} // namespace UE::MLDeformer::TrainingDataProcessor

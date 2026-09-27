// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Widgets/SCompoundWidget.h"


class ISceneOutliner;
class SSceneOutliner;
class SHorizontalBox;
struct FTypedElementWidgetConstructor;

namespace UE::Editor::DataStorage
{
	class STedsTableViewer;
	class FTedsTableViewerColumn;
	class SRowDetails;
	class IUiProvider;

	namespace QueryStack
	{
		class FRowViewNode;
	}

	namespace Debug::QueryEditor
	{
		class FTedsQueryEditorModel;

		class SResultsView : public SCompoundWidget
		{
		public:
			SLATE_BEGIN_ARGS( SResultsView ){}
			SLATE_END_ARGS()

			~SResultsView() override;
			void Construct(const FArguments& InArgs, FTedsQueryEditorModel& InModel);
			void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

		private:

			void OnModelChanged();
			void CreateRowHandleColumn();
		
			FTedsQueryEditorModel* Model = nullptr;
			FDelegateHandle ModelChangedDelegateHandle;
			bool bModelDirty = true;


			QueryHandle CountQueryHandle = InvalidQueryHandle;
			QueryHandle TableViewerQueryHandle = InvalidQueryHandle;

			TArray<RowHandle> TableViewerRows;
			// We have to keep a TSet copy to have a sorted order for the rows for now
			TSet<RowHandle> TableViewerRows_Set;
			TSharedPtr<STedsTableViewer> TableViewer;
			TSharedPtr<QueryStack::FRowViewNode> RowQueryStack;

			// Custom column for the table viewer to display row handles
			TSharedPtr<FTedsTableViewerColumn> RowHandleColumn;

			// Widget that displays details of a row
			TSharedPtr<SRowDetails> RowDetailsWidget;
			
			IUiProvider* UiProvider = nullptr;
		};

	} // namespace Debug::QueryEditor
} // namespace UE::Editor::DataStorage

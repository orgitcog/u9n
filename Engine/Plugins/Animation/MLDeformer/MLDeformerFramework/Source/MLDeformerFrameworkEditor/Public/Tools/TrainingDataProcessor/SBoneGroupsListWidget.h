// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "Framework/Commands/Commands.h"
#include "MLDeformerTrainingDataProcessorSettings.h"
#include "Styling/SlateColor.h"
#include "EditorUndoClient.h"

class USkeleton;
class FUICommandList;
class FNotifyHook;
struct FReferenceSkeleton;

namespace UE::MLDeformer::TrainingDataProcessor
{
	class SBoneGroupsTreeWidget;
	class SBoneGroupsListWidget;
	
	class MLDEFORMERFRAMEWORKEDITOR_API FBoneGroupsListWidgetCommands final : public TCommands<FBoneGroupsListWidgetCommands>
	{
	public:
		FBoneGroupsListWidgetCommands();
		virtual void RegisterCommands() override;

	public:
		TSharedPtr<FUICommandInfo> CreateGroup;
		TSharedPtr<FUICommandInfo> DeleteSelectedItems;
		TSharedPtr<FUICommandInfo> ClearGroups;
		TSharedPtr<FUICommandInfo> AddBoneToGroup;
	};


	class MLDEFORMERFRAMEWORKEDITOR_API FBoneGroupTreeElement final : public TSharedFromThis<FBoneGroupTreeElement>
	{
	public:
		static TSharedRef<ITableRow> MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable,
		                                               const TSharedRef<FBoneGroupTreeElement>& InTreeElement,
		                                               const TSharedPtr<SBoneGroupsTreeWidget>& InTreeWidget);

		bool IsGroup() const { return GroupIndex != INDEX_NONE; }

	public:
		FString Name;
		TArray<TSharedPtr<FBoneGroupTreeElement>> Children;
		TWeakPtr<FBoneGroupTreeElement> ParentGroup;
		FSlateColor TextColor;
		int32 GroupIndex = INDEX_NONE;
		int32 GroupBoneIndex = INDEX_NONE;
	};


	class MLDEFORMERFRAMEWORKEDITOR_API SBoneGroupTreeRowWidget final : public STableRow<TSharedPtr<FBoneGroupTreeElement>>
	{
	public:
		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, const TSharedRef<FBoneGroupTreeElement>& InTreeElement,
		               const TSharedPtr<SBoneGroupsTreeWidget>& InTreeView);

	private:
		FText GetName() const;

	private:
		TWeakPtr<FBoneGroupTreeElement> WeakTreeElement;
		friend class SBoneGroupsTreeWidget;
	};


	class MLDEFORMERFRAMEWORKEDITOR_API SBoneGroupsTreeWidget final : public STreeView<TSharedPtr<FBoneGroupTreeElement>>
	{
	public:
		friend class SBoneGroupsListWidget;

		SLATE_BEGIN_ARGS(SBoneGroupsTreeWidget)
			{
			}

			SLATE_ARGUMENT(TSharedPtr<SBoneGroupsListWidget>, BoneGroupsWidget)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);
		void Refresh();
		int32 GetNumSelectedGroups() const;

	private:
		//~ Begin STreeView overrides.
		virtual FReply OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
		//~ End STreeView overrides.

		void AddElement(const TSharedPtr<FBoneGroupTreeElement>& Element, const TSharedPtr<FBoneGroupTreeElement>& ParentElement);
		const TArray<TSharedPtr<FBoneGroupTreeElement>>& GetRootElements() const;
		TSharedRef<ITableRow> MakeTableRowWidget(TSharedPtr<FBoneGroupTreeElement> InItem, const TSharedRef<STableViewBase>& OwnerTable);
		TSharedPtr<SWidget> CreateContextMenuWidget() const;
		void UpdateTreeElements();
		void RefreshTree();

		static void HandleGetChildrenForTree(TSharedPtr<FBoneGroupTreeElement> InItem, TArray<TSharedPtr<FBoneGroupTreeElement>>& OutChildren);
		static TSharedPtr<SWidget> CreateContextWidget();

	private:
		TArray<TSharedPtr<FBoneGroupTreeElement>> RootElements;
		TWeakPtr<SBoneGroupsListWidget> BoneGroupsWidget;
	};


	DECLARE_DELEGATE_RetVal(TArray<FMLDeformerTrainingDataProcessorBoneGroup>*, FBoneGroupsListWidgetGetBoneGroups)

	/**
	 * A widget that shows a set of bone groups, and allows you to manage them by creating, removing and editing of groups.
	 * We see a bone group as a list of bone names. Multiple bone groups can exist. If you need only one list of bones
	 * then you can use the bone SBoneListWidget instead.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API SBoneGroupsListWidget final : public SCompoundWidget, public FEditorUndoClient
	{
		friend class SBoneGroupsTreeWidget;

		SLATE_BEGIN_ARGS(SBoneGroupsListWidget)
			{
			}

			SLATE_ARGUMENT(TWeakObjectPtr<USkeleton>, Skeleton)
			SLATE_ARGUMENT(TWeakObjectPtr<UObject>, UndoObject)
			SLATE_EVENT(FBoneGroupsListWidgetGetBoneGroups, GetBoneGroups)
		SLATE_END_ARGS()

	public:
		virtual ~SBoneGroupsListWidget() override;

		void Construct(const FArguments& InArgs, FNotifyHook* InNotifyHook);

	private:
		//~ Begin FEditorUndoClient overrides.
		virtual void PostUndo(bool bSuccess) override;
		virtual void PostRedo(bool bSuccess) override;
		//~ End FEditorUndoClient overrides.

		void BindCommands(const TSharedPtr<FUICommandList>& InCommandList);
		void OnFilterTextChanged(const FText& InFilterText);
		void RefreshTree() const;
		FReply OnAddButtonClicked() const;
		FReply OnClearButtonClicked() const;

		TSharedPtr<SBoneGroupsTreeWidget> GetTreeWidget() const;
		TSharedPtr<FUICommandList> GetCommandList() const;
		TArray<FMLDeformerTrainingDataProcessorBoneGroup>* GetBoneGroupsValues() const;
		TWeakObjectPtr<USkeleton> GetSkeleton() const;
		const FString& GetFilterText() const;

		void OnCreateBoneGroup() const;
		void OnDeleteSelectedItems() const;
		void OnClearBoneGroups() const;
		void OnAddBoneToGroup() const;

	private:
		TSharedPtr<SBoneGroupsTreeWidget> TreeWidget;
		TWeakObjectPtr<USkeleton> Skeleton;
		TWeakObjectPtr<UObject> UndoObject;
		TSharedPtr<FUICommandList> CommandList;
		FBoneGroupsListWidgetGetBoneGroups GetBoneGroups;
		FString FilterText;
		FNotifyHook* NotifyHook = nullptr;
	};
} // namespace UE::MLDeformer::TrainingDataProcessor

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STreeView.h"
#include "Framework/Commands/Commands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "UObject/NameTypes.h"
#include "Styling/SlateColor.h"
#include "EditorUndoClient.h"

class USkeleton;
class FUICommandList;
class FNotifyHook;
struct FReferenceSkeleton;

namespace UE::MLDeformer::TrainingDataProcessor
{
	class SBoneTreeWidget;
	class SBoneListWidget;

	class MLDEFORMERFRAMEWORKEDITOR_API FBoneListWidgetCommands final : public TCommands<FBoneListWidgetCommands>
	{
	public:
		FBoneListWidgetCommands();
		virtual void RegisterCommands() override;

	public:
		TSharedPtr<FUICommandInfo> AddBones;
		TSharedPtr<FUICommandInfo> RemoveBones;
		TSharedPtr<FUICommandInfo> ClearBones;
	};


	class MLDEFORMERFRAMEWORKEDITOR_API FBoneTreeWidgetElement final : public TSharedFromThis<FBoneTreeWidgetElement>
	{
	public:
		TSharedRef<ITableRow> MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, TSharedRef<FBoneTreeWidgetElement> InTreeElement,
		                                        TSharedPtr<SBoneTreeWidget> InTreeWidget);

	public:
		FName Name;
		TArray<TSharedPtr<FBoneTreeWidgetElement>> Children;
		FSlateColor TextColor;
	};


	class MLDEFORMERFRAMEWORKEDITOR_API SBoneTreeRowWidget final : public STableRow<TSharedPtr<FBoneTreeWidgetElement>>
	{
	public:
		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, const TSharedRef<FBoneTreeWidgetElement>& InTreeElement,
		               const TSharedPtr<SBoneTreeWidget>& InTreeView);

	private:
		TWeakPtr<FBoneTreeWidgetElement> TreeElement;
		FText GetName() const;
	};


	class MLDEFORMERFRAMEWORKEDITOR_API SBoneTreeWidget final : public STreeView<TSharedPtr<FBoneTreeWidgetElement>>
	{
		SLATE_BEGIN_ARGS(SBoneTreeWidget) { }
			SLATE_ARGUMENT(TSharedPtr<SBoneListWidget>, BoneListWidget)
		SLATE_END_ARGS()

	public:
		void Construct(const FArguments& InArgs);
		void RefreshElements(const TArray<FName>& BoneNames, const FReferenceSkeleton* RefSkeleton, const FString& FilterText);
		TArray<FName> ExtractAllElementNames() const;
		const TArray<TSharedPtr<FBoneTreeWidgetElement>>& GetRootElements() const { return RootElements; }

	private:
		virtual FReply OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
		
		static void RecursiveAddNames(const FBoneTreeWidgetElement& Element, TArray<FName>& OutNames);
		TSharedRef<ITableRow> MakeTableRowWidget(TSharedPtr<FBoneTreeWidgetElement> InItem, const TSharedRef<STableViewBase>& OwnerTable);
		TSharedPtr<SWidget> OnContextMenuOpening() const;
		static TSharedPtr<FBoneTreeWidgetElement> FindParentElementForBone(FName BoneName, const FReferenceSkeleton& RefSkeleton, const TMap<FName, TSharedPtr<FBoneTreeWidgetElement>>& NameToElementMap);
		void RecursiveSortElements(const TSharedPtr<FBoneTreeWidgetElement>& Element);
		static void HandleGetChildrenForTree(TSharedPtr<FBoneTreeWidgetElement> InItem, TArray<TSharedPtr<FBoneTreeWidgetElement>>& OutChildren);

	private:
		TArray<TSharedPtr<FBoneTreeWidgetElement>> RootElements;
		TWeakPtr<SBoneListWidget> BoneListWidget;
	};


	DECLARE_DELEGATE_OneParam(FOnBoneListWidgetBonesAdded, const TArray<FName>& BoneNames)
	DECLARE_DELEGATE_OneParam(FOnBoneListWidgetBonesRemoved, const TArray<FName>& BoneNames)
	DECLARE_DELEGATE(FOnBoneListWidgetBonesCleared)
	DECLARE_DELEGATE_RetVal(TArray<FName>*, FBoneListWidgetGetBoneNames)

	/**
	 * The bone list widget, which displays a list of bones (in a hierarchy) and allows you to add and remove bones.
	 * It works directly on a TArray<FName> as source. You can use the GetBoneNames event to provide this array.
	 * You can use some of the delegates to listen for changes to the array.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API SBoneListWidget final : public SCompoundWidget, public FEditorUndoClient
	{
		SLATE_BEGIN_ARGS(SBoneListWidget) {}
			SLATE_ARGUMENT(TWeakObjectPtr<USkeleton>, Skeleton)
			SLATE_ARGUMENT(TWeakObjectPtr<UObject>, UndoObject)
			SLATE_EVENT(FOnBoneListWidgetBonesAdded, OnBonesAdded)
			SLATE_EVENT(FOnBoneListWidgetBonesRemoved, OnBonesRemoved)
			SLATE_EVENT(FOnBoneListWidgetBonesCleared, OnBonesCleared)
			SLATE_EVENT(FBoneListWidgetGetBoneNames, GetBoneNames)
		SLATE_END_ARGS()

	public:
		virtual ~SBoneListWidget() override;

		void Construct(const FArguments& InArgs, FNotifyHook* InNotifyHook);
		void Refresh() const;
		TSharedPtr<SBoneTreeWidget> GetTreeWidget() const;
		TSharedPtr<FUICommandList> GetCommandList() const;

	private:
		// FEditorUndoClient overrides.
		virtual void PostUndo(bool bSuccess) override;
		virtual void PostRedo(bool bSuccess) override;
		// ~END FEditorUndoClient overrides.

		void BindCommands(const TSharedPtr<FUICommandList>& InCommandList);
		void OnFilterTextChanged(const FText& InFilterText);
		void RefreshTree() const;
		FReply OnAddBonesButtonClicked() const;
		FReply OnClearBonesButtonClicked() const;
		void NotifyPropertyChanged() const;

		void OnAddBones() const;
		void OnRemoveBones() const;
		void OnClearBones() const;

	private:
		TSharedPtr<SBoneTreeWidget> TreeWidget;
		TWeakObjectPtr<USkeleton> Skeleton;
		TWeakObjectPtr<UObject> UndoObject;
		TSharedPtr<FUICommandList> CommandList;
		FNotifyHook* NotifyHook = nullptr;
		FString FilterText;
		FOnBoneListWidgetBonesAdded OnBonesAdded;
		FOnBoneListWidgetBonesRemoved OnBonesRemoved;
		FOnBoneListWidgetBonesCleared OnBonesCleared;
		FBoneListWidgetGetBoneNames GetBoneNames;
	};
} // namespace UE::MLDeformer::TrainingDataProcessor

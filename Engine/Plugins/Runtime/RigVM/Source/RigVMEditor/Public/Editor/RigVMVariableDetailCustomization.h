// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "IDetailCustomization.h"
#include "Styling/SlateTypes.h"
#include "UObject/WeakFieldPtr.h"
#include "Layout/Visibility.h"
#include "Widgets/Views/ITableRow.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/SListView.h"

class URigVMBlueprint;
class SRigVMEditorGraphExplorer;
class IDetailLayoutBuilder;
class IBlueprintEditor;
class UBlueprint;
class IRigVMEditor;
class FRigVMVariableDetailCustomization;
class UK2Node_EditablePinBase;
class FStructOnScope;
class SEditableTextBox;
class SComboButton;

class RIGVMEDITOR_API FRigVMVariableDetailCustomization : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance(TSharedPtr<IRigVMEditor> InEditor);
	FRigVMVariableDetailCustomization(TSharedPtr<IRigVMEditor> InEditor, URigVMBlueprint* Blueprint);

#if WITH_RIGVMLEGACYEDITOR
	static TSharedPtr<IDetailCustomization> MakeLegacyInstance(TSharedPtr<IBlueprintEditor> InBlueprintEditor);
	FRigVMVariableDetailCustomization(TSharedPtr<IBlueprintEditor> RigVMigEditor, UBlueprint* Blueprint);
#endif

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

private:
	void PopulateCategories();

private:
	/** Accessors passed to parent */
	FName GetVariableName() const;
	FText OnGetVariableName() const;
	void OnVarNameCommitted(const FText& InNewName, ETextCommit::Type InTextCommit);

	// Callbacks for uproperty details customization
	FEdGraphPinType OnGetVarType() const;
	void OnVarTypeChanged(const FEdGraphPinType& NewPinType);

	void OnBrowseToVarType() const;
	bool CanBrowseToVarType() const;

	
	FText OnGetTooltipText() const;
	void OnTooltipTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit, FName VarName);
	EVisibility IsToolTipVisible() const;
	
	FText OnGetCategoryText() const;
	void OnCategoryTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit, FName VarName);
	TSharedRef< ITableRow > MakeCategoryViewWidget( TSharedPtr<FText> Item, const TSharedRef< STableViewBase >& OwnerTable );
	void OnCategorySelectionChanged( TSharedPtr<FText> ProposedSelection, ESelectInfo::Type /*SelectInfo*/ );
	

	ECheckBoxState OnGetExposedToSpawnCheckboxState() const;
	void OnExposedToSpawnChanged(ECheckBoxState InNewState);

	ECheckBoxState OnGetPrivateCheckboxState() const;
	void OnPrivateChanged(ECheckBoxState InNewState);
	
	ECheckBoxState OnGetExposedToCinematicsCheckboxState() const;
	void OnExposedToCinematicsChanged(ECheckBoxState InNewState);


	FText OnGetMetaKeyValue(FName Key) const;
	void OnMetaKeyValueChanged(const FText& NewMinValue, ETextCommit::Type CommitInfo, FName Key);
	EVisibility RangeVisibility() const;


	
	/** Refreshes cached data that changes after a Blueprint recompile */
	void OnPostEditorRefresh();

private:
	/** The Blueprint editor we are embedded in */
	TWeakPtr<IRigVMEditor> EditorPtr;

	/** The blueprint we are editing */
	TWeakObjectPtr<URigVMBlueprint> BlueprintPtr;

	/** The widget used when in variable name editing mode */ 
	TSharedPtr<SEditableTextBox> VarNameEditableTextBox;
	
	/** A list of all category names to choose from */
	TArray<TSharedPtr<FText>> CategorySource;
	/** Widgets for the categories */
	TWeakPtr<SComboButton> CategoryComboButton;
	TWeakPtr<SListView<TSharedPtr<FText>>> CategoryListView;

	/** Cached property for the variable we are affecting */
	TWeakFieldPtr<FProperty> CachedVariableProperty;

	/** Cached name for the variable we are affecting */
	mutable FName CachedVariableName;
};

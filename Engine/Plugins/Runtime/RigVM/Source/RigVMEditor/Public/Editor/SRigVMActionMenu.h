// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Editor/RigVMActionMenuBuilder.h"
#include "SGraphActionMenu.h"


class SGraphActionMenu;
class IRigVMEditor;


/*******************************************************************************
* SRigVMActionMenu
*******************************************************************************/

class SRigVMActionMenu : public SBorder
{
public:

	SLATE_BEGIN_ARGS( SRigVMActionMenu )
		: _GraphObj( nullptr )
		, _NewNodePosition( FVector2f::ZeroVector )
	{}

		SLATE_ARGUMENT( UEdGraph*, GraphObj )
		SLATE_ARGUMENT( FDeprecateSlateVector2D, NewNodePosition )
		SLATE_ARGUMENT( TArray<UEdGraphPin*>, DraggedFromPins )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, TSharedPtr<IRigVMEditor> InEditor );

	~SRigVMActionMenu();

	TSharedRef<SEditableTextBox> GetFilterTextBox();

	// SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	// End of SWidget interface

protected:
	/** UI Callback functions */
	FText GetSearchContextDesc() const;
	void OnContextToggleChanged(ECheckBoxState CheckState);
	ECheckBoxState ContextToggleIsChecked() const;

	void OnActionSelected( const TArray< TSharedPtr<FEdGraphSchemaAction> >& SelectedAction, ESelectInfo::Type InSelectionType );

	TSharedRef<SWidget> OnCreateWidgetForAction(struct FCreateWidgetForActionData* const InCreateData);

	/** Callback used to populate all actions list in SGraphActionMenu */
	TSharedRef<FGraphActionListBuilderBase> OnGetActionList();

	/**  */
	void ConstructActionContext(FBlueprintActionContext& ContextDescOut);

	/** Function to try to insert a promote to variable entry if it is possible to do so. */
	void TryInsertPromoteToVariable(FBlueprintActionContext const& Context, FGraphActionListBuilderBase& OutAllActions);


private:
	TObjectPtr<UEdGraph> GraphObj = nullptr;
	TArray<UEdGraphPin*> DraggedFromPins;
	FDeprecateSlateVector2D NewNodePosition;

	TSharedPtr<SGraphActionMenu> GraphActionMenu;
	TWeakPtr<IRigVMEditor> EditorPtr;
	TSharedPtr<FRigVMActionMenuBuilder> ContextMenuBuilder;

	bool bActionExecuted = false;
};

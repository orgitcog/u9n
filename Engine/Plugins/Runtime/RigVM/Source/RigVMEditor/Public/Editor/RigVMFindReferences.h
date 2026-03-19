// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/Views/STreeView.h"
#include "EdGraph/EdGraphPin.h"
#include "Textures/SlateIcon.h"
#include "EdGraph/EdGraphSchema.h"

class SWidget;
class SDockTab;
class UBlueprint;
class FUICommandList;
class URigVMBlueprint;


class RIGVMEDITOR_API FRigVMFindResult : public TSharedFromThis< FRigVMFindResult >
{
public:
	FRigVMFindResult() = default;
	virtual ~FRigVMFindResult() = default;

	/* Create a root */
	explicit FRigVMFindResult(TWeakObjectPtr<URigVMBlueprint> InBlueprint);
	explicit FRigVMFindResult(TWeakObjectPtr<URigVMBlueprint> InBlueprint, const FText& InDisplayText);

	/* Called when user clicks on the search item */
	virtual FReply OnClick();

	/* Get Category for this search result */
	virtual FText GetCategory() const;

	/* Create an icon to represent the result */
	virtual TSharedRef<SWidget>	CreateIcon() const;

	/** Finalizes any content for the search data that was unsafe to do on a separate thread */
	virtual void FinalizeSearchData() {};

	/** gets the blueprint housing all these search results */
	URigVMBlueprint* GetBlueprint() const;

	/**
	* Parses search info for specific data important for displaying the search result in an easy to understand format
	*
	* @param	InKey			This is the tag for the data, describing what it is so special handling can occur if needed
	* @param	InValue			Compared against search query to see if it passes the filter, sometimes data is rejected because it is deemed unsearchable
	*/
	virtual void ParseSearchInfo(FText InKey, FText InValue) {};

	/** Returns the Object represented by this search information give the Blueprint it can be found in */
	virtual UObject* GetObject(URigVMBlueprint* InBlueprint) const;

	/** Returns the display string for the row */
	FText GetDisplayString() const;

public:
	/*Any children listed under this category */
	TArray< TSharedPtr<FRigVMFindResult> > Children;

	/*If it exists it is the blueprint*/
	TWeakPtr<FRigVMFindResult> Parent;

	/*If it exists it is the blueprint*/
	TWeakObjectPtr<URigVMBlueprint> WeakBlueprint;

	/*The display text for this item */
	FText DisplayText;
};

typedef TSharedPtr<FRigVMFindResult> FRigVMSearchResult;
typedef STreeView<FRigVMSearchResult>  SRigVMTreeViewType;

/** Some utility functions to help with Find-in-Blueprint functionality */
namespace RigVMFindReferencesHelpers
{
	/**
	 * Retrieves the pin type as a string value
	 *
	 * @param InPinType		The pin type to look at
	 *
	 * @return				The pin type as a string in format [category]'[sub-category object]'
	 */
	FString GetPinTypeAsString(const FEdGraphPinType& InPinType);

	/**
	 * Parses a pin type from passed in key names and values
	 *
	 * @param InKey					The key name for what the data should be translated as
	 * @param InValue				Value to be be translated
	 * @param InOutPinType			Modifies the PinType based on the passed parameters, building it up over multiple calls
	 * @return						TRUE when the parsing is successful
	 */
	bool ParsePinType(FText InKey, FText InValue, FEdGraphPinType& InOutPinType);

	/**
	* Iterates through all the given tree node's children and tells the tree view to expand them
	*/
	void ExpandAllChildren(FRigVMSearchResult InTreeNode, TSharedPtr<STreeView<FRigVMSearchResult>> InTreeView);
}

/** Graph nodes use this class to store their data */
class FRigVMFindReferencesGraphNode : public FRigVMFindResult
{
public:
	FRigVMFindReferencesGraphNode(TWeakObjectPtr<URigVMBlueprint> InBlueprint);
	virtual ~FRigVMFindReferencesGraphNode() {}

	/** FRigVMFindResult Interface */
	virtual FReply OnClick() override;
	virtual TSharedRef<SWidget>	CreateIcon() const override;
	virtual void ParseSearchInfo(FText InKey, FText InValue) override;
	virtual void FinalizeSearchData() override;
	virtual UObject* GetObject(URigVMBlueprint* InBlueprint) const override;
	virtual FText GetCategory() const override;
	/** End FRigVMFindResult Interface */

private:
	/** The Node Guid to find when jumping to the node */
	FGuid NodeGuid;

	/** The glyph brush for this node */
	FSlateIcon Glyph;

	/** The glyph color for this node */
	FLinearColor GlyphColor;

	/*The class this item refers to */
	UClass* Class;

	/*The class name this item refers to */
	FString ClassName;
};

/** Pins use this class to store their data */
class FRigVMFindReferencesPin : public FRigVMFindResult
{
public:
	FRigVMFindReferencesPin(TWeakObjectPtr<URigVMBlueprint> InBlueprint, FString InSchemaName);
	virtual ~FRigVMFindReferencesPin() {}

	/** FRigVMFindResult Interface */
	virtual TSharedRef<SWidget>	CreateIcon() const override;
	virtual void ParseSearchInfo(FText InKey, FText InValue) override;
	virtual FText GetCategory() const override;
	virtual void FinalizeSearchData() override;
	/** End FRigVMFindResult Interface */

private:
	/** The name of the schema this pin exists under */
	FString SchemaName;

	/** The pin that this search result refers to */
	FEdGraphPinType PinType;

	/** Pin's icon color */
	FSlateColor IconColor;
};

/** Property data is stored here */
class FRigVMFindReferencesVariable : public FRigVMFindResult
{
public:
	FRigVMFindReferencesVariable(TWeakObjectPtr<URigVMBlueprint> InBlueprint);
	virtual ~FRigVMFindReferencesVariable() {}

	/** FRigVMFindResult Interface */
	virtual TSharedRef<SWidget>	CreateIcon() const override;
	virtual void ParseSearchInfo(FText InKey, FText InValue) override;
	virtual FText GetCategory() const override;
	virtual void FinalizeSearchData() override;
	/** End FRigVMFindResult Interface */

private:
	/** The pin that this search result refers to */
	FEdGraphPinType PinType;

	/** The default value of a property as a string */
	FString DefaultValue;
};

/** Graphs, such as functions and macros, are stored here */
class FRigVMFindReferencesGraph : public FRigVMFindResult
{
public:
	FRigVMFindReferencesGraph(TWeakObjectPtr<URigVMBlueprint> InBlueprint, EGraphType InGraphType);
	virtual ~FRigVMFindReferencesGraph() {}

	/** FRigVMFindResult Interface */
	virtual FReply OnClick() override;
	virtual TSharedRef<SWidget>	CreateIcon() const override;
	virtual void ParseSearchInfo(FText InKey, FText InValue) override;
	virtual FText GetCategory() const override;
	/** End FRigVMFindResult Interface */

private:
	/** The type of graph this represents */
	EGraphType GraphType;
};

/*Widget for searching for (functions/events) across all blueprints or just a single blueprint */
class RIGVMEDITOR_API SRigVMFindReferences: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SRigVMFindReferences )
	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<class FRigVMEditorBase> InBlueprintEditor = nullptr);

	/** Focuses this widget's search box, and changes the mode as well, and optionally the search terms */
	void FocusForUse(bool bSetFindWithinBlueprint, FString NewSearchTerms = FString(), bool bSelectFirstResult = false);

	/** SWidget overrides */
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	
	/** The main function that will find references and build the tree */
	void FindReferences(const FString& SearchTerms);

	/** Register any Find-in-Blueprint commands */
	void RegisterCommands();

	/*Called when user changes the text they are searching for */
	void OnSearchTextChanged(const FText& Text);

	/*Called when user changes commits text to the search box */
	void OnSearchTextCommitted(const FText& Text, ETextCommit::Type CommitType);

	/* Get the children of a row */
	void OnGetChildren( FRigVMSearchResult InItem, TArray< FRigVMSearchResult >& OutChildren );

	/* Called when user double clicks on a new result */
	void OnTreeSelectionDoubleClicked( FRigVMSearchResult Item );

	/* Called when a new row is being generated */
	TSharedRef<ITableRow> OnGenerateRow(FRigVMSearchResult InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Callback to build the context menu when right clicking in the tree */
	TSharedPtr<SWidget> OnContextMenuOpening();

	/** Helper function to select all items */
	void SelectAllItemsHelper(FRigVMSearchResult InItemToSelect);

	/** Callback when user attempts to select all items in the search results */
	void OnSelectAllAction();

	/** Callback when user attempts to copy their selection in the Find-in-Blueprints */
	void OnCopyAction();

private:
	/** Pointer back to the blueprint editor that owns us */
	TWeakPtr<class FRigVMEditorBase> EditorPtr;
	
	/* The tree view displays the results */
	TSharedPtr<SRigVMTreeViewType> TreeView;

	/** The search text box */
	TSharedPtr<class SSearchBox> SearchTextField;
	
	/* This buffer stores the currently displayed results */
	TArray<FRigVMSearchResult> ItemsFound;

	/* Map relationship between element hash and its result  */ 
	TMap<uint32, FRigVMSearchResult> ElementHashToResult;

	/* The string to highlight in the results */
	FText HighlightText;

	/* The string to search for */
	FString	SearchValue;

	/** Commands handled by this widget */
	TSharedPtr< FUICommandList > CommandList;
};
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Layout/Margin.h"
#include "Modules/ModuleInterface.h"
#include "Styling/SlateColor.h"
#include "Types/SlateEnums.h"
#include "Widgets/Layout/Anchors.h"

#if WITH_EDITOR
class IAssetViewport;
#endif
#if WITH_ENGINE
class UGameViewportClient;
class ULocalPlayer;
class UMaterialInterface;
class UTextureRenderTarget2D;
class UTexture2D;
#endif

struct FButtonStyle;
struct FCheckBoxStyle;
struct FComboBoxStyle;
struct FEditableTextBoxStyle;
struct FKey;
struct FProgressBarStyle;
struct FSlateBrush;
struct FSliderStyle;
struct FSpinBoxStyle;
struct FTableRowStyle;
struct FTableViewStyle;
struct FTextBlockStyle;

enum class ECheckBoxState : uint8;

class SWidget;


class FSlateIMModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

/**
 * This is the full list of available functions for creating SlateIM widgets. See FSlateIMTestWidget::Draw() for examples.
 */
namespace SlateIM
{
#pragma region Roots
#if WITH_EDITOR || WITH_ENGINE
	struct FViewportRootLayout
	{
		/** How to anchor the root within the viewport (normalized units) */
		FAnchors Anchors = FAnchors(0);
		
		/** Offset the root from the anchored position in the viewport (in slate units) */
		FVector2f Offset = FVector2f::ZeroVector;

		/** How the root is aligned to its anchor (normalized units) */
		FVector2f Alignment = FVector2f::ZeroVector;

		/** Optional set the size (in slate units) of the root in the viewport. When unset, the root will auto-size for its content. */
		TOptional<FVector2f> Size;

		/** The ZOrder of the SlateIM widget in the viewport */
		int32 ZOrder = 10000;
	};
#endif // WITH_EDITOR || WITH_ENGINE

	/**
	 * Begins a new floating window root
	 *
	 * @param UniqueName a unique name to identify this window
	 * @param WindowTitle The name of the window. This will also be displayed in the windows title bar
	 * @param WindowSize The size to open the window at (does not update the size of an existing window)
	 * @param bShouldReopen (optional) If this window was created with a previous call to BeginWindow and then closed, passing in true will reopen the window and false will leave it closed. If the window has never been seen this parameter does nothing and the window will open
	 *
	 * @return the current window state. True if open, false if closed or not updating
	 *
	 * @note Consider using FSlateIMWindowBase to handle creating and updating your Slate IM Window
	 */
	SLATEIM_API bool BeginWindowRoot(FName UniqueName, const FStringView& WindowTitle, FVector2f WindowSize, bool bShouldReopen = false);

#if WITH_ENGINE
	// TODO - Game viewport widgets might need something to control HitTestability and cursor visibility
	/**
	 *	Begins a new root in the game viewport
	 * 
	 * @param UniqueName a globally unique name to identify this root
	 * @param ViewportClient the viewport client to add the widget to
	 * @param Layout How to lay out the root within the viewport
	 * 
	 * @return Whether the root is valid and updating
	 */
	SLATEIM_API bool BeginViewportRoot(FName UniqueName, UGameViewportClient* ViewportClient, const FViewportRootLayout& Layout = FViewportRootLayout());
	
	/**
	 *	Begins a new root in the provided player's viewport
	 * 
	 * @param UniqueName a globally unique name to identify this root
	 * @param LocalPlayer the player whose viewport to add the widget to
	 * @param Layout How to lay out the root within the viewport
	 * 
	 * @return Whether the root is valid and updating
	 */
	SLATEIM_API bool BeginViewportRoot(FName UniqueName, ULocalPlayer* LocalPlayer, const FViewportRootLayout& Layout = FViewportRootLayout());
#endif

#if WITH_EDITOR
	/**
	 * Begins a SlateIM root in the provided in editor viewport
	 * 
	 * @param UniqueName Unique identifier for this root. Must be globally unique within the same editor session
	 * @param AssetViewport The editor viewpor to add the SlateIM content to
	 * @param Layout How to lay out the root within the viewport
	 * 
	 * @return Whether the root is valid and updating
	 */
	SLATEIM_API bool BeginViewportRoot(FName UniqueName, TSharedPtr<IAssetViewport> AssetViewport, const FViewportRootLayout& Layout = FViewportRootLayout());
#endif

	/**
	 * Begins a SlateIM root that exposes a slate widget for embedding in other Slate hierarchies
	 * 
	 * @param UniqueName a globally unique name to identify this root
	 * @param OutSlateIMWidget where to output the resulting SlateIM widget to
	 * 
	 * @return Whether the root is valid and updating
	 * 
	 * @note Consider using FSlateIMExposedBase to handle creating and updating your Slate IM exposed widget
	 */
	SLATEIM_API bool BeginExposedRoot(FName UniqueName, TSharedPtr<SWidget>& OutSlateIMWidget);

	/**
	 * Ends any Root type, must always be called regardless of the result of the Begin function
	 */
	SLATEIM_API void EndRoot();
#pragma endregion Roots

#pragma region Containers
	/**
	 * Begins a horizontally stacked container. All widgets created within the Horizontal Stack container will be placed in-order left-to-right
	 */
	SLATEIM_API void BeginHorizontalStack();
	SLATEIM_API void EndHorizontalStack();

	/**
	 * Begins a vertically stacked container. All widgets created within the Vertical Stack container will be placed in-order top-to-bottom
	 */
	SLATEIM_API void BeginVerticalStack();
	SLATEIM_API void EndVerticalStack();

	/**
	 * Begins a horizontally wrapped container. All widgets created within the Horizontal Wrap container will be placed in-order left-to-right until the allotted width is filled,
	 * then the content will begin a new line and so-on.
	 *
	 * @note Wrap box children are always AutoSize and do not support Fill
	 */
	SLATEIM_API void BeginHorizontalWrap();
	SLATEIM_API void EndHorizontalWrap();
	
	/**
	 * Begins a vertically wrapped container. All widgets created within the Vertical Wrap container will be placed in-order top-to-bottom until the allotted height is filled,
	 * then the content will begin a new column and so-on.
	 *
	 * @note Wrap container children are always AutoSize and do not support Fill
	 */
	SLATEIM_API void BeginVerticalWrap();
	SLATEIM_API void EndVerticalWrap();

	/**
	 * Begin a Table container. The contents are automatically scrollable if the table is sized smaller than all its content.
	 * 
	 * @param InStyle (optional) A Table View Style to override the default style. Changes to the style after the first invocation are not reflected,
	 *				  provide a new style object to update the visuals at runtime
	 * @param InRowStyle (optional) A Table Row Style to override the default table row style. Changing the style only affects new table rows.
	 */
	SLATEIM_API void BeginTable(const FTableViewStyle* InStyle = nullptr, const FTableRowStyle* InRowStyle = nullptr);
	/**
	 * End the current Table container.
	 */
	SLATEIM_API void EndTable();

	/**
	 * Add a column to a label. Call this before adding any content to the table.
	 * 
	 * @param ColumnLabel (optional) The text label to display in the header for this column
	 *
	 * @note Table columns support SetToolTip, HAlign, and VAlign. See FixedTableColumnWidth() and InitialTableColumnWidth() for size options.
	 */
	SLATEIM_API void AddTableColumn(const FStringView& ColumnLabel = FStringView());

	/**
	 * Set the column to a fixed width, not resizable by users. Call this before calling AddTableColumn().
	 * 
	 * @param Width The fixed width of the column
	 */
	SLATEIM_API void FixedTableColumnWidth(float Width);

	/**
	 * Set the column to an initial width, which can then be resized by users. Call this before calling AddTableColumn().
	 * 
	 * @param Width The initial width of the column
	 */
	SLATEIM_API void InitialTableColumnWidth(float Width);

	/**
	 * Begin the content for the next table cell. The column and row are tracked internally based on the number of columns.
	 * 
	 * @return Whether the contents of this table cell are visible. When false, the content can be skipped to save cycles.
	 *
	 * @note There is no "End" function counterpart to this call. This function supports the various slot functions (Padding, HAlign, VAlign, Min/Max Width/Height, etc).
	 */
	SLATEIM_API bool NextTableCell();

	/**
	 * Begin adding content as a child to the current table row. NextTableCell() must still be called after this function.
	 * 
	 * @return Whether the parent row is expanded or not. When false, the child content can be skipped to save cycles.
	 */
	SLATEIM_API bool BeginTableRowChildren();
	/**
	 * Stop adding child content to the parent row. This must be called no matter the result of BeginTableRowChildren()
	 */
	SLATEIM_API void EndTableRowChildren();

	/**
	 * Begin a container with a background image.
	 * 
	 * @param BorderBrush The brush to use as the background for this container
	 * @param Orientation Which direction the contents of the container should flow
	 * @param bAbsorbMouse Whether the container should handle all mouse inputs
	 * @param ContentPadding How much to pad the contents of the container
	 */
	SLATEIM_API void BeginBorder(const FSlateBrush* BorderBrush, EOrientation Orientation = Orient_Vertical, bool bAbsorbMouse = true, FMargin ContentPadding = FMargin(2.0f));
	
	/**
	 * Begin a container with a background image.
	 * 
	 * @param BorderStyleName The name of the brush in FAppStyle to use as the background for this container
	 * @param Orientation Which direction the contents of the container should flow
	 * @param bAbsorbMouse Whether the container should handle all mouse inputs
	 * @param ContentPadding How much to pad the contents of the container
	 */
	SLATEIM_API void BeginBorder(const FName BorderStyleName, EOrientation Orientation = Orient_Vertical, bool bAbsorbMouse = true, FMargin ContentPadding = FMargin(2.0f));
	SLATEIM_API void EndBorder();

	/**
	 * Begins a container that will allow the user to scroll when its content is larger than the allotted space.
	 * 
	 * @param ScrollBarOrientation Which direction content should flow and scroll
	 * 
	 * @return Whether the user scroll the content this frame
	 */
	SLATEIM_API bool BeginScrollBox(EOrientation ScrollBarOrientation = EOrientation::Orient_Vertical);
	SLATEIM_API void EndScrollBox();

	SLATEIM_API void BeginPopUp(const FName BorderStyleName = TEXT("ToolPanel.GroupBorder"), EOrientation Orientation = Orient_Vertical, bool bAbsorbMouse = true, FMargin ContentPadding = FMargin(2.0f));
	SLATEIM_API void BeginPopUp(const FSlateBrush* BorderBrush, EOrientation Orientation = Orient_Vertical, bool bAbsorbMouse = true, FMargin ContentPadding = FMargin(2.0f));
	SLATEIM_API void EndPopUp();
#pragma endregion Containers

#pragma region Slots
	/**
	 * Sets the padding for the next widget
	 * 
	 * @param NextPadding The padding to apply to the next widget
	 */
	SLATEIM_API void Padding(const FMargin NextPadding);
	
	/**
	 * Sets the horizontal alignment for the next widget
	 * 
	 * @param NextAlignment The alignment to apply to the next widget
	 */
	SLATEIM_API void HAlign(EHorizontalAlignment NextAlignment);
	/**
	 * Sets the vertical alignment for the next widget
	 * 
	 * @param NextAlignment The alignment to apply to the next widget
	 */
	SLATEIM_API void VAlign(EVerticalAlignment NextAlignment);

	/**
	 * Set the next slot to AutoSize to its content
	 */
	SLATEIM_API void AutoSize();
	/**
	 * Sets the next slot to Fill the remaining space in its container
	 *
	 * @note The filled space is shared equally with any other slots set to Fill the same container
	 */
	SLATEIM_API void Fill();

	/**
	 * Set the minimum width the next slot can have.
	 * 
	 * @param InMinWidth The minimum width for the next slot
	 */
	SLATEIM_API void MinWidth(float InMinWidth);
	/**
	 * Set the minimum height the next slot can have.
	 * 
	 * @param InMinHeight The minimum height for the next slot
	 */
	SLATEIM_API void MinHeight(float InMinHeight);
	/**
	 * Set the maximum width the next slot can have.
	 * 
	 * @param InMaxWidth The maximum width for the next slot
	 */
	SLATEIM_API void MaxWidth(float InMaxWidth);
	/**
	 * Set the maximum height the next slot can have.
	 * 
	 * @param InMaxHeight The maximum height for the next slot
	 */
	SLATEIM_API void MaxHeight(float InMaxHeight);
#pragma endregion Slots

#pragma region Widgets
	/**
	 * Display a string of text
	 * 
	 * @param InText The text string to display
	 * @param TextStyle (optional) A Text Block Style to override the default style. Changes to the style after the first invocation are not reflected,
	 *					provide a new style object to update the visuals at runtime
	 */
	SLATEIM_API void Text(const FStringView& InText, const FTextBlockStyle* TextStyle = nullptr);
	/**
	 * Display a colored string of text
	 * 
	 * @param InText The text string to display
	 * @param Color The color to display the text as
	 * @param TextStyle (optional) A Text Block Style to override the default style. Changes to the style after the first invocation are not reflected,
	 *					provide a new style object to update the visuals at runtime
	 */
	SLATEIM_API void Text(const FStringView& InText, FSlateColor Color, const FTextBlockStyle* TextStyle = nullptr);

	/**
	 * Create a text input field
	 * 
	 * @param InOutText The text that is in the input field. Can have a default value
	 * @param HintText The hint text to display when the input field is empty
	 * @param TextStyle (optional) An Editable Text Box Style to override the default style. Changes to the style after the first invocation are not reflected,
	 *					provide a new style object to update the visuals at runtime
	 *					
	 * @return Whether the user changed or committed text this frame
	 */
	SLATEIM_API bool EditableText(FString& InOutText, const FStringView& HintText = FStringView(), const FEditableTextBoxStyle* TextStyle = nullptr);

	/**
	 * Display a brush
	 * 
	 * @param ImageBrush The brush to display
	 * @param ColorAndOpacity (optional) The color to tint the brush
	 * @param DesiredSize (optional) Override the desired size to display the brush at
	 */
	SLATEIM_API void Image(const FSlateBrush* ImageBrush, const FSlateColor& ColorAndOpacity = FLinearColor::White, FVector2D DesiredSize = FVector2D::ZeroVector);
	/**
	 * Display a slate style brush
	 * 
	 * @param ImageStyleName The named of the brush to display from FAppStyle
	 * @param ColorAndOpacity (optional) The color to tint the brush
	 * @param DesiredSize (optional) Override the desired size to display the brush at
	 */
	SLATEIM_API void Image(const FName ImageStyleName, const FSlateColor& ColorAndOpacity = FLinearColor::White, FVector2D DesiredSize = FVector2D::ZeroVector);
#if WITH_ENGINE
	/**
	 * Display a texture
	 * 
	 * @param ImageTexture The texture to display
	 * @param ColorAndOpacity (optional) The color to tint the brush
	 * @param DesiredSize (optional) Override the desired size to display the brush at
	 */
	SLATEIM_API void Image(UTexture2D* ImageTexture, const FSlateColor& ColorAndOpacity = FLinearColor::White, FVector2D DesiredSize = FVector2D::ZeroVector);
	/**
	 * Display a render target texture
	 * 
	 * @param ImageRenderTarget The render target texture to display
	 * @param ColorAndOpacity (optional) The color to tint the brush
	 * @param DesiredSize (optional) Override the desired size to display the brush at
	 */
	SLATEIM_API void Image(UTextureRenderTarget2D* ImageRenderTarget, const FSlateColor& ColorAndOpacity = FLinearColor::White, FVector2D DesiredSize = FVector2D::ZeroVector);
	/**
	 * Display a material
	 * 
	 * @param ImageMaterial The material to display
	 * @param BrushSize The size to create the internal brush with
	 * @param ColorAndOpacity (optional) The color to tint the brush
	 * @param DesiredSize (optional) Override the desired size to display the brush at
	 */
	SLATEIM_API void Image(UMaterialInterface* ImageMaterial, FVector2D BrushSize, const FSlateColor& ColorAndOpacity = FLinearColor::White, FVector2D DesiredSize = FVector2D::ZeroVector);
#endif

	/**
	 * Display a button with text
	 * 
	 * @param InText The text to display on the button
	 * @param InStyle (optional) A Button Style to override the default style. Changes to the style after the first invocation are not reflected,
	 *				  provide a new style object to update the visuals at runtime
	 *					
	 * @return Whether the user clicked the button this frame
	 */
	SLATEIM_API bool Button(const FStringView& InText, const FButtonStyle* InStyle = nullptr);

	/**
	 * Display a two-state checkbox
	 * 
	 * @param InText The label for the checkbox
	 * @param InOutCurrentState The current state of the checkbox. Can provide a default value.
	 * @param CheckBoxStyle (optional) A Checkbox Style to override the default style. Changes to the style after the first invocation are not reflected,
	 *						provide a new style object to update the visuals at runtime
	 *					
	 * @return Whether the user changed the state of the checkbox this frame
	 */
	SLATEIM_API bool CheckBox(const FStringView& InText, bool& InOutCurrentState, const FCheckBoxStyle* CheckBoxStyle = nullptr);
	
	/**
	 * Display a three-state checkbox
	 * 
	 * @param InText The label for the checkbox
	 * @param InOutCurrentState The current state of the checkbox. Can provide a default value.
	 * @param CheckBoxStyle (optional) A Checkbox Style to override the default style. Changes to the style after the first invocation are not reflected,
	 *						provide a new style object to update the visuals at runtime
	 *					
	 * @return Whether the user changed the state of the checkbox this frame
	 */
	SLATEIM_API bool CheckBox(const FStringView& InText, ECheckBoxState& InOutCurrentState, const FCheckBoxStyle* CheckBoxStyle = nullptr);

	/**
	 * Display a float-based spin box
	 * 
	 * @param InOutValue The current value of the spin box. Can provide a default value.
	 * @param Min The minimum value of the spin box (or unset for no limit).
	 * @param Max The maximum value of the spin box (or unset for no limit).
	 * @param SpinBoxStyle (optional) A Spin Box Style to override the default style. Changes to the style after the first invocation are not reflected,
	 *						provide a new style object to update the visuals at runtime
	 * 
	 * @return Whether the user changed or committed the spin box value this frame
	 */
	SLATEIM_API bool SpinBox(float& InOutValue, TOptional<float> Min, TOptional<float> Max, const FSpinBoxStyle* SpinBoxStyle = nullptr);
	/**
	 * Display a double-based spin box
	 * 
	 * @param InOutValue The current value of the spin box. Can provide a default value.
	 * @param Min The minimum value of the spin box (or unset for no limit).
	 * @param Max The maximum value of the spin box (or unset for no limit).
	 * @param SpinBoxStyle (optional) A Spin Box Style to override the default style. Changes to the style after the first invocation are not reflected,
	 *						provide a new style object to update the visuals at runtime
	 * 
	 * @return Whether the user changed or committed the spin box value this frame
	 */
	SLATEIM_API bool SpinBox(double& InOutValue, TOptional<double> Min, TOptional<double> Max, const FSpinBoxStyle* SpinBoxStyle = nullptr);
	/**
	 * Display an integer-based spin box
	 * 
	 * @param InOutValue The current value of the spin box. Can provide a default value.
	 * @param Min The minimum value of the spin box (or unset for no limit).
	 * @param Max The maximum value of the spin box (or unset for no limit).
	 * @param SpinBoxStyle (optional) A Spin Box Style to override the default style. Changes to the style after the first invocation are not reflected,
	 *						provide a new style object to update the visuals at runtime
	 * 
	 * @return Whether the user committed the spin box value this frame
	 */
	SLATEIM_API bool SpinBox(int32& InOutValue, TOptional<int32> Min, TOptional<int32> Max, const FSpinBoxStyle* SpinBoxStyle = nullptr);

	/**
	 * Display a float-based slider
	 * 
	 * @param InOutValue The current value of the slider. Can provide a default value.
	 * @param Min The minimum value of the slider
	 * @param Max The maximum value of the slider
	 * @param Step The smallest incremental change that can be made to the slider's value
	 * @param SliderStyle (optional) A Slider Style to override the default style. Changes to the style after the first invocation are not reflected,
	 *					  provide a new style object to update the visuals at runtime
	 *					  
	 * @return Whether the user changed the value of the slider this frame
	 */
	SLATEIM_API bool Slider(float& InOutValue, float Min, float Max, float Step, const FSliderStyle* SliderStyle = nullptr);

	/**
	 * Display a progress bar
	 * 
	 * @param Percent The value of the progress bar (0.0 to 1.0)
	 * @param ProgressBarStyle (optional) A Progress Bar Style to override the default style. Changes to the style after the first invocation are not reflected,
	 *						   provide a new style object to update the visuals at runtime
	 */
	SLATEIM_API void ProgressBar(TOptional<float> Percent, const FProgressBarStyle* ProgressBarStyle = nullptr);

	/**
	 * Display a dropdown of text options
	 * 
	 * @param ComboItems The options the user can choose from
	 * @param InOutSelectedItemIndex The index of the currently selected option
	 * @param bForceRefresh (optional) Whether to force a refresh of the available options or the selected option (set to true for a frame when changing the list of options or manually setting the selected index)
	 * @param InComboStyle (optional) A Combo Box Style to override the default style. Changes to the style after the first invocation are not reflected,
	 *					   provide a new style object to update the visuals at runtime
	 *					   
	 * @return Whether the user changed the selected option this frame
	 */
	SLATEIM_API bool ComboBox(const TArray<FString>& ComboItems, int32& InOutSelectedItemIndex, bool bForceRefresh = false, const FComboBoxStyle* InComboStyle = nullptr);

	/**
	 * Display a list of text options
	 * 
	 * @param ListItems The list of options the user can choose from
	 * @param InOutSelectedItemIndex The index of the currently selected option
	 * @param bForceRefresh (optional) Whether to force a refresh of the available options or the selected option (set to true for a frame when changing the list of options or manually setting the selected index)
	 * @param InStyle (optional) A Table View Style to override the default style. Changes to the style after the first invocation are not reflected,
	 *				  provide a new style object to update the visuals at runtime
	 *					   
	 * @return Whether the user changed the selected option this frame
	 */
	SLATEIM_API bool SelectionList(const TArray<FString>& ListItems, int32& InOutSelectedItemIndex, bool bForceRefresh = false, const FTableViewStyle* InStyle = nullptr);

	/**
	 * Create a block of empty space
	 * 
	 * @param Size The size of whitespace to create
	 */
	SLATEIM_API void Spacer(const FVector2D& Size);

	/** 
	 * Add an already created slate widget to the IM hierarchy
	 * 
	 * @note Do not create the widget you are passing in here every frame. This is meant to be used to add already created widgets
	 */
	SLATEIM_API void Widget(TSharedRef<SWidget> InWidget);
#pragma endregion Widgets

#pragma region Menu
	/**
	 * Begin an area where a menu appears with a right click
	 *
	 * @return Whether the menu is open
	 */
	SLATEIM_API bool BeginContextMenuAnchor();
	SLATEIM_API void EndContextMenuAnchor();

	/**
	 * Add a new section to the current context menu
	 * 
	 * @param SectionText The label of the section to add
	 */
	SLATEIM_API void AddMenuSection(const FStringView& SectionText);
	
	/**
	 * Add a separator to the current context menu
	 */
	SLATEIM_API void AddMenuSeparator();

	/**
	 * Add a menu item button to the current context menu
	 * 
	 * @param RowText Label for the menu item
	 * @param ToolTipText (optional) The tooltip to display for the menu item
	 * 
	 * @return Whether the user clicked the menu item this frame
	 */
	SLATEIM_API bool AddMenuButton(const FStringView& RowText, const FStringView& ToolTipText = FStringView());
	/**
	 * Display a menu item button with a checkbox
	 * 
	 * @param RowText Label for the menu item
	 * @param InOutCurrentState The current state of the toggle
	 * @param ToolTipText (optional) The tooltip to display for the menu item
	 * 
	 * @return Whether the user toggled the checkbox this frame
	 */
	SLATEIM_API bool AddMenuToggleButton(const FStringView& RowText, bool& InOutCurrentState, const FStringView& ToolTipText = FStringView());
	/**
	 * Display a menu item button with a checkmark
	 * 
	 * @param RowText Label for the menu item
	 * @param InOutCurrentState The current state of the checkmark
	 * @param ToolTipText (optional) The tooltip to display for the menu item
	 * 
	 * @return Whether the user clicked the menu item this frame
	 */
	SLATEIM_API bool AddMenuCheckButton(const FStringView& RowText, bool& InOutCurrentState, const FStringView& ToolTipText = FStringView());

	/**
	 * Add a submenu item to the current menu
	 * 
	 * @param SubMenuText The label for the menu item
	 */
	SLATEIM_API void BeginSubMenu(const FStringView& SubMenuText);
	SLATEIM_API void EndSubMenu();
#pragma endregion Menu

#pragma region Util
	/**
	 * SlateIM updates are disabled in specific scenarios (like when a SlateIM::ModalDialog is open), use this function to react accordingly
	 * 
	 * @return Whether SlateIM can update currently
	 */
	SLATEIM_API bool CanUpdateSlateIM();
	
	/**
	 * Disables all widgets until EndDisabledState is called.
	 * 
	 * @note It is not possible to enable a widget inside a disabled parent widget by calling EndDisabledState before a child is created inside a disabled Widget.
	 */
	SLATEIM_API void BeginDisabledState();
	SLATEIM_API void EndDisabledState();

	/**
	 * Sets the tooltip to be used for the next widget created. Resets after tooltip is used
	 *
	 * @param NextToolTip The tooltip to display
	 */
	SLATEIM_API void SetToolTip(const FStringView& NextToolTip);

	/**
	 * Opens a modal dialog of the specified type. This function will not return until the user closes the modal dialog
	 * 
	 * @param MessageType The type of options the user can respond to the dialog with
	 * @param DialogText The message to display to the user
	 * @param Category The type of dialog to display
	 * @param DialogTitle The title to display in the dialog window
	 * 
	 * @return The option the user selected
	 *
	 * @note SlateIM updates are disable while a SlateIM modal is open
	 * @see CanUpdateSlateIM()
	 */
	SLATEIM_API EAppReturnType::Type ModalDialog(EAppMsgType::Type MessageType, const FStringView& DialogText, EAppMsgCategory Category = EAppMsgCategory::Warning, const FStringView& DialogTitle = FStringView());
#pragma endregion Util

#pragma region Queries
	/**
	 * Query whether the last widget is hovered or not
	 * 
	 * @return true if the cursor is hovering the last rendered widget, false otherwise
	 */
	SLATEIM_API bool IsHovered();

	enum class EFocusDepth : uint8
	{
		// Only check if the previous widget is focused
		SelfOnly,

		// Check if the previous widget or any of its childs widgets are focused (recursively)
		IncludingDescendants
	};

	/**
	 * Query whether the previous widget is focused
	 * 
	 * @param Depth (optional) How far to check for focus. Includes child widgets by default.
	 * @return true if the previous widget has focus (for the specified depth), false otherwise
	 */
	SLATEIM_API bool IsFocused(EFocusDepth Depth = EFocusDepth::IncludingDescendants);
#pragma endregion Queries

#pragma region Graphing
	/**
	 * Begin a graph widget, call graphing functions between this and EndGraph to include multiple graphs in a single chart
	 */
	SLATEIM_API void BeginGraph();

	/**
	 * Call when finished calling graphing functions for the widget
	 */
	SLATEIM_API void EndGraph();

	/**
	 * Add a line graph of 2D vectors to the graph widget
	 * 
	 * @param Points The X,Y points to plot on the graph
	 * @param LineColor The color of the line to draw for this graph
	 * @param LineThickness How thick to draw the line
	 * @param XViewRange The min and max X values to horizontally scale the graph to
	 * @param YViewRange The min and max Y values to vertically scale the graph to
	 */
	SLATEIM_API void GraphLine(const TArrayView<FVector2D>& Points, const FLinearColor& LineColor, float LineThickness, const FDoubleRange& XViewRange, const FDoubleRange& YViewRange);

	/**
	 * Add a line graph of values to the graph widget.
	 * The value index is used as the X-value when plotting, the graph will scale horizontally to fit all values in the array.
	 * 
	 * @param Values The Y values to plot on the graph
	 * @param LineColor The color of the line to draw for this graph
	 * @param LineThickness How thick to draw the line
	 * @param ViewRange The min and max Y values to vertically scale the graph to
	 */
	SLATEIM_API void GraphLine(const TArrayView<double>& Values, const FLinearColor& LineColor, float LineThickness, const FDoubleRange& ViewRange);
#pragma endregion Graphing

#pragma region Inputs
	/**
	 * Query whether a key was pressed this frame.
	 *
	 * Only considers input events that make it to the SlateIM root. Input events handled by child widgets will not be considered.
	 * 
	 * @param InKey The key to query the state of
	 * @return true if the key was just pressed this frame
	 */
	SLATEIM_API bool IsKeyPressed(const FKey& InKey);
	
	/**
	 * Query whether a key is being held this frame.
	 *
	 * Only considers input events that make it to the SlateIM root. Input events handled by child widgets will not be considered.
	 * 
	 * @param InKey The key to query the state of
	 * @return true if the key was pressed or held this frame
	 */
	SLATEIM_API bool IsKeyHeld(const FKey& InKey);
	
	/**
	 * Query whether a key was released this frame.
	 *
	 * Only considers input events that make it to the SlateIM root. Input events handled by child widgets will not be considered.
	 * 
	 * @param InKey The key to query the state of
	 * @return true if the key was just released this frame
	 */
	SLATEIM_API bool IsKeyReleased(const FKey& InKey);
	
	/**
	 * Retrieve the analog value for a key
	 *
	 * Only considers input events that make it to the SlateIM root. Input events handled by child widgets will not be considered.
	 * 
	 * @param InKey The key to query the state of
	 * @return the last analog value the SlateIM root received for the specified key
	 */
	SLATEIM_API float GetKeyAnalogValue(const FKey& InKey);
#pragma endregion Inputs
}


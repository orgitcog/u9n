// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GraphEditorDragDropAction.h"
#include "IDetailDragDropHandler.h"
#include "StructUtils/PropertyBag.h"

class FPropertyBagDetailsDragDropOp;
class IPropertyBagEdGraphDragAndDrop;

DECLARE_DELEGATE_RetVal_TwoParams(TOptional<EItemDropZone>, FCanAcceptPropertyBagDetailsRowDropOp, TSharedPtr<FPropertyBagDetailsDragDropOp> /*DropOperation*/, EItemDropZone /*DropZone*/);
DECLARE_DELEGATE_RetVal_TwoParams(FReply, FOnPropertyBagDetailsRowDropOp, const FPropertyBagPropertyDesc& /*PropertyDescription*/, EItemDropZone /*DropZone*/);

/** State of the drop, useful for source-is-target validation on details rows. */
enum class EPropertyBagDropState
{
	Invalid,
	Valid,
	SourceIsTarget
};

/**
 * Provides information about the source row (single property) being dragged.
 * Inherits from FGraphEditorDragDropAction to support dragging to a graph pin/node/panel.
 */
class STRUCTUTILSEDITOR_API FPropertyBagDetailsDragDropOp : public FGraphEditorDragDropAction
{
public:
	DRAG_DROP_OPERATOR_TYPE(FPropertyBagDetailsDragDropOp, FGraphEditorDragDropAction);

	struct FDecoration
	{
		explicit FDecoration(const FText& InMessage, const FSlateBrush* InIcon = nullptr, const FLinearColor& InColor = FLinearColor::White);

		const FText Message;
		const FSlateBrush* Icon;
		const FLinearColor IconColor;
	};

	/**
	 * Property Bag Details - Drop Operation Constructor
	 * @param InPropertyDesc Property description for the dropped property.
	 */
	FPropertyBagDetailsDragDropOp(const FPropertyBagPropertyDesc& InPropertyDesc);

	/**
	 * Sets the pop up widget's icon and text.
	 * @param NewDropState The state of the drop operation.
	 * @param OverriddenDecoration An optional overridden decorator for the UI, including Message, Icon, and IconColor
	 */
	void SetDecoration(EPropertyBagDropState NewDropState, TOptional<FDecoration> OverriddenDecoration = {});

	// ~Begin FGraphEditorDragDropAction interface
	/** Event when the drag and drop operation changes hover targets. */
	virtual void HoverTargetChanged() override;

	/** Visibility of the cursor decorator icon. */
	virtual EVisibility GetIconVisible() const override;
	/** Visibility of the cursor decorator error icon. */
	virtual EVisibility GetErrorIconVisible() const override;

	/**
	 * The drop operation has been executed on an UEdGraphPin in an Editor Graph.
	 * @param ScreenPosition 2D screen coordinates.
	 * @param GraphPosition Local 2D graph coordinates.
	 * @return If the drop operation was handled by the graph.
	 */
	virtual FReply DroppedOnPin(const FVector2f& ScreenPosition, const FVector2f& GraphPosition) override;

	/**
	 * The drop operation has been executed on an UEdGraphNode in an Editor Graph.
	 * @param ScreenPosition 2D screen coordinates.
	 * @param GraphPosition Local 2D graph coordinates.
	 * @return If the drop operation was handled by the graph.
	 */
	virtual FReply DroppedOnNode(const FVector2f& ScreenPosition, const FVector2f& GraphPosition) override;

	/**
	 * The drop operation has been executed on an SGraphPanel in an Editor Graph.
	 * @param Panel The graph panel widget to receive the drop action.
	 * @param ScreenPosition 2D screen coordinates.
	 * @param GraphPosition Local 2D graph coordinates.
	 * @param Graph The graph owner of the target panel.
	 * @return If the drop operation was handled by the graph.
	 */
	virtual FReply DroppedOnPanel(const TSharedRef<SWidget>& Panel, const FVector2f& ScreenPosition, const FVector2f& GraphPosition, UEdGraph& Graph) override;
	// ~End FGraphEditorDragDropAction interface

	/** The property description of the dragged property. */
	const FPropertyBagPropertyDesc PropertyDesc;
	/** Cached state for the decorator. */
	EPropertyBagDropState CurrentDropState = EPropertyBagDropState::Invalid;

private:
	IPropertyBagEdGraphDragAndDrop* GetPropertyBagEdGraphDragAndDropInterface() const;
};

/** Handles drag-and-drop (as a target) for a single property's child widget row. */
class STRUCTUTILSEDITOR_API FPropertyBagDetailsDragDropHandlerTarget : public IDetailDragDropHandler
{
public:
	/**
	 * Property Bag Details - Drag/Drop Handler Default Constructor
	 */
	FPropertyBagDetailsDragDropHandlerTarget() = default;

	/**
	 * Property Bag Details - Drag/Drop Handler Constructor
	 * @param CanAcceptDragDrop [Signature]: TOptional<EItemDropZone>(TSharedPtr<FPropertyBagDetailsDragDropOp>, EItemDropZone)
	 * @param OnDragDrop [Signature]: FReply(const FPropertyBagPropertyDesc&, EItemDropZone)
	 */
	FPropertyBagDetailsDragDropHandlerTarget(const FCanAcceptPropertyBagDetailsRowDropOp& CanAcceptDragDrop, const FOnPropertyBagDetailsRowDropOp& OnDragDrop);

	/** Creates the drag and drop operation. Disabled for the target. Enabled in subclasses. */
	virtual TSharedPtr<FDragDropOperation> CreateDragDropOperation() const override;

	/**
	 * Bind the delegate for determining if this handler can accept this drag and drop operation.
	 * @param CanAcceptDragDrop [Signature]: TOptional<EItemDropZone>(TSharedPtr<FPropertyBagDetailsDragDropOp>, EItemDropZone)
	 */
	virtual void BindCanAcceptDragDrop(FCanAcceptPropertyBagDetailsRowDropOp&& CanAcceptDragDrop);

	/**
	 * Bind the delegate to handle a valid drag and drop operation.
	 * @param OnDragDrop [Signature]: FReply(const FPropertyBagPropertyDesc&, EItemDropZone)
	 */
	virtual void BindOnHandleDragDrop(FOnPropertyBagDetailsRowDropOp&& OnDragDrop);

	/** Disable automatic creation of the handle widget for targets. It is overridden in the source below. */
	virtual bool UseHandleWidget() const override
	{
		return false;
	}

protected:
	/**
	 * Whether the details view child row can accept a drag and drop operation. 
	 * @param DragDropSource The source dragged onto this row.
	 * @param DropZone Zone of the drop, relative to this child row.
	 * @return [Optional] Target zone, which can be overridden. False if the row will not accept.
	 */
	virtual TOptional<EItemDropZone> CanAcceptDrop(const FDragDropEvent& DragDropSource, EItemDropZone DropZone) const override;

	/**
	 * Accept and handle the drag and drop operation.
	 * @param DragDropSource The source dragged onto this row.
	 * @param DropZone Zone of the drop, relative to this child row.
	 * @return True if the operation was successful.
	 */
	virtual bool AcceptDrop(const FDragDropEvent& DragDropSource, EItemDropZone DropZone) const override;

private:
	/**
	 * Callback to determine if the target is acceptable for a details view child row drag and drop operation.
	 * Signature: Optional<EItemDropZone>(TSharedPtr<FPropertyBagDetailsDragDropOp>, EItemDropZone)
	 */
	FCanAcceptPropertyBagDetailsRowDropOp CanAcceptDetailsRowDropOp;

	/**
	 * Callback to determine if the drag to a details view child row was handled successfully.
	 * Signature: FReply(const FPropertyBagPropertyDesc&, EItemDropZone)
	 */
	FOnPropertyBagDetailsRowDropOp OnHandleDetailsRowDropOp;
};

/** Handles drag-and-drop (as a source or target) for a single property's child widget row. */
class STRUCTUTILSEDITOR_API FPropertyBagDetailsDragDropHandler : public FPropertyBagDetailsDragDropHandlerTarget
{
public:
	/**
	 * Constructor.
	 * @param InPropertyDesc The property desc of the property represented by this child widget row.
	 */
	FPropertyBagDetailsDragDropHandler(const FPropertyBagPropertyDesc& InPropertyDesc);

protected:
	/**
	 * Creates the drag and drop operation object that contains necessary information for a drag and drop operation.
	 * @return Shared pointer to the drag and drop operation.
	 */
	virtual TSharedPtr<FDragDropOperation> CreateDragDropOperation() const override;

	/** Enable the automatic creation of the handle (grip) widget for the source. */
	virtual bool UseHandleWidget() const override { return true; }

private:
	/** The current child row's property bag property description. */
	const FPropertyBagPropertyDesc PropertyDesc;
};

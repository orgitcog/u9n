// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DragAndDrop/GraphNodeDragDropOp.h"
#include "Rigs/RigHierarchyDefines.h"

class SRigHierarchyTagWidget;

class FRigElementHierarchyDragDropOp : public FGraphNodeDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FRigElementHierarchyDragDropOp, FGraphNodeDragDropOp)

	static TSharedRef<FRigElementHierarchyDragDropOp> New(const TArray<FRigHierarchyKey>& InElements);

	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;

	/** @return true if this drag operation contains property paths */
	bool HasElements() const
	{
		return Elements.Num() > 0;
	}

	/** @return The property paths from this drag operation */
	const TArray<FRigHierarchyKey>& GetElements() const
	{
		return Elements;
	}

	FString GetJoinedElementNames() const;

	bool IsDraggingSingleConnector() const;
	bool IsDraggingSingleSocket() const;

private:

	/** Data for the property paths this item represents */
	TArray<FRigHierarchyKey> Elements;
};

class FRigHierarchyTagDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FRigHierarchyTagDragDropOp, FDecoratedDragDropOp)

	static TSharedRef<FRigHierarchyTagDragDropOp> New(TSharedPtr<SRigHierarchyTagWidget> InTagWidget);

	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;

	/** @return The identifier being dragged */
	const FString& GetIdentifier() const
	{
		return Identifier;
	}

private:

	FText Text;
	FString Identifier;
};

class FModularRigModuleDragDropOp : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FModularRigModuleDragDropOp, FDragDropOperation)

	static TSharedRef<FModularRigModuleDragDropOp> New(const TArray<FName>& InModuleNames);

	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;

	/** @return true if this drag operation contains dragged modules */
	bool HasModules() const
	{
		return ModuleNames.Num() > 0;
	}

	/** @return The module names from this drag operation */
	const TArray<FName>& GetModules() const
	{
		return ModuleNames;
	}

	FString GetJoinedModuleNames() const;

private:

	/** Data for the module names this item represents */
	TArray<FName> ModuleNames;
};


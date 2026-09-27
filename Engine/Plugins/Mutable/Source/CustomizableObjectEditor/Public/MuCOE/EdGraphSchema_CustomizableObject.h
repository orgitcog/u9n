// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphSchema.h"

#include "EdGraphSchema_CustomizableObject.generated.h"

enum class EMaterialParameterType : uint8;

class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UObject;
struct FAssetData;
struct FEdGraphPinType;
class ICustomizableObjectEditor;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UEdGraphSchema_CustomizableObject : public UEdGraphSchema
{
public:
	GENERATED_BODY()

	// Allowable PinType.PinCategory values
	static const FName PC_Object;
	static const FName PC_Component;
	static const FName PC_Material;
	static const FName PC_Modifier;
	static const FName PC_Mesh;
	static const FName PC_PassThroughMesh;
	static const FName PC_Layout;
	static const FName PC_Image;
	static const FName PC_PassThroughImage;
	static const FName PC_Projector;
	static const FName PC_GroupProjector;
	static const FName PC_Color;
	static const FName PC_Float;
	static const FName PC_Bool;
	static const FName PC_Enum;
	static const FName PC_Stack;
	static const FName PC_MaterialAsset;
	static const FName PC_Wildcard;
	static const FName PC_PoseAsset;
	static const FName PC_Transform;
	static const FName PC_String;

	static const TArray<FName> SupportedMacroPinTypes;

	// Node categories
	static const FText NC_Experimental;

	// EdGraphSchema interface
	virtual void GetAssetsGraphHoverMessage(const TArray<FAssetData>& Assets, const UEdGraph* HoverGraph, FString& OutTooltipText, bool& OutOkIcon) const override;
	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;
	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override;
	virtual bool ShouldHidePinDefaultValue(UEdGraphPin* Pin) const override;
	virtual void DroppedAssetsOnGraph(const TArray<FAssetData>& Assets, const FVector2f& GraphPosition, UEdGraph* Graph) const override;
	virtual void OnPinConnectionDoubleCicked(UEdGraphPin* PinA, UEdGraphPin* PinB, const FVector2f& GraphPosition) const override;
	virtual void GetContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;
	virtual void BreakNodeLinks(UEdGraphNode& TargetNode) const override;
	virtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const override;
	virtual void BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const override;
	virtual bool TryCreateConnection(UEdGraphPin* PinA, UEdGraphPin* PinB) const override;
	virtual FPinConnectionResponse MovePinLinks(UEdGraphPin& MoveFromPin, UEdGraphPin& MoveToPin, bool bIsIntermediateMove, bool bNotifyLinkedNodes) const override;
	virtual FConnectionDrawingPolicy* CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj) const override;

	//  Own interface
	static FLinearColor GetPinTypeColor(const FName& PinType);
	
	/** Given a pin category name, get its friendly name (user readable). */
	static FName GetPinCategoryName(const FName& PinCategory);
	static FText GetPinCategoryFriendlyName(const FName& PinCategory);
};

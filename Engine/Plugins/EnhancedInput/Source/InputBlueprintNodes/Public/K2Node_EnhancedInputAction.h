// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphNodeUtils.h"
#include "K2Node.h"
#include "K2Node_EventNodeInterface.h"
#include "BlueprintNodeSpawner.h"

#include "K2Node_EnhancedInputAction.generated.h"

class UInputAction;
enum class ETriggerEvent : uint8;
namespace ENodeTitleType { enum Type : int; }
struct FBlueprintNodeSignature;

class FBlueprintActionDatabaseRegistrar;
class FKismetCompilerContext;

/**
 * A node spawner which will check if there is already an enhanced input event node (UK2Node_EnhancedInputAction)
 * before spawning a new one in the graph. This makes the behavior of enhanced input action nodes the same as other
 * event nodes. 
 */
UCLASS(Transient)
class UInputActionEventNodeSpawner final : public UBlueprintNodeSpawner
{
	GENERATED_BODY()
public:
	static UInputActionEventNodeSpawner* Create(TSubclassOf<UEdGraphNode> const NodeClass, TObjectPtr<const UInputAction> InAction);
private:
	virtual UEdGraphNode* Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const override;

	UK2Node* FindExistingNode(const UBlueprint* Blueprint) const;

	// We don't want references to node spawners to be keeping any input action assets from GC
    // if you unload a plugin for example, so we keep it as a weak pointer.
	UPROPERTY()
	TWeakObjectPtr<const UInputAction> WeakActionPtr = nullptr;
};

UCLASS()
class INPUTBLUEPRINTNODES_API UK2Node_EnhancedInputAction : public UK2Node, public IK2Node_EventNodeInterface
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TObjectPtr<const UInputAction> InputAction;

	//~ Begin UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	virtual void JumpToDefinition() const override;
	//~ End UEdGraphNode Interface.

	//~ Begin UK2Node Interface
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	virtual bool ShouldShowNodeProperties() const override { return true; }
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual bool CanUserEditPinAdvancedViewFlag() const override { return true; }
	virtual FBlueprintNodeSignature GetSignature() const override;
	virtual void PostReconstructNode();
	virtual void PinConnectionListChanged(UEdGraphPin* Pin);
	virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;
	//~ End UK2Node Interface

	//~ Begin IK2Node_EventNodeInterface Interface.
	virtual TSharedPtr<FEdGraphSchemaAction> GetEventNodeAction(const FText& ActionCategory) override;
	//~ End IK2Node_EventNodeInterface Interface.

	bool HasAnyConnectedEventPins() const;

private:
	FName GetActionName() const;
	void HideEventPins(UEdGraphPin* RetainPin);

	// Iterates each connected trigger event pin until Predicate returns false or we've iterated all active pins 
	void ForEachActiveEventPin(TFunctionRef<bool(ETriggerEvent, UEdGraphPin&)> Predicate) const;

	/** Gets the ETriggerEvent from an exec pin based on the Pins name. */
	const ETriggerEvent GetTriggerTypeFromExecPin(const UEdGraphPin* ExecPin) const;
	
	/** Constructing FText strings can be costly, so we cache the node's title/tooltip */
	FNodeTextCache CachedTooltip;
	FNodeTextCache CachedNodeTitle;
};

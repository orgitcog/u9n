// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMModel/RigVMControllerActions.h"
#include "AnimNextController.h"
// --- ---
#include "AnimNextControllerActions.generated.h"


/**
 * The base action is the base struct for all actions, and provides
 * access to sub actions, merge functionality as well as undo and redo
 * base implementations.
 */
USTRUCT()
struct FAnimNextBaseAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:
	FAnimNextBaseAction() = default;

	explicit FAnimNextBaseAction(URigVMController* InController)
		: FRigVMBaseAction(InController)
	{
		if(InController)
		{
			ControllerPath = FSoftObjectPath(InController);
		}
	}


	// Returns the controller of this action
	inline UAnimNextController* GetAnimNextController() const
	{
		return CastChecked<UAnimNextController>(GetController());
	}
};

/**
 * An action to add or remove a node from the node manifest.
 */
USTRUCT()
struct FAnimNextManifestAction : public FAnimNextBaseAction
{
	GENERATED_BODY()

public:

	FAnimNextManifestAction();
	FAnimNextManifestAction(UAnimNextController* InController, URigVMNode* InNode, bool bInNewIncludeInManifestState);

	virtual ~FAnimNextManifestAction() = default;
	virtual UScriptStruct* GetScriptStruct() const override { return FAnimNextManifestAction::StaticStruct(); }
	virtual bool Merge(const FRigVMBaseAction* Other);
	virtual bool Undo() override;
	virtual bool Redo() override;

	UPROPERTY()
	FString NodePath;

	UPROPERTY()
	bool bOldManifestState = false;

	UPROPERTY()
	bool bIncludeInManifestState = false;
};

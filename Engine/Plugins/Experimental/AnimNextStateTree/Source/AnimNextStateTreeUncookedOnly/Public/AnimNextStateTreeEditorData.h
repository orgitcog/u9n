// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextStateTree.h"
#include "StateTreeEditorData.h"
#include "UncookedOnlyUtils.h"

#include "AnimNextStateTreeEditorData.generated.h"

enum class EAnimNextEditorDataNotifType : uint8;

UCLASS()
class ANIMNEXTSTATETREEUNCOOKEDONLY_API UAnimNextStateTreeTreeEditorData : public UStateTreeEditorData
{
	GENERATED_BODY()

public:
	//~ Begin UStateTreeEditorData overrides
	virtual void PostLoad() override;
	
	virtual const FInstancedPropertyBag& GetRootParametersPropertyBag() const override
	{
		const UStateTree* OuterStateTree = CastChecked<UStateTree>(GetOuter());
		const UAnimNextStateTree* OuterAnimNextStateTree = CastChecked<UAnimNextStateTree>(OuterStateTree->GetOuter());
		return OuterAnimNextStateTree->VariableDefaults;
	}

	virtual void CreateRootProperties(TArrayView<UE::PropertyBinding::FPropertyCreationDescriptor> InOutCreationDescs) override;
	//~ End UStateTreeEditorData overrides

protected:
	void HandleStateTreeAssetChanges(UAnimNextRigVMAssetEditorData* InEditorData, EAnimNextEditorDataNotifType InType, UObject* InSubject) const;

	friend class UAnimNextStateTree_EditorData;
};
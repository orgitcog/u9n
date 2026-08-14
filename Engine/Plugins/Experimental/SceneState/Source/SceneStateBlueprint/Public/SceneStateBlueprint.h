// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Engine/Blueprint.h"
#include "PropertyBindingBindingCollectionOwner.h"
#include "SceneStateBindingCollection.h"
#include "SceneStateBlueprint.generated.h"

class USceneStateGeneratedClass;
class USceneStateMachineGraph;
class USceneStateMachineStateNode;

UCLASS(MinimalAPI, DisplayName="Motion Design Scene State Blueprint")
class USceneStateBlueprint : public UBlueprint, public IPropertyBindingBindingCollectionOwner
{
	GENERATED_BODY()

public:
	SCENESTATEBLUEPRINT_API USceneStateBlueprint(const FObjectInitializer& InObjectInitializer);

	const FGuid& GetRootId() const
	{
		return RootId;
	}

	template<typename T>
	T* FindExtension() const
	{
		return Cast<T>(FindExtension(T::StaticClass()));
	}

	SCENESTATEBLUEPRINT_API UBlueprintExtension* FindExtension(TSubclassOf<UBlueprintExtension> InClass) const;

	SCENESTATEBLUEPRINT_API FSceneStateBindingDesc CreateRootBinding() const;

	//~ Begin IPropertyBindingBindingCollectionOwner
	SCENESTATEBLUEPRINT_API virtual void GetBindableStructs(const FGuid InTargetStructId, TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>>& OutStructDescs) const override;
	SCENESTATEBLUEPRINT_API virtual bool GetBindableStructByID(const FGuid InStructId, TInstancedStruct<FPropertyBindingBindableStructDescriptor>& OutStructDesc) const override;
	SCENESTATEBLUEPRINT_API virtual bool GetBindingDataViewByID(const FGuid InStructId, FPropertyBindingDataView& OutDataView) const override;
	SCENESTATEBLUEPRINT_API virtual FPropertyBindingBindingCollection* GetEditorPropertyBindings() override;
	SCENESTATEBLUEPRINT_API virtual const FPropertyBindingBindingCollection* GetEditorPropertyBindings() const override;
	SCENESTATEBLUEPRINT_API virtual bool CanCreateParameter(const FGuid InStructId) const override;
	SCENESTATEBLUEPRINT_API virtual void CreateParametersForStruct(const FGuid InStructId, TArrayView<UE::PropertyBinding::FPropertyCreationDescriptor> InOutCreationDescs) override;
	//~ End IPropertyBindingBindingCollectionOwner

	//~ Begin UBlueprint
	SCENESTATEBLUEPRINT_API virtual void SetObjectBeingDebugged(UObject* InNewObject) override;
	SCENESTATEBLUEPRINT_API virtual UClass* GetBlueprintClass() const override;
	SCENESTATEBLUEPRINT_API virtual void GetReparentingRules(TSet<const UClass*>& OutAllowedChildrenOfClasses, TSet<const UClass*>& OutDisallowedChildrenOfClasses) const override;
	SCENESTATEBLUEPRINT_API virtual bool SupportedByDefaultBlueprintFactory() const override;
	SCENESTATEBLUEPRINT_API virtual void LoadModulesRequiredForCompilation() override;
	SCENESTATEBLUEPRINT_API virtual bool IsValidForBytecodeOnlyRecompile() const override;
	//~ End UBlueprint

	//~ Begin UObject
	SCENESTATEBLUEPRINT_API virtual void BeginDestroy() override;
	//~ End UObject

	/** The top level State Machine Graphs of this State. Does not include the state machine graphs of nested state nodes */
	UPROPERTY()
	TArray<TObjectPtr<UEdGraph>> StateMachineGraphs;

	/** Holds all the editor bindings prior to compilation */
	UPROPERTY()
	FSceneStateBindingCollection BindingCollection;

private:
	/** Called when a blueprint variable has been renamed. Used to fix the bindings to the variable being renamed to use the new name */
	void OnRenameVariableReferences(UBlueprint* InBlueprint, UClass* InVariableClass, const FName& InOldVariableName, const FName& InNewVariableName);

	/** Called when a blueprint variable has been renamed. Used to fix the path to the variable being renamed to use the new name */
	void RenameVariableReferenceInPath(FPropertyBindingPath& InPath, FName InOldVariableName, FName InNewVariableName);

	/** Called when a state machine graph's parameters have changed */
	void OnGraphParametersChanged(USceneStateMachineGraph* InGraph);

	/** Called when a state machine graph's parameters have changed. Used to fix the path to parameters that have possibly been renamed */
	void UpdateGraphParametersBindings(FPropertyBindingPath& InPath, USceneStateMachineGraph* InGraph);

	/** Unique id representing this blueprint / generated class as a bindable struct */
	UPROPERTY()
	FGuid RootId;

	/** Handle to the delegate when a blueprint variable has been renamed */
	FDelegateHandle OnRenameVariableReferencesHandle;

	/** Handle to the delegate when a state machine graph parameters have changed */
	FDelegateHandle OnGraphParametersChangedHandle;
};

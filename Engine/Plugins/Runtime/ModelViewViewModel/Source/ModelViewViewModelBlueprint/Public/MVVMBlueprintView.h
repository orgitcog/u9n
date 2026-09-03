// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVMBlueprintViewBinding.h"
#include "MVVMBlueprintViewModelContext.h"

#include "MVVMBlueprintView.generated.h"

class UMVVMWidgetBlueprintExtension_View;
class UMVVMBlueprintViewEvent;
class UMVVMBlueprintViewCondition;

class UWidget;
class UWidgetBlueprint;

namespace  UE::MVVM
{
	enum class EBindingMessageType : uint8
	{
		Info,
		Warning,
		Error
	};

	struct FBindingMessage
	{
		FText MessageText;
		EBindingMessageType MessageType;
	};
}

/**
 *
 */
UCLASS(MinimalAPI)
class UMVVMBlueprintViewSettings : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Auto initialize the view sources when the Widget is constructed.
	 * If false, the user will have to initialize the sources manually.
	 * It prevents the sources evaluating until you are ready.
	 */
	UPROPERTY(EditAnywhere, Category = "View")
	bool bInitializeSourcesOnConstruct = true;

	/**
	 * Auto initialize the view bindings when the Widget is constructed.
	 * If false, the user will have to initialize the bindings manually.
	 * It prevents bindings execution and improves performance when you know the widget won't be visible.
	 * @note All bindings are executed when the view is automatically initialized or manually initialized.
	 * @note Sources needs to be initialized before initializing the bindings.
	 * @note When Sources is manually initialized, the bindings will also be initialized if this is true.
	 */
	UPROPERTY(EditAnywhere, Category = "View", meta=(EditCondition="bInitializeSourcesOnConstruct"))
	bool bInitializeBindingsOnConstruct = true;

	/**
	 * Auto initialize the view events when the Widget is constructed.
	 * If false, the user will have to initialize the event manually.
	 */
	UPROPERTY(EditAnywhere, Category = "View")
	bool bInitializeEventsOnConstruct = true;
	
	/**
	 * Create the view even when there are no view bindings or events.
	 * If false, the view models will not be automatically available for use in blueprints if there are no bindings.
	 */
	UPROPERTY(EditAnywhere, Category = "View")
	bool bCreateViewWithoutBindings = false;
};

/**
 * 
 */
UCLASS(Within=MVVMWidgetBlueprintExtension_View)
class MODELVIEWVIEWMODELBLUEPRINT_API UMVVMBlueprintView : public UObject
{
	GENERATED_BODY()

public:
	UMVVMBlueprintView();

public:
	UMVVMBlueprintViewSettings* GetSettings()
	{
		return Settings;
	}

	FMVVMBlueprintViewModelContext* FindViewModel(FGuid ViewModelId);
	const FMVVMBlueprintViewModelContext* FindViewModel(FGuid ViewModelId) const;
	const FMVVMBlueprintViewModelContext* FindViewModel(FName ViewModelName) const;

	void AddViewModel(const FMVVMBlueprintViewModelContext& NewContext);
	bool RemoveViewModel(FGuid ViewModelId);
	int32 RemoveViewModels(const TArrayView<FGuid> ViewModelIds);
	bool RenameViewModel(FName OldViewModelName, FName NewViewModelName);
	bool ReparentViewModel(FGuid ViewModelId, const UClass* ViewModelClass);

	const TArrayView<const FMVVMBlueprintViewModelContext> GetViewModels() const
	{
		return AvailableViewModels; 
	}

	const FMVVMBlueprintViewBinding* FindBinding(const UWidget* Widget, const FProperty* Property) const;
	FMVVMBlueprintViewBinding* FindBinding(const UWidget* Widget, const FProperty* Property);

	void RemoveBinding(const FMVVMBlueprintViewBinding* Binding);
	const FMVVMBlueprintViewBinding* DuplicateBinding(const FMVVMBlueprintViewBinding* Binding);

	void RemoveBindingAt(int32 Index);

	FMVVMBlueprintViewBinding& AddDefaultBinding();

	int32 GetNumBindings() const
	{
		return Bindings.Num();
	}

	FMVVMBlueprintViewBinding* GetBindingAt(int32 Index);
	const FMVVMBlueprintViewBinding* GetBindingAt(int32 Index) const;
	FMVVMBlueprintViewBinding* GetBinding(FGuid Id);
	const FMVVMBlueprintViewBinding* GetBinding(FGuid Id) const;

	TArrayView<FMVVMBlueprintViewBinding> GetBindings()
	{
		return Bindings;
	}

	const TArrayView<const FMVVMBlueprintViewBinding> GetBindings() const
	{
		return Bindings;
	}

	UMVVMBlueprintViewEvent* AddDefaultEvent();
	void AddEvent(UMVVMBlueprintViewEvent* Event);
	void RemoveEvent(UMVVMBlueprintViewEvent* Event);
	UMVVMBlueprintViewEvent* DuplicateEvent(UMVVMBlueprintViewEvent* Event);

	TArrayView<TObjectPtr<UMVVMBlueprintViewEvent>> GetEvents()
	{
		return Events;
	}

	const TArrayView<const TObjectPtr<UMVVMBlueprintViewEvent>> GetEvents() const
	{
		return Events;
	}

	UMVVMBlueprintViewCondition* AddDefaultCondition();
	void AddCondition(UMVVMBlueprintViewCondition* Condition);
	void RemoveCondition(UMVVMBlueprintViewCondition* Condition);
	UMVVMBlueprintViewCondition* DuplicateCondition(UMVVMBlueprintViewCondition* Condition);

	TArrayView<TObjectPtr<UMVVMBlueprintViewCondition>> GetConditions()
	{
		return Conditions;
	}

	const TArrayView<const TObjectPtr<UMVVMBlueprintViewCondition>> GetConditions() const
	{
		return Conditions;
	}

	bool HasAnyTypeOfBinding()
	{
		return !Bindings.IsEmpty() || !Events.IsEmpty() || !Conditions.IsEmpty();
	}

	TArray<FText> GetBindingMessages(FGuid Id, UE::MVVM::EBindingMessageType InMessageType) const;
	bool HasBindingMessage(FGuid Id, UE::MVVM::EBindingMessageType InMessageType) const;
	void AddMessageToBinding(FGuid Id, UE::MVVM::FBindingMessage MessageToAdd);
	void ResetBindingMessages();

	FGuid GetCompiledBindingLibraryId() const
	{
		return CompiledBindingLibraryId;
	}

#if WITH_EDITOR
	virtual void PostLoad() override;
	virtual void PreSave(FObjectPreSaveContext Context) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChainEvent) override;
	virtual void PostEditUndo() override;

	void AddAssetTags(FAssetRegistryTagsContext Context) const;
	UE_DEPRECATED(5.4, "Implement the version that takes FAssetRegistryTagsContext instead.")
	void AddAssetTags(TArray<FAssetRegistryTag>& OutTags) const;
	void OnFieldRenamed(UClass* FieldOwnerClass, FName OldObjectName, FName NewObjectName);
#endif

	virtual void Serialize(FArchive& Ar) override;

	DECLARE_EVENT(UMVVMBlueprintView, FOnBindingsUpdated);
	FOnBindingsUpdated OnBindingsUpdated;

	DECLARE_EVENT(UMVVMBlueprintView, FOnBindingsAdded);
	FOnBindingsAdded OnBindingsAdded;

	DECLARE_EVENT(UMVVMBlueprintView, FOnEventsUpdated);
	FOnEventsUpdated OnEventsUpdated;

	DECLARE_EVENT(UMVVMBlueprintView, FOnConditionsUpdated);
	FOnConditionsUpdated OnConditionsUpdated;

	DECLARE_EVENT_OneParam(UMVVMBlueprintView, FOnEventParametersRegenerate, UMVVMBlueprintViewEvent*);
	FOnEventParametersRegenerate OnEventParametersRegenerate;

	DECLARE_EVENT_OneParam(UMVVMBlueprintView, FOnConditionParametersRegenerate, UMVVMBlueprintViewCondition*);
	FOnConditionParametersRegenerate OnConditionParametersRegenerate;

	DECLARE_EVENT(UMVVMBlueprintView, FOnViewModelsUpdated);
	FOnViewModelsUpdated OnViewModelsUpdated;

	// Use during compilation to clean the automatically generated graph.
	UPROPERTY(Transient, NonTransactional)
	TArray<TObjectPtr<UEdGraph>> TemporaryGraph;

	// Use during compilation to clean the automatically generated graph.
	UPROPERTY(Transient)
	TArray<FName> TemporaryGraphNames;

private:
	UPROPERTY(Instanced)
	TObjectPtr<UMVVMBlueprintViewSettings> Settings;

	UPROPERTY(EditAnywhere, Category = "Viewmodel")
	TArray<FMVVMBlueprintViewBinding> Bindings;
	
	UPROPERTY(Instanced, EditAnywhere, Category = "Viewmodel")
	TArray<TObjectPtr<UMVVMBlueprintViewEvent>> Events;

	UPROPERTY(Instanced, EditAnywhere, Category = "Viewmodel")
	TArray<TObjectPtr<UMVVMBlueprintViewCondition>> Conditions;

	UPROPERTY(EditAnywhere, Category = "Viewmodel")
	TArray<FMVVMBlueprintViewModelContext> AvailableViewModels;

	UPROPERTY(VisibleAnywhere, Category = "Viewmodel", meta = (IgnoreForMemberInitializationTest))
	FGuid CompiledBindingLibraryId;

	TMap<FGuid, TArray<UE::MVVM::FBindingMessage>> BindingMessages;

	bool bIsContextSensitive;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "ScopedTransaction.h"
#include "Interfaces/Interface_BoneReferenceSkeletonProvider.h"
#include "RigEditor/IKRigEditorController.h"

#include "IKRigStructViewer.generated.h"


class UIKRetargeterController;

struct FIKRigStructToView
{
	// the type that corresponds to the struct memory returned by the StructMemoryProvider
	const UScriptStruct* Type = nullptr;
	// provides the memory address of the struct to edit (refreshed after undo/redo)
	// NOTE: we can't just pass in raw pointers to the struct memory because these can be destroyed after a transaction.
	// So instead we pass in TFunction callbacks that get the latest memory locations when the details panel is refreshed.
	TFunction<uint8*()> MemoryProvider;
	// a UObject that owns the struct (this is what will be transacted when the property is edited)
	TWeakObjectPtr<UObject> Owner = nullptr;
	// a unique identifier that callbacks can use to know what struct was modified
	FName UniqueName;

	void Reset()
	{
		Type = nullptr;
		MemoryProvider.Reset();
		Owner = nullptr;
		UniqueName = NAME_None;
	}
	
	bool IsValid() const
	{
		if (!ensure(MemoryProvider.IsSet()))
		{
			return false;
		}
		
		uint8* Memory = MemoryProvider();
		if (!ensure(Memory != nullptr))
		{
			return false;
		}
		
		return Type && Owner.IsValid() && UniqueName != NAME_None;
	}
};

// a thin wrapper around UStruct data to display in a details panel
// this is a generic wrapper that works for any struct
// it is intended to work with FIKRigStructViewerCustomization which simply puts the entire struct in the details panel
// if you need customization, you need to work with UIKRigStructWrapperBase which allows customized derived classes
UCLASS(Blueprintable)
class UIKRigStructViewer : public UObject, public IBoneReferenceSkeletonProvider
{
	GENERATED_BODY()

public:
	
	/**
	 * Configures an instance of a struct to display in the details panel with undo/redo support.
	 * @param InStructToView the struct to view (see FStructToView for details) */
	void SetStructToView(const FIKRigStructToView& InStructToView)
	{
		StructToView = InStructToView;
	}

	virtual bool IsValid() const
	{
		return StructToView.IsValid();
	}

	void Reset()
	{
		StructToView.Reset();
	}

	TSharedPtr<FStructOnScope>& GetStructOnScope()
	{
		uint8* Memory = StructToView.MemoryProvider();
		StructOnScope = MakeShared<FStructOnScope>(StructToView.Type, Memory);
		return StructOnScope;
	}

	FName GetTypeName() const
	{
		// try to get the "nice name" from metadata
		FString TypeName = StructToView.Type->GetMetaData(TEXT("DisplayName"));

		// if no "DisplayName" metadata is found, fall back to the struct name
		if (TypeName.IsEmpty())
		{
			TypeName = StructToView.Type->GetName();
		}
		
		return FName(TypeName);
	}

	UObject* GetStructOwner() const
	{
		return StructToView.Owner.Get();
	};

	void TriggerReinitIfNeeded(const FPropertyChangedEvent& InEvent);

	void SetupPropertyEditingCallbacks(const TSharedPtr<IPropertyHandle>& InProperty);

	// IBoneReferenceSkeletonProvider overrides.
	virtual USkeleton* GetSkeleton(bool& bInvalidSkeletonIsError, const IPropertyHandle* PropertyHandle) override;
	// ~END IBoneReferenceSkeletonProvider overrides.

protected:

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnStructPropertyEdited, const FName& /*UniqueStructName*/, const FPropertyChangedEvent& /*Modified Property*/);
	FOnStructPropertyEdited StructPropertyEditedDelegate;
	// a wrapper of the struct sent to the details panel
	TSharedPtr<FStructOnScope> StructOnScope;
	// the data needed to display and edit an instance of a struct in memory
	FIKRigStructToView StructToView;

public:
	
	// Attach a delegate to be notified whenever a property (or child property) marked " in the currently displayed UStruct is edited
	FOnStructPropertyEdited& OnStructNeedsReinit(){ return StructPropertyEditedDelegate; };
};

class FIKRigStructViewerCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	// END IDetailCustomization interface

	static TArray<IDetailPropertyRow*> AddAllPropertiesToCategoryGroups(
		const TSharedPtr<FStructOnScope>& StructData,
		IDetailLayoutBuilder& InDetailBuilder);
};

// this is meant to be subclassed by a type that contains a UProperty of a struct to be edited
// similar to UIKRigStructViewer but supports multi-struct editing and greater customization
UCLASS(BlueprintType)
class UIKRigStructWrapperBase : public UIKRigStructViewer
{
	GENERATED_BODY()

public:
	
	void Initialize(FIKRigStructToView& InStructToWrap, const FName& InWrapperPropertyName);

	void InitializeWithRetargeter(FIKRigStructToView& InStructToWrap, const FName& InWrapperPropertyName, UIKRetargeterController* InRetargeterController);

	virtual bool IsValid() const override;

	FName GetWrapperPropertyName() const { return WrapperPropertyName; };

	bool IsPropertyHidden(const FName& InPropertyName) const { return PropertiesToHide.Contains(InPropertyName); };

	void SetPropertyHidden(const FName& InPropertyName, bool bHidden);

	void UpdateWrappedStructWithLatestValues();

	void UpdateWrapperStructWithLatestValues();
	
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent ) override;
	
private:
	
	TArray<FName> PropertiesToHide;
	FName WrapperPropertyName;
	FProperty* WrapperProperty;
};

class FIKRigStructWrapperCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	// END IDetailCustomization interface
};
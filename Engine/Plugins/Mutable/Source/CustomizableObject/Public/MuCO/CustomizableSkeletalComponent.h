// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"

#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObject.h"

#include "CustomizableSkeletalComponent.generated.h"

#define UE_API CUSTOMIZABLEOBJECT_API

class UCustomizableSkeletalComponentPrivate;

DECLARE_DELEGATE(FCustomizableSkeletalComponentUpdatedDelegate);


UCLASS(MinimalAPI, Blueprintable, BlueprintType, ClassGroup = (CustomizableObject), meta = (BlueprintSpawnableComponent))
class UCustomizableSkeletalComponent : public USceneComponent
{
	friend UCustomizableSkeletalComponentPrivate;

public:
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = CustomizableSkeletalComponent)
	TObjectPtr<UCustomizableObjectInstance> CustomizableObjectInstance;

	/** This component index refers to the object list of components 
	* DEPRECATED: Use FNames instead with Get/SetComponentName
	*/
	UPROPERTY(BlueprintReadWrite, Category = CustomizableSkeletalComponent)
	int32 ComponentIndex;

private:
	/** Only used if the ComponentIndex is INDEX_NONE.
	 *  Editing this property will set ComponentIndex to INDEX_NONE. */
	UPROPERTY(EditAnywhere, Category = CustomizableSkeletalComponent)
	FName ComponentName;

	UPROPERTY(EditAnywhere, Category = CustomizableSkeletalComponent)
	bool bSkipSetReferenceSkeletalMesh = false;
	
public:
	FCustomizableSkeletalComponentUpdatedDelegate UpdatedDelegate;

private:
	UPROPERTY()
	bool bSkipSkipSetSkeletalMeshOnAttach = false;
	
	UPROPERTY(Instanced)
	TObjectPtr<UCustomizableSkeletalComponentPrivate> Private;

public:
	
	UE_API UCustomizableSkeletalComponent();
	
protected:

	// USceneComponent interface
	UE_API virtual void OnAttachmentChanged() override;

public:
	// Own interface
	
	UFUNCTION(BlueprintCallable, Category = CustomizableSkeletalComponent)
	UE_API void SetComponentName(const FName& Name);

	UFUNCTION(BlueprintCallable, Category = CustomizableSkeletalComponent)
	UE_API FName GetComponentName() const;
	
	UFUNCTION(BlueprintCallable, Category = CustomizableSkeletalComponent)
	UE_API UCustomizableObjectInstance* GetCustomizableObjectInstance() const;
	
	UFUNCTION(BlueprintCallable, Category = CustomizableSkeletalComponent)
	UE_API void SetCustomizableObjectInstance(UCustomizableObjectInstance* Instance);
	
	/** Set to true to avoid automatically replacing the Skeletal Mesh of the parent Skeletal Mesh Component by the Reference Skeletal Mesh.
	 * If SkipSetSkeletalMeshOnAttach is true, it will not replace it. */
	UFUNCTION(BlueprintCallable, Category = CustomizableSkeletalComponent)
	UE_API void SetSkipSetReferenceSkeletalMesh(bool bSkip);

	UFUNCTION(BlueprintCallable, Category = CustomizableSkeletalComponent)
	UE_API bool GetSkipSetReferenceSkeletalMesh() const;

	/** Set to true to avoid automatically replacing the Skeletal Mesh of the parent Skeletal Mesh Component with any mesh. */
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstanceUsage)
	UE_API void SetSkipSetSkeletalMeshOnAttach(bool bSkip);

	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstanceUsage)
	UE_API bool GetSkipSetSkeletalMeshOnAttach() const;
	
	/** Update Skeletal Mesh asynchronously. */
	UFUNCTION(BlueprintCallable, Category = CustomizableSkeletalComponent)
	UE_API void UpdateSkeletalMeshAsync(bool bNeverSkipUpdate = false);

	/** Update Skeletal Mesh asynchronously. Callback will be called once the update finishes, even if it fails. */
	UFUNCTION(BlueprintCallable, Category = CustomizableSkeletalComponent)
	UE_API void UpdateSkeletalMeshAsyncResult(FInstanceUpdateDelegate Callback, bool bIgnoreCloseDist = false, bool bForceHighPriority = false);

	UE_API UCustomizableSkeletalComponentPrivate* GetPrivate();

	UE_API const UCustomizableSkeletalComponentPrivate* GetPrivate() const;
};

#undef UE_API

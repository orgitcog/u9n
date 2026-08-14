// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Expressions/TG_Expression_MaterialBase.h"
#include "Materials/Material.h"
#include "TG_Texture.h"
#include "TG_Material.h"

#include "TG_Expression_Material.generated.h"

typedef std::weak_ptr<class Job>		JobPtrW;

UCLASS()
class TEXTUREGRAPH_API UTG_Expression_Material : public UTG_Expression_MaterialBase
{
	GENERATED_BODY()

public:
	UTG_Expression_Material();
	virtual ~UTG_Expression_Material();
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif

	virtual void PostLoad() override;

#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(5.6, "Material field has been deprecated.")
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use the InputMaterial to specify the Material asset referenced"))
	TObjectPtr<UMaterialInterface> Material_DEPRECATED;
#endif // WITH_EDITORONLY_DATA

	// The input material referenced by this Material node
	UPROPERTY(EditAnywhere, Setter, Category = NoCategory, meta = (TGType = "TG_InputParam", DisplayName = "Material", PinDisplayName = "Material"))
	FTG_Material InputMaterial;
	void SetInputMaterial(const FTG_Material& InMaterial);

	// The Material attribute identifier among all the attributes of the material that is rendered in the output
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Category = NoCategory, meta = (TGType = "TG_Setting", GetOptions = "GetRenderAttributeOptions"))
	FName RenderedAttribute;
	void SetRenderedAttribute(FName InRenderedAttribute);

	// THe list of Rendered attribute options available 
	UFUNCTION(CallInEditor)
	TArray<FName> GetRenderAttributeOptions() const;

	virtual bool CanHandleAsset(UObject* Asset) override;
	virtual void SetAsset(UObject* Asset) override;
	virtual FText GetTooltipText() const override { return FText::FromString(TEXT("Renders a material into a quad and makes it available. It is automatically exposed as a graph input parameter.")); } 

protected:
	// Transient and per instance Data, recreated on every new instance from the reference material
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<UMaterialInterface> MaterialCopy = nullptr;

	// As the "Material" or the "InputMaterial" properties change
	// we update the current active "MaterialCopy" and pass on internally as the active "GetMaterial()"
	virtual void SetMaterialInternal(UMaterialInterface* InMaterial) override;

	// Title name is still used mostly for legacy but not exposed anymore in the details.
	// This is changed on the node itself and then call SetTitleName and rename the InnputMaterial Alias name
	UPROPERTY()
	FName TitleName = TEXT("Material");

	virtual void Initialize() override;

public:
	
	virtual void SetTitleName(FName NewName) override;
	virtual FName GetTitleName() const override;

	virtual FName GetCategory() const override { return TG_Category::Input;}
	
protected:
	virtual TObjectPtr<UMaterialInterface> GetMaterial() const override { return MaterialCopy;};
	virtual EDrawMaterialAttributeTarget GetRenderedAttributeId()  override;

#if WITH_EDITOR // Listener for referenced material being saved to update the integration in TG 
	FDelegateHandle PreSaveHandle; 
	void OnReferencedObjectPreSave(UObject* Object, FObjectPreSaveContext SaveContext);
#endif

};


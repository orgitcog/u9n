// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "WidgetBlueprintExtension.h"

#include "Extensions/UIComponent.h"

#include "UIComponentWidgetBlueprintExtension.generated.h"

class UUIComponentUserWidgetExtension;
class UUIComponentContainer;

UCLASS()
/***
 * Extension to the Widget Blueprint that will save the Components and will be used in the editor for compilation.
 */
class UMGEDITOR_API UUIComponentWidgetBlueprintExtension : public UWidgetBlueprintExtension
{
	GENERATED_BODY()
	
	explicit UUIComponentWidgetBlueprintExtension(const FObjectInitializer& ObjectInitializer = FObjectInitializer());

public:
	static const FName MD_ComponentVariable;

	UUIComponent* AddComponent(const UClass* ComponentClass, FName OwnerName);
	void RemoveComponent(const UClass* ComponentClass, FName OwnerName);
	TArray<UUIComponent*> GetComponentsFor(const UWidget* Target) const;
	UUIComponent* GetComponent(const UClass* ComponentClass, FName OwnerName) const;

	UUIComponentUserWidgetExtension* GetOrCreateExtension(UUserWidget* PreviewWidget);	
	void RenameWidget(const FName& OldVarName, const FName& NewVarName);

	bool VerifyContainer(UUserWidget* UserWidget) const;

protected:
	virtual void HandleBeginCompilation(FWidgetBlueprintCompilerContext& InCreationContext) override;	
	virtual void HandleCleanAndSanitizeClass(UWidgetBlueprintGeneratedClass* ClassToClean, UObject* OldCDO) override;
	virtual void HandlePopulateGeneratedVariables(const FWidgetBlueprintCompilerContext::FPopulateGeneratedVariablesContext& Context) override;
	virtual void HandleFinishCompilingClass(UWidgetBlueprintGeneratedClass* Class) override;
	virtual bool HandleValidateGeneratedClass(UWidgetBlueprintGeneratedClass* Class) override; 

	virtual void HandleEndCompilation() override;

private:	
	UUIComponentContainer* DuplicateContainer(UObject* Outer) const;

	FWidgetBlueprintCompilerContext* CompilerContext = nullptr;

#if WITH_EDITORONLY_DATA	
	UPROPERTY()
	TObjectPtr<UUIComponentContainer> ComponentContainer;
#endif //WITH_EDITORONLY_DATA
};
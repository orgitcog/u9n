// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectMacroLibrary/CustomizableObjectMacroLibraryFactory.h"

#include "MuCOE/CustomizableObjectGraph.h"
#include "MuCOE/CustomizableObjectMacroLibrary/CustomizableObjectMacroLibrary.h"

UCustomizableObjectMacroLibraryFactory::UCustomizableObjectMacroLibraryFactory() 
{
	SupportedClass = UCustomizableObjectMacroLibrary::StaticClass();
	bEditAfterNew = true;
}

UObject* UCustomizableObjectMacroLibraryFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UCustomizableObjectMacroLibrary* NewMacroLibrary = NewObject<UCustomizableObjectMacroLibrary>(InParent, Class, Name, Flags, Context);
	
	if (NewMacroLibrary)
	{
		NewMacroLibrary->AddMacro();
	}

	return NewMacroLibrary;
}

bool UCustomizableObjectMacroLibraryFactory::CanCreateNew() const
{
	return true;
}
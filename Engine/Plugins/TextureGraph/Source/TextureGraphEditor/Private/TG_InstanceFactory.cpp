// Copyright Epic Games, Inc. All Rights Reserved.

#include "TG_InstanceFactory.h"
#include "Model/ModelObject.h"
#include "TextureGraph.h"

UTG_InstanceFactory::UTG_InstanceFactory()
{
	SupportedClass = UTextureGraphInstance::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UTG_InstanceFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UTextureGraphInstance* TextureGraphInstance = NewObject<UTextureGraphInstance>(InParent, Class, Name, Flags, Context);

	check(TextureGraphInstance);
	TextureGraphInstance->Construct(Name.GetPlainNameString());
	TextureGraphInstance->SetParent(InitialParent);
	return TextureGraphInstance;
}

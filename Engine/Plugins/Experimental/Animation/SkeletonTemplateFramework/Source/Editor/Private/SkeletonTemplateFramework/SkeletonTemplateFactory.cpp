// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletonTemplateFramework/SkeletonTemplateFactory.h"
#include "SkeletonTemplateFramework/SkeletonTemplate.h"
#include "UObject/Package.h"

USkeletonTemplateFactory::USkeletonTemplateFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = USkeletonTemplate::StaticClass();
}

bool USkeletonTemplateFactory::ConfigureProperties()
{
	return true;
}

UObject* USkeletonTemplateFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	EObjectFlags FlagsToUse = Flags | RF_Public | RF_Standalone | RF_Transactional | RF_LoadCompleted;
	if (InParent == GetTransientPackage())
	{
		FlagsToUse &= ~RF_Standalone;
	}

	USkeletonTemplate* NewBinding = NewObject<USkeletonTemplate>(InParent, Class, Name, FlagsToUse);

	return NewBinding;
}

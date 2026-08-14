// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanIdentityFactoryNew.h"
#include "MetaHumanIdentity.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaHumanIdentityFactoryNew)

UMetaHumanIdentityFactoryNew::UMetaHumanIdentityFactoryNew()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UMetaHumanIdentity::StaticClass();
}

UObject* UMetaHumanIdentityFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* Context, FFeedbackContext* Warn)
{
	UObject* NewIdentity = NewObject<UMetaHumanIdentity>(InParent, InClass, InName, InFlags | RF_Transactional);
	
	// Disable exporting for the identity asset until we implement a custom exporter
	// JIRA: MH-7716
	check(NewIdentity);
	check(NewIdentity->GetPackage());
	NewIdentity->GetPackage()->SetPackageFlags(PKG_DisallowExport);

	return NewIdentity;
}

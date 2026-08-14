// Copyright Epic Games, Inc. All Rights Reserved.

#include "DNAImporterLibrary.h"
#include "DNAAssetImportFactory.h"

DEFINE_LOG_CATEGORY_STATIC(LogDNAImporterLibrary, Log, All);

#define LOCTEXT_NAMESPACE "RigLogicEditor"

void UDNAImporterLibrary::ImportSkeletalMeshDNA(const FString FileName, UObject* Mesh)
{
	UDNAAssetImportFactory* Factory = Cast<UDNAAssetImportFactory>(UDNAAssetImportFactory::StaticClass()->GetDefaultObject());
	//Reimport will do the same thing as import we just won't get problems when having the same DNA name as SkeletalMesh, new DNA gets initialized anyway
	bool bSuccess = FReimportManager::Instance()->Reimport(Mesh, false, false, FileName, Factory, INDEX_NONE, false, true, false);
	
	if (!bSuccess)
	{
		const FText Message = LOCTEXT("DNA_ReimportFailedMessage", "Reimporting of DNA failed");
		UE_LOG(LogDNAImporterLibrary, Error, TEXT("%s"), *Message.ToString());
	}
}

#undef LOCTEXT_NAMESPACE

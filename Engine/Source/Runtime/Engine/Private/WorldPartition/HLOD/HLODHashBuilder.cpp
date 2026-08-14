// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/HLOD/HLODHashBuilder.h"

#if WITH_EDITOR

#include "Engine/HLODProxy.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"
#include "Misc/TransformUtilities.h"
#include "WorldPartition/HLOD/HLODBuilder.h"

void FHLODHashBuilder::PushContext(const TCHAR* Context)
{
	LogContext(Context, false);

	++IndentationLevel;
}

void FHLODHashBuilder::PopContext()
{
	check(IndentationLevel > 0);
	--IndentationLevel;
}

FArchive& FHLODHashBuilder::operator<<(FTransform InTransform)
{
	*this << TransformUtilities::GetRoundedTransformCRC32(InTransform);
	return *this;
}

FArchive& FHLODHashBuilder::operator<<(UObject*& InObject)
{
	if (UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(InObject))
	{
		FHLODHashScope HashScope(*this, TEXT("UMaterialInterface"));

		*this << UHLODProxy::GetCRC(MaterialInterface);
		*this << FHLODHashContext(TEXT("%s (%s)"), *InObject->GetClass()->GetName(), *InObject->GetName());

		TArray<UTexture*> Textures;
		MaterialInterface->GetUsedTextures(Textures, EMaterialQualityLevel::High, true, ERHIFeatureLevel::SM5, true);
		*this << Textures;
	
		if (UMaterialInterface* NaniteOverride = MaterialInterface->GetNaniteOverride())
		{ 
			*this << UHLODProxy::GetCRC(NaniteOverride);
			*this << FHLODHashContext(TEXT("%s (%s)"), *NaniteOverride->GetClass()->GetName(), *NaniteOverride->GetName());

			Textures.Reset();
			NaniteOverride->GetUsedTextures(Textures, EMaterialQualityLevel::High, true, ERHIFeatureLevel::SM5, true);
			*this << Textures;
		}
	}
	else if (UTexture* Texture = Cast<UTexture>(InObject))
	{
		FHLODHashScope HashScope(*this, TEXT("UTexture"));

		*this << UHLODProxy::GetCRC(Texture);
		*this << FHLODHashContext(TEXT("%s (%s)"), *InObject->GetClass()->GetName(), *InObject->GetName());
	}
	else if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(InObject))
	{
		FHLODHashScope HashScope(*this, TEXT("UStaticMesh"));

		*this << UHLODProxy::GetCRC(StaticMesh);
		*this << FHLODHashContext(TEXT("%s (%s)"), *InObject->GetClass()->GetName(), *InObject->GetName());
	}
	else
	{
		FArchive::operator<<(InObject);
	}
	
	return *this;
}

FArchive& FHLODHashBuilder::operator<<(UMaterialInterface* InMaterialInterface)
{
	UObject* Object = InMaterialInterface;
	return *this << Object;
}

FArchive& FHLODHashBuilder::operator<<(UTexture* InTexture)
{
	UObject* Object = InTexture;
	return *this << Object;
}

FArchive& FHLODHashBuilder::operator<<(UStaticMesh* InStaticMesh)
{
	UObject* Object = InStaticMesh;
	return *this << Object;
}

FArchive& FHLODHashBuilder::operator<<(USkinnedAsset* InSkinnedAsset)
{
	UObject* Object = InSkinnedAsset;
	return *this << Object;
}

void FHLODHashBuilder::LogContext(const TCHAR* Context, bool bOutputHash)
{
	FString Indentation = FString::ChrN(IndentationLevel * 4, TEXT(' '));

	if (bOutputHash)
	{
		UE_LOG(LogHLODBuilder, VeryVerbose, TEXT("%s- %s = %x"), *Indentation, Context, GetCrc());
	}
	else
	{
		UE_LOG(LogHLODBuilder, VeryVerbose, TEXT("%s- %s"), *Indentation, Context);
	}
}

#endif // #if WITH_EDITOR

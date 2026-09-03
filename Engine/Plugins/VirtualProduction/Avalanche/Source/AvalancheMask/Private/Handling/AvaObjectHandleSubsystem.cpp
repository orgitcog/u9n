// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handling/AvaObjectHandleSubsystem.h"

#include "AvaHandleUtilities.h"
#include "AvaMaskActorMaterialCollectionHandle.h"
#include "AvaMaskAvaShapeMaterialCollectionHandle.h"
#include "AvaMaskLog.h"
#include "AvaMaskMaterialReference.h"
#include "AvaMaskMediaPlateMaterialHandle.h"
#include "AvaMaskText3DActorMaterialCollectionHandle.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "Handling/AvaMaskDesignedMaterialHandle.h"
#include "Handling/AvaMaskMaterialInstanceHandle.h"
#include "Handling/AvaMaskParametricMaterialHandle.h"
#include "IAvaMaskMaterialCollectionHandle.h"
#include "Material/DynamicMaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectIterator.h"

void UAvaObjectHandleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FindObjectHandleFactories();
}

TSharedPtr<IAvaObjectHandle> UAvaObjectHandleSubsystem::MakeHandleDirect(const FAvaMaskMaterialReference& InInstance, FName InTag)
{
	for (const TPair<FIsSupportedFunction, FMakeHandleFunction>& SupportedFactoryPair : ObjectHandleFactories)
	{
		if (SupportedFactoryPair.Key(InInstance, InTag))
		{
			TSharedPtr<IAvaObjectHandle> Handle = SupportedFactoryPair.Value(InInstance);
			if (!UE::Ava::Internal::IsHandleValid(Handle))
			{
				if (InTag.IsNone())
				{
					UE_LOG(LogAvaMask, Warning, TEXT("Object Handle for '%s' was created but invalid."), *InInstance.ToString())
				}
				else
				{
					UE_LOG(LogAvaMask, Warning, TEXT("Object Handle for '%s' (with tag '%s') was created but invalid."), *InInstance.ToString(), *InTag.ToString())
				}
			}
			return Handle;
		}
	}

	UE_LOG(LogAvaMask, Display, TEXT("No ObjectHandle found for '%s'"), *InInstance.ToString());
	return nullptr;
}

// @note: registration order matters!
void UAvaObjectHandleSubsystem::FindObjectHandleFactories()
{
	// Material Collection Handles
	{
		ObjectHandleFactories.Add({
			FAvaMaskAvaShapeMaterialCollectionHandle::IsSupported
			, [](const FAvaMaskMaterialReference& InObject) -> TSharedPtr<IAvaMaskMaterialCollectionHandle>
			{
				return MakeShared<FAvaMaskAvaShapeMaterialCollectionHandle>(InObject.GetTypedObject<AActor>());
			}});

		ObjectHandleFactories.Add({
			FAvaMaskText3DActorMaterialCollectionHandle::IsSupported
			, [](const FAvaMaskMaterialReference& InObject) -> TSharedPtr<IAvaMaskMaterialCollectionHandle>
			{
				return MakeShared<FAvaMaskText3DActorMaterialCollectionHandle>(InObject.GetTypedObject<AActor>());
			}});

		ObjectHandleFactories.Add({
			FAvaMaskActorMaterialCollectionHandle::IsSupported
			, [](const FAvaMaskMaterialReference& InObject) -> TSharedPtr<IAvaMaskMaterialCollectionHandle>
			{
				return MakeShared<FAvaMaskActorMaterialCollectionHandle>(InObject.GetTypedObject<AActor>());
			}});
	}

	// Material Handles
	{
		ObjectHandleFactories.Add({
			FAvaMaskDesignedMaterialHandle::IsSupported
			, [](const FAvaMaskMaterialReference& InMaterial) -> TSharedPtr<IAvaMaskMaterialHandle>
			{
				return MakeShared<FAvaMaskDesignedMaterialHandle>(InMaterial.GetTypedObject<UDynamicMaterialInstance>());
			}});

		ObjectHandleFactories.Add({
			FAvaMaskParametricMaterialHandle::IsSupported
			, [](const FAvaMaskMaterialReference& InMaterial) -> TSharedPtr<IAvaMaskMaterialHandle>
			{
				return MakeShared<FAvaMaskParametricMaterialHandle>(InMaterial);
			}});

		ObjectHandleFactories.Add({
			FAvaMaskMediaPlateMaterialHandle::IsSupported
			, [](const FAvaMaskMaterialReference& InMaterial) -> TSharedPtr<IAvaMaskMaterialHandle>
			{
				return MakeShared<FAvaMaskMediaPlateMaterialHandle>(InMaterial.GetTypedObject<UMaterialInterface>());
			}});

		ObjectHandleFactories.Add({
			FAvaMaskMaterialInstanceHandle::IsSupported
			, [](const FAvaMaskMaterialReference& InMaterial) -> TSharedPtr<IAvaMaskMaterialHandle>
			{
				return MakeShared<FAvaMaskMaterialInstanceHandle>(InMaterial.GetTypedObject<UMaterialInterface>());
			}});
	}
}

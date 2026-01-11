// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaMaskLog.h"
#include "AvaMaskMaterialReference.h"
#include "IAvaObjectHandle.h"
#include "Subsystems/EngineSubsystem.h"
#include "AvaObjectHandleSubsystem.generated.h"

class UAvaObjectHandleSubsystem;

/** Responsible for providing Handlers for a given UObject. */
UCLASS()
class UAvaObjectHandleSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	// ~Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	// ~End USubsystem

	TSharedPtr<IAvaObjectHandle> MakeHandleDirect(const FAvaMaskMaterialReference& InInstance, FName InTag = NAME_None);

	template <typename HandleType, typename ObjectType = UObject
		UE_REQUIRES(!std::is_const_v<HandleType> && std::is_base_of_v<IAvaObjectHandle, HandleType>
				&&	!std::is_const_v<ObjectType> && TModels_V<CStaticClassProvider, std::decay_t<ObjectType>>)>
    TSharedPtr<HandleType> MakeHandle(ObjectType* InInstance, FName InTag = NAME_None)
    {
		if (!InInstance)
		{
			UE_LOG(LogAvaMask, Warning, TEXT("Invalid or null object provided to MakeHandle"));
			return nullptr;
		}
    	return StaticCastSharedPtr<HandleType>(MakeHandleDirect(FAvaMaskMaterialReference(InInstance), InTag));
    }

	template<typename HandleType UE_REQUIRES(!std::is_const_v<HandleType> && std::is_base_of_v<IAvaObjectHandle, HandleType>)>
	TSharedPtr<HandleType> MakeHandle(const FAvaMaskMaterialReference& InInstance, FName InTag = NAME_None)
	{
		return StaticCastSharedPtr<HandleType>(MakeHandleDirect(InInstance, InTag));
	}

private:
	void FindObjectHandleFactories();

	using FIsSupportedFunction = TFunction<bool(const FAvaMaskMaterialReference&, FName)>;
	using FMakeHandleFunction = TFunction<TSharedPtr<IAvaObjectHandle>(const FAvaMaskMaterialReference&)>;

	TArray<TPair<FIsSupportedFunction, FMakeHandleFunction>> ObjectHandleFactories;
};

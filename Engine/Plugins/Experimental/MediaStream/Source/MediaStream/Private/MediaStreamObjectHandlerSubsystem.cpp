// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaStreamObjectHandlerSubsystem.h"

#include "Engine/Engine.h"
#include "MediaStreamModule.h"
#include "MediaStreamObjectHandlerManager.h"

UMediaStreamObjectHandlerSubsystem* UMediaStreamObjectHandlerSubsystem::Get()
{
	if (!UObjectInitialized())
	{
		UE_LOG(LogMediaStream, Error, TEXT("UObject system not initialized in UMediaStreamObjectHandlerSubsystem::Get"));
		return nullptr;
	}

	if (!GEngine)
	{
		UE_LOG(LogMediaStream, Error, TEXT("Invalid GEngine in UMediaStreamObjectHandlerSubsystem::Get"));
		return nullptr;
	}

	UMediaStreamObjectHandlerSubsystem* ObjectHandlerSubsystem = GEngine->GetEngineSubsystem<UMediaStreamObjectHandlerSubsystem>();

	if (!ObjectHandlerSubsystem)
	{
		UE_LOG(LogMediaStream, Error, TEXT("Unable to find Media Source Object Handler Subsystem in UMediaStreamObjectHandlerSubsystem::Get"));
		return nullptr;
	}

	return ObjectHandlerSubsystem;
}

bool UMediaStreamObjectHandlerSubsystem::CanHandleObject(const UClass* InClass) const
{
	return FMediaStreamObjectHandlerManager::Get().CanHandleObject(InClass);
}

UMediaPlayer* UMediaStreamObjectHandlerSubsystem::CreateMediaPlayer(const FMediaStreamObjectHandlerCreatePlayerParams& InParams) const
{
	return FMediaStreamObjectHandlerManager::Get().CreateOrUpdatePlayer(InParams);
}

bool UMediaStreamObjectHandlerSubsystem::HasObjectHandler(const UClass* InClass) const
{
	return FMediaStreamObjectHandlerManager::Get().HasObjectHandler(InClass);
}

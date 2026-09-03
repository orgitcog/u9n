// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameplayEffectTypes.h"
#include "Abilities/GameplayAbilityTargetDataFilter.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitGameplayEffectApplied.generated.h"

#define UE_API GAMEPLAYABILITIES_API

class UAbilitySystemComponent;

UCLASS(MinimalAPI)
class UAbilityTask_WaitGameplayEffectApplied : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UFUNCTION()
	UE_API void OnApplyGameplayEffectCallback(UAbilitySystemComponent* Target, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle);

	UE_API virtual void Activate() override;

	FGameplayTargetDataFilterHandle Filter;
	FGameplayTagRequirements SourceTagRequirements;
	FGameplayTagRequirements TargetTagRequirements;

	FGameplayTagQuery SourceTagQuery;
	FGameplayTagQuery TargetTagQuery;

	bool TriggerOnce;
	bool ListenForPeriodicEffects;

	UE_API void SetExternalActor(AActor* InActor);

protected:

	UE_API UAbilitySystemComponent* GetASC();

	virtual void BroadcastDelegate(AActor* Avatar, FGameplayEffectSpecHandle SpecHandle, FActiveGameplayEffectHandle ActiveHandle) { }
	virtual void RegisterDelegate() { }
	virtual void RemoveDelegate() { }

	UE_API virtual void OnDestroy(bool AbilityEnded) override;

	bool RegisteredCallback;
	bool UseExternalOwner;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> ExternalOwner;

	// If we are in the process of broadcasting and should not accept additional GE callbacks
	bool Locked;
};

#undef UE_API

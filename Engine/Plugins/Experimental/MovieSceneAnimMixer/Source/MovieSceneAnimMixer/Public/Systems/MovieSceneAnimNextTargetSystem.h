// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "EntitySystem/MovieSceneEntitySystem.h"
#include "MovieSceneMixedAnimationTarget.h"
#include "Injection/IEvaluationModifier.h"
#include "Injection/InjectionRequest.h"
#include "Component/AnimNextComponent.h"
#include "MovieSceneAnimNextTargetSystem.generated.h"


struct FAnimNextEvaluationTask;

struct FMovieSceneAnimMixerEvaluationModifier : public UE::AnimNext::IEvaluationModifier
{
	FMovieSceneAnimMixerEvaluationModifier(TSharedPtr<FAnimNextEvaluationTask> InTaskToInject)
		: TaskToInject(InTaskToInject)
	{
	}

	// IEvaluationModifier interface
	virtual void PreEvaluate(UE::AnimNext::FEvaluateTraversalContext& Context) const override {}

	virtual void PostEvaluate(UE::AnimNext::FEvaluateTraversalContext& Context) const override;

	TSharedPtr<FAnimNextEvaluationTask> TaskToInject = nullptr;

};

/**
 * Declaring a unique target for targeting an injection site on an Unreal Animation Framework module. Will find the default injection site, or one specified by name.
 */
USTRUCT(meta=(DisplayName="UAF Module Injection"))
struct MOVIESCENEANIMMIXER_API FMovieSceneAnimNextInjectionTarget : public FMovieSceneMixedAnimationTarget
{
	GENERATED_BODY() 

	// Site name to use for injection. If empty will use the default one found for the actor.
	// TODO: Custom UI that finds injection sites from modules.
	UPROPERTY(EditAnywhere, Category="Animation")
	FName InjectionSiteName;

	FORCEINLINE friend uint32 GetTypeHash(const FMovieSceneAnimNextInjectionTarget& Target)
	{
		return HashCombine(GetTypeHash(FMovieSceneAnimNextInjectionTarget::StaticStruct()), GetTypeHash(Target.InjectionSiteName));
	}
};

struct FMovieSceneAnimNextTargetData
{
	TWeakObjectPtr<UAnimNextComponent> AnimNextComponent = nullptr;
	UE::AnimNext::FInjectionSite InjectionSite;
	UE::AnimNext::FInjectionRequestPtr InjectionRequestHandle = nullptr;
	TSharedPtr<FMovieSceneAnimMixerEvaluationModifier> Modifier = nullptr;
};

// System that handles applying animation mixer evaluation tasks to an injection site in an Unreal Animation Framework module
UCLASS(MinimalAPI)
class UMovieSceneAnimNextTargetSystem
	: public UMovieSceneEntitySystem
{
public:

	GENERATED_BODY()

	UMovieSceneAnimNextTargetSystem(const FObjectInitializer& ObjInit);

	MOVIESCENEANIMMIXER_API static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	void OnUnlink() override;

	TArray<FMovieSceneAnimNextTargetData> CurrentTargets;

private:

	virtual void OnSchedulePersistentTasks(UE::MovieScene::IEntitySystemScheduler* TaskScheduler) override;
};
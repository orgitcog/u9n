// Copyright Epic Games, Inc. All Rights Reserved.


#include "LearningAgentsGym.h"
#include "LearningLog.h"
#include "LearningAgentsEntityInterface.h"
#include "LearningAgentsLearningComponentInterface.h"
#include "Kismet/KismetMathLibrary.h"

ALearningAgentsGymBase::ALearningAgentsGymBase()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void ALearningAgentsGymBase::Initialize()
{
	if (!RandomStream.IsValid())
	{
		RandomStream = MakeShareable(new FRandomStream);
		RandomStream->Initialize(RandomSeed);
	}
	PopulateLearningComponents();
	for (TScriptInterface<ILearningAgentsLearningComponentInterface>& LearningComponent : LearningComponents)
	{
		LearningComponent->InitializeLearningComponent();
	}
	
	OnGymInitialized.Broadcast();
}

void ALearningAgentsGymBase::Reset()
{
	OnBeginGymReset.Broadcast();

	for (TScriptInterface<ILearningAgentsLearningComponentInterface>& LearningComponent : LearningComponents)
	{
		LearningComponent->ResetLearningComponent();
	}

	OnPostGymReset.Broadcast();
}

void ALearningAgentsGymBase::GetRandomStream(FRandomStream& OutRandomStream) const
{
	OutRandomStream = *RandomStream;
}

TSharedPtr<FRandomStream> ALearningAgentsGymBase::GetRandomStream() const
{
	return RandomStream;
}

void ALearningAgentsGymBase::SetRandomStream(const TSharedPtr<FRandomStream>& InRandomStream)
{
	RandomStream = InRandomStream;
}

bool ALearningAgentsGymBase::IsMemberOfGym(TObjectPtr<AActor> Actor) const
{
	if (Actor and Actor->Implements<ULearningAgentsEntityInterface>())
	{
		return ILearningAgentsEntityInterface::Execute_GetGym(Actor) == this;
	}
	return false;
}

void ALearningAgentsGymBase::PopulateLearningComponents()
{
	TArray<TObjectPtr<UActorComponent>> AllComponents;
	this->GetComponents(AllComponents);

	for (TObjectPtr<UActorComponent> Component : AllComponents)
	{
		if (Component && Component->Implements<ULearningAgentsLearningComponentInterface>())
		{
			TScriptInterface<ILearningAgentsLearningComponentInterface> LearningComponentInterface = TScriptInterface<ILearningAgentsLearningComponentInterface>(Component);
			LearningComponents.Add(LearningComponentInterface);
		}
	}
}

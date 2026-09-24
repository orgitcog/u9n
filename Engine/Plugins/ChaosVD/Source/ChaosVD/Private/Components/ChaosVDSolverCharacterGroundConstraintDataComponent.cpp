// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/ChaosVDSolverCharacterGroundConstraintDataComponent.h"

#include "ChaosVDCharacterGroundConstraintDataProviderInterface.h"
#include "ChaosVDParticleDataComponent.h"
#include "ChaosVDScene.h"
#include "ChaosVDSettingsManager.h"
#include "Selection.h"
#include "Actors/ChaosVDSolverInfoActor.h"
#include "Settings/ChaosVDCharacterConstraintsVisualizationSettings.h"

void UChaosVDSolverCharacterGroundConstraintDataComponent::HandleSceneUpdated()
{
	if (const UChaosVDCharacterConstraintsVisualizationSettings* CharacterConstraintsVisualizationSettings = FChaosVDSettingsManager::Get().GetSettingsObject<UChaosVDCharacterConstraintsVisualizationSettings>())
	{
		if (!CharacterConstraintsVisualizationSettings->bAutoSelectConstraintFromSelectedParticle)
		{
			return;
		}
	}

	const TSharedPtr<FChaosVDScene> ScenePtr = SceneWeakPtr.Pin();
	if (!ScenePtr)
	{
		return;
	}

	AChaosVDSolverInfoActor* OwnerSolverData = Cast<AChaosVDSolverInfoActor>(GetOwner());
	if (!OwnerSolverData)
	{
		return;
	}

	UChaosVDParticleDataComponent* ParticleDataComponent = OwnerSolverData->GetParticleDataComponent();
	if (!ParticleDataComponent)
	{
		return;
	}

	TSharedPtr<FChaosVDSolverDataSelection> SolverDataSelection = ScenePtr->GetSolverDataSelectionObject().Pin();
	if(!SolverDataSelection)
	{
		return;
	}

	if (FChaosVDSceneParticle* ParticleInstance = ParticleDataComponent->GetSelectedParticle())
	{
		if (ParticleInstance->HasCharacterGroundConstraintData())
		{
			TArray<TSharedPtr<FChaosVDCharacterGroundConstraint>> FoundConstraintData;
			ParticleInstance->GetCharacterGroundConstraintData(FoundConstraintData);
			SolverDataSelection->SelectData(SolverDataSelection->MakeSelectionHandle(FoundConstraintData[0]));
		}
	}
}

void UChaosVDSolverCharacterGroundConstraintDataComponent::BeginDestroy()
{
	Super::BeginDestroy();

	const TSharedPtr<FChaosVDScene> ScenePtr = SceneWeakPtr.Pin();
	if (!ScenePtr)
	{
		return;
	}
	
	ScenePtr->OnSceneUpdated().RemoveAll(this);
}

void UChaosVDSolverCharacterGroundConstraintDataComponent::SetScene(const TWeakPtr<FChaosVDScene>& InSceneWeakPtr)
{
	Super::SetScene(InSceneWeakPtr);
	
	const TSharedPtr<FChaosVDScene> ScenePtr = SceneWeakPtr.Pin();
	if (!ScenePtr)
	{
		return;
	}
	
	ScenePtr->OnSceneUpdated().AddUObject(this, &UChaosVDSolverCharacterGroundConstraintDataComponent::HandleSceneUpdated);
}

void UChaosVDSolverCharacterGroundConstraintDataComponent::UpdateFromSolverFrameData(const FChaosVDSolverFrameData& InSolverFrameData)
{
	UpdateConstraintData(InSolverFrameData.RecordedCharacterGroundConstraints);
}


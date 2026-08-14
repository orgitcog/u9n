// Copyright Epic Games, Inc. All Rights Reserved.

#include "Effector/Effects/CEEffectorEffectBase.h"

#include "Effector/CEEffectorComponent.h"

void UCEEffectorEffectBase::OnExtensionParametersChanged(UCEEffectorComponent* InComponent)
{
	Super::OnExtensionParametersChanged(InComponent);
	
	UpdateEffectChannelData(InComponent->GetChannelData(), /** Enabled */true);
}

void UCEEffectorEffectBase::OnExtensionDeactivated()
{
	Super::OnExtensionDeactivated();

	if (UCEEffectorComponent* EffectorComponent = GetEffectorComponent())
	{
		UpdateEffectChannelData(EffectorComponent->GetChannelData(), /** Enabled */false);
	}
}

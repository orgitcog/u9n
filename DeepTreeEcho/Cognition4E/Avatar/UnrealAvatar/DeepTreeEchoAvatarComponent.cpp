#include "DeepTreeEchoAvatarComponent.h"
#include "AGICoreCommunication.h"

UDeepTreeEchoAvatarComponent::UDeepTreeEchoAvatarComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDeepTreeEchoAvatarComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AGICoreComm)
	{
		AGICoreComm->OnAGIStateUpdate.AddDynamic(this, &UDeepTreeEchoAvatarComponent::OnAGIStateUpdate);
	}
}

void UDeepTreeEchoAvatarComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UDeepTreeEchoAvatarComponent::OnAGIStateUpdate(const FAGIStateUpdateMessage& StateUpdateMessage)
{
	// Process the AGI state update
}

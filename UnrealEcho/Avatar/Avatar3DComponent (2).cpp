#include "Avatar3DComponent.h"
#include "FacialAnimationSystem.h"
#include "GestureSystem.h"
#include "EmotionalAuraComponent.h"
#include "CognitiveVisualizationComponent.h"

UAvatar3DComponent::UAvatar3DComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UAvatar3DComponent::BeginPlay()
{
    Super::BeginPlay();

    // Initialize sub-components
    if (FacialSystem)
    {
        FacialSystem->RegisterComponent();
    }

    if (GestureSystem)
    {
        GestureSystem->RegisterComponent();
    }

    if (EmotionalAura)
    {
        EmotionalAura->RegisterComponent();
    }

    if (CognitiveViz)
    {
        CognitiveViz->RegisterComponent();
    }
}

void UAvatar3DComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Tick sub-components
}

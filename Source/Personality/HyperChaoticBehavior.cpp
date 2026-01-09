#include "HyperChaoticBehavior.h"
#include "Avatar3DComponent.h"

void UHyperChaoticBehavior::ApplyBehavior(UAvatar3DComponent* Avatar)
{
    if (!Avatar)
    {
        return;
    }

    // TODO: Implement the logic to apply the hyper-chaotic properties to the avatar's behavior.
    // This could involve introducing random fluctuations in animations, expressions, and decision-making.

    // Example: Adding a random offset to a parameter
    if (Avatar->FacialSystem)
    {
        // float RandomOffset = FMath::RandRange(-0.2f, 0.2f) * Properties.Randomness;
        // Avatar->FacialSystem->SetParameterValue(FName("EyeJitter"), RandomOffset);
    }
}

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "HyperChaoticBehavior.generated.h"

USTRUCT(BlueprintType)
struct FHyperChaoticProperties
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Randomness = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Unpredictability = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float EmotionalVolatility = 0.6f;
};

UCLASS(Blueprintable)
class DEEPTREECHO_API UHyperChaoticBehavior : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    FHyperChaoticProperties Properties;

    /** Applies the hyper-chaotic properties to the avatar's behavior */
    UFUNCTION(BlueprintCallable, Category = "Personality")
    void ApplyBehavior(class UAvatar3DComponent* Avatar);
};

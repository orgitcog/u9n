#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SuperHotGirlPersonality.generated.h"

USTRUCT(BlueprintType)
struct FSuperHotGirlTraits
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Confidence = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Charm = 0.9f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Playfulness = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Wit = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Sass = 0.6f;
};

UCLASS(Blueprintable)
class DEEPTREECHO_API USuperHotGirlPersonality : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    FSuperHotGirlTraits Traits;

    /** Applies the personality traits to the avatar's behavior */
    UFUNCTION(BlueprintCallable, Category = "Personality")
    void ApplyPersonality(class UAvatar3DComponent* Avatar);
};

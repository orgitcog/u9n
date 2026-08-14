#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "Avatar3DComponent.generated.h"

// Forward declarations
class UFacialAnimationSystem;
class UGestureSystem;
class UEmotionalAuraComponent;
class UCognitiveVisualizationComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEEPTREECHO_API UAvatar3DComponent : public USkeletalMeshComponent
{
    GENERATED_BODY()

public:
    UAvatar3DComponent();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar")
    class UFacialAnimationSystem* FacialSystem;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar")
    class UGestureSystem* GestureSystem;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar")
    class UEmotionalAuraComponent* EmotionalAura;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar")
    class UCognitiveVisualizationComponent* CognitiveViz;
};

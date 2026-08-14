#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepTreeEchoAvatarComponent.generated.h"

class AAGICoreCommunication;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DEEPTREEECHOAVATAR_API UDeepTreeEchoAvatarComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UDeepTreeEchoAvatarComponent();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGI")
	AAGICoreCommunication* AGICoreComm;

	UFUNCTION()
	void OnAGIStateUpdate(const struct FAGIStateUpdateMessage& StateUpdateMessage);
};

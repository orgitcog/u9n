#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AGIComms.h"
#include "AGICoreCommunication.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAGIStateUpdate, const FAGIStateUpdateMessage&, StateUpdateMessage);

class AAGIPCGManager;

UCLASS()
class DEEPTREEECHOAVATAR_API AAGICoreCommunication : public AActor
{
	GENERATED_BODY()
	
public:	
	AAGICoreCommunication();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(BlueprintAssignable, Category = "AGI")
	FOnAGIStateUpdate OnAGIStateUpdate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGI|PCG")
	AAGIPCGManager* PCGManager;

private:
	void GenerateMockStateUpdate(float DeltaTime);

	float TimeAccumulator;
};

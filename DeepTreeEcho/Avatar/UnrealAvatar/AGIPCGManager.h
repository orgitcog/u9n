#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AGIComms.h"
#include "AGIPCGManager.generated.h"

UCLASS()
class DEEPTREEECHOAVATAR_API AAGIPCGManager : public AActor
{
	GENERATED_BODY()
	
public:	
	AAGIPCGManager();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	void ExecutePCGCommand(const FPCGCommand& PCGCommand);
};

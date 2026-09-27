// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UTBBaseCommand.h"
#include "FillStaticMeshActor.generated.h"

/**
 * 
 */
UCLASS()
class USERTOOLBOXBASICCOMMAND_API UFillStaticMeshActor : public UUTBBaseCommand
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="FillStaticMeshActor")
	TArray<FString>	RootPaths;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="FillStaticMeshActor")
	bool	bAffectOnlyEmptyStaticMeshActor=true;
	
	void Execute() override;
	UFillStaticMeshActor();
};

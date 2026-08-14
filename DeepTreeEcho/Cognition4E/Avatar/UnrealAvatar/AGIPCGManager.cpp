#include "AGIPCGManager.h"
#include "PCGComponent.h"
#include "PCGGraph.h"

AAGIPCGManager::AAGIPCGManager()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AAGIPCGManager::BeginPlay()
{
	Super::BeginPlay();
}

void AAGIPCGManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AAGIPCGManager::ExecutePCGCommand(const FPCGCommand& PCGCommand)
{
	if (PCGCommand.CommandType == EPCGCommandType::Spawn)
	{
		UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *PCGCommand.TargetGraph);
		if (Graph)
		{
			FActorSpawnParameters SpawnParams;
			AActor* PCGActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), PCGCommand.Location, PCGCommand.Rotation, SpawnParams);
			if (PCGActor)
			{
				UPCGComponent* PCGComponent = NewObject<UPCGComponent>(PCGActor);
				PCGComponent->SetGraph(Graph);
				PCGComponent->RegisterComponent();
				PCGActor->SetActorScale3D(PCGCommand.Scale);

				for (auto const& [Key, Val] : PCGCommand.Parameters)
				{
				}
			}
		}
	}
}

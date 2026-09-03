// Copyright Epic Games, Inc. All Rights Reserved. 
#include "Scene/InterchangeActorFactory.h"

#include "InterchangeActorFactoryNode.h"
#include "InterchangeCameraFactoryNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Nodes/InterchangeFactoryBaseNode.h"
#include "Scene/InterchangeActorHelper.h"

#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/ActorComponent.h"

#if WITH_EDITORONLY_DATA
#include "Engine/World.h"
#include "Layers/LayersSubsystem.h"
#include "Layers/Layer.h"
#include "Editor/EditorEngine.h"

extern UNREALED_API UEditorEngine* GEditor;
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeActorFactory)

UClass* UInterchangeActorFactory::GetFactoryClass() const
{
	return AActor::StaticClass();
}

void UInterchangeActorFactory::ExecuteResetObjectProperties(const UInterchangeBaseNodeContainer* BaseNodeContainer, UInterchangeFactoryBaseNode* FactoryNode, UObject* ImportedObject)
{
	using namespace UE::Interchange;

	FImportSceneObjectsParams TempSceneObjectParmeters;
	TempSceneObjectParmeters.FactoryNode = FactoryNode;
	TempSceneObjectParmeters.NodeContainer = BaseNodeContainer;
	TempSceneObjectParmeters.ReimportObject = ImportedObject;
	
	if (AActor* ImportedActor = ActorHelper::SpawnFactoryActor(TempSceneObjectParmeters))
	{
		if (UInterchangeActorFactoryNode* ActorFactoryNode = Cast<UInterchangeActorFactoryNode>(FactoryNode))
		{
			if (UObject* ObjectToUpdate = ProcessActor(*ImportedActor, *ActorFactoryNode, *BaseNodeContainer, TempSceneObjectParmeters))
			{
				if (USceneComponent* RootComponent = ImportedActor->GetRootComponent())
				{
					UActorComponent* ActorComponent = Cast<UActorComponent>(ObjectToUpdate);
					if (ActorComponent)
					{
#if WITH_EDITOR
						ActorComponent->PreEditChange(nullptr);
#endif
						ActorComponent->UnregisterComponent();
					}

					// Cache mobility value to allow application of transform
					EComponentMobility::Type CachedMobility = RootComponent->Mobility;
					RootComponent->SetMobility(EComponentMobility::Type::Movable);
					
					ApplyAllCustomAttributesToObject(TempSceneObjectParmeters, *ImportedActor, ObjectToUpdate);

					// Restore mobility value
					if (CachedMobility != EComponentMobility::Type::Movable)
					{
						RootComponent->SetMobility(CachedMobility);
					}
					
					if (ActorComponent)
					{
						ActorComponent->RegisterComponent();
#if WITH_EDITOR
						ActorComponent->PostEditChange();
#endif
					}
					
					return;
				}
			}
		}
	}

	Super::ExecuteResetObjectProperties(BaseNodeContainer, FactoryNode, ImportedObject);	
}

UObject* UInterchangeActorFactory::ImportSceneObject_GameThread(const UInterchangeFactoryBase::FImportSceneObjectsParams& CreateSceneObjectsParams)
{
	using namespace UE::Interchange;

	UInterchangeActorFactoryNode* FactoryNode = Cast<UInterchangeActorFactoryNode>(CreateSceneObjectsParams.FactoryNode);
	if (!ensure(FactoryNode) || !CreateSceneObjectsParams.NodeContainer)
	{
		return nullptr;
	}

	AActor* SpawnedActor = ActorHelper::SpawnFactoryActor(CreateSceneObjectsParams);

	if (SpawnedActor)
	{
		if (UObject* ObjectToUpdate = ProcessActor(*SpawnedActor, *FactoryNode, *CreateSceneObjectsParams.NodeContainer, CreateSceneObjectsParams))
		{
			if (USceneComponent* RootComponent = SpawnedActor->GetRootComponent())
			{
				// Cache mobility value to allow application of transform
				EComponentMobility::Type CachedMobility = RootComponent->Mobility;
				RootComponent->SetMobility(EComponentMobility::Type::Movable);

				ApplyAllCustomAttributesToObject(CreateSceneObjectsParams, *SpawnedActor, ObjectToUpdate);

				// Restore mobility value
				if (CachedMobility != EComponentMobility::Type::Movable)
				{
					RootComponent->SetMobility(CachedMobility);
				}
			}
		}

		ProcessTags(FactoryNode, SpawnedActor);

		ProcessLayerNames(FactoryNode, SpawnedActor);
	}

	return SpawnedActor;
}

UObject* UInterchangeActorFactory::ProcessActor(AActor& SpawnedActor, const UInterchangeActorFactoryNode& /*FactoryNode*/, const UInterchangeBaseNodeContainer& /*NodeContainer*/, const FImportSceneObjectsParams& /*Params*/)
{
	return SpawnedActor.GetRootComponent();
}

void UInterchangeActorFactory::ProcessTags(UInterchangeActorFactoryNode* FactoryNode, AActor* SpawnedActor)
{
	TArray<FString> TagsArray;
	FactoryNode->GetTags(TagsArray);

	TSet<FString> Tags(TagsArray);
	TSet<FName> AlreadySetTags(SpawnedActor->Tags);

	for (const FString& Tag : Tags)
	{
		FName TagName(Tag);
		if (!AlreadySetTags.Contains(TagName))
		{
			SpawnedActor->Tags.Add(TagName);
		}
	}
}

void UInterchangeActorFactory::ProcessLayerNames(UInterchangeActorFactoryNode* FactoryNode, AActor* SpawnedActor)
{
	TArray<FString> LayerNamesArray;
	FactoryNode->GetLayerNames(LayerNamesArray);

	TSet<FString> LayerNames(LayerNamesArray);
#if WITH_EDITORONLY_DATA
	AddUniqueLayersToWorld(SpawnedActor->GetWorld(), LayerNames);
#endif

	TSet<FName> AlreadySetLayerNames(SpawnedActor->Layers);

	for (const FString& LayerNameString : LayerNames)
	{
		FName LayerName(LayerNameString);
		if (!AlreadySetLayerNames.Contains(LayerName))
		{
			SpawnedActor->Layers.Add(FName(LayerName));
		}
	}
}

#if WITH_EDITORONLY_DATA
void UInterchangeActorFactory::AddUniqueLayersToWorld(UWorld* World, const TSet<FString>& LayerNames)
{
	if (!World || !IsValidChecked(World) || World->IsUnreachable() || LayerNames.Num() == 0)
	{
		return;
	}

	TSet< FName > ExistingLayers;
	for (ULayer* Layer : World->Layers)
	{
		ExistingLayers.Add(Layer->GetLayerName());
	}

	int32 NumberOfExistingLayers = World->Layers.Num();

	ULayersSubsystem* LayersSubsystem = GEditor ? GEditor->GetEditorSubsystem<ULayersSubsystem>() : nullptr;
	for (const FString& LayerNameString : LayerNames)
	{
		FName LayerName(LayerNameString);

		if (!ExistingLayers.Contains(LayerName))
		{
			// Use the ILayers if we are adding the layers to the currently edited world
			if (LayersSubsystem && GWorld && World == GWorld.GetReference())
			{
				LayersSubsystem->CreateLayer(LayerName);
			}
			else
			{
				ULayer* NewLayer = NewObject<ULayer>(World, NAME_None, RF_Transactional);
				if (!ensure(NewLayer != NULL))
				{
					continue;
				}

				World->Layers.Add(NewLayer);

				NewLayer->SetLayerName(LayerName);
				NewLayer->SetVisible(true);
			}
		}
	}

	if (NumberOfExistingLayers != World->Layers.Num())
	{
		World->Modify();
	}
}
#endif

void UInterchangeActorFactory::ApplyAllCustomAttributesToObject(const UInterchangeFactoryBase::FImportSceneObjectsParams& CreateSceneObjectsParams, AActor& SpawnedActor, UObject* ObjectToUpdate)
{
	using namespace UE::Interchange;
	ActorHelper::ApplyAllCustomAttributes(CreateSceneObjectsParams, *ObjectToUpdate);
}

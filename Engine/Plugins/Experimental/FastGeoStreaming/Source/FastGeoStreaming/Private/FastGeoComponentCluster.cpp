// Copyright Epic Games, Inc. All Rights Reserved.

#include "FastGeoComponentCluster.h"
#include "FastGeoContainer.h"
#include "SceneInterface.h"
#include "PrimitiveSceneInfo.h"
#include "FastGeoWeakElement.h"

const FFastGeoElementType FFastGeoComponentCluster::Type(&IFastGeoElement::Type);

FFastGeoComponentCluster::FFastGeoComponentCluster(UFastGeoContainer* InOwner, FName InName, FFastGeoElementType InType)
	: Super(InType)
	, Owner(InOwner)
	, Name(InName.ToString())
	, ComponentClusterIndex(INDEX_NONE)
{
}

#if WITH_EDITOR
FFastGeoComponentCluster::FFastGeoComponentCluster(const FFastGeoComponentCluster& InOther)
	: Super(InOther.ElementType)
{
	ComponentClusterIndex = InOther.ComponentClusterIndex;
	Owner = InOther.Owner;
	Name = InOther.Name;
	StaticMeshComponents = InOther.StaticMeshComponents;
	InstancedStaticMeshComponents = InOther.InstancedStaticMeshComponents;
	SkinnedMeshComponents = InOther.SkinnedMeshComponents;
	InstancedSkinnedMeshComponents = InOther.InstancedSkinnedMeshComponents;
}
#endif

FArchive& operator<<(FArchive& Ar, FFastGeoComponentCluster& ComponentCluster)
{
	ComponentCluster.Serialize(Ar);
	return Ar;
}

void FFastGeoComponentCluster::Serialize(FArchive& Ar)
{
	Ar << Name;
	Ar << ComponentClusterIndex;
	Ar << StaticMeshComponents;
	Ar << InstancedStaticMeshComponents;
	Ar << SkinnedMeshComponents;
	Ar << InstancedSkinnedMeshComponents;
}

void FFastGeoComponentCluster::InitializeDynamicProperties()
{
	ForEachComponent([this](FFastGeoComponent& Component)
	{
		Component.SetOwnerComponentCluster(this);
		Component.InitializeDynamicProperties();
	});
}

void FFastGeoComponentCluster::SetOwnerContainer(UFastGeoContainer* InOwner)
{
	check(InOwner);
	Owner = InOwner;
}

void FFastGeoComponentCluster::SetComponentClusterIndex(int32 InComponentClusterIndex)
{
	ComponentClusterIndex = InComponentClusterIndex;
}

UFastGeoContainer* FFastGeoComponentCluster::GetOwnerContainer() const
{
	return Owner;
}

FFastGeoComponent* FFastGeoComponentCluster::GetComponent(uint32 InComponentTypeID, int32 InComponentIndex)
{
	if (FFastGeoInstancedStaticMeshComponent::Type.IsSameTypeID(InComponentTypeID))
	{
		return InstancedStaticMeshComponents.IsValidIndex(InComponentIndex) ? &InstancedStaticMeshComponents[InComponentIndex] : nullptr;
	}
	else if (FFastGeoStaticMeshComponent::Type.IsSameTypeID(InComponentTypeID))
	{
		return StaticMeshComponents.IsValidIndex(InComponentIndex) ? &StaticMeshComponents[InComponentIndex] : nullptr;
	}
	else if (FFastGeoInstancedSkinnedMeshComponent::Type.IsSameTypeID(InComponentTypeID))
	{
		return InstancedSkinnedMeshComponents.IsValidIndex(InComponentIndex) ? &InstancedSkinnedMeshComponents[InComponentIndex] : nullptr;
	}
	else if (FFastGeoSkinnedMeshComponent::Type.IsSameTypeID(InComponentTypeID))
	{
		return SkinnedMeshComponents.IsValidIndex(InComponentIndex) ? &SkinnedMeshComponents[InComponentIndex] : nullptr;
	}
	check(false);
	return nullptr;
}

ULevel* FFastGeoComponentCluster::GetLevel() const
{
	return GetOwnerContainer()->GetLevel();
}

void FFastGeoComponentCluster::UpdateVisibility()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FFastGeoComponentCluster::UpdateVisibility);

	TArray<FFastGeoPrimitiveComponent*> ShowComponents;
	TArray<FFastGeoPrimitiveComponent*> HideComponents;
	ForEachComponent<FFastGeoPrimitiveComponent>([&ShowComponents, &HideComponents](FFastGeoPrimitiveComponent& Component)
	{
		FWriteScopeLock WriteLock(Component.Lock.Get());
		const bool bOldIsDrawnInGame = Component.IsDrawnInGame();
		Component.UpdateVisibility();
		const bool bIsDrawnInGame = Component.IsDrawnInGame();
		if (bIsDrawnInGame != bOldIsDrawnInGame)
		{
			if (FPrimitiveSceneProxy* SceneProxy = Component.GetSceneProxy())
			{
				if (bIsDrawnInGame)
				{
					ShowComponents.Add(&Component);
				}
				else
				{
					HideComponents.Add(&Component);
				}
			}
		}
	});

	UpdateVisibility_Internal(MoveTemp(ShowComponents), MoveTemp(HideComponents));
}

void FFastGeoComponentCluster::ForceUpdateVisibility(const TArray<FFastGeoPrimitiveComponent*>& Components, int32 UpdateCounter)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FFastGeoComponentCluster::ForceUpdateVisibility);

	TArray<FFastGeoPrimitiveComponent*> ShowComponents;
	TArray<FFastGeoPrimitiveComponent*> HideComponents;
	for (FFastGeoPrimitiveComponent* Component : Components)
	{
		FWriteScopeLock WriteLock(Component->Lock.Get());
		Component->UpdateVisibility();
		const bool bIsDrawnInGame = Component->IsDrawnInGame();
		if (FPrimitiveSceneProxy* SceneProxy = Component->GetSceneProxy())
		{
			if (bIsDrawnInGame)
			{
				ShowComponents.Add(Component);
			}
			else
			{
				HideComponents.Add(Component);
			}
		}
	}

	UpdateVisibility_Internal(MoveTemp(ShowComponents), MoveTemp(HideComponents), ++UpdateCounter);
}

void FFastGeoComponentCluster::UpdateVisibility_Internal(TArray<FFastGeoPrimitiveComponent*>&& ShowComponents, TArray<FFastGeoPrimitiveComponent*>&& HideComponents, int32 UpdateCounter)
{
	if (!ShowComponents.IsEmpty() || !HideComponents.IsEmpty())
	{
		FWeakFastGeoComponentCluster ClusterWeak(this);
		ENQUEUE_RENDER_COMMAND()([ClusterWeak, UpdateCounter, ShowComponents = MoveTemp(ShowComponents), HideComponents = MoveTemp(HideComponents)](FRHICommandListBase&) mutable
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FFastGeoComponentCluster::UpdateVisibility_RenderThread);

			if (!ClusterWeak.Get())
			{
				return;
			}

			TArray<FFastGeoPrimitiveComponent*> NotReadyComponents;
			auto ProcessComponents = [&NotReadyComponents](TArray<FFastGeoPrimitiveComponent*>& InOutComponents, bool bShow)
			{
				for (FFastGeoPrimitiveComponent* Component : InOutComponents)
				{
					FReadScopeLock ReadLock(Component->Lock.Get());
					if (FPrimitiveSceneProxy* Proxy = Component->GetSceneProxy())
					{
						// Test whether the primitive was added to the scene (or is pending)
						FPrimitiveSceneInfo* PrimitiveSceneInfo = Proxy->GetPrimitiveSceneInfo();
						if (!PrimitiveSceneInfo->IsIndexValid())
						{
							NotReadyComponents.Add(Component);
						}
						else
						{
							Proxy->GetScene().UpdatePrimitivesDrawnInGame_RenderThread(MakeArrayView(&Proxy, 1), bShow);
						}
					}
				}
			};

			ProcessComponents(ShowComponents, true);
			ProcessComponents(HideComponents, false);

			if (!NotReadyComponents.IsEmpty())
			{
				UE::Tasks::Launch(TEXT("ForceUpdateVisibility"), [ClusterWeak, UpdateCounter, NotReadyComponents = MoveTemp(NotReadyComponents)]()
				{
					if (FFastGeoComponentCluster* Cluster = ClusterWeak.Get())
					{
						Cluster->ForceUpdateVisibility(NotReadyComponents, UpdateCounter);
					}
				}, LowLevelTasks::ETaskPriority::Normal, UE::Tasks::EExtendedTaskPriority::GameThreadNormalPri);
			}
		});
	}
}

#if WITH_EDITOR
FFastGeoComponent& FFastGeoComponentCluster::AddComponent(FFastGeoElementType InComponentType)
{
	FFastGeoComponent* NewComponent = nullptr;

	if (InComponentType == FFastGeoInstancedStaticMeshComponent::Type)
	{
		NewComponent = &InstancedStaticMeshComponents.Emplace_GetRef(InstancedStaticMeshComponents.Num());
	}
	else if (InComponentType == FFastGeoStaticMeshComponent::Type)
	{
		NewComponent = &StaticMeshComponents.Emplace_GetRef(StaticMeshComponents.Num());
	}
	else if (InComponentType == FFastGeoSkinnedMeshComponent::Type)
	{
		NewComponent = &SkinnedMeshComponents.Emplace_GetRef(SkinnedMeshComponents.Num());
	}
	else if (InComponentType == FFastGeoInstancedSkinnedMeshComponent::Type)
	{
		NewComponent = &InstancedSkinnedMeshComponents.Emplace_GetRef(InstancedSkinnedMeshComponents.Num());
	}
	else
	{
		unimplemented(); // Should never be reached, missing type handling
	}

	return *NewComponent;
}

bool FFastGeoComponentCluster::HasComponents() const
{
	return InstancedStaticMeshComponents.Num() > 0 || StaticMeshComponents.Num() > 0 || InstancedSkinnedMeshComponents.Num() > 0 || SkinnedMeshComponents.Num() > 0;
}
#endif
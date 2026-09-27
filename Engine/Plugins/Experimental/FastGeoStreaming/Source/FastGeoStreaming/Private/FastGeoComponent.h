// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IFastGeoElement.h"
#include "FastGeoElementType.h"
#include "Physics/Experimental/AsyncPhysicsStateProcessorInterface.h"
#include "FastGeoComponent.generated.h"

class UWorld;
class UBodySetup;
class UActorComponent;
class FFastGeoComponentCluster;
class UFastGeoContainer;
class FSceneInterface;
class FPrimitiveSceneProxy;
class FRegisterComponentContext;

class FASTGEOSTREAMING_API FFastGeoComponent : public IFastGeoElement
{
public:
	typedef IFastGeoElement Super;

	/** Static type identifier for this element class */
	static const FFastGeoElementType Type;

	FFastGeoComponent(int32 InComponentIndex = INDEX_NONE, FFastGeoElementType InType = Type);
	virtual ~FFastGeoComponent() = default;

#if WITH_EDITOR
	virtual void InitializeFromComponent(UActorComponent* Component);
	virtual UClass* GetEditorProxyClass() const;
#endif

	int32 GetComponentIndex() const { return ComponentIndex; }
	virtual UBodySetup* GetBodySetup() const { return nullptr; }
	virtual bool IsCollisionEnabled() const { return false; }
	virtual void Serialize(FArchive& Ar);
	virtual void InitializeDynamicProperties() {}
	virtual void OnAsyncCreatePhysicsState();
	virtual void OnAsyncDestroyPhysicsStateBegin_GameThread();
	virtual void OnAsyncDestroyPhysicsState();
	virtual void OnAsyncDestroyPhysicsStateEnd_GameThread();
	void OnAsyncCreatePhysicsStateEnd_GameThread();

	FFastGeoComponentCluster* GetOwnerComponentCluster() const;
	UFastGeoContainer* GetOwnerContainer() const;
	UWorld* GetWorld() const;
	bool IsRegistered() const;
	FLinearColor GetDebugColor() const;

#if WITH_EDITOR
	void SetEditorProxy(UFastGeoComponentEditorProxy* InComponentEditorProxy)
	{
		ComponentEditorProxy = InComponentEditorProxy;
	}
		
	template <typename TEditorProxy>
	TEditorProxy* GetEditorProxy() const
	{
		return Cast<TEditorProxy>(ComponentEditorProxy);
	}
#endif

protected:
	// Persistent Data
	int32 ComponentIndex;

	// Transient Data
	FFastGeoComponentCluster* Owner;

	enum EPhysicsStateCreation
	{
		NotCreated,
		Creating,
		Created,
		Destroying
	};

	EPhysicsStateCreation PhysicsStateCreation = EPhysicsStateCreation::NotCreated;

private:
	void SetOwnerComponentCluster(FFastGeoComponentCluster* InOwner);
	friend class FFastGeoComponentCluster;

	friend FArchive& operator<<(FArchive& Ar, FFastGeoComponent& Component);

#if WITH_EDITOR
	UFastGeoComponentEditorProxy* ComponentEditorProxy;
#endif
};


UCLASS()
class UFastGeoComponentEditorProxy : public UObject
{
	GENERATED_BODY()

#if WITH_EDITOR
	friend class UFastGeoContainer;

protected:
	template <typename TComponent>
	const TComponent& GetComponent() const
	{
		return FastGeoComponent->CastToRef<TComponent>();
	}

	template <typename TComponent>
	TComponent& GetComponent()
	{
		return FastGeoComponent->CastToRef<TComponent>();
	}

private:
	void SetFastGeoComponent(FFastGeoComponent* InFastGeoComponent)
	{
		FastGeoComponent = InFastGeoComponent;
	}

	const FFastGeoComponent* GetFastGeoComponent() const
	{
		return FastGeoComponent;
	}

private:
	FFastGeoComponent* FastGeoComponent;
#endif
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "HAL/Platform.h"
#include "IStereoLayers.h"
#include "IXRLoadingScreen.h"
#include "Misc/AssertionMacros.h"
#include "Misc/ScopeLock.h"
#include "RHI.h"
#include "Templates/Function.h"
#include "Templates/RefCounting.h"
#include "Templates/TypeHash.h"

/**
	Partial implementation of the Layer management code for the IStereoLayers interface.
	Implements adding, deleting and updating layers regardless of how they are rendered.

	A class that wishes to implement the IStereoLayer interface can extend this class instead.
	The template argument should be a type for storing layer data. It should have a constructor matching the following:
		LayerType(const FLayerDesc& InLayerDesc);
	... and implement the following function overloads:
		void SetLayerId(uint32 InId),
		uint32 GetLayerId() const,
		bool GetLayerDescMember(LayerType& Layer, LayerType& OutLayerDesc),
		void SetLayerDescMember(LayerType& Layer, const LayerType& Desc), and
		void MarkLayerTextureForUpdate(LayerType& Layer)
	
	To perform additional bookkeeping each time individual layers are changed, you can override the following protected method:
		UpdateLayer(LayerType& Layer, uint32 LayerId, bool bIsValid)
	It is called whenever CreateLayer, DestroyLayer, SetLayerDesc and MarkTextureForUpdate are called.

	Simple implementations that do not to track additional data per layer may use FLayerDesc directly.
	The FSimpleLayerManager subclass can be used in that case and it implements all the required glue functions listed above.

	To access the layer data from your subclass, you have the following protected interface:
		bool GetStereoLayersDirty() -- Returns true if layer data have changed since the status was last cleared
		ForEachLayer(...) -- pass in a lambda to iterate through each existing layer.
		CopyLayers(TArray<LayerType>& OutArray, bool bMarkClean = true) -- Copies the layers into OutArray.
		CopySortedLayers(TArray<LayerType>& OutArray, bool bMarkClean = true) -- Copies the layers into OutArray sorted by their priority.
		WithLayer(uint32 LayerId, TFunction<void(LayerType*)> Func) -- Finds the layer by Id and passes it to Func or nullptr if not found.
	The last two methods will clear the layer dirty flag unless you pass in false as the optional final argument.
	
	Thread safety:
	All functions and state in this class should only be accessed from the game thread.

*/

template<typename LayerType>
class UE_DEPRECATED(5.6, "Use FSimpleLayerManager directly.") TStereoLayerManager : public IStereoLayers
{
private:
	bool bStereoLayersDirty;

	struct FLayerComparePriority
	{
		bool operator()(const LayerType& A, const LayerType& B) const
		{
			FLayerDesc DescA, DescB;
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			if (GetLayerDescMember(A, DescA) && GetLayerDescMember(B, DescB))
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
			{
				if (DescA.Priority < DescB.Priority)
				{
					return true;
				}
				if (DescA.Priority > DescB.Priority)
				{
					return false;
				}
				return DescA.Id < DescB.Id;
			}
			return false;
		}
	};

	struct FLayerData {
		TMap<uint32, LayerType> Layers;
		uint32 NextLayerId;
		bool bShowBackground;

		FLayerData(uint32 InNext, bool bInShowBackground = true) : Layers(), NextLayerId(InNext), bShowBackground(bInShowBackground) {}
		FLayerData(const FLayerData& In) : Layers(In.Layers), NextLayerId(In.NextLayerId), bShowBackground(In.bShowBackground) {}
	};
	TArray<FLayerData> LayerStack;

	FLayerData& LayerState(int Level=0) { return LayerStack.Last(Level); }
	//const FLayerData& LayerState(int Level = 0) const { return LayerStack.Last(Level); }
	TMap<uint32, LayerType>& StereoLayers(int Level = 0) { return LayerState(Level).Layers; }
	uint32 MakeLayerId() { return LayerState().NextLayerId++; }
	
	LayerType* FindLayerById(uint32 LayerId, int32& OutLevel)
	{
		if (LayerId == FLayerDesc::INVALID_LAYER_ID || LayerId >= LayerState().NextLayerId)
		{
			return nullptr;
		}

		// If the layer id does not exist in the current state, we'll search up the stack for the last version of the layer.
		for (int32 I = 0; I < LayerStack.Num(); I++)
		{
			LayerType* Found = StereoLayers(I).Find(LayerId);
			if (Found)
			{
				OutLevel = I;
				return Found;
			}
		}
		return nullptr;
	}

protected:

	virtual void UpdateLayer(LayerType& Layer, uint32 LayerId, bool bIsValid)
	{}

	bool GetStereoLayersDirty()
	{
		check(IsInGameThread());

		return bStereoLayersDirty;
	}


	void ForEachLayer(TFunction<void(uint32, LayerType&)> Func, bool bMarkClean = true)
	{
		check(IsInGameThread());

		for (auto& Pair : StereoLayers())
		{
			Func(Pair.Key, Pair.Value);
		}

		if (bMarkClean)
		{
			bStereoLayersDirty = false;
		}
	}

	UE_DEPRECATED(5.6, "Use ForEachLayer instead if needed")
	void CopyLayers(TArray<LayerType>& OutArray, bool bMarkClean = true)
	{
		check(IsInGameThread());

		StereoLayers().GenerateValueArray(OutArray);

		if (bMarkClean)
		{
			bStereoLayersDirty = false;
		}
	}

	UE_DEPRECATED(5.6, "Use ForEachLayer instead if needed")
	void CopySortedLayers(TArray<LayerType>& OutArray, bool bMarkClean = true)
	{
		check(IsInGameThread());

		CopyLayers(OutArray, bMarkClean);
		OutArray.Sort(FLayerComparePriority());
	}

	UE_DEPRECATED(5.6, "Use FindLayerDesc instead if needed")
	void WithLayer(uint32 LayerId, TFunction<void(LayerType*)> Func)
	{
		check(IsInGameThread());

		int32 FoundLevel;
		Func(FindLayerById(LayerId, FoundLevel));
	}

public:

	TStereoLayerManager()
		: bStereoLayersDirty(false)
		, LayerStack{ 1 }
	{
	}

	virtual ~TStereoLayerManager()
	{}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS // for deprecated fields
	TStereoLayerManager(const TStereoLayerManager&) = default;
	TStereoLayerManager(TStereoLayerManager&&) = default;
	TStereoLayerManager& operator=(const TStereoLayerManager&) = default;
	TStereoLayerManager& operator=(TStereoLayerManager&&) = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// IStereoLayers interface
	virtual uint32 CreateLayer(const FLayerDesc& InLayerDesc) override
	{
		check(IsInGameThread());

		uint32 LayerId = MakeLayerId();
		check(LayerId != FLayerDesc::INVALID_LAYER_ID);
		LayerType& NewLayer = StereoLayers().Emplace(LayerId, InLayerDesc);
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		NewLayer.SetLayerId(LayerId);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		UpdateLayer(NewLayer, LayerId, InLayerDesc.IsVisible());
		bStereoLayersDirty = true;
		return LayerId;
	}

	virtual void DestroyLayer(uint32 LayerId) override
	{
		check(IsInGameThread());

		if (LayerId == FLayerDesc::INVALID_LAYER_ID || LayerId >= LayerState().NextLayerId)
		{
			return;
		}

		int32 FoundLevel;
		LayerType* Found = FindLayerById(LayerId, FoundLevel);

		// Destroy layer will delete the last active copy of the layer even if it's currently not active
		if (Found)
		{
			if (FoundLevel == 0)
			{
				UpdateLayer(*Found, LayerId, false);
				bStereoLayersDirty = true;
			}

			StereoLayers(FoundLevel).Remove(LayerId);
		}
	}

	virtual void SetLayerDesc(uint32 LayerId, const FLayerDesc& InLayerDesc) override
	{
		check(IsInGameThread());

		if (LayerId == FLayerDesc::INVALID_LAYER_ID)
		{
			return;
		}

		// SetLayerDesc layer will update the last active copy of the layer.
		int32 FoundLevel;
		LayerType* Found = FindLayerById(LayerId, FoundLevel);
		if (Found)
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			SetLayerDescMember(*Found, InLayerDesc);
			Found->SetLayerId(LayerId);
			PRAGMA_ENABLE_DEPRECATION_WARNINGS

			// If the layer is currently active, update layer state
			if (FoundLevel == 0)
			{
				UpdateLayer(*Found, LayerId, InLayerDesc.IsVisible());
				bStereoLayersDirty = true;
			}
		}
	}

	virtual bool GetLayerDesc(uint32 LayerId, FLayerDesc& OutLayerDesc) override
	{
		check(IsInGameThread());

		if (LayerId == FLayerDesc::INVALID_LAYER_ID)
		{
			return false;
		}

		int32 FoundLevel;
		LayerType* Found = FindLayerById(LayerId, FoundLevel);
		if (Found)
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			return GetLayerDescMember(*Found, OutLayerDesc);
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
		else
		{
			return false;
		}
	}

	virtual void MarkTextureForUpdate(uint32 LayerId) override 
	{
		check(IsInGameThread());

		if (LayerId == FLayerDesc::INVALID_LAYER_ID)
		{
			return;
		}
		// If the layer id does not exist in the current state, we'll search up the stack for the last version of the layer.
		int32 FoundLevel;
		LayerType* Found = FindLayerById(LayerId, FoundLevel);
		if (Found)
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			MarkLayerTextureForUpdate(*Found);
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
			UpdateLayer(*Found, LayerId, true);
		}
	}

	virtual void PushLayerState(bool bPreserve = false) override
	{
		check(IsInGameThread());

		const FLayerData CurrentState = LayerState(); // Copy because we modify the Stack that LayerState returns an element of.

		if (bPreserve)
		{
			// If bPreserve is true, copy the entire state.
			LayerStack.Emplace(CurrentState);
			// We don't need to mark stereo layers as dirty as the new state is a copy of the existing one.
		}
		else
		{
			// Else start with an empty set of layers, but preserve NextLayerId.
			for (auto& Pair : StereoLayers())
			{
				// We need to mark the layers going out of scope as invalid, so implementations will remove them from the screen.
				UpdateLayer(Pair.Value, Pair.Key, false);
			}

			// New layers should continue using unique layer ids
			LayerStack.Emplace(CurrentState.NextLayerId, CurrentState.bShowBackground);
			bStereoLayersDirty = true;
		}
	}

	virtual void PopLayerState() override
	{
		check(IsInGameThread());

		// Ignore if there is only one element on the stack
		if (LayerStack.Num() <= 1)
		{
			return;
		}

		// First mark all layers in the current state as invalid if they did not exist previously.
		for (auto& Pair : StereoLayers(0))
		{
			if (!StereoLayers(1).Contains(Pair.Key))
			{
				UpdateLayer(Pair.Value, Pair.Key, false);
			}
		}

		// Destroy the top of the stack
		LayerStack.Pop();

		// Update the layers in the new current state to mark them as valid and restore previous state
		for (auto& Pair : StereoLayers(0))
		{
			FLayerDesc LayerDesc;
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			GetLayerDescMember(Pair.Value, LayerDesc);
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
			UpdateLayer(Pair.Value, Pair.Key, LayerDesc.IsVisible());
		}

		bStereoLayersDirty = true;
	}

	virtual bool SupportsLayerState() override { return true; }

	virtual void HideBackgroundLayer() { LayerState().bShowBackground = false; }
	virtual void ShowBackgroundLayer() { LayerState().bShowBackground = true; }
	virtual bool IsBackgroundLayerVisible() const { return LayerStack.Last().bShowBackground; }

	friend class FSimpleLayerManager;
};

PRAGMA_DISABLE_DEPRECATION_WARNINGS
class XRBASE_API FSimpleLayerManager : public TStereoLayerManager<IStereoLayers::FLayerDesc>, public FGCObject
{
protected:
	virtual void MarkTextureForUpdate(uint32 LayerId) override
	{}

public:
	// IStereoLayers interface
	virtual const FLayerDesc* FindLayerDesc(uint32 LayerId) const override
	{
		return const_cast<FSimpleLayerManager*>(this)->StereoLayers().Find(LayerId);
	}

	// Shadowed to silence deprecation warnings
	bool GetStereoLayersDirty()
	{
		return TStereoLayerManager::GetStereoLayersDirty();
	}

	// Shadowed to silence deprecation warnings
	template<typename F>
	void ForEachLayer(F&& Func, bool bMarkClean = true)
	{
		TStereoLayerManager::ForEachLayer(Forward<F&&>(Func), bMarkClean);
	}

	// Shadowed to silence deprecation warnings
	bool IsBackgroundLayerVisible() const
	{
		return TStereoLayerManager::IsBackgroundLayerVisible();
	}

	// Shadowed to silence deprecation warnings
	virtual uint32 CreateLayer(const FLayerDesc& InLayerDesc) override
	{
		return TStereoLayerManager::CreateLayer(InLayerDesc);
	}

	// Shadowed to silence deprecation warnings
	virtual void DestroyLayer(uint32 LayerId) override
	{
		TStereoLayerManager::DestroyLayer(LayerId);
	}

	// Shadowed to silence deprecation warnings
	virtual void SetLayerDesc(uint32 LayerId, const FLayerDesc& InLayerDesc) override
	{
		TStereoLayerManager::SetLayerDesc(LayerId, InLayerDesc);
	}

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		for (FLayerData& Snapshot : LayerStack)
		{
			for (auto& Pair : Snapshot.Layers)
			{
				Collector.AddReferencedObject(Pair.Value.TextureObj);
				Collector.AddReferencedObject(Pair.Value.LeftTextureObj);
			}
		}
	}
	virtual FString GetReferencerName() const override { return TEXT("FSimpleLayerManager"); }
};
PRAGMA_ENABLE_DEPRECATION_WARNINGS

UE_DEPRECATED(5.6, "Use FSimpleLayerManager directly.")
XRBASE_API bool GetLayerDescMember(const IStereoLayers::FLayerDesc& Layer, IStereoLayers::FLayerDesc& OutLayerDesc);
UE_DEPRECATED(5.6, "Use FSimpleLayerManager directly.")
XRBASE_API void SetLayerDescMember(IStereoLayers::FLayerDesc& OutLayer, const IStereoLayers::FLayerDesc& InLayerDesc);
UE_DEPRECATED(5.6, "Use FSimpleLayerManager directly.")
XRBASE_API void MarkLayerTextureForUpdate(IStereoLayers::FLayerDesc& Layer);

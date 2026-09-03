// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewportLockManager.h"

#include "VCamComponent.h"
#include "Interfaces/IViewportLocker.h"

#include "Output/VCamOutputProviderBase.h"

namespace UE::VCamCore
{
	FViewportLockManager::FViewportLockManager(IViewportLocker& ViewportLocker, FHasViewportOwnership HasViewportOwnershipDelegate)
		: ViewportLocker(ViewportLocker)
		, HasViewportOwnershipDelegate(MoveTemp(HasViewportOwnershipDelegate))
	{}

	FViewportLockManager::FViewportLockState& FViewportLockManager::GetLockState(EVCamTargetViewportID ViewportID)
	{
		static_assert(static_cast<int32>(EVCamTargetViewportID::Viewport1) == 0, "Update this location");
		return LockState[static_cast<int32>(ViewportID)];
	}

	void FViewportLockManager::UpdateViewportLockState(TConstArrayView<TWeakObjectPtr<UVCamComponent>> RegisteredVCams)
	{
		UpdateViewport(RegisteredVCams, EVCamTargetViewportID::Viewport1);
		UpdateViewport(RegisteredVCams, EVCamTargetViewportID::Viewport2);
		UpdateViewport(RegisteredVCams, EVCamTargetViewportID::Viewport3);
		UpdateViewport(RegisteredVCams, EVCamTargetViewportID::Viewport4);
	}

	bool FViewportLockManager::WantsToLockViewportTo(const UVCamOutputProviderBase& OutputProvider) const
	{
		const EVCamTargetViewportID TargetViewportID = OutputProvider.GetTargetViewport();
		const TWeakObjectPtr<AActor> ActorLock = ViewportLocker.GetActorLock(TargetViewportID);
		const TWeakObjectPtr<AActor> CinematicActorLock = ViewportLocker.GetCinematicActorLock(TargetViewportID);

		AActor* OwningActor = OutputProvider.GetTypedOuter<AActor>();
		const bool bIsExternalLockInPlace =
			ViewportLocker.IsViewportLocked(TargetViewportID)
			&& ((ActorLock != nullptr && ActorLock != OwningActor) || CinematicActorLock != nullptr);
		UVCamComponent* Component = OutputProvider.GetVCamComponent(); 
		const bool bWantsLock = HasViewportOwnershipDelegate.Execute(OutputProvider)
			&& Component && Component->GetViewportLockState().ShouldLock(TargetViewportID);
		return ensure(OwningActor) && !bIsExternalLockInPlace && bWantsLock;
	}

	void FViewportLockManager::UpdateViewport(TConstArrayView<TWeakObjectPtr<UVCamComponent>> RegisteredVCams, EVCamTargetViewportID ViewportID)
	{
		FViewportLockState& LockInfo = GetLockState(ViewportID);
		const TWeakObjectPtr<const UVCamOutputProviderBase>& WeakLockReason = LockInfo.LockReason;
		const UVCamOutputProviderBase* LockReason = WeakLockReason.Get();
		const bool bWasLockReasonInvalidated = LockReason
			&& (!HasViewportOwnershipDelegate.Execute(*LockReason)
				|| LockReason->GetTargetViewport() != ViewportID);
		if (bWasLockReasonInvalidated || WeakLockReason.IsStale())
		{
			ClearActorLock(ViewportID, LockInfo);
		}
		
		for (const TWeakObjectPtr<UVCamComponent>& WeakVCamComponent : RegisteredVCams)
		{
			UVCamComponent* VCamComponent = WeakVCamComponent.Get();
			if (!VCamComponent)
			{
				continue;
			}
			
			for (UVCamOutputProviderBase* OutputProvider : VCamComponent->GetOutputProviders())
			{
				if (!OutputProvider)
				{
					continue;
				}

				const EVCamTargetViewportID TargetViewportID = OutputProvider->GetTargetViewport();
				if (WantsToLockViewportTo(*OutputProvider))
				{
					ViewportLocker.SetActorLock(TargetViewportID, FActorLockContext{ OutputProvider });
					GetLockState(TargetViewportID).SetLockReason(*OutputProvider, *OutputProvider->GetTypedOuter<AActor>());
				}
			}
		}
	}
	
	void FViewportLockManager::ClearActorLock(EVCamTargetViewportID ViewportID, FViewportLockState& LockInfo)
	{
		const TWeakObjectPtr<AActor> WeakCurrentLock = ViewportLocker.GetActorLock(ViewportID);
		const AActor* CurrentLock = WeakCurrentLock.Get();
		const bool bHasLockedActorChanged = CurrentLock && LockInfo.OwningActor != CurrentLock;

		// If an external system has the lock, do not accidentally clear it.
		if (!bHasLockedActorChanged)
		{
			ViewportLocker.SetActorLock(ViewportID, FActorLockContext{ nullptr });
		}
		
		LockInfo.Reset();
	}
}

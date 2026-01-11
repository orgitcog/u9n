// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapePatchManager.h"

#include "CoreGlobals.h" // GIsReconstructingBlueprintInstances
#include "Engine/Level.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h" // FAutoConsoleCommand
#include "Landscape.h"
#include "LandscapeEditLayerMergeRenderContext.h"
#include "LandscapeEditTypes.h"
#include "LandscapeDataAccess.h"
#include "LandscapePatchComponent.h"
#include "LandscapePatchLogging.h"
#include "LandscapePatchUtil.h"
#include "LandscapeModule.h"
#include "LandscapeEditorServices.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "UObject/UObjectBaseUtility.h" // GetNameSafe
#include "UObject/UObjectIterator.h"
#include "Algo/AnyOf.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "LevelEditorSubsystem.h"
#include "ScopedTransaction.h"
#include "UnrealEdGlobals.h" //GUnrealEd
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LandscapePatchManager)

#define LOCTEXT_NAMESPACE "LandscapePatchManager"

namespace LandscapePatchManagerLocals
{
	const FText MigratePatchesTransactionName(LOCTEXT("MigratePatchesTransaction", "Migrate Patches"));

	static FAutoConsoleVariable CVarMigrateLegacyPatchListToPrioritySystem(
		TEXT("LandscapePatch.AutoMigrateLegacyListToPrioritySystem"),
		true,
		TEXT("When loaded in the editor, automatically remove all LandscapePatchManagers and bind their patches directly to an edit layer.  Set their patch priorities according to their index."));

#if WITH_EDITOR
	// Note: the priorities would become jumbled up if someone had multiple managers in the same edit layer. But
	// this is an unexpected case that is not worth trying to handle differently.
	FAutoConsoleCommand CCmdMigrateLegacyPatchListToPrioritySystem(
		TEXT("LandscapePatch.MigrateLegacyListToPrioritySystem"),
		TEXT("For all patch managers, make any patches in their patch list be directly bound to their edit layer, and "
			"set the patch priorities according to their index."),
		FConsoleCommandDelegate::CreateLambda([]()
	{
		const FScopedTransaction Transaction(MigratePatchesTransactionName);

		for (TObjectIterator<ALandscapePatchManager> It(
			/*AdditionalExclusionFlags = */RF_ClassDefaultObject,
			/*bIncludeDerivedClasses = */true,
			/*InInternalExclusionFlags = */EInternalObjectFlags::Garbage); It; ++It)
		{
			ALandscapePatchManager* Manager = *It;
			if (!IsValid(Manager))
			{
				continue;
			}

			UWorld* World = Manager->GetWorld();
			if (Manager->IsTemplate() || !IsValid(World) || World->WorldType != EWorldType::Editor)
			{
				continue;
			}

			if (Manager)
			{
				Manager->MigrateToPrioritySystemAndDelete();
			}
		}
	}));
#endif // WITH_EDITOR

	// Removes invalid patches from the list. This happens automatically when applying patches.
	void FilterLegacyRegisteredPatches(TArray<TSoftObjectPtr<ULandscapePatchComponent>>& PatchComponents,
		TMap<TSoftObjectPtr<ULandscapePatchComponent>, int32>& PatchToIndex, const ALandscapePatchManager* ThisPatchManager)
	{
		// Used for removing invalid brushes. We remove from the index map immediately but then remove
		// from the array and update other indices at the very end.
		bool bHaveInvalidPatches = false;
		int32 MinRemovedIndex = PatchComponents.Num();
		auto RemoveComponentFromIndexMap = [&PatchToIndex, &MinRemovedIndex](TSoftObjectPtr<ULandscapePatchComponent>& Component)
		{
			int32 RemovedIndex = -1;
			if (PatchToIndex.RemoveAndCopyValue(Component, RemovedIndex))
			{
				MinRemovedIndex = FMath::Min(MinRemovedIndex, RemovedIndex);
			}
		};

		for (TSoftObjectPtr<ULandscapePatchComponent>& Component : PatchComponents)
		{
			if (Component.IsPending())
			{
				Component.LoadSynchronous();
			}

			if (Component.IsNull())
			{
				// Theoretically when components are marked for destruction, they should remove themselves from
				// the patch manager in their OnComponentDestroyed call. However there seem to be ways to end up
				// with destroyed patches not being removed, for instance through saving the manager but not the
				// patch actor.
				UE_LOG(LogLandscapePatch, Warning, TEXT("ALandscapePatchManager: Found an invalid patch in patch manager. It will be removed."));
				RemoveComponentFromIndexMap(Component);
				bHaveInvalidPatches = true;
				continue;
			}

			if (!Component.IsValid())
			{
				// This means that IsPending() was true, but LoadSynchronous() failed, which we generally don't
				// expect to happen. However, it can happen in some edge cases such as if you force delete a patch
				// holder blueprint and don't save the patch manager afterward. Whatever the reason, this is likely
				// a dead patch that actually needs removal.
				UE_LOG(LogLandscapePatch, Warning, TEXT("ALandscapePatchManager: Found a pending patch pointer in patch manager that "
					"turned out to be invalid. It will be removed."));
				RemoveComponentFromIndexMap(Component);
				bHaveInvalidPatches = true;
				continue;
			}

			// Make sure the patch has this manager set as its patch manager.
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
				if (Component->GetPatchManager() != ThisPatchManager)
					PRAGMA_ENABLE_DEPRECATION_WARNINGS
				{
						UE_LOG(LogLandscapePatch, Warning, TEXT("ALandscapePatchManager: Found a patch whose patch manager is not set "
								"to a patch manager that contains it. It will be removed."));
						RemoveComponentFromIndexMap(Component);
						bHaveInvalidPatches = true;
					continue;
				}

					if (!Component->IsPatchInWorld())
					{
						UE_LOG(LogLandscapePatch, Warning, TEXT("ALandscapePatchManager: Found a non-world patch in patch manager. It will be removed."));
						RemoveComponentFromIndexMap(Component);
						bHaveInvalidPatches = true;
						continue;
					}
		}

		if (bHaveInvalidPatches)
		{
			PatchComponents.RemoveAll([ThisPatchManager](TSoftObjectPtr<ULandscapePatchComponent> Component) {
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
					return !Component.IsValid() || Component->GetPatchManager() != ThisPatchManager || !Component->IsPatchInWorld();
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
			});
			// Update forward indices
			for (int32 i = MinRemovedIndex; i < PatchComponents.Num(); ++i)
			{
				PatchToIndex.Add(PatchComponents[i], i);
			}
		}
	}

	/**
	 * Returns true if any of the patches in the list pass the predicate.
	 */
	bool AnyOfPatchComponents(TArrayView<const TSoftObjectPtr<ULandscapePatchComponent>> LegacyRegisteredPatches,
		TFunctionRef<bool(ULandscapePatchComponent&)> InPredicate)
	{
		return Algo::AnyOf(LegacyRegisteredPatches, [InPredicate](const TSoftObjectPtr<ULandscapePatchComponent>& InComponent)
		{
			if (InComponent.IsPending())
			{
				InComponent.LoadSynchronous();
			}

			if (InComponent.IsValid() && InPredicate(*InComponent.Get()))
			{
				return true;
			}
			return false;
		});
	}

#if WITH_EDITOR
	// Try to get a soft pointer to the owner actor for a ULandscapePatchComponent when that actor and component have not loaded.
	TSoftObjectPtr<UObject> GetActorPtrFromPatchComponent(TSoftObjectPtr<ULandscapePatchComponent> Patch)
	{
		const FSoftObjectPath& PatchPath = Patch.ToSoftObjectPath();
		if (PatchPath.IsSubobject())
		{
			FTopLevelAssetPath AssetPath = PatchPath.GetAssetPath();
			FUtf8String SubPath = PatchPath.GetSubPathUtf8String();

			// Remove the last part of the subpath, which should be the component name ".LandscapeTexturePatch", etc.
			int32 LastPathSeparator = INDEX_NONE;
			if (SubPath.FindLastChar(UTF8TEXT('.'), LastPathSeparator))
			{
				check(LastPathSeparator != INDEX_NONE);
				SubPath.LeftInline(LastPathSeparator, EAllowShrinking::No);

				FSoftObjectPath ActorPath = FSoftObjectPath::ConstructFromAssetPathAndSubpath(AssetPath, MoveTemp(SubPath));
				TSoftObjectPtr<UObject> ActorPtr(MoveTemp(ActorPath));

				return ActorPtr;
			}
		}
		return {};
	}
#endif //WITH_EDITOR
}

// TODO: Not sure if using this kind of constructor is a proper thing to do vs some other hook...
ALandscapePatchManager::ALandscapePatchManager(const FObjectInitializer& ObjectInitializer)
	: ALandscapeBlueprintBrushBase(ObjectInitializer)
{
#if WITH_EDITOR
	SetCanAffectHeightmap(true);
	SetCanAffectWeightmap(true);
	SetCanAffectVisibilityLayer(true);
#endif
}

void ALandscapePatchManager::Initialize_Native(const FTransform& InLandscapeTransform,
	const FIntPoint& InLandscapeSize,
	const FIntPoint& InLandscapeRenderTargetSize)
{
	HeightmapCoordsToWorld = UE::Landscape::PatchUtil::GetHeightmapToWorld(InLandscapeTransform);
}

// Called in global merge to apply the patches
UTextureRenderTarget2D* ALandscapePatchManager::RenderLayer_Native(const FLandscapeBrushParameters& InParameters)
{
	using namespace LandscapePatchManagerLocals;

	// Note: We do not expect RenderLayer_Native to be called in the batched merge case.
	// TODO: Check the cvar to make sure that is not the case?

	FilterLegacyRegisteredPatches(PatchComponents, PatchToIndex, this);
	FLandscapeBrushParameters BrushParameters = InParameters;
	for (TSoftObjectPtr<ULandscapePatchComponent>& Component : PatchComponents)
	{
		if (!Component->IsEnabled())
		{
			// Skip disabled patches
			continue;
		}

		BrushParameters.CombinedResult = Component->RenderLayer_Native(BrushParameters, GetHeightmapCoordsToWorld());
	}

	return BrushParameters.CombinedResult;
}

#if WITH_EDITOR
// Called in batched merge path to apply the patches
TArray<UE::Landscape::EditLayers::FEditLayerRendererState> ALandscapePatchManager::GetEditLayerRendererStates(const UE::Landscape::EditLayers::FMergeContext* InMergeContext)
{
	using namespace UE::Landscape::EditLayers;
	using namespace LandscapePatchManagerLocals;

	FilterLegacyRegisteredPatches(PatchComponents, PatchToIndex, this);

	// Inherit the base brush's enabled state and intersect it with all patches' state, so that the heightmap/weightmap/visibility toggle at the brush's level applies to them too : 
	FEditLayerTargetTypeState BaseBrushSupportedState(InMergeContext);
	FEditLayerTargetTypeState BaseBrushEnabledState(InMergeContext);
	TArray<TBitArray<>> BaseTargetLayerGroups;
	Super::GetRendererStateInfo(InMergeContext, BaseBrushSupportedState, BaseBrushEnabledState, BaseTargetLayerGroups);

	TArray<FEditLayerRendererState> RendererStates;
	RendererStates.Reserve(PatchComponents.Num());
	for (TSoftObjectPtr<ULandscapePatchComponent>& PatchSoft : PatchComponents)
	{
		ULandscapePatchComponent* Patch = PatchSoft.Get();
		if (!Patch)
		{
			continue;
		}

		FEditLayerRendererState& RendererState = RendererStates.Emplace_GetRef(InMergeContext, Patch);
		// Disable all target types that are disabled in base brush : 
		RendererState.DisableTargetTypeMask(~BaseBrushEnabledState.GetTargetTypeMask());
		if (InMergeContext->ShouldSkipProceduralRenderers() || !Patch->IsEnabled())
		{
			RendererState.DisableTargetTypeMask(ELandscapeToolTargetTypeFlags::All);
		}
	}

	return RendererStates;
}

#endif // WITH_EDITOR

void ALandscapePatchManager::SetTargetLandscape(ALandscape* InTargetLandscape)
{
#if WITH_EDITOR
	if (OwningLandscape != InTargetLandscape && !bDead)
	{
		if (OwningLandscape)
		{
			OwningLandscape->RemoveBrush(this);
		}

		if (!InTargetLandscape)
		{
			if (OwningLandscape != nullptr)
			{
				// This can occur if the RemoveBrush call above did not do anything because the manager
				// was removed from the landscape in some other way (probably in landscape mode panel)
				SetOwningLandscape(nullptr);
			}
			return;
		}

		if (!InTargetLandscape->CanHaveLayersContent())
		{
			UE_LOG(LogLandscapePatch, Warning, TEXT("Landscape target for patch manager did not have edit layers enabled. Unable to attach manager."));
			SetOwningLandscape(nullptr);
		}
	}
#endif
}

bool ALandscapePatchManager::ContainsPatch(ULandscapePatchComponent* Patch) const
{
	return PatchToIndex.Contains(TSoftObjectPtr<ULandscapePatchComponent>(Patch));
}

void ALandscapePatchManager::AddPatch(ULandscapePatchComponent* Patch)
{
	using namespace LandscapePatchManagerLocals;

	if (Patch && Patch->IsPatchInWorld() && !IsDead())
	{
		if (!ContainsPatch(Patch))
		{
			Modify();
			TSoftObjectPtr<ULandscapePatchComponent> PatchSoftPtr(Patch);
			PatchComponents.Add(PatchSoftPtr);
			PatchToIndex.Add(PatchSoftPtr, PatchComponents.Num() - 1);
		}

		PRAGMA_DISABLE_DEPRECATION_WARNINGS
			if (Patch->GetPatchManager() != this)
			{
				UE_LOG(LogLandscapePatch, Warning, TEXT("ALandscapePatchManager::AddPatch: Added patch does not have this manager set "
					"as its manager. Patches are typically added to managers by setting the manager on the patch. "
					"(Package: % s, Actor : % s)"), *GetNameSafe(Patch->GetPackage()), *GetNameSafe(Patch->GetAttachmentRootActor()));
			}
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

			// No need to update if the patch is disabled. Important to avoid needlessly updating while dragging a blueprint with
			// a disabled patch (since construction scripts constantly add and remove).
			if (Patch->IsEnabled())
			{
				RequestLandscapeUpdate(!UE::GetIsEditorLoadingPackage());
			}
	}
	else if (Patch && IsDead())
	{
		// IsDead means the migration code ran.  A patch showing up here means it wasn't successfully migrated, possibly from failing to load.
		UE_LOG(LogLandscapePatch, Error, TEXT("Landscape patch is not correctly connected to a LandscapePatchEditLayer.  Fix this by first clicking Build->Build Landscape and then running LandscapePatch.FixPatchBindings. "
			"(Package: %s, Actor : %s)"), *GetNameSafe(Patch->GetPackage()), *GetNameSafe(Patch->GetAttachmentRootActor()));
	}
}

bool ALandscapePatchManager::RemovePatch(ULandscapePatchComponent* Patch)
{
	bool bRemoved = false;

	if (Patch && ContainsPatch(Patch))
	{
		Modify();
		TSoftObjectPtr<ULandscapePatchComponent> PatchSoftPtr(Patch);
		bRemoved = PatchComponents.Remove(PatchSoftPtr) > 0;
		if (bRemoved)
		{
			int32 RemovedIndex = -1;
			if (PatchToIndex.RemoveAndCopyValue(PatchSoftPtr, RemovedIndex))
			{
				for (int32 i = RemovedIndex; i < PatchComponents.Num(); ++i)
				{
					PatchToIndex.Add(PatchComponents[i], i);
				}
			}

		}

		// No need to update if the patch was already disabled.Important to avoid needlessly updating while dragging 
		// a blueprint with a disabled patch (since construction scripts constantly add and remove).
		if (bRemoved && Patch->IsEnabled())
		{
			RequestLandscapeUpdate(!UE::GetIsEditorLoadingPackage());
		}
	}

	return bRemoved;
}

int32 ALandscapePatchManager::GetIndexOfPatch(const ULandscapePatchComponent* Patch) const
{
	if (const int32* Index = PatchToIndex.Find(TSoftObjectPtr<ULandscapePatchComponent>(const_cast<ULandscapePatchComponent*>(Patch))))
	{
		return *Index;
	}
	return INDEX_NONE;
}

void ALandscapePatchManager::MovePatchToIndex(ULandscapePatchComponent* Patch, int32 Index)
{
	if (!Patch || !Patch->IsPatchInWorld() || Index < 0)
	{
		return;
	}

	int32 OriginalIndex = GetIndexOfPatch(Patch);
	if (OriginalIndex == Index)
	{
		return;
	}

	Modify();

	// It might seem like the index needs adjusting if we're removing before the given index, but that
	// is not the case if our goal is for the index of the patch to be the given index at the end (rather
	// than our goal being that the patch be in a particular position relative to the existing patches).
	RemovePatch(Patch);

	Index = FMath::Clamp(Index, 0, PatchComponents.Num());
	PatchComponents.Insert(TSoftObjectPtr<ULandscapePatchComponent>(Patch), Index);

	// Update our index lookup structure
	int32 OtherEndOfChangedIndices = OriginalIndex < 0 ? PatchComponents.Num() - 1
		: FMath::Min(OriginalIndex, PatchComponents.Num() - 1);

	int32 StartIndex = FMath::Min(Index, OtherEndOfChangedIndices);
	int32 EndIndex = FMath::Max(Index, OtherEndOfChangedIndices);

	for (int32 i = StartIndex; i <= EndIndex; ++i)
	{
		PatchToIndex.Add(PatchComponents[i], i);
	}

	if (Patch->IsEnabled())
	{
		RequestLandscapeUpdate();
	}
}

#if WITH_EDITOR

void ALandscapePatchManager::MigrateToPrioritySystemAndDelete()
{
	MigrateToPrioritySystemAndDeleteInternal(/*bAllowUI=*/true);
}

void ALandscapePatchManager::MigrateToPrioritySystemAndDeleteInternal(bool bAllowUI)
{
	check(GetWorld()->WorldType == EWorldType::Editor);
	
	// Create LandscapeInfo if needed.  During load time, it might not exist yet, depending on load order.
	ULandscapeInfo* LandscapeInfo = OwningLandscape ? OwningLandscape->CreateLandscapeInfo(true) : nullptr;
	if (OwningLandscape && LandscapeInfo && !bDead && IsValid(this) && !PatchComponents.IsEmpty())
	{
		Modify();
		LandscapeInfo->MarkObjectDirty(OwningLandscape, /*bInForceResave = */true);

		// Patches will remove themselves from PatchComponents as we go along, so we need to iterate
		// a copy.
		TArray<TSoftObjectPtr<ULandscapePatchComponent>> PatchListCopy;

		// We call Modify on all the patches we'll be touching at the start, otherwise they will
		// store incorrect indices for undo as they are removed.
		int32 PatchErrors = 0;
		for (TSoftObjectPtr<ULandscapePatchComponent> Patch : PatchComponents)
		{
			if (Patch.IsPending())
			{
				Patch.LoadSynchronous();
			}

			if (!Patch.IsValid())
			{
				// Loading directly from a component TSoftObjectPtr is unreliable, so try deriving the owner actor pointer using
				// the path and load via that.  If the setup is weird and this doesn't work, it should at least still be safe. 
				TSoftObjectPtr<UObject> ActorPtr = LandscapePatchManagerLocals::GetActorPtrFromPatchComponent(Patch);
				if (ActorPtr.IsPending())
				{
					ActorPtr.LoadSynchronous();  // If this succeeds, Patch will turn valid.
				}
			}

			if (Patch.IsValid())
			{
				LandscapeInfo->MarkObjectDirty(Patch.Get(), /*bInForceResave = */true, OwningLandscape);
				Patch->Modify();
				PatchListCopy.Add(Patch);
			}
			else
			{
				// Failed to load?  This can happen if patch bindings were broken in the pre-migration scene.
				// LandscapePatch.FixPatchBindings after deleting the patch manager can fix them.
				++PatchErrors;
			}
		}

		if (!bAllowUI)
		{
			// Create the ULandscapePatchEditLayer in advance, if needed.  This prevents BindToLandscape from triggering the
			// modal UI window for choosing the new layer index.
			const ULandscapeEditLayerBase* Layer = Cast<ULandscapePatchEditLayer>(OwningLandscape->FindEditLayerOfTypeConst(ULandscapePatchEditLayer::StaticClass()));

			if (!Layer)
			{
				const FName PatchLayerName = OwningLandscape->GenerateUniqueLayerName(*ULandscapePatchEditLayer::StaticClass()->GetDefaultObject<ULandscapePatchEditLayer>()->GetDefaultName());
				// Ignore the layer count limit to avoid failing here.  It's only a soft limit to aid performance.  The old non-ULandscapePatchEditLayer is probably unused after this, but
				// it could have also been used for manual painting and we can't safely delete it.
				int32 LayerIdx = OwningLandscape->CreateLayer(PatchLayerName, ULandscapePatchEditLayer::StaticClass(), /*bIgnoreLayerCountLimit=*/ true);
				check(LayerIdx != INDEX_NONE);

				// Move the new layer beside the old-style "LandscapePatches" layer
				int32 OldPathLayerIdx = OwningLandscape->GetBrushLayer(this);
				if (ensure(OldPathLayerIdx != INDEX_NONE))
				{
					OwningLandscape->ReorderLayer(LayerIdx, OldPathLayerIdx);
				}
			}
		}

		double Priority = LEGACY_PATCH_PRIORITY_BASE;
		double PriorityStep = 1.0 / FMath::Max(1, PatchComponents.Num());

		for (TSoftObjectPtr<ULandscapePatchComponent> Patch : PatchListCopy)
		{
			Patch->SetPriority(Priority);
			Priority += PriorityStep;

			PRAGMA_DISABLE_DEPRECATION_WARNINGS
				Patch->SetPatchManager(nullptr);
			PRAGMA_ENABLE_DEPRECATION_WARNINGS

			Patch->FixBindings();
		}

		UE_LOG(LogLandscapePatch, Warning, TEXT("ALandscapePatchManager: %d landscape patches have been migrated from the legacy patch manager \"%s\" to be bound directly to a ULandscapePatchEditLayer"),
			PatchListCopy.Num(), *GetActorLabel(false));
		if (PatchErrors > 0)
		{
			UE_LOG(LogLandscapePatch, Error, TEXT("ALandscapePatchManager: %d landscape patches failed to migrate successfully.  They may be restorable by running the command LandscapePatch.FixPatchBindings"), PatchErrors);
		}

		PatchComponents.Empty();
		RequestLandscapeUpdate();
	}

	// Important so that we remove ourselves from the landscape blueprint brush list
	SetTargetLandscape(nullptr);

	// bDead is used as protection from unexpected weirdness if anything happens in the window between this code running and the
	// actor being actually removed from the world (by the actionable message update button).
	bDead = true;

	if (LandscapeInfo)
	{
		// We can't delete an actor during load time, so enqueue this to be deleted later.  We want this deletion to be applied from
		// MarkModifiedLandscapesAsDirty, the same place that the deferred dirty state from MarkObjectDirty is finally applied.  This
		// will leave the scene in a consistent state before and after the user clicks the "Update" button from the landscape check
		// code.  If the scene is closed without clicking update, it will remain fully un-migrated on disk.
		LandscapeInfo->DeleteActorWhenApplyingModifiedStatus(this, bAllowUI);
	}
	else 
	{
		// No LandscapeInfo, likely because no OwningLandscape.  Try to delete directly.
		UE::Landscape::DeleteActors({ this }, GetWorld(), bAllowUI);
	}
}


bool ALandscapePatchManager::AffectsHeightmap() const
{
	using namespace LandscapePatchManagerLocals;

	if (!Super::AffectsHeightmap())
	{
		return false;
	}

	return AnyOfPatchComponents(PatchComponents,
		[](ULandscapePatchComponent& InComponent) { return InComponent.AffectsHeightmap(); });
}

bool ALandscapePatchManager::AffectsWeightmap() const
{
	using namespace LandscapePatchManagerLocals;

	if (!Super::AffectsWeightmap())
	{
		return false;
	}

	return AnyOfPatchComponents(PatchComponents,
		[](ULandscapePatchComponent& InComponent) { return InComponent.AffectsWeightmap(); });

}

bool ALandscapePatchManager::AffectsWeightmapLayer(const FName& InLayerName) const
{
	using namespace LandscapePatchManagerLocals;

	if (!Super::AffectsWeightmap()) // Don't call Super::AffectsWeightmapLayer(InLayerName) here as we don't want to use the AffectedWeightmapLayers list for weightmap layers
	{
		return false;
	}

	return AnyOfPatchComponents(PatchComponents,
		[InLayerName](ULandscapePatchComponent& InComponent) { return InComponent.AffectsWeightmapLayer(InLayerName); });
}

bool ALandscapePatchManager::AffectsVisibilityLayer() const
{
	using namespace LandscapePatchManagerLocals;

	if (!Super::AffectsVisibilityLayer())
	{
		return false;
	}

	return AnyOfPatchComponents(PatchComponents,
		[](ULandscapePatchComponent& InComponent) { return InComponent.AffectsVisibilityLayer(); });
}

bool ALandscapePatchManager::CanAffectWeightmapLayer(const FName& InLayerName) const
{
	using namespace LandscapePatchManagerLocals;

	if (!Super::CanAffectWeightmap()) // Don't call Super::CanAffectWeightmapLayer(InLayerName) here as we don't want to use the AffectedWeightmapLayers list for weightmap layers
	{
		return false;
	}

	return AnyOfPatchComponents(PatchComponents,
		[InLayerName](ULandscapePatchComponent& InComponent) { return InComponent.CanAffectWeightmapLayer(InLayerName); });
}

void ALandscapePatchManager::GetRenderDependencies(TSet<UObject*>& OutDependencies)
{
	for (TSoftObjectPtr<ULandscapePatchComponent>& Component : PatchComponents)
	{
		if (Component.IsPending())
		{
			Component.LoadSynchronous();
		}

		if (Component.IsValid())
		{
			Component->GetRenderDependencies(OutDependencies);
		}
	}
}

void ALandscapePatchManager::PostEditUndo()
{
	RequestLandscapeUpdate();
}

void ALandscapePatchManager::SetOwningLandscape(ALandscape* InOwningLandscape)
{
	Super::SetOwningLandscape(InOwningLandscape);

	DetailPanelLandscape = OwningLandscape;
}

// We override PostEditChange to allow the users to change the owning landscape via a property displayed in the detail panel.
void ALandscapePatchManager::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Do a bunch of checks to make sure that we don't try to do anything when the editing is happening inside the blueprint editor.
	UWorld* World = GetWorld();
	if (IsTemplate() || !IsValidChecked(this) || !IsValid(World) || World->WorldType != EWorldType::Editor)
	{
		return;
	}

	if (PropertyChangedEvent.Property
		&& (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ALandscapePatchManager, DetailPanelLandscape)))
	{
		SetTargetLandscape(DetailPanelLandscape.Get());
	}
}

void ALandscapePatchManager::PostLoad()
{
	Super::PostLoad();

	PatchToIndex.Reset();
	for (int32 i = 0; i < PatchComponents.Num(); ++i)
	{
		PatchToIndex.Add(PatchComponents[i], i);
	}
}

void ALandscapePatchManager::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	if (!IsTemplate() && IsValid(GetWorld()) && GetWorld()->WorldType == EWorldType::Editor &&
		LandscapePatchManagerLocals::CVarMigrateLegacyPatchListToPrioritySystem->GetBool())
	{
		MigrateToPrioritySystemAndDeleteInternal(/*bAllowUI=*/false);
	}
}

void ALandscapePatchManager::CheckForErrors()
{
	using namespace LandscapePatchManagerLocals;

	Super::CheckForErrors();

	auto GetPackageAndActorArgs = [this]()
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Package"), FText::FromString(*GetNameSafe(GetPackage())));
		Arguments.Add(TEXT("Actor"), FText::FromString(*GetNameSafe(this)));
		return Arguments;
	};

	// See if we're holding on to any patches that don't have us as the owning patch manager
	bool bHavePatchWithIncorrectManager = false;
	for (TSoftObjectPtr<ULandscapePatchComponent>& Component : PatchComponents)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		if (Component.IsValid() && Component->GetPatchManager() != this)
		{
			bHavePatchWithIncorrectManager = true;
			break;
		}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	if (bHavePatchWithIncorrectManager)
	{
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT("PatchesHaveIncorrectManagerPointer", "Patch manager holds at "
				"least one patch whose patch manager pointer is set incorrectly. These patches should be removed from the manager."
				"(Package: {Package}, Manager: {Actor})."), GetPackageAndActorArgs())))
			->AddToken(FActionToken::Create(LOCTEXT("FixPatchesButton", "Fix patches"), FText(),
				FOnActionTokenExecuted::CreateWeakLambda(this, [this]()
				{
					// Hard to say whether this should be in a transaction, or even be an action, because this happens
					// automatically on the next landscape update... We'll stick with having it be user triggerable but
					// not undoable.

					PatchComponents.RemoveAll([this](TSoftObjectPtr<ULandscapePatchComponent> Component) 
					{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
						return Component.IsValid() && Component->GetPatchManager() != this;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
					});
				})));
	}

	if (PatchComponents.Num() > 0)
	{
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT("UsingLegacyPatchList", "The use of the patch manager to "
				"determine patch ordering is deprecated. Patches should point to a specific edit layer via a guid and "
				"use Priority for ordering. You can use LandscapePatch.MigrateLegacyListToPrioritySystem to "
				"fix this. (Package: {Package}, Manager: {Actor})."), GetPackageAndActorArgs())))
			->AddToken(FActionToken::Create(LOCTEXT("MigrateToGuidsButton", "Migrate to guid system"), FText(),
				FOnActionTokenExecuted::CreateWeakLambda(this, [this]()
		{
			const FScopedTransaction Transaction(MigratePatchesTransactionName);
			MigrateToPrioritySystemAndDelete();
		})));
	}
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE

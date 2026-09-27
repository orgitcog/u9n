// Copyright Epic Games, Inc. All Rights Reserved.

#include "Renderers/Text3DRendererBase.h"

#include "Extensions/Text3DGeometryExtensionBase.h"
#include "Extensions/Text3DCharacterExtensionBase.h"
#include "Extensions/Text3DLayoutEffectBase.h"
#include "Extensions/Text3DLayoutExtensionBase.h"
#include "Extensions/Text3DMaterialExtensionBase.h"
#include "Extensions/Text3DRenderingExtensionBase.h"
#include "GameFramework/Actor.h"
#include "Logs/Text3DLogs.h"
#include "Text3DComponent.h"

UText3DComponent* UText3DRendererBase::GetText3DComponent() const
{
	return GetTypedOuter<UText3DComponent>();
}

void UText3DRendererBase::RefreshBounds()
{
	CachedBounds = OnCalculateBounds();
}

void UText3DRendererBase::Create()
{
	if (bInitialized)
	{
		return;
	}

	const UText3DComponent* Text3DComponent = GetText3DComponent();
	if (!IsValid(Text3DComponent))
	{
		return;
	}

	OnCreate();
	bInitialized = true;
	UE_LOG(LogText3D, Log, TEXT("%s : Text3DRenderer %s Created"), *Text3DComponent->GetOwner()->GetActorNameOrLabel(), *GetName().ToString())
}

void UText3DRendererBase::Update(EText3DRendererFlags InFlags)
{
	if (!bInitialized)
	{
		return;
	}

	const UText3DComponent* Text3DComponent = GetText3DComponent();
	if (!IsValid(Text3DComponent))
	{
		return;
	}

	TArray<UText3DExtensionBase*, TInlineAllocator<6>> Extensions
	{
		Text3DComponent->GetCharacterExtension(),
		Text3DComponent->GetGeometryExtension(),
		Text3DComponent->GetLayoutExtension(),
		Text3DComponent->GetMaterialExtension(),
		Text3DComponent->GetRenderingExtension()
	};

	Extensions.Append(Text3DComponent->GetLayoutEffects());

	Extensions.RemoveAll([](const UText3DExtensionBase* InExtension)
	{
		return !IsValid(InExtension);
	});

	// Sort in reverse order since we iterate in reverse order
	Extensions.StableSort([](const UText3DExtensionBase& InExtensionA, const UText3DExtensionBase& InExtensionB)->bool
	{
		return InExtensionA.GetUpdatePriority() > InExtensionB.GetUpdatePriority();
	});

	uint8 Flag = 1 << 0;

	while (Flag < static_cast<uint8>(EText3DRendererFlags::All))
	{
		const EText3DRendererFlags FlagCasted = static_cast<EText3DRendererFlags>(Flag);

		if (EnumHasAnyFlags(InFlags, FlagCasted))
		{
			for (int32 Index = Extensions.Num() - 1; Index >= 0; --Index)
			{
				if (UText3DExtensionBase* Extension = Extensions[Index])
				{
					const EText3DExtensionResult ExtensionStatus = Extension->PreRendererUpdate(FlagCasted);

					if (ExtensionStatus == EText3DExtensionResult::Failed)
					{
						UE_LOG(LogText3D, Error, TEXT("Failed to PRE update Text3D %s extension"), *GetNameSafe(Extension->GetClass()))
						return;
					}
					else if (ExtensionStatus == EText3DExtensionResult::Finished)
					{
						Extensions.RemoveAt(Index);
					}
				}
			}

			OnUpdate(FlagCasted);

			for (int32 Index = Extensions.Num() - 1; Index >= 0; --Index)
			{
				if (UText3DExtensionBase* Extension = Extensions[Index])
				{
					const EText3DExtensionResult ExtensionStatus = Extension->PostRendererUpdate(FlagCasted);

					if (ExtensionStatus == EText3DExtensionResult::Failed)
					{
						UE_LOG(LogText3D, Error, TEXT("Failed to POST update Text3D %s extension"), *GetNameSafe(Extension->GetClass()))
						return;
					}
					else if (ExtensionStatus == EText3DExtensionResult::Finished)
					{
						Extensions.RemoveAt(Index);
					}
				}
			}
		}

		Flag <<= 1;
	}

	UE_LOG(LogText3D, Verbose, TEXT("%s : Text3DRenderer %s Updated with flags %i"), *Text3DComponent->GetOwner()->GetActorNameOrLabel(), *GetName().ToString(), InFlags)
}

void UText3DRendererBase::Clear()
{
	if (!bInitialized)
	{
		return;
	}

	const UText3DComponent* Text3DComponent = GetText3DComponent();
	if (!Text3DComponent)
	{
		return;
	}

	OnClear();
	CachedBounds.Reset();

	const AActor* Owner = Text3DComponent->GetOwner();
	UE_LOG(LogText3D, Verbose, TEXT("%s : Text3DRenderer %s Cleared"), !!Owner ? *Owner->GetActorNameOrLabel() : TEXT("Invalid owner"), *GetName().ToString())
}

void UText3DRendererBase::Destroy()
{
	if (!bInitialized)
	{
		return;
	}

	const UText3DComponent* Text3DComponent = GetText3DComponent();
	if (!Text3DComponent)
	{
		return;
	}

	OnDestroy();
	CachedBounds.Reset();
	bInitialized = false;

	const AActor* Owner = Text3DComponent->GetOwner();
	UE_LOG(LogText3D, Log, TEXT("%s : Text3DRenderer %s Destroyed"), !!Owner ? *Owner->GetActorNameOrLabel() : TEXT("Invalid owner"), *GetName().ToString())
}

FBox UText3DRendererBase::GetBounds() const
{
	if (!CachedBounds.IsSet())
	{
		return FBox(ForceInitToZero);
	}

	return CachedBounds.GetValue();
}

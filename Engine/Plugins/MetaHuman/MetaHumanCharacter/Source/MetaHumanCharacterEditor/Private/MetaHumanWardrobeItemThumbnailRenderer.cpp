// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanWardrobeItemThumbnailRenderer.h"
#include "MetaHumanWardrobeItem.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "AssetDefinitionDefault.h"
#include "ObjectTools.h"

bool UMetaHumanWardrobeItemThumbnailRenderer::CanVisualizeAsset(UObject* InObject)
{
	if (UMetaHumanWardrobeItem* WardrobeItem = Cast<UMetaHumanWardrobeItem>(InObject))
	{
		if (UObject* PrincipalAsset = WardrobeItem->PrincipalAsset.Get())
		{
			// Using WI for principal asset can create cyclic dependencies and cause infinite recursion,
			// so it's not allowed.
			if (PrincipalAsset->IsA<UMetaHumanWardrobeItem>())
			{
				return false;
			}

			if (FThumbnailRenderingInfo* Info = UThumbnailManager::Get().GetRenderingInfo(PrincipalAsset))
			{
				if (UThumbnailRenderer* ThumbnailRenderer = Info->Renderer)
				{
					return ThumbnailRenderer->CanVisualizeAsset(PrincipalAsset);
				}
			}
		}
	}
	else
	{
		ThumbnailTools::CacheEmptyThumbnail(InObject->GetFullName(), InObject->GetPackage());
	}

	return false;
}

void UMetaHumanWardrobeItemThumbnailRenderer::Draw(UObject* InObject, int32 InX, int32 InY, uint32 InWidth, uint32 InHeight, FRenderTarget* InRenderTarget, FCanvas* InCanvas, bool bInAdditionalViewFamily)
{
	if (UMetaHumanWardrobeItem* WardrobeItem = Cast<UMetaHumanWardrobeItem>(InObject))
	{
		 // WardrobeItem->PrincipalAsset.LoadSynchronous();
		if (UObject* PrincipalAsset = WardrobeItem->PrincipalAsset.Get())
		{
			if (FThumbnailRenderingInfo* Info = UThumbnailManager::Get().GetRenderingInfo(PrincipalAsset))
			{
				if (UThumbnailRenderer* ThumbnailRenderer = Info->Renderer)
				{
					using namespace UE::Editor;
					if (ThumbnailRenderer->CanVisualizeAsset(PrincipalAsset))
					{
						if (USceneThumbnailInfo* OriginalThumbnailInfo = Cast<USceneThumbnailInfo>(FindOrCreateThumbnailInfo(PrincipalAsset, USceneThumbnailInfo::StaticClass())))
						{
							if (USceneThumbnailInfo* WardrobeItemSceneThumbnailInfo = Cast<USceneThumbnailInfo>(FindOrCreateThumbnailInfo(WardrobeItem, USceneThumbnailInfo::StaticClass())))
							{
								UE::Editor::TrySetExistingThumbnailInfo(PrincipalAsset, WardrobeItemSceneThumbnailInfo);
								ThumbnailRenderer->Draw(PrincipalAsset, InX, InY, InWidth, InHeight, InRenderTarget, InCanvas, bInAdditionalViewFamily);
								UE::Editor::TrySetExistingThumbnailInfo(PrincipalAsset, OriginalThumbnailInfo);
							}
						}
						else
						{
							ThumbnailRenderer->Draw(PrincipalAsset, InX, InY, InWidth, InHeight, InRenderTarget, InCanvas, bInAdditionalViewFamily);
						}
					}
				}
			}
		}
	}
}

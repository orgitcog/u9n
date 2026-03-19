// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimDetailsProxyManagerDetails.h"

#include "AnimDetails/AnimDetailsProxyManager.h"
#include "AnimDetails/Proxies/AnimDetailsProxyBase.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "ISequencer.h"

namespace UE::ControlRigEditor
{
	TSharedRef<IDetailCustomization> FAnimDetailProxyManagerDetails::MakeInstance()
	{
		return MakeShared<FAnimDetailProxyManagerDetails>();
	}

	void FAnimDetailProxyManagerDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
	{
		TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
		DetailLayout.GetObjectsBeingCustomized(ObjectsBeingCustomized);

		for (const TWeakObjectPtr<UObject>& ObjectBeingCustomized : ObjectsBeingCustomized)
		{
			UAnimDetailsProxyManager* ProxyManager = Cast<UAnimDetailsProxyManager>(ObjectBeingCustomized.Get());
			if (!ProxyManager)
			{
				continue;
			}

			FAnimDetailsFilter& Filter = ProxyManager->GetAnimDetailsFilter();
			const TArray<UAnimDetailsProxyBase*> FilteredProxies = Filter.GetFilteredProxies();

			TMap<FName, TArray<UObject*>> DetailRowIDToGroupedProxiesMap;
			TMap<FName, TArray<UObject*>> DetailRowIDToIndividualProxiesMap;
			for (UAnimDetailsProxyBase* Proxy : FilteredProxies)
			{
				if (!Proxy)
				{
					continue;
				}

				const FName DetailsRowID = Proxy->GetDetailRowID();
				if (Proxy->bIsIndividual)
				{
					DetailRowIDToIndividualProxiesMap.FindOrAdd(DetailsRowID).Add(Proxy);
				}
				else
				{
					DetailRowIDToGroupedProxiesMap.FindOrAdd(DetailsRowID).Add(Proxy);
				}
			}

			// Mock up the details view as per anim details specifics, whereas grouped proxies show first, followed by individual proxies.
			IDetailCategoryBuilder& CategoryBuilder_None = DetailLayout.EditCategory("nocategory");
			for (const TTuple<FName, TArray<UObject*>>& FieldTypeToProxyPair : DetailRowIDToGroupedProxiesMap)
			{
				CategoryBuilder_None.AddExternalObjects(
					FieldTypeToProxyPair.Value,
					EPropertyLocation::Default,
					FAddPropertyParams()
					.HideRootObjectNode(true)
				);
			}

			// Note, individual proxies are displayed individually, but are still multi-edited across multiple control rigs
			IDetailCategoryBuilder& CategoryBuilder_Attributes = DetailLayout.EditCategory("Attributes");
			for (const TTuple<FName, TArray<UObject*>>& ControlNameToIndividualProxiesPair : DetailRowIDToIndividualProxiesMap)
			{
				CategoryBuilder_Attributes.AddExternalObjects(
					ControlNameToIndividualProxiesPair.Value,
					EPropertyLocation::Default,
					FAddPropertyParams()
					.HideRootObjectNode(true)
				);
			}
		}
	}
}

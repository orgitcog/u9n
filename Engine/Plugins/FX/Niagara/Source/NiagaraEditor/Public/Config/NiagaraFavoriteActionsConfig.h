// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorConfigBase.h"
#include "NiagaraFavoriteActionsConfig.generated.h"

USTRUCT()
struct FNiagaraActionIdentifier
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FName> Names;

	UPROPERTY()
	TArray<FGuid> Guids;	
	
	bool IsValid() const
	{
		return Guids.Num() > 0 || Names.Num() > 0;
	}
	
	bool operator==(const FNiagaraActionIdentifier& OtherIdentity) const
	{
		if(Guids.Num() != OtherIdentity.Guids.Num() || Names.Num() != OtherIdentity.Names.Num())
		{
			return false;
		}

		for(int32 GuidIndex = 0; GuidIndex < Guids.Num(); GuidIndex++)
		{
			if(Guids[GuidIndex] != OtherIdentity.Guids[GuidIndex])
			{
				return false;
			}
		}

		for(int32 NameIndex = 0; NameIndex < Names.Num(); NameIndex++)
		{
			if(!Names[NameIndex].IsEqual(OtherIdentity.Names[NameIndex]))
			{
				return false;
			}
		}

		return true;
	}

	bool operator!=(const FNiagaraActionIdentifier& OtherIdentity) const
	{
		return !(*this == OtherIdentity);
	}
};

FORCEINLINE uint32 GetTypeHash(const FNiagaraActionIdentifier& Identity)
{
	uint32 Hash = 0;
	
	for(const FGuid& Guid : Identity.Guids)
	{
		Hash = HashCombine(Hash, GetTypeHash(Guid));
	}
	
	for(const FName& Name : Identity.Names)
	{
		Hash = HashCombine(Hash, GetTypeHash(Name));
	}
	
	return Hash;
}

struct FNiagaraFavoritesActionData
{
	FNiagaraActionIdentifier ActionIdentifier;
	bool bFavoriteByDefault = false;
};

USTRUCT()
struct NIAGARAEDITOR_API FNiagaraFavoriteActionsProfile
{
	GENERATED_BODY()
public:
	
	bool IsFavorite(FNiagaraFavoritesActionData InAction);
	void ToggleFavoriteAction(FNiagaraFavoritesActionData InAction);
	
private:
	/** Explicitly favorited actions */
	UPROPERTY()
	TSet<FNiagaraActionIdentifier> FavoriteActions;

	/** For unfavorited actions */
	UPROPERTY()
	TSet<FNiagaraActionIdentifier> UnfavoriteActions;
};
/**
 * 
 */
UCLASS(EditorConfig="FavoriteNiagaraActions")
class NIAGARAEDITOR_API UNiagaraFavoriteActionsConfig : public UEditorConfigBase
{
	GENERATED_BODY()

public:
	bool HasActionsProfile(FName ProfileName) const { return Profiles.Contains(ProfileName); }
	FNiagaraFavoriteActionsProfile& GetActionsProfile(FName ProfileName);
	
	static UNiagaraFavoriteActionsConfig* Get();
	static void Shutdown();
	
private:
	UPROPERTY(meta=(EditorConfig))
	TMap<FName, FNiagaraFavoriteActionsProfile> Profiles;

private:
	static TStrongObjectPtr<UNiagaraFavoriteActionsConfig> Instance;
};

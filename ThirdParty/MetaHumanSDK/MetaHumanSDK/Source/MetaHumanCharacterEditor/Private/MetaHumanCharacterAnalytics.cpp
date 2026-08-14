// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterAnalytics.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterBodyIdentity.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "MetaHumanCharacterPipeline.h"
#include "Cloud/MetaHumanARServiceRequest.h"

#include "EngineAnalytics.h"
#include "Logging/LogMacros.h"
#include "Logging/StructuredLog.h"

namespace UE::MetaHuman::Analytics
{
	namespace
	{
		const FString EventNamePrefix = TEXT("Editor.MetaHumanCharacter.");

		FString AnonymizeString(const FString& String)
		{
			FSHA1 Sha1;
			Sha1.UpdateWithString(*String, String.Len());
			const FSHAHash HashedName = Sha1.Finalize();
			return HashedName.ToString();
		}

		FString AnonymizeName(const FName& Name)
		{
			return AnonymizeString(Name.ToString());
		}

		void StartRecordEvent(TArray<FAnalyticsEventAttribute>& EventAttributes, const UMetaHumanCharacter* InMetaHumanCharacter)
		{
			FPrimaryAssetId PrimaryAssetId(InMetaHumanCharacter->GetClass()->GetFName(), InMetaHumanCharacter->GetFName());
			const FString PrimaryAssetIdStr = PrimaryAssetId.PrimaryAssetType.GetName().ToString() / PrimaryAssetId.PrimaryAssetName.ToString();
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("CharacterId"), AnonymizeString(PrimaryAssetId.PrimaryAssetType.GetName().ToString() / PrimaryAssetId.PrimaryAssetName.ToString())));
		}

		void FinishRecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& EventAttributes)
		{
			check(FEngineAnalytics::IsAvailable());
			const FString FullEventName = EventNamePrefix + EventName;
			FEngineAnalytics::GetProvider().RecordEvent(FullEventName, EventAttributes);
		}

		void RecordBodyTypeInformation(TArray<FAnalyticsEventAttribute>& EventAttributes, const UMetaHumanCharacter* InMetaHumanCharacter)
		{
			if (UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterEditorSubsystem = UMetaHumanCharacterEditorSubsystem::Get())
			{
				TSharedRef<const FMetaHumanCharacterBodyIdentity::FState> BodyState = MetaHumanCharacterEditorSubsystem->GetBodyState(InMetaHumanCharacter);
				EMetaHumanBodyType BodyType = BodyState->GetMetaHumanBodyType();
				if (BodyType == EMetaHumanBodyType::BlendableBody)
				{
					EventAttributes.Add(FAnalyticsEventAttribute(TEXT("BlendableBody"), true));
				}
				else
				{
					EventAttributes.Add(FAnalyticsEventAttribute(TEXT("LegacyBodyType"), static_cast<int32>(BodyType)));
				}
			}
		}
	}

#define NO_ANALYTICS_CIRCUIT_BREAK()\
if (!FEngineAnalytics::IsAvailable()) return

#define BEGIN_RECORD_EVENT(EventName,FuncName,...)\
void Record##FuncName##Event(TNotNull<const UMetaHumanCharacter*> InMetaHumanCharacter, __VA_ARGS__)\
{\
	const FString EventNameStr = TEXT(#EventName);\
	if (!FEngineAnalytics::IsAvailable()) return;\
	TArray<FAnalyticsEventAttribute> EventAttributes;\
	StartRecordEvent(EventAttributes, InMetaHumanCharacter);


#define END_RECORD_EVENT()\
FinishRecordEvent(EventNameStr, EventAttributes);\
}

#define DEFINE_RECORD_EVENT(EventName,FuncName)\
void Record##FuncName##Event(TNotNull<const UMetaHumanCharacter*> InMetaHumanCharacter)\
{\
	const FString EventNameStr = TEXT(#EventName);\
	if (!FEngineAnalytics::IsAvailable()) return;\
	TArray<FAnalyticsEventAttribute> EventAttributes;\
	StartRecordEvent(EventAttributes, InMetaHumanCharacter);\
	FinishRecordEvent(EventNameStr, EventAttributes);\
}

	DEFINE_RECORD_EVENT(New, NewCharacter);
	DEFINE_RECORD_EVENT(OpenEditor, OpenCharacterEditor);

	BEGIN_RECORD_EVENT(Build, BuildPipelineCharacter, const TSubclassOf<UMetaHumanCharacterPipeline> InMaybePipeline)
	{
		if (InMaybePipeline != nullptr)
		{
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("PipelineID"), AnonymizeString(InMaybePipeline->GetPathName())));
		}
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("HasSynthesisedTextures"), InMetaHumanCharacter->HasSynthesizedTextures()));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("HasHighResolutionTextures"), InMetaHumanCharacter->HasHighResolutionTextures()));

		RecordBodyTypeInformation(EventAttributes, InMetaHumanCharacter);
	}
	END_RECORD_EVENT();

	BEGIN_RECORD_EVENT(Autorig, RequestAutorig, UE::MetaHuman::ERigType RigType)
	{
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("RigType"), static_cast<int32>(RigType)));
		RecordBodyTypeInformation(EventAttributes, InMetaHumanCharacter);		
	}
	END_RECORD_EVENT();

	BEGIN_RECORD_EVENT(HighResolutionTextures, RequestHighResolutionTextures, ERequestTextureResolution RequestTextureResolution)
	{
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Resolution"), static_cast<int32>(RequestTextureResolution)));
	}
	END_RECORD_EVENT();

	DEFINE_RECORD_EVENT(SaveFaceDNA, SaveFaceDNA);
	DEFINE_RECORD_EVENT(SaveBodyDNA, SaveBodyDNA);
	DEFINE_RECORD_EVENT(SaveHighResolutionTextures, SaveHighResolutionTextures);
	DEFINE_RECORD_EVENT(ImportFaceDNA, ImportFaceDNA);
	DEFINE_RECORD_EVENT(ImportBodyDNA, ImportBodyDNA);
	DEFINE_RECORD_EVENT(CreateMeshFromDNA, CreateMeshFromDNA);
	
	
	void RecordWardrobeItemEventImpl(TArray<FAnalyticsEventAttribute>& EventAttributes, const FName& SlotName, const FName& AssetName)
	{
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("AssetName"), AnonymizeName(AssetName)));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("SlotName"), SlotName.ToString())); //< this doesn't need to be anonymized since it's something *we* have defined
	}
	
	void RecordWardrobeItemWornEvent(const FName& SlotName, const FName& AssetName)
	{
		NO_ANALYTICS_CIRCUIT_BREAK();
		TArray<FAnalyticsEventAttribute> EventAttributes;
		RecordWardrobeItemEventImpl(EventAttributes, SlotName, AssetName);
		FinishRecordEvent(TEXT("WardrobeItemWorn"), EventAttributes);
	}

	void RecordWardrobeItemPreparedEvent(const FName& SlotName, const FName& AssetName)
	{
		NO_ANALYTICS_CIRCUIT_BREAK();
		TArray<FAnalyticsEventAttribute> EventAttributes;
		RecordWardrobeItemEventImpl(EventAttributes, SlotName, AssetName);
		FinishRecordEvent(TEXT("WardrobeItemPrepared"), EventAttributes);
	}
}


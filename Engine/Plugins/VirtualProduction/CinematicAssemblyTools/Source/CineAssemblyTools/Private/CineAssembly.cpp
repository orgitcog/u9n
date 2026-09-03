// Copyright Epic Games, Inc. All Rights Reserved.

#include "CineAssembly.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "CineAssemblyNamingTokens.h"
#include "HAL/FileManager.h"
#include "LevelSequenceShotMetaDataLibrary.h"
#include "MovieScene.h"
#include "NamingTokensEngineSubsystem.h"
#include "Sections/MovieSceneSubSection.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "UObject/AssetRegistryTagsContext.h"

#if WITH_EDITOR
#include "AssetToolsModule.h"
#endif

#define LOCTEXT_NAMESPACE "CineAssembly"

const FName UCineAssembly::AssetRegistryTag_AssemblyType = "AssemblyType";
const FName UCineAssembly::AssemblyGuidPropertyName = GET_MEMBER_NAME_CHECKED(UCineAssembly, AssemblyGuid);

UCineAssembly::UCineAssembly()
{
	MetadataJsonObject = MakeShared<FJsonObject>();
}

void UCineAssembly::PostInitProperties()
{
	Super::PostInitProperties();

	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_NeedLoad | RF_WasLoaded) && !AssemblyGuid.IsValid())
	{
		AssemblyGuid = FGuid::NewGuid();
	}
}

void UCineAssembly::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if (!bDuplicateForPIE)
	{
		AssemblyGuid = FGuid::NewGuid();
	}
}

void UCineAssembly::PostLoad()
{
	Super::PostLoad();

	if (!AssemblyGuid.IsValid())
	{
		AssemblyGuid = FGuid::NewGuid();
	}
}

void UCineAssembly::Initialize()
{
	Super::Initialize();
}

FGuid UCineAssembly::GetAssemblyGuid() const
{
	return AssemblyGuid;
}

const UCineAssemblySchema* UCineAssembly::GetSchema() const
{
	return BaseSchema.Get();
}

void UCineAssembly::SetSchema(UCineAssemblySchema* InSchema)
{
	if (BaseSchema == nullptr)
	{
		BaseSchema = InSchema;
		ChangeSchema(InSchema);
	}
}

void UCineAssembly::ChangeSchema(UCineAssemblySchema* InSchema)
{
	// Remove all metadata associated with the old schema before changing it
	if (BaseSchema)
	{
		for (const FAssemblyMetadataDesc& MetadataDesc : BaseSchema->AssemblyMetadata)
		{
			MetadataJsonObject->RemoveField(MetadataDesc.Key);
		}
	}

	BaseSchema = InSchema;

	// Reset the assembly's name based on the schema template
	if (BaseSchema)
	{
		AssemblyName.Template = BaseSchema->DefaultAssemblyName;
	}
	else
	{
		AssemblyName.Template = TEXT("");
	}

	// Add all metadata associated with the new schema (initialized to the default values for each field)
	if (BaseSchema)
	{
		for (const FAssemblyMetadataDesc& MetadataDesc : BaseSchema->AssemblyMetadata)
		{
			if (MetadataDesc.DefaultValue.IsType<FString>())
			{
				const FString& DefaultValue = MetadataDesc.DefaultValue.Get<FString>();
				MetadataJsonObject->SetStringField(MetadataDesc.Key, DefaultValue);
			}
			else if (MetadataDesc.DefaultValue.IsType<bool>())
			{
				const bool& DefaultValue = MetadataDesc.DefaultValue.Get<bool>();
				MetadataJsonObject->SetBoolField(MetadataDesc.Key, DefaultValue);
			}
			else if (MetadataDesc.DefaultValue.IsType<int32>())
			{
				const int32& DefaultValue = MetadataDesc.DefaultValue.Get<int32>();
				MetadataJsonObject->SetNumberField(MetadataDesc.Key, DefaultValue);
			}
			else if (MetadataDesc.DefaultValue.IsType<float>())
			{
				const float& DefaultValue = MetadataDesc.DefaultValue.Get<float>();
				MetadataJsonObject->SetNumberField(MetadataDesc.Key, DefaultValue);
			}
		}
	}

	// Reset the list of SubAssembly names to create from the Schema
	SubAssemblyNames.Reset();
	if (BaseSchema)
	{
		for (const FString& SubAssemblyName : BaseSchema->SubsequencesToCreate)
		{
			FTemplateString NewTemplateName;
			NewTemplateName.Template = SubAssemblyName;
			SubAssemblyNames.Add(NewTemplateName);
		}
	}

	// Reset the list of folders names to create from the Schema
	DefaultFolderNames.Reset();
	if (BaseSchema)
	{
		for (const FString& FolderName : BaseSchema->FoldersToCreate)
		{
			FTemplateString NewTemplateName;
			NewTemplateName.Template = FolderName;
			DefaultFolderNames.Add(NewTemplateName);
		}
	}
}

#if WITH_EDITOR
void UCineAssembly::CreateSubAssemblies()
{
	if (!BaseSchema || !SubAssemblies.IsEmpty())
	{
		return;
	}

	// Get the path where the top-level assembly will be created so we can create other assets relative to it
	const FAssetData AssemblyAssetData = FAssetData(this);
	FString PackagePath = AssemblyAssetData.PackagePath.ToString();

	// Remove the default assembly path from the top-level assembly's package path to get back to the "root" path for the schema's folder hierarchy
	FString ResolvedDefaultAssemblyPath = UCineAssemblyNamingTokens::GetResolvedText(BaseSchema->DefaultAssemblyPath, this).ToString();
	PackagePath = PackagePath.Replace(*ResolvedDefaultAssemblyPath, TEXT(""));

	// Create the default folders for this assembly, based on the schema
	for (FTemplateString& FolderPath : DefaultFolderNames)
	{
		// Resolve any tokens found in the name of the SubAssembly before attempting to create it 
		FolderPath.Resolved = UCineAssemblyNamingTokens::GetResolvedText(FolderPath.Template, this);

		if (FolderPath.Resolved.IsEmpty())
		{
			continue;
		}

		const FString PathToCreate = PackagePath / FolderPath.Resolved.ToString();
		const FString RelativeFilePath = FPackageName::LongPackageNameToFilename(PathToCreate);
		const FString AbsoluteFilePath = FPaths::ConvertRelativePathToFull(RelativeFilePath);

		// Create the directory on disk, then add its path to the asset registry so it appears in Content Browser
		if (!IFileManager::Get().DirectoryExists(*AbsoluteFilePath))
		{
			constexpr bool bCreateParentFoldersIfMissing = true;
			if (IFileManager::Get().MakeDirectory(*AbsoluteFilePath, bCreateParentFoldersIfMissing))
			{
				FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry");
				AssetRegistryModule.Get().AddPath(PathToCreate);
			}
		}
	}

	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

	// Create a new CineAssembly for each subsequence, set its playback range to match the parent sequence, and add it to the subsequence track
	for (FTemplateString& SubAssemblyName : SubAssemblyNames)
	{
		// Resolve any tokens found in the name of the SubAssembly before attempting to create it 
		SubAssemblyName.Resolved = UCineAssemblyNamingTokens::GetResolvedText(SubAssemblyName.Template, this);

		const FString SubAssemblyFilename = FPaths::GetBaseFilename(SubAssemblyName.Resolved.ToString());

		if (SubAssemblyFilename.IsEmpty())
		{
			continue;
		}

		// Add a subsequence track to the assembly's sequence
		UMovieSceneSubTrack* SubTrack = Cast<UMovieSceneSubTrack>(MovieScene->AddTrack(UMovieSceneSubTrack::StaticClass()));
		SubTrack->SetDisplayName(FText::FromString(SubAssemblyFilename));

		// Before creating each subassembly, sanity check that each one will actually have a unique name (in case there are duplicates in the schema description)
		FString UniquePackageName;
		FString UniqueAssetName;
		AssetTools.CreateUniqueAssetName(PackagePath / SubAssemblyName.Resolved.ToString(), TEXT(""), UniquePackageName, UniqueAssetName);

		const FString SubAssemblyPath = FPaths::GetPath(UniquePackageName);
		UCineAssembly* SubAssembly = Cast<UCineAssembly>(AssetTools.CreateAsset(UniqueAssetName, SubAssemblyPath, UCineAssembly::StaticClass(), nullptr));
		if (SubAssembly)
		{
			SubAssembly->Initialize();

			const TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();
			SubAssembly->GetMovieScene()->SetPlaybackRange(PlaybackRange);

			SubAssembly->Level = Level;
			SubAssembly->ParentAssembly = this;
			SubAssembly->Production = Production;
			SubAssembly->ProductionName = ProductionName;

			ULevelSequenceShotMetaDataLibrary::SetIsSubSequence(SubAssembly, true);

			const FFrameNumber StartFrame = PlaybackRange.GetLowerBoundValue();
			const int32 Duration = PlaybackRange.Size<FFrameNumber>().Value;

			UMovieSceneSubSection* SubSection = SubTrack->AddSequence(SubAssembly, StartFrame, Duration);
			SubAssemblies.Add(SubSection);
		}
	}
}
#endif

void UCineAssembly::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

	const FString AssemblyType = BaseSchema ? BaseSchema->SchemaName : TEXT("");
	Context.AddTag(FAssetRegistryTag(AssetRegistryTag_AssemblyType, AssemblyType, FAssetRegistryTag::ETagType::TT_Alphabetical, FAssetRegistryTag::TD_None));

	// Add tags associated with the assembly metadata
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : MetadataJsonObject->Values)
	{
		if (!Pair.Key.IsEmpty())
		{
			FString ValueString;
			if (MetadataJsonObject->TryGetStringField(Pair.Key, ValueString))
			{
				Context.AddTag(FAssetRegistryTag(*Pair.Key, ValueString, FAssetRegistryTag::ETagType::TT_Alphabetical, FAssetRegistryTag::TD_None));
			}
		}
	}
}

#if WITH_EDITOR

void UCineAssembly::GetAssetRegistryTagMetadata(TMap<FName, FAssetRegistryTagMetadata>& OutMetadata) const
{
	Super::GetAssetRegistryTagMetadata(OutMetadata);

	OutMetadata.Add(AssetRegistryTag_AssemblyType, FAssetRegistryTagMetadata()
		.SetDisplayName(LOCTEXT("AssemblyType_Label", "AssemblyType"))
		.SetTooltip(LOCTEXT("AssemblyType_Tooltip", "The assembly type of this instance"))
	);
}

void UCineAssembly::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UCineAssembly, InstanceMetadata))
	{
		UpdateInstanceMetadata();
	}
}

#endif

void UCineAssembly::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	FString JsonString;

	if (Ar.IsSaving())
 	{
 		TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
 		FJsonSerializer::Serialize(MetadataJsonObject.ToSharedRef(), JsonWriter);
 	}

	Ar << JsonString;

	if (Ar.IsLoading())
	{
		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonString);
		FJsonSerializer::Deserialize(JsonReader, MetadataJsonObject);

		// After the JsonObject has been loaded, add a naming token for each of its keys
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : MetadataJsonObject->Values)
		{
			AddMetadataNamingToken(Pair.Key);
		}
	}
}

TSoftObjectPtr<UWorld> UCineAssembly::GetLevel()
{
	if (Level.IsValid())
	{
		return TSoftObjectPtr<UWorld>(Level);
	}
	return nullptr;
}

void UCineAssembly::SetLevel(TSoftObjectPtr<UWorld> InLevel)
{
	Level = InLevel.ToSoftObjectPath();
}

FString UCineAssembly::GetNoteText()
{
	return AssemblyNote;
}

void UCineAssembly::SetNoteText(FString InNote)
{
	AssemblyNote = InNote;
}

void UCineAssembly::AppendToNoteText(FString InNote)
{
	AssemblyNote.Append(TEXT("\n"));
	AssemblyNote.Append(InNote);
}

FGuid UCineAssembly::GetProductionID()
{
	return Production;
}

FString UCineAssembly::GetProductionName()
{
	return ProductionName;
}

TSoftObjectPtr<UCineAssembly> UCineAssembly::GetParentAssembly()
{
	if (ParentAssembly.IsValid())
	{
		return TSoftObjectPtr<UCineAssembly>(ParentAssembly);
	}
	return nullptr;
}

void UCineAssembly::SetParentAssembly(TSoftObjectPtr<UCineAssembly> InParent)
{
	ParentAssembly = InParent.ToSoftObjectPath();
}

FString UCineAssembly::GetFullMetadataString() const
{
	FString JsonString;

	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(MetadataJsonObject.ToSharedRef(), JsonWriter);

	return JsonString;
}

void UCineAssembly::SetMetadataAsString(FString InKey, FString InValue)
{
	Modify();
	MetadataJsonObject->SetStringField(InKey, InValue);
	AddMetadataNamingToken(InKey);
}

void UCineAssembly::SetMetadataAsBool(FString InKey, bool InValue)
{
	Modify();
	MetadataJsonObject->SetBoolField(InKey, InValue);
	AddMetadataNamingToken(InKey);
}

void UCineAssembly::SetMetadataAsInteger(FString InKey, int32 InValue)
{
	Modify();
	MetadataJsonObject->SetNumberField(InKey, InValue);
	AddMetadataNamingToken(InKey);
}

void UCineAssembly::SetMetadataAsFloat(FString InKey, float InValue)
{
	Modify();
	MetadataJsonObject->SetNumberField(InKey, InValue);
	AddMetadataNamingToken(InKey);
}

bool UCineAssembly::GetMetadataAsString(FString InKey, FString& OutValue) const
{
	if (!MetadataJsonObject->TryGetStringField(InKey, OutValue))
	{
		OutValue = TEXT("");
		return false;
	}
	return true;
}

bool UCineAssembly::GetMetadataAsBool(FString InKey, bool& OutValue) const
{
	if (!MetadataJsonObject->TryGetBoolField(InKey, OutValue))
	{
		OutValue = false;
		return false;
	}
	return true;
}

bool UCineAssembly::GetMetadataAsInteger(FString InKey, int32& OutValue) const
{
	if (!MetadataJsonObject->TryGetNumberField(InKey, OutValue))
	{
		OutValue = 0;
		return false;
	}
	return true;
}

bool UCineAssembly::GetMetadataAsFloat(FString InKey, float& OutValue) const
{
	if (!MetadataJsonObject->TryGetNumberField(InKey, OutValue))
	{
		OutValue = 0;
		return false;
	}
	return true;
}

void UCineAssembly::UpdateInstanceMetadata()
{
	// Copy our metadata key list so that we can remove keys as we encounter them in the map non-destructively
	TArray<FName> InstanceMetadataKeysCopy = InstanceMetadataKeys;
	for (const TPair<FName, FString>& Pair : InstanceMetadata)
	{
		if (!Pair.Key.IsNone())
		{
			if (InstanceMetadataKeys.Contains(Pair.Key))
			{
				// This is an existing metadata key that we are already tracking
				InstanceMetadataKeysCopy.Remove(Pair.Key);
			}
			else
			{
				// This is a new metadata key that we were not previously tracking
				InstanceMetadataKeys.Add(Pair.Key);
			}

			SetMetadataAsString(Pair.Key.ToString(), Pair.Value);
		}
	}

	// If there are any keys remaining in our copy of the metadata key list, then those keys must have been removed from the instance metadata map
	for (const FName& Key : InstanceMetadataKeysCopy)
	{
		MetadataJsonObject->RemoveField(Key.ToString());
	}
}

void UCineAssembly::AddMetadataNamingToken(const FString& InKey)
{
	UNamingTokensEngineSubsystem* NamingTokensSubsystem = GEngine->GetEngineSubsystem<UNamingTokensEngineSubsystem>();
	UCineAssemblyNamingTokens* CineAssemblyNamingTokens = Cast<UCineAssemblyNamingTokens>(NamingTokensSubsystem->GetNamingTokens(UCineAssemblyNamingTokens::TokenNamespace));

	CineAssemblyNamingTokens->AddMetadataToken(InKey);
}

#undef LOCTEXT_NAMESPACE

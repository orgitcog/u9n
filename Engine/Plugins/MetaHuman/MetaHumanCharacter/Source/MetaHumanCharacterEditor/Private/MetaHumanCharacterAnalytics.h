// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "Misc/NotNull.h"

class UMetaHumanCharacter;
class UMetaHumanCharacterPipeline;
enum class ERequestTextureResolution;

namespace UE::MetaHuman
{
	enum class ERigType;
}

namespace UE::MetaHuman::Analytics
{
	void RecordNewCharacterEvent(TNotNull<const UMetaHumanCharacter*> InMetaHumanCharacter);
	void RecordOpenCharacterEditorEvent(TNotNull<const UMetaHumanCharacter*> InMetaHumanCharacter);
	void RecordBuildPipelineCharacterEvent(TNotNull<const UMetaHumanCharacter*> InMetaHumanCharacter, const TSubclassOf<UMetaHumanCharacterPipeline> InMaybePipeline);
	void RecordRequestAutorigEvent(TNotNull<const UMetaHumanCharacter*> InMetaHumanCharacter, ERigType RigType);
	void RecordRequestHighResolutionTexturesEvent(TNotNull<const UMetaHumanCharacter*> InMetaHumanCharacter, ERequestTextureResolution RequestTextureResolution);
	void RecordSaveFaceDNAEvent(TNotNull<const UMetaHumanCharacter*> InMetaHumanCharacter);
	void RecordSaveBodyDNAEvent(TNotNull<const UMetaHumanCharacter*> InMetaHumanCharacter);
	void RecordSaveHighResolutionTexturesEvent(TNotNull<const UMetaHumanCharacter*> InMetaHumanCharacter);
	void RecordCreateMeshFromDNAEvent(TNotNull<const UMetaHumanCharacter*> InMetaHumanCharacter);
	void RecordWardrobeItemPreparedEvent(const FName& ItemSlotName, const FName& ItemAssetName);
	void RecordWardrobeItemWornEvent(const FName& ItemSlotName, const FName& ItemAssetName);
	void RecordImportFaceDNAEvent(TNotNull<const UMetaHumanCharacter*> InMetaHumanCharacter);
	void RecordImportBodyDNAEvent(TNotNull<const UMetaHumanCharacter*> InMetaHumanCharacter);
}

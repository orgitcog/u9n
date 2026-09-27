// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "InterchangeTranslatorBase.h"
#include "InterchangeMeshDefinitions.h"
#include "Mesh/InterchangeMeshPayload.h"
#include "Mesh/InterchangeMeshPayloadInterface.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeDnaTranslator.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(InterchangeDNATranslator, Log, All);

class IDNAReader;

UCLASS(BlueprintType)
class DNAINTERCHANGE_API UInterchangeDnaTranslator : public UInterchangeTranslatorBase
	, public IInterchangeMeshPayloadInterface
{
	GENERATED_BODY()
public:
	UInterchangeDnaTranslator();

	/** Begin UInterchangeTranslatorBase API*/
	virtual bool IsThreadSafe() const override;
	virtual EInterchangeTranslatorType GetTranslatorType() const override;
	virtual EInterchangeTranslatorAssetType GetSupportedAssetTypes() const override;
	virtual TArray<FString> GetSupportedFormats() const override;
	virtual bool Translate(UInterchangeBaseNodeContainer& BaseNodeContainer) const override;
	virtual void ReleaseSource() override;
	virtual void ImportFinish() override;
	/** End UInterchangeTranslatorBase API*/

	//////////////////////////////////////////////////////////////////////////
	/* IInterchangeMeshPayloadInterface Begin */

	/**
	 * Once the translation is done, the import process need a way to retrieve payload data.
	 * This payload will be use by the factories to create the asset.
	 *
	 * @param SourceData - The source data containing the data to translate
	 * @param PayloadKey - The key to retrieve the a particular payload contain into the specified source data.
	 * @return a PayloadData containing the imported data. The TOptional will not be set if there is an error.
	 */
	UE_DEPRECATED(5.6, "Deprecated. Use GetMeshPayloadData(const FInterchangeMeshPayLoadKey&) instead.")
	virtual TOptional<UE::Interchange::FMeshPayloadData> GetMeshPayloadData(const FInterchangeMeshPayLoadKey& PayLoadKey, const FTransform& MeshGlobalTransform) const override
	{
		using namespace UE::Interchange;
		UE::Interchange::FAttributeStorage Attributes;
		Attributes.RegisterAttribute(UE::Interchange::FAttributeKey{ MeshPayload::Attributes::MeshGlobalTransform }, MeshGlobalTransform);
		return GetMeshPayloadData(PayLoadKey, Attributes);
	}

	virtual TOptional<UE::Interchange::FMeshPayloadData> GetMeshPayloadData(const FInterchangeMeshPayLoadKey& PayLoadKey, const UE::Interchange::FAttributeStorage& PayloadAttributes) const override;

	///* IInterchangeMeshPayloadInterface End */

	/** Populates mesh description attributes with  static mesh data from the DNA reader for specified mesh */
	static void PopulateStaticMeshDescription(FMeshDescription& OutMeshDescription, const IDNAReader &InDNAReader, const int32 InMeshIndex);

private:

	TSharedPtr<IDNAReader> DNAReader;
	
	FString GetJointHierarchyName(TSharedPtr<IDNAReader> InDNAReader, int32 JointIndex) const;
	FString AddDNAMissingJoints(UInterchangeBaseNodeContainer& NodeContainer, const FString& InLastNodeId, FTransform& OutCombinedTransform) const;
	int32 GetMeshIndexForPayload(const FString& PayloadKey) const;
	bool FetchMeshPayloadData(const FString& PayloadKey, const FTransform& MeshGlobalTransform, FMeshDescription& OutMeshDescription, TArray<FString>& OutJointNames) const;

	static const TArray<FString> DNAMissingJoints;
};
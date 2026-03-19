// Copyright Epic Games, Inc. All Rights Reserved. 

#include "MaterialX/InterchangeMaterialXTranslator.h"
#include "InterchangeManager.h"
#include "InterchangeTranslatorHelper.h"
#include "MaterialX/MaterialXUtils/MaterialXManager.h"
#include "Nodes/InterchangeSourceNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeMaterialXTranslator)

static bool GInterchangeEnableMaterialXImport = true;
static FAutoConsoleVariableRef CCvarInterchangeEnableMaterialXImport(
	TEXT("Interchange.FeatureFlags.Import.MTLX"),
	GInterchangeEnableMaterialXImport,
	TEXT("Whether MaterialX support is enabled."),
	ECVF_Default);

EInterchangeTranslatorType UInterchangeMaterialXTranslator::GetTranslatorType() const
{
	return EInterchangeTranslatorType::Assets;
}

EInterchangeTranslatorAssetType UInterchangeMaterialXTranslator::GetSupportedAssetTypes() const
{
	return EInterchangeTranslatorAssetType::Materials;
}

TArray<FString> UInterchangeMaterialXTranslator::GetSupportedFormats() const
{
	// Call to UInterchangeMaterialXTranslator::GetSupportedFormats is not supported out of game thread
	// A more global solution must be found for translators which require some initialization
	if(!IsInGameThread() || (!GInterchangeEnableMaterialXImport && !GIsAutomationTesting))
	{
		return TArray<FString>{};
	}

	return UE::Interchange::MaterialX::AreMaterialFunctionPackagesLoaded() ? TArray<FString>{ TEXT("mtlx;MaterialX File Format") } : TArray<FString>{};
}

bool UInterchangeMaterialXTranslator::Translate(UInterchangeBaseNodeContainer& BaseNodeContainer) const
{
	bool bIsDocumentValid = false;

#if WITH_EDITOR
	namespace mx = MaterialX;

	FString Filename = GetSourceData()->GetFilename();

	bIsDocumentValid = FMaterialXManager::GetInstance().Translate(Filename, BaseNodeContainer, this);

#endif // WITH_EDITOR

	if(bIsDocumentValid)
	{
		UInterchangeSourceNode* SourceNode = UInterchangeSourceNode::FindOrCreateUniqueInstance(&BaseNodeContainer);
		SourceNode->SetCustomImportUnusedMaterial(true);
	}

	return bIsDocumentValid;
}

TOptional<UE::Interchange::FImportImage> UInterchangeMaterialXTranslator::GetTexturePayloadData(const FString& PayloadKey, TOptional<FString>& AlternateTexturePath) const
{
	FString Filename = PayloadKey;
	TextureCompressionSettings CompressionSettings = TextureCompressionSettings::TC_Default;

#if WITH_EDITOR
	if(int32 IndexTextureCompression; PayloadKey.FindChar(FMaterialXManager::TexturePayloadSeparator, IndexTextureCompression))
	{
		Filename = PayloadKey.Mid(0, IndexTextureCompression);
		CompressionSettings = TextureCompressionSettings(FCString::Atoi(*PayloadKey.Mid(IndexTextureCompression + 1)));
	}
#endif

	UE::Interchange::Private::FScopedTranslator ScopedTranslator(Filename, Results, AnalyticsHelper);
	const IInterchangeTexturePayloadInterface* TextureTranslator = ScopedTranslator.GetPayLoadInterface<IInterchangeTexturePayloadInterface>();

	if(!ensure(TextureTranslator))
	{
		return TOptional<UE::Interchange::FImportImage>();
	}

	AlternateTexturePath = Filename;

	TOptional<UE::Interchange::FImportImage> TexturePayloadData = TextureTranslator->GetTexturePayloadData(PayloadKey, AlternateTexturePath);

	if(TexturePayloadData.IsSet())
	{
		TexturePayloadData.GetValue().CompressionSettings = CompressionSettings;
	}

	return TexturePayloadData;
}

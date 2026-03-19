// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConversionDataGenerator.h"
#include "MetaHumanConfig.h"
#include "DNAAsset.h"
#include "DNAUtils.h"
#include "DNAReader.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Interfaces/IPluginManager.h"

#if WITH_DEV_AUTOMATION_TESTS

static dna::Reader* GetDnaBehaviorReader()
{
	const FString PluginDir = IPluginManager::Get().FindPlugin(TEXT(UE_PLUGIN_NAME))->GetContentDir();
	const FString PathToDNA = PluginDir + TEXT("/IdentityTemplate/Face_Archetype.ardna");
	TObjectPtr<UDNAAsset> DNAAsset = GetDNAAssetFromFile(PathToDNA, GetTransientPackage());
	
	dna::Reader* BehaviorReader = DNAAsset->GetBehaviorReader()->Unwrap();
	return BehaviorReader;
}

static TSharedPtr<FJsonObject> GetSolverDefinitionsJson()
{
	UMetaHumanConfig* DeviceConfig = LoadObject<UMetaHumanConfig>(GetTransientPackage(), TEXT("/" UE_PLUGIN_NAME "/Solver/iphone12.iphone12"));
	
	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(DeviceConfig->GetSolverHierarchicalDefinitionsData());
	FJsonSerializer::Deserialize(JsonReader, JsonParsed);
	return JsonParsed;
}

void WriteMappingsInfoFromDnaToFile(const FString& InFilepath)
{
	dna::Reader* BehaviorReader = GetDnaBehaviorReader();
	
	int32 NumGuiControls = BehaviorReader->getGUIControlCount();
	int32 NumRawControls = BehaviorReader->getRawControlCount();

	TArray<FString> LinesToWrite;

	// Add default names
	LinesToWrite.Add("const TArray<FString> DefaultGuiNames = {");
	for (int32 GuiControlIndex = 0; GuiControlIndex < NumGuiControls; ++GuiControlIndex)
	{
		FString GuiName(BehaviorReader->getGUIControlName(std::uint16_t(GuiControlIndex)));
		LinesToWrite.Add(FString::Printf(TEXT("\t\"%s\","), *GuiName));
	}
	LinesToWrite.Add("};\n");

	LinesToWrite.Add("const TArray<FString> DefaultRawControlNames = {");
	for (int32 RawControlIndex = 0; RawControlIndex < NumRawControls; ++RawControlIndex)
	{
		FString RawName(BehaviorReader->getRawControlName(std::uint16_t(RawControlIndex)));
		RawName = RawName.Replace(TEXT("."), TEXT("_"));
		LinesToWrite.Add(FString::Printf(TEXT("\t\"%s\","), *RawName));
	}
	LinesToWrite.Add("};\n");

	// Write mappings and update ranges
	TArray<TPair<float, float>> Ranges;
	Ranges.Init({1e6f, -1e6f}, NumGuiControls);

	LinesToWrite.Add("const TArray<GuiToRawControlInfo> GuiToRawMappings = {");
	
	int32 NumGuiToRawIndices = BehaviorReader->getGUIToRawInputIndices().size();
	for (int32 GuiToRawIndex = 0; GuiToRawIndex < NumGuiToRawIndices; ++GuiToRawIndex)
	{
		// write out mapping
		int32 InputIndex = BehaviorReader->getGUIToRawInputIndices()[GuiToRawIndex];
		float From = BehaviorReader->getGUIToRawFromValues()[GuiToRawIndex];
		float To = BehaviorReader->getGUIToRawToValues()[GuiToRawIndex];
		LinesToWrite.Add(FString::Printf(TEXT("\t{%i, %i, %f, %f, %f, %f},"), InputIndex, BehaviorReader->getGUIToRawOutputIndices()[GuiToRawIndex], From, To, BehaviorReader->getGUIToRawSlopeValues()[GuiToRawIndex],  BehaviorReader->getGUIToRawCutValues()[GuiToRawIndex]));

		// update ranges
		Ranges[InputIndex] = {std::min(Ranges[InputIndex].Key, From), std::max(Ranges[InputIndex].Value, To)};
	}
	LinesToWrite.Add("};\n");

	// Write out ranges
	LinesToWrite.Add("const TArray<TPair<float, float>> GuiControlRanges = {");
	for (const TPair<float, float>& Range : Ranges)
	{
		LinesToWrite.Add(FString::Printf(TEXT("\t{%f, %f},"),Range.Key, Range.Value));
	}
	LinesToWrite.Add("};\n");

	// Write out default values
	TSharedPtr<FJsonObject> SolverDefinitionsJson = GetSolverDefinitionsJson();

	LinesToWrite.Add("const GuiControlsArray DefaultGuiValues{");

	for (int32 GuiControlIndex = 0; GuiControlIndex < NumGuiControls; ++GuiControlIndex)
	{
		FString GuiControlName(BehaviorReader->getGUIControlName(std::uint16_t(GuiControlIndex)).c_str());
		auto DefaultsJson = SolverDefinitionsJson->GetObjectField(TEXT("Defaults"));
		float DefaultValue = (DefaultsJson->HasField(GuiControlName)) ? DefaultsJson->GetNumberField(GuiControlName) : 0.0f;
		LinesToWrite.Add(FString::Printf(TEXT("\t%f,"), DefaultValue));

	}
	LinesToWrite.Add("};\n");

	FFileHelper::SaveStringArrayToFile(LinesToWrite, *InFilepath);
}

#endif
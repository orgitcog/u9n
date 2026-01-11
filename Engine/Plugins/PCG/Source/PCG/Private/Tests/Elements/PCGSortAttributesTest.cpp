// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/PCGTestsCommon.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "Data/PCGBasePointData.h"
#include "Elements/PCGSortAttributes.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

namespace SortCommonTestData
{
	static const FName AttributeName = TEXT("Attr");

	UPCGParamData* GenerateParamData(int32 EntriesCount, int32 Seed, bool bRandom)
	{
		UPCGParamData* OutData = NewObject<UPCGParamData>();
		check(OutData && OutData->Metadata);
		FPCGMetadataAttribute<int32>* Attribute = OutData->Metadata->CreateAttribute<int32>(AttributeName, 0, false, false);
		check(Attribute);
		FRandomStream RandomStream(Seed);

		for (int32 i = 0; i < EntriesCount; ++i)
		{
			Attribute->SetValueFromValueKey(OutData->Metadata->AddEntry(), bRandom ? RandomStream.RandRange(1, 9999) : 1);
		}

		return OutData;
	}

	TUniquePtr<FPCGContext> RunSortElementOnData(EPCGSortMethod Method, const UPCGData* InData, const FPCGAttributePropertyInputSelector& InputSource)
	{
		PCGTestsCommon::FTestData TestData;
		UPCGSortAttributesSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGSortAttributesSettings>(TestData);
		check(Settings);
		Settings->InputSource = InputSource;
		Settings->SortMethod = Method;
		FPCGElementPtr TestElement = TestData.Settings->GetElement();

		FPCGTaggedData& SourcePin = TestData.InputData.TaggedData.Emplace_GetRef();
		SourcePin.Pin = PCGPinConstants::DefaultInputLabel;
		SourcePin.Data = InData;

		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

		while (!TestElement->Execute(Context.Get())) {}
	
		return Context;
	}
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGSortPointsTest_Ascending, FPCGTestBaseClass, "Plugins.PCG.SortAttributes.Points.Ascending", PCGTestsCommon::TestFlags)

bool FPCGSortPointsTest_Ascending::RunTest(const FString& Parameters)
{
	TUniquePtr<FPCGContext> Context = SortCommonTestData::RunSortElementOnData(EPCGSortMethod::Ascending,
		PCGTestsCommon::CreateRandomPointData(100, 42, /*bRandomDensity=*/ true), 
		FPCGAttributePropertySelector::CreatePointPropertySelector<FPCGAttributePropertyInputSelector>(EPCGPointProperties::Density));

	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	// compare point data
	const FPCGTaggedData& Output = Outputs[0];
	const UPCGBasePointData* OutPointData = Cast<UPCGBasePointData>(Output.Data);
	UTEST_NOT_NULL("Output is a point data", OutPointData);
	check(OutPointData);
	
	const TConstPCGValueRange<float> DensityRange = OutPointData->GetConstDensityValueRange();

	for (int32 i = 0; i < DensityRange.Num() - 1; ++i)
	{
		UTEST_TRUE(FString::Format(TEXT("{0} is less than/equal to {1}"), { DensityRange[i], DensityRange[i + 1] }), DensityRange[i] <= DensityRange[i + 1]);
	}

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGSortAttributesTest_Ascending, FPCGTestBaseClass, "Plugins.PCG.SortAttributes.AttributeSet.Ascending", PCGTestsCommon::TestFlags)

bool FPCGSortAttributesTest_Ascending::RunTest(const FString& Parameters)
{
	TUniquePtr<FPCGContext> Context = SortCommonTestData::RunSortElementOnData(EPCGSortMethod::Ascending,
		SortCommonTestData::GenerateParamData(100, 42, /*bRandom=*/true),
		FPCGAttributePropertySelector::CreateAttributeSelector<FPCGAttributePropertyInputSelector>(SortCommonTestData::AttributeName));

	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	const FPCGTaggedData& Output = Outputs[0];
	const UPCGParamData* OutParamData = Cast<UPCGParamData>(Output.Data);
	UTEST_NOT_NULL("Output is an attribute set data", OutParamData);
	check(OutParamData && OutParamData->Metadata);

	const FPCGMetadataAttribute<int32>* Attribute = OutParamData->Metadata->GetConstTypedAttribute<int32>(SortCommonTestData::AttributeName);
	UTEST_NOT_NULL("Output has the sorted attribute", Attribute);
	check(Attribute);

	for (int i = 0; i < OutParamData->Metadata->GetLocalItemCount() - 1; ++i)
	{
		const int32 Value = Attribute->GetValue(i);
		const int32 NextValue = Attribute->GetValue(i + 1);
		UTEST_TRUE(FString::Format(TEXT("{0} is less than/equal to {1}"), { Value, NextValue }), Value <= NextValue);
	}

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGSortPointsTest_Descending, FPCGTestBaseClass, "Plugins.PCG.SortAttributes.Points.Descending", PCGTestsCommon::TestFlags)

bool FPCGSortPointsTest_Descending::RunTest(const FString& Parameters)
{
	TUniquePtr<FPCGContext> Context = SortCommonTestData::RunSortElementOnData(EPCGSortMethod::Descending,
		PCGTestsCommon::CreateRandomPointData(100, 42, /*bRandomDensity=*/ true),
		FPCGAttributePropertySelector::CreatePointPropertySelector<FPCGAttributePropertyInputSelector>(EPCGPointProperties::Density));

	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	// compare point data
	const FPCGTaggedData& Output = Outputs[0];
	const UPCGBasePointData* OutPointData = Cast<UPCGBasePointData>(Output.Data);

	UTEST_NOT_NULL("Output is a point data", OutPointData);
	check(OutPointData);
	
	const TConstPCGValueRange<float> DensityRange = OutPointData->GetConstDensityValueRange();

	for (int i = 0; i < DensityRange.Num() - 1; ++i)
	{
		UTEST_TRUE(FString::Format(TEXT("{0} is greater than/equal to {1}"), { DensityRange[i], DensityRange[i + 1] }), DensityRange[i] >= DensityRange[i + 1]);
	}
	
	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGSortAttributesTest_Descending, FPCGTestBaseClass, "Plugins.PCG.SortAttributes.AttributeSet.Descending", PCGTestsCommon::TestFlags)

bool FPCGSortAttributesTest_Descending::RunTest(const FString& Parameters)
{
	TUniquePtr<FPCGContext> Context = SortCommonTestData::RunSortElementOnData(EPCGSortMethod::Descending,
		SortCommonTestData::GenerateParamData(100, 42, /*bRandom=*/true),
		FPCGAttributePropertySelector::CreateAttributeSelector<FPCGAttributePropertyInputSelector>(SortCommonTestData::AttributeName));

	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	const FPCGTaggedData& Output = Outputs[0];
	const UPCGParamData* OutParamData = Cast<UPCGParamData>(Output.Data);
	UTEST_NOT_NULL("Output is an attribute set data", OutParamData);
	check(OutParamData && OutParamData->Metadata);

	const FPCGMetadataAttribute<int32>* Attribute = OutParamData->Metadata->GetConstTypedAttribute<int32>(SortCommonTestData::AttributeName);
	UTEST_NOT_NULL("Output has the sorted attribute", Attribute);
	check(Attribute);

	for (int i = 0; i < OutParamData->Metadata->GetLocalItemCount() - 1; ++i)
	{
		const int32 Value = Attribute->GetValue(i);
		const int32 NextValue = Attribute->GetValue(i + 1);
		UTEST_TRUE(FString::Format(TEXT("{0} is greater than/equal to {1}"), { Value, NextValue }), Value >= NextValue);
	}

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGSortPointsTest_SameValues, FPCGTestBaseClass, "Plugins.PCG.SortAttributes.SameValues", PCGTestsCommon::TestFlags)

bool FPCGSortPointsTest_SameValues::RunTest(const FString& Parameters)
{
	TUniquePtr<FPCGContext> Context = SortCommonTestData::RunSortElementOnData(EPCGSortMethod::Ascending,
		PCGTestsCommon::CreateRandomPointData(100, 42, /*bRandomDensity=*/ false),
		FPCGAttributePropertySelector::CreatePointPropertySelector<FPCGAttributePropertyInputSelector>(EPCGPointProperties::Density));

	const TArray<FPCGTaggedData>& Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	// compare point data
	const FPCGTaggedData& Input = Inputs[0];
	const FPCGTaggedData& Output = Outputs[0];

	const UPCGBasePointData* InPointData = Cast<UPCGBasePointData>(Input.Data);
	const UPCGBasePointData* OutPointData = Cast<UPCGBasePointData>(Output.Data);

	UTEST_NOT_NULL("Output is a point data", OutPointData);
	check(InPointData && OutPointData);
		
	UTEST_EQUAL("Arrays have the same number of points:", InPointData->GetNumPoints(), OutPointData->GetNumPoints());

	const TConstPCGValueRange<int32> InSeedRange = InPointData->GetConstSeedValueRange();
	const TConstPCGValueRange<float> InDensityRange = InPointData->GetConstDensityValueRange();
	const TConstPCGValueRange<FTransform> InTransformRange = InPointData->GetConstTransformValueRange();

	const TConstPCGValueRange<int32> OutSeedRange = OutPointData->GetConstSeedValueRange();
	const TConstPCGValueRange<float> OutDensityRange = OutPointData->GetConstDensityValueRange();
	const TConstPCGValueRange<FTransform> OutTransformRange = OutPointData->GetConstTransformValueRange();

	for (int i = 0; i < InPointData->GetNumPoints(); ++i)
	{
		//if they're in the same spots after sorting, they should have exactly the same properties across the board
		UTEST_EQUAL(FString::Format(TEXT("UnsortedArray[{0}].Seed is equal to SortedArray[{0}].Seed"), {i}), OutSeedRange[i], InSeedRange[i]);
		UTEST_EQUAL(FString::Format(TEXT("UnsortedArray[{0}].Density is equal to SortedArray[{0}].Density"), {i}), OutDensityRange[i], InDensityRange[i]);
		UTEST_EQUAL(FString::Format(TEXT("UnsortedArray[{0}].Transform is equal to SortedArray[{0}].Transform"), {i}), OutTransformRange[i], InTransformRange[i]);
	}

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGSortAttributesTest_SameValues, FPCGTestBaseClass, "Plugins.PCG.SortAttributes.AttributeSet.SameValues", PCGTestsCommon::TestFlags)

bool FPCGSortAttributesTest_SameValues::RunTest(const FString& Parameters)
{
	const UPCGParamData* InputParamData = SortCommonTestData::GenerateParamData(100, 42, /*bRandom=*/false);
	check(InputParamData);

	TUniquePtr<FPCGContext> Context = SortCommonTestData::RunSortElementOnData(EPCGSortMethod::Ascending,
		InputParamData,
		FPCGAttributePropertySelector::CreateAttributeSelector<FPCGAttributePropertyInputSelector>(SortCommonTestData::AttributeName));

	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	const FPCGTaggedData& Output = Outputs[0];
	const UPCGParamData* OutParamData = Cast<UPCGParamData>(Output.Data);
	UTEST_NOT_NULL("Output is an attribute set data", OutParamData);
	check(OutParamData && OutParamData->Metadata);


	const FPCGMetadataAttribute<int32>* InAttribute = InputParamData->Metadata->GetConstTypedAttribute<int32>(SortCommonTestData::AttributeName);
	check(InAttribute);

	const FPCGMetadataAttribute<int32>* OutAttribute = OutParamData->Metadata->GetConstTypedAttribute<int32>(SortCommonTestData::AttributeName);
	UTEST_NOT_NULL("Output has the sorted attribute", OutAttribute);
	check(OutAttribute);

	for (int i = 0; i < OutParamData->Metadata->GetLocalItemCount() - 1; ++i)
	{
		const PCGMetadataValueKey InValueKey = InAttribute->GetValueKey(i);
		const PCGMetadataValueKey OutValueKey = OutAttribute->GetValueKey(i);
		UTEST_TRUE(FString::Format(TEXT("InAttribute and OutAttribute have the same value key for the same entry key ({0} vs {1})"), { InValueKey, OutValueKey }), InValueKey == OutValueKey);
	}

	return true;
}
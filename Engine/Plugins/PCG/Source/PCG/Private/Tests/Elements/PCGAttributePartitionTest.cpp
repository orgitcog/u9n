// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/PCGTestsCommon.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"

#include "Elements/Metadata/PCGMetadataPartition.h"

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGAttributePartition_Points, FPCGTestBaseClass, "Plugins.PCG.AttributePartition.Points", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGAttributePartition_AttributeSet, FPCGTestBaseClass, "Plugins.PCG.AttributePartition.AttributeSet", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGAttributePartition_Order, FPCGTestBaseClass, "Plugins.PCG.AttributePartition.Order", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGAttributePartition_MultiPartition, FPCGTestBaseClass, "Plugins.PCG.AttributePartition.MultiPartition", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGAttributePartition_MultiPartitionOverride, FPCGTestBaseClass, "Plugins.PCG.AttributePartition.MultiPartitionOverride", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGAttributePartition_WithPartitionIndex, FPCGTestBaseClass, "Plugins.PCG.AttributePartition.WithPartitionIndex", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGAttributePartition_NoPartitionWithPartitionIndex, FPCGTestBaseClass, "Plugins.PCG.AttributePartition.NoPartitionWithPartitionIndex", PCGTestsCommon::TestFlags)

namespace PCGAttributePartitionTest
{
	const static FName PartitionIndexAttributeName = TEXT("PartitionIndex");

	bool MultiPartitionTest(FPCGTestBaseClass& TestClass, bool bWithOverride)
	{
		PCGTestsCommon::FTestData TestData;
		UPCGMetadataPartitionSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGMetadataPartitionSettings>(TestData);
		check(Settings);

		if (!bWithOverride)
		{
			Settings->PartitionAttributeSelectors.Empty(2);
			Settings->PartitionAttributeSelectors.Emplace_GetRef().Update(TEXT("$Position.X"));
			Settings->PartitionAttributeSelectors.Emplace_GetRef().Update(TEXT("$Position.Y"));
		}
		else
		{
			UPCGParamData* OverrideParamData = NewObject<UPCGParamData>();
			FPCGMetadataAttribute<FString>* OverrideAttribute = OverrideParamData->Metadata->CreateAttribute<FString>(TEXT(""), TEXT("$Position.X,$Position.Y"), false, false);
			OverrideParamData->Metadata->AddEntry();

			FPCGTaggedData& OverrideTaggedData = TestData.InputData.TaggedData.Emplace_GetRef();
			OverrideTaggedData.Data = OverrideParamData;
			OverrideTaggedData.Pin = GET_MEMBER_NAME_CHECKED(UPCGMetadataPartitionSettings, PartitionAttributeNames);
		}

		UPCGBasePointData* InputPointData = PCGTestsCommon::CreateEmptyBasePointData();
		InputPointData->SetNumPoints(20);
		TPCGValueRange<FTransform> TransformRange = InputPointData->GetTransformValueRange();

		for (int32 i = 0; i < InputPointData->GetNumPoints(); ++i)
		{
			// Also add a very small number on Y, to verify that approximation errors are not giving different partitions.
			const double Jitter = i >= 10 ? UE_DOUBLE_SMALL_NUMBER : 0.0;
			TransformRange[i].SetLocation(FVector(i % 10, i % 5 + Jitter, 0.0));
		}

		FPCGTaggedData& InputTaggedData = TestData.InputData.TaggedData.Emplace_GetRef();
		InputTaggedData.Data = InputPointData;
		InputTaggedData.Pin = PCGPinConstants::DefaultInputLabel;

		FPCGElementPtr TestElement = TestData.Settings->GetElement();
		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

		while (!TestElement->Execute(Context.Get())) {}

		if (!TestClass.TestEqual(TEXT("There are 10 outputs"), Context->OutputData.TaggedData.Num(), 10))
		{
			return false;
		}

		for (int32 i = 0; i < Context->OutputData.TaggedData.Num(); ++i)
		{
			const UPCGBasePointData* OutputPointData = Cast<const UPCGBasePointData>(Context->OutputData.TaggedData[i].Data);
			if (!TestClass.TestNotNull(*FString::Printf(TEXT("Output %d is a point data"), i), OutputPointData))
			{
				return false;
			}

			if (!TestClass.TestEqual(*FString::Printf(TEXT("Output %d has 2 points"), i), OutputPointData->GetNumPoints(), 2))
			{
				return false;
			}

			FVector TempValue;
			bool bFirstPoint = true;
			bool bAllEquals = true;
			
			const TConstPCGValueRange<FTransform> OutTransformRange = OutputPointData->GetConstTransformValueRange();
			
			for (const FTransform& Transform : OutTransformRange)
			{
				if (bFirstPoint)
				{
					TempValue = Transform.GetLocation();
					bFirstPoint = false;
				}
				else
				{
					bAllEquals &= TempValue.Equals(Transform.GetLocation());
				}
			}

			if (!TestClass.TestTrue(*FString::Printf(TEXT("Output points for output %d have all the same position"), i), bAllEquals))
			{
				return false;
			}
		}

		return true;
	}

	bool PointPartitionTest(FPCGTestBaseClass& TestClass, bool bWithPartitionIndex, bool bNoPartition, auto VerifyFunction)
	{
		PCGTestsCommon::FTestData TestData;
		UPCGMetadataPartitionSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGMetadataPartitionSettings>(TestData);
		check(Settings);

		// By default there should be one selector with @Last attribute
		if (!TestClass.TestEqual(TEXT("There is one Partition Attribute Selector by default"), Settings->PartitionAttributeSelectors.Num(), 1))
		{
			return false;
		}

		Settings->PartitionAttributeSelectors[0].SetPointProperty(EPCGPointProperties::Density);
		Settings->bAssignIndexPartition = bWithPartitionIndex;
		Settings->bDoNotPartition = bNoPartition;
		Settings->PartitionIndexAttributeName = PartitionIndexAttributeName;

		UPCGBasePointData* InputPointData = PCGTestsCommon::CreateEmptyBasePointData();
		InputPointData->SetNumPoints(100);

		TPCGValueRange<float> DensityRange = InputPointData->GetDensityValueRange();
		for (int32 i = 0; i < InputPointData->GetNumPoints(); ++i)
		{
			DensityRange[i] = i % 10;
		}

		const int32 ExpectedPointsPerData = bNoPartition ? 100 : 10;
		const int32 ExpectedDataNum = bNoPartition ? 1 : 10;

		FPCGTaggedData& InputTaggedData = TestData.InputData.TaggedData.Emplace_GetRef();
		InputTaggedData.Data = InputPointData;
		InputTaggedData.Pin = PCGPinConstants::DefaultInputLabel;

		FPCGElementPtr TestElement = TestData.Settings->GetElement();
		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

		while (!TestElement->Execute(Context.Get())) {}

		if (!TestClass.TestEqual(TEXT("There are the right number outputs"), Context->OutputData.TaggedData.Num(), ExpectedDataNum))
		{
			return false;
		}

		for (int32 i = 0; i < Context->OutputData.TaggedData.Num(); ++i)
		{
			const UPCGBasePointData* OutputPointData = Cast<const UPCGBasePointData>(Context->OutputData.TaggedData[i].Data);
			if (!TestClass.TestNotNull(*FString::Printf(TEXT("Output %d is a point data"), i), OutputPointData))
			{
				return false;
			}

			if (!TestClass.TestEqual(*FString::Printf(TEXT("Output %d has the right number of points"), i), OutputPointData->GetNumPoints(), ExpectedPointsPerData))
			{
				return false;
			}

			if (!VerifyFunction(OutputPointData, i))
			{
				return false;
			}
		}

		return true;
	}
}

bool FPCGAttributePartition_Points::RunTest(const FString& Parameters)
{
	return PCGAttributePartitionTest::PointPartitionTest(*this, /*bWithPartitionIndex=*/ false, /*bNoPartition=*/ false, [this](const UPCGBasePointData* OutputPointData, int32 OutputIndex) -> bool
	{
		check(OutputPointData);
		float TempValue = -1.0f;
		bool bFirstPoint = true;
		bool bAllEquals = true;

		const TConstPCGValueRange<float> DensityRange = OutputPointData->GetConstDensityValueRange();

		for (float Density : DensityRange)
		{
			if (bFirstPoint)
			{
				TempValue = Density;
				bFirstPoint = false;
			}
			else
			{
				bAllEquals &= TempValue == Density;
			}
		}

		return TestTrue(*FString::Printf(TEXT("Output points for output %d have all the same density"), OutputIndex), bAllEquals);
	});
}

/**
* Test that the partitions are in the right order.
* Points in each partition should appear in the same order that they were in the original set.
* Partitions should be in the same order than the partition value appear in the original set.
* For example, if the original set was [(0, 4), (1, 4), (2, 2), (3, 2)], and we partition on the second value, the result should be
* [(0, 4), (1, 4)] and [(2, 2), (3, 2)]
*/
bool FPCGAttributePartition_Order::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGMetadataPartitionSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGMetadataPartitionSettings>(TestData);
	check(Settings);

	// By default there should be one selector with @Last attribute
	UTEST_EQUAL("There is one Partition Attribute Selector by default", Settings->PartitionAttributeSelectors.Num(), 1);

	const FName AttributeName = TEXT("Attr");
	Settings->PartitionAttributeSelectors[0].SetAttributeName(AttributeName);

	UPCGBasePointData* InputPointData = PCGTestsCommon::CreateEmptyBasePointData();
	FPCGMetadataAttribute<int>* Attribute = InputPointData->Metadata->CreateAttribute<int>(AttributeName, 0, false, false);

	constexpr int NumPoints = 10;

	InputPointData->SetNumPoints(NumPoints);
	
	TPCGValueRange<float> DensityRange = InputPointData->GetDensityValueRange();
	TPCGValueRange<int64> MetadataEntryRange = InputPointData->GetMetadataEntryValueRange();

	for (int32 i = 0; i < NumPoints; ++i)
	{
		// Store the index in the density
		DensityRange[i] = i;
		InputPointData->Metadata->InitializeOnSet(MetadataEntryRange[i]);
		Attribute->SetValue(MetadataEntryRange[i], (2 * i) / NumPoints);
	}

	// Reverse the points order
	for (int32 i = 0; i < NumPoints / 2; ++i)
	{
		std::swap(DensityRange[i], DensityRange[NumPoints - i - 1]);
		std::swap(MetadataEntryRange[i], MetadataEntryRange[NumPoints - i - 1]);
	}

	FPCGTaggedData& InputTaggedData = TestData.InputData.TaggedData.Emplace_GetRef();
	InputTaggedData.Data = InputPointData;
	InputTaggedData.Pin = PCGPinConstants::DefaultInputLabel;

	FPCGElementPtr TestElement = TestData.Settings->GetElement();
	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!TestElement->Execute(Context.Get())) {}

	UTEST_EQUAL("There are 2 outputs", Context->OutputData.TaggedData.Num(), 2);

	for (int32 i = 0; i < Context->OutputData.TaggedData.Num(); ++i)
	{
		const UPCGBasePointData* OutputPointData = Cast<const UPCGBasePointData>(Context->OutputData.TaggedData[i].Data);
		UTEST_NOT_NULL(*FString::Printf(TEXT("Output %d is a point data"), i), OutputPointData);
		check(OutputPointData);

		const FPCGMetadataAttribute<int>* OutputAttribute = OutputPointData->Metadata->GetConstTypedAttribute<int>(AttributeName);

		UTEST_NOT_NULL(*FString::Printf(TEXT("Output %d has the expected attribute"), i), OutputAttribute);
		check(OutputAttribute);

		UTEST_EQUAL(*FString::Printf(TEXT("Output %d has %d points"), i, NumPoints / 2), OutputPointData->GetNumPoints(), NumPoints / 2);

		const TConstPCGValueRange<float> OutDensityRange = OutputPointData->GetConstDensityValueRange();
		const TConstPCGValueRange<int64> OutMetadataEntryRange = OutputPointData->GetConstMetadataEntryValueRange();

		for (int32 j = 0; j < OutputPointData->GetNumPoints() - 1; ++j)
		{
			UTEST_TRUE(*FString::Printf(TEXT("Output %d: Point_%d is at the right place"), i, j), OutDensityRange[j] > OutDensityRange[j + 1]);
			// First partition should have the value 1, and second partition should have the value 0
			UTEST_EQUAL(*FString::Printf(TEXT("Output %d: Point_%d has the right attribute value"), i, j), OutputAttribute->GetValue(OutMetadataEntryRange[j]), (i + 1) % 2);
		}
	}

	return true;
}

bool FPCGAttributePartition_AttributeSet::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGMetadataPartitionSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGMetadataPartitionSettings>(TestData);
	check(Settings);

	// By default there should be one selector with @Last attribute
	UTEST_EQUAL("There is one Partition Attribute Selector by default", Settings->PartitionAttributeSelectors.Num(), 1);

	const FName InputAttributeName = TEXT("Double");
	Settings->PartitionAttributeSelectors[0].SetAttributeName(InputAttributeName);

	UPCGParamData* InputParam = NewObject<UPCGParamData>();
	FPCGMetadataAttribute<double>* Attribute = InputParam->Metadata->CreateAttribute<double>(InputAttributeName, 0.0, true, false);
	for (int32 i = 0; i < 100; ++i)
	{
		Attribute->SetValue(InputParam->Metadata->AddEntry(), (i % 10) / 10.0);
	}

	FPCGTaggedData& InputTaggedData = TestData.InputData.TaggedData.Emplace_GetRef();
	InputTaggedData.Data = InputParam;
	InputTaggedData.Pin = PCGPinConstants::DefaultInputLabel;

	FPCGElementPtr TestElement = TestData.Settings->GetElement();
	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!TestElement->Execute(Context.Get())) {}

	UTEST_EQUAL("There are 10 outputs", Context->OutputData.TaggedData.Num(), 10);

	for (int32 i = 0; i < Context->OutputData.TaggedData.Num(); ++i)
	{
		const UPCGParamData* OutputParamData = Cast<const UPCGParamData>(Context->OutputData.TaggedData[i].Data);
		UTEST_NOT_NULL(*FString::Printf(TEXT("Output %d is a param data"), i), OutputParamData);

		UTEST_EQUAL(*FString::Printf(TEXT("Output %d has 10 entries"), i), OutputParamData->Metadata->GetLocalItemCount(), 10ll);
		const FPCGMetadataAttribute<double>* OutAttribute = OutputParamData->Metadata->GetConstTypedAttribute<double>(InputAttributeName);
		UTEST_NOT_NULL(*FString::Printf(TEXT("Output %d has the 'Double' attribute"), i), OutAttribute);


		double TempValue = -1.0;
		bool bFirstItem = true;
		bool bAllEquals = true;
		for (PCGMetadataEntryKey Key = 0; Key < OutputParamData->Metadata->GetLocalItemCount(); ++Key)
		{
			if (bFirstItem)
			{
				TempValue = OutAttribute->GetValueFromItemKey(Key);
				bFirstItem = false;
			}
			else
			{
				bAllEquals &= TempValue == OutAttribute->GetValueFromItemKey(Key);
			}
		}

		UTEST_TRUE(*FString::Printf(TEXT("Output values for output %d are all the same"), i), bAllEquals);
	}

	return true;
}

bool FPCGAttributePartition_MultiPartition::RunTest(const FString& Parameters)
{
	return PCGAttributePartitionTest::MultiPartitionTest(*this, /*bWithOverride=*/false);
}

bool FPCGAttributePartition_MultiPartitionOverride::RunTest(const FString& Parameters)
{
	return PCGAttributePartitionTest::MultiPartitionTest(*this, /*bWithOverride=*/true);
}

bool FPCGAttributePartition_WithPartitionIndex::RunTest(const FString& Parameters)
{
	return PCGAttributePartitionTest::PointPartitionTest(*this, /*bWithPartitionIndex=*/ true, /*bNoPartition=*/ false, [this](const UPCGBasePointData* OutputPointData, int32 OutputIndex) -> bool
		{
			check(OutputPointData && OutputPointData->Metadata);
			const FPCGMetadataAttribute<int32>* PartitionIndexAttribute = OutputPointData->Metadata->GetConstTypedAttribute<int32>(PCGAttributePartitionTest::PartitionIndexAttributeName);

			UTEST_NOT_NULL("Partition Index attribute exists", PartitionIndexAttribute);

			float TempValue = -1.0f;
			bool bFirstPoint = true;
			bool bAllEquals = true;

			const TConstPCGValueRange<float> DensityRange = OutputPointData->GetConstDensityValueRange();
			const TConstPCGValueRange<int64> MetadataEntryRange = OutputPointData->GetConstMetadataEntryValueRange();

			for (int32 Index = 0; Index < OutputPointData->GetNumPoints(); ++Index)
			{
				if (bFirstPoint)
				{
					TempValue = DensityRange[Index];
					bFirstPoint = false;
				}
				else
				{
					bAllEquals &= TempValue == DensityRange[Index];
				}

				UTEST_EQUAL("Partition index value is equal to output index", PartitionIndexAttribute->GetValueFromItemKey(MetadataEntryRange[Index]), OutputIndex);
			}

			return TestTrue(*FString::Printf(TEXT("Output points for output %d have all the same density"), OutputIndex), bAllEquals);
		});
}

bool FPCGAttributePartition_NoPartitionWithPartitionIndex::RunTest(const FString& Parameters)
{
	return PCGAttributePartitionTest::PointPartitionTest(*this, /*bWithPartitionIndex=*/ true, /*bNoPartition=*/ true, [this](const UPCGBasePointData* OutputPointData, int32 OutputIndex) -> bool
		{
			check(OutputPointData && OutputPointData->Metadata);
			const FPCGMetadataAttribute<int32>* PartitionIndexAttribute = OutputPointData->Metadata->GetConstTypedAttribute<int32>(PCGAttributePartitionTest::PartitionIndexAttributeName);

			UTEST_NOT_NULL("Partition Index attribute exists", PartitionIndexAttribute);

			const TConstPCGValueRange<float> DensityRange = OutputPointData->GetConstDensityValueRange();
			const TConstPCGValueRange<int64> MetadataEntryRange = OutputPointData->GetConstMetadataEntryValueRange();

			for (int32 Index = 0; Index < OutputPointData->GetNumPoints(); ++Index)
			{
				const int32 PartitionIndex = FMath::FloorToInt32(DensityRange[Index]);
				UTEST_EQUAL("Partition index has the right value", PartitionIndexAttribute->GetValueFromItemKey(MetadataEntryRange[Index]), PartitionIndex);
			}

			return true;
		});
}
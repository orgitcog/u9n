// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "Tests/PCGTestsCommon.h"

#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGMetadataFlatten, FPCGTestBaseClass, "Plugins.PCG.Metadata.Flatten", PCGTestsCommon::TestFlags)

/**
* Series of operations to manipulate points and attributes, to validate the flatten operation.
*/
bool FPCGMetadataFlatten::RunTest(const FString& Parameters)
{
	static const FName Attribute1Name = TEXT("FloatAttr");
	static const FName Attribute2Name = TEXT("StringAttr");
	static const FName Attribute3Name = TEXT("IntAttr");

	UPCGBasePointData* RootPointData = PCGTestsCommon::CreateEmptyBasePointData();
	FPCGMetadataAttribute<float>* Attribute1 = RootPointData->Metadata->CreateAttribute<float>(Attribute1Name, -0.1f, true, true);
	FPCGMetadataAttribute<FString>* Attribute2 = RootPointData->Metadata->CreateAttribute<FString>(Attribute2Name, TEXT("Default"), true, true);

	check(Attribute1 && Attribute2);
	
	RootPointData->SetNumPoints(10);

	TPCGValueRange<int64> MetadataEntryRange = RootPointData->GetMetadataEntryValueRange();
	for (int32 i = 0; i < RootPointData->GetNumPoints(); ++i)
	{
		RootPointData->Metadata->InitializeOnSet(MetadataEntryRange[i]);
		Attribute1->SetValue(MetadataEntryRange[i], static_cast<float>(i) * 0.1f);

		if (i % 2 == 0)
		{
			// Will be either "0" or "1"
			Attribute2->SetValue(MetadataEntryRange[i], FString::Printf(TEXT("%d"), i % 4));
		}
	}

	// At the end of the first set, metadata has 10 entries, and values for each points are
	// for Attribute 1: [0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9]
	// for Attribute 2: ["0", "Default", "2", "Default", "0", ...]
	UTEST_EQUAL("RootMetadata has 10 entries", RootPointData->Metadata->GetItemCountForChild(), 10);

	UPCGBasePointData* FirstChildPointData = Cast<UPCGBasePointData>(RootPointData->DuplicateData(nullptr));
	Attribute1 = FirstChildPointData->Metadata->GetMutableTypedAttribute<float>(Attribute1Name);
	Attribute2 = FirstChildPointData->Metadata->GetMutableTypedAttribute<FString>(Attribute2Name);

	UTEST_TRUE("Attributes exists in first child", Attribute1 && Attribute2);
	check(Attribute1 && Attribute2);

	// Override all the values for Attribute 1, and replace all "1" by "0" in attribute 2
	// Also add a third attribute
	FPCGMetadataAttribute<int>* Attribute3 = FirstChildPointData->Metadata->CreateAttribute<int>(Attribute3Name, -1, true, true);
	check(Attribute3);

	UTEST_EQUAL("First child has 10 points", FirstChildPointData->GetNumPoints(), 10);

	TPCGValueRange<int64> FirstChildMetadataEntryRange = FirstChildPointData->GetMetadataEntryValueRange();
	for (int32 i = 0; i < FirstChildPointData->GetNumPoints(); ++i)
	{
		FirstChildPointData->Metadata->InitializeOnSet(FirstChildMetadataEntryRange[i]);
		Attribute1->SetValue(FirstChildMetadataEntryRange[i], static_cast<float>(i) * 1.1f);

		if (i % 2 == 0)
		{
			Attribute2->SetValue(FirstChildMetadataEntryRange[i], TEXT("0"));
		}

		Attribute3->SetValue(FirstChildMetadataEntryRange[i], i);
	}

	// At the end of the second set, metadata has 20 entries, and values for each points are
	// for Attribute 1: [0.0, 1.1, 2.2, 3.3, 4.4, 5.5, 6.6, 7.7, 8.8, 9.9]
	// for Attribute 2: ["0", "Default", "0", "Default", "0", ...]
	// for Attribute 3: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
	UTEST_EQUAL("FirstChildMetadata has 20 entries", FirstChildPointData->Metadata->GetItemCountForChild(), 20);

	// Second child is keeping the metadata entry for even numbers and reset metadata entry for the rest (point back to default). Also override Attribute 3 values for even numbers
	UPCGBasePointData* SecondChildPointData = PCGTestsCommon::CreateEmptyBasePointData();

	FPCGInitializeFromDataParams InitializeFromDataParams(FirstChildPointData);
	InitializeFromDataParams.bInheritSpatialData = false;
	SecondChildPointData->InitializeFromDataWithParams(InitializeFromDataParams);
	
	Attribute3 = SecondChildPointData->Metadata->GetMutableTypedAttribute<int>(Attribute3Name);
	UTEST_NOT_NULL("Attributes exists in first child", Attribute3);
	check(Attribute3);

	SecondChildPointData->SetNumPoints(FirstChildPointData->GetNumPoints());
	UPCGBasePointData::SetPoints(FirstChildPointData, SecondChildPointData, {}, /*bCopyAll=*/true);

	TPCGValueRange<int64> SecondChildMetadataEntryRange = SecondChildPointData->GetMetadataEntryValueRange();
	for (int32 i = 0; i < SecondChildPointData->GetNumPoints(); ++i)
	{
		if (i % 2 == 0)
		{
			SecondChildPointData->Metadata->InitializeOnSet(SecondChildMetadataEntryRange[i]);
			Attribute3->SetValue(SecondChildMetadataEntryRange[i], 10 * i);
		}
		else
		{
			SecondChildMetadataEntryRange[i] = PCGInvalidEntryKey;
		}
	}

	// At the end of the third set, metadata has 25 entries, and values for each points are:
	// for Attribute 1: [0.0, -0.1, 2.2, -0.1, 4.4, -0.1, 6.6, -0.1, 8.8, -0.1]
	// for Attribute 2: ["0", "Default", "0", "Default", "0", ...]
	// for Attribute 3: [0, -1, 20, -1, 40, -1, 60, -1, 80, -1]
	UTEST_EQUAL("SecondChildMetadata has 25 entries", SecondChildPointData->Metadata->GetItemCountForChild(), 25);

	// For final set, duplicate the data and flatten it
	UPCGBasePointData* FinalPointData = Cast<UPCGBasePointData>(SecondChildPointData->DuplicateData(nullptr));
	FinalPointData->Flatten();

	Attribute1 = FinalPointData->Metadata->GetMutableTypedAttribute<float>(Attribute1Name);
	Attribute2 = FinalPointData->Metadata->GetMutableTypedAttribute<FString>(Attribute2Name);
	Attribute3 = FinalPointData->Metadata->GetMutableTypedAttribute<int>(Attribute3Name);

	UTEST_TRUE("Attributes exists in final child", Attribute1 && Attribute2 && Attribute3);
	check(Attribute1 && Attribute2 && Attribute3);

	UTEST_EQUAL("Final metadata has 5 entries", FinalPointData->Metadata->GetItemCountForChild(), 5);
	UTEST_EQUAL("Attribute 1 has 5 values", Attribute1->GetValueKeyOffsetForChild(), 5);
	UTEST_EQUAL("Attribute 2 has 1 value", Attribute2->GetValueKeyOffsetForChild(), 1);
	UTEST_EQUAL("Attribute 3 has 5 values", Attribute3->GetValueKeyOffsetForChild(), 5);
	UTEST_EQUAL("Final point data has 10 points", FinalPointData->GetNumPoints(), 10);

	// Validate the values
	const TConstPCGValueRange<int64> FinalMetadataEntryRange = FinalPointData->GetConstMetadataEntryValueRange();
	for (int32 i = 0; i < FinalPointData->GetNumPoints(); ++i)
	{
		auto FormatMessage = [i](const FString& Msg) { return FString::Printf(TEXT("Point %d: %s"), i, *Msg); };

		const int64 MetadataEntry = FinalMetadataEntryRange[i];
		if (i % 2 == 0)
		{
			UTEST_EQUAL(*FormatMessage(TEXT("Valid metadata entry")), MetadataEntry, static_cast<PCGMetadataEntryKey>(i / 2));
			UTEST_EQUAL(*FormatMessage(TEXT("Attribute 1 value")), Attribute1->GetValueFromItemKey(MetadataEntry), i * 1.1f);
			UTEST_EQUAL(*FormatMessage(TEXT("Attribute 2 value")), Attribute2->GetValueFromItemKey(MetadataEntry), TEXT("0"));
			UTEST_EQUAL(*FormatMessage(TEXT("Attribute 3 value")), Attribute3->GetValueFromItemKey(MetadataEntry), i * 10);
		}
		else
		{
			UTEST_EQUAL(*FormatMessage(TEXT("Invalid metadata entry")), MetadataEntry, PCGInvalidEntryKey);
			UTEST_EQUAL(*FormatMessage(TEXT("Attribute 1 value")), Attribute1->GetValueFromItemKey(MetadataEntry), -0.1f);
			UTEST_EQUAL(*FormatMessage(TEXT("Attribute 2 value")), Attribute2->GetValueFromItemKey(MetadataEntry), TEXT("Default"));
			UTEST_EQUAL(*FormatMessage(TEXT("Attribute 3 value")), Attribute3->GetValueFromItemKey(MetadataEntry), -1);
		}
	}

	return true;
}
#endif // WITH_EDITOR
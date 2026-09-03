// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGParamData.h"
#if WITH_EDITOR

#include "Tests/PCGTestsCommon.h"

#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

#include "Engine/Engine.h"

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGMetadataTest_Inherit, FPCGTestBaseClass, "Plugins.PCG.Metadata.Inherit", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGMetadataTest_InheritCopy, FPCGTestBaseClass, "Plugins.PCG.Metadata.InheritCopy", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGMetadataTest_InheritWithNoParenting, FPCGTestBaseClass, "Plugins.PCG.Metadata.InheritWithNoParenting", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGMetadataTest_TwoAddEntryToData, FPCGTestBaseClass, "Plugins.PCG.Metadata.TwoAddEntryToData", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGMetadataTest_AddEntriesToData, FPCGTestBaseClass, "Plugins.PCG.Metadata.AddEntriesToData", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGMetadataTest_AddEntriesInPlaceToData, FPCGTestBaseClass, "Plugins.PCG.Metadata.AddEntriesToData", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGMetadataTest_DelayedAddEntriesToData, FPCGTestBaseClass, "Plugins.PCG.Metadata.DelayedAddEntriesToData", PCGTestsCommon::TestFlags)

bool FPCGMetadataTest_Inherit::RunTest(const FString& Parameters)
{
	const FName AttributeName = TEXT("MyAttr");
	
	UPCGBasePointData* PointData3 = nullptr;

	{
		TObjectPtr<UPCGBasePointData> PointData = PCGTestsCommon::CreateEmptyBasePointData();
		PointData->SetNumPoints(5);
		UPCGMetadata* PointMetadata = PointData->MutableMetadata();
		FPCGMetadataAttribute<int32>* Attribute = PointMetadata->CreateAttribute<int32>(AttributeName, 0, true, false);
	
		TPCGValueRange<int64> MetadataEntryRange = PointData->GetMetadataEntryValueRange();
		for (int32 i = 0 ; i < PointData->GetNumPoints(); ++i)
		{
			PointMetadata->InitializeOnSet(MetadataEntryRange[i]);
			Attribute->SetValue(MetadataEntryRange[i], i);
		}
	
		TObjectPtr<UPCGBasePointData> PointData2 = PCGTestsCommon::CreateEmptyBasePointData();
		UPCGMetadata* PointMetadata2 = PointData2->MutableMetadata();
		PointMetadata2->Initialize(PointMetadata);

		// Verify that the initialization inherited
		UTEST_EQUAL("Initialization inherited", PointMetadata2->GetLocalItemCount(), 0);

		FPCGMetadataAttribute<int32>* Attribute2 = PointMetadata2->GetMutableTypedAttribute<int32>(AttributeName);

		UPCGBasePointData::SetPoints(PointData, PointData2, {}, /*bCopyAll=*/true);
		PointData2->SetNumPoints(10);
	
		TPCGValueRange<int64> MetadataEntryRange2 = PointData2->GetMetadataEntryValueRange();
		for (int32 i = PointData->GetNumPoints() ; i < PointData2->GetNumPoints(); ++i)
		{
			PointMetadata2->InitializeOnSet(MetadataEntryRange2[i]);
			Attribute2->SetValue(MetadataEntryRange2[i], i);
		}
		
		PointData3 = PCGTestsCommon::CreateEmptyBasePointData();
		UPCGMetadata* PointMetadata3 = PointData3->MutableMetadata();
		PointMetadata3->Initialize(PointMetadata2);

		UTEST_EQUAL("Initialization inherited", PointMetadata3->GetLocalItemCount(), 0);

		UPCGBasePointData::SetPoints(PointData2, PointData3, {}, /*bCopyAll=*/true);

		UTEST_EQUAL("Same number of points", PointData2->GetNumPoints(), PointData3->GetNumPoints());
	}

	// Force a GC run
	if (ensure(GEngine))
	{
		GEngine->ForceGarbageCollection(/*bFullPurge=*/true);
	}

	// Verify that all the values are readable from the metadata
	check(PointData3);
	const FPCGMetadataAttribute<int32>* Attribute = PointData3->ConstMetadata()->GetConstTypedAttribute<int32>(AttributeName);
	UTEST_NOT_NULL("Attribute exists", Attribute);
	check(Attribute);

	TConstPCGValueRange<int64> MetadataEntryRange3 = PointData3->GetConstMetadataEntryValueRange();
	for (int32 i = 0; i < PointData3->GetNumPoints(); ++i)
	{
		UTEST_EQUAL("Values are the same", Attribute->GetValueFromItemKey(MetadataEntryRange3[i]), i);
	}
	
	return true;
}

bool FPCGMetadataTest_InheritCopy::RunTest(const FString& Parameters)
{
	const FName AttributeName = TEXT("MyAttr");
	
	UPCGBasePointData* PointData3 = nullptr;

	{
		TObjectPtr<UPCGBasePointData> PointData = PCGTestsCommon::CreateEmptyBasePointData();
		PointData->SetNumPoints(5);
		
		UPCGMetadata* PointMetadata = PointData->MutableMetadata();
		FPCGMetadataAttribute<int32>* Attribute = PointMetadata->CreateAttribute<int32>(AttributeName, 0, true, false);
	
		TPCGValueRange<int64> MetadataEntryRange = PointData->GetMetadataEntryValueRange();
		for (int32 i = 0 ; i < PointData->GetNumPoints(); ++i)
		{
			PointMetadata->InitializeOnSet(MetadataEntryRange[i]);
			Attribute->SetValue(MetadataEntryRange[i], i);
		}
	
		TObjectPtr<UPCGBasePointData> PointData2 = PCGTestsCommon::CreateEmptyBasePointData();
		UPCGMetadata* PointMetadata2 = PointData2->MutableMetadata();
		PointMetadata2->Initialize(PointMetadata);

		// Verify that the initialization inherited
		UTEST_EQUAL("Initialization inherited", PointMetadata2->GetLocalItemCount(), 0);

		FPCGMetadataAttribute<int32>* Attribute2 = PointMetadata2->GetMutableTypedAttribute<int32>(AttributeName);

		UPCGBasePointData::SetPoints(PointData, PointData2, {}, /*bCopyAll=*/true);
		PointData2->SetNumPoints(10);
	
		TPCGValueRange<int64> MetadataEntryRange2 = PointData2->GetMetadataEntryValueRange();
		for (int32 i = PointData->GetNumPoints(); i < PointData2->GetNumPoints(); ++i)
		{
			PointMetadata2->InitializeOnSet(MetadataEntryRange2[i]);
			Attribute2->SetValue(MetadataEntryRange2[i], i);
		}
		
		PointData3 = PCGTestsCommon::CreateEmptyBasePointData();
		UPCGMetadata* PointMetadata3 = PointData3->MutableMetadata();
		PointMetadata3->InitializeAsCopy(FPCGMetadataInitializeParams(PointMetadata2));

		// Verify that the initialization made a copy
		UTEST_EQUAL("Initialization made a copy", PointMetadata3->GetLocalItemCount(), 5);
		
		UPCGBasePointData::SetPoints(PointData2, PointData3, {}, /*bCopyAll=*/true);

		UTEST_EQUAL("Same number of points", PointData2->GetNumPoints(), PointData3->GetNumPoints());
	}

	// Force a GC run
	if (ensure(GEngine))
	{
		GEngine->ForceGarbageCollection(/*bFullPurge=*/true);
	}

	// Verify that all the values are readable from the metadata
	check(PointData3);
	const FPCGMetadataAttribute<int32>* Attribute = PointData3->ConstMetadata()->GetConstTypedAttribute<int32>(AttributeName);
	UTEST_NOT_NULL("Attribute exists", Attribute);
	check(Attribute);

	TConstPCGValueRange<int64> MetadataEntryRange3 = PointData3->GetConstMetadataEntryValueRange();
	for (int32 i = 0; i < PointData3->GetNumPoints(); ++i)
	{
		UTEST_EQUAL("Values are the same", Attribute->GetValueFromItemKey(MetadataEntryRange3[i]), i);
	}
	
	return true;
}

bool FPCGMetadataTest_InheritWithNoParenting::RunTest(const FString& Parameters)
{
	const FName AttributeName = TEXT("MyAttr");
	
	UPCGParamData* ParamData3 = nullptr;

	{
		TObjectPtr<UPCGParamData> ParamData = NewObject<UPCGParamData>();
		UPCGMetadata* ParamMetadata = ParamData->MutableMetadata();
		FPCGMetadataAttribute<int32>* Attribute = ParamMetadata->CreateAttribute<int32>(AttributeName, 0, true, false);
	
		for (int32 i = 0 ; i < 5; ++i)
		{
			PCGMetadataEntryKey EntryKey = PCGInvalidEntryKey;
			ParamMetadata->InitializeOnSet(EntryKey);
			Attribute->SetValue(EntryKey, i);
		}
	
		TObjectPtr<UPCGParamData> ParamData2 = NewObject<UPCGParamData>();
		UPCGMetadata* ParamMetadata2 = ParamData2->MutableMetadata();
		ParamMetadata2->Initialize(ParamMetadata);

		// Verify that the initialization made a copy
		UTEST_EQUAL("Initialization made a copy", ParamMetadata2->GetLocalItemCount(), 5);

		FPCGMetadataAttribute<int32>* Attribute2 = ParamMetadata2->GetMutableTypedAttribute<int32>(AttributeName);
	
		for (int32 i = 5 ; i < 10; ++i)
		{
			PCGMetadataEntryKey EntryKey = PCGInvalidEntryKey;
			ParamMetadata2->InitializeOnSet(EntryKey);
			Attribute2->SetValue(EntryKey, i);
		}
		
		ParamData3 = NewObject<UPCGParamData>();
		UPCGMetadata* ParamMetadata3 = ParamData3->MutableMetadata();
		ParamMetadata3->Initialize(ParamMetadata2);

		// Verify that the initialization made a copy
		UTEST_EQUAL("Initialization made a copy", ParamMetadata3->GetLocalItemCount(), 10);
	}

	// Force a GC run
	if (ensure(GEngine))
	{
		GEngine->ForceGarbageCollection(/*bFullPurge=*/true);
	}

	// Verify that all the values are readable from the metadata
	check(ParamData3);
	const FPCGMetadataAttribute<int32>* Attribute = ParamData3->ConstMetadata()->GetConstTypedAttribute<int32>(AttributeName);
	UTEST_NOT_NULL("Attribute exists", Attribute);
	check(Attribute);
	
	for (int32 i = 0; i < 10; ++i)
	{
		UTEST_EQUAL("Values are the same", Attribute->GetValueFromItemKey(i), i);
	}
	
	return true;
}

bool FPCGMetadataTest_TwoAddEntryToData::RunTest(const FString& Parameters)
{
	const FName AttributeName = TEXT("MyAttr");
	
	TObjectPtr<UPCGParamData> ParamData = NewObject<UPCGParamData>();
	UPCGMetadata* ParamMetadata = ParamData->MutableMetadata();
	FPCGMetadataDomain* ParamMetadataDomain = ParamMetadata->GetMetadataDomain(PCGMetadataDomainID::Data);
	FPCGMetadataAttribute<int32>* Attribute = ParamMetadataDomain->CreateAttribute<int32>(AttributeName, 0, false, false);

	AddExpectedError(TEXT("Try to add an entry to a domain (Data) that doesn't support multi entries. Will always return 0."), EAutomationExpectedErrorFlags::MatchType::Exact, 1, /*bIsRegex=*/false);

	for (int32 i = 0 ; i < 2; ++i)
	{
		PCGMetadataEntryKey EntryKey = PCGInvalidEntryKey;
		ParamMetadataDomain->InitializeOnSet(EntryKey);
		Attribute->SetValue(EntryKey, i);
	}

	UTEST_EQUAL("Just have a single entry", ParamMetadataDomain->GetItemCountForChild(), 1);
	UTEST_EQUAL("Entry has the latest value", Attribute->GetValueFromItemKey(PCGFirstEntryKey), 1);
	
	return true;
}

bool FPCGMetadataTest_AddEntriesToData::RunTest(const FString& Parameters)
{
	const FName AttributeName = TEXT("MyAttr");
	
	TObjectPtr<UPCGParamData> ParamData = NewObject<UPCGParamData>();
	UPCGMetadata* ParamMetadata = ParamData->MutableMetadata();
	FPCGMetadataDomain* ParamMetadataDomain = ParamMetadata->GetMetadataDomain(PCGMetadataDomainID::Data);
	FPCGMetadataAttribute<int32>* Attribute = ParamMetadataDomain->CreateAttribute<int32>(AttributeName, 0, false, false);

	AddExpectedError(TEXT("Try to add multiple entries to a metadata domain that don't support it (Data). Will always return 0."), EAutomationExpectedErrorFlags::MatchType::Exact, 1, /*bIsRegex=*/false);

	TStaticArray<PCGMetadataEntryKey, 2> ParentKeys = {PCGInvalidEntryKey, PCGInvalidEntryKey};
	TArray<PCGMetadataEntryKey> NewKeys = ParamMetadataDomain->AddEntries(ParentKeys);
	TStaticArray<int32, 2> Values = {5, 6};
	Attribute->SetValues(NewKeys, Values);

	UTEST_EQUAL("Just have a single entry", ParamMetadataDomain->GetItemCountForChild(), 1);
	UTEST_EQUAL("Entry has the latest value", Attribute->GetValueFromItemKey(PCGFirstEntryKey), Values[1]);
	
	return true;
}

bool FPCGMetadataTest_AddEntriesInPlaceToData::RunTest(const FString& Parameters)
{
	const FName AttributeName = TEXT("MyAttr");
	
	TObjectPtr<UPCGParamData> ParamData = NewObject<UPCGParamData>();
	UPCGMetadata* ParamMetadata = ParamData->MutableMetadata();
	FPCGMetadataDomain* ParamMetadataDomain = ParamMetadata->GetMetadataDomain(PCGMetadataDomainID::Data);
	FPCGMetadataAttribute<int32>* Attribute = ParamMetadataDomain->CreateAttribute<int32>(AttributeName, 0, false, false);

	AddExpectedError(TEXT("Try to add multiple entries to a metadata domain that don't support it (Data). Will always return 0."), EAutomationExpectedErrorFlags::MatchType::Exact, 1, /*bIsRegex=*/false);

	TStaticArray<PCGMetadataEntryKey, 2> Keys = {PCGInvalidEntryKey, PCGInvalidEntryKey};
	TStaticArray<PCGMetadataEntryKey*, 2> KeysPtr = {&Keys[0], &Keys[1]};
	
	ParamMetadataDomain->AddEntriesInPlace(KeysPtr);
	TStaticArray<int32, 2> Values = {5, 6};
	Attribute->SetValues(Keys, Values);

	UTEST_EQUAL("Just have a single entry", ParamMetadataDomain->GetItemCountForChild(), 1);
	UTEST_EQUAL("Entry has the latest value", Attribute->GetValueFromItemKey(PCGFirstEntryKey), Values[1]);
	
	return true;
}

bool FPCGMetadataTest_DelayedAddEntriesToData::RunTest(const FString& Parameters)
{
	const FName AttributeName = TEXT("MyAttr");
	
	TObjectPtr<UPCGParamData> ParamData = NewObject<UPCGParamData>();
	UPCGMetadata* ParamMetadata = ParamData->MutableMetadata();
	FPCGMetadataDomain* ParamMetadataDomain = ParamMetadata->GetMetadataDomain(PCGMetadataDomainID::Data);
	FPCGMetadataAttribute<int32>* Attribute = ParamMetadataDomain->CreateAttribute<int32>(AttributeName, 0, false, false);

	AddExpectedError(TEXT("Try to add an entry to a domain (Data) that doesn't support multi entries. Will always return 0."), EAutomationExpectedErrorFlags::MatchType::Exact, 1, /*bIsRegex=*/false);

	TArray<TTuple<PCGMetadataEntryKey, PCGMetadataEntryKey>> Mapping;
	TStaticArray<PCGMetadataEntryKey, 2> Keys;

	Keys[0] = ParamMetadataDomain->AddEntryPlaceholder();
	Mapping.Emplace(PCGInvalidEntryKey, Keys[0]);
	Keys[1] = ParamMetadataDomain->AddEntryPlaceholder();
	Mapping.Emplace(PCGInvalidEntryKey, Keys[1]);

	TStaticArray<int32, 2> Values = {5, 6};
	Attribute->SetValues(Keys, Values);
	
	AddExpectedError(TEXT("Try to add multiple entries to a metadata domain that don't support it (Data). Will always return 0."), EAutomationExpectedErrorFlags::MatchType::Exact, 1, /*bIsRegex=*/false);
	ParamMetadataDomain->AddDelayedEntries(Mapping);

	UTEST_EQUAL("Just have a single entry", ParamMetadataDomain->GetItemCountForChild(), 1);
	UTEST_EQUAL("Entry has the latest value", Attribute->GetValueFromItemKey(PCGFirstEntryKey), Values[1]);
	
	return true;
}

#endif // WITH_EDITOR
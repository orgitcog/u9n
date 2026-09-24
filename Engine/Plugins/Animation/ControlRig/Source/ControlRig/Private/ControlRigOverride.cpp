// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigOverride.h"
#include "ControlRig.h"
#include "RigVMCore/RigVMMemoryCommon.h"

#if WITH_EDITOR
#include "PropertyPath.h"
#include "AssetToolsModule.h"
#include "Misc/TransactionObjectEvent.h"
#endif
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigOverride)

TAutoConsoleVariable<bool> CVarControlRigEnableOverrides(TEXT("ControlRig.Overrides"), false, TEXT("Enables overrides for use in Control Rig"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
{
	for (TObjectIterator<UControlRigOverrideAsset> It(RF_ClassDefaultObject, true, EInternalObjectFlags::Garbage); It; ++It)
	{
		UControlRigOverrideAsset* Override = *It;
		if(IsValid(Override))
		{
			Override->BroadcastChanged();
		}
	}
}));

////////////////////////////////////////////////////////////////////////////////
// FControlRigOverrideValue
////////////////////////////////////////////////////////////////////////////////

FControlRigOverrideValue::FControlRigOverrideValue(const FString& InPath, const UObject* InSubject)
	: SubjectKey(InSubject ? InSubject->GetFName() : NAME_None)
{
	if(InSubject)
	{
		if(SetPropertiesFromPath(InPath, InSubject->GetClass()))
		{
			(void)SetFromSubject(InSubject);
		}
	}
}

FControlRigOverrideValue::FControlRigOverrideValue(const FString& InPath, const UStruct* InOwnerStruct, const void* InSubjectPtr, const FName& InSubjectKey)
	: SubjectKey(InSubjectKey)
{
	ensure(InOwnerStruct);
	ensureMsgf(InPath.Contains(TEXT(".")) == false, TEXT("Paths for overrides should use the -> separator."));

	if(SetPropertiesFromPath(InPath, InOwnerStruct))
	{
		if(InSubjectPtr)
		{
			(void)SetFromSubject(InSubjectPtr);
		}
	}
}

FControlRigOverrideValue::FControlRigOverrideValue(const FString& InPath, const UStruct* InOwnerStruct, const FString& InValueAsString, const FName& InSubjectKey, const FControlRigOverrideValueErrorPipe::TReportFunction& InReportFunction)
	: SubjectKey(InSubjectKey)
{
	if(SetPropertiesFromPath(InPath, InOwnerStruct))
	{
		SetFromString(InValueAsString, InReportFunction);
	}
}

FControlRigOverrideValue::FControlRigOverrideValue(const FControlRigOverrideValue& InOther)
{
	*this = InOther;
}

FControlRigOverrideValue::~FControlRigOverrideValue()
{
	Reset();
}

FControlRigOverrideValue& FControlRigOverrideValue::operator=(const FControlRigOverrideValue& InOther)
{
	Reset();
	Path = InOther.Path;
	SubjectKey = InOther.SubjectKey;
	CachedStringValue = InOther.CachedStringValue;
	Properties = InOther.Properties;
	Hash = InOther.Hash;

	if(GetLeafProperty())
	{
		if(const void* OtherDataPtr = InOther.GetData())
		{
			void* DataPtr = AllocateDataIfRequired();
			check(DataPtr);
			CopyValue(DataPtr, OtherDataPtr);
		}
	}

	return *this;
}

bool FControlRigOverrideValue::Serialize(FArchive& Ar)
{
	if(Ar.IsLoading())
	{
		Reset();
	}

	Ar << SubjectKey;
	
	int32 NumProperties = 0;

	if(Ar.IsLoading())
	{
		Ar << NumProperties;
		Properties.Reserve(NumProperties);

		for(int32 Index = 0; Index < NumProperties; Index++)
		{
			FPropertyInfo Info;
			Ar << Info.Property;
			Ar << Info.ArrayIndex;
			Properties.Add(Info);
		}

		// rebuild path from properties
		FStringBuilderBase PathBuilder;
		bool bFirstProperty = true;

		for(int32 Index = 0; Index < NumProperties; Index++)
		{
			if(const FProperty* Property = Properties[Index].Property.Get())
			{
				if(!bFirstProperty)
				{
					PathBuilder.Append(FControlRigOverrideValue::PathSeparator);
				}
				else
				{
					bFirstProperty = false;
				}
				PathBuilder.Append(Property->GetName());
				Path = Property->GetName();
				if(Properties[Index].ArrayIndex != INDEX_NONE)
				{
					Path->Appendf(TEXT("[%d]"),  Properties[Index].ArrayIndex);
				}
			}
			else
			{
				PathBuilder.Reset();
				break;
			}
		}

		if(PathBuilder.Len() > 0)
		{
			Path = PathBuilder.ToString();
		}
	}
	else
	{
		NumProperties = Properties.Num();
		Ar << NumProperties;

		for(int32 Index = 0; Index < NumProperties; Index++)
		{
			Ar << Properties[Index].Property;
			Ar << Properties[Index].ArrayIndex;
		}
	}

	const int64 ArchivePositionBeforeData = Ar.GetArchiveState().Tell();

	int64 OffsetForData = 0;
	Ar << OffsetForData;

	if(Ar.IsLoading())
	{
		if(const FProperty* LeafProperty = GetLeafProperty())
		{
			void* DataPtr = AllocateDataIfRequired();
			LeafProperty->SerializeItem(FStructuredArchiveFromArchive(Ar).GetSlot(), DataPtr);

			FString ValueAsString;
			LeafProperty->ExportTextItem_Direct(ValueAsString, DataPtr, DataPtr, nullptr, PPF_None, nullptr);
			CachedStringValue = ValueAsString;
		}
		else
		{
			// we can't deserialize the data - skip it
			Ar.Seek(ArchivePositionBeforeData + OffsetForData);
		}

		UpdateHash();
	}
	else
	{
		if(const FProperty* LeafProperty = GetLeafProperty())
		{
			void *DataPtr = AllocateDataIfRequired();
			LeafProperty->SerializeItem(FStructuredArchiveFromArchive(Ar).GetSlot(), DataPtr, nullptr);

			const int64 ArchivePositionAfterData = Ar.GetArchiveState().Tell();
			int32 DataToSkip = ArchivePositionAfterData - ArchivePositionBeforeData;
			
			Ar.Seek(ArchivePositionBeforeData);
			Ar << DataToSkip;
			Ar.Seek(ArchivePositionAfterData);
		}
	}

	return true;
}

bool FControlRigOverrideValue::IsValid() const
{
	const bool bIsSet = Path.IsSet() && CachedStringValue.IsSet() && (GetData() != nullptr);
	return bIsSet && (GetLeafProperty() != nullptr);
}

void FControlRigOverrideValue::Reset()
{
	FreeDataIfRequired();
	CachedStringValue.Reset();
	Properties.Reset();
	Hash = 0;
}

const FName& FControlRigOverrideValue::GetSubjectKey() const
{
	static const FName InvalidName = FName(NAME_None);
	return SubjectKey.Get(InvalidName);
}

const FString& FControlRigOverrideValue::GetPath() const
{
	static const FString EmptyPath;
	return Path.Get(EmptyPath);
}

uint8* FControlRigOverrideValue::SubjectPtrToValuePtr(const void* InSubjectPtr, bool bResizeArrays) const
{
	if(Properties.IsEmpty())
	{
		return nullptr;
	}

	if(InSubjectPtr == nullptr)
	{
		return nullptr;
	}

	void* ValuePtr = const_cast<void*>(InSubjectPtr);
	
	for (const FPropertyInfo& Info : Properties)
	{
		const FProperty* Property = Info.Property.Get();
		check(Property);

		const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);
		if(ArrayProperty == nullptr)
		{
			ArrayProperty = CastField<FArrayProperty>(Property->GetOwnerProperty());
		}

		if(Info.ArrayIndex != INDEX_NONE)
		{
			check(ArrayProperty);
			
			ValuePtr = ArrayProperty->ContainerPtrToValuePtr<void*>(ValuePtr);

			FScriptArrayHelper ArrayHelper(ArrayProperty, ValuePtr);
			if(ArrayHelper.Num() <= Info.ArrayIndex)
			{
				if(bResizeArrays)
				{
					ArrayHelper.Resize(Info.ArrayIndex + 1);
				}
				else
				{
					return nullptr;
				}
			}
			
			ValuePtr = ArrayHelper.GetRawPtr(Info.ArrayIndex);
			continue;
		}
		
		if(ArrayProperty)
		{
			Property = ArrayProperty;
		}

		ValuePtr = Property->ContainerPtrToValuePtr<void*>(ValuePtr);
	}

	return static_cast<uint8*>(ValuePtr);
}

const FProperty* FControlRigOverrideValue::GetRootProperty() const
{
	if(Properties.IsEmpty())
	{
		return nullptr;
	}
	return Properties[0].Property.Get();
}

FProperty* FControlRigOverrideValue::FindProperty(const UStruct* InStruct, const FString& InNameOrDisplayName)
{
	if(InStruct == nullptr)
	{
		return  nullptr;
	}

	if(InNameOrDisplayName.IsEmpty())
	{
		return nullptr;
	}
	
	if(FProperty* Property = InStruct->FindPropertyByName(*InNameOrDisplayName))
	{
		return Property;
	}

#if WITH_EDITORONLY_DATA
	const FText DisplayName = FText::FromString(InNameOrDisplayName);
	for (TFieldIterator<FProperty> PropertyIt(InStruct); PropertyIt; ++PropertyIt)
	{
		if(FProperty* PropertyOnStruct = *PropertyIt)
		{
			if(PropertyOnStruct->GetDisplayNameText().EqualToCaseIgnored(DisplayName))
			{
				return PropertyOnStruct;
			}
		}
	}
#endif

	return nullptr;	
}

uint32 GetTypeHash(const FControlRigOverrideValue& InOverride)
{
	return InOverride.Hash;
}

const FProperty* FControlRigOverrideValue::GetLeafProperty() const
{
	if(Properties.IsEmpty())
	{
		return nullptr;
	}
	
	if(const FProperty* LeafProperty = Properties.Last().Property.Get())
	{
		const int32& ArrayIndex = Properties.Last().ArrayIndex;
		if(ArrayIndex != INDEX_NONE)
		{
			if(const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(LeafProperty))
			{
				LeafProperty = ArrayProperty->Inner;
			}
		}
		else
		{
			if(const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(LeafProperty->GetOwnerProperty()))
			{
				LeafProperty = ArrayProperty;
			}
		}

		return LeafProperty;
	}

	return nullptr;
}

int32 FControlRigOverrideValue::GetNumProperties() const
{
	return Properties.Num();
}

const FProperty* FControlRigOverrideValue::GetProperty(int32 InIndex) const
{
	if(Properties[InIndex].Property.IsValid())
	{
		return Properties[InIndex].Property.Get();
	}
	return nullptr;
}

int32 FControlRigOverrideValue::GetArrayIndex(int32 InIndex) const
{
	return Properties[InIndex].ArrayIndex;
}

bool FControlRigOverrideValue::ContainsProperty(const FProperty* InProperty) const
{
	for(int32 Index = 0; Index < Properties.Num(); Index++)
	{
		if(Properties[Index].Property.IsValid())
		{
			if(Properties[Index].Property.Get() == InProperty)
			{
				return true;
			}
		}
	}
	return false;
}

void* FControlRigOverrideValue::GetData()
{
	if(DataArray.IsEmpty())
	{
		return nullptr;
	}
	return DataArray.GetData();
}

const void* FControlRigOverrideValue::GetData() const
{
	if(DataArray.IsEmpty())
	{
		return nullptr;
	}
	return DataArray.GetData();
}

bool FControlRigOverrideValue::SetPropertiesFromPath(const FString& InPath, const UStruct* InOwnerStruct)
{
	Path = InPath;

	FString PropertyNameString;
	FString ArrayIndexString;
	bool bIsParsingPropertyName = true;
	const UStruct* OwnerStruct = InOwnerStruct;

	auto ProcessPropertyName = [this, &PropertyNameString, &OwnerStruct]() -> bool
	{
		FString StringToProcess;
		Swap(StringToProcess, PropertyNameString);
		if(!StringToProcess.IsEmpty())
		{
			if(FProperty* Property = FindProperty(OwnerStruct, StringToProcess))
			{
				FProperty* PropertyForPath = Property;
				
				if(const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
				{
					Property = ArrayProperty->Inner;

					if(Properties.IsEmpty())
					{
						PropertyForPath = Property;
					}
				}
				if(const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					OwnerStruct = StructProperty->Struct;
				}

				Properties.Add({PropertyForPath, INDEX_NONE});

				return true;
			}
		}
		return false;
	};
	
	auto ProcessArrayIndex = [this, &ArrayIndexString]() -> bool
	{
		FString StringToProcess;
		Swap(StringToProcess, ArrayIndexString);
		if(!StringToProcess.IsEmpty())
		{
			Properties.Last().ArrayIndex = FCString::Atoi(*StringToProcess);
			return true;
		}
		return false;
	};

	// Parse the provided Property path. Path may look like
	// Settings[2]->Color->R resulting in three segments, the
	// first one with an array index != INDEX_NONE
	int32 CharIndex = 0;
	while(CharIndex < InPath.Len())
	{
		// potentially start parsing an array index
		if(InPath[CharIndex] == TEXT('['))
		{
			if(!ProcessPropertyName())
			{
				Reset();
				return false;
			}
			bIsParsingPropertyName = false;
			CharIndex++;
			continue;;
		}
		
		// potentially end parsing an array index
		if(InPath[CharIndex] == TEXT(']'))
		{
			if(!ProcessArrayIndex())
			{
				Reset();
				return false;
			}
			CharIndex++;
			continue;;
		}

		// given a path separator parse the property name and store it.
		// if bIsParsingPropertyName == false we already processed the property name
		// and have been parsing an array index
		if(InPath.Mid(CharIndex, PathSeparatorLength).Equals(PathSeparator, ESearchCase::CaseSensitive))
		{
			if(bIsParsingPropertyName)
			{
				if(!ProcessPropertyName())
				{
					Reset();
					return false;
				}
			}
			bIsParsingPropertyName = true;
			CharIndex += PathSeparatorLength;
			continue;
		}

		if(bIsParsingPropertyName)
		{
			PropertyNameString += InPath[CharIndex];
		}
		else
		{
			ArrayIndexString += InPath[CharIndex];
		}
		CharIndex++;
	}

	if(!PropertyNameString.IsEmpty() && !ProcessPropertyName())
	{
		Reset();
		return false;
	}
	if(!ArrayIndexString.IsEmpty() && !ProcessArrayIndex())
	{
		Reset();
		return false;
	}
	return true;
}

void* FControlRigOverrideValue::AllocateDataIfRequired()
{
	return AllocateDataIfRequired(DataArray);
}

void FControlRigOverrideValue::FreeDataIfRequired()
{
	FreeDataIfRequired(DataArray);
}

void* FControlRigOverrideValue::AllocateDataIfRequired(TArray<uint8, TAlignedHeapAllocator<16>>& InOutDataArray) const
{
	if(const FProperty* LeafProperty = GetLeafProperty())
	{
		if(InOutDataArray.IsEmpty())
		{
			InOutDataArray.AddZeroed(LeafProperty->GetSize());
			LeafProperty->InitializeValue(InOutDataArray.GetData());
		}
		else
		{
			check(InOutDataArray.Num() == LeafProperty->GetSize());
		}
	}
	return InOutDataArray.GetData();
}

void FControlRigOverrideValue::FreeDataIfRequired(TArray<uint8, TAlignedHeapAllocator<16>>& InOutDataArray) const
{
	if(void* DataPtr = InOutDataArray.GetData())
	{
		if(const FProperty* LeafProperty = GetLeafProperty())
		{
			if(const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(LeafProperty))
			{
				FScriptArrayHelper ArrayHelper(ArrayProperty, DataPtr);
				ArrayHelper.DestroyContainer_Unsafe();
			}
			else
			{
				LeafProperty->DestroyValue(DataPtr);
			}
		}
	}
	InOutDataArray.Reset();
}

void FControlRigOverrideValue::CopyValue(void* InDestPtr, const void* InSourcePtr) const
{
	check(InDestPtr && InSourcePtr);
	
	const FProperty* LeafProperty = GetLeafProperty();
	check(LeafProperty);

	if(const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(LeafProperty))
	{
		FScriptArrayHelper SourceArray(ArrayProperty, InSourcePtr);
		FScriptArrayHelper DestArray(ArrayProperty, InDestPtr);
		DestArray.Resize(SourceArray.Num());

		for(int32 Index = 0; Index < SourceArray.Num(); Index++)
		{
			const void* SourceElement = SourceArray.GetRawPtr(Index);
			void* DestElement = DestArray.GetRawPtr(Index);
			if(SourceElement && DestElement)
			{
				ArrayProperty->Inner->CopyCompleteValue(DestElement, SourceElement);
			}
		}
	}
	else
	{
		LeafProperty->CopyCompleteValue(InDestPtr, InSourcePtr);
	}
}

void FControlRigOverrideValue::UpdateHash()
{
	Hash = GetTypeHash(SubjectKey);
	if(IsValid())
	{
		for(const FPropertyInfo& PropertyInfo : Properties)
		{
			Hash = HashCombine(Hash, GetTypeHash(PropertyInfo.Property));
			Hash = HashCombine(Hash, GetTypeHash(PropertyInfo.ArrayIndex));
		}
		Hash = HashCombine(Hash, GetTypeHash(CachedStringValue));
	}
}

const FString& FControlRigOverrideValue::ToString() const
{
	static const FString EmptyPath;
	return CachedStringValue.Get(EmptyPath);
}

bool FControlRigOverrideValue::SetFromString(const FString& InValue, const FControlRigOverrideValueErrorPipe::TReportFunction& InReportFunction)
{
	if(InValue != CachedStringValue.Get(FString()))
	{
		if(const FProperty* LeafProperty = GetLeafProperty())
		{
			TArray<uint8, TAlignedHeapAllocator<16>> TempDataArray;
			void* TempDataPtr = AllocateDataIfRequired(TempDataArray); 

			// first try to import the string value using the property.
			// we need to react to any message coming in from the import since missing parentheses for
			// example are reported at verbosity level Log.
			FControlRigOverrideValueErrorPipe ErrorPipe(ELogVerbosity::Log, InReportFunction);
			LeafProperty->ImportText_Direct(*InValue, TempDataPtr, nullptr, PPF_None, &ErrorPipe);
			if(ErrorPipe.GetNumErrors() == 0)
			{
				FreeDataIfRequired(DataArray);
				Swap(DataArray, TempDataArray);
				
				CachedStringValue = InValue;
				UpdateHash();
				return true;
			}
			FreeDataIfRequired(TempDataArray);

			// we've hit a problem when importing the value
			if(const FStructProperty* StructProperty = CastField<FStructProperty>(LeafProperty))
			{
				// potentially we have a default value for a math type here.
				const UStruct* Struct = StructProperty->Struct;

				if(Struct == TBaseStructure<FVector2D>::Get())
				{
					FString ValueString = InValue;
					for(int32 TryIndex = 0; TryIndex < 2; TryIndex++)
					{
						if(InitFromLegacyString<FVector2D>(ValueString, FVector2D::ZeroVector))
						{
							return true;
						}

						// potentially we have the value as "1.000,2.000"
						if(TryIndex == 0)
						{
							TArray<FString> Parts;
							if(RigVMStringUtils::SplitString(ValueString, TEXT(","), Parts))
							{
								if(Parts.Num() == 2)
								{
									ValueString = FString::Printf(TEXT("(X=%s,Y=%s)"), *Parts[0], *Parts[1]);
									continue;
								}
							}
						}
						
						break;
					}
				}

				if(Struct == TBaseStructure<FVector>::Get())
				{
					FString ValueString = InValue;
					for(int32 TryIndex = 0; TryIndex < 2; TryIndex++)
					{
						if(InitFromLegacyString<FVector>(ValueString, FVector::ZeroVector))
						{
							return true;
						}
					
						// potentially we have the value as "1.000,2.000,3.000"
						if(TryIndex == 0)
						{
							TArray<FString> Parts;
							if(RigVMStringUtils::SplitString(ValueString, TEXT(","), Parts))
							{
								if(Parts.Num() == 3)
								{
									ValueString = FString::Printf(TEXT("(X=%s,Y=%s,Z=%s)"), *Parts[0], *Parts[1], *Parts[2]);
									continue;
								}
							}
						}
						
						break;
					}
				}

				if(Struct == TBaseStructure<FRotator>::Get())
				{
					FString ValueString = InValue;
					for(int32 TryIndex = 0; TryIndex < 2; TryIndex++)
					{
						if(InitFromLegacyString<FRotator>(ValueString, FRotator::ZeroRotator))
						{
							return true;
						}

						// potentially we have the value as "1.000,2.000,3.000"
						if(TryIndex == 0)
						{
							TArray<FString> Parts;
							if(RigVMStringUtils::SplitString(ValueString, TEXT(","), Parts))
							{
								if(Parts.Num() == 3)
								{
									ValueString = FString::Printf(TEXT("(P=%s,Y=%s,R=%s)"), *Parts[0], *Parts[1], *Parts[2]);
									continue;
								}
							}
						}
						
						break;
					}
				}

				if(Struct == TBaseStructure<FQuat>::Get())
				{
					if(InitFromLegacyString<FQuat>(InValue, FQuat::Identity))
					{
						return true;
					}
				}

				if(Struct == TBaseStructure<FTransform>::Get())
				{
					if(InitFromLegacyString<FTransform>(InValue, FTransform::Identity))
					{
						return true;
					}
				}
			}

			if(SubjectKey.IsSet())
			{
				UE_LOG(LogControlRig, Warning, TEXT("Unable to import default value '%s' for override '%s' on subject '%s'."), *InValue, *GetPath(), *SubjectKey.GetValue().ToString());
			}
			else
			{
				UE_LOG(LogControlRig, Warning, TEXT("Unable to import default value '%s' for override '%s'"), *InValue, *GetPath());
			}
		}
	}
	return false;
}

bool FControlRigOverrideValue::operator==(const FControlRigOverrideValue& InOtherValue) const
{
	if(Hash != InOtherValue.Hash)
	{
		return false;
	}
	return IdenticalValue(InOtherValue.GetData());
}

bool FControlRigOverrideValue::Identical(const FControlRigOverrideValue& InOtherValue) const
{
	return *this == InOtherValue;
}

bool FControlRigOverrideValue::IdenticalValue(const void* InValuePtr) const
{
	if(!IsValid() || (InValuePtr == nullptr))
	{
		return false;
	}

	// IsValid checks the validity of the data pointer and leaf property
	const FProperty* LeafProperty = GetLeafProperty();
	check(LeafProperty);
	return LeafProperty->Identical(GetData(), InValuePtr);
}

bool FControlRigOverrideValue::IdenticalValueInSubject(const void* InSubjectPtr) const
{
	return IdenticalValue(SubjectPtrToValuePtr(InSubjectPtr, false));
}

bool FControlRigOverrideValue::CopyToSubject(void* InSubjectPtr, const UStruct* InSubjectStruct) const
{
	if(InSubjectStruct)
	{
		if(const FProperty* RootProperty = GetRootProperty())
		{
			if(RootProperty->GetOwnerStruct() != InSubjectStruct)
			{
				return false;
			}
		}
	}

	if(const void* SourcePtr = GetData())
	{
		if(void* DestPtr = SubjectPtrToValuePtr(InSubjectPtr, true))
		{
			CopyValue(DestPtr, SourcePtr);
			return true;
		}
	}

	return false;
}

bool FControlRigOverrideValue::SetFromSubject(const void* InSubjectPtr, const UStruct* InSubjectStruct)
{
	if(InSubjectStruct)
	{
		if(const FProperty* RootProperty = GetRootProperty())
		{
			if(RootProperty->GetOwnerStruct() != InSubjectStruct)
			{
				return false;
			}
		}
	}
	
	if(const FProperty* LeafProperty = GetLeafProperty())
	{
		void* DestPtr = AllocateDataIfRequired();
		if(InSubjectPtr && DestPtr)
		{
			if(const void* SourcePtr = SubjectPtrToValuePtr(InSubjectPtr, false))
			{
				FString ValueAsString;
				CopyValue(DestPtr, SourcePtr);
				LeafProperty->ExportTextItem_Direct(ValueAsString, DestPtr, DestPtr, nullptr, PPF_None, nullptr);
				CachedStringValue = ValueAsString;
				UpdateHash();
				return true;
			}
		}
	}

	return false;
}

void FControlRigOverrideValue::CopyToUObject(UObject* InSubjectPtr) const
{
	if(InSubjectPtr)
	{
		(void)CopyToSubject(InSubjectPtr, InSubjectPtr->GetClass());
	}
}

void FControlRigOverrideValue::SetFromUObject(const UObject* InSubjectPtr)
{
	if(InSubjectPtr)
	{
		(void)SetFromSubject(InSubjectPtr, InSubjectPtr->GetClass());
	}
}

////////////////////////////////////////////////////////////////////////////////
// FControlRigOverrideContainer
////////////////////////////////////////////////////////////////////////////////

void FControlRigOverrideContainer::Reset()
{
	Values.Reset();
	HashIndexLookup.Reset();
	SubjectIndexLookup.Reset();
	InvalidateCache();
}

void FControlRigOverrideContainer::Empty()
{
	Values.Empty();
	HashIndexLookup.Empty();
	SubjectIndexLookup.Reset();
	InvalidateCache();
}

void FControlRigOverrideContainer::Reserve(int32 InNum)
{
	Values.Reserve(InNum);
	HashIndexLookup.Reserve(InNum);
}

bool FControlRigOverrideContainer::UsesKeyForSubject() const
{
	return bUsesKeyForSubject;
}

void FControlRigOverrideContainer::SetUsesKeyForSubject(bool InUsesKeyForSubject)
{
	bUsesKeyForSubject = InUsesKeyForSubject;
	RebuildLookup();
	InvalidateCache();
}

int32 FControlRigOverrideContainer::Add(const FControlRigOverrideValue& InValue)
{
	if(!InValue.IsValid())
	{
		return INDEX_NONE;
	}
	
	const FString& Path = InValue.GetPath();
	if(!Path.IsEmpty())
	{
		if(!Contains(Path, InValue.GetSubjectKey()))
		{
			// don't allow to add overrides for child paths of
			// existing parent paths
			if(ContainsParentPathOf(Path, InValue.GetSubjectKey()))
			{
				return INDEX_NONE;
			}
			
			// remove child paths when adding parent overrides
			if(ContainsChildPathOf(Path, InValue.GetSubjectKey()))
			{
				TArray<int32> IndicesToRemove;
				for(int32 Index = 0; Index < Values.Num(); Index++)
				{
					const FString& ChildPath = Values[Index].GetPath();
					if(IsChildPathOf(ChildPath, Path))
					{
						IndicesToRemove.Add(Index);
					}
				}

				if(!IndicesToRemove.IsEmpty())
				{
					// remove in reverse
					for(int32 Index = IndicesToRemove.Num() - 1; Index >= 0; Index--)
					{
						Values.RemoveAt(IndicesToRemove[Index]);
					}
					RebuildLookup();
				}
			}
			
			const int32 Index = Values.Add(InValue);
			const uint32 Hash = GetLookupHash(Path, InValue.GetSubjectKey());
			HashIndexLookup.Add(Hash, Index);
			SubjectIndexLookup.FindOrAdd(InValue.GetSubjectKey()).Add(Index);

			InvalidateCache();
			return Index;
		}
	}
	return INDEX_NONE;
}

const FControlRigOverrideValue* FControlRigOverrideContainer::FindOrAdd(const FControlRigOverrideValue& InValue)
{
	const FString& Path = InValue.GetPath();
	if(!Path.IsEmpty())
	{
		if(FControlRigOverrideValue* ExistingValue = Find(Path, InValue.GetSubjectKey()))
		{
			*ExistingValue = InValue;
			return ExistingValue;
		}

		const int32 NewIndex = Add(InValue);
		if(IsValidIndex(NewIndex))
		{
			return &Values[NewIndex];
		}
	}
	return nullptr;
}

bool FControlRigOverrideContainer::Remove(const FControlRigOverrideValue& InValue)
{
	return Remove(InValue.GetPath(), InValue.GetSubjectKey());
}

bool FControlRigOverrideContainer::Remove(const FString& InPath, const FName& InSubjectKey)
{
	const int32 Index = GetIndex(InPath, InSubjectKey);
	if(IsValidIndex(Index))
	{
		Values.RemoveAt(Index);
		RebuildLookup();
		InvalidateCache();
		return true;
	}
	return false;
}

bool FControlRigOverrideContainer::RemoveAll(const FName& InSubjectKey)
{
	if(IsEmpty())
	{
		return false;
	}
	if(!InSubjectKey.IsNone())
	{
		if(const TArray<int32>* IndicesPtr = GetIndicesForSubject(InSubjectKey))
		{
			for(int32 Index = IndicesPtr->Num() - 1; Index >= 0; Index--)
			{
				Values.RemoveAt(IndicesPtr->operator[](Index));
			}
			RebuildLookup();
			InvalidateCache();
			return true;
		}
		return false;
	}
	else
	{
		Reset();
	}
	return true;
}

int32 FControlRigOverrideContainer::GetIndex(const FString& InPath, const FName& InSubjectKey) const
{
	const uint32 Hash = GetLookupHash(InPath, InSubjectKey);
	if(const int32* Index = HashIndexLookup.Find(Hash))
	{
		return *Index;
	}
	return INDEX_NONE;
}

const TArray<int32>* FControlRigOverrideContainer::GetIndicesForSubject(const FName& InSubjectKey) const
{
	return SubjectIndexLookup.Find(InSubjectKey);
}

const FControlRigOverrideValue* FControlRigOverrideContainer::Find(const FString& InPath, const FName& InSubjectKey) const
{
	const int32 Index = GetIndex(InPath, InSubjectKey);
	if(IsValidIndex(Index))
	{
		return &Values[Index];
	}
	return nullptr;
}

FControlRigOverrideValue* FControlRigOverrideContainer::Find(const FString& InPath, const FName& InSubjectKey)
{
	const int32 Index = GetIndex(InPath, InSubjectKey);
	if(IsValidIndex(Index))
	{
		return &Values[Index];
	}
	return nullptr;
}

const FControlRigOverrideValue& FControlRigOverrideContainer::FindChecked(const FString& InPath, const FName& InSubjectKey) const
{
	const int32 Index = GetIndex(InPath, InSubjectKey);
	check(IsValidIndex(Index));
	return Values[Index];
}

FControlRigOverrideValue& FControlRigOverrideContainer::FindChecked(const FString& InPath, const FName& InSubjectKey)
{
	const int32 Index = GetIndex(InPath, InSubjectKey);
	check(IsValidIndex(Index));
	return Values[Index];
}

TArray<FName> FControlRigOverrideContainer::GenerateSubjectArray() const
{
	TArray<FName> Keys;
	SubjectIndexLookup.GenerateKeyArray(Keys);
	return Keys;
}

bool FControlRigOverrideContainer::Contains(const FString& InPath, const FName& InSubjectKey) const
{
	return IsValidIndex(GetIndex(InPath, InSubjectKey));
}

bool FControlRigOverrideContainer::ContainsParentPathOf(const FString& InChildPath, const FName& InSubjectKey) const
{
	if(InChildPath.IsEmpty())
	{
		return false;
	}

	const uint32 Hash = GetLookupHash(InChildPath, InSubjectKey);
	if(const bool* CachedResult = ContainsParentPathCache.Find(Hash))
	{
		return *CachedResult;
	}

	const FString ParentPath = GetParentPath(InChildPath);
	const bool bResult = !ParentPath.IsEmpty() && (Contains(ParentPath, InSubjectKey) || ContainsParentPathOf(ParentPath, InSubjectKey));
	ContainsParentPathCache.Add(Hash, bResult);
	return bResult;
}

bool FControlRigOverrideContainer::ContainsChildPathOf(const FString& InParentPath, const FName& InSubjectKey) const
{
	if(InParentPath.IsEmpty())
	{
		return false;
	}
	
	const uint32 Hash = GetLookupHash(InParentPath, InSubjectKey);
	if(const bool* CachedResult = ContainsChildPathCache.Find(Hash))
	{
		return *CachedResult;
	}

	for(const FControlRigOverrideValue& Value : Values)
	{
		if(bUsesKeyForSubject)
		{
			if(Value.GetSubjectKey() != InSubjectKey)
			{
				continue;
			}
		}
		const FString& Path = Value.GetPath();
		if(IsChildPathOf(Path, InParentPath))
		{
			ContainsChildPathCache.Add(Hash, true);
			return true;
		}
	}

	ContainsChildPathCache.Add(Hash, false);
	return false;
}

bool FControlRigOverrideContainer::ContainsAnyPathForSubject(const FName& InSubjectKey) const
{
	return SubjectIndexLookup.Contains(InSubjectKey);
}

bool FControlRigOverrideContainer::ContainsPathForAnySubject(const FString& InPath) const
{
	for(const FControlRigOverrideValue& Override : Values)
	{
		if(Override.GetPath() == InPath)
		{
			return true;
		}
	}
	return false;
}

bool FControlRigOverrideContainer::Contains(const FControlRigOverrideValue& InOverrideValue) const
{
	return Contains(InOverrideValue.GetPath(), InOverrideValue.GetSubjectKey());
}

bool FControlRigOverrideContainer::ContainsParentPathOf(const FControlRigOverrideValue& InOverrideValue) const
{
	return ContainsParentPathOf(InOverrideValue.GetPath(), InOverrideValue.GetSubjectKey());
}

bool FControlRigOverrideContainer::ContainsChildPathOf(const FControlRigOverrideValue& InOverrideValue) const
{
	return ContainsChildPathOf(InOverrideValue.GetPath(), InOverrideValue.GetSubjectKey());
}

void FControlRigOverrideContainer::CopyToSubject(void* InSubjectPtr, const UStruct* InSubjectStruct, const FName& InSubjectKey) const
{
	for(const FControlRigOverrideValue& Override : Values)
	{
		if(bUsesKeyForSubject)
		{
			if(Override.GetSubjectKey() != InSubjectKey)
			{
				continue;
			}
		}
		(void)Override.CopyToSubject(InSubjectPtr, InSubjectStruct);
	}
}

void FControlRigOverrideContainer::SetFromSubject(const void* InSubjectPtr, const UStruct* InSubjectStruct, const FName& InSubjectKey)
{
	for(FControlRigOverrideValue& Override : Values)
	{
		if(bUsesKeyForSubject)
		{
			if(Override.GetSubjectKey() != InSubjectKey)
			{
				continue;
			}
		}
		(void)Override.SetFromSubject(InSubjectPtr, InSubjectStruct);
	}
}

void FControlRigOverrideContainer::CopyToUObject(UObject* InSubjectPtr) const
{
	CopyToSubject(InSubjectPtr, InSubjectPtr->GetClass(), InSubjectPtr->GetFName());
}

void FControlRigOverrideContainer::SetFromUObject(const UObject* InSubjectPtr)
{
	SetFromSubject(InSubjectPtr, InSubjectPtr->GetClass(), InSubjectPtr->GetFName());
}

bool FControlRigOverrideContainer::Serialize(FArchive& Ar)
{
	Ar << bUsesKeyForSubject;
	Ar << Values;
	
	if(Ar.IsLoading())
	{
		RebuildLookup();
		InvalidateCache();
	}
	return true;
}

void FControlRigOverrideContainer::RebuildLookup()
{
	HashIndexLookup.Reset();
	HashIndexLookup.Reserve(Values.Num());
	SubjectIndexLookup.Reset();

	for(int32 Index = 0; Index < Values.Num(); Index++)
	{
		if(Values[Index].IsValid())
		{
			const FString& Path = Values[Index].GetPath();
			if(!Path.IsEmpty())
			{
				const uint32 Hash = GetLookupHash(Path, Values[Index].GetSubjectKey());
				HashIndexLookup.Add(Hash, Index);
				SubjectIndexLookup.FindOrAdd(Values[Index].GetSubjectKey()).Add(Index);
			}
		}
	}
}

void FControlRigOverrideContainer::InvalidateCache() const
{
	ContainsParentPathCache.Reset();
	ContainsChildPathCache.Reset();
}

#if WITH_EDITOR
void UControlRigOverrideAsset::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	UObject::PostTransacted(TransactionEvent);

	if(TransactionEvent.GetEventType() == ETransactionObjectEventType::UndoRedo)
	{
		for (TObjectIterator<UControlRig> It(RF_ClassDefaultObject, true, EInternalObjectFlags::Garbage); It; ++It)
		{
			UControlRig* ControlRig = *It;
			if(IsValid(ControlRig))
			{
				if(ControlRig->IsLinkedToOverrideAsset(this))
				{
					ControlRig->RequestConstruction();
				}
			}
		}

	}
}
#endif

UControlRigOverrideAsset* UControlRigOverrideAsset::CreateOverrideAsset(const FString& InLongName)
{
	UControlRigOverrideAsset* Asset = nullptr;
#if WITH_EDITOR
	const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	FString UniquePackageName;
	FString UniqueAssetName;
	AssetToolsModule.Get().CreateUniqueAssetName(InLongName, TEXT(""), UniquePackageName, UniqueAssetName);
	if (UniquePackageName.EndsWith(UniqueAssetName))
	{
		UniquePackageName = UniquePackageName.LeftChop(UniqueAssetName.Len() + 1);
	}
	Asset = Cast<UControlRigOverrideAsset>(AssetToolsModule.Get().CreateAsset(*UniqueAssetName, *UniquePackageName, UControlRigOverrideAsset::StaticClass(), nullptr));
	check(Asset);
#endif
	return Asset;
}

UControlRigOverrideAsset* UControlRigOverrideAsset::CreateOverrideAssetInDeveloperFolder(const UObject* InSubject)
{
	UControlRigOverrideAsset* Asset = nullptr;
#if WITH_EDITOR
	FString DeveloperPathWithoutSlash = FPackageName::FilenameToLongPackageName(FPaths::GameUserDeveloperDir()).LeftChop(1);
	FString ShortName = InSubject ? InSubject->GetClass()->GetOutermost()->GetName() : TEXT("Default");
	ShortName = FPaths::GetBaseFilename(ShortName) + TEXT("_Override");
	const FString LongName = FPaths::Combine(DeveloperPathWithoutSlash, TEXT("Overrides"), ShortName);
	Asset = CreateOverrideAsset(LongName);
#endif
	return Asset;
}

void UControlRigOverrideAsset::BroadcastChanged()
{
	OverrideChangedDelegate.Broadcast(this);
}

void FControlRigOverrideContainer::AddStructReferencedObjects(FReferenceCollector& Collector)
{
	for(const FControlRigOverrideValue& Override : Values)
	{
		if(const FProperty* RootProperty = Override.GetRootProperty())
		{
			if(UClass* Class = RootProperty->GetOwnerClass())
			{
				TWeakObjectPtr<UClass> WeakClass(Class);
				Collector.AddReferencedObject(WeakClass);
			}
			else if(UStruct* Struct = RootProperty->GetOwnerStruct())
			{
				TWeakObjectPtr<UStruct> WeakStruct(Struct);
				Collector.AddReferencedObject(WeakStruct);
			}
		}
	}
}

bool FControlRigOverrideContainer::Identical(const FControlRigOverrideContainer* Other, uint32 PortFlags) const
{
	if(Other == nullptr)
	{
		return false;
	}
	if(Num() != Other->Num())
	{
		return false;
	}
	for(int32 Index = 0; Index < Num(); Index++)
	{
		const FControlRigOverrideValue& Value = operator[](Index);
		const FControlRigOverrideValue& OtherValue = Other->operator[](Index);
		if(!Value.Identical(OtherValue))
		{
			return false;
		}
	}
	return true;
}

FString FControlRigOverrideContainer::GetParentPath(const FString& InChildPath)
{
	int32 LastSeparatorIndex = InChildPath.Find(FControlRigOverrideValue::PathSeparator, ESearchCase::CaseSensitive, ESearchDir::FromEnd);

	int32 LastArrayIndex = INDEX_NONE;
	if(InChildPath.FindLastChar(FControlRigOverrideValue::ArraySeparator, LastArrayIndex))
	{
		LastSeparatorIndex = FMath::Max(LastSeparatorIndex, LastArrayIndex);
	}

	if(LastSeparatorIndex != INDEX_NONE)
	{
		return InChildPath.Left(LastSeparatorIndex);
	}

	return FString();
}

bool FControlRigOverrideContainer::IsChildPathOf(const FString& InChildPath, const FString& InParentPath)
{
	if(!InChildPath.IsEmpty())
	{
		// this is true for both cases of child paths - one being separated by -> and the other being separated by [
		if(InChildPath.Len() > InParentPath.Len() + FControlRigOverrideValue::PathSeparatorLength)
		{
			const FStringView ChildPathView(InChildPath);
			if(ChildPathView.StartsWith(InParentPath))
			{
				if(ChildPathView[InParentPath.Len()] == FControlRigOverrideValue::ArraySeparator)
				{
					return true;
				}
				if(ChildPathView.Mid(InParentPath.Len(), FControlRigOverrideValue::PathSeparatorLength) == FControlRigOverrideValue::PathSeparator)
				{
					return true;
				}
			}
		}
	}
	return false;
}

#if WITH_EDITOR
TSharedPtr<FPropertyPath> FControlRigOverrideValue::ToPropertyPath() const
{
	if(Properties.IsEmpty())
	{
		return nullptr;
	}
	
	TSharedPtr<FPropertyPath> PropertyPath = FPropertyPath::CreateEmpty();

	for(const FPropertyInfo& PropertyInfo : Properties)
	{
		PropertyPath->AddProperty({PropertyInfo.Property, PropertyInfo.ArrayIndex});
	}

	return PropertyPath;
}
#endif

uint32 GetTypeHash(const FControlRigOverrideContainer& InContainer)
{
	uint32 Hash = GetTypeHash(InContainer.bUsesKeyForSubject);
	for(const FControlRigOverrideValue& Override : InContainer.Values)
	{
		Hash = HashCombine(Hash, GetTypeHash(Override));
	}
	return Hash;
}

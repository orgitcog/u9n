// Copyright Epic Games, Inc. All Rights Reserved.

#if (WITH_VERSE_COMPILER && WITH_VERSE_BPVM) || WITH_VERSE_VM

#include "VerseVM/VVMTypeInitOrValidate.h"

namespace Verse
{
#if WITH_METADATA
void FInitOrValidateUField::SetMetaData(bool bEnabled, FName MetaDataName, const TCHAR* MetaDataValue) const
{
	if (bEnabled)
	{
		check(MetaDataValue);
		if (bIsValidating)
		{
			const FString* Value = Field->FindMetaData(MetaDataName);
			if (Value == nullptr || *Value != MetaDataValue)
			{
				LogError(FString::Format(TEXT("'{0}:metadata:{1}' doesn't have the expected value.  Expected '{2}'"),
					{Field->GetName(), *MetaDataName.ToString(), MetaDataValue}));
			}
		}
		else
		{
			Field->SetMetaData(MetaDataName, MetaDataValue);
		}
	}
	else
	{
		if (bIsValidating)
		{
			const FString* Value = Field->FindMetaData(MetaDataName);
			if (Value != nullptr)
			{
				LogError(FString::Format(TEXT("'{0}:metadata:{1}' has a value when none was expected."),
					{Field->GetName(), *MetaDataName.ToString()}));
			}
		}
		else
		{
			// Not setting, we don't care
		}
	}
}
#endif

void FInitOrValidateUEnum::SetEnums(TArray<TPair<FName, int64>>& InNames, UEnum::ECppForm InCppForm) const
{
	if (bIsValidating)
	{
		UEnum* Enum = GetUEnum();
		CheckValueMismatch(Enum->GetCppForm(), InCppForm, TEXT("CppForm"));

		int32 ExistingCount = Enum->NumEnums();
		TMap<FName, int64> ExistingValues;
		ExistingValues.Reserve(ExistingCount);
		for (int32 Index = 0; Index < ExistingCount; ++Index)
		{
			ExistingValues.Emplace(Enum->GetNameByIndex(Index), Enum->GetValueByIndex(Index));
		}

		// We don't care if the desintation has extra values (i.e. _MAX).  However, the verse values
		// must match
		for (const TPair<FName, int64>& Name : InNames)
		{
			const int64* ExistingValue = ExistingValues.Find(Name.Key);
			if (ExistingValue == nullptr)
			{
				LogError(FString::Format(TEXT("'{0}:Names:{1}' is expected but not found"),
					{Field->GetName(), *Name.Key.ToString()}));
			}
			else if (*ExistingValue != Name.Value)
			{
				LogValueMismatch(*ExistingValue, Name.Value, TEXT("Names"), *Name.Key.ToString());
			}
		}
	}
	else
	{
		GetUEnum()->SetEnums(InNames, InCppForm);
	}
}

void FInitOrValidateUStruct::SetSuperStruct(UClass* SuperStruct) const
{
	UStruct* Struct = GetUStruct();
	if (bIsValidating)
	{
		CheckValueMismatch(Struct->GetSuperStruct(), SuperStruct, TEXT("SuperStruct"));
	}
	else
	{
		Struct->SetSuperStruct(SuperStruct);
		Struct->PropertyLink = SuperStruct->PropertyLink;
	}
}

void FInitOrValidateUVerseStruct::SetVerseClassFlags(uint32 ClassFlags, bool bSetFlags, const TCHAR* What) const
{
	UVerseStruct* VerseStruct = GetUVerseStruct();
	uint32 Value = bSetFlags ? ClassFlags : 0;

	if (bIsValidating)
	{
		CheckValueMismatch((VerseStruct->VerseClassFlags & ClassFlags) != 0, Value != 0, TEXT("VerseStructFlag"), What);
	}
	else
	{
		VerseStruct->VerseClassFlags = (VerseStruct->VerseClassFlags & ~ClassFlags) | Value;
	}
}

void FInitOrValidateUVerseStruct::ForceVerseClassFlags(uint32 ClassFlags, bool bSetFlags) const
{
	UVerseStruct* VerseStruct = GetUVerseStruct();
	uint32 Value = bSetFlags ? ClassFlags : 0;
	VerseStruct->VerseClassFlags = (VerseStruct->VerseClassFlags & ~ClassFlags) | Value;
}

void FInitOrValidateUClass::SetClassFlags(EClassFlags ClassFlags, bool bSetFlags, const TCHAR* What) const
{
	UClass* Class = GetUClass();
	EClassFlags Value = bSetFlags ? ClassFlags : CLASS_None;

	if (bIsValidating)
	{
		CheckValueMismatch((Class->ClassFlags & ClassFlags) != 0, Value != 0, TEXT("ClassFlag"), What);
	}
	else
	{
		Class->ClassFlags = (Class->ClassFlags & ~ClassFlags) | Value;
	}
}

void FInitOrValidateUClass::SetClassFlagsNoValidate(EClassFlags ClassFlags, bool bSetFlags) const
{
	if (!bIsValidating)
	{
		UClass* Class = GetUClass();
		EClassFlags Value = bSetFlags ? ClassFlags : CLASS_None;
		Class->ClassFlags = (Class->ClassFlags & ~ClassFlags) | Value;
	}
}

void FInitOrValidateUVerseClass::SetVerseClassFlags(uint32 ClassFlags, bool bSetFlags, const TCHAR* What) const
{
	UVerseClass* VerseClass = GetUVerseClass();
	uint32 Value = bSetFlags ? ClassFlags : 0;

	if (bIsValidating)
	{
		CheckValueMismatch((VerseClass->SolClassFlags & ClassFlags) != 0, Value != 0, TEXT("VerseClassFlag"), What);
	}
	else
	{
		VerseClass->SolClassFlags = (VerseClass->SolClassFlags & ~ClassFlags) | Value;
	}
}

void FInitOrValidateUVerseClass::ForceVerseClassFlags(uint32 ClassFlags, bool bSetFlags) const
{
	UVerseClass* VerseClass = GetUVerseClass();
	uint32 Value = bSetFlags ? ClassFlags : 0;
	VerseClass->SolClassFlags = (VerseClass->SolClassFlags & ~ClassFlags) | Value;
}

bool FInitOrValidateUVerseClass::AddInterface(UClass* InterfaceClass, EAddInterfaceType InterfaceType)
{
	UVerseClass* VerseClass = GetUVerseClass();

	if (bIsValidating)
	{
		if (Interfaces.Contains(InterfaceClass))
		{
			return false;
		}

		Interfaces.Emplace(InterfaceClass);

		if (InterfaceType == EAddInterfaceType::Direct)
		{
			check(!DirectInterfaces.Contains(InterfaceClass));
			DirectInterfaces.Emplace(InterfaceClass);
		}
		return true;
	}
	else
	{
		if (VerseClass->Interfaces.ContainsByPredicate([InterfaceClass](const FImplementedInterface& Interface) { return Interface.Class == InterfaceClass; }))
		{
			return false;
		}

		// Note: We always set PointerOffset to 0 here even though the interface might be native
		// This is because PointerOffset is not currently used anywhere with relevance to Verse generated code
		VerseClass->Interfaces.Emplace(InterfaceClass, 0, false);

		if (InterfaceType == EAddInterfaceType::Direct)
		{
			UVerseClass* InterfaceVerseClass = CastChecked<UVerseClass>(InterfaceClass);
			check(!VerseClass->DirectInterfaces.Contains(InterfaceVerseClass));
			VerseClass->DirectInterfaces.Emplace(CastChecked<UVerseClass>(InterfaceVerseClass));
		}
		return true;
	}
}

void FInitOrValidateUVerseClass::ValidateInterfaces()
{
	if (!bIsValidating)
	{
		return;
	}

	UVerseClass* VerseClass = GetUVerseClass();

	// It is possible for the UHT class to support extra interfaces
	for (const UClass* InterfaceClass : Interfaces)
	{
		if (!VerseClass->Interfaces.ContainsByPredicate([InterfaceClass](const FImplementedInterface& Interface) { return Interface.Class == InterfaceClass; }))
		{
			LogError(FString::Format(TEXT("'{0}:Interfaces' is missing the expected interface '%s'"),
				{Field->GetName(), FormatValue(InterfaceClass)}));
		}
	}

	// Direct interfaces should all be verse interfaces
	bool bMismatch = false;
	for (const UClass* InterfaceClass : DirectInterfaces)
	{
		if (!VerseClass->DirectInterfaces.Contains(InterfaceClass))
		{
			bMismatch = true;
			LogError(FString::Format(TEXT("'{0}:DirectInterfaces' is missing the expected direct interface '%s'"),
				{Field->GetName(), FormatValue(InterfaceClass)}));
		}
	}
	if (bMismatch)
	{
		for (const UClass* InterfaceClass : VerseClass->DirectInterfaces)
		{
			if (!DirectInterfaces.Contains(InterfaceClass))
			{
				LogError(FString::Format(TEXT("'{0}:DirectInterfaces' has the unexpected direct interface '%s'"),
					{Field->GetName(), FormatValue(InterfaceClass)}));
			}
		}
	}
}

} // namespace Verse
#endif // (WITH_VERSE_COMPILER && WITH_VERSE_BPVM) || WITH_VERSE_VM

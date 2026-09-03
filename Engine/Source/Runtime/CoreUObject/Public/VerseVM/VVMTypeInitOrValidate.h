// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if (WITH_VERSE_COMPILER && WITH_VERSE_BPVM) || WITH_VERSE_VM

#include "CoreTypes.h"
#include "VerseVM/VVMVerseClass.h"
#include "VerseVM/VVMVerseEnum.h"
#include "VerseVM/VVMVerseStruct.h"
#include <type_traits>

namespace Verse
{
struct FInitOrValidateUEnum;
struct FInitOrValidateUVerseEnum;
struct FInitOrValidateUStruct;
struct FInitOrValidateUScriptStruct;
struct FInitOrValidateUVerseStruct;
struct FInitOrValidateUClass;
struct FInitOrValidateUVerseClass;

enum class EAddInterfaceType : uint8
{
	Direct,
	Indirect,
};

struct FInitOrValidateUField
{
	FInitOrValidateUField(UField* InField, bool bInIsValidating)
		: Field(InField)
		, bIsValidating(bInIsValidating)
	{
	}
	virtual ~FInitOrValidateUField() = default;

	UField* GetField() const { return Field; }

	virtual const FInitOrValidateUEnum* AsUEnum() const { return nullptr; }
	virtual const FInitOrValidateUVerseEnum* AsUVerseEnum() const { return nullptr; }
	virtual const FInitOrValidateUStruct* AsUStruct() const { return nullptr; }
	virtual const FInitOrValidateUScriptStruct* AsUScriptStruct() const { return nullptr; }
	virtual const FInitOrValidateUVerseStruct* AsUVerseStruct() const { return nullptr; }
	virtual const FInitOrValidateUClass* AsUClass() const { return nullptr; }
	virtual const FInitOrValidateUVerseClass* AsUVerseClass() const { return nullptr; }

	bool IsValidating() const { return bIsValidating; }
	bool IsInitializing() const { return !bIsValidating; }

#if WITH_METADATA
	COREUOBJECT_API void SetMetaData(bool bEnabled, FName MetaDataName, const TCHAR* MetaDataValue) const;
#endif

	template <typename DestinationType, typename ValueType>
	void SetValue(DestinationType& Destination, const ValueType& Value, const TCHAR* What) const
	{
		if (bIsValidating)
		{
			CheckValueMismatch(Destination, Value, What);
		}
		else
		{
			Destination = Value;
		}
	}

	// Set value, but skip any validation
	template <typename DestinationType, typename ValueType>
	void SetValueNoValidate(DestinationType& Destination, const ValueType& Value) const
	{
		if (!bIsValidating)
		{
			Destination = Value;
		}
	}

	// Set the value regardless of validation step.
	template <typename DestinationType, typename ValueType>
	void ForceValue(DestinationType& Destination, const ValueType& Value) const
	{
		Destination = Value;
	}

protected:
	template <typename ValueType>
	static FString FormatValue(ValueType Value)
	{
		if constexpr (std::is_enum<ValueType>::value)
		{
			using UEnumType = std::underlying_type<ValueType>::type;
			UEnumType UnderlyingValue = (UEnumType)Value;
			return LexToString(UnderlyingValue);
		}
		else if constexpr (std::is_same_v<FName, ValueType>)
		{
			return Value.ToString();
		}
		else if constexpr (std::is_same_v<FProperty, std::remove_pointer_t<ValueType>>)
		{
			return Value ? Value->GetFullName() : TEXT("null");
		}
		else if constexpr (std::is_base_of_v<UObject, std::remove_pointer_t<ValueType>>)
		{
			return Value ? Value->GetFullName() : TEXT("null");
		}
		else if constexpr (std::is_same_v<FGuid, ValueType>)
		{
			return LexToString(Value);
		}
		else if constexpr (std::is_same_v<FUtf8String, ValueType>)
		{
			return FString(Value);
		}
		else
		{
			return FString::Format(TEXT("{0}"), {Value}); // yeah, this is horrible
		}
	}

	template <typename ObjectType>
	static FString FormatValue(TObjectPtr<ObjectType> Value)
	{
		return FormatValue(Value.Get());
	}

	template <typename DestinationType, typename ValueType>
	void CheckValueMismatch(const DestinationType& Destination, const ValueType& Value, const TCHAR* What, const TCHAR* SubWhat = nullptr) const
	{
		if (Destination != Value)
		{
			LogValueMismatch(Destination, Value, What, SubWhat);
		}
	}

	template <typename DestinationType, typename ValueType>
	void LogValueMismatch(const DestinationType& Destination, const ValueType& Value, const TCHAR* What, const TCHAR* SubWhat = nullptr) const
	{
		if (SubWhat != nullptr)
		{
			LogError(FString::Format(TEXT("'{0}:{1}:{2}' doesn't have the correct value. Expected: '{3}' Got: '{4}'"),
				{Field->GetName(), What, SubWhat, FormatValue(Value), FormatValue(Destination)}));
		}
		else
		{
			LogError(FString::Format(TEXT("'{0}:{1}' doesn't have the correct value. Expected: '{2}' Got: '{3}'"),
				{Field->GetName(), What, FormatValue(Value), FormatValue(Destination)}));
		}
	}

	virtual void LogError(const FString& text) const = 0;

	TObjectPtr<UField> Field;
	bool bIsValidating;
};

struct FInitOrValidateUEnum : FInitOrValidateUField
{
	FInitOrValidateUEnum(UEnum* InEnum, bool bInIsValidating)
		: FInitOrValidateUField(InEnum, bInIsValidating)
	{
	}

	virtual const FInitOrValidateUEnum* AsUEnum() const override { return this; }

	COREUOBJECT_API void SetEnums(TArray<TPair<FName, int64>>& InNames, UEnum::ECppForm InCppForm) const;

	UEnum* GetUEnum() const { return static_cast<UEnum*>(Field.Get()); }
};

struct FInitOrValidateUVerseEnum : FInitOrValidateUEnum
{
	FInitOrValidateUVerseEnum(UVerseEnum* InVerseEnum, bool bInIsValidating)
		: FInitOrValidateUEnum(InVerseEnum, bInIsValidating)
	{
	}

	virtual const FInitOrValidateUVerseEnum* AsUVerseEnum() const override { return this; }

	UVerseEnum* GetUVerseEnum() const { return static_cast<UVerseEnum*>(Field.Get()); }
};

struct FInitOrValidateUStruct : FInitOrValidateUField
{
	FInitOrValidateUStruct(UStruct* InStruct, bool bInIsValidating)
		: FInitOrValidateUField(InStruct, bInIsValidating)
	{
	}

	virtual const FInitOrValidateUStruct* AsUStruct() const override { return this; }

	COREUOBJECT_API void SetSuperStruct(UClass* SuperStruct) const;

	UStruct* GetUStruct() const { return static_cast<UStruct*>(Field.Get()); }
};

struct FInitOrValidateUScriptStruct : FInitOrValidateUStruct
{
	FInitOrValidateUScriptStruct(UScriptStruct* InScriptStruct, bool bInIsValidating)
		: FInitOrValidateUStruct(InScriptStruct, bInIsValidating)
	{
	}

	virtual const FInitOrValidateUScriptStruct* AsUScriptStruct() const override { return this; }

	UScriptStruct* GetUScriptStruct() const { return static_cast<UScriptStruct*>(Field.Get()); }
};

struct FInitOrValidateUVerseStruct : FInitOrValidateUScriptStruct
{
	FInitOrValidateUVerseStruct(UVerseStruct* InVerseStruct, bool bInIsValidating)
		: FInitOrValidateUScriptStruct(InVerseStruct, bInIsValidating)
	{
	}

	virtual const FInitOrValidateUVerseStruct* AsUVerseStruct() const override { return this; }

	COREUOBJECT_API void SetVerseClassFlags(uint32 ClassFlags, bool bSetFlags, const TCHAR* What) const;
	COREUOBJECT_API void ForceVerseClassFlags(uint32 ClassFlags, bool bSetFlags) const;

	UVerseStruct* GetUVerseStruct() const { return static_cast<UVerseStruct*>(Field.Get()); }
};

struct FInitOrValidateUClass : FInitOrValidateUStruct
{
	FInitOrValidateUClass(UClass* InClass, bool bInIsValidating)
		: FInitOrValidateUStruct(InClass, bInIsValidating)
	{
	}

	virtual const FInitOrValidateUClass* AsUClass() const override { return this; }

	COREUOBJECT_API void SetClassFlags(EClassFlags ClassFlags, bool bSetFlags, const TCHAR* What) const;
	COREUOBJECT_API void SetClassFlagsNoValidate(EClassFlags ClassFlags, bool bSetFlags) const;

	UClass* GetUClass() const { return static_cast<UClass*>(Field.Get()); }
};

struct FInitOrValidateUVerseClass : FInitOrValidateUClass
{
	FInitOrValidateUVerseClass(UVerseClass* InVerseClass, bool bInIsValidating)
		: FInitOrValidateUClass(InVerseClass, bInIsValidating)
	{
	}

	virtual const FInitOrValidateUVerseClass* AsUVerseClass() const override { return this; }

	COREUOBJECT_API void SetVerseClassFlags(uint32 ClassFlags, bool bSetFlags, const TCHAR* What) const;
	COREUOBJECT_API void ForceVerseClassFlags(uint32 ClassFlags, bool bSetFlags) const;

	UVerseClass* GetUVerseClass() const { return static_cast<UVerseClass*>(Field.Get()); }

	COREUOBJECT_API bool AddInterface(UClass* InterfaceClass, EAddInterfaceType InterfaceType);
	COREUOBJECT_API void ValidateInterfaces();

private:
	TArray<UClass*> Interfaces;
	TArray<UClass*> DirectInterfaces;
};

template <typename UEType>
struct FInitOrValidatorSelector
{
};

template <>
struct FInitOrValidatorSelector<UVerseClass>
{
	using Validator = FInitOrValidateUVerseClass;
};

template <>
struct FInitOrValidatorSelector<UVerseStruct>
{
	using Validator = FInitOrValidateUVerseStruct;
};

template <>
struct FInitOrValidatorSelector<UVerseEnum>
{
	using Validator = FInitOrValidateUVerseEnum;
};

} // namespace Verse

#endif // (WITH_VERSE_COMPILER && WITH_VERSE_BPVM) || WITH_VERSE_VM

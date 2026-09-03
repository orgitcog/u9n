// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/UnrealString.h"
#include "CoreMinimal.h"

namespace Verse
{

enum class EAttributeValueType
{
	Logic,
	Int,
	Float,
	Type,
	Class,
	Array,
	String,
};

class CAttributeValue
{
public:
	CAttributeValue(EAttributeValueType InType)
		: Type(InType)
	{
	}

	EAttributeValueType Type;
};

class CAttributeLogicValue : public CAttributeValue
{
public:
	CAttributeLogicValue(bool InValue)
		: CAttributeValue(EAttributeValueType::Logic)
		, Value(InValue)
	{
	}

	bool Value;
};

class CAttributeIntValue : public CAttributeValue
{
public:
	CAttributeIntValue(int64 InValue)
		: CAttributeValue(EAttributeValueType::Int)
		, Value(InValue)
	{
	}

	int64 Value;
};

class CAttributeFloatValue : public CAttributeValue
{
public:
	CAttributeFloatValue(double InValue)
		: CAttributeValue(EAttributeValueType::Float)
		, Value(InValue)
	{
	}

	double Value;
};

class CAttributeTypeValue : public CAttributeValue
{
public:
	CAttributeTypeValue()
		: CAttributeValue(EAttributeValueType::Type)
	{
	}

	FString TypeName;
	FString FullTypeName;
	TArray<FString> TypeArgs;
};

using CAttributeValueMap = TMap<FName, TSharedPtr<CAttributeValue>>;

class CAttributeClassValue : public CAttributeValue
{
public:
	CAttributeClassValue()
		: CAttributeValue(EAttributeValueType::Class)
	{
	}

	TSharedPtr<CAttributeTypeValue> ClassType;
	CAttributeValueMap Value;
};

class CAttributeArrayValue : public CAttributeValue
{
public:
	CAttributeArrayValue()
		: CAttributeValue(EAttributeValueType::Array)
	{
	}

	TArray<TSharedPtr<CAttributeValue>> Value;
};

class CAttributeStringValue : public CAttributeValue
{
public:
	CAttributeStringValue(FString InValue)
		: CAttributeValue(EAttributeValueType::String)
		, Value(InValue)
	{
	}

	FString Value;
};

/**
 * Interface for native processing of Verse Custom Attributes
 * ----------------------------------------------------------
 * Any Verse attribute that has the attribute @customattribhandler will attempt to call back into native
 * code at UClass construction time, which should be handled by an implementation of this interface.
 *
 * In general, these handlers should be implemented in the native module corresponding to the Verse module
 * that declares the custom attribute. In StartupModule() in that module, the handler should add itself
 * to the AttributeHandlers map, and when UClasses are constructed, any custom attributes with registered
 * names will be processed. When the instance is cleared, the handler is unregistered automatically in the
 * destructor.
 */
class ICustomAttributeHandler
{
public:
	COREUOBJECT_API virtual ~ICustomAttributeHandler();

	/** Static helper to search for a handler registered for a given custom attribute name */
	COREUOBJECT_API static ICustomAttributeHandler* FindHandlerForAttribute(const FName AttributeName);

	/**
	 * Handlers to be implemented by each custom attribute, for each type that custom attributes can be applied to.  Returns false if
	 * we were unable to process for the specified name, and implies the linker task will retry again until it succeeds.
	 */
	COREUOBJECT_API virtual bool ProcessAttribute(const CAttributeValue& Payload, UStruct* UeStruct, TArray<FString>& OutErrorMessages);
	COREUOBJECT_API virtual bool ProcessAttribute(const CAttributeValue& Payload, FProperty* UeProperty, TArray<FString>& OutErrorMessages);
	COREUOBJECT_API virtual bool ProcessAttribute(const CAttributeValue& Payload, UFunction* UeFunction, TArray<FString>& OutErrorMessages);
	COREUOBJECT_API virtual bool ProcessAttribute(const CAttributeValue& Payload, UEnum* UeEnum, TArray<FString>& OutErrorMessages);

protected:
	/** A static map of the name of the custom attribute, to the handler responsible for processing it */
	COREUOBJECT_API static TMap<FName, ICustomAttributeHandler*> AttributeHandlers;
};

} // namespace Verse
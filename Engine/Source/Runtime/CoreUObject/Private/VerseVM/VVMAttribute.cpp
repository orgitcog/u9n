// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMAttribute.h"
#include "VerseVM/Inline/VVMObjectInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMClass.h"
#include "VerseVM/VVMContext.h"
#include "VerseVM/VVMCustomAttributeHandler.h"
#include "VerseVM/VVMNames.h"
#include "VerseVM/VVMOption.h"
#include "VerseVM/VVMValue.h"
#include "VerseVM/VVMValueObject.h"
#include "VerseVM/VVMValuePrinting.h"

namespace Verse
{

template <typename DefinitionType>
FAttributeElement::FAttributeElement(DefinitionType* InUeDefinition)
	: UeDefinition(InUeDefinition)
	, InvokeHandler([](ICustomAttributeHandler* Handler, const CAttributeValue& Payload, void* UeDefinition, TArray<FString>& OutErrors) {
		return Handler->ProcessAttribute(Payload, static_cast<DefinitionType*>(UeDefinition), OutErrors);
	})
{
}

template FAttributeElement::FAttributeElement(UStruct* InUeDefinition);
template FAttributeElement::FAttributeElement(FProperty* InUeDefinition);
template FAttributeElement::FAttributeElement(UFunction* InUeDefinition);
template FAttributeElement::FAttributeElement(UEnum* InUeDefinition);

namespace
{
// TODO: Ideally, ICustomAttributeHandler would work with native attribute class objects, rather than CAttributeValue.
// This would eliminate the need for this conversion, and make custom handlers more strongly typed.
TSharedPtr<CAttributeValue> AttributeFromVValue(FAllocationContext Context, VValue AttributeValue)
{
	if (AttributeValue.IsLogic())
	{
		return MakeShared<CAttributeLogicValue>(AttributeValue.AsBool());
	}
	else if (AttributeValue.IsInt())
	{
		return MakeShared<CAttributeIntValue>(AttributeValue.AsInt32());
	}
	else if (AttributeValue.IsFloat())
	{
		return MakeShared<CAttributeFloatValue>(AttributeValue.AsFloat().AsDouble());
	}
	else if (VClass* AttributeType = AttributeValue.DynamicCast<VClass>())
	{
		TSharedPtr<CAttributeTypeValue> TypePayload = MakeShared<CAttributeTypeValue>();
		TypePayload->TypeName = FString(AttributeType->GetBaseName().AsStringView());
		TypePayload->FullTypeName = TypePayload->TypeName;
		return TypePayload;
	}
	else if (VValueObject* AttributeClass = AttributeValue.DynamicCast<VValueObject>())
	{
		VEmergentType* EmergentType = AttributeClass->GetEmergentType();

		TSharedPtr<CAttributeClassValue> ClassPayload = MakeShared<CAttributeClassValue>();
		ClassPayload->ClassType = StaticCastSharedPtr<CAttributeTypeValue>(AttributeFromVValue(Context, *EmergentType->Type));
		for (VShape::FieldsMap::TIterator I = EmergentType->Shape->CreateFieldsIterator(); I; ++I)
		{
			if (I->Value.Type == EFieldType::Offset)
			{
				VUniqueString& Name = *I->Key;
				FOpResult Result = AttributeClass->LoadField(Context, Name);
				V_DIE_UNLESS(Result.IsReturn()); // Loading a field of type EFieldType::Offset should always be safe.
				ClassPayload->Value.Emplace(FName(Verse::Names::RemoveQualifier(Name.AsStringView())), AttributeFromVValue(Context, Result.Value));
			}
		}
		return ClassPayload;
	}
	else if (AttributeValue.IsUObject())
	{
		UObject* AttributeUObject = AttributeValue.AsUObject();
		UVerseClass* AttributeUVerseClass = CastChecked<UVerseClass>(AttributeUObject->GetClass());
		VClass* Class = AttributeUVerseClass->Class.Get();
		VShape* Shape = AttributeUVerseClass->Shape.Get();

		TSharedPtr<CAttributeClassValue> ClassPayload = MakeShared<CAttributeClassValue>();
		ClassPayload->ClassType = StaticCastSharedPtr<CAttributeTypeValue>(AttributeFromVValue(Context, *Class));
		for (VShape::FieldsMap::TIterator I = Shape->CreateFieldsIterator(); I; ++I)
		{
			VUniqueString& Name = *I->Key;
			FOpResult Result = UVerseClass::LoadField(Context, AttributeUObject, Name);
			if (!Result.IsReturn())
			{
				continue;
			}
			ClassPayload->Value.Emplace(FName(Verse::Names::RemoveQualifier(Name.AsStringView())), AttributeFromVValue(Context, Result.Value));
		}
		return ClassPayload;
	}
	else if (VArray* AttributeArray = AttributeValue.DynamicCast<VArray>())
	{
		if (TOptional<FUtf8String> MaybeString = AttributeArray->AsOptionalUtf8String())
		{
			return MakeShared<CAttributeStringValue>(FString(*MaybeString));
		}
		else
		{
			TSharedPtr<CAttributeArrayValue> ArrayPayload = MakeShared<CAttributeArrayValue>();
			for (int32 I = 0; I < AttributeArray->Num(); I++)
			{
				ArrayPayload->Value.Add(AttributeFromVValue(Context, AttributeArray->GetValue(I)));
			}
			return ArrayPayload;
		}
	}
	else if (VOption* AttributeOption = AttributeValue.DynamicCast<VOption>())
	{
		return AttributeFromVValue(Context, AttributeOption->GetValue());
	}
	else
	{
		return nullptr;
	}
}
} // namespace

void FAttributeElement::Apply(FAllocationContext Context, VValue AttributeValue, TArray<FString>& OutErrors)
{
	TSharedPtr<CAttributeValue> Payload = AttributeFromVValue(Context, AttributeValue);
	if (!Payload)
	{
		OutErrors.Add(FString::Printf(TEXT("Unexpected value for attribute: %s"), *FString(AttributeValue.ToString(Context, EValueStringFormat::Cells))));
		return;
	}

	FName AttributeName;
	if (Payload->Type == EAttributeValueType::Type)
	{
		AttributeName = FName(static_cast<CAttributeTypeValue*>(Payload.Get())->TypeName);
	}
	else if (Payload->Type == EAttributeValueType::Class)
	{
		AttributeName = FName(static_cast<CAttributeClassValue*>(Payload.Get())->ClassType->TypeName);
	}

	ICustomAttributeHandler* Handler = ICustomAttributeHandler::FindHandlerForAttribute(AttributeName);
	if (!Handler)
	{
		OutErrors.Add(FString::Printf(TEXT("No custom handler for attribute: %s"), *AttributeName.ToString()));
		return;
	}

	InvokeHandler(Handler, *Payload, UeDefinition, OutErrors);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)

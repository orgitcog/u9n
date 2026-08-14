// Copyright Epic Games, Inc. All Rights Reserved.

#if !WITH_VERSE_BPVM || defined(__INTELLISENSE__)
#include "VerseVM/VVMNativeRef.h"
#include "UObject/EnumProperty.h"
#include "UObject/PropertyOptional.h"
#include "UObject/VerseStringProperty.h"
#include "UObject/VerseValueProperty.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMEnumerationInline.h"
#include "VerseVM/Inline/VVMMarkStackVisitorInline.h"
#include "VerseVM/Inline/VVMVarInline.h"
#include "VerseVM/VVMNativeConverter.h"
#include "VerseVM/VVMVerseEnum.h"
#include "VerseVM/VVMVerseException.h"
#include "VerseVM/VVMVerseStruct.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VNativeRef);
TGlobalTrivialEmergentTypePtr<&VNativeRef::StaticCppClassInfo> VNativeRef::GlobalTrivialEmergentType;

FOpResult VNativeRef::Get(FAllocationContext Context)
{
	V_DIE_UNLESS(Type == EType::FProperty);

	if (UObject* Object = Base.Get().ExtractUObject())
	{
		return Get(Context, Object, UProperty);
	}
	else if (VNativeStruct* Struct = Base.Get().DynamicCast<VNativeStruct>())
	{
		return Get(Context, Struct->GetStruct(), UProperty);
	}
	else
	{
		VERSE_UNREACHABLE();
	}
}

FOpResult VNativeRef::Get(FAllocationContext Context, void* Container, FProperty* Property)
{
	if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		UEnum* UeEnum = EnumProperty->GetEnum();
		if (UeEnum == StaticEnum<EVerseTrue>())
		{
			// Get value of EVerseTrue even though technically not necessary as it's always zero
			EVerseTrue* NativeValue = EnumProperty->ContainerPtrToValuePtr<EVerseTrue>(Container);
			V_RETURN(FNativeConverter::ToVValue(Context, *NativeValue));
		}

		// Convert integer value to corresponding VEnumerator cell
		UVerseEnum* VerseEnum = CastChecked<UVerseEnum>(UeEnum);
		VEnumeration* Enumeration = VerseEnum->Enumeration.Get();
		V_DIE_UNLESS(EnumProperty->GetUnderlyingProperty()->IsA<FByteProperty>());
		uint8* NativeValue = EnumProperty->ContainerPtrToValuePtr<uint8>(Container);
		V_RETURN(Enumeration->GetEnumeratorChecked(*NativeValue));
	}
	else if (FBoolProperty* LogicProperty = CastField<FBoolProperty>(Property))
	{
		bool* NativeValue = LogicProperty->ContainerPtrToValuePtr<bool>(Container);
		V_RETURN(FNativeConverter::ToVValue(Context, *NativeValue));
	}
	else if (FInt64Property* IntProperty = CastField<FInt64Property>(Property))
	{
		int64* NativeValue = IntProperty->ContainerPtrToValuePtr<int64>(Container);
		V_RETURN(FNativeConverter::ToVValue(Context, *NativeValue));
	}
	else if (FDoubleProperty* FloatProperty = CastField<FDoubleProperty>(Property))
	{
		double* NativeValue = FloatProperty->ContainerPtrToValuePtr<double>(Container);
		V_RETURN(FNativeConverter::ToVValue(Context, *NativeValue));
	}
	else if (FByteProperty* CharProperty = CastField<FByteProperty>(Property))
	{
		UTF8CHAR* NativeValue = CharProperty->ContainerPtrToValuePtr<UTF8CHAR>(Container);
		V_RETURN(FNativeConverter::ToVValue(Context, *NativeValue));
	}
	else if (FIntProperty* Char32Property = CastField<FIntProperty>(Property))
	{
		UTF32CHAR* NativeValue = Char32Property->ContainerPtrToValuePtr<UTF32CHAR>(Container);
		V_RETURN(FNativeConverter::ToVValue(Context, *NativeValue));
	}
	else if (FClassProperty* TypeProperty = CastField<FClassProperty>(Property))
	{
		TSubclassOf<UObject>* NativeValue = TypeProperty->ContainerPtrToValuePtr<TSubclassOf<UObject>>(Container);
		V_RETURN(FNativeConverter::ToVValue(Context, *NativeValue));
	}
	else if (FObjectProperty* ClassProperty = CastField<FObjectProperty>(Property))
	{
		TNonNullPtr<UObject>* NativeValue = ClassProperty->ContainerPtrToValuePtr<TNonNullPtr<UObject>>(Container);
		V_RETURN(FNativeConverter::ToVValue(Context, NativeValue->Get()));
	}
	else if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		void* NativeValue = StructProperty->ContainerPtrToValuePtr<void>(Container);
		VClass* Class = nullptr;
		if (UVerseStruct* UeStruct = Cast<UVerseStruct>(StructProperty->Struct))
		{
			if ((UeStruct->VerseClassFlags & VCLASS_Tuple) != 0)
			{
				uint32 NumElements = 0;
				for (TFieldIterator<FProperty> Counter(UeStruct); Counter; ++Counter)
				{
					++NumElements;
				}
				TFieldIterator<FProperty> Iterator(UeStruct);
				// We assume here that the element initializer gets invoked in ascending index order
				VArray& Array = VArray::New(Context, NumElements, [Context, NativeValue, &Iterator](uint32 Index) {
					FOpResult TupleElem = VNativeRef::Get(Context, NativeValue, *Iterator);
					++Iterator;
					V_DIE_UNLESS(TupleElem.IsReturn()); // TODO(johnstiles): propagate exceptions here  #jira SOL-6023
					return TupleElem.Value;
				});
				V_RETURN(Array);
			}

			Class = UeStruct->Class.Get();
		}
		else
		{
			VNamedType* ImportedType = GlobalProgram->LookupImport(Context, StructProperty->Struct);
			V_DIE_UNLESS(ImportedType);
			Class = &ImportedType->StaticCast<VClass>();
		}
		VEmergentType& EmergentType = Class->GetOrCreateEmergentTypeForNativeStruct(Context);
		VNativeStruct& Struct = VNativeStruct::NewUninitialized(Context, EmergentType);
		StructProperty->CopyCompleteValue(Struct.GetStruct(), NativeValue);
		V_RETURN(Struct);
	}
	else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		FScriptArrayHelper_InContainer NativeValue(ArrayProperty, Container);
		VArray& Array = VArray::New(Context, NativeValue.Num(), [Context, ArrayProperty, &NativeValue](uint32 Index) {
			FOpResult ArrayElem = VNativeRef::Get(Context, NativeValue.GetElementPtr(Index), ArrayProperty->Inner);
			V_DIE_UNLESS(ArrayElem.IsReturn()); // TODO(johnstiles): propagate exceptions here  #jira SOL-6023
			return ArrayElem.Value;
		});
		V_RETURN(Array);
	}
	else if (FVerseStringProperty* StringProperty = CastField<FVerseStringProperty>(Property))
	{
		FNativeString* NativeValue = StringProperty->ContainerPtrToValuePtr<FNativeString>(Container);
		V_RETURN(FNativeConverter::ToVValue(Context, *NativeValue));
	}
	else if (FMapProperty* MapProperty = CastField<FMapProperty>(Property))
	{
		FScriptMapHelper_InContainer NativeValue(MapProperty, Container);

		TArray<TPair<VValue, VValue>> Pairs;
		Pairs.Reserve(NativeValue.Num());
		for (auto Pair = NativeValue.CreateIterator(); Pair; ++Pair)
		{
			void* Data = NativeValue.GetPairPtr(Pair);
			FOpResult EntryKey = VNativeRef::Get(Context, Data, MapProperty->KeyProp);
			V_DIE_UNLESS(EntryKey.IsReturn()); // TODO(johnstiles): propagate exceptions here  #jira SOL-6023
			FOpResult EntryValue = VNativeRef::Get(Context, Data, MapProperty->ValueProp);
			V_DIE_UNLESS(EntryValue.IsReturn()); // TODO(johnstiles): propagate exceptions here  #jira SOL-6023
			Pairs.Push({EntryKey.Value, EntryValue.Value});
		}

		V_RETURN(VMapBase::New<VMap>(Context, Pairs.Num(), [&Pairs](uint32 I) { return Pairs[I]; }));
	}
	else if (FOptionalProperty* OptionProperty = CastField<FOptionalProperty>(Property))
	{
		void* NativeValue = OptionProperty->ContainerPtrToValuePtr<void>(Container);
		if (OptionProperty->IsSet(NativeValue))
		{
			FOpResult Inner = VNativeRef::Get(Context, NativeValue, OptionProperty->GetValueProperty());
			V_DIE_UNLESS(Inner.IsReturn()); // TODO(johnstiles): propagate exceptions here  #jira SOL-6023
			V_RETURN(VOption::New(Context, Inner.Value));
		}
		else
		{
			V_RETURN(GlobalFalse());
		}
	}
	else
	{
		VERSE_UNREACHABLE();
	}
}

FOpResult VNativeRef::Set(FAllocationContext Context, VValue Value)
{
	if (UObject* Object = Base.Get().ExtractUObject())
	{
		V_DIE_UNLESS(Type == EType::FProperty);
		return Set<true>(Context, Object, Object, UProperty, Value);
	}
	else if (VNativeStruct* Struct = Base.Get().DynamicCast<VNativeStruct>())
	{
		V_DIE_UNLESS(Type == EType::FProperty);
		return Set<true>(Context, Struct, Struct->GetStruct(), UProperty, Value);
	}
	else
	{
		VERSE_UNREACHABLE();
	}
}

FOpResult VNativeRef::SetNonTransactionally(FAllocationContext Context, VValue Value)
{
	if (UObject* Object = Base.Get().ExtractUObject())
	{
		V_DIE_UNLESS(Type == EType::FProperty);
		return Set<false>(Context, nullptr, Object, UProperty, Value);
	}
	else if (VNativeStruct* Struct = Base.Get().DynamicCast<VNativeStruct>())
	{
		V_DIE_UNLESS(Type == EType::FProperty);
		return Set<false>(Context, nullptr, Struct->GetStruct(), UProperty, Value);
	}
	else
	{
		VERSE_UNREACHABLE();
	}
}

template FOpResult VNativeRef::Set<true>(FAllocationContext Context, UObject* Base, void* Container, FProperty* Property, VValue Value);
template FOpResult VNativeRef::Set<true>(FAllocationContext Context, VNativeStruct* Base, void* Container, FProperty* Property, VValue Value);
template FOpResult VNativeRef::Set<false>(FAllocationContext Context, std::nullptr_t Base, void* Container, FProperty* Property, VValue Value);

#define OP_RESULT_HELPER(Result) \
	if (!Result.IsReturn())      \
	{                            \
		return Result;           \
	}

namespace
{
template <bool bTransactional, typename BaseType, typename FunctionType>
FOpResult WriteImpl(FAllocationContext Context, BaseType Root, FunctionType F)
{
	if constexpr (bTransactional)
	{
		AutoRTFM::EContextStatus Status = AutoRTFM::Close(F);
		if (Status != AutoRTFM::EContextStatus::OnTrack)
		{
			Context.RaiseVerseRuntimeError(ERuntimeDiagnostic::ErrRuntime_NativeInternal, FText::FromString("Closed write to native field did not yield AutoRTFM::EContextStatus::OnTrack"));
			return {FOpResult::Error};
		}
	}
	else
	{
		F();
	}

	return {FOpResult::Return};
}

template <bool bTransactional, typename BaseType, typename ValueType, typename PropertyType>
FOpResult SetImpl(FAllocationContext Context, BaseType Base, void* Container, PropertyType* Property, VValue Value)
{
	TFromVValue<ValueType> NativeValue;
	FOpResult Result = FNativeConverter::FromVValue(Context, Value, NativeValue);
	OP_RESULT_HELPER(Result);

	return WriteImpl<bTransactional>(Context, Base, [Property, Container, &NativeValue] {
		ValueType* ValuePtr = Property->template ContainerPtrToValuePtr<ValueType>(Container);
		*ValuePtr = NativeValue.GetValue();
	});
}
} // namespace

template <bool bTransactional, typename BaseType>
FOpResult VNativeRef::Set(FAllocationContext Context, BaseType Base, void* Container, FProperty* Property, VValue Value)
{
	if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		UEnum* UeEnum = EnumProperty->GetEnum();
		if (UeEnum == StaticEnum<EVerseTrue>())
		{
			return SetImpl<bTransactional, BaseType, EVerseTrue>(Context, Base, Container, EnumProperty, Value);
		}

		V_DIE_UNLESS(Value.IsCellOfType<VEnumerator>() && EnumProperty->GetUnderlyingProperty()->IsA<FByteProperty>());
		VEnumerator& Enumerator = Value.StaticCast<VEnumerator>();
		uint8 NativeValue = static_cast<uint8>(Enumerator.GetIntValue());
		if (NativeValue != Enumerator.GetIntValue())
		{
			Context.RaiseVerseRuntimeError(ERuntimeDiagnostic::ErrRuntime_NativeInternal, FText::FromString("Native enumerators must be integers between 0 and 255"));
			return {FOpResult::Error};
		}
		return WriteImpl<bTransactional>(Context, Base, [EnumProperty, Container, NativeValue] {
			*EnumProperty->ContainerPtrToValuePtr<uint8>(Container) = NativeValue;
		});
	}
	else if (FBoolProperty* LogicProperty = CastField<FBoolProperty>(Property))
	{
		return SetImpl<bTransactional, BaseType, bool>(Context, Base, Container, LogicProperty, Value);
	}
	if (FInt64Property* IntProperty = CastField<FInt64Property>(Property))
	{
		return SetImpl<bTransactional, BaseType, int64>(Context, Base, Container, IntProperty, Value);
	}
	else if (FDoubleProperty* FloatProperty = CastField<FDoubleProperty>(Property))
	{
		return SetImpl<bTransactional, BaseType, double>(Context, Base, Container, FloatProperty, Value);
	}
	else if (FByteProperty* CharProperty = CastField<FByteProperty>(Property))
	{
		return SetImpl<bTransactional, BaseType, UTF8CHAR>(Context, Base, Container, CharProperty, Value);
	}
	else if (FIntProperty* Char32Property = CastField<FIntProperty>(Property))
	{
		return SetImpl<bTransactional, BaseType, UTF32CHAR>(Context, Base, Container, Char32Property, Value);
	}
	else if (FClassProperty* TypeProperty = CastField<FClassProperty>(Property))
	{
		return SetImpl<bTransactional, BaseType, TSubclassOf<UObject>>(Context, Base, Container, TypeProperty, Value);
	}
	else if (FObjectProperty* ClassProperty = CastField<FObjectProperty>(Property))
	{
		return SetImpl<bTransactional, BaseType, TNonNullPtr<UObject>>(Context, Base, Container, ClassProperty, Value);
	}
	else if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		V_REQUIRE_CONCRETE(Value);

		if (UVerseStruct* VerseStruct = Cast<UVerseStruct>(StructProperty->Struct))
		{
			if ((VerseStruct->VerseClassFlags & VCLASS_Tuple) != 0)
			{
				VArrayBase& Array = Value.StaticCast<VArrayBase>();
				// Unpack to temporary storage first
				TArray<std::byte, TInlineAllocator<64>> TempStorage;
				TempStorage.AddUninitialized(VerseStruct->GetStructureSize() + VerseStruct->GetMinAlignment() - 1); // Uses heap memory if inline storage is exceeded
				std::byte* AlignedTempStorage = Align(TempStorage.GetData(), VerseStruct->GetMinAlignment());       // Make sure the buffer is properly aligned
				FOpResult Result = WriteImpl<bTransactional>(Context, nullptr, [&] {
					StructProperty->InitializeValue(AlignedTempStorage);
				});
				OP_RESULT_HELPER(Result);
				TFieldIterator<FProperty> Iterator(VerseStruct);
				for (int32 Index = 0; Index < Array.Num(); ++Index, ++Iterator)
				{
					FOpResult ElemResult = VNativeRef::Set<false>(Context, nullptr, AlignedTempStorage, *Iterator, Array.GetValue(Index));
					OP_RESULT_HELPER(ElemResult);
				}
				// Upon success, copy temporary storage to final destination
				return WriteImpl<bTransactional>(Context, Base, [StructProperty, Container, AlignedTempStorage] {
					void* ValuePtr = StructProperty->ContainerPtrToValuePtr<void>(Container);
					StructProperty->CopyCompleteValue(ValuePtr, AlignedTempStorage);
					StructProperty->DestroyValue(AlignedTempStorage);
				});
			}
		}

		VNativeStruct& Struct = Value.StaticCast<VNativeStruct>();
		checkSlow(VNativeStruct::GetUScriptStruct(*Struct.GetEmergentType()) == StructProperty->Struct);

		return WriteImpl<bTransactional>(Context, Base, [StructProperty, Container, &Struct] {
			void* ValuePtr = StructProperty->ContainerPtrToValuePtr<void>(Container);
			StructProperty->CopyCompleteValue(ValuePtr, Struct.GetStruct());
		});
	}
	else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		V_REQUIRE_CONCRETE(Value);
		V_DIE_UNLESS(Value.IsCellOfType<VArrayBase>());
		VArrayBase& Array = Value.StaticCast<VArrayBase>();

		FScriptArray NativeValue;
		FScriptArrayHelper Helper(ArrayProperty, &NativeValue);
		FOpResult Result = WriteImpl<bTransactional>(Context, nullptr, [&] { Helper.EmptyAndAddValues(Array.Num()); });
		OP_RESULT_HELPER(Result);
		for (int32 Index = 0; Index < Array.Num(); Index++)
		{
			FOpResult ElemResult = VNativeRef::Set<false>(Context, nullptr, Helper.GetElementPtr(Index), ArrayProperty->Inner, Array.GetValue(Index));
			OP_RESULT_HELPER(ElemResult);
		}

		return WriteImpl<bTransactional>(Context, Base, [ArrayProperty, Container, &NativeValue] {
			FScriptArrayHelper_InContainer ValuePtr(ArrayProperty, Container);
			ValuePtr.MoveAssign(&NativeValue);
		});
	}
	else if (FVerseStringProperty* StringProperty = CastField<FVerseStringProperty>(Property))
	{
		return SetImpl<bTransactional, BaseType, FNativeString>(Context, Base, Container, StringProperty, Value);
	}
	else if (FMapProperty* MapProperty = CastField<FMapProperty>(Property))
	{
		V_REQUIRE_CONCRETE(Value);
		V_DIE_UNLESS(Value.IsCellOfType<VMapBase>());
		VMapBase& Map = Value.StaticCast<VMapBase>();

		FScriptMap NativeValue;
		FScriptMapHelper Helper(MapProperty, &NativeValue);
		FOpResult Result = WriteImpl<bTransactional>(Context, nullptr, [&] { Helper.EmptyValues(Map.Num()); });
		OP_RESULT_HELPER(Result);
		for (TPair<VValue, VValue> Pair : Map)
		{
			int32 Index = Helper.AddDefaultValue_Invalid_NeedsRehash();
			FOpResult KeyResult = VNativeRef::Set<false>(Context, nullptr, Helper.GetPairPtr(Index), Helper.GetKeyProperty(), Pair.Key);
			OP_RESULT_HELPER(KeyResult);
			FOpResult ValueResult = VNativeRef::Set<false>(Context, nullptr, Helper.GetPairPtr(Index), Helper.GetValueProperty(), Pair.Value);
			OP_RESULT_HELPER(ValueResult);
		}
		Helper.Rehash();

		return WriteImpl<bTransactional>(Context, Base, [MapProperty, Container, &NativeValue] {
			FScriptMapHelper_InContainer ValuePtr(MapProperty, Container);
			ValuePtr.MoveAssign(&NativeValue);
		});
	}
	else if (FOptionalProperty* OptionProperty = CastField<FOptionalProperty>(Property))
	{
		V_REQUIRE_CONCRETE(Value);

		if (VOption* Option = Value.DynamicCast<VOption>())
		{
			void* Data;
			FOpResult Result = WriteImpl<bTransactional>(Context, Base, [OptionProperty, Container, Value, &Data] {
				void* ValuePtr = OptionProperty->ContainerPtrToValuePtr<void>(Container);
				Data = OptionProperty->MarkSetAndGetInitializedValuePointerToReplace(ValuePtr);
			});
			OP_RESULT_HELPER(Result);

			return VNativeRef::Set<bTransactional>(Context, Base, Data, OptionProperty->GetValueProperty(), Option->GetValue());
		}
		else
		{
			V_DIE_UNLESS(Value == GlobalFalse());

			return WriteImpl<bTransactional>(Context, Base, [OptionProperty, Container] {
				void* ValuePtr = OptionProperty->ContainerPtrToValuePtr<void>(Container);
				OptionProperty->MarkUnset(ValuePtr);
			});
		}
	}
	else
	{
		VERSE_UNREACHABLE();
	}
}

#undef OP_RESULT_HELPER

FOpResult VNativeRef::FreezeImpl(FAllocationContext Context)
{
	return Get(Context);
}

template <typename TVisitor>
void VNativeRef::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Base, TEXT("Base"));
}

} // namespace Verse
#endif

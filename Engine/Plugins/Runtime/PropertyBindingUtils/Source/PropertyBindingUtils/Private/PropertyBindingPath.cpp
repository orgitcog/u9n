// Copyright Epic Games, Inc. All Rights Reserved.
#include "PropertyBindingPath.h"
#include "PropertyBindingDataView.h"
#include "Misc/EnumerateRange.h"
#include "StructUtils/InstancedStructContainer.h"

#if WITH_EDITOR
#include "UObject/CoreRedirects.h"
#include "UObject/Package.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Kismet2/StructureEditorUtils.h"
#endif


#include UE_INLINE_GENERATED_CPP_BY_NAME(PropertyBindingPath)

bool FPropertyBindingPath::FromString(const FStringView InPath)
{
	Segments.Reset();
	
	if (InPath.IsEmpty())
	{
		return true;
	}
	
	auto ParseSegment = [This = this](const FStringView Segment)
	{
		if (Segment.IsEmpty())
		{
			return false;
		}

		int32 FirstBracket = INDEX_NONE;
		int32 LastBracket = INDEX_NONE;
		if (Segment.FindChar(TEXT('['), FirstBracket) && Segment.FindLastChar(TEXT(']'), LastBracket))
		{
			const int32 NameStringLength = FirstBracket;
			const int32 IndexStringLength = LastBracket - FirstBracket - 1;
			if (NameStringLength < 1
				|| IndexStringLength <= 0)
			{
				return false;
			}

			const FStringView NameString = Segment.Left(FirstBracket);
			const FStringView IndexString = Segment.Mid(FirstBracket + 1, IndexStringLength);
			int32 ArrayIndex = INDEX_NONE;
			LexFromString(ArrayIndex, IndexString.GetData());
			if (ArrayIndex < 0)
			{
				return false;
			}
			
			This->AddPathSegment(FName(NameString), ArrayIndex);
		}
		else
		{
			This->AddPathSegment(FName(Segment));
		}
		return true;
	};

	bool bResult = true;
	FStringView Path = InPath;
	int32 FoundChar = INDEX_NONE;
	while (Path.FindChar(TEXT('.'), FoundChar))
	{
		const FStringView Segment = Path.SubStr(0, FoundChar);
		if (!ParseSegment(Segment))
		{
			bResult = false;
			break;
		}
		Path = Path.Mid(FoundChar+1);
	}
	if (bResult)
	{
		bResult = ParseSegment(Path);
	}

	if (!bResult)
	{
		Segments.Reset();
	}
	
	return bResult;
}

bool FPropertyBindingPath::UpdateSegments(const UStruct* BaseStruct, FString* OutError)
{
	return UpdateSegmentsFromValue(FPropertyBindingDataView(BaseStruct, nullptr), OutError);
}

bool FPropertyBindingPath::UpdateSegmentsFromValue(const FPropertyBindingDataView BaseValueView, FString* OutError)
{
	TArray<FPropertyBindingPathIndirection> Indirections;
	if (!ResolveIndirectionsWithValue(BaseValueView, Indirections, OutError, /*bHandleRedirects*/true))
	{
		return false;
	}

	for (const FPropertyBindingPathIndirection& Indirection : Indirections)
	{
		FPropertyBindingPathSegment& Segment = Segments[Indirection.PathSegmentIndex];

		if (Indirection.InstanceStruct != nullptr)
		{
			UE_CLOG(Indirection.InstanceStruct != Segment.GetInstanceStruct(), LogPropertyBindingUtils, Verbose
				, TEXT("Updating instanced struct for segment '%s' in path '%s' from '%s' to '%s'")
				, *Segment.GetName().ToString(), *ToString(), *GetNameSafe(Segment.GetInstanceStruct()), *GetNameSafe(Indirection.InstanceStruct));
			Segment.SetInstanceStruct(Indirection.InstanceStruct, Indirection.GetAccessType());
		}
		else
		{
			UE_CLOG(Segment.GetInstanceStruct(), LogPropertyBindingUtils, Verbose, TEXT("Clearing instanced struct for segment '%s' in path '%s'")
				, *Segment.GetName().ToString(), *ToString());
			Segment.SetInstanceStruct(nullptr, EPropertyBindingPropertyAccessType::Unset);
		}
#if WITH_EDITORONLY_DATA
		if (!Indirection.GetRedirectedName().IsNone())
		{
			Segment.SetName(Indirection.GetRedirectedName());
		}
		Segment.SetPropertyGuid(Indirection.GetPropertyGuid());
#endif
	}

	return true;
}

FString FPropertyBindingPath::ToString(const int32 HighlightedSegment, const TCHAR* HighlightPrefix, const TCHAR* HighlightPostfix, const bool bOutputInstances, const int32 FirstSegment) const
{
	FStringBuilderBase Result;
	for (TEnumerateRef<const FPropertyBindingPathSegment> Segment : EnumerateRange(TConstArrayView<FPropertyBindingPathSegment>(Segments).Mid(FMath::Max(FirstSegment, 0))))
	{
		if (Segment.GetIndex() > 0)
		{
			Result += TEXT(".");
		}
		if (Segment.GetIndex() == HighlightedSegment && HighlightPrefix)
		{
			Result += HighlightPrefix;
		}

		if (bOutputInstances && Segment->GetInstanceStruct())
		{
			Result += FString::Printf(TEXT("(%s)"), *GetNameSafe(Segment->GetInstanceStruct()));
		}

#if WITH_EDITORONLY_DATA
		const UStruct* ParentInstanceStruct = Segment.GetIndex() > 0 ? Segments[Segment.GetIndex() - 1].GetInstanceStruct() : nullptr;
		if (const UUserDefinedStruct* ParentUserDefinedStruct = Cast<const UUserDefinedStruct>(ParentInstanceStruct))
		{
			// Find friendly names for UDS properties (the property name itself has hash in it). 
			const FString FriendlyName = FStructureEditorUtils::GetVariableFriendlyName(ParentUserDefinedStruct, Segment->GetPropertyGuid());
			if (!FriendlyName.IsEmpty())
			{
				Result += FriendlyName;
			}
			else
			{
				Result += Segment->GetName().ToString();
			}
		}
		else
#endif // WITH_EDITORONLY_DATA
		{
			Result += Segment->GetName().ToString();
		}

		if (Segment->GetArrayIndex() >= 0)
		{
			Result += FString::Printf(TEXT("[%d]"), Segment->GetArrayIndex());
		}

		if (Segment.GetIndex() == HighlightedSegment && HighlightPostfix)
		{
			Result += HighlightPostfix;
		}
	}
	return Result.ToString();
}

bool FPropertyBindingPath::Includes(const FPropertyBindingPath& Other) const
{
#if WITH_EDITORONLY_DATA
	if (StructID != Other.StructID)
	{
		return false;
	}
#endif // WITH_EDITORONLY_DATA

	if (Segments.Num() < Other.Segments.Num())
	{
		return false;
	}

	for (TEnumerateRef<const FPropertyBindingPathSegment> OtherSegment : EnumerateRange(Other.Segments))
	{
		if (*OtherSegment != Segments[OtherSegment.GetIndex()])
		{
			return false;
		}
	}

	return true;
}

bool FPropertyBindingPath::ResolveIndirections(const UStruct* BaseStruct, TArray<FPropertyBindingPathIndirection>& OutIndirections, FString* OutError, bool bHandleRedirects) const
{
	return ResolveIndirectionsWithValue(FPropertyBindingDataView(BaseStruct, nullptr), OutIndirections, OutError, bHandleRedirects);
}

bool FPropertyBindingPath::ResolveIndirectionsWithValue(const FPropertyBindingDataView BaseValueView, TArray<FPropertyBindingPathIndirection>& OutIndirections, FString* OutError, bool bHandleRedirects) const
{
	OutIndirections.Reset();
	if (OutError)
	{
		OutError->Reset();
	}
	
	// Nothing to do for an empty path.
	if (IsPathEmpty())
	{
		return true;
	}

	const uint8* CurrentAddress = static_cast<const uint8*>(BaseValueView.GetMemory());
	const UStruct* CurrentStruct = BaseValueView.GetStruct();
	
	for (const TEnumerateRef<const FPropertyBindingPathSegment> Segment : EnumerateRange(Segments))
	{
		if (CurrentStruct == nullptr)
		{
			if (OutError)
			{
				*OutError = FString::Printf(TEXT("Malformed path '%s'."),
					*ToString(Segment.GetIndex(), TEXT("<"), TEXT(">")));
			}
			OutIndirections.Reset();
			return false;
		}

		const FProperty* Property = CurrentStruct->FindPropertyByName(Segment->GetName());
		const bool bWithValue = CurrentAddress != nullptr;

#if WITH_EDITORONLY_DATA
		FName RedirectedName;
		FGuid PropertyGuid = Segment->GetPropertyGuid();

		// Try to fix the path in editor.
		if (bHandleRedirects)
		{
			
			// Check if there's a core redirect for it.
			if (!Property)
			{
				// Try to match by property ID (Blueprint or User Defined Struct).
				if (Segment->GetPropertyGuid().IsValid())
				{
					if (const UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(CurrentStruct))
					{
						if (const FName* Name = BlueprintClass->PropertyGuids.FindKey(Segment->GetPropertyGuid()))
						{
							RedirectedName = *Name;
							Property = CurrentStruct->FindPropertyByName(RedirectedName);
						}
					}
					else if (const UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(CurrentStruct))
					{
						if (FProperty* FoundProperty = FStructureEditorUtils::GetPropertyByGuid(UserDefinedStruct, Segment->GetPropertyGuid()))
						{
							RedirectedName = FoundProperty->GetFName();
							Property = FoundProperty;
						}
					}
					else if (const UPropertyBag* PropertyBag = Cast<UPropertyBag>(CurrentStruct))
					{
						if (const FPropertyBagPropertyDesc* Desc = PropertyBag->FindPropertyDescByID(Segment->GetPropertyGuid()))
						{
							if (Desc->CachedProperty)
							{
								RedirectedName = Desc->CachedProperty->GetFName();
								Property = Desc->CachedProperty;
							}
						}
					}
				}
				else
				{
					// Try core redirect
					const FCoreRedirectObjectName OldPropertyName(Segment->GetName(), CurrentStruct->GetFName(), *CurrentStruct->GetOutermost()->GetPathName());
					const FCoreRedirectObjectName NewPropertyName = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Property, OldPropertyName);
					if (OldPropertyName != NewPropertyName)
					{
						// Cached the result for later use.
						RedirectedName = NewPropertyName.ObjectName;

						Property = CurrentStruct->FindPropertyByName(RedirectedName);
					}
				}
			}

			// Update PropertyGuid 
			if (Property)
			{
				const FName PropertyName = !RedirectedName.IsNone() ? RedirectedName : Segment->GetName();
				if (const UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(CurrentStruct))
				{
					if (const FGuid* VarGuid = BlueprintClass->PropertyGuids.Find(PropertyName))
					{
						PropertyGuid = *VarGuid;
					}
				}
				else if (const UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(CurrentStruct))
				{
					// Parse Guid from UDS property name.
					PropertyGuid = FStructureEditorUtils::GetGuidFromPropertyName(PropertyName);
				}
				else if (const UPropertyBag* PropertyBag = Cast<UPropertyBag>(CurrentStruct))
				{
					if (const FPropertyBagPropertyDesc* Desc = PropertyBag->FindPropertyDescByPropertyName(PropertyName))
					{
						PropertyGuid = Desc->ID;
					}
				}
			}
		}
#endif // WITH_EDITORONLY_DATA

		if (!Property)
		{
			if (OutError)
			{
				*OutError = FString::Printf(TEXT("Malformed path '%s', could not find property '%s%s::%s'."),
					*ToString(Segment.GetIndex(), TEXT("<"), TEXT(">")),
					CurrentStruct->GetPrefixCPP(), *CurrentStruct->GetName(), *Segment->GetName().ToString());
			}
			OutIndirections.Reset();
			return false;
		}

		const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);
		int ArrayIndex = INDEX_NONE;
		int32 Offset = INDEX_NONE;

		if (ArrayProperty && Segment->GetArrayIndex() != INDEX_NONE)
		{
			FPropertyBindingPathIndirection& Indirection = OutIndirections.AddDefaulted_GetRef();
			Indirection.Property = Property;
			Indirection.ContainerAddress = CurrentAddress;
			Indirection.ContainerStruct = CurrentStruct;
			Indirection.InstanceStruct = nullptr;
			Indirection.ArrayIndex = Segment->GetArrayIndex();
			Indirection.PropertyOffset = ArrayProperty->GetOffset_ForInternal();
			Indirection.PathSegmentIndex = Segment.GetIndex();
			Indirection.AccessType = EPropertyBindingPropertyAccessType::IndexArray;
#if WITH_EDITORONLY_DATA
			Indirection.RedirectedName = RedirectedName;
			Indirection.PropertyGuid = PropertyGuid;
#endif
			
			ArrayIndex = 0;
			Offset = 0;
			Property = ArrayProperty->Inner;

			if (bWithValue)
			{
				FScriptArrayHelper Helper(ArrayProperty, CurrentAddress + ArrayProperty->GetOffset_ForInternal());
				if (!Helper.IsValidIndex(Segment->GetArrayIndex()))
				{
					if (OutError)
					{
						*OutError = FString::Printf(TEXT("Index %d out of range (num elements %d) trying to access dynamic array '%s'."),
							Segment->GetArrayIndex(), Helper.Num(), *ToString(Segment.GetIndex(), TEXT("<"), TEXT(">")));
					}
					OutIndirections.Reset();
					return false;
				}
				CurrentAddress = Helper.GetRawPtr(Segment->GetArrayIndex());
			}
		}
		else
		{
			// FInstancedStructContainer needs to handle the ArrayIndex manually
			const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
			if (StructProperty && StructProperty->Struct == TBaseStructure<FInstancedStructContainer>::Get())
			{
				ArrayIndex = FMath::Max(0, Segment->GetArrayIndex());
				Offset = Property->GetOffset_ForInternal();
			}
			else
			{
				if (Segment->GetArrayIndex() >= Property->ArrayDim)
				{
					if (OutError)
					{
						*OutError = FString::Printf(TEXT("Index %d out of range %d trying to access static array '%s'."),
							Segment->GetArrayIndex(), Property->ArrayDim, *ToString(Segment.GetIndex(), TEXT("<"), TEXT(">")));
					}
					OutIndirections.Reset();
					return false;
				}
				ArrayIndex = FMath::Max(0, Segment->GetArrayIndex());
				Offset = Property->GetOffset_ForInternal() + Property->GetElementSize() * ArrayIndex;
			}
		}

		FPropertyBindingPathIndirection& Indirection = OutIndirections.AddDefaulted_GetRef();
		Indirection.Property = Property;
		Indirection.ContainerAddress = CurrentAddress;
		Indirection.ContainerStruct = CurrentStruct;
		Indirection.ArrayIndex = ArrayIndex;
		Indirection.PropertyOffset = Offset;
		Indirection.PathSegmentIndex = Segment.GetIndex();
		Indirection.AccessType = EPropertyBindingPropertyAccessType::Offset; 
#if WITH_EDITORONLY_DATA
		Indirection.RedirectedName = RedirectedName;
		Indirection.PropertyGuid = PropertyGuid;
#endif
		const bool bLastSegment = Segment.GetIndex() == (Segments.Num() - 1);

		if (!bLastSegment)
		{
			if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				if (bWithValue)
				{
					// For StructInstance types:
					// The property path is pointing into the instanced struct, it must be present.
					// @TODO:	We could potentially check the BaseStruct metadata in editor (for similar behavior as objects)
					//			Omitting for now to have matching functionality in editor and runtime.
					auto SetStructInstanceIndirection = [this, &CurrentAddress, &CurrentStruct, &Indirection, &OutError, &OutIndirections]
						(const UScriptStruct* ScriptStruct, const uint8* StructMemory, const int32 SegmentIndex, const UStruct* SegmentScriptStruct)
						{
							if (ScriptStruct == nullptr || StructMemory == nullptr)
							{
								if (OutError)
								{
									*OutError = FString::Printf(TEXT("Expecting valid instanced struct value at path '%s'."),
										*ToString(SegmentIndex, TEXT("<"), TEXT(">")));
								}
								OutIndirections.Reset();
								return false;
							}

							CurrentAddress = StructMemory;
							CurrentStruct = ScriptStruct;
							Indirection.InstanceStruct = CurrentStruct;
							Indirection.ArrayIndex = INDEX_NONE;
							return true;
						};

					if (StructProperty->Struct == TBaseStructure<FInstancedStruct>::Get())
					{
						const FInstancedStruct& InstancedStruct = *reinterpret_cast<const FInstancedStruct*>((uint8*)CurrentAddress + Offset);
						if (!SetStructInstanceIndirection(InstancedStruct.GetScriptStruct(), InstancedStruct.GetMemory(), Segment.GetIndex(), Segment->GetInstanceStruct()))
						{
							return false;
						}
						Indirection.AccessType = EPropertyBindingPropertyAccessType::StructInstance;
					}
					else if (StructProperty->Struct == TBaseStructure<FSharedStruct>::Get())
					{
						const FSharedStruct& SharedStruct = *reinterpret_cast<const FSharedStruct*>((uint8*)CurrentAddress + Offset);
						if (!SetStructInstanceIndirection(SharedStruct.GetScriptStruct(), SharedStruct.GetMemory(), Segment.GetIndex(), Segment->GetInstanceStruct()))
						{
							return false;
						}
						Indirection.AccessType = EPropertyBindingPropertyAccessType::SharedStruct;
					}
					else if (StructProperty->Struct == TBaseStructure<FInstancedStructContainer>::Get())
					{
						const FInstancedStructContainer& InstancedStructContainer = *reinterpret_cast<const FInstancedStructContainer*>((uint8*)CurrentAddress + Offset);
						check(InstancedStructContainer.Num() > ArrayIndex);

						const FConstStructView StructView = InstancedStructContainer[ArrayIndex];
						if (!SetStructInstanceIndirection(StructView.GetScriptStruct(), StructView.GetMemory(), Segment.GetIndex(), Segment->GetInstanceStruct()))
						{
							return false;
						}
						Indirection.AccessType = EPropertyBindingPropertyAccessType::StructInstanceContainer;
						Indirection.ArrayIndex = ArrayIndex;
					}
					else
					{
						CurrentAddress = (uint8*)CurrentAddress + Offset;
						CurrentStruct = StructProperty->Struct;
						Indirection.AccessType = EPropertyBindingPropertyAccessType::Offset;
					}
				}
				else
				{
					if (Segment->GetInstanceStruct())
					{
						CurrentStruct = Segment->GetInstanceStruct();
						Indirection.InstanceStruct = CurrentStruct;
						Indirection.AccessType = Segment->GetInstancedStructAccessType();
					}
					else
					{
						CurrentStruct = StructProperty->Struct;
						Indirection.AccessType = EPropertyBindingPropertyAccessType::Offset;
					}
				}
			}
			else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
			{
				if (bWithValue)
				{
					const UObject* Object = *reinterpret_cast<UObject* const*>(CurrentAddress + Offset);
					CurrentAddress = reinterpret_cast<const uint8*>(Object);
					
					// The property path is pointing into the object, if the object is present use it's specific type, otherwise use the type of the pointer.
					if (Object)
					{
						CurrentStruct = Object->GetClass();
						Indirection.InstanceStruct = CurrentStruct;
						Indirection.AccessType = EPropertyBindingPropertyAccessType::ObjectInstance;
					}
					else
					{
						CurrentStruct = ObjectProperty->PropertyClass;
						Indirection.AccessType = EPropertyBindingPropertyAccessType::Object;
					}
				}
				else
				{
					if (Segment->GetInstanceStruct())
					{
						CurrentStruct = Segment->GetInstanceStruct();
						Indirection.InstanceStruct = CurrentStruct;
						Indirection.AccessType = EPropertyBindingPropertyAccessType::ObjectInstance;
					}
					else
					{
						CurrentStruct = ObjectProperty->PropertyClass;
						Indirection.AccessType = EPropertyBindingPropertyAccessType::Object;
					}
				}
			}
			// Check to see if this is a simple weak object property (eg. not an array of weak objects).
			else if (const FWeakObjectProperty* WeakObjectProperty = CastField<FWeakObjectProperty>(Property))
			{
				if (bWithValue)
				{
					const TWeakObjectPtr<UObject>& WeakObjectPtr = *reinterpret_cast<const TWeakObjectPtr<UObject>*>(CurrentAddress + Offset);
					const UObject* Object = WeakObjectPtr.Get();
					CurrentAddress = reinterpret_cast<const uint8*>(Object);

					if (Object)
					{
						CurrentStruct = Object->GetClass();
						Indirection.InstanceStruct = CurrentStruct;
					}
				}
				else
				{
					CurrentStruct = WeakObjectProperty->PropertyClass;
				}

				Indirection.AccessType = EPropertyBindingPropertyAccessType::WeakObject;
			}
			// Check to see if this is a simple soft object property (eg. not an array of soft objects).
			else if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
			{
				if (bWithValue)
				{
					const FSoftObjectPtr& SoftObjectPtr = *reinterpret_cast<const FSoftObjectPtr*>(CurrentAddress + Offset);
					const UObject* Object = SoftObjectPtr.Get();
					CurrentAddress = reinterpret_cast<const uint8*>(Object);

					if (Object)
					{
						CurrentStruct = Object->GetClass();
						Indirection.InstanceStruct = CurrentStruct;
					}
				}
				else
				{
					CurrentStruct = SoftObjectProperty->PropertyClass;
				}

				Indirection.AccessType = EPropertyBindingPropertyAccessType::SoftObject;
			}
			else
			{
				// We get here if we encounter a property type that is not supported for indirection (e.g. Map or Set).
				if (OutError)
				{
					*OutError = FString::Printf(TEXT("Unsupported property indirection type %s in path '%s'."),
						*Property->GetCPPType(), *ToString(Segment.GetIndex(), TEXT("<"), TEXT(">")));
				}
				OutIndirections.Reset();
				return false;
			}
		}
	}

	return true;
}

bool FPropertyBindingPath::operator==(const FPropertyBindingPath& RHS) const
{
#if WITH_EDITORONLY_DATA
	if (StructID != RHS.StructID)
	{
		return false;
	}
#endif // WITH_EDITORONLY_DATA
	if (Segments.Num() != RHS.Segments.Num())
	{
		return false;
	}

	for (TEnumerateRef<const FPropertyBindingPathSegment> Segment : EnumerateRange(Segments))
	{
		if (*Segment != RHS.Segments[Segment.GetIndex()])
		{
			return false;
		}
	}

	return true;
}
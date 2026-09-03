// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeBindingExtension.h"
#include "EdGraphSchema_K2.h"
#include "Features/IModularFeatures.h"
#include "IPropertyAccessEditor.h"
#include "StateTreeAnyEnum.h"
#include "StateTreeCompiler.h"
#include "StateTreeEditorPropertyBindings.h"
#include "StateTreeNodeBase.h"
#include "UObject/EnumProperty.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "StateTreePropertyRef.h"
#include "StateTreePropertyRefHelpers.h"
#include "DetailLayoutBuilder.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "IPropertyUtilities.h"
#endif // UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "IDetailChildrenBuilder.h"
#include "IStructureDataProvider.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "PropertyBindingExtension.h"
#include "StateTreeEditorNodeUtils.h"
#include "StateTreeEditorModule.h"
#include "StateTreeEditorData.h"
#include "StateTreeDelegate.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"

namespace UE::StateTree::PropertyBinding
{

/**
 * Information for the types gathered from a FStateTreePropertyRef property meta-data 
 * Kept this type to facilitate introduction of base type FPropertyInfoOverride.
 */
struct FRefTypeInfo : UE::PropertyBinding::FPropertyInfoOverride
{
};

const FName AllowAnyBindingName(TEXT("AllowAnyBinding"));

UObject* FindEditorBindingsOwner(UObject* InObject)
{
	UObject* Result = nullptr;

	for (UObject* Outer = InObject; Outer; Outer = Outer->GetOuter())
	{
		IStateTreeEditorPropertyBindingsOwner* BindingOwner = Cast<IStateTreeEditorPropertyBindingsOwner>(Outer);
		if (BindingOwner)
		{
			Result = Outer;
			break;
		}
	}
	return Result;
}


static bool IsDelegateDispatcherProperty(const FProperty* Property)
{
	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		return StructProperty->Struct == FStateTreeDelegateDispatcher::StaticStruct();
	}

	return false;
}

static bool IsDelegateListenerProperty(const FProperty* Property)
{
	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		return StructProperty->Struct == FStateTreeDelegateListener::StaticStruct() || StructProperty->Struct == FStateTreeTransitionDelegateListener::StaticStruct();
	}

	return false;
}


FOnStateTreePropertyBindingChanged STATETREEEDITORMODULE_API OnStateTreePropertyBindingChanged;

struct FStateTreeCachedBindingData : UE::PropertyBinding::FCachedBindingData, TSharedFromThis<FStateTreeCachedBindingData>
{
	using Super = FCachedBindingData;

	using FCachedBindingData::FCachedBindingData;

	virtual bool AddBindingInternal(TConstStructView<FPropertyBindingBindableStructDescriptor> InDescriptor,
		FPropertyBindingPath& InOutSourcePath,
		const FPropertyBindingPath& InTargetPath) override
	{
		const FStateTreeBindableStructDesc& SourceDesc = InDescriptor.Get<FStateTreeBindableStructDesc>();
		if (SourceDesc.DataSource == EStateTreeBindableStructSource::PropertyFunction)
		{
			IStateTreeEditorPropertyBindingsOwner* BindingsOwner = Cast<IStateTreeEditorPropertyBindingsOwner>(GetOwner());
			const UScriptStruct* PropertyFunctionNodeStruct = nullptr;

			BindingsOwner->EnumerateBindablePropertyFunctionNodes([ID = SourceDesc.ID, &PropertyFunctionNodeStruct](const UScriptStruct* NodeStruct, const FStateTreeBindableStructDesc& Desc, const FPropertyBindingDataView Value)
				{
					if (Desc.ID == ID)
					{
						PropertyFunctionNodeStruct = NodeStruct;
						return EStateTreeVisitor::Break;
					}

					return EStateTreeVisitor::Continue;
				});

			if (ensure(PropertyFunctionNodeStruct))
			{
				FStateTreeEditorPropertyBindings* EditorBindings = BindingsOwner->GetPropertyEditorBindings();

				// If there are no segments, bindings leads directly into source struct's single output property. It's path has to be recovered.
				if (InOutSourcePath.NumSegments() == 0)
				{
					const FProperty* SingleOutputProperty = UE::StateTree::GetStructSingleOutputProperty(*SourceDesc.Struct);
					check(SingleOutputProperty);

					FPropertyBindingPathSegment SingleOutputPropertySegment = FPropertyBindingPathSegment(SingleOutputProperty->GetFName());
					InOutSourcePath = EditorBindings->AddFunctionBinding(PropertyFunctionNodeStruct, { SingleOutputPropertySegment }, InTargetPath);
				}
				else
				{
					InOutSourcePath = EditorBindings->AddFunctionBinding(PropertyFunctionNodeStruct, InOutSourcePath.GetSegments(), InTargetPath);
				}

				return true;
			}
		}
		return false;
	}

	virtual void UpdatePropertyReferenceTooltip(const FProperty& InProperty, FTextBuilder& InOutTextBuilder) const override
	{
		if (InProperty.HasMetaData(PropertyRefHelpers::IsRefToArrayName))
		{
			InOutTextBuilder.AppendLineFormat(LOCTEXT("PropertyRefBindingTooltipArray", "Supported types are Array of {0}"),
				FText::FromString(InProperty.GetMetaData(PropertyRefHelpers::RefTypeName)));
		}
		else
		{
			InOutTextBuilder.AppendLineFormat(LOCTEXT("PropertyRefBindingTooltip", "Supported types are {0}"),
				FText::FromString(InProperty.GetMetaData(PropertyRefHelpers::RefTypeName)));
			if (InProperty.HasMetaData(PropertyRefHelpers::CanRefToArrayName))
			{
				InOutTextBuilder.AppendLine(LOCTEXT("PropertyRefBindingTooltipCanSupportArray", "Supports Arrays"));
			}
		}
	}

	virtual void UpdateSourcePropertyPath(const TConstStructView<FPropertyBindingBindableStructDescriptor> InDescriptor, const FPropertyBindingPath& InSourcePath, FString& OutString) override
	{
		const FStateTreeBindableStructDesc& SourceDesc = InDescriptor.Get<FStateTreeBindableStructDesc>();
		// Making first segment of the path invisible for the user if it's property function's single output property.
		if (SourceDesc.DataSource == EStateTreeBindableStructSource::PropertyFunction && GetStructSingleOutputProperty(*SourceDesc.Struct))
		{
			OutString = InSourcePath.ToString(/*HighlightedSegment*/ INDEX_NONE, /*HighlightPrefix*/ nullptr, /*HighlightPostfix*/ nullptr, /*bOutputInstances*/ false, 1);
		}
	}

	virtual void GetSourceDataViewForNewBinding(TNotNull<IPropertyBindingBindingCollectionOwner*> InBindingsOwner, TConstStructView<FPropertyBindingBindableStructDescriptor> InDescriptor, FPropertyBindingDataView& OutSourceDataView) override
	{
		const FStateTreeBindableStructDesc& SourceDesc = InDescriptor.Get<FStateTreeBindableStructDesc>();
		if (SourceDesc.DataSource == EStateTreeBindableStructSource::PropertyFunction)
		{
			OutSourceDataView = FPropertyBindingDataView(SourceDesc.Struct, nullptr);
		}
		else
		{
			Super::GetSourceDataViewForNewBinding(InBindingsOwner, InDescriptor, OutSourceDataView);
		}
	}

	virtual bool GetPinTypeAndIconForProperty(const FProperty& InProperty, FPropertyBindingDataView InTargetDataView, FEdGraphPinType& OutPinType, FName& OutIconName) const override
	{
		const bool bIsPropertyRef = PropertyRefHelpers::IsPropertyRef(InProperty);;
		if (bIsPropertyRef && InTargetDataView.IsValid())
		{
			// Use internal type to construct PinType if it's property of PropertyRef type.
			TArray<FPropertyBindingPathIndirection> TargetIndirections;
			if (ensure(GetTargetPath().ResolveIndirectionsWithValue(InTargetDataView, TargetIndirections)))
			{
				const void* PropertyRef = TargetIndirections.Last().GetPropertyAddress();
				OutPinType = PropertyRefHelpers::GetPropertyRefInternalTypeAsPin(InProperty, PropertyRef);
			}
			OutIconName = TEXT("Kismet.Tabs.Variables");
			return true;
		}

		if (IsDelegateListenerProperty(&InProperty))
		{
			OutPinType.PinCategory = UEdGraphSchema_K2::PC_Delegate;
			OutIconName = TEXT("Icons.Event");
			return true;
		}

		return false;
	}

	virtual bool IsPropertyReference(const FProperty& InProperty) override
	{
		return PropertyRefHelpers::IsPropertyRef(InProperty);
	}

	virtual void AddPropertyInfoOverride(const FProperty& Property, TArray<TSharedPtr<const UE::PropertyBinding::FPropertyInfoOverride>>& OutPropertyInfoOverrides) const override
	{
		// Add the PropertyRef property type with its RefTypes
		const FStructProperty* StructProperty = CastField<const FStructProperty>(&Property);
		if (StructProperty && StructProperty->Struct && StructProperty->Struct->IsChildOf(FStateTreePropertyRef::StaticStruct()))
		{
			TArray<FEdGraphPinType, TInlineAllocator<1>> PinTypes;
			if (StructProperty->Struct->IsChildOf(FStateTreeBlueprintPropertyRef::StaticStruct()))
			{
				void* PropertyRefAddress = nullptr;
				if (GetPropertyHandle()->GetValueData(PropertyRefAddress) == FPropertyAccess::Result::Success)
				{
					check(PropertyRefAddress);
					PinTypes.Add(PropertyRefHelpers::GetBlueprintPropertyRefInternalTypeAsPin(*static_cast<const FStateTreeBlueprintPropertyRef*>(PropertyRefAddress)));
				}
			}
			else
			{
				PinTypes = PropertyRefHelpers::GetPropertyRefInternalTypesAsPins(Property);
			}

			// If Property supports Arrays, add the Array version of these pin types
			if (GetPropertyHandle()->HasMetaData(PropertyRefHelpers::CanRefToArrayName))
			{
				const int32 PinTypeNum = PinTypes.Num();
				for (int32 Index = 0; Index < PinTypeNum; ++Index)
				{
					const FEdGraphPinType& SourcePinType = PinTypes[Index];
					if (!SourcePinType.IsArray())
					{
						FEdGraphPinType& PinType = PinTypes.Emplace_GetRef(SourcePinType);
						PinType.ContainerType = EPinContainerType::Array;
					}
				}
			}

			for (const FEdGraphPinType& PinType : PinTypes)
			{
				TSharedRef<FRefTypeInfo> RefTypeInfo = MakeShared<FRefTypeInfo>();
				RefTypeInfo->PinType = PinType;

				FString TypeName;
				if (UObject* SubCategoryObject = PinType.PinSubCategoryObject.Get())
				{
					TypeName = SubCategoryObject->GetName();
				}
				else
				{
					TypeName = PinType.PinCategory.ToString() + TEXT(" ") + PinType.PinSubCategory.ToString();
				}

				RefTypeInfo->TypeNameText = FText::FromString(TypeName);
				OutPropertyInfoOverrides.Emplace(MoveTemp(RefTypeInfo));
			}
		}
	}

	virtual bool CanBindToContextStructInternal(const UStruct* InStruct, const int32 InStructIndex) override
	{
		// Do not allow to bind directly StateTree nodes
		// @todo: find a way to more specifically call out the context structs, e.g. pass the property path to the callback.
		if (InStruct != nullptr)
		{
			const bool bIsStateTreeNode = GetAccessibleStructs().ContainsByPredicate([InStruct](const TInstancedStruct<FPropertyBindingBindableStructDescriptor>& StructDesc)
			{
				const FStateTreeBindableStructDesc& AccessibleStruct = StructDesc.Get<FStateTreeBindableStructDesc>();
				return AccessibleStruct.DataSource != EStateTreeBindableStructSource::Context
					&& AccessibleStruct.DataSource != EStateTreeBindableStructSource::Parameter
					&& AccessibleStruct.DataSource != EStateTreeBindableStructSource::TransitionEvent
					&& AccessibleStruct.DataSource != EStateTreeBindableStructSource::StateEvent
					&& AccessibleStruct.DataSource != EStateTreeBindableStructSource::PropertyFunction
					&& AccessibleStruct.Struct == InStruct;
			});

			if (bIsStateTreeNode)
			{
				return false;
			}
		}

		const FStateTreeBindableStructDesc& StructDesc = GetBindableStructDescriptor(InStructIndex).Get<FStateTreeBindableStructDesc>();
		// Binding directly into PropertyFunction's struct is allowed if it contains a compatible single output property.
		if (StructDesc.DataSource == EStateTreeBindableStructSource::PropertyFunction)
		{
			const IStateTreeEditorPropertyBindingsOwner* BindingOwner = Cast<IStateTreeEditorPropertyBindingsOwner>(GetOwner());
			FStateTreeDataView DataView;
			// If DataView exists, struct is an instance of already bound function.
			if (BindingOwner == nullptr || BindingOwner->GetBindingDataViewByID(StructDesc.ID, DataView))
			{
				return false;
			}

			if (const FProperty* SingleOutputProperty = GetStructSingleOutputProperty(*StructDesc.Struct))
			{
				return CanBindToProperty(SingleOutputProperty, {FBindingChainElement(nullptr, InStructIndex), FBindingChainElement(const_cast<FProperty*>(SingleOutputProperty))});
			}
		}

		return Super::CanBindToContextStructInternal(InStruct, InStructIndex);
	}

	virtual bool CanAcceptPropertyOrChildrenInternal(const FProperty& SourceProperty, TConstArrayView<FBindingChainElement, int> InBindingChain) override
	{
		const int32 SourceStructIndex = InBindingChain[0].ArrayIndex;
		const FStateTreeBindableStructDesc& StructDesc = GetBindableStructDescriptor(SourceStructIndex).Get<FStateTreeBindableStructDesc>();

		if (StructDesc.DataSource == EStateTreeBindableStructSource::PropertyFunction)
		{
			IStateTreeEditorPropertyBindingsOwner* BindingOwner = Cast<IStateTreeEditorPropertyBindingsOwner>(GetOwner());
			FStateTreeDataView DataView;
			// If DataView exists, struct is an instance of already bound function.
			if (BindingOwner == nullptr || BindingOwner->GetBindingDataViewByID(StructDesc.ID, DataView))
			{
				return false;
			}

			// To avoid duplicates, PropertyFunction struct's children are not allowed to be bound if it contains a compatible single output property.
			if (const FProperty* SingleOutputProperty = GetStructSingleOutputProperty(*StructDesc.Struct))
			{
				if (CanBindToProperty(SingleOutputProperty, {FBindingChainElement(nullptr, SourceStructIndex), FBindingChainElement(const_cast<FProperty*>(SingleOutputProperty))}))
				{
					return false;
				}
			}

			// Binding to non-output PropertyFunctions properties is not allowed.
			if (InBindingChain.Num() == 1 && GetUsageFromMetaData(&SourceProperty) != EStateTreePropertyUsage::Output)
			{
				return false;
			}
		}

		if (PropertyRefHelpers::IsPropertyRef(*GetPropertyHandle()->GetProperty()) && !PropertyRefHelpers::IsPropertyAccessibleForPropertyRef(SourceProperty, InBindingChain, StructDesc))
		{
			if (!PropertyRefHelpers::IsPropertyAccessibleForPropertyRef(SourceProperty, InBindingChain, StructDesc))
			{
				return false;
			}
		}

		// Listener can only bind to dispatcher (prevents listener to listener)
		if (IsDelegateListenerProperty(GetPropertyHandle()->GetProperty()))
		{
			return IsDelegateDispatcherProperty(&SourceProperty);
		}

		return true;
	}

	virtual bool DeterminePropertiesCompatibilityInternal(
		const FProperty* InSourceProperty,
		const FProperty* InTargetProperty,
		const void* InSourcePropertyValue,
		const void* InTargetPropertyValue,
		bool& bOutAreCompatible) const override
	{
		// @TODO: Refactor FStateTreePropertyBindings::ResolveCopyType() so that we can use it directly here.
		
		const FStructProperty* TargetStructProperty = CastField<FStructProperty>(InTargetProperty);
		
		// AnyEnums need special handling.
		// It is a struct property but we want to treat it as an enum. We need to do this here, instead of 
		// GetPropertyCompatibility() because the treatment depends on the value too.
		// Note: AnyEnums will need special handling before they can be used for binding.
		if (TargetStructProperty && TargetStructProperty->Struct == FStateTreeAnyEnum::StaticStruct())
		{
			// If the AnyEnum has AllowAnyBinding, allow to bind to any enum.
			const bool bAllowAnyBinding = InTargetProperty->HasMetaData(AllowAnyBindingName);

			check(InTargetPropertyValue);
			const FStateTreeAnyEnum* TargetAnyEnum = static_cast<const FStateTreeAnyEnum*>(InTargetPropertyValue);

			// If the enum class is not specified, allow to bind to any enum, if the class is specified allow only that enum.
			if (const FByteProperty* SourceByteProperty = CastField<FByteProperty>(InSourceProperty))
			{
				if (UEnum* Enum = SourceByteProperty->GetIntPropertyEnum())
				{
					bOutAreCompatible = bAllowAnyBinding || TargetAnyEnum->Enum == Enum;
					return true;
				}
			}
			else if (const FEnumProperty* SourceEnumProperty = CastField<FEnumProperty>(InSourceProperty))
			{
				bOutAreCompatible = bAllowAnyBinding || TargetAnyEnum->Enum == SourceEnumProperty->GetEnum();
				return true;
			}
		}
		else if (TargetStructProperty && TargetStructProperty->Struct == FStateTreeStructRef::StaticStruct())
		{
			FString BaseStructName;
			const UScriptStruct* TargetStructRefBaseStruct = Compiler::GetBaseStructFromMetaData(InTargetProperty, BaseStructName);

			if (const FStructProperty* SourceStructProperty = CastField<FStructProperty>(InSourceProperty))
			{
				if (SourceStructProperty->Struct == TBaseStructure<FStateTreeStructRef>::Get())
				{
					FString SourceBaseStructName;
					const UScriptStruct* SourceStructRefBaseStruct = Compiler::GetBaseStructFromMetaData(SourceStructProperty, SourceBaseStructName);
					bOutAreCompatible = SourceStructRefBaseStruct && SourceStructRefBaseStruct->IsChildOf(TargetStructRefBaseStruct);
					return true;
				}
				else
				{
					bOutAreCompatible = SourceStructProperty->Struct && SourceStructProperty->Struct->IsChildOf(TargetStructRefBaseStruct);
					return true;
				}
			}
		}
		else if (TargetStructProperty && PropertyRefHelpers::IsPropertyRef(*TargetStructProperty))
		{
			check(InTargetPropertyValue);
			bOutAreCompatible = PropertyRefHelpers::IsPropertyRefCompatibleWithProperty(*TargetStructProperty, *InSourceProperty, InTargetPropertyValue, InSourcePropertyValue);
			return true;
		}
		else if (TargetStructProperty && IsDelegateListenerProperty(TargetStructProperty))
		{
			bOutAreCompatible = IsDelegateDispatcherProperty(InSourceProperty);
			return true;
		}

		return false;
	}

	virtual bool GetPropertyFunctionText(FConstStructView InPropertyFunctionStructView, FText& OutText) const override
	{
		const FStateTreeEditorNode& EditorNode = InPropertyFunctionStructView.Get<const FStateTreeEditorNode>();
		if (const FStateTreeNodeBase* Node = EditorNode.Node.GetPtr<const FStateTreeNodeBase>())
		{
			IStateTreeEditorPropertyBindingsOwner* BindingOwner = Cast<IStateTreeEditorPropertyBindingsOwner>(GetOwner());
			const FText Description = Node->GetDescription(GetSourcePath().GetStructID(), EditorNode.GetInstance(), FStateTreeBindingLookup(BindingOwner), EStateTreeNodeFormatting::Text);
			if (!Description.IsEmpty())
			{
				OutText = FText::FormatNamed(GetFormatableText(), TEXT("SourceStruct"), Description);
				return true;
			}
		}
		return false;
	}

	virtual bool GetPropertyFunctionTooltipText(FConstStructView InPropertyFunctionStructView, FText& OutText) const override
	{
		const FStateTreeEditorNode& EditorNode = InPropertyFunctionStructView.Get<const FStateTreeEditorNode>();
		if (const FStateTreeNodeBase* Node = EditorNode.Node.GetPtr<const FStateTreeNodeBase>())
		{
			IStateTreeEditorPropertyBindingsOwner* BindingOwner = Cast<IStateTreeEditorPropertyBindingsOwner>(GetOwner());
			const FText Description = Node->GetDescription(GetSourcePath().GetStructID(), EditorNode.GetInstance(), FStateTreeBindingLookup(BindingOwner), EStateTreeNodeFormatting::Text);
			if (!Description.IsEmpty())
			{
				OutText = FText::FormatNamed(GetFormatableTooltipText(), TEXT("SourceStruct"), Description);
				return true;
			}
		}
		return false;
	}

	virtual bool GetPropertyFunctionIconColor(FConstStructView InPropertyFunctionStructView, FLinearColor& OutColor) const override
	{
		const FStateTreeEditorNode& EditorNode = InPropertyFunctionStructView.Get<const FStateTreeEditorNode>();
		if (const FStateTreeNodeBase* Node = EditorNode.Node.GetPtr<const FStateTreeNodeBase>())
		{
			if (GetStructSingleOutputProperty(*Node->GetInstanceDataType()))
			{
				OutColor = Node->GetIconColor();
				return true;
			}
		}
		return false;
	}

	virtual bool GetPropertyFunctionImage(FConstStructView InPropertyFunctionStructView, const FSlateBrush*& OutImage) const override
	{
		const FStateTreeEditorNode& EditorNode = InPropertyFunctionStructView.Get<const FStateTreeEditorNode>();
		if (const FStateTreeNodeBase* Node = EditorNode.Node.GetPtr<const FStateTreeNodeBase>())
		{
			if (GetStructSingleOutputProperty(*Node->GetInstanceDataType()))
			{
				OutImage = UE::StateTreeEditor::EditorNodeUtils::ParseIcon(Node->GetIconName()).GetIcon();
				return true;
			}
		}
		return false;
	}
};

/* Provides PropertyFunctionNode instance for a property node. */
class FStateTreePropertyFunctionNodeProvider : public IStructureDataProvider
{
public:
	FStateTreePropertyFunctionNodeProvider(IStateTreeEditorPropertyBindingsOwner& InBindingsOwner, FPropertyBindingPath InTargetPath)
		: BindingsOwner(Cast<UObject>(&InBindingsOwner))
		, TargetPath(MoveTemp(InTargetPath))
	{}
	
	virtual bool IsValid() const override
	{
		return GetPropertyFunctionEditorNodeView(BindingsOwner.Get(), TargetPath).IsValid();
	};

	virtual const UStruct* GetBaseStructure() const override
	{
		return FStateTreeEditorNode::StaticStruct();
	}
	
	virtual void GetInstances(TArray<TSharedPtr<FStructOnScope>>& OutInstances, const UStruct* ExpectedBaseStructure) const override
	{
		if (ExpectedBaseStructure)
		{
			const FStructView Node = GetPropertyFunctionEditorNodeView(BindingsOwner.Get(), TargetPath);

			if (Node.IsValid() && Node.GetScriptStruct()->IsChildOf(ExpectedBaseStructure))
			{
				OutInstances.Add(MakeShared<FStructOnScope>(Node.GetScriptStruct(), Node.GetMemory()));
			}
		}
	}

	static bool IsBoundToValidPropertyFunction(UObject& InBindingsOwner, const FPropertyBindingPath& InTargetPath)
	{
		return GetPropertyFunctionEditorNodeView(&InBindingsOwner, InTargetPath).IsValid();
	}
	
private:
	static FStructView GetPropertyFunctionEditorNodeView(UObject* RawBindingsOwner, const FPropertyBindingPath& InTargetPath)
	{
		if(IStateTreeEditorPropertyBindingsOwner* Owner = Cast<IStateTreeEditorPropertyBindingsOwner>(RawBindingsOwner))
		{
			FStateTreeEditorPropertyBindings* EditorBindings = Owner->GetPropertyEditorBindings();
			FPropertyBindingBinding* FoundBinding = EditorBindings->GetMutableBindings().FindByPredicate([&InTargetPath](const FStateTreePropertyPathBinding& Binding)
			{
				return Binding.GetTargetPath() == InTargetPath;
			});

			
			if (FoundBinding)
			{
				const FStructView EditorNodeView = FoundBinding->GetMutablePropertyFunctionNode();
				if (EditorNodeView.IsValid())
				{
					const FStateTreeEditorNode& EditorNode = EditorNodeView.Get<FStateTreeEditorNode>();
					if (EditorNode.Node.IsValid() && EditorNode.Instance.IsValid())
					{
						return EditorNodeView;
					}
				}
			}
		}

		return FStructView();
	}

	TWeakObjectPtr<UObject> BindingsOwner;
	FPropertyBindingPath TargetPath;
};

} // UE::StateTree::PropertyBinding

TSharedPtr<UE::PropertyBinding::FCachedBindingData> FStateTreeBindingExtension::CreateCachedBindingData(IPropertyBindingBindingCollectionOwner* InBindingsOwner, const FPropertyBindingPath& InTargetPath, const TSharedPtr<IPropertyHandle>& InPropertyHandle, const TConstArrayView<TInstancedStruct<FPropertyBindingBindableStructDescriptor>> InAccessibleStructs) const
{
	return MakeShared<UE::StateTree::PropertyBinding::FStateTreeCachedBindingData>(InBindingsOwner, InTargetPath, InPropertyHandle, InAccessibleStructs);
}

bool FStateTreeBindingExtension::CanBindToProperty(const FPropertyBindingPath& InTargetPath, const IPropertyHandle& InPropertyHandle) const
{
	const EStateTreePropertyUsage Usage = UE::StateTree::GetUsageFromMetaData(InPropertyHandle.GetProperty());
	if (Usage == EStateTreePropertyUsage::Input || Usage == EStateTreePropertyUsage::Context)
	{
		// Allow to bind only to the main level on input and context properties.
		return InTargetPath.GetSegments().Num() == 1;
	}

	return (Usage == EStateTreePropertyUsage::Parameter);
}

void FStateTreeBindingExtension::UpdateContextStruct(TConstStructView<FPropertyBindingBindableStructDescriptor> InStructDesc, FBindingContextStruct& InOutContextStruct, TMap<FString, FText>& InOutSectionNames) const
{
	const FStateTreeBindableStructDesc& StructDesc = InStructDesc.Get<FStateTreeBindableStructDesc>();
	// Mare sure same section names get exact same FText representation (binding widget uses IsIdentical() to compare the section names).
	if (const FText* SectionText = InOutSectionNames.Find(StructDesc.StatePath))
	{
		InOutContextStruct.Section = *SectionText;
	}
	else
	{
		InOutContextStruct.Section = InOutSectionNames.Add(StructDesc.StatePath, FText::FromString(StructDesc.StatePath));
	}

	// PropertyFunction overrides it's struct's icon color.
	if (StructDesc.DataSource == EStateTreeBindableStructSource::PropertyFunction)
	{
		if (const FProperty* OutputProperty = UE::StateTree::GetStructSingleOutputProperty(*StructDesc.Struct))
		{
			const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
			check(Schema);

			FEdGraphPinType PinType;
			if (Schema->ConvertPropertyToPinType(OutputProperty, PinType))
			{
				InOutContextStruct.Color = Schema->GetPinTypeColor(PinType);
			}
		}
	}
}

bool FStateTreeBindingExtension::GetPromotionToParameterOverrideInternal(const FProperty& InProperty, bool& bOutOverride) const
{
	if (const FStructProperty* StructProperty = CastField<FStructProperty>(&InProperty))
	{
		// Support Property Refs as even though these aren't bp types, the actual types that would be added are the ones in the meta-data RefType
		if (StructProperty->Struct && StructProperty->Struct->IsChildOf(FStateTreePropertyRef::StaticStruct()))
		{
			bOutOverride = false;
			return true;
		}
	}
	return false;
}

void FStateTreeBindingsChildrenCustomization::CustomizeChildren(IDetailChildrenBuilder& ChildrenBuilder, TSharedPtr<IPropertyHandle> InPropertyHandle)
{
	TArray<UObject*> OuterObjects;
	InPropertyHandle->GetOuterObjects(OuterObjects);
	if (OuterObjects.Num() == 1)
	{
		FPropertyBindingPath TargetPath;
		UE::PropertyBinding::MakeStructPropertyPathFromPropertyHandle(InPropertyHandle, TargetPath);

		using FStateTreePropertyFunctionNodeProvider = UE::StateTree::PropertyBinding::FStateTreePropertyFunctionNodeProvider;
		UObject* BindingsOwner = UE::StateTree::PropertyBinding::FindEditorBindingsOwner(OuterObjects[0]);
		if (BindingsOwner && FStateTreePropertyFunctionNodeProvider::IsBoundToValidPropertyFunction(*BindingsOwner, TargetPath))
		{
			// Bound PropertyFunction takes control over property's children composition.
			const TSharedPtr<FStateTreePropertyFunctionNodeProvider> StructProvider = MakeShared<FStateTreePropertyFunctionNodeProvider>(*CastChecked<IStateTreeEditorPropertyBindingsOwner>(BindingsOwner), MoveTemp(TargetPath));
			// Create unique name to persists expansion state.
			const FName UniqueName = FName(LexToString(TargetPath.GetStructID()) + TargetPath.ToString());
			ChildrenBuilder.AddChildStructure(InPropertyHandle.ToSharedRef(), StructProvider, UniqueName);
		}
	}
}

bool FStateTreeBindingsChildrenCustomization::ShouldCustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle)
{
	TArray<UObject*> OuterObjects;
	InPropertyHandle->GetOuterObjects(OuterObjects);
	if (OuterObjects.Num() == 1)
	{
		// Bound property's children composition gets overridden.
		FPropertyBindingPath TargetPath;
		UE::PropertyBinding::MakeStructPropertyPathFromPropertyHandle(InPropertyHandle, TargetPath);
		IStateTreeEditorPropertyBindingsOwner* BindingOwner = Cast<IStateTreeEditorPropertyBindingsOwner>(UE::StateTree::PropertyBinding::FindEditorBindingsOwner(OuterObjects[0]));
		if (!TargetPath.IsPathEmpty() && BindingOwner)
		{
			if (const FStateTreeEditorPropertyBindings* EditorBindings = BindingOwner->GetPropertyEditorBindings())
			{
				return EditorBindings->HasBinding(TargetPath);
			}
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE

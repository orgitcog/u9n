// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InstancedStructDetails.h"
#include "EdGraphSchema_K2.h"
#include "StructUtils/PropertyBag.h"
#include "PropertyBagDetails.generated.h"

class FReply;
enum class ESelectorType : uint8;
struct FEdGraphSchemaAction;
struct FMenuEntryParams;

class IPropertyHandle;
class IPropertyUtilities;
class IDetailPropertyRow;
class SInlineEditableTextBlock;
class SWidget;

/**
 * The property bag details child rows can be completely customized by assigning a combination of these values
 * to their FPropertyBag 'ChildRowFeatures' metadata via the meta specifier. TODO: It isn't currently possible
 * to specify bitwise operations in the metadata string, but this will be added shortly. More configurations can be
 * added in the meantime to satisfy specific needs.
 */
UENUM(meta = (Bitflags))
enum class EPropertyBagChildRowFeatures : uint64
{
	/** General Options */
	Fixed                 = 0,         // Fixed layout. No features enabled.
	Renaming              = 1ULL << 0, // The property name is editable from the details view.
	Deletion              = 1ULL << 1, // The property is able to be deleted.
	DragAndDrop           = 1ULL << 2, // Drag and dropping properties is enabled.
	CompactTypeSelector   = 1ULL << 3, // A compact type selector widget is enabled to the left of the name.
	AccessSpecifierButton = 1ULL << 4, // The property metadata can be set for public/private access.
	DropDownMenuButton    = 1ULL << 5, // Drop-down menu (down arrow button) is enabled.
	Categories            = 1ULL << 6, // Categories are enabled (UPROPERTY categories).
	AllGeneralOptions     = Renaming | Deletion | DragAndDrop | CompactTypeSelector | AccessSpecifierButton | DropDownMenuButton | Categories,

	// Insert new drop general features above this line.

	/** Menu Options (for drop-down menu or other future menus (right click) */
	Menu_TypeSelector = 1ULL << 17, // Adds the type selector pill widget to the drop-down.
	Menu_Rename       = 1ULL << 18, // Renaming the property from the drop-down menu. Requires property renaming enabled.
	Menu_Delete       = 1ULL << 19, // Deleting the property from the drop-down menu.
	Menu_Categories   = 1ULL << 20, // Create new/remove from categories

	AllMenuOptions = Menu_TypeSelector | Menu_Rename | Menu_Delete | Menu_Categories,

	// Insert new menu features above this line.
	Deprecated = 1ULL << 63, // To allow for deprecating older features.

	// Below are configurations for convenience. These can be set via the Metadata specifier on the property bag.
	ReadOnly = Fixed,
	// Renaming and deleting enabled, with type selection happening in the drop-down menu.
	Core     = Renaming | Deletion | DropDownMenuButton | Menu_TypeSelector | Menu_Rename | Menu_Delete,
	// Also enables the compact type selector icon, drag and drop, and categories support.
	Extended = Core | DragAndDrop | CompactTypeSelector | Categories | Menu_Categories,
	// All options.
	All      = AllGeneralOptions | AllMenuOptions,
	// The default version includes deprecated UI features to support previous behavior.
	Default = Renaming | Deletion | Deprecated
};

ENUM_CLASS_FLAGS(EPropertyBagChildRowFeatures);

namespace UE::StructUtils
{
	STRUCTUTILSEDITOR_API void SetPropertyDescFromPin(FPropertyBagPropertyDesc& Desc, const FEdGraphPinType& PinType);
	STRUCTUTILSEDITOR_API FEdGraphPinType GetPropertyDescAsPin(const FPropertyBagPropertyDesc& Desc);
	/** Creates type selection pill widget. */
	STRUCTUTILSEDITOR_API TSharedRef<SWidget> CreateTypeSelectionWidget(TSharedPtr<IPropertyHandle> ChildPropertyHandle, const TSharedPtr<IPropertyHandle>& InBagStructProperty, const TSharedPtr<IPropertyUtilities>& InPropUtils, ESelectorType SelectorType, bool bAllowContainers = true);
}

/**
 * Type customization for FInstancedPropertyBag.
 */
class STRUCTUTILSEDITOR_API FPropertyBagDetails : public IPropertyTypeCustomization
{
public:
	// Required for deprecated variable 'bFixedLayout'. TODO: Remove when removing 5.6 deprecations.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FPropertyBagDetails() = default;
	~FPropertyBagDetails() = default;
	FPropertyBagDetails(const FPropertyBagDetails& Other) = delete;
	FPropertyBagDetails(FPropertyBagDetails&& Other) noexcept = delete;
	FPropertyBagDetails& operator=(const FPropertyBagDetails& Other) = delete;
	FPropertyBagDetails& operator=(FPropertyBagDetails&& Other) noexcept = delete;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** Creates add property widget. */
	static TSharedPtr<SWidget> MakeAddPropertyWidget(TSharedPtr<IPropertyHandle> InStructProperty, TSharedPtr<IPropertyUtilities> InPropUtils, EPropertyBagPropertyType DefaultType = EPropertyBagPropertyType::Bool, FSlateColor IconColor = FSlateColor::UseForeground());

protected:
	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:
	/** Handle to the struct property being edited */
	TSharedPtr<IPropertyHandle> StructProperty;
	TSharedPtr<IPropertyUtilities> PropUtils;
	EPropertyBagPropertyType DefaultType = EPropertyBagPropertyType::Bool;
	EPropertyBagChildRowFeatures ChildRowFeatures = EPropertyBagChildRowFeatures::Default;

	UE_DEPRECATED(5.6, "Use 'ChildRowFeatures' instead.")
	bool bFixedLayout = false;
	bool bAllowContainers = true;
};

/** 
 * Node builder for FInstancedPropertyBag children.
 *  - ValueProperty is FInstancedStruct of the FInstancedPropertyBag
 *  - StructProperty is FInstancedPropertyBag
 * Can be used in a implementation of a IPropertyTypeCustomization CustomizeChildren() to display editable FInstancedPropertyBag contents.
 * Use FPropertyBagDetails::MakeAddPropertyWidget() to create the add property widget.
 * OnChildRowAdded() is called right after each property is added, which allows the property row to be customizable.
 */
class STRUCTUTILSEDITOR_API FPropertyBagInstanceDataDetails : public FInstancedStructDataDetails
{
public:
	struct FConstructParams
	{
		TSharedPtr<IPropertyHandle> BagStructProperty = nullptr;
		TSharedPtr<IPropertyUtilities> PropUtils = nullptr;
		bool bAllowContainers = true;
		EPropertyBagChildRowFeatures ChildRowFeatures = EPropertyBagChildRowFeatures::Default;
	};

	explicit FPropertyBagInstanceDataDetails(const FConstructParams& ConstructParams);
	FPropertyBagInstanceDataDetails(TSharedPtr<IPropertyHandle> InStructProperty, const TSharedPtr<IPropertyUtilities>& InPropUtils, bool bInFixedLayout, bool bInAllowContainers = true);

	// Required for deprecated variable 'bFixedLayout' and 'bAllowArrays'. TODO: Remove when removing 5.6 deprecations.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	~FPropertyBagInstanceDataDetails() = default;
	FPropertyBagInstanceDataDetails(const FPropertyBagInstanceDataDetails& Other) = delete;
	FPropertyBagInstanceDataDetails(FPropertyBagInstanceDataDetails&& Other) noexcept = delete;
	FPropertyBagInstanceDataDetails& operator=(const FPropertyBagInstanceDataDetails& Other) = delete;
	FPropertyBagInstanceDataDetails& operator=(FPropertyBagInstanceDataDetails&& Other) noexcept = delete;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	virtual void OnGroupRowAdded(IDetailGroup& GroupRow, int32 Level, const FString& Category) const override;
	virtual void OnChildRowAdded(IDetailPropertyRow& ChildRow) override;
	
	/** Enum describing if a property is overridden, or undetermined (e.g. multiselection) */
	enum class EPropertyOverrideState
	{
		Yes,
		No,
		Undetermined,
	};

	/** Interface to allow to modify override status of a specific parameter. */
	struct IPropertyBagOverrideProvider
	{
		virtual ~IPropertyBagOverrideProvider()
		{
		}

		virtual bool IsPropertyOverridden(const FGuid PropertyID) const = 0;
		virtual void SetPropertyOverride(const FGuid PropertyID, const bool bIsOverridden) const = 0;
	};

	/**
	 * Callback function for EnumeratePropertyBags.
	 * @return true to continue enumeration
	 */
	using EnumeratePropertyBagFuncRef = TFunctionRef<bool(const FInstancedPropertyBag& /*DefaultPropertyBag*/, FInstancedPropertyBag& /*PropertyBag*/, IPropertyBagOverrideProvider& /*OverrideProvider*/)>;

	/**
	 * Method that is called to determine if a derived class has property override logic implemented.
	 * If true is returned, the overridden class is expected to implement PreChangeOverrides(), PostChangeOverrides(), EnumeratePropertyBags().
	 * @return true of derived class has override logic implemented.
	 */
	virtual bool HasPropertyOverrides() const
	{
		return false;
	}

	/** Called before property override is changed. */
	virtual void PreChangeOverrides()
	{
		// ParentPropertyHandle would be the property that holds the property overrides. 
		//		ParentPropertyHandle->NotifyPreChange();
		checkf(false, TEXT("PreChangeOverrides() is expecgted to be implemented when HasPropertyOverrides() returns true."));
	}

	/** Called after property override is changed. */
	virtual void PostChangeOverrides()
	{
		// ParentPropertyHandle is be the property that holds the property overrides. 
		//		ParentPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		//		ParentPropertyHandle->NotifyFinishedChangingProperties();
		checkf(false, TEXT("PostChangeOverrides() is expecgted to be implemented when HasPropertyOverrides() returns true."));
	}

	/**
	 * Called to enumerate each property bag on the property handle.
	 * The Func expects DefaultPropertyBag (the values that are override), and PropertyBag (the one that PropertyBagHandle points to),
	 * and instance of IPropertyBagOverrideProvider which is used to query if specific property is overridden, or to set the property override state.
	 */
	virtual void EnumeratePropertyBags(TSharedPtr<IPropertyHandle> PropertyBagHandle, const EnumeratePropertyBagFuncRef& Func) const
	{
		checkf(false, TEXT("EnumeratePropertyBags() is expected to be implemented when HasPropertyOverrides() returns true."));
	}

	/** @return true if property of specified child property is overridden. */
	virtual EPropertyOverrideState IsPropertyOverridden(TSharedPtr<IPropertyHandle> ChildPropertyHandle) const;

	/** Called to set the override state of specified child property. */
	virtual void SetPropertyOverride(TSharedPtr<IPropertyHandle> ChildPropertyHandle, const bool bIsOverridden);

	/** @return true if the child property has default value. */
	virtual bool IsDefaultValue(TSharedPtr<IPropertyHandle> ChildPropertyHandle) const;

	/** Called to reset the child property to default value. */
	virtual void ResetToDefault(TSharedPtr<IPropertyHandle> ChildPropertyHandle);

protected:
	UE_DEPRECATED(5.6, "Replaced by 'CreatePropertyDetailsWidget' to allow for more customizable drop-down menu location and content.")
	TSharedRef<SWidget> OnPropertyNameContent(TSharedPtr<IPropertyHandle> ChildPropertyHandle, TSharedPtr<SInlineEditableTextBlock> InlineWidget) const;

	TSharedPtr<IPropertyHandle> BagStructProperty;
	TSharedPtr<IPropertyUtilities> PropUtils;

	bool bAllowContainers = true;
	EPropertyBagChildRowFeatures ChildRowFeatures = EPropertyBagChildRowFeatures::Default;

	UE_DEPRECATED(5.6, "Use 'ChildRowFeatures' instead.")
	bool bFixedLayout = false;
	UE_DEPRECATED(5.6, "Use 'bAllowContainers' instead.")
	bool bAllowArrays = true;
};

/**
 * Specific property bag schema to allow customizing the requirements (e.g. supported containers).
 */
UCLASS()
class STRUCTUTILSEDITOR_API UPropertyBagSchema : public UEdGraphSchema_K2
{
	GENERATED_BODY()
public:
	virtual bool SupportsPinTypeContainer(TWeakPtr<const FEdGraphSchemaAction> SchemaAction, const FEdGraphPinType& PinType, const EPinContainerType& ContainerType) const override;
};

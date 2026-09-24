// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Customizations/MathStructCustomizations.h"

class IDetailLayoutBuilder;
class IDetailPropertyRow;
class IPropertyHandle;
class SWidget;

namespace UE::ControlRigEditor
{
	/** Property type customization for struct properties such as FAnimDetailsBool or FAnimDetailsTransform */
	class FAnimDetailsValueCustomization
		: public FMathStructCustomization
	{
	public:
		/** Creates an instance of this struct customization */
		static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	protected:
		//~ Begin IPropertyTypeCustomization interface
		virtual void MakeHeaderRow(TSharedRef<IPropertyHandle>& StructPropertyHandle, FDetailWidgetRow& Row) override;
		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& InStructBuilder, IPropertyTypeCustomizationUtils& InStructCustomizationUtils) override;
		virtual TSharedRef<SWidget> MakeChildWidget(TSharedRef<IPropertyHandle>& StructurePropertyHandle, TSharedRef<IPropertyHandle>& PropertyHandle) override;
		//~ End IPropertyTypeCustomization interface

	private:
		/** Makes a widget to display the property name */
		TSharedRef<SWidget> MakePropertyNameWidget(const TSharedRef<IPropertyHandle>& InStructPropertyHandle) const;

		/** Returns true if this struct is hidden by the filter */
		bool IsStructPropertyHiddenByFilter(const TSharedRef<IPropertyHandle>& InStructPropertyHandle) const;

		/** Returns true if this struct is hidden by the filter */
		bool IsChildPropertyHiddenByFilter(const TSharedRef<IPropertyHandle>& InPropertyHandle) const;

		/** Returns visible if the value struct is expanded, collapsed otherwise */
		EVisibility GetVisibilityFromExpansionState() const;

		/** Gets a color from the property */
		FLinearColor GetColorFromProperty(const FName& PropertyName) const;

		// use the property row to get expansion state to hide/show widgets, and use the builder/structhandler to get it
		mutable IDetailPropertyRow* DetailPropertyRow = nullptr;

		/** Pointer to the detail builder, or nullptr if not initialized */
		IDetailLayoutBuilder* DetailBuilder = nullptr;

		/** The customized struct */
		TSharedPtr<IPropertyHandle> StructPropertyHandle;

		/** The numeric entry box widget */
		TSharedPtr<SWidget> NumericEntryBox;
	};
}

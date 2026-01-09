// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "PropertyHandle.h"
#include "DetailWidgetRow.h"

class UMLDeformerTrainingDataProcessorSettings;

namespace UE::MLDeformer::TrainingDataProcessor
{
	/**
	 * The property detail customization for a single animation input that can be enabled or disabled.
	 * This is a customization for the type FMLDeformerTrainingDataProcessorAnim.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API FAnimCustomization final : public IPropertyTypeCustomization
	{
	public:
		static TSharedRef<IPropertyTypeCustomization> MakeInstance();

		//~ Begin IPropertyTypeCustomization overrides.
		virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
		EVisibility GetAnimErrorVisibility(const TSharedRef<IPropertyHandle> StructPropertyHandle, int32 AnimIndex) const;
		// ~End IPropertyTypeCustomization overrides.

	private:
		void RefreshProperties() const;
		static UMLDeformerTrainingDataProcessorSettings* FindSettings(const TSharedRef<IPropertyHandle>& StructPropertyHandle);

	private:
		TSharedPtr<IPropertyUtilities> PropertyUtilities;
	};
}	// namespace UE::MLDeformer::TrainingDataProcessor

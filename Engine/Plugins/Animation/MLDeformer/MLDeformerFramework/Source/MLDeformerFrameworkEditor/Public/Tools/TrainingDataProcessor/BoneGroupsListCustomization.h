// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "PropertyHandle.h"
#include "MLDeformerTrainingDataProcessorSettings.h"

class UMLDeformerTrainingDataProcessorSettings;
class USkeleton;

namespace UE::MLDeformer::TrainingDataProcessor
{
	/**
	 * The property detail customization for a list of bone groups.
	 * This is a customization for the type FMLDeformerTrainingDataProcessorBoneGroupsList.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API FBoneGroupsListCustomization : public IPropertyTypeCustomization
	{
	public:
		static TSharedRef<IPropertyTypeCustomization> MakeInstance();

		//~ Begin IPropertyTypeCustomization overrides.
		virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow,
		                             IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder,
		                               IPropertyTypeCustomizationUtils& StructCustomizationUtils) override {}
		//~ End IPropertyTypeCustomization overrides.

	private:
		TArray<FMLDeformerTrainingDataProcessorBoneGroup>* GetBoneGroups() const;

	private:
		TSharedPtr<IPropertyUtilities> PropertyUtilities;
		TSharedPtr<IPropertyHandle> StructProperty;
	};
} // namespace UE::MLDeformer::TrainingDataProcessor

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "PropertyHandle.h"

class UMLDeformerTrainingDataProcessorSettings;
class USkeleton;

namespace UE::MLDeformer::TrainingDataProcessor
{
	/**
	 * The property detail customization for a list of bones.
	 * This is a detail customization for the type FMLDeformerTrainingDataProcessorBoneList.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API FBoneListCustomization : public IPropertyTypeCustomization
	{
	public:
		static TSharedRef<IPropertyTypeCustomization> MakeInstance();

		//~ Begin IPropertyTypeCustomization overrides.
		virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow,
		                             IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder,
		                               IPropertyTypeCustomizationUtils& StructCustomizationUtils) override { }
		// ~End IPropertyTypeCustomization overrides.

	private:
		TArray<FName>* GetBoneNames() const;

	private:
		TSharedPtr<IPropertyUtilities> PropertyUtilities;
		TSharedPtr<IPropertyHandle> StructProperty;
	};
} // namespace UE::MLDeformer::TrainingDataProcessor

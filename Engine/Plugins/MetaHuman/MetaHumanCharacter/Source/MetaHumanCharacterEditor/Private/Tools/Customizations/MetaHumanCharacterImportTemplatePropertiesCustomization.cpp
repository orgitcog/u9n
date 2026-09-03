// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/Customizations/MetaHumanCharacterImportTemplatePropertiesCustomization.h"
#include "Tools/MetaHumanCharacterEditorConformTool.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "PropertyHandle.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/StaticMesh.h" 
#include "Engine/SkeletalMesh.h" 

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditor"

TSharedRef<IDetailCustomization> FMetaHumanCharacterImportTemplatePropertiesCustomization::MakeInstance()
{
	return MakeShareable(new FMetaHumanCharacterImportTemplatePropertiesCustomization);
}

void FMetaHumanCharacterImportTemplatePropertiesCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	InDetailBuilder.GetObjectsBeingCustomized(Objects);

	UObject* SelectedObject = Objects.Num() > 0 ? Objects[0].Get() : nullptr;
	if (!SelectedObject)
	{
		return;
	}

	FProperty* MeshProperty = SelectedObject->GetClass()->FindPropertyByName("Mesh");
	if (!MeshProperty)
	{
		return;
	}

	TSharedPtr<IPropertyHandle> MeshHandle = InDetailBuilder.GetProperty("Mesh");

	auto IsSkelMeshLambda = [MeshProperty, MeshHandle]()
		{
			if (CastField<FSoftObjectProperty>(MeshProperty)) // check it is a soft object property first
			{
				// get the type of mesh from the property handle. Because this is a soft object ptr, we need to use the asset registry as
				// it won't necessarily be loaded / resolved
				void* AssetPtr;
				if (MeshHandle->GetValueData(AssetPtr) == FPropertyAccess::Success)
				{
					TSoftObjectPtr<UObject>* MeshSoftObjectPtr = static_cast<TSoftObjectPtr<UObject>*>(AssetPtr); // we need an explicit cast from the value data 

					if (MeshSoftObjectPtr)
					{
						// Get the asset path and look for it in the asset registry
						FSoftObjectPath AssetPath = MeshSoftObjectPtr->ToString();
						FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
						FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(AssetPath);

						if (AssetData.IsValid() && AssetData.GetClass()->IsChildOf(USkeletalMesh::StaticClass()))
						{
							return true;
						}
					}

				}

			}

			return false;
		};

	auto SkelMeshVisibilityLambda = [IsSkelMeshLambda, MeshProperty, MeshHandle]()
		{
			if (IsSkelMeshLambda())
			{
				return EVisibility::Visible;
			}

			return EVisibility::Hidden;
		};

	auto IsStaticMeshLambda = [MeshProperty, MeshHandle]()
		{
			if (CastField<FSoftObjectProperty>(MeshProperty)) // check it is a soft object property first
			{
				// get the type of mesh from the property handle. Because this is a soft object ptr, we need to use the asset registry as
				// it won't necessarily be loaded / resolved
				void* AssetPtr;
				if (MeshHandle->GetValueData(AssetPtr) == FPropertyAccess::Success)
				{
					TSoftObjectPtr<UObject>* MeshSoftObjectPtr = static_cast<TSoftObjectPtr<UObject>*>(AssetPtr); // we need an explicit cast from the value data 

					if (MeshSoftObjectPtr)
					{
						// Get the asset path and look for it in the asset registry
						FSoftObjectPath AssetPath = MeshSoftObjectPtr->ToString();
						FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
						FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(AssetPath);

						if (AssetData.IsValid() && AssetData.GetClass()->IsChildOf(UStaticMesh::StaticClass()))
						{
							return true;
						}
					}

				}

			}

			return false;
		};

	auto StaticMeshVisibilityLambda = [IsStaticMeshLambda, MeshProperty, MeshHandle]()
		{
			if (IsStaticMeshLambda())
			{
				return EVisibility::Visible;
			}

			return EVisibility::Hidden;
		};

	TSharedPtr<IPropertyHandle> ImportOptionsHandle = InDetailBuilder.GetProperty("ImportOptions");
	if (!ImportOptionsHandle || !ImportOptionsHandle->IsValidHandle())
	{
		return;
	}

	TSharedPtr<IPropertyHandle> UseEyeMeshesHandle = ImportOptionsHandle->GetChildHandle("bUseEyeMeshes");
	if (!UseEyeMeshesHandle || !UseEyeMeshesHandle->IsValidHandle())
	{
		return;
	}

	TSharedPtr<IPropertyHandle> UseTeethMeshHandle = ImportOptionsHandle->GetChildHandle("bUseTeethMesh");
	if (!UseTeethMeshHandle|| !UseTeethMeshHandle->IsValidHandle())
	{
		return;
	}

	IDetailCategoryBuilder& ImportOptionsCategory = InDetailBuilder.EditCategory("Import Template Options");
	ImportOptionsCategory.AddProperty(UseEyeMeshesHandle).Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(SkelMeshVisibilityLambda)));
	ImportOptionsCategory.AddProperty(UseTeethMeshHandle).Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(SkelMeshVisibilityLambda)));

	TSharedPtr<IPropertyHandle> LeftEyeMeshHandle = InDetailBuilder.GetProperty("LeftEyeMesh");
	if (!LeftEyeMeshHandle || !LeftEyeMeshHandle->IsValidHandle())
	{
		return;
	}

	TSharedPtr<IPropertyHandle> RightEyeMeshHandle = InDetailBuilder.GetProperty("RightEyeMesh");
	if (!RightEyeMeshHandle || !RightEyeMeshHandle->IsValidHandle())
	{
		return;
	}

	TSharedPtr<IPropertyHandle> TeethMeshHandle = InDetailBuilder.GetProperty("TeethMesh");
	if (!TeethMeshHandle || !TeethMeshHandle->IsValidHandle())
	{
		return;
	}

	IDetailCategoryBuilder& AssetCategory = InDetailBuilder.EditCategory("Asset");
	AssetCategory.AddProperty(MeshHandle);
	AssetCategory.AddProperty(LeftEyeMeshHandle).Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(StaticMeshVisibilityLambda)));
	AssetCategory.AddProperty(RightEyeMeshHandle).Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(StaticMeshVisibilityLambda)));
	AssetCategory.AddProperty(TeethMeshHandle).Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(StaticMeshVisibilityLambda)));

}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanIdentityEditorModule.h"

#include "MetaHumanIdentity.h"
#include "MetaHumanIdentityParts.h"
#include "MetaHumanIdentityPose.h"
#include "MetaHumanIdentityPromotedFrames.h"
#include "MetaHumanTemplateMeshComponent.h"
#include "MetaHumanPredictiveSolversTask.h"

#include "Customizations/MetaHumanIdentityPoseCustomizations.h"
#include "ThumbnailRendering/MetaHumanIdentityThumbnailRenderer.h"

#include "ThumbnailRendering/ThumbnailManager.h"
#include "PropertyEditorModule.h"
#include "Engine/SkeletalMesh.h"
#include "SkinnedAssetCompiler.h"

void FMetaHumanIdentityEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	ClassesToUnregisterOnShutdown = { UMetaHumanIdentity::StaticClass()->GetFName(), UMetaHumanIdentityPose::StaticClass()->GetFName(), UMetaHumanIdentityBody::StaticClass()->GetFName(), 
		UMetaHumanIdentityFace::StaticClass()->GetFName(), UMetaHumanTemplateMesh::StaticClass()->GetFName(), UMetaHumanTemplateMeshComponent::StaticClass()->GetFName() };
	PropertyToUnregisterOnShutdown = UMetaHumanIdentityPromotedFrame::StaticClass()->GetFName();

	PropertyEditorModule.RegisterCustomClassLayout(ClassesToUnregisterOnShutdown[0], FOnGetDetailCustomizationInstance::CreateStatic(&FMetaHumanIdentityCustomization::MakeInstance));
	PropertyEditorModule.RegisterCustomClassLayout(ClassesToUnregisterOnShutdown[1], FOnGetDetailCustomizationInstance::CreateStatic(&FMetaHumanIdentityPoseCustomization::MakeInstance));
	PropertyEditorModule.RegisterCustomClassLayout(ClassesToUnregisterOnShutdown[2], FOnGetDetailCustomizationInstance::CreateStatic(&FMetaHumanIdentityBodyCustomization::MakeInstance));
	PropertyEditorModule.RegisterCustomClassLayout(ClassesToUnregisterOnShutdown[3], FOnGetDetailCustomizationInstance::CreateStatic(&FMetaHumanIdentityFaceCustomization::MakeInstance));
	PropertyEditorModule.RegisterCustomPropertyTypeLayout(PropertyToUnregisterOnShutdown, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMetaHumanIdentityPromotedFramePropertyCustomization::MakeInstance));

#ifdef MASK_SELECTION_UI
	PropertyEditorModule.RegisterCustomClassLayout(ClassesToUnregisterOnShutdown[4], FOnGetDetailCustomizationInstance::CreateStatic(&FMetaHumanTemplateMeshCustomization::MakeInstance));
#endif

	PropertyEditorModule.RegisterCustomClassLayout(ClassesToUnregisterOnShutdown[5], FOnGetDetailCustomizationInstance::CreateStatic(&FMetaHumanTemplateMeshComponentCustomization::MakeInstance));

	// Register the thumbnail renderer
	UThumbnailManager::Get().RegisterCustomRenderer(UMetaHumanIdentity::StaticClass(), UMetaHumanIdentityThumbnailRenderer::StaticClass());
}

void FMetaHumanIdentityEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyEditorModule.UnregisterCustomClassLayout(ClassesToUnregisterOnShutdown[0]);
		PropertyEditorModule.UnregisterCustomClassLayout(ClassesToUnregisterOnShutdown[1]);
		PropertyEditorModule.UnregisterCustomClassLayout(ClassesToUnregisterOnShutdown[2]);
		PropertyEditorModule.UnregisterCustomClassLayout(ClassesToUnregisterOnShutdown[3]);
#ifdef MASK_SELECTION_UI
		PropertyEditorModule.UnregisterCustomClassLayout(ClassesToUnregisterOnShutdown[4]);
#endif
		PropertyEditorModule.UnregisterCustomClassLayout(ClassesToUnregisterOnShutdown[5]);
		PropertyEditorModule.UnregisterCustomPropertyTypeLayout(PropertyToUnregisterOnShutdown);
	}

	FPredictiveSolversTaskManager::Get().StopAll();
}

IMPLEMENT_MODULE(FMetaHumanIdentityEditorModule, MetaHumanIdentityEditor)
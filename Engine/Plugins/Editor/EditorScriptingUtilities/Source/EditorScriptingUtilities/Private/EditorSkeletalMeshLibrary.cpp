// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorSkeletalMeshLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "LODUtilities.h"
#include "SkeletalMeshEditorSubsystem.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EditorSkeletalMeshLibrary)

bool UDEPRECATED_EditorSkeletalMeshLibrary::RegenerateLOD(USkeletalMesh* SkeletalMesh, int32 NewLODCount /*= 0*/, bool bRegenerateEvenIfImported /*= false*/, bool bGenerateBaseLOD /*= false*/)
{
	USkeletalMeshEditorSubsystem* SkeletalMeshEditorSubsystem = GEditor->GetEditorSubsystem<USkeletalMeshEditorSubsystem>();

	return FLODUtilities::RegenerateLOD(SkeletalMesh, GetTargetPlatformManagerRef().GetRunningTargetPlatform(), NewLODCount, bRegenerateEvenIfImported, bGenerateBaseLOD);
}

int32 UDEPRECATED_EditorSkeletalMeshLibrary::GetNumVerts(USkeletalMesh* SkeletalMesh, int32 LODIndex)
{
	USkeletalMeshEditorSubsystem* SkeletalMeshEditorSubsystem = GEditor->GetEditorSubsystem<USkeletalMeshEditorSubsystem>();

	return SkeletalMeshEditorSubsystem ? SkeletalMeshEditorSubsystem->GetNumVerts(SkeletalMesh, LODIndex) : 0;
}

bool UDEPRECATED_EditorSkeletalMeshLibrary::RenameSocket(USkeletalMesh* SkeletalMesh, FName OldName, FName NewName)
{
	USkeletalMeshEditorSubsystem* SkeletalMeshEditorSubsystem = GEditor->GetEditorSubsystem<USkeletalMeshEditorSubsystem>();

	return SkeletalMeshEditorSubsystem ? SkeletalMeshEditorSubsystem->RenameSocket(SkeletalMesh, OldName, NewName) : 0;
}
int32 UDEPRECATED_EditorSkeletalMeshLibrary::GetLODCount(USkeletalMesh* SkeletalMesh)
{
	USkeletalMeshEditorSubsystem* SkeletalMeshEditorSubsystem = GEditor->GetEditorSubsystem<USkeletalMeshEditorSubsystem>();

	return SkeletalMeshEditorSubsystem ? SkeletalMeshEditorSubsystem->GetLODCount(SkeletalMesh) : INDEX_NONE;
}

int32 UDEPRECATED_EditorSkeletalMeshLibrary::ImportLOD(USkeletalMesh* BaseMesh, const int32 LODIndex, const FString& SourceFilename)
{
	USkeletalMeshEditorSubsystem* SkeletalMeshEditorSubsystem = GEditor->GetEditorSubsystem<USkeletalMeshEditorSubsystem>();

	return SkeletalMeshEditorSubsystem ? SkeletalMeshEditorSubsystem->ImportLOD(BaseMesh, LODIndex, SourceFilename) : INDEX_NONE;
}

bool UDEPRECATED_EditorSkeletalMeshLibrary::ReimportAllCustomLODs(USkeletalMesh* SkeletalMesh)
{
	USkeletalMeshEditorSubsystem* SkeletalMeshEditorSubsystem = GEditor->GetEditorSubsystem<USkeletalMeshEditorSubsystem>();

	return SkeletalMeshEditorSubsystem ? SkeletalMeshEditorSubsystem->ReimportAllCustomLODs(SkeletalMesh) : false;
}

void UDEPRECATED_EditorSkeletalMeshLibrary::GetLodBuildSettings(const USkeletalMesh* SkeletalMesh, const int32 LodIndex, FSkeletalMeshBuildSettings& OutBuildOptions)
{
	if (USkeletalMeshEditorSubsystem* SkeletalMeshEditorSubsystem = GEditor->GetEditorSubsystem<USkeletalMeshEditorSubsystem>())
	{
		SkeletalMeshEditorSubsystem->GetLodBuildSettings(SkeletalMesh, LodIndex, OutBuildOptions);
	}
}

void UDEPRECATED_EditorSkeletalMeshLibrary::SetLodBuildSettings(USkeletalMesh* SkeletalMesh, const int32 LodIndex, const FSkeletalMeshBuildSettings& BuildOptions)
{
	if (USkeletalMeshEditorSubsystem* SkeletalMeshEditorSubsystem = GEditor->GetEditorSubsystem<USkeletalMeshEditorSubsystem>())
	{
		SkeletalMeshEditorSubsystem->SetLodBuildSettings(SkeletalMesh, LodIndex, BuildOptions);
	}
}

bool UDEPRECATED_EditorSkeletalMeshLibrary::RemoveLODs(USkeletalMesh* SkeletalMesh, TArray<int32> ToRemoveLODs)
{
	USkeletalMeshEditorSubsystem* SkeletalMeshEditorSubsystem = GEditor->GetEditorSubsystem<USkeletalMeshEditorSubsystem>();

	return SkeletalMeshEditorSubsystem ? SkeletalMeshEditorSubsystem->RemoveLODs(SkeletalMesh, ToRemoveLODs) : false;
}

bool UDEPRECATED_EditorSkeletalMeshLibrary::StripLODGeometry(USkeletalMesh* SkeletalMesh, const int32 LODIndex, UTexture2D* TextureMask, const float Threshold)
{
	USkeletalMeshEditorSubsystem* SkeletalMeshEditorSubsystem = GEditor->GetEditorSubsystem<USkeletalMeshEditorSubsystem>();

	return SkeletalMeshEditorSubsystem ? SkeletalMeshEditorSubsystem->StripLODGeometry(SkeletalMesh, LODIndex, TextureMask, Threshold) : false;
}

UPhysicsAsset* UDEPRECATED_EditorSkeletalMeshLibrary::CreatePhysicsAsset(USkeletalMesh* SkeletalMesh)
{
	USkeletalMeshEditorSubsystem* SkeletalMeshEditorSubsystem = GEditor->GetEditorSubsystem<USkeletalMeshEditorSubsystem>();

	return SkeletalMeshEditorSubsystem ? SkeletalMeshEditorSubsystem->CreatePhysicsAsset(SkeletalMesh) : nullptr;
}


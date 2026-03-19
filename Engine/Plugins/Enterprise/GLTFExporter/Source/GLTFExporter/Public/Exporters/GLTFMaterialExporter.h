// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Exporters/GLTFExporter.h"
#include "GLTFMaterialExporter.generated.h"

#define UE_API GLTFEXPORTER_API

class UStaticMesh;

UCLASS(MinimalAPI)
class UGLTFMaterialExporter : public UGLTFExporter
{
public:

	GENERATED_BODY()

	UE_API explicit UGLTFMaterialExporter(const FObjectInitializer& ObjectInitializer = FObjectInitializer());

	UE_API virtual bool AddObject(FGLTFContainerBuilder& Builder, const UObject* Object) override;
};

#undef UE_API

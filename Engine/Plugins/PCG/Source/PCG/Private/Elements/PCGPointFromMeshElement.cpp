// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGPointFromMeshElement.h"

#include "PCGComponent.h"
#include "PCGEdge.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"

#if WITH_EDITOR
#include "Helpers/PCGDynamicTrackingHelpers.h"
#endif // WITH_EDITOR

#include "Engine/StaticMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGPointFromMeshElement)

#define LOCTEXT_NAMESPACE "PCGPointFromMeshElement"

#if WITH_EDITOR
FText UPCGPointFromMeshSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Point From Mesh");
}

FText UPCGPointFromMeshSettings::GetNodeTooltipText() const
{
	return LOCTEXT("PointFromMeshNodeTooltip", "Creates a single point at the origin with an attribute named MeshPathAttributeName containing a SoftObjectPath to the StaticMesh.");
}

void UPCGPointFromMeshSettings::GetStaticTrackedKeys(FPCGSelectionKeyToSettingsMap& OutKeysToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const
{
	if (StaticMesh.IsNull() || IsPropertyOverriddenByPin(GET_MEMBER_NAME_CHECKED(UPCGPointFromMeshSettings, StaticMesh)))
	{
		return;
	}

	FPCGSelectionKey Key = FPCGSelectionKey::CreateFromPath(StaticMesh.ToSoftObjectPath());

	OutKeysToSettings.FindOrAdd(MoveTemp(Key)).Emplace(this, /*bCulling=*/false);
}
#endif

FPCGElementPtr UPCGPointFromMeshSettings::CreateElement() const
{
	return MakeShared<FPCGPointFromMeshElement>();
}

bool FPCGPointFromMeshElement::PrepareDataInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGPointFromMeshElement::PrepareData);

	check(Context);

	const UPCGPointFromMeshSettings* Settings = Context->GetInputSettings<UPCGPointFromMeshSettings>();
	check(Settings);

	if (Settings->StaticMesh.IsNull())
	{
		return true;
	}

	FPCGPointFromMeshContext* ThisContext = static_cast<FPCGPointFromMeshContext*>(Context);

	if (!ThisContext->WasLoadRequested())
	{
		return ThisContext->RequestResourceLoad(ThisContext, { Settings->StaticMesh.ToSoftObjectPath() }, !Settings->bSynchronousLoad);
	}

	return true;
}

bool FPCGPointFromMeshElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGPointFromMeshElement::Execute);

	check(Context);

	const UPCGPointFromMeshSettings* Settings = Context->GetInputSettings<UPCGPointFromMeshSettings>();
	check(Settings);

	if (Settings->StaticMesh.IsNull())
	{
		return true;
	}

	if (!Settings->StaticMesh.Get())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("LoadStaticMeshFailed", "Failed to load StaticMesh"));
		return true;
	}

#if WITH_EDITOR
	if (Context->IsValueOverriden(GET_MEMBER_NAME_CHECKED(UPCGPointFromMeshSettings, StaticMesh)))
	{
		FPCGDynamicTrackingHelper::AddSingleDynamicTrackingKey(Context, FPCGSelectionKey::CreateFromPath(Settings->StaticMesh.ToSoftObjectPath()), /*bIsCulled=*/false);
	}
#endif // WITH_EDITOR

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	UPCGBasePointData* OutPointData = FPCGContext::NewPointData_AnyThread(Context);
	Outputs.Emplace_GetRef().Data = OutPointData;
	
	OutPointData->SetNumPoints(1);

	// Capture StaticMesh bounds
	const FBox StaticMeshBounds = Settings->StaticMesh->GetBoundingBox();

	OutPointData->SetBoundsMin(StaticMeshBounds.Min);
	OutPointData->SetBoundsMax(StaticMeshBounds.Max);

	// Write StaticMesh path to MeshPathAttribute
	check(OutPointData->Metadata);
	OutPointData->Metadata->CreateSoftObjectPathAttribute(Settings->MeshPathAttributeName, Settings->StaticMesh.ToSoftObjectPath(), /*bAllowsInterpolation=*/false);

	return true;
}

#undef LOCTEXT_NAMESPACE

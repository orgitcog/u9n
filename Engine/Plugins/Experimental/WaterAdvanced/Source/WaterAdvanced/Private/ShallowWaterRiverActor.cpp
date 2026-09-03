// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShallowWaterRiverActor.h"

#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"

#include "WaterBodyActor.h"
#include "WaterBodyRiverComponent.h"
#include "WaterSplineComponent.h"
#include "WaterSubsystem.h"

#include "BakedShallowWaterSimulationComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureRenderTarget2DArray.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SplineMeshComponent.h"
#include "PhysicsEngine/ConvexElem.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/OverlapResult.h"

#include "TextureResource.h"
#include "ShallowWaterCommon.h"
#include "FFTOceanPatchSubsystem.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Math/Float16Color.h"
#include "Kismet/GameplayStatics.h"
#include "Landscape.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "LevelInstance/LevelInstanceSubsystem.h"

#include "EngineUtils.h"

bool bShallowWaterRiverDebugVisualize = false;
FAutoConsoleVariableRef CVarShallowWaterRiverDebugVisualize(TEXT("r.ShallowWater.RiverDebugVisualize"), bShallowWaterRiverDebugVisualize, TEXT(""));

UShallowWaterRiverComponent::UShallowWaterRiverComponent(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	PrimaryComponentTick.bCanEverTick = true;	

#if WITH_EDITORONLY_DATA
	bTickInEditor = true;
#endif // WITH_EDITORONLY_DATA

	bIsInitialized = false;
	bTickInitialize = false;
	bRenderStateTickInitialize = false;

	ResolutionMaxAxis = 512;
	SourceSize = 1000;
	
	// initialize landscape array with all landscapes
	if (GetWorld())
	{
		for (TActorIterator<ALandscape> It(GetWorld(), ALandscape::StaticClass()); It; ++It)
		{
			BottomContourLandscapeActors.Add(*It);
		}
	}
}

TObjectPtr<UTextureRenderTarget2D> UShallowWaterRiverComponent::GetSharedFFTOceanPatchNormalRTFromSubsystem(UWorld* World)
{
	if (World != nullptr)
	{
		UFFTOceanPatchSubsystem *OceanPatchSubsystem = World->GetSubsystem<UFFTOceanPatchSubsystem>();

		if (OceanPatchSubsystem != nullptr)
		{
			return OceanPatchSubsystem->GetOceanNormalRT(World);
		}
		else
		{
			UE_LOG(LogShallowWater, Warning, TEXT("No valid FFT ocean patch subsystem."));	
		}
	}
	else
	{
		UE_LOG(LogShallowWater, Warning, TEXT("No valid World."));
	}

	return nullptr;
}

FBoxSphereBounds UShallowWaterRiverComponent::InitializeCaptureDI(const FName& DIName, TArray<AActor*> RawActorPtrArray)
{
	UNiagaraFunctionLibrary::SetSceneCapture2DDataInterfaceManagedMode(RiverSimSystem, DIName,
				ESceneCaptureSource::SCS_SceneDepth,
				FIntPoint(ResolutionMaxAxis, ResolutionMaxAxis),
				ETextureRenderTargetFormat::RTF_R32f,
				ECameraProjectionMode::Orthographic,
				90.0f,
				FMath::Max(WorldGridSize.X, WorldGridSize.Y),
				true,
				false,
				RawActorPtrArray);

	// accumulate bounding box for river water bodies
	FBoxSphereBounds::Builder BottomContourCombinedWorldBoundsBuilder;
	for (AActor *BottomContourActor : RawActorPtrArray)
	{
		if (BottomContourActor != nullptr)
		{
			// accumulate bounds
			FBoxSphereBounds WorldBounds;
			BottomContourActor->GetActorBounds(false, WorldBounds.Origin, WorldBounds.BoxExtent);

			BottomContourCombinedWorldBoundsBuilder += WorldBounds;
		}
		else
		{
			UE_LOG(LogShallowWater, Verbose, TEXT("UShallowWaterRiverComponent::Rebuild() - skipping null bottom contour boundary actor found"));
			continue;
		}
	}
	return FBoxSphereBounds(BottomContourCombinedWorldBoundsBuilder);	
}

void UShallowWaterRiverComponent::PostLoad()
{
	Super::PostLoad();	

	if (RenderState == EShallowWaterRenderState::LiveSim || RiverSimSystem == nullptr)
	{
	#if WITH_EDITOR
		bIsInitialized = false;
		bTickInitialize = false;

		Rebuild();
	#endif
	}
	else
	{
		RiverSimSystem->ReinitializeSystem();
		RiverSimSystem->Activate();
	}

	bRenderStateTickInitialize = false;
}

void UShallowWaterRiverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
#if WITH_EDITOR
	// lots of tick ordering issues, so we try to initialize on the first tick too
	if (!bTickInitialize && (RiverSimSystem == nullptr || (RenderState == EShallowWaterRenderState::LiveSim && !bIsInitialized)))
	{
		bTickInitialize = true;
		Rebuild();
	}
	else if (bIsInitialized)
	{
		if (RiverSimSystem)
		{
			RiverSimSystem->Activate();	
		}
		else
		{
			UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::TickComponent() - null Niagara sim when trying to activate. Please reset."));
		}
	}
	else
	{
		// System is in a bad state
		// UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::TickComponent() - null Niagara sim when trying to activate. Please reset."));
	}
#endif

	if (!bRenderStateTickInitialize)
	{
		UpdateRenderState();
	}
}

void UShallowWaterRiverComponent::BeginPlay()
{
	Super::BeginPlay();

	bRenderStateTickInitialize = false;

	UpdateRenderState();

	// make sure the simulation is not going to be run in case of various initialization edge cases
	bool bReadBakedSim = RenderState == EShallowWaterRenderState::BakedSim || RenderState == EShallowWaterRenderState::WaterComponentWithBakedSim || RenderState == EShallowWaterRenderState::WaterComponent;	
	if (RiverSimSystem != nullptr && bReadBakedSim)
	{
		RiverSimSystem->SetVariableBool(FName("ReadCachedSim"), bReadBakedSim);
		RiverSimSystem->ReinitializeSystem();
		RiverSimSystem->Activate();
	}
}

void UShallowWaterRiverComponent::OnUnregister()
{
	Super::OnUnregister();
}

#if WITH_EDITOR

void UShallowWaterRiverComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName;
	if (PropertyChangedEvent.Property)
	{
		PropertyName = PropertyChangedEvent.Property->GetFName();
	}
			
	// this should go before rebuild not after...something is wrong
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UShallowWaterRiverComponent, RenderState) && RiverSimSystem != nullptr && RiverSimSystem->IsActive())
	{
		bool bReadBakedSim = RenderState == EShallowWaterRenderState::BakedSim || RenderState == EShallowWaterRenderState::WaterComponentWithBakedSim || RenderState == EShallowWaterRenderState::WaterComponent;	
		RiverSimSystem->SetVariableBool(FName("ReadCachedSim"), bReadBakedSim);
	}
	else
	{
		bIsInitialized = false;
		bTickInitialize = false;		
	}

	bRenderStateTickInitialize = false;

	Rebuild();
	UpdateRenderState();
	ReregisterComponent();

}

void UShallowWaterRiverComponent::Rebuild()
{	
	bIsInitialized = false;
	bTickInitialize = false;

	if (NiagaraRiverSimulation == nullptr)
	{
		NiagaraRiverSimulation = LoadObject<UNiagaraSystem>(nullptr, TEXT("/WaterAdvanced/Niagara/Systems/Grid2D_SW_River.Grid2D_SW_River"));
	}

	if (RiverSimSystem != nullptr)
	{
		RiverSimSystem->SetActive(false);
		RiverSimSystem->DestroyComponent();
		RiverSimSystem = nullptr;
	}
	
	if (ResolutionMaxAxis <= 0)
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Rebuild() - resolution must be greater than 0"));
		return;
	}

	if (NumSteps <= 0)
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Rebuild() - num steps must be greater than 0"));
		return;
	}

	if (SimSpeed <= 1e-8)
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Rebuild() - speed must be greater than zero"));
		return;
	}

	if (NiagaraRiverSimulation == nullptr)
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Rebuild() - null Niagara system asset"));
		return;
	}

	AllWaterBodies.Empty();

	// collect all the water bodies	
	if (SourceRiverWaterBodies.Num() != 0)
	{
		for (TSoftObjectPtr<AWaterBody > CurrWaterBody : SourceRiverWaterBodies)
		{
			if (CurrWaterBody)
			{
				AllWaterBodies.Add(CurrWaterBody);
			}
			else 
			{
				UE_LOG(LogShallowWater, Verbose, TEXT("UShallowWaterRiverComponent::Rebuild() - skipping null water body actor found"));
				continue;
			}
		}
	}
	else	
	{
		UE_LOG(LogShallowWater, Verbose, TEXT("UShallowWaterRiverComponent::Rebuild() - No source water bodies specified"));
		return;
	}
	
	if (AllWaterBodies.Num() == 0)
	{
		UE_LOG(LogShallowWater, Verbose, TEXT("UShallowWaterRiverComponent::Rebuild() - No valid source water bodies specified"));
		return;
	}

	bool HasValidSinks = false;
	for (TSoftObjectPtr<AWaterBody> CurrWaterBody : SinkRiverWaterBodies)
	{
		if (CurrWaterBody != nullptr)
		{
			HasValidSinks = true;
			AllWaterBodies.Add(CurrWaterBody);			
		}
		else
		{
			UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Rebuild() - skipping null sink water body actor found"));
			continue;
		}
	}

	// flush all debug draw lines
#if ENABLE_DRAW_DEBUG
	FlushPersistentDebugLines(GetWorld());
#endif

	if (!HasValidSinks)
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Rebuild() - no valid sinks, using the first source as a sink"));
		SinkRiverWaterBodies.Add(*AllWaterBodies.CreateConstIterator());
	}

	// accumulate bounding box for river water bodies
	FBoxSphereBounds::Builder CombinedWorldBoundsBuilder;
	for (TSoftObjectPtr<AWaterBody > CurrWaterBody : AllWaterBodies)
	{
		TObjectPtr<UWaterBodyComponent> CurrWaterBodyComponent = CurrWaterBody->GetWaterBodyComponent();

		if (CurrWaterBodyComponent != nullptr)
		{
			// accumulate bounds
			FBoxSphereBounds WorldBounds;
			CurrWaterBody->GetActorBounds(true, WorldBounds.Origin, WorldBounds.BoxExtent);				

			CombinedWorldBoundsBuilder += WorldBounds;
		}
	}
	FBoxSphereBounds CombinedBounds(CombinedWorldBoundsBuilder);

	if (CombinedBounds.BoxExtent.Length() < SMALL_NUMBER)
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Rebuild() - river bodies have zero bounds"));
		return;
	}
	
	SystemPos = CombinedBounds.Origin - FVector(0, 0, CombinedBounds.BoxExtent.Z);
	
	RiverSimSystem = NewObject<UNiagaraComponent>(this, NAME_None, RF_Public);
	RiverSimSystem->bUseAttachParentBound = false;
	RiverSimSystem->SetWorldLocation(SystemPos);

	bool bReadBakedSim = RenderState == EShallowWaterRenderState::BakedSim || RenderState == EShallowWaterRenderState::WaterComponentWithBakedSim || RenderState == EShallowWaterRenderState::WaterComponent;

	if (GetWorld() && GetWorld()->bIsWorldInitialized)
	{
		if (!RiverSimSystem->IsRegistered())
		{
			RiverSimSystem->RegisterComponentWithWorld(GetWorld());
		}

		RiverSimSystem->SetVisibleFlag(true);
		RiverSimSystem->SetAsset(NiagaraRiverSimulation);
							
		// convert to raw ptr array for function library
		if (!bReadBakedSim && bUseCapture)
		{
			// landscape captures
			TArray<AActor*> LandscapeBottomContourActorsRawPtr;
			LandscapeBottomContourActorsRawPtr.Add(nullptr);
			for (TSoftObjectPtr<ALandscape> CurrLandscapeActor : BottomContourLandscapeActors)
			{
				LandscapeBottomContourActorsRawPtr.Add(CurrLandscapeActor.Get());
			}
			FBoxSphereBounds LandscapeBottomContourBounds = InitializeCaptureDI("User.LandscapeBottomCapture", LandscapeBottomContourActorsRawPtr);
		
			// undilated captures
			TArray<AActor*> BottomContourActorsRawPtr;
			BottomContourActorsRawPtr.Add(nullptr);
			AddActorsToRawArray(BottomContourActors, BottomContourActorsRawPtr);			
			AddTaggedActorsToArray(BottomContourTags, BottomContourActorsRawPtr);
			FBoxSphereBounds CombinedBottomContourBounds = InitializeCaptureDI("User.BottomCapture", BottomContourActorsRawPtr);
			FBoxSphereBounds CombinedBottomContourBoundsUnder = InitializeCaptureDI("User.BottomCaptureUnder", BottomContourActorsRawPtr);

			// Dilated capture
			TArray<AActor*> DilatedBottomContourActorsRawPtr;
			DilatedBottomContourActorsRawPtr.Add(nullptr);
			AddActorsToRawArray(DilatedBottomContourActors, DilatedBottomContourActorsRawPtr);
			AddTaggedActorsToArray(DilatedBottomContourTags, DilatedBottomContourActorsRawPtr);

			FBoxSphereBounds DilatedCombinedBottomContourBounds = InitializeCaptureDI("User.DilatedBottomCapture", DilatedBottomContourActorsRawPtr);
			FBoxSphereBounds DilatedCombinedBottomContourBoundsUnder = InitializeCaptureDI("User.DilatedBottomCaptureUnder", DilatedBottomContourActorsRawPtr);

			// reinitialize and set variables on the system
			RiverSimSystem->ReinitializeSystem();

			RiverSimSystem->SetVariableFloat(FName("LandscapeCaptureOffset"), LandscapeBottomContourBounds.Origin.Z + LandscapeBottomContourBounds.BoxExtent.Z + BottomContourCaptureOffset);			

			RiverSimSystem->SetVariableFloat(FName("CaptureOffset"), CombinedBottomContourBounds.Origin.Z + CombinedBottomContourBounds.BoxExtent.Z + BottomContourCaptureOffset);
			RiverSimSystem->SetVariableFloat(FName("DilatedCaptureOffset"), DilatedCombinedBottomContourBounds.Origin.Z + DilatedCombinedBottomContourBounds.BoxExtent.Z + BottomContourCaptureOffset);
			
			RiverSimSystem->SetVariableFloat(FName("CaptureOffsetUnder"), CombinedBottomContourBounds.Origin.Z - CombinedBottomContourBounds.BoxExtent.Z - BottomContourCaptureOffset);
			RiverSimSystem->SetVariableFloat(FName("DilatedCaptureOffsetUnder"), DilatedCombinedBottomContourBounds.Origin.Z - DilatedCombinedBottomContourBounds.BoxExtent.Z - BottomContourCaptureOffset);
		}
		else
		{
			RiverSimSystem->ReinitializeSystem();
		}
	}
	else
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Rebuild() - World not initialized"));
		return;
	}
	

	if (RiverSimSystem == nullptr)
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Rebuild() - Cannot spawn river system"));
		return;
	}

	// look for the water info texture
	for (TSoftObjectPtr<AWaterBody > CurrWaterBody : AllWaterBodies)
	{
		if (AWaterZone* WaterZone = CurrWaterBody->GetWaterBodyComponent()->GetWaterZone())
		{			
			const TObjectPtr<UTextureRenderTarget2DArray> NewWaterInfoTexture = WaterZone->WaterInfoTextureArray;
			if (NewWaterInfoTexture == nullptr)
			{
				WaterZone->GetOnWaterInfoTextureArrayCreated().RemoveDynamic(this, &UShallowWaterRiverComponent::OnWaterInfoTextureArrayCreated);
				WaterZone->GetOnWaterInfoTextureArrayCreated().AddDynamic(this, &UShallowWaterRiverComponent::OnWaterInfoTextureArrayCreated);
			}
			else
			{
				OnWaterInfoTextureArrayCreated(NewWaterInfoTexture);
			}			

			const int32 PlayerIndex = 0;
			FVector ZoneLocation;
			WaterZone->GetDynamicWaterInfoCenter(PlayerIndex, ZoneLocation);
			const FVector2D ZoneExtent = FVector2D(WaterZone->GetDynamicWaterInfoExtent());
			const FVector2D WaterHeightExtents = FVector2D(WaterZone->GetWaterHeightExtents());
			const float GroundZMin = WaterZone->GetGroundZMin();

			RiverSimSystem->SetVariableVec2(FName("WaterZoneLocation"), FVector2D(ZoneLocation));
			RiverSimSystem->SetVariableVec2(FName("WaterZoneExtent"), ZoneExtent);
			RiverSimSystem->SetVariableInt(FName("WaterZoneIdx"), WaterZone->GetWaterZoneIndex());
			
			break;
		}
	}

	RiverSimSystem->Activate();
	
	WorldGridSize = 2.0f * FVector2D(CombinedBounds.BoxExtent.X, CombinedBounds.BoxExtent.Y);

	if (WorldGridSize.Length() < 1e-8)
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Bake() - Simulation grid has (0,0) size."));
		return;
	}

	RiverSimSystem->SetVariableVec2(FName("WorldGridSize"), WorldGridSize);
	RiverSimSystem->SetVariableInt(FName("ResolutionMaxAxis"), ResolutionMaxAxis);

	// pad out source's box height a so it intersects the sim plane.  This value doesn't matter much so we hardcode it
	float Overshoot = 1000.f;
	float FinalSourceHeight = 2. * CombinedBounds.BoxExtent.Z + Overshoot;

	// Get sources
	int32 i = 0;
	for (TSoftObjectPtr<AWaterBody> CurrWaterBody : SourceRiverWaterBodies)
	{
		FVector CurrSourcePos;
		float CurrSourceWidth;
		float CurrSourceDepth;
		FVector CurrSourceDir;
		if (!QueryWaterAtSplinePoint(CurrWaterBody, 0, CurrSourcePos, CurrSourceDir, CurrSourceWidth, CurrSourceDepth))
		{
			UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Rebuild() - water source query failed"));
			continue;
		}		
		
		FVector FullSourcePos = CurrSourcePos - FVector(0, 0, .5 * FinalSourceHeight) + FVector(CurrSourceDir.X, CurrSourceDir.Y, 0) * .5 * SourceSize;
		FVector FullSourceSize = FVector(CurrSourceWidth, SourceSize, FinalSourceHeight); 
		
		CurrSourceDir = FVector(CurrSourceDir.X, CurrSourceDir.Y, 0);
		CurrSourceDir.Normalize();
		
		FVector BaseVector = {0,1,0};	
		double FullSourceAngle =  FMath::Acos(FVector::DotProduct(BaseVector, CurrSourceDir));
		
		FVector AxisToUse = FVector::CrossProduct(BaseVector, CurrSourceDir);
		AxisToUse.Normalize();

#if ENABLE_DRAW_DEBUG
		if (bShallowWaterRiverDebugVisualize)
		{
			FQuat TmpQ = FQuat::MakeFromRotationVector(AxisToUse * FullSourceAngle);		
			DrawDebugBox(GetWorld() , (FVector) FullSourcePos, .5 * FullSourceSize, TmpQ, FColor::Green, true);		
		}
#endif

		// flip axis so we don't need to store the vector itself
		if (AxisToUse.Z < 0)
		{
			FullSourceAngle *= -1;
		}

		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayPositionValue(RiverSimSystem, "User.SourcePosArray", i, FullSourcePos, true);
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVectorValue(RiverSimSystem, "User.SourceSizeArray", i, FullSourceSize, true);
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloatValue(RiverSimSystem, "User.SourceAngleArray", i, FullSourceAngle, true);
		i++;
	}

	// get sinks	
	FVector SinkPos(0, 0, 0);
	float SinkWidth = 1;
	float SinkDepth = 1;
	FVector SinkDir(1, 0, 0);
	
	for (TSoftObjectPtr<AWaterBody> CurrWaterBody : SinkRiverWaterBodies)
	{
		if (!QueryWaterAtSplinePoint(CurrWaterBody, -1, SinkPos, SinkDir, SinkWidth, SinkDepth))
		{
			UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Rebuild() - water sink query failed"));
			continue;
		}

		// height of the sink box doesn't matter
		float SinkBoxHeight = 100000;
		FVector FullSinkSize = FVector(SinkWidth, SourceSize, SinkBoxHeight);
		
		SinkDir = FVector(SinkDir.X, SinkDir.Y, 0);
		SinkDir.Normalize();
		
		FVector BaseVector = {0,1,0};	
		double FullSinkAngle =  FMath::Acos(FVector::DotProduct(BaseVector, SinkDir));
		

		FVector AxisToUse = FVector::CrossProduct(BaseVector, SinkDir);
		AxisToUse.Normalize();

#if ENABLE_DRAW_DEBUG
		if (bShallowWaterRiverDebugVisualize)
		{
			FQuat TmpQ = FQuat::MakeFromRotationVector(AxisToUse * FullSinkAngle);		
			DrawDebugBox(GetWorld() , (FVector) SinkPos, .5 * FullSinkSize, TmpQ, FColor::Red, true);		
		}
#endif

		// flip axis so we don't need to store the vector itself
		if (AxisToUse.Z < 0)
		{
			FullSinkAngle *= -1;
		}

		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayPositionValue(RiverSimSystem, "User.SinkPosArray", i, SinkPos, true);

		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVectorValue(RiverSimSystem, "User.SinkSizeArray", i, FullSinkSize, true);
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloatValue(RiverSimSystem, "User.SinkAngleArray", i, FullSinkAngle, true);		

		i++;
	}

	RiverSimSystem->SetVariableFloat(FName("SimSpeed"), SimSpeed);
	RiverSimSystem->SetVariableInt(FName("NumSteps"), NumSteps);

	RiverSimSystem->SetVariableBool(FName("MatchSpline"), bMatchSpline);
	RiverSimSystem->SetVariableFloat(FName("RemoveOutsideSplineAmount"), RemoveOutsideSplineAmount);
	RiverSimSystem->SetVariableFloat(FName("SplineHeightMatchingAmount"), MatchSplineHeightAmount);
	
	BakedWaterSurfaceRT = NewObject<UTextureRenderTarget2D>(this, NAME_None, RF_Transient);
	BakedWaterSurfaceRT->InitAutoFormat(1, 1);
	RiverSimSystem->SetVariableTextureRenderTarget(FName("SimGridRT"), BakedWaterSurfaceRT);
	
	BakedFoamRT = NewObject<UTextureRenderTarget2D>(this, NAME_None, RF_Transient);
	BakedFoamRT->InitAutoFormat(1, 1);
	RiverSimSystem->SetVariableTextureRenderTarget(FName("FoamRT"), BakedFoamRT);
	
	BakedWaterSurfaceNormalRT = NewObject<UTextureRenderTarget2D>(this, NAME_None, RF_Transient);
	BakedWaterSurfaceNormalRT->InitAutoFormat(1, 1);
	RiverSimSystem->SetVariableTextureRenderTarget(FName("NormalRT"), BakedWaterSurfaceNormalRT);

	RiverSimSystem->SetVariableBool(FName("ReadCachedSim"), bReadBakedSim);

	RiverSimSystem->SetVariableFloat(FName("BottomContourCollisionDilation"), BottomContourCollisionDilation);
	
	RiverSimSystem->SetVariableInt(FName("ExtrapolationHalfWidth"), SmoothingWidth);
	RiverSimSystem->SetVariableFloat(FName("SmoothingHeightCutoff"), SmoothingCutoff);

	if (BakedWaterSurfaceTexture != nullptr && BakedFoamTexture != nullptr && BakedWaterSurfaceNormalTexture != nullptr)
	{
		RiverSimSystem->SetVariableTexture(FName("BakedSimTexture"), BakedWaterSurfaceTexture);
		RiverSimSystem->SetVariableTexture(FName("BakedFoamTexture"), BakedFoamTexture);
		RiverSimSystem->SetVariableTexture(FName("BakedWaterSurfaceNormalTexture"), BakedWaterSurfaceNormalTexture);
	}

	TObjectPtr<UTextureRenderTarget2D>  OceanPatchNormalRT = GetSharedFFTOceanPatchNormalRTFromSubsystem(GetWorld());

	if (OceanPatchNormalRT == nullptr)
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Rebuild() - ocean patch normal RT is not initialized"));
		return;
	}

	NormalDetailRT = OceanPatchNormalRT;
	RiverSimSystem->SetVariableTextureRenderTarget(FName("NormalDetailRT"), NormalDetailRT);

	bIsInitialized = true;
}

void UShallowWaterRiverComponent::AddActorsToRawArray(const TArray<TSoftObjectPtr<AActor>> &ActorsArray, TArray<AActor*>& BottomContourActorsRawPtr)
{
	for (TSoftObjectPtr<AActor> CurrActor : ActorsArray)
	{
		AActor* CurrActorRawPtr = CurrActor.Get();

		// if we have a level instance, break it up and add each actor
		if (ALevelInstance* LevelInstancePtr = Cast<ALevelInstance>(CurrActorRawPtr))
		{
			const ULevelInstanceSubsystem* LevelInstanceSubsystem = GetWorld()->GetSubsystem<ULevelInstanceSubsystem>();

			LevelInstanceSubsystem->ForEachActorInLevelInstance(LevelInstancePtr, [&](AActor* SubActor)
				{
					BottomContourActorsRawPtr.Add(SubActor);
					return true;
				});
		}
		else
		{
			BottomContourActorsRawPtr.Add(CurrActorRawPtr);
		}
	}
}

void UShallowWaterRiverComponent::AddTaggedActorsToArray(TArray<FName> &TagsToUse, TArray<AActor*>& BottomContourActorsRawPtr)
{
	// if we have a tag set
	// do an overlap test
	//  filter by tag and add to the bottomcontour actors list
	//  if a level instance is tagged, loop over the contained actors

	if (!TagsToUse.IsEmpty())
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ShallowWaterRiverActorQuery), false);

		TArray<FOverlapResult> Overlaps;
		GetWorld()->OverlapMultiByChannel(Overlaps, SystemPos, FQuat::Identity, ECollisionChannel::ECC_WorldStatic,
			FCollisionShape::MakeBox(0.5f * FVector(WorldGridSize.X, WorldGridSize.Y, 100000)), Params);

		for (const FOverlapResult& OverlapResult : Overlaps)
		{
			if (UPrimitiveComponent* PrimitiveComponent = OverlapResult.GetComponent())
			{
				if (AActor* ComponentActor = PrimitiveComponent->GetOwner())
				{
					if (TagsToUse.ContainsByPredicate([&](const FName& Tag) { return Tag == NAME_None || ComponentActor->Tags.Contains(Tag); }))
					{
						// if we have a level instance, break it up and add each actor
						if (ALevelInstance* LevelInstancePtr = Cast<ALevelInstance>(ComponentActor))
						{
							const ULevelInstanceSubsystem* LevelInstanceSubsystem = GetWorld()->GetSubsystem<ULevelInstanceSubsystem>();

							LevelInstanceSubsystem->ForEachActorInLevelInstance(LevelInstancePtr, [&](AActor* SubActor)
								{
									BottomContourActorsRawPtr.Add(SubActor);
									return true;
								});
						}
						else
						{
							BottomContourActorsRawPtr.Add(ComponentActor);
						}
					}
				}
			}
		}
	}
}

void UShallowWaterRiverComponent::Bake()
{
	EObjectFlags TextureObjectFlags = EObjectFlags::RF_Public;

	if (!RiverSimSystem || !BakedWaterSurfaceRT || !BakedFoamRT || !BakedWaterSurfaceNormalRT)
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Bake() - No simulation to bake"));
		return;
	}

	if (SourceRiverWaterBodies.Num() != 0)
	{
		for (TSoftObjectPtr<AWaterBody > CurrWaterBody : SourceRiverWaterBodies)
		{
			if (CurrWaterBody == nullptr)
			{			
				UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Rebuild() - Cannot bake with a null water body.  Please make sure all water bodies are loaded and that all entires on the actor are valid"));
				return;
			}
		}
	}

	BakedWaterSurfaceTexture = BakedWaterSurfaceRT->ConstructTexture2D(this, "BakedRiverTexture", TextureObjectFlags);

	RiverSimSystem->SetVariableTexture(FName("BakedSimTexture"), BakedWaterSurfaceTexture);

	// Readback to get the river texture values as an array
	TArray<FFloat16Color> TmpShallowWaterSimArrayValues;
	BakedWaterSurfaceRT->GameThread_GetRenderTargetResource()->ReadFloat16Pixels(TmpShallowWaterSimArrayValues);

	TArray<FVector4> ShallowWaterSimArrayValues;
	ShallowWaterSimArrayValues.Empty();
	ShallowWaterSimArrayValues.AddZeroed(TmpShallowWaterSimArrayValues.Num());

	// cast all values to floats
	int Index = 0;
	for (FFloat16Color Val : TmpShallowWaterSimArrayValues)
	{
		const float WaterHeight = Val.R;
		const float WaterDepth = Val.G;
		const FVector2D WaterVelocity(Val.B, Val.A);

		FVector4 FloatVal;
		FloatVal.X = WaterHeight;
		FloatVal.Y = WaterDepth;
		FloatVal.Z = WaterVelocity.X;
		FloatVal.W = WaterVelocity.Y;

		ShallowWaterSimArrayValues[Index++] = FloatVal;
	}

	// bake foam and other data to texture
	BakedFoamTexture = BakedFoamRT->ConstructTexture2D(this, "BakedFoamTexture", TextureObjectFlags);
	RiverSimSystem->SetVariableTexture(FName("BakedFoamTexture"), BakedFoamTexture);
	
	// bake normal to texture
	BakedWaterSurfaceNormalTexture = BakedWaterSurfaceNormalRT->ConstructTexture2D(this, "BakedWaterSurfaceNormalTexture", TextureObjectFlags);
	RiverSimSystem->SetVariableTexture(FName("BakedWaterSurfaceNormalTexture"), BakedWaterSurfaceNormalTexture);

	// clear references to old baked sim on water body actors
	if (BakedSim != nullptr)
	{ 
		for (TSoftObjectPtr<AWaterBody > CurrWaterBody : BakedSim->WaterBodies)
		{
			TObjectPtr<UWaterBodyComponent> CurrWaterBodyComponent = CurrWaterBody->GetWaterBodyComponent();

			if (CurrWaterBodyComponent != nullptr)
			{
				CurrWaterBodyComponent->SetBakedShallowWaterSimulation(nullptr);
				CurrWaterBodyComponent->PostEditChange();
			}
		}
	}

	BakedSim = NewObject<UBakedShallowWaterSimulationComponent>(this, NAME_None, RF_Public);
	BakedSim->SimulationData = FShallowWaterSimulationGrid(ShallowWaterSimArrayValues, BakedWaterSurfaceTexture, FIntVector2(BakedWaterSurfaceRT->SizeX, BakedWaterSurfaceRT->SizeY), SystemPos, WorldGridSize);
	BakedSim->WaterBodies = AllWaterBodies;	
		
	// compute the maximum water height for each convex in each water body simulated by this river
	// we use this to modify the collision geometry so it fully encompasses the baked water sim
	TMap<FKConvexElem*, float> ConvexToMaxHeight;
	for (int32 y = 0; y < BakedWaterSurfaceRT->SizeY; ++y) {
	for (int32 x = 0; x < BakedWaterSurfaceRT->SizeX; ++x) {
		FVector WorldPos = BakedSim->SimulationData.IndexToWorld(FIntVector2(x, y));

		FVector Vel;
		float Height, Depth;
		BakedSim->SimulationData.QueryShallowWaterSimulationAtIndex(FIntVector2(x, y), Vel, Height, Depth);
		WorldPos.Z = Height;
				
		if (Depth > 1e-5)
		{
			for (TSoftObjectPtr<AWaterBody > CurrWaterBody : AllWaterBodies)
			{						
				TObjectPtr<UWaterBodyComponent> CurrWaterBodyComponent = CurrWaterBody->GetWaterBodyComponent();

				TArray<UPrimitiveComponent*> CollisionComponents = CurrWaterBodyComponent->GetCollisionComponents();
				for (UPrimitiveComponent* CurrCollisionComponent : CollisionComponents)
				{
					USplineMeshComponent* CurrSplineComponent = StaticCast<USplineMeshComponent*>(CurrCollisionComponent);
					TObjectPtr<UBodySetup> CurrBodySetup = CurrSplineComponent->BodySetup;

					const FTransform CurrMeshTransform = CurrCollisionComponent->GetComponentTransform();

					// make sure the collision convex hull vertices are clamped to the min/max water height
					for (FKConvexElem& ConvexElem : CurrBodySetup->AggGeom.ConvexElems)
					{					
						const TArray<FVector>& VertexData = ConvexElem.VertexData;		

						// see if the current point is inside the convex projected to the xy plane
						const FBox CurrBox = ConvexElem.CalcAABB(CurrMeshTransform, FVector(1, 1, 1));										

						if (CurrBox.IsInsideXY(FBox(WorldPos, WorldPos)))
						{						
							float* TmpMaxHeight = ConvexToMaxHeight.Find(&ConvexElem);
							if (TmpMaxHeight == nullptr)
							{							
								ConvexToMaxHeight.Emplace(&ConvexElem, WorldPos.Z);
							}
							else
							{
								*TmpMaxHeight = FMath::Max(*TmpMaxHeight, WorldPos.Z);
							}
						}
					}
				}
			}
		}
	}}

	// set the sim texture on each water body that is in the simulated river.  
	for (TSoftObjectPtr<AWaterBody > CurrWaterBody : AllWaterBodies)
	{
		TObjectPtr<UWaterBodyComponent> CurrWaterBodyComponent = CurrWaterBody->GetWaterBodyComponent();
				
		CurrWaterBodyComponent->SetBakedShallowWaterSimulation(BakedSim);

		// grow bounds in z to include the tallest height
		TArray<UPrimitiveComponent*> CollisionComponents = CurrWaterBodyComponent->GetCollisionComponents();

		// Make sure that the collision objects includes the maximum height of the baked water sim otherwise
		// we will miss collisions.
		for (UPrimitiveComponent* CurrCollisionComponent : CollisionComponents)
		{
			USplineMeshComponent* CurrSplineComponent = StaticCast<USplineMeshComponent*>(CurrCollisionComponent);
			
			TObjectPtr<UBodySetup> CurrBodySetup = CurrSplineComponent->BodySetup;

			FTransform CurrMeshTransform = CurrCollisionComponent->GetComponentTransform();

			// make sure the collision convex hull vertices are clamped to the min/max water height
			for (FKConvexElem& ConvexElem : CurrBodySetup->AggGeom.ConvexElems)
			{			
				// see if the current point is inside the convex projected to the xy plane
				FBox CurrBox = ConvexElem.CalcAABB(CurrMeshTransform, FVector(1, 1, 1));				

				TArray<FVector>& VertexData = ConvexElem.VertexData;
				
				// 
				// We know based on the way each convex elem is created which indices correspond with
				// top and bottom of the box.  From SplineMeshComponent.cpp:1075
				// ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector(-1, -1, -1)));
				// ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector(-1, -1, 1)));
				// ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector(-1, 1, -1)));
				// ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector(-1, 1, 1)));
				// ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector(1, -1, -1)));
				// ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector(1, -1, 1)));
				// ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector(1, 1, -1)));
				// ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector(1, 1, 1)));
				
				if (const float* WorldMaxZForConvex = ConvexToMaxHeight.Find(&ConvexElem))
				{
					// for each vertex in the convex hull, set the Z to the maximum baked water sim Z height for the convex
					int32 Idx = 0;
					for (FVector& Vertex : VertexData)
					{
						FVector VWorld = CurrMeshTransform.TransformPosition(Vertex);					
					
						// Only set on top vertices of the convex hull with +Z to push it up
						if (Idx == 1 || Idx == 3 || Idx == 5 || Idx == 7)
						{
							VWorld.Z = *WorldMaxZForConvex;
						}

						const FVector VLocal = CurrMeshTransform.InverseTransformPosition(VWorld);
						Vertex.X = VLocal.X;
						Vertex.Y = VLocal.Y;
						Vertex.Z = VLocal.Z;

						Idx++;
					}
				}
			}			

			CurrSplineComponent->PostEditChange();
		}

		CurrWaterBodyComponent->PostEditChange();
	}
}

bool UShallowWaterRiverComponent::QueryWaterAtSplinePoint(TSoftObjectPtr<AWaterBody> WaterBody, int SplinePoint, FVector& OutPos, FVector& OutTangent, float& OutWidth, float& OutDepth)
{	
	if (WaterBody != nullptr)
	{
		TObjectPtr<UWaterBodyComponent> CurrWaterBodyComponent = WaterBody->GetWaterBodyComponent();

		UWaterSplineComponent* CurrSpline = WaterBody->GetWaterSpline();
		
		if (CurrSpline != nullptr)
		{
			// -1 means last spline point
			if (SplinePoint == -1)
			{
				SplinePoint = CurrSpline->GetNumberOfSplinePoints() - 1;
			}

			UWaterSplineMetadata* Metadata = WaterBody->GetWaterSplineMetadata();

			if (Metadata != nullptr)
			{
				OutPos = CurrSpline->GetLocationAtSplineInputKey(SplinePoint, ESplineCoordinateSpace::Local);
				OutPos = CurrSpline->GetComponentTransform().TransformPosition(OutPos);

				OutWidth = Metadata->RiverWidth.Points[SplinePoint].OutVal;
				OutDepth = Metadata->Depth.Points[SplinePoint].OutVal;

				OutTangent = CurrSpline->GetLeaveTangentAtSplinePoint(SplinePoint, ESplineCoordinateSpace::Local);

				OutTangent = CurrSpline->GetComponentTransform().TransformVector(OutTangent);
				OutTangent.Normalize();
			}
			else
			{
				UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::QueryWaterAtSplinePoint() - Water spline metadata is null"));
				return false;
			}
		}
		else
		{
			UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::QueryWaterAtSplinePoint() - Water spline component is null"));
			return false;
		}
	}
	else
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::QueryWaterAtSplinePoint() - Water actor is null"));
		return false;
	}

	return true;
}

void UShallowWaterRiverComponent::OnWaterInfoTextureArrayCreated(const UTextureRenderTarget2DArray* InWaterInfoTexture)
{	
	if (InWaterInfoTexture == nullptr)
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::OnWaterInfoTextureCreated was called with NULL WaterInfoTexture"));
		return;
	}
	
	WaterInfoTexture = InWaterInfoTexture;
	if (RiverSimSystem)
	{
		UTexture* WITTextureArray = Cast<UTexture>(const_cast<UTextureRenderTarget2DArray*>(WaterInfoTexture.Get()));
		if (WITTextureArray == nullptr)
		{
			UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::OnWaterInfoTextureCreated was called with Water Info Texture that isn't valid"));
			return;
		}

		RiverSimSystem->SetVariableTexture(FName("WaterInfoTexture"), WITTextureArray);
	}
	else
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::OnWaterInfoTextureCreated was called with NULL ShallowWaterNiagaraSimulation"));
		return;
	}
}
#endif

void UShallowWaterRiverComponent::SetPaused(bool Pause)
{
	if (RiverSimSystem)
	{
		RiverSimSystem->SetPaused(Pause);
	}

	UFFTOceanPatchSubsystem *OceanPatchSubsystem = GetWorld()->GetSubsystem<UFFTOceanPatchSubsystem>();
	if (OceanPatchSubsystem != nullptr)
	{
		TObjectPtr<UNiagaraComponent> OceanSystem = OceanPatchSubsystem->GetOceanSystem();
		if (OceanSystem)
		{
			OceanSystem->SetPaused(Pause);
		}
	}
}

void UShallowWaterRiverComponent::UpdateRenderState()
{
	TObjectPtr<UTextureRenderTarget2D>  OceanPatchNormalRT = GetSharedFFTOceanPatchNormalRTFromSubsystem(GetWorld());

	if (OceanPatchNormalRT == nullptr)
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Rebuild() - ocean patch normal RT is not initialized"));
		return;
	}
	
	NormalDetailRT = OceanPatchNormalRT;

	if (BakedSimMaterial == nullptr)
	{
		BakedSimMaterial = LoadObject<UMaterialInstance>(nullptr, TEXT("/WaterAdvanced/Niagara/Materials/SW_Water_Material_River.SW_Water_Material_River"));
	}
	
	if (BakedSimRiverToLakeTransitionMaterial == nullptr)
	{
		BakedSimRiverToLakeTransitionMaterial = LoadObject<UMaterialInstance>(nullptr, TEXT("/WaterAdvanced/Niagara/Materials/SW_Water_Material_River_To_Lake_Transition.SW_Water_Material_River_To_Lake_Transition"));
	}

	if (BakedSimRiverToOceanTransitionMaterial == nullptr)
	{
		BakedSimRiverToOceanTransitionMaterial = LoadObject<UMaterialInstance>(nullptr, TEXT("/WaterAdvanced/Niagara/Materials/SW_Water_Material_River_To_Ocean_Transition.SW_Water_Material_River_To_Ocean_Transition"));
	}
	
	if (SplineRiverMaterial == nullptr)
	{
		SplineRiverMaterial = LoadObject<UMaterialInstance>(nullptr, TEXT("/WaterAdvanced/Niagara/Materials/SW_Water_Material_River_Spline.SW_Water_Material_River_Spline"));
	}

	if (SplineRiverToLakeTransitionMaterial == nullptr)
	{
		SplineRiverToLakeTransitionMaterial = LoadObject<UMaterialInstance>(nullptr, TEXT("/WaterAdvanced/Niagara/Materials/SW_Water_Material_River_To_Lake_Transition_Spline.SW_Water_Material_River_To_Lake_Transition_Spline"));
	}

	if (SplineRiverToOceanTransitionMaterial == nullptr)
	{
		SplineRiverToOceanTransitionMaterial = LoadObject<UMaterialInstance>(nullptr, TEXT("/WaterAdvanced/Niagara/Materials/SW_Water_Material_River_To_Ocean_Transition_Spline.SW_Water_Material_River_To_Ocean_Transition_Spline"));
	}

	bool bReadBakedSim = RenderState == EShallowWaterRenderState::BakedSim || RenderState == EShallowWaterRenderState::WaterComponentWithBakedSim || RenderState == EShallowWaterRenderState::WaterComponent;
	bool RenderWaterBody = RenderState == EShallowWaterRenderState::WaterComponent || RenderState == EShallowWaterRenderState::WaterComponentWithBakedSim;
	bool RenderSecondary = 
		RenderState == EShallowWaterRenderState::WaterComponentWithBakedSim || 
		RenderState == EShallowWaterRenderState::BakedSim || RenderState == EShallowWaterRenderState::LiveSim;

	if (RiverSimSystem != nullptr)
	{		
		RiverSimSystem->SetVariableBool(FName("RenderWaterSurface"), !RenderWaterBody);
		RiverSimSystem->SetVariableBool(FName("RenderSecondary"), RenderSecondary);
		RiverSimSystem->SetVariableBool(FName("DebugRenderBottomContour"), RenderState == EShallowWaterRenderState::DebugRenderBottomContour);
		RiverSimSystem->SetVariableBool(FName("DebugRenderFoam"), RenderState == EShallowWaterRenderState::DebugRenderFoam);
		RiverSimSystem->SetVariableBool(FName("ReadCachedSim"), bReadBakedSim);
		
		RiverSimSystem->SetVariableTextureRenderTarget("OceanNormalRT", NormalDetailRT);
		RiverSimSystem->ReinitializeSystem();
	}

	if ((RenderState == EShallowWaterRenderState::BakedSim || RenderState == EShallowWaterRenderState::WaterComponentWithBakedSim) && 
		(BakedWaterSurfaceTexture == nullptr || BakedWaterSurfaceTexture->GetSizeX() == 0 || BakedWaterSurfaceTexture->GetSizeY() == 0))
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::UpdateRenderState() - No baked sim to render"));		
	}

	for (TSoftObjectPtr<AWaterBody > CurrWaterBody : AllWaterBodies)
	{
		if (!CurrWaterBody)
		{
			UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::UpdateRenderState() - Water Body Actor is null- skipping setting render state"));
			continue;
		}

		TObjectPtr<UWaterBodyRiverComponent> CurrWaterBodyComponent = Cast<UWaterBodyRiverComponent>(CurrWaterBody->GetWaterBodyComponent());

		if (CurrWaterBodyComponent != nullptr)
		{
			CurrWaterBodyComponent->SetVisibility(RenderWaterBody);

			if (RenderState == EShallowWaterRenderState::WaterComponentWithBakedSim)
			{				
				CurrWaterBodyComponent->SetWaterMaterial(BakedSimMaterial);
				UMaterialInstanceDynamic* WaterMID = CurrWaterBodyComponent->GetWaterMaterialInstance();			
				SetWaterMIDParameters(WaterMID);

				CurrWaterBodyComponent->SetLakeTransitionMaterial(BakedSimRiverToLakeTransitionMaterial);
				UMaterialInstanceDynamic* WaterLakeTransitionMID = CurrWaterBodyComponent->GetRiverToLakeTransitionMaterialInstance();
				SetWaterMIDParameters(WaterLakeTransitionMID);

				CurrWaterBodyComponent->SetOceanTransitionMaterial(BakedSimRiverToOceanTransitionMaterial);
				UMaterialInstanceDynamic* WaterOceanTransitionMID = CurrWaterBodyComponent->GetRiverToOceanTransitionMaterialInstance();
				SetWaterMIDParameters(WaterOceanTransitionMID);
												
				UMaterialInstanceDynamic* WaterInfoMID = CurrWaterBodyComponent->GetWaterInfoMaterialInstance();
				if (WaterInfoMID)
				{
					WaterInfoMID->SetTextureParameterValue("BakedWaterSimTex", BakedWaterSurfaceTexture);
					WaterInfoMID->SetTextureParameterValue("FoamTex", BakedFoamTexture);
					WaterInfoMID->SetTextureParameterValue("BakedWaterSimNormalTex", BakedWaterSurfaceNormalTexture);
					WaterInfoMID->SetVectorParameterValue("BakedWaterSimLocation", SystemPos);
					WaterMID->SetDoubleVectorParameterValue("BakedWaterSimLocationDouble", SystemPos);
					WaterInfoMID->SetVectorParameterValue("BakedWaterSimSize", FVector(WorldGridSize.X, WorldGridSize.Y, 1));
				}
				else
				{
					UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::UpdateRenderState() - Water Component Water Info MID is null"));
					return;
				}
			}
			else if (RenderState == EShallowWaterRenderState::WaterComponent)
			{
				CurrWaterBodyComponent->SetWaterMaterial(SplineRiverMaterial);
				CurrWaterBodyComponent->SetLakeTransitionMaterial(SplineRiverToLakeTransitionMaterial);
				CurrWaterBodyComponent->SetOceanTransitionMaterial(SplineRiverToOceanTransitionMaterial);
			}

			CurrWaterBodyComponent->SetUseBakedSimulationForQueriesAndPhysics(
				RenderState == EShallowWaterRenderState::WaterComponentWithBakedSim || RenderState == EShallowWaterRenderState::BakedSim);

			/*
			// #todo(dmp): I'd prefer if we could set an editor time only static switch to control using baked sims in the material or not
			TArray<FMaterialParameterInfo> OutMaterialParameterInfos;
			TArray<FGuid> Guids;
			WaterMID->GetAllStaticSwitchParameterInfo(OutMaterialParameterInfos, Guids);

			for (FMaterialParameterInfo& MaterialParameterInfo : OutMaterialParameterInfos)
			{
				if (MaterialParameterInfo.Name == "UseBakedSim")
				{
					WaterMID->SetStaticSwitchParameterValueEditorOnly(MaterialParameterInfo, RenderState == EShallowWaterRenderState::WaterComponentWithBakedSim);
				}
			}
			*/			
		}
	}

	bRenderStateTickInitialize = true;
}

void UShallowWaterRiverComponent::SetWaterMIDParameters(UMaterialInstanceDynamic* WaterMID)
{
	if (WaterMID)
	{
		WaterMID->SetTextureParameterValue("BakedWaterSimTex", BakedWaterSurfaceTexture);
		WaterMID->SetTextureParameterValue("FoamTex", BakedFoamTexture);
		WaterMID->SetTextureParameterValue("BakedWaterSimNormalTex", BakedWaterSurfaceNormalTexture);

		WaterMID->SetVectorParameterValue("BakedWaterSimLocation", SystemPos);
		WaterMID->SetDoubleVectorParameterValue("BakedWaterSimLocationDouble", SystemPos);
		WaterMID->SetVectorParameterValue("BakedWaterSimSize", FVector(WorldGridSize.X, WorldGridSize.Y, 1));

		WaterMID->SetTextureParameterValue("NormalDetailTex", NormalDetailRT);

		float Dx;
		if (WorldGridSize.X > WorldGridSize.Y)
		{
			Dx = WorldGridSize.X / ResolutionMaxAxis;
		}
		else
		{
			Dx = WorldGridSize.Y / ResolutionMaxAxis;
		}
		WaterMID->SetScalarParameterValue("BakedWaterSimDx", Dx);

		WaterMID->SetScalarParameterValue("UseBakedSimHack", RenderState == EShallowWaterRenderState::WaterComponentWithBakedSim ? 1 : 0);
	}
	else
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::UpdateRenderState() - Water Component MID is null"));
		return;
	}
}

AShallowWaterRiver::AShallowWaterRiver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ShallowWaterRiverComponent = CreateDefaultSubobject<UShallowWaterRiverComponent>(TEXT("ShallowWaterRiverComponent"));
	RootComponent = ShallowWaterRiverComponent;

	PrimaryActorTick.bCanEverTick = true;
	SetHidden(false);
}


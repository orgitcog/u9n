// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/ModularVehicleSimulationCU.h"
#include "ChaosModularVehicle/ModularVehicleDefaultAsyncInput.h"
#include "SimModule/SimModulesInclude.h"
#include "SimModule/ModuleInput.h"
#include "PhysicsProxy/ClusterUnionPhysicsProxy.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Chaos/ClusterUnionManager.h"
#include "Chaos/DebugDrawQueue.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "ChaosModularVehicle/ModularVehicleDebug.h"

FModularVehicleDebugParams GModularVehicleDebugParams;
DEFINE_LOG_CATEGORY(LogModularVehicleSim);

#if CHAOS_DEBUG_DRAW
FAutoConsoleVariableRef CVarChaosModularVehiclesRaycastsEnabled(TEXT("p.ModularVehicle.SuspensionRaycastsEnabled"), GModularVehicleDebugParams.SuspensionRaycastsEnabled, TEXT("Enable/Disable Suspension Raycasts."));
FAutoConsoleVariableRef CVarChaosModularVehiclesShowRaycasts(TEXT("p.ModularVehicle.ShowSuspensionRaycasts"), GModularVehicleDebugParams.ShowSuspensionRaycasts, TEXT("Enable/Disable Suspension Raycast Visualisation."));
FAutoConsoleVariableRef CVarChaosModularVehiclesShowWheelData(TEXT("p.ModularVehicle.ShowWheelData"), GModularVehicleDebugParams.ShowWheelData, TEXT("Enable/Disable Displaying Wheel Simulation Data."));
FAutoConsoleVariableRef CVarChaosModularVehiclesShowRaycastMaterial(TEXT("p.ModularVehicle.ShowRaycastMaterial"), GModularVehicleDebugParams.ShowRaycastMaterial, TEXT("Enable/Disable Raycast Material Hit Visualisation."));
FAutoConsoleVariableRef CVarChaosModularVehiclesShowWheelCollisionNormal(TEXT("p.ModularVehicle.ShowWheelCollisionNormal"), GModularVehicleDebugParams.ShowWheelCollisionNormal, TEXT("Enable/Disable Wheel Collision Normal Visualisation."));
FAutoConsoleVariableRef CVarChaosModularVehiclesFrictionOverride(TEXT("p.ModularVehicle.FrictionOverride"), GModularVehicleDebugParams.FrictionOverride, TEXT("Override the physics material friction value.."));
FAutoConsoleVariableRef CVarChaosModularVehiclesDisableAnim(TEXT("p.ModularVehicle.DisableAnim"), GModularVehicleDebugParams.DisableAnim, TEXT("Disable animating wheels, etc"));
#endif


void FModularVehicleSimulation::Initialize(TUniquePtr<Chaos::FSimModuleTree>& InSimModuleTree)
{
	SimModuleTree = MoveTemp(InSimModuleTree);
}

void FModularVehicleSimulation::Terminate()
{
	SimModuleTree.Reset(nullptr);
}

void FModularVehicleSimulation::Simulate(UWorld* InWorld, float DeltaSeconds, const FModularVehicleAsyncInput& InputData, FModularVehicleAsyncOutput& OutputData, IPhysicsProxyBase* Proxy)
{
	if (RootParticle == nullptr)
	{
		CacheRootParticle(Proxy);
	}
	
	Chaos::FPBDRigidsSolver* RigidsSolver = static_cast<Chaos::FPBDRigidsSolver*>(Proxy->GetSolver<Chaos::FPhysicsSolver>());
	int CurrentFrame = -1;
	if (RigidsSolver != nullptr)
	{
		Chaos::FRewindData* RewindData = RigidsSolver->GetRewindData();
		if (RewindData != nullptr)
		{
			CurrentFrame = RewindData->CurrentFrame();
		}
	}

	SimulateModuleTree(InWorld, DeltaSeconds, InputData, OutputData, Proxy);
}

void FModularVehicleSimulation::OnContactModification(Chaos::FCollisionContactModifier& Modifier, IPhysicsProxyBase* Proxy)
{
	using namespace Chaos;
	Chaos::EnsureIsInPhysicsThreadContext();

	if (SimModuleTree.IsValid())
	{
		SimModuleTree->OnContactModification(Modifier, Proxy);
	}
}

void FModularVehicleSimulation::SimulateModuleTree(UWorld* InWorld, float DeltaSeconds, const FModularVehicleAsyncInput& InputData, FModularVehicleAsyncOutput& OutputData, IPhysicsProxyBase* Proxy)
{
	Chaos::EnsureIsInPhysicsThreadContext();

	if (Proxy && SimModuleTree.IsValid())
	{
		int InitialNum = SimModuleTree->GetSimulationModuleTree().Num();

		//if (InWorld)
		//{
		//	WriteNetReport(InWorld->IsNetMode(NM_Client), FString::Printf(TEXT("X %s,  R %s,  V %s,  W %s")
		//		, *Proxy->GetParticle_Internal()->X().ToString()
		//		, *Proxy->GetParticle_Internal()->R().ToString()
		//		, *Proxy->GetParticle_Internal()->V().ToString()
		//		, *Proxy->GetParticle_Internal()->W().ToString()));
		//}

		UE::TReadScopeLock InputConfigLock(InputConfigurationLock);

		FModuleInputContainer Container = InputData.PhysicsInputs.NetworkInputs.VehicleInputs.Container;

		if (ImplementsTestBuffer())
		{
			Chaos::FPBDRigidsSolver* RigidsSolver = static_cast<Chaos::FPBDRigidsSolver*>(Proxy->GetSolver<Chaos::FPhysicsSolver>());
			check(RigidsSolver);
			const int32 CurrentPhysicsFrame = RigidsSolver->GetCurrentFrame();

			if (TestInputBufferStartFrame < 0 || TestInputBufferStartFrame > CurrentPhysicsFrame)
			{
				TestInputBufferStartFrame = CurrentPhysicsFrame;
			}
			int32 InputFrame = (CurrentPhysicsFrame - TestInputBufferStartFrame);

			if (ImplementsLoopingTestBuffer() && !TestInputBuffer.IsValidIndex(InputFrame))
			{
				TestInputBufferStartFrame = CurrentPhysicsFrame;
				InputFrame = 0;
			}

			if (TestInputBuffer.IsValidIndex(InputFrame))
			{
				Container = TestInputBuffer[InputFrame];
			}
		}

		FInputInterface InputInterface(InputNameMap, Container);

		FModuleInputContainer StateInputContainer = InputData.PhysicsInputs.StateInputs.StateInputContainer;
		FInputInterface StateInterface(StateNameMap, StateInputContainer);

		SimInputData.ControlInputs = &InputInterface;
		SimInputData.StateInputs = &StateInterface;
		SimInputData.bKeepVehicleAwake = InputData.PhysicsInputs.NetworkInputs.VehicleInputs.KeepAwake;

		PerformAdditionalSimWork(InWorld, InputData, Proxy, SimInputData);

		// run the dynamics simulation, engine, suspension, wheels, aerofoils etc.
		SimModuleTree->Simulate(DeltaSeconds, SimInputData, Proxy, RootParticle);
	}

}

void FModularVehicleSimulation::CacheRootParticle(IPhysicsProxyBase* Proxy)
{
	Chaos::EnsureIsInPhysicsThreadContext();
	using namespace Chaos;
	RootParticle = nullptr;

	if (Proxy == nullptr)
	{
		return;
	}

	switch (Proxy->GetType())
	{
		case EPhysicsProxyType::ClusterUnionProxy:
		{
			if (FClusterUnionPhysicsProxy* CUProxy = static_cast<FClusterUnionPhysicsProxy*>(Proxy))
			{
				FPBDRigidsEvolutionGBF& Evolution = *static_cast<FPBDRigidsSolver*>(CUProxy->GetSolver<FPBDRigidsSolver>())->GetEvolution();
				FClusterUnionManager& ClusterUnionManager = Evolution.GetRigidClustering().GetClusterUnionManager();
				const FClusterUnionIndex& CUI = CUProxy->GetClusterUnionIndex();
				if (FClusterUnion* ClusterUnion = ClusterUnionManager.FindClusterUnion(CUI))
				{
					if (FPBDRigidClusteredParticleHandle* ClusterHandle = ClusterUnion->InternalCluster)
					{
						RootParticle = ClusterHandle;
					}
				}
			}
		}
		break;

		case EPhysicsProxyType::SingleParticleProxy:
		{
			if (FSingleParticlePhysicsProxy* ParticleProxy = static_cast<FSingleParticlePhysicsProxy*>(Proxy))
			{
				RootParticle = ParticleProxy->GetHandle_LowLevel() ? ParticleProxy->GetHandle_LowLevel()->CastToRigidParticle() : nullptr;
			}
		}
		break;

		default:
		{
			UE_LOG(LogModularVehicleSim, Error, TEXT("Unsupported Particle type"));
		}
		break;
	}
}

void FModularVehicleSimulation::PerformAdditionalSimWork(UWorld* InWorld, const FModularVehicleAsyncInput& InputData, IPhysicsProxyBase* Proxy, Chaos::FAllInputs& AllInputs)
{
	using namespace Chaos;
	check(Proxy);
	Chaos::EnsureIsInPhysicsThreadContext();

	if (SimModuleTree && RootParticle)
	{
		FRigidTransform3 ClusterWorldTM = FRigidTransform3(RootParticle->GetX(), RootParticle->GetR());

		const TArray<FSimModuleTree::FSimModuleNode>& ModuleArray = SimModuleTree->GetSimulationModuleTree();

		for (const FSimModuleTree::FSimModuleNode& Node : ModuleArray)
		{
			if (Node.IsValid() && Node.SimModule && Node.SimModule->IsEnabled())
			{
				FRigidTransform3 Frame = Node.SimModule->GetParentRelativeTransform();

						
				AllInputs.VehicleWorldTransform = ClusterWorldTM;

				if (Node.SimModule->IsClustered() && Node.SimModule->IsBehaviourType(Chaos::eSimModuleTypeFlags::Raycast))
				{
					Chaos::FSpringTrace OutTrace;
					Chaos::FSuspensionBaseInterface* Suspension = static_cast<Chaos::FSuspensionBaseInterface*>(Node.SimModule);

					// would be cleaner an faster to just store radius in suspension also
					float WheelRadius = 0;
					if (Suspension->GetWheelSimTreeIndex() != Chaos::ISimulationModuleBase::INVALID_IDX)
					{
						Chaos::FWheelBaseInterface* Wheel = static_cast<Chaos::FWheelBaseInterface*>(ModuleArray[Suspension->GetWheelSimTreeIndex()].SimModule);
						if (Wheel)
						{
							WheelRadius = Wheel->GetWheelRadius();
						}
					}

					Suspension->GetWorldRaycastLocation(ClusterWorldTM, WheelRadius, OutTrace);

					FVector TraceStart = OutTrace.Start;
					FVector TraceEnd = OutTrace.End;

					const FCollisionQueryParams& TraceParams = InputData.PhysicsInputs.TraceParams;
					FVector TraceVector(TraceStart - TraceEnd);
					FVector TraceNormal = TraceVector.GetSafeNormal();

					TArray<FHitResult> HitResultsOut;
					FHitResult HitResult = FHitResult();
					ECollisionChannel SpringCollisionChannel = InputData.PhysicsInputs.CollisionChannel;
					const FCollisionResponseParams& ResponseParams = InputData.PhysicsInputs.TraceCollisionResponse;
					if (InWorld)
					{
						switch (InputData.PhysicsInputs.TraceType)
						{
							case ETraceType::Spherecast:
							{
								float QueryRadius = WheelRadius;
								if (Chaos::Private::FGenericPhysicsInterface_Internal::SpherecastMulti(InWorld
									, QueryRadius
									, HitResultsOut
									, TraceStart + TraceNormal * WheelRadius
									, TraceEnd + TraceNormal * WheelRadius
									, SpringCollisionChannel
									, TraceParams
									, ResponseParams))
								{
									HitResult = HitResultsOut.Last();
								}
							}
							break;

							case ETraceType::Raycast:
							default:
							{
								float QueryRadius = 0.0f;
								if (Chaos::Private::FGenericPhysicsInterface_Internal::SpherecastMulti(InWorld, QueryRadius, HitResultsOut, TraceStart, TraceEnd, SpringCollisionChannel, TraceParams, ResponseParams))
								{
									HitResult = HitResultsOut.Last();
								}
							}
							break;
						}
					}

					float Offset = Suspension->GetMaxSpringLength();
					if (HitResult.bBlockingHit && GModularVehicleDebugParams.SuspensionRaycastsEnabled)
					{
						Offset = HitResult.Distance - WheelRadius;

						if (Suspension->GetWheelSimTreeIndex() != Chaos::ISimulationModuleBase::INVALID_IDX)
						{
							const Chaos::FSimModuleTree::FSimModuleNode& WheelNode = ModuleArray[Suspension->GetWheelSimTreeIndex()];

							Chaos::FWheelBaseInterface* Wheel = static_cast<Chaos::FWheelBaseInterface*>(WheelNode.SimModule);
							if (Wheel && HitResult.PhysMaterial.IsValid())
							{
								if (GModularVehicleDebugParams.FrictionOverride > 0)
								{
									Wheel->SetSurfaceFriction(GModularVehicleDebugParams.FrictionOverride);
								}
								else
								{
									Wheel->SetSurfaceFriction(HitResult.PhysMaterial->Friction);
								}
							}
						}

#if CHAOS_DEBUG_DRAW
						if (GModularVehicleDebugParams.ShowSuspensionRaycasts)
						{
							Chaos::FDebugDrawQueue::GetInstance().DrawDebugSphere(HitResult.ImpactPoint, 3, 16, FColor::Red, false, -1.f, 0, 10.f);
						}

						if (Suspension->GetWheelSimTreeIndex() != Chaos::ISimulationModuleBase::INVALID_IDX)
						{
							Chaos::FWheelBaseInterface* Wheel = static_cast<Chaos::FWheelBaseInterface*>(ModuleArray[Suspension->GetWheelSimTreeIndex()].SimModule);
							if (Wheel)
							{
								if (GModularVehicleDebugParams.ShowWheelData)
								{
									FString TextOut = FString::Format(TEXT("{0}"), { Wheel->GetForceIntoSurface() });
									FColor Col = FColor::White;
									if (InWorld)
									{
										if (InWorld->GetNetMode() == ENetMode::NM_Client)
										{
											Col = FColor::Blue;
										}
										else
										{
											Col = FColor::Red;
										}
									}
									Chaos::FDebugDrawQueue::GetInstance().DrawDebugString(HitResult.ImpactPoint + FVec3(0, 50, 50), TextOut, nullptr, Col, -1.f, true, 1.0f);
								}
							}
						}

#endif
					}

#if CHAOS_DEBUG_DRAW
					if (GModularVehicleDebugParams.ShowSuspensionRaycasts)
					{
						FColor DrawColor = FColor::Green;
						DrawColor = (HitResult.bBlockingHit) ? FColor::Red : FColor::Green;
						Chaos::FDebugDrawQueue::GetInstance().DrawDebugLine(TraceStart, TraceEnd, DrawColor, false, -1.f, 0, 2.f);
						Chaos::FDebugDrawQueue::GetInstance().DrawDebugSphere(TraceStart, 3, 16, FColor::White, false, -1.f, 0, 10.f);
						Chaos::FDebugDrawQueue::GetInstance().DrawDebugSphere(HitResult.ImpactPoint, 1, 16, FColor::Red, false, -1.f, 0, 10.f);
						FString TextOut = FString::Format(TEXT("{0}"), { HitResult.Time });

						FColor Col = FColor::White;
						if (InWorld)
						{
							if (InWorld->GetNetMode() == ENetMode::NM_Client)
							{
								Col = FColor::Blue;
							}
							else
							{
								Col = FColor::Red;
							}
						}
						Chaos::FDebugDrawQueue::GetInstance().DrawDebugString(HitResult.ImpactPoint + FVec3(0, 50, 50), TextOut, nullptr, Col, -1.f, true, 1.0f);
					}

					if (GModularVehicleDebugParams.ShowRaycastMaterial)
					{
						if (HitResult.PhysMaterial.IsValid())
						{
							FDebugDrawQueue::GetInstance().DrawDebugString(HitResult.ImpactPoint, HitResult.PhysMaterial->GetName(), nullptr, FColor::White, -1.f, true, 1.0f);
						}
					}

					if (GModularVehicleDebugParams.ShowWheelCollisionNormal)
					{
						FVector Pt = HitResult.ImpactPoint;
						FDebugDrawQueue::GetInstance().DrawDebugLine(Pt, Pt + HitResult.Normal * 20.0f, FColor::Yellow, false, 1.0f, 0, 1.0f);
						FDebugDrawQueue::GetInstance().DrawDebugSphere(Pt, 5.0f, 4, FColor::White, false, 1.0f, 0, 1.0f);
					}

#endif
					Suspension->SetSpringLength(Offset, WheelRadius);
					FVector Up = ClusterWorldTM.GetUnitAxis(EAxis::Z);

					FVector HitPoint;
					float HitDistance = 0.f;
					if (InputData.PhysicsInputs.TraceType == ETraceType::Spherecast)
					{
						HitPoint = HitResult.Location;
						HitDistance = HitResult.Distance;
					}
					else
					{
						HitPoint = HitResult.ImpactPoint + Up * WheelRadius;
						HitDistance = HitResult.Distance - WheelRadius;
					}

					const TEnumAsByte<EPhysicalSurface> DefaultSurfaceType = EPhysicalSurface::SurfaceType_Default;
					FSuspensionTargetPoint TargetPoint(
						HitPoint
						, HitResult.ImpactNormal
						, HitDistance
						, HitResult.bBlockingHit
						, HitResult.PhysMaterial.IsValid() ? HitResult.PhysMaterial->SurfaceType : DefaultSurfaceType
					);

					Suspension->SetTargetPoint(TargetPoint);
				}

			}

		}

	}
}

void FModularVehicleSimulation::ApplyDeferredForces(IPhysicsProxyBase* Proxy)
{
	using namespace Chaos;

	Chaos::EnsureIsInPhysicsThreadContext();

	if (SimModuleTree && Proxy)
	{

		SimModuleTree->AccessDeferredForces().Apply(RootParticle);
		
	}
}

void FModularVehicleSimulation::FillOutputState(FModularVehicleAsyncOutput& Output)
{
	if (Chaos::FSimModuleTree* SimTree = GetSimComponentTree().Get())
	{
		for (int I = 0; I < SimTree->GetNumNodes(); I++)
		{
			if (SimTree->GetSimModule(I))
			{
				if (Chaos::FSimOutputData* OutData = SimTree->AccessSimModule(I)->GenerateOutputData())
				{
					OutData->FillOutputState(SimTree->GetSimModule(I));
					Output.VehicleSimOutput.SimTreeOutputData.Add(OutData);
				}
			}
		}
	}
}


// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigPhysicsSimulation.h"

#include "Rigs/RigHierarchy.h"
#include "Rigs/RigHierarchyController.h"
#include "ControlRig.h"

#include "RigVMCore/RigVMDrawInterface.h"

#include "Physics/ImmediatePhysics/ImmediatePhysicsActorHandle.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsJointHandle.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsSimulation.h"

#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/PBDJointConstraints.h"
#include "Chaos/ImplicitObject.h"
#include "Chaos/ShapeInstance.h"
#include "Chaos/Capsule.h"
#include "Chaos/PBDJointConstraintUtilities.h"

#include "Engine/Engine.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "Math/Color.h"

namespace RigPhysicsSolverDraw
{
	FLinearColor DynamicColor = FColor::Yellow;
	FLinearColor KinematicColor = FColor::Blue;

	FLinearColor ActiveContactColor = FColor::Red;
	FLinearColor InactiveContactColor = FColor::Silver;

}

TAutoConsoleVariable<int> CVarControlRigPhysicsShowActiveContactsOverride(
	TEXT("ControlRig.Physics.ShowActiveContactsOveride"), -1,
	TEXT("Whether to draw active contacts (requires visualization to be enabled). -1 uses the visualization setting, 0 forces drawing to be disabled, 1 forces it to be enabled."));

TAutoConsoleVariable<int> CVarControlRigPhysicsShowInactiveContactsOverride(
	TEXT("ControlRig.Physics.ShowInactiveContactsOveride"), -1,
	TEXT("Whether to draw inactive contacts (requires visualization to be enabled). -1 uses the visualization setting, 0 forces drawing to be disabled, 1 forces it to be enabled."));

//======================================================================================================================
static void DrawShapes(
	FRigVMDrawInterface*                    DI,
	const FRigPhysicsVisualizationSettings& VisualizationSettings,
	const bool                              bIsKinematic,
	const Chaos::FGeometryParticleHandle*   Particle,
	const FTransform&                       ShapeTransform, 
	const Chaos::FImplicitObject*           ImplicitObject,
	const Chaos::FPerShapeData*             Shape)
{
	using namespace Chaos;

	if (!ImplicitObject)
	{
		return;
	}
	const EImplicitObjectType PackedType = ImplicitObject->GetType(); // Type includes scaling and instancing data
	const EImplicitObjectType InnerType = GetInnerType(ImplicitObject->GetType());

	// For simplicity, we're going to assume no scaling, instancing, transforming etc

	if (!ImplicitObject)
	{
		return;
	}

	switch (InnerType)
	{
	case ImplicitObjectType::Transformed:
	{
		const TImplicitObjectTransformed<FReal, 3>* Transformed = 
			ImplicitObject->GetObject<TImplicitObjectTransformed<FReal, 3>>();
		FTransform TransformedTransform = FTransform(
			ShapeTransform.GetRotation() * Transformed->GetTransform().GetRotation(),
			ShapeTransform.TransformPosition(Transformed->GetTransform().GetLocation()));
		DrawShapes(
			DI, VisualizationSettings, bIsKinematic, Particle, 
			TransformedTransform, Transformed->GetTransformedObject(), Shape);
		return;
	}
	case ImplicitObjectType::Sphere:
	{
		const Chaos::FSphere* Sphere = ImplicitObject->GetObject<Chaos::FSphere>();
		DI->DrawSphere(ShapeTransform, FTransform(FVec3(Sphere->GetCenterOfMass())), Sphere->GetRadiusf(),
			bIsKinematic ? RigPhysicsSolverDraw::KinematicColor : RigPhysicsSolverDraw::DynamicColor,
			VisualizationSettings.LineThickness, VisualizationSettings.ShapeDetail);
		break;
	}
	case ImplicitObjectType::Box:
	{
		const TBox<FReal, 3>* Box = ImplicitObject->GetObject<TBox<FReal, 3>>();
		DI->DrawBox(ShapeTransform, FTransform(FQuat::Identity, Box->GetCenter(), Box->Extents()),
			bIsKinematic ? RigPhysicsSolverDraw::KinematicColor : RigPhysicsSolverDraw::DynamicColor,
			VisualizationSettings.LineThickness);
		break;
	}
	case ImplicitObjectType::Capsule:
	{
		const Chaos::FCapsule* Capsule = ImplicitObject->GetObject<Chaos::FCapsule>();
		const FQuat Q = FRotationMatrix::MakeFromZ(Capsule->GetAxis()).ToQuat();
		DI->DrawCapsule(ShapeTransform, FTransform(Q, Capsule->GetCenterOfMass()),
			Capsule->GetRadiusf(), Capsule->GetHeightf(),
			bIsKinematic ? RigPhysicsSolverDraw::KinematicColor : RigPhysicsSolverDraw::DynamicColor,
			VisualizationSettings.LineThickness);
		break;
	}
	default:
		// If we don't know what it is, don't draw it.
		break;
	}

}

//======================================================================================================================
static void DrawActor(
	FRigVMDrawInterface*                    DI,
	const FRigPhysicsVisualizationSettings& VisualizationSettings,
	const FTransform&                       SpaceTransform,
	const ImmediatePhysics::FActorHandle&   ActorHandle)
{
	bool bIsKinematic = ActorHandle.GetIsKinematic();
	const Chaos::FGeometryParticleHandle* GeometryParticleHandle = ActorHandle.GetParticle();
	const FTransform ParticleTransform = GeometryParticleHandle->GetTransformXR() * SpaceTransform;
	for (const Chaos::FShapeInstancePtr& ShapeInstance : GeometryParticleHandle->ShapeInstances())
	{
		const Chaos::FImplicitObject* ImplicitObject = ShapeInstance->GetGeometry();
		DrawShapes(
			DI, VisualizationSettings, bIsKinematic, GeometryParticleHandle,
			ParticleTransform, ImplicitObject, ShapeInstance.Get());
	}
}

enum
{
	TwistIndex = 0,
	Swing1Index = 1,
	Swing2Index = 2
};

// Returns a number to use for the limit for limit based on limit type
float GetLimitAngleRadians(float LimitAngle, Chaos::EJointMotionType LimitType)
{
	switch (LimitType)
	{
	case Chaos::EJointMotionType::Free: return UE_PI;
	case Chaos::EJointMotionType::Locked: return 0.f;
	default: return LimitAngle;
	}
}

//======================================================================================================================
static void DrawJoint(
	FRigVMDrawInterface*                    DI,
	const FRigPhysicsVisualizationSettings& VisualizationSettings,
	const FTransform&                       SpaceTransform,
	const ImmediatePhysics::FJointHandle&   JointHandle)
{
	const Chaos::FPBDJointConstraintHandle* ConstraintHandle = JointHandle.GetConstraint();
	if (!ConstraintHandle)
	{
		return;
	}

	const Chaos::TVec2<const ImmediatePhysics_Chaos::FActorHandle*>& ActorHandles = JointHandle.GetActorHandles();
	const ImmediatePhysics_Chaos::FActorHandle* ChildActor = ActorHandles[0];
	const ImmediatePhysics_Chaos::FActorHandle* ParentActor = ActorHandles[1];

	FTransform ChildActorTM = (ChildActor ? ChildActor->GetWorldTransform() : FTransform()) * SpaceTransform;
	FTransform ParentActorTM = (ParentActor ? ParentActor->GetWorldTransform() : FTransform()) * SpaceTransform;

	bool bChildIsKinematic = ChildActor ? ChildActor->GetIsKinematic() : true;
	bool bParentIsKinematic = ParentActor ? ParentActor->GetIsKinematic() : true;

	float Size = 5.0f * VisualizationSettings.ShapeSize;

	const Chaos::FPBDJointSettings& JointSettings = ConstraintHandle->GetSettings();

	FTransform DialFrame = JointSettings.ConnectorTransforms[0] * ChildActorTM;
	FTransform LimitFrame = JointSettings.ConnectorTransforms[1] * ParentActorTM;

	static bool bDrawAsAxes = false;
	if (bDrawAsAxes)
	{
		DI->DrawAxes(LimitFrame, FTransform(), Size, VisualizationSettings.LineThickness * 2);
		DI->DrawAxes(DialFrame, FTransform(), Size, VisualizationSettings.LineThickness);
	}

	{
		// See FConstraintInstance::DrawConstraintImp for inspiration

		// There seems to be a swap between swing1 and swing2 compared to GetSwingTwistAngles
		FVector LimitAngleRadians(
			GetLimitAngleRadians(JointSettings.AngularLimits[0], JointSettings.AngularMotionTypes[0]),
			GetLimitAngleRadians(JointSettings.AngularLimits[2], JointSettings.AngularMotionTypes[2]),
			GetLimitAngleRadians(JointSettings.AngularLimits[1], JointSettings.AngularMotionTypes[1])
		);

		Chaos::FReal TwistAngle, Swing1Angle, Swing2Angle;
		const FQuat LimitQ = LimitFrame.GetRotation();
		FQuat DialQ = DialFrame.GetRotation();
		DialQ.EnforceShortestArcWith(LimitQ);
		Chaos::FPBDJointUtilities::GetSwingTwistAngles(LimitQ, DialQ, TwistAngle, Swing1Angle, Swing2Angle);

		bool bDrawViolatedLimits = true;

		bool bTwistViolated = false;
		bool bSwing1Violated = false;
		bool bSwing2Violated = false;

		if (bDrawViolatedLimits)
		{
			bTwistViolated = JointSettings.AngularMotionTypes[TwistIndex] == Chaos::EJointMotionType::Limited &&
				FMath::Abs(TwistAngle) > LimitAngleRadians[0];

			bSwing1Violated = JointSettings.AngularMotionTypes[Swing1Index] == Chaos::EJointMotionType::Limited &&
				FMath::Abs(Swing1Angle) > LimitAngleRadians[1];

			bSwing2Violated = JointSettings.AngularMotionTypes[Swing2Index] == Chaos::EJointMotionType::Limited &&
				FMath::Abs(Swing2Angle) > LimitAngleRadians[2];
		}

		const bool bLockSwing1 = JointSettings.AngularMotionTypes[Swing1Index] == Chaos::EJointMotionType::Locked;
		const bool bLockSwing2 = JointSettings.AngularMotionTypes[Swing2Index] == Chaos::EJointMotionType::Locked;
		const bool bLockAllSwing = bLockSwing1 && bLockSwing2;

		check(GEngine->ConstraintLimitMaterialX && GEngine->ConstraintLimitMaterialY && GEngine->ConstraintLimitMaterialZ);
		static UMaterialInterface* LimitMaterialX = GEngine->ConstraintLimitMaterialX;
		static UMaterialInterface* LimitMaterialXAxis = GEngine->ConstraintLimitMaterialXAxis;
		static UMaterialInterface* LimitMaterialY = GEngine->ConstraintLimitMaterialY;
		static UMaterialInterface* LimitMaterialYAxis = GEngine->ConstraintLimitMaterialYAxis;
		static UMaterialInterface* LimitMaterialZ = GEngine->ConstraintLimitMaterialZ;
		static UMaterialInterface* LimitMaterialZAxis = GEngine->ConstraintLimitMaterialZAxis;

		float ArrowLength = Size * 1.05f; // stick out a little bit 

		// If swing is limited (but not locked) - draw the swing limit cone.
		if (!bLockAllSwing)
		{
			if (JointSettings.AngularMotionTypes[Swing1Index] == Chaos::EJointMotionType::Free && 
				JointSettings.AngularMotionTypes[Swing2Index] == Chaos::EJointMotionType::Free)
			{
				DI->DrawSphere(LimitFrame, FTransform(), Size * 0.2f, FColor::White, 
					VisualizationSettings.LineThickness, VisualizationSettings.ShapeDetail);
			}
			else
			{
				FTransform ConeLimitTM = LimitFrame;
				ConeLimitTM.SetScale3D(FVector(Size));
				const float Swing1LimitAngle = LimitAngleRadians[Swing1Index];
				const float Swing2LimitAngle = LimitAngleRadians[Swing2Index];
				DI->DrawCone(
					ConeLimitTM, FTransform(), Swing1LimitAngle, Swing2LimitAngle, VisualizationSettings.ShapeDetail, 
					true, FColor::Green, LimitMaterialX->GetRenderProxy(), VisualizationSettings.LineThickness);
			}
			// Draw the swing Dial indicator - shows the current orientation of the child frame
			// relative to the parent frame on the swing axis.
			// Start the arrow at the limit position as it can be confusing if there is joint separation
			DI->DrawArrow(FTransform(LimitFrame.GetTranslation()), 
				DialFrame.GetUnitAxis(EAxis::X) * ArrowLength,
				DialFrame.GetUnitAxis(EAxis::Y), FColor::Red, VisualizationSettings.LineThickness);

			if (bSwing1Violated || bSwing2Violated)
			{
				// Drawing a sphere seems a bit heavy... but it appears that if DrawPoint is used then they get
				// culled when you get close!
				DI->DrawSphere(FTransform(
					DialFrame.GetRotation(),
					LimitFrame.GetTranslation() + DialFrame.GetUnitAxis(EAxis::X) * ArrowLength),
					FTransform(),
					ArrowLength * 0.01f, FColor::Orange, VisualizationSettings.LineThickness * 4);
			}

		}

		// Draw the twist limit
		if (JointSettings.AngularMotionTypes[TwistIndex] != Chaos::EJointMotionType::Locked)
		{
			// Draw as a flat cone
			FTransform ConeLimitTM = LimitFrame;
			ConeLimitTM.SetScale3D(FVector(Size));
			const float TwistLimitAngle = LimitAngleRadians[TwistIndex];
			DI->DrawCone(ConeLimitTM, FTransform(FQuat::MakeFromRotationVector(FVector(0, 1, 0) * -HALF_PI)),
				TwistLimitAngle, 0.0f, VisualizationSettings.ShapeDetail, true, FColor::Green,
				LimitMaterialYAxis->GetRenderProxy(), VisualizationSettings.LineThickness);

			// Draw the twist Dial indicator - shows the current orientation of the child frame
			// relative to the parent frame on the twist axis.
			FQuat Rot(LimitFrame.GetUnitAxis(EAxis::X), TwistAngle);
			FVector TwistArrow = Rot * LimitFrame.GetUnitAxis(EAxis::Z);
			FVector TwistSideDir = FVector::CrossProduct(LimitFrame.GetUnitAxis(EAxis::X), TwistArrow);

			DI->DrawArrow(FTransform(LimitFrame.GetTranslation()),
				TwistArrow * ArrowLength,
				TwistSideDir, FColor::Blue, VisualizationSettings.LineThickness);

			if (bTwistViolated)
			{
				DI->DrawSphere(FTransform(
					DialFrame.GetRotation(),
					LimitFrame.GetTranslation() + TwistArrow * ArrowLength),
					FTransform(),
					ArrowLength * 0.01f, FColor::Orange, VisualizationSettings.LineThickness * 4);
			}
		}
	}
}

//======================================================================================================================
void FRigPhysicsSimulation::Draw(
	FRigVMDrawInterface*                    DI,
	const FRigPhysicsSolverSettings&        SolverSettings,
	const FRigPhysicsVisualizationSettings& VisualizationSettings,
	const UWorld*                           DebugWorld) const
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RigPhysics_Draw);

#if CHAOS_DEBUG_DRAW
	// Danny TODO enable/disable chaos debug draw with a cvar or flag
	if (DebugWorld && DebugWorld->GetPhysicsScene() && DebugWorld->GetPhysicsScene()->GetDebugDrawScene())
	{
		Simulation->SetDebugDrawScene(FString("ControlRig"), DebugWorld->GetPhysicsScene()->GetDebugDrawScene());
		Simulation->DebugDraw();
	}
#endif

	const URigHierarchy* Hierarchy = OwningControlRig->GetHierarchy();
	if (!DI || !Hierarchy)
	{
		return;
	}

	// All rendering is done relative to the component, so convert the sim space (identity) into the
	// component space.
	const FTransform SpaceTransform = ConvertSimSpaceTransformToComponentSpace(SolverSettings, FTransform());

	if (CollisionActorHandle)
	{
		DrawActor(DI, VisualizationSettings, SpaceTransform, *CollisionActorHandle);
	}

	for (const TPair<FRigComponentKey, FRigBodyRecord>& BodyRecordPair : BodyRecords)
	{
		const FRigBodyRecord& Record = BodyRecordPair.Value;
		if (const ImmediatePhysics::FActorHandle* ActorHandle = Record.ActorHandle)
		{
			DrawActor(DI, VisualizationSettings, SpaceTransform, *ActorHandle);
		}
	}
	for (const TPair<FRigComponentKey, FRigJointRecord>& JointRecordPair : JointRecords)
	{
		const FRigJointRecord& Record = JointRecordPair.Value;
		if (const ImmediatePhysics::FJointHandle* JointHandle = Record.JointHandle)
		{
			DrawJoint(DI, VisualizationSettings, SpaceTransform, *JointHandle);
		}
	}

	int32 ShowActiveOverride = CVarControlRigPhysicsShowActiveContactsOverride.GetValueOnAnyThread();
	int32 ShowInactiveOverride = CVarControlRigPhysicsShowInactiveContactsOverride.GetValueOnAnyThread();

	bool bShowActiveContacts = ShowActiveOverride < 0 ? VisualizationSettings.bShowActiveContacts : 
		(ShowActiveOverride ? true : false);
	bool bShowInactiveContacts = ShowInactiveOverride < 0 ? VisualizationSettings.bShowInactiveContacts :
		(ShowInactiveOverride ? true : false);

	if (bShowActiveContacts || bShowInactiveContacts)
	{
		Simulation->VisitCollisions(
			[DI, &SpaceTransform, &VisualizationSettings, bShowActiveContacts, bShowInactiveContacts]
			(const ImmediatePhysics::FSimulation::FCollisionData& Collision)
			{
				using namespace Chaos;

				bool bIsActive = Collision.GetCollisionAccumulatedImpulse().IsZero();
				if ((bIsActive && bShowActiveContacts) || (!bIsActive && bShowInactiveContacts))
				{
					float Size = 5.0f * VisualizationSettings.ShapeSize;

					int32 NumManifoldPoints = Collision.GetNumManifoldPoints();
					for (int32 PointIndex = 0; PointIndex != NumManifoldPoints; ++PointIndex)
					{
						Chaos::FRealSingle Depth;
						Chaos::FVec3 PlaneNormal;
						Chaos::FVec3 PointLocation;
						Chaos::FVec3 PlaneLocation;
						Collision.GetManifoldPointData(
							PointIndex, Depth, PlaneNormal, PointLocation, PlaneLocation);

						const FVec3 PointPlaneLocation = PointLocation -
							FVec3::DotProduct(PointLocation - PlaneLocation, PlaneNormal) * PlaneNormal;

						FMatrix Axes = FRotationMatrix::MakeFromZ(PlaneNormal);
						FTransform TM(Axes);
						TM.SetTranslation(PointPlaneLocation);

						DI->DrawCircle(
							SpaceTransform, TM, Size,
							bIsActive ? RigPhysicsSolverDraw::ActiveContactColor : RigPhysicsSolverDraw::InactiveContactColor,
							VisualizationSettings.LineThickness, VisualizationSettings.ShapeDetail);
					}
				}
			}, Chaos::ECollisionVisitorFlags::VisitDefault);
	}
}


// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "AutoRTFMTesting.h"
#include "AutoRTFMTestActor.h"
#include "AutoRTFMTestAnotherActor.h"
#include "AutoRTFMTestBodySetup.h"
#include "AutoRTFMTestChildActorComponent.h"
#include "AutoRTFMTestLevel.h"
#include "AutoRTFMTestObject.h"
#include "AutoRTFMTestPrimitiveComponent.h"
#include "AutoRTFMTestCameraShake.h"
#include "Chaos/Core.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/LevelStreamingPersistent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/LightWeightInstanceSubsystem.h"
#include "PBDRigidsSolver.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "UObject/Package.h"
#include "UObject/UObjectArray.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtrTemplates.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{

// Declares a new AutoRTFM actor component test with the given name.
// The test body should follow the call to the macro with braces.
// Prior to calling the test, the test will create the following objects and
// will be within scope to the test body:
// - UWorld* World
// - UAutoRTFMTestLevel* Level
// - AAutoRTFMTestActor* Actor
// - UAutoRTFMTestPrimitiveComponent* Component
//
// Initial state:
// - Level->OwningWorld will be assigned World.
// - Component will *not* be automatically registered.
//
// Example:
// AUTORTFM_ACTOR_COMPONENT_TEST(MyTest)
// {
//     // Test something using World, Level, Actor, Component.
// }
#define AUTORTFM_ACTOR_COMPONENT_TEST(NAME) \
	class FAutoRTFMTest##NAME : public FAutoRTFMActorComponentTestBase \
	{ \
		using FAutoRTFMActorComponentTestBase::FAutoRTFMActorComponentTestBase; \
		void Run(UWorld* World, UAutoRTFMTestLevel* Level, AAutoRTFMTestActor* Actor, UAutoRTFMTestPrimitiveComponent* Component) override; \
	}; \
	FAutoRTFMTest##NAME AutoRTFMTestInstance##NAME(TEXT(#NAME), TEXT(__FILE__), __LINE__); \
	void FAutoRTFMTest##NAME::Run(UWorld* World, UAutoRTFMTestLevel* Level, AAutoRTFMTestActor* Actor, UAutoRTFMTestPrimitiveComponent* Component)

// The base class used by the AUTORTFM_ACTOR_COMPONENT_TEST() tests
class FAutoRTFMActorComponentTestBase : public FAutomationTestBase
{
public:
	FAutoRTFMActorComponentTestBase(const TCHAR* InName, const TCHAR* File, int32 Line)
		: FAutomationTestBase(InName, /* bInComplexTask */ false), TestFile(File), TestLine(Line) {}

	// The AUTORTFM_ACTOR_COMPONENT_TEST() virtual function.
	virtual void Run(UWorld* World,
		UAutoRTFMTestLevel* Level,
		AAutoRTFMTestActor* Actor,
		UAutoRTFMTestPrimitiveComponent* Component) = 0;

	// GetTestFlags() changed return type between branches. Support old and new types.
	using GetTestFlagsReturnType = decltype(std::declval<FAutomationTestBase>().GetTestFlags());

	GetTestFlagsReturnType GetTestFlags() const
	{
		return EAutomationTestFlags::EngineFilter |
			EAutomationTestFlags::ClientContext |
			EAutomationTestFlags::ServerContext |
			EAutomationTestFlags::CommandletContext;
	}

	bool IsStressTest() const { return false; }
	uint32 GetRequiredDeviceNum() const override { return 1; }
	FString GetTestSourceFileName() const override { return TestFile; }
	int32 GetTestSourceFileLine() const override { return TestLine; }

protected:
	void GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const override
	{
		OutBeautifiedNames.Add("AutoRTFM.ActorComponent." + TestName);
		OutTestCommands.Add(FString());
	}

	FString GetBeautifiedTestName() const override { return "AutoRTFM.ActorComponent." + TestName; }

	// Implementation of the pure-virtual FAutomationTestBase::RunTest().
	// Skips the test with a message if IsAutoRTFMRuntimeEnabled() return false,
	// otherwise constructs the test World, Level, Actor and Component objects
	// and passes these to Run().
	bool RunTest(const FString& Parameters) override
	{
		if (AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled())
		{
			UWorld* World = NewObject<UWorld>();
			World->CreatePhysicsScene(nullptr);
			World->InitializeNewWorld();

			UAutoRTFMTestLevel* Level = NewObject<UAutoRTFMTestLevel>();
			Level->OwningWorld = World;
			AAutoRTFMTestActor* Actor = NewObject<AAutoRTFMTestActor>(Level);
			UAutoRTFMTestPrimitiveComponent* Component = NewObject<UAutoRTFMTestPrimitiveComponent>(Actor);

			Run(World, Level, Actor, Component);

			World->CleanupWorld();

			if (Component->IsRegistered())
			{
				Component->UnregisterComponent();
			}
		}
		else
		{
			FString Desc = FString::Printf(TEXT("SKIPPED test '%s'. AutoRTFM disabled."), GetData(TestName));
			ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Info, Desc));
		}

		return true;
	}

	// Adds an error message to the test with the provided What description.
	// File and Line should be the source file and line number that performed
	// the test, respectively.
	void Fail(const TCHAR* What, const TCHAR* File, unsigned int Line)
	{
		AddError(FString::Printf(TEXT("FAILED: %s:%u %s"), File, Line, What), 1);
	}

private:
	const TCHAR* const TestFile;
	const int32 TestLine;
};

// Calls ForEachObjectOfClass() to count the number of AAutoRTFMTestActor instances currently alive.
size_t CountAAutoRTFMTestActors()
{
	size_t Count = 0;
	ForEachObjectOfClass(AAutoRTFMTestActor::StaticClass(), [&](UObject* const Obj)
	{
		Count++;
	});
	return Count;
};

// General tests for calling RegisterComponent() and UnregisterComponent() in transactions.
// See: SOL-6709
AUTORTFM_ACTOR_COMPONENT_TEST(RegisterComponent_UnregisterComponent)
{
	Component->BodyInstance.SetPhysicsActor(Chaos::FSingleParticlePhysicsProxy::Create(Chaos::FGeometryParticle::CreateParticle()));
	Component->BodyInstance.GetPhysicsActor()->GetParticle_LowLevel()->SetGeometry(MakeImplicitObjectPtr<Chaos::FSphere>(Chaos::FVec3(1, 2, 3), 1));
	World->GetPhysicsScene()->GetSolver()->RegisterObject(Component->BodyInstance.GetPhysicsActor());

	AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			Component->RegisterComponent();

			if (Component->IsRegistered())
			{
				AutoRTFM::AbortTransaction();
			}
		});

	TestTrueExpr(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	TestFalseExpr(Component->IsRegistered());

	bool bWasRegistered = false;

	AutoRTFM::Commit([&]
		{
			Component->RegisterComponent();
			bWasRegistered = Component->IsRegistered();
		});

	TestTrueExpr(bWasRegistered);
	TestTrueExpr(Component->IsRegistered());

	Result = AutoRTFM::Transact([&]
		{
			Component->UnregisterComponent();
			AutoRTFM::AbortTransaction();
		});

	TestTrueExpr(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	TestTrueExpr(Component->IsRegistered());

	AutoRTFM::Commit([&]
		{
			Component->UnregisterComponent();
		});

	TestFalseExpr(Component->IsRegistered());
}

// Test aborting a call to Component::RegisterComponentWithWorld().
// See: FORT-761015
AUTORTFM_ACTOR_COMPONENT_TEST(RegisterComponentWithWorld)
{
	// Create a valid body setup so that there are shapes created
	Component->BodySetup = NewObject<UAutoRTFMTestBodySetup>();
	Component->BodySetup->AggGeom.SphereElems.Add(FKSphereElem(1.0f));

	AutoRTFM::ETransactionResult Result;
	Result = AutoRTFM::Transact([&]
		{
			Component->RegisterComponentWithWorld(World);
			AutoRTFM::AbortTransaction();
		});

	TestTrueExpr(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	TestFalseExpr(Component->IsRegistered());

	AutoRTFM::Commit([&]
		{
			Component->RegisterComponentWithWorld(World);
		});

	TestTrueExpr(Component->IsRegistered());
}

// Test aborting a call to Component::WeldTo().
// See: SOL-6757
AUTORTFM_ACTOR_COMPONENT_TEST(WeldTo)
{
	Component->RegisterComponent();

	FBodyInstance SomeInstance;

	// This test requires us to have a fresh body instance so that it has to be created during the register.
	Component->BodyInstance = FBodyInstance();
	Component->BodyInstance.bSimulatePhysics = 1;
	Component->BodyInstance.WeldParent = &SomeInstance;
	TestTrueExpr(Component->IsWelded());

	UAutoRTFMTestBodySetup* BodySetup = NewObject<UAutoRTFMTestBodySetup>();
	BodySetup->AggGeom.SphereElems.Add(FKSphereElem(1.0f));

	Component->BodyInstance.BodySetup = BodySetup;

	UAutoRTFMTestPrimitiveComponent* Parent0 = NewObject<UAutoRTFMTestPrimitiveComponent>(Actor);
	UAutoRTFMTestPrimitiveComponent* Parent1 = NewObject<UAutoRTFMTestPrimitiveComponent>(Actor);

	AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			Component->WeldTo(Parent0);
			AutoRTFM::AbortTransaction();
		});

	TestTrueExpr(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	TestTrueExpr(Component->IsWelded());
	TestTrueExpr(&SomeInstance == Component->BodyInstance.WeldParent);

	AutoRTFM::Commit([&]
		{
			Component->WeldTo(Parent0);
		});

	TestFalseExpr(Component->IsWelded());
	TestTrueExpr(nullptr == Component->BodyInstance.WeldParent);

	Result = AutoRTFM::Transact([&]
		{
			Component->WeldTo(Parent1);
			AutoRTFM::AbortTransaction();
		});

	TestTrueExpr(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	TestFalseExpr(Component->IsWelded());

	AutoRTFM::Commit([&]
		{
			Component->WeldTo(Parent1);
		});

	TestFalseExpr(Component->IsWelded());

	Result = AutoRTFM::Transact([&]
		{
			Component->UnWeldFromParent();
			AutoRTFM::AbortTransaction();
		});

	TestTrueExpr(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	TestFalseExpr(Component->IsWelded());

	AutoRTFM::Commit([&]
		{
			Component->UnWeldFromParent();
		});

	TestFalseExpr(Component->IsWelded());
}

// Test calling Component->UnregisterComponent() on a Component with an event
// listener for OnComponentPhysicsStateChanged().
// See: SOL-6765
AUTORTFM_ACTOR_COMPONENT_TEST(FSparseDelegate)
{
	UAutoRTFMTestObject* const Object = NewObject<UAutoRTFMTestObject>();

	Component->RegisterComponent();
	Component->OnComponentPhysicsStateChanged.AddDynamic(Object, &UAutoRTFMTestObject::OnComponentPhysicsStateChanged);

	TestFalseExpr(Object->bHitOnComponentPhysicsStateChanged);

	AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			Component->UnregisterComponent();
			AutoRTFM::AbortTransaction();
		});

	TestTrueExpr(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	TestFalseExpr(Object->bHitOnComponentPhysicsStateChanged);

	AutoRTFM::Commit([&]
		{
			Component->UnregisterComponent();
		});

	TestTrueExpr(Object->bHitOnComponentPhysicsStateChanged);
}

AUTORTFM_ACTOR_COMPONENT_TEST(ChildActor)
{
	UAutoRTFMTestChildActorComponent* const ChildActorComponent = NewObject<UAutoRTFMTestChildActorComponent>(Actor);

	AAutoRTFMTestAnotherActor* const AnotherActor = NewObject<AAutoRTFMTestAnotherActor>();

	ChildActorComponent->RegisterComponentWithWorld(World);

	ChildActorComponent->ForceActorClass(AnotherActor->GetClass());

	if (nullptr != ChildActorComponent->GetChildActor())
	{
		AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
			{
				ChildActorComponent->DestroyChildActor();
				AutoRTFM::AbortTransaction();
			});

		TestTrueExpr(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
		TestTrueExpr(nullptr != ChildActorComponent->GetChildActor());

		Result = AutoRTFM::Transact([&]
			{
				ChildActorComponent->DestroyChildActor();
			});

		TestTrueExpr(AutoRTFM::ETransactionResult::Committed == Result);
		TestTrueExpr(nullptr == ChildActorComponent->GetChildActor());
	}

	AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			ChildActorComponent->CreateChildActor();
			AutoRTFM::AbortTransaction();
		});

	TestTrueExpr(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	TestTrueExpr(nullptr == ChildActorComponent->GetChildActor());

	Result = AutoRTFM::Transact([&]
		{
			ChildActorComponent->CreateChildActor();
		});

	TestTrueExpr(AutoRTFM::ETransactionResult::Committed == Result);
	TestTrueExpr(nullptr != ChildActorComponent->GetChildActor());

	Result = AutoRTFM::Transact([&]
		{
			ChildActorComponent->DestroyChildActor();
			AutoRTFM::AbortTransaction();
		});

	TestTrueExpr(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
	TestTrueExpr(nullptr != ChildActorComponent->GetChildActor());

	Result = AutoRTFM::Transact([&]
		{
			ChildActorComponent->DestroyChildActor();
		});

	TestTrueExpr(AutoRTFM::ETransactionResult::Committed == Result);
	TestTrueExpr(nullptr == ChildActorComponent->GetChildActor());
}

// Test aborting a call to USkeletalMeshComponent::RegisterComponent() with an assigned skeletal
// mesh and empty PostProcessAnimInstance.
// See: SOL-6779
AUTORTFM_ACTOR_COMPONENT_TEST(USkeletalMeshComponent)
{
	USkeleton* Skeleton = NewObject<USkeleton>();
	USkeletalMesh* SkeletalMesh = NewObject<USkeletalMesh>();
	SkeletalMesh->SetSkeleton(Skeleton);
	SkeletalMesh->AllocateResourceForRendering();
	FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
	TRefCountPtr<FSkeletalMeshLODRenderData> LODRenderData = MakeRefCount<FSkeletalMeshLODRenderData>();
	RenderData->LODRenderData.Add(LODRenderData);
	USkeletalMeshComponent* SkeletalMeshComponent = NewObject<USkeletalMeshComponent>(Actor);
	SkeletalMeshComponent->SetSkeletalMeshAsset(SkeletalMesh);
	SkeletalMeshComponent->PostProcessAnimInstance = NewObject<UAnimInstance>(SkeletalMeshComponent);

	AutoRTFM::ETransactionResult Result;
	Result = AutoRTFM::Transact([&]
		{
			SkeletalMeshComponent->RegisterComponent();
			AutoRTFM::AbortTransaction();
		});

	TestTrueExpr(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
}

// Test aborting a call to AAutoRTFMTestActor::CreateComponentFromTemplate().
// See: SOL-7002
AUTORTFM_ACTOR_COMPONENT_TEST(CreateComponentFromTemplate)
{
	AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			Actor->CreateComponentFromTemplate(Component);
			AutoRTFM::AbortTransaction();
		});

	TestTrueExpr(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
}

// Test aborting a call to UObject::GetArchetype().
// See: SOL-7024
AUTORTFM_ACTOR_COMPONENT_TEST(GetArchetype)
{
	AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			Actor->GetArchetype();
			AutoRTFM::AbortTransaction();
		});

	TestTrueExpr(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
}

// Test aborting a call to FUObjectArray::CloseDisregardForGC().
// See: SOL-7027
AUTORTFM_ACTOR_COMPONENT_TEST(CloseDisregardForGC)
{
	FUObjectArray ObjectArray;
	AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			ObjectArray.CloseDisregardForGC();
			AutoRTFM::AbortTransaction();
		});

	TestTrueExpr(AutoRTFM::ETransactionResult::AbortedByRequest == Result);
}

AUTORTFM_ACTOR_COMPONENT_TEST(WorldGetWorldSettings)
{
	AWorldSettings* Settings = nullptr;

	AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			Settings = World->GetWorldSettings();
		});

	TestTrueExpr(AutoRTFM::ETransactionResult::Committed == Result);
	TestTrueExpr(Settings != nullptr);
}

AUTORTFM_ACTOR_COMPONENT_TEST(WorldProcessLevelStreamingVolumes)
{
	AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			World->ProcessLevelStreamingVolumes();
		});

	TestTrueExpr(AutoRTFM::ETransactionResult::Committed == Result);
}

AUTORTFM_ACTOR_COMPONENT_TEST(WorldBlockTillLevelStreamingCompleted)
{
	ULevelStreamingPersistent* LevelStreamingPersistent = NewObject<ULevelStreamingPersistent>(
		World,
		TEXT("WOWWEE"));
	LevelStreamingPersistent->SetWorldAsset(World);
	World->AddStreamingLevel(LevelStreamingPersistent);

	TestTrueExpr(World->HasStreamingLevelsToConsider());

	AutoRTFM::ETransactionResult Result = AutoRTFM::Transact([&]
		{
			World->BlockTillLevelStreamingCompleted();
		});

	TestFalseExpr(World->HasStreamingLevelsToConsider());

	TestTrueExpr(AutoRTFM::ETransactionResult::Committed == Result);
}

AUTORTFM_ACTOR_COMPONENT_TEST(ReconstructActor)
{
	FName Name = TEXT("MyObjectToBeReplaced");
	UObject* Outer = static_cast<UObject*>(GetTransientPackage());
	AAutoRTFMTestActor* Old = NewObject<AAutoRTFMTestActor>(Outer, Name);
	TestTrueExpr(0 == Old->ActorCategory);
	Old->ActorCategory = 123;

	TWeakObjectPtr<AAutoRTFMTestActor> OldWeak = Old;
	TestTrueExpr(OldWeak == Old);
	
	const uint32 OldID = Old->GetUniqueID();

	// Create some additional actors to append more objects to the UObjectHash tables
	for (int I = 0; I < 5; I++)
	{
		NewObject<AAutoRTFMTestActor>();
	}
	
	AutoRTFM::Testing::Abort([&]
		{
			// Reconstruct the object by using the same name, then immediately abort.
			AAutoRTFMTestActor* New = NewObject<AAutoRTFMTestActor>(Outer, Name);
			TestTrueExpr(Old == New);
			TestTrueExpr(OldWeak == New);
			TestTrueExpr(OldID == New->GetUniqueID());
			TestTrueExpr(0 == New->ActorCategory);
			AutoRTFM::AbortTransaction();
		});

	TestTrueExpr(OldWeak == Old);
	TestTrueExpr(OldID == Old->GetUniqueID());
	TestTrueExpr(123 == Old->ActorCategory);

	// Finally reconstruct the object one more time. This exercises code that
	// can detect corrupt UObjectHash tables
	NewObject<AAutoRTFMTestActor>(Outer, Name);
}

// Reproduction for mixed open / closed writes on FUObjectItem::SerialNumber.
AUTORTFM_ACTOR_COMPONENT_TEST(ReconstructActorThenObtainWeakPtr) // SOL-7678
{
	AutoRTFM::Testing::Abort([&]
	{
		// Construct a UObject.
		// The FUObjectItem::SerialNumber begins with 0, assigned in the open
		// with validation disabled. This is open-write is slightly iffy, but
		// can be considered part of the UObject construction / reconstruction
		// logic, which has a lot of "special leniency".
		UAutoRTFMTestPrimitiveComponent* Original = NewObject<UAutoRTFMTestPrimitiveComponent>(Actor, "ReconstructedComponent");
		// Reconstruct the object.
		// This calls FUObjectArray::ResetSerialNumber() which assigns 0 to
		// FUObjectItem::SerialNumber in the closed.
		UAutoRTFMTestPrimitiveComponent* Reconstructed = NewObject<UAutoRTFMTestPrimitiveComponent>(Actor, "ReconstructedComponent");
		TestTrueExpr(Original == Reconstructed);
		// Obtain a weak pointer to the object.
		// This calls into FUObjectArray::AllocateSerialNumber() which assigns a
		// new number to FUObjectItem::SerialNumber in the open.
		// The validator would catch the closed write followed by the open
		// write. The applied fix for this is to disable validation on this
		// open, and to explicitly record the write. To the memory validator
		// this behaves like a regular closed write, despite the logic using
		// atomic CAS which is not permitted in the closed.
		FWeakObjectPtr WeakReconstructed = Reconstructed;
		AutoRTFM::AbortTransaction();
	});
}

AUTORTFM_ACTOR_COMPONENT_TEST(ReconstructCameraShake) // SOL-7529
{
	FName Name = TEXT("ShakyMcShakeface");
	UObject* Outer = static_cast<UObject*>(GetTransientPackage());
	UAutoRTFMTestCameraShake* Old = NewObject<UAutoRTFMTestCameraShake>(Outer, Name);
	TestTrueExpr(1.0 == Old->ShakeScale);
	Old->ShakeScale = 123;

	UAutoRTFMTestCameraShake* New = nullptr;

	AutoRTFM::Testing::Abort([&]
		{
			// Reconstruct the object by using the same name, then immediately abort.
			New = NewObject<UAutoRTFMTestCameraShake>(Outer, Name);
			AutoRTFM::AbortTransaction();
		});

	TestTrueExpr(nullptr == New);
	TestTrueExpr(123 == Old->ShakeScale);
}

AUTORTFM_ACTOR_COMPONENT_TEST(ForEachObjectOfClass_Fresh)
{
	const size_t InitialCount = CountAAutoRTFMTestActors();

	TestTrueExpr(CountAAutoRTFMTestActors() == InitialCount);

	AutoRTFM::Testing::Abort([&]
	{
		AAutoRTFMTestActor* Object = NewObject<AAutoRTFMTestActor>();
		TestTrueExpr(CountAAutoRTFMTestActors() == InitialCount + 1);
		AutoRTFM::AbortTransaction();
	});

	TestTrueExpr(CountAAutoRTFMTestActors() == InitialCount);
}

AUTORTFM_ACTOR_COMPONENT_TEST(ForEachObjectOfClass_Reconstructed)
{
	const size_t InitialCount = CountAAutoRTFMTestActors();
	
	TestTrueExpr(CountAAutoRTFMTestActors() == InitialCount);

	AutoRTFM::Testing::Commit([&]
	{
		FName Name = TEXT("MyObject");
		UObject* Outer = static_cast<UObject*>(GetTransientPackage());
		AAutoRTFMTestActor* Old = NewObject<AAutoRTFMTestActor>(Outer, Name);
		TestTrueExpr(CountAAutoRTFMTestActors() == InitialCount + 1u);

		AutoRTFM::Testing::Abort([&]
		{
			AAutoRTFMTestActor* New = NewObject<AAutoRTFMTestActor>(Outer, Name);
			TestTrueExpr(Old == New);
			TestTrueExpr(CountAAutoRTFMTestActors() == InitialCount + 1u);
			AutoRTFM::AbortTransaction();
		});

		TestTrueExpr(CountAAutoRTFMTestActors() == InitialCount + 1u);
	});

	TestTrueExpr(CountAAutoRTFMTestActors() == InitialCount + 1u);
}

AUTORTFM_ACTOR_COMPONENT_TEST(LightWeightInstanceSubsystemTest)
{
	AutoRTFM::Testing::Commit([&]
	{
		FLightWeightInstanceSubsystem& LWI = FLightWeightInstanceSubsystem::Get();

		FLWIData InitData = {};
		FActorInstanceHandle Handle = LWI.CreateNewLightWeightInstance(Actor->GetClass(), &InitData, nullptr, World);

		LWI.DeleteInstance(Handle);
	});
}

}  // anonymous namespace

#undef AUTORTFM_ACTOR_COMPONENT_TEST

#endif //WITH_DEV_AUTOMATION_TESTS

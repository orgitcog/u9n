// Copyright Epic Games, Inc. All Rights Reserved.

#include "VerseVM/VVMVerse.h"
#include "AutoRTFM.h"
#include "UObject/VerseValueProperty.h"
#include "VerseVM/VVMEmergentTypeCreator.h"
#include "VerseVM/VVMFalse.h"
#include "VerseVM/VVMFrame.h"
#include "VerseVM/VVMGlobalProgram.h"
#include "VerseVM/VVMHeap.h"
#include "VerseVM/VVMIntrinsics.h"
#include "VerseVM/VVMTask.h"
#include "VerseVM/VVMVerseClass.h"
#include "VerseVM/VVMVerseEnum.h"
#include "VerseVM/VVMVerseStruct.h"

namespace Verse
{

namespace Private
{
IEngineEnvironment* GEngineEnvironment = nullptr;
}

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
void VerseVM::Startup()
{
	FHeap::Initialize();
	FRunningContext Context = FRunningContextPromise{};

	VCell::InitializeGlobals(Context);

	VEmergentTypeCreator::Initialize(Context);
	VFalse::InitializeGlobals(Context);

	VVoidType::Initialize(Context);
	VAnyType::Initialize(Context);
	VComparableType::Initialize(Context);
	VLogicType::Initialize(Context);
	VRationalType::Initialize(Context);
	VChar8Type::Initialize(Context);
	VChar32Type::Initialize(Context);
	VRangeType::Initialize(Context);
	VReferenceType::Initialize(Context);
	VFunctionType::Initialize(Context);
	VPersistableType::Initialize(Context);

	VFrame::InitializeGlobals(Context);
	VTask::InitializeGlobals(Context);
	VTask::BindStructTrivial(Context);

	// VerseVM requires RTFM enabled
#if UE_AUTORTFM || defined(__INTELLISENSE__)
	AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime(AutoRTFM::ForTheRuntime::EAutoRTFMEnabledState::AutoRTFM_Enabled);
#endif

	// Register our property types
	FVCellProperty::StaticClass();
	FVValueProperty::StaticClass();
	FVRestValueProperty::StaticClass();

	if (!GlobalProgram)
	{
		GlobalProgram.Set(Context, &VProgram::New(Context, 32));
		VIntrinsics::Initialize(Context);
	}
}

void VerseVM::Shutdown()
{
	FHeap::Deinitialize();
}
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)

IEngineEnvironment* VerseVM::GetEngineEnvironment()
{
	return Private::GEngineEnvironment;
}

void VerseVM::SetEngineEnvironment(IEngineEnvironment* Environment)
{
	ensure(Environment == nullptr || Private::GEngineEnvironment == nullptr);
	Private::GEngineEnvironment = Environment;
}

bool VerseVM::IsUHTGeneratedVerseVNIObject(UObject* Object)
{
	if (Object == nullptr)
	{
		return false;
	}
	else if (UVerseClass* VerseClass = Cast<UVerseClass>(Object))
	{
		return VerseClass->IsUHTNative();
	}
	else if (UVerseStruct* VerseStruct = Cast<UVerseStruct>(Object))
	{
		return VerseStruct->IsUHTNative();
	}
	else if (UVerseEnum* VerseEnum = Cast<UVerseEnum>(Object))
	{
		return VerseEnum->IsUHTNative();
	}
	else
	{
		return false;
	}
}

} // namespace Verse

// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMIntrinsics.h"

#include "Serialization/AsyncLoadingEvents.h"
#include "VerseVM/Inline/VVMIntInline.h"
#include "VerseVM/Inline/VVMMapInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMFloat.h"
#include "VerseVM/VVMGlobalProgram.h"
#include "VerseVM/VVMInt.h"
#include "VerseVM/VVMMap.h"
#include "VerseVM/VVMOpResult.h"
#include "VerseVM/VVMPackage.h"
#include "VerseVM/VVMRational.h"
#include "VerseVM/VVMUniqueString.h"
#include "VerseVM/VVMValue.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VIntrinsics);
TGlobalTrivialEmergentTypePtr<&VIntrinsics::StaticCppClassInfo> VIntrinsics::GlobalTrivialEmergentType;

void VIntrinsics::Initialize(FAllocationContext Context)
{
	// This VPackage enables cooked data to import intrinsics by Verse path.
	FUtf8StringView VersePackageName = "$BuiltIn";
	FUtf8StringView RootPath = "/Verse.org/Verse";
	VPackage& BuiltInPackage = VPackage::New(Context, VUniqueString::New(Context, VersePackageName), VUniqueString::New(Context, RootPath), 4);
	GlobalProgram->AddPackage(Context, BuiltInPackage.GetName(), BuiltInPackage, false);

	// This only affects the cooker.
	// Disabling it elsewhere enables VM tests to assume there are no UObject references.
	if (IsRunningCookCommandlet())
	{
		BuiltInPackage.AssociatedUPackage.Set(Context, VValue(FindPackage(nullptr, TEXT("/Script/CoreUObject"))));
	}

	FPackageScope PackageScope = Context.SetCurrentPackage(&BuiltInPackage);
	BuiltInPackage.RecordCells(Context);

	GlobalProgram->AddIntrinsics(Context, VIntrinsics::New(Context, BuiltInPackage));

	NotifyScriptVersePackage(&BuiltInPackage);
}

VIntrinsics::VIntrinsics(FAllocationContext Context, VPackage& BuiltInPackage)
	: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
{
	Abs.Set(Context, VNativeFunction::New(Context, 1, &AbsImpl, VUniqueString::New(Context, "(/Verse.org/Verse:)Abs"), GlobalFalse()));
	BuiltInPackage.AddDefinition(Context, VUniqueString::New(Context, "(/Verse.org/Verse/(/Verse.org/Verse:)Abs:)Native"), *Abs);

	Ceil.Set(Context, VNativeFunction::New(Context, 1, &CeilImpl, VUniqueString::New(Context, "(/Verse.org/Verse:)Ceil"), GlobalFalse()));
	BuiltInPackage.AddDefinition(Context, VUniqueString::New(Context, "(/Verse.org/Verse/(/Verse.org/Verse:)Ceil:)Native"), *Ceil);

	Floor.Set(Context, VNativeFunction::New(Context, 1, &FloorImpl, VUniqueString::New(Context, "(/Verse.org/Verse:)Floor"), GlobalFalse()));
	BuiltInPackage.AddDefinition(Context, VUniqueString::New(Context, "(/Verse.org/Verse/(/Verse.org/Verse:)Floor:)Native"), *Floor);

	ConcatenateMaps.Set(Context, VNativeFunction::New(Context, 2, &ConcatenateMapsImpl, VUniqueString::New(Context, "(/Verse.org/Verse:)ConcatenateMaps"), GlobalFalse()));
	BuiltInPackage.AddDefinition(Context, VUniqueString::New(Context, "(/Verse.org/Verse/(/Verse.org/Verse:)ConcatenateMaps:)Native"), *ConcatenateMaps);
}

FNativeCallResult VIntrinsics::AbsImpl(FRunningContext Context, VValue Scope, VNativeFunction::Args Arguments)
{
	checkSlow(Arguments.Num() == 1); // The interpreter already checks this
	V_REQUIRE_CONCRETE(Arguments[0]);
	V_RETURN(Arguments[0].IsFloat()
				 ? VValue(VFloat(FMath::Abs(Arguments[0].AsFloat().AsDouble())))
				 : VValue(VInt::Abs(Context, VInt(Arguments[0]))));
}

FNativeCallResult VIntrinsics::CeilImpl(FRunningContext Context, VValue Scope, VNativeFunction::Args Arguments)
{
	checkSlow(Arguments.Num() == 1); // The interpreter already checks this
	V_REQUIRE_CONCRETE(Arguments[0]);
	VRational& Argument = Arguments[0].StaticCast<VRational>();
	V_RETURN(Argument.Ceil(Context));
}

FNativeCallResult VIntrinsics::FloorImpl(FRunningContext Context, VValue Scope, VNativeFunction::Args Arguments)
{
	checkSlow(Arguments.Num() == 1); // The interpreter already checks this
	V_REQUIRE_CONCRETE(Arguments[0]);
	VRational& Argument = Arguments[0].StaticCast<VRational>();
	V_RETURN(Argument.Floor(Context));
}

FNativeCallResult VIntrinsics::ConcatenateMapsImpl(FRunningContext Context, VValue Scope, VNativeFunction::Args Arguments)
{
	checkSlow(Arguments.Num() == 2); // The interpreter already checks this
	V_REQUIRE_CONCRETE(Arguments[0]);
	V_REQUIRE_CONCRETE(Arguments[1]);
	VMap& Lhs = Arguments[0].StaticCast<VMap>();
	VMap& Rhs = Arguments[1].StaticCast<VMap>();
	V_RETURN(VMapBase::New<VMap>(Context, Lhs.Num() + Rhs.Num(), [&](int32 I) {
		if (I < Lhs.Num())
		{
			return TPair<VValue, VValue>{Lhs.GetKey(I), Lhs.GetValue(I)};
		}
		checkSlow(I >= Lhs.Num());
		return TPair<VValue, VValue>{Rhs.GetKey(I - Lhs.Num()), Rhs.GetValue(I - Lhs.Num())};
	}));
}

template <typename TVisitor>
void VIntrinsics::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Abs, TEXT("Abs"));
	Visitor.Visit(Ceil, TEXT("Ceil"));
	Visitor.Visit(Floor, TEXT("Floor"));
	Visitor.Visit(ConcatenateMaps, TEXT("ConcatenateMaps"));
}

} // namespace Verse
#endif

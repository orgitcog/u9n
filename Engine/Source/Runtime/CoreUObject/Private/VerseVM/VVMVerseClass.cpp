// Copyright Epic Games, Inc. All Rights Reserved.

#include "VerseVM/VVMVerseClass.h"
#include "AutoRTFM.h"
#include "Containers/VersePath.h"
#include "HAL/IConsoleManager.h"
#include "Logging/LogMacros.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/CoreRedirects.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/PropertyOptional.h"
#include "UObject/UObjectThreadContext.h"
#include "UObject/UnrealType.h"
#include "VerseVM/VVMCVars.h"
#include "VerseVM/VVMEngineEnvironment.h"
#include "VerseVM/VVMNames.h"
#include "VerseVM/VVMPackageName.h"
#include "VerseVM/VVMVerse.h"
#include "VerseVM/VVMVerseFunction.h"
#include "VerseVM/VVMVerseStruct.h"

#if WITH_EDITOR
#include "Interfaces/ITargetPlatform.h"
#include "UObject/CookedMetaData.h"
#include "UObject/PropertyBagRepository.h"
#endif

#if WITH_VERSE_BPVM
#include "VerseVM/VBPVMDynamicProperty.h"
#endif

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "UObject/VerseValueProperty.h"
#include "VerseVM/Inline/VVMEnterVMInline.h"
#include "VerseVM/Inline/VVMNativeConstructorWrapperInline.h"
#include "VerseVM/VVMFunction.h"
#endif

#if WITH_EDITORONLY_DATA
#include "UObject/PropertyStateTracking.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(VVMVerseClass)

DEFINE_LOG_CATEGORY_STATIC(LogSolGeneratedClass, Log, All);

bool CVar_UseAuthoredNameNonEditor = true;
static FAutoConsoleVariableRef CVarUseAuthoredNameNonEditor(TEXT("Verse.UseAuthoredNameNonEditor"), CVar_UseAuthoredNameNonEditor, TEXT(""));

const FName UVerseClass::NativeParentClassTagName("NativeParentClass");
const FName UVerseClass::PackageVersePathTagName("PackageVersePath");
const FName UVerseClass::PackageRelativeVersePathTagName("PackageRelativeVersePath");
const FName UVerseClass::InitCDOFunctionName(TEXT("$InitCDO"));
const FName UVerseClass::StructPaddingDummyName(TEXT("$StructPaddingDummy"));
UVerseClass::FOnPropertyRemoved UVerseClass::OnPropertyRemoved;

UVerseClass::UVerseClass(
	EStaticConstructor,
	FName InName,
	uint32 InSize,
	uint32 InAlignment,
	EClassFlags InClassFlags,
	EClassCastFlags InClassCastFlags,
	const TCHAR* InClassConfigName,
	EObjectFlags InFlags,
	ClassConstructorType InClassConstructor,
	ClassVTableHelperCtorCallerType InClassVTableHelperCtorCaller,
	FUObjectCppClassStaticFunctions&& InCppClassStaticFunctions)
	: Super(
		EC_StaticConstructor,
		InName,
		InSize,
		InAlignment,
		InClassFlags,
		InClassCastFlags,
		InClassConfigName,
		InFlags,
		InClassConstructor,
		InClassVTableHelperCtorCaller,
		MoveTemp(InCppClassStaticFunctions))
{
}

UVerseClass::UVerseClass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UE::Core::FVersePath UVerseClass::GetVersePath() const
{
	if (MangledPackageVersePath.IsNone())
	{
		return {};
	}

	FString PackageVersePath = Verse::Names::Private::UnmangleCasedName(MangledPackageVersePath);
	FString VersePath = PackageRelativeVersePath.IsEmpty() ? PackageVersePath : PackageVersePath / PackageRelativeVersePath;
	UE::Core::FVersePath Result;
	ensure(UE::Core::FVersePath::TryMake(Result, MoveTemp(VersePath)));
	return Result;
}

void UVerseClass::Link(FArchive& Ar, bool bRelinkExistingProperties)
{
	Super::Link(Ar, bRelinkExistingProperties);

// TODO: Move this to compiled package registry.  See https://jira.it.epicgames.com/browse/SOL-7734.
#if WITH_SERVER_CODE
	UPackage* Package = GetPackage();
	EVersePackageType PackageType;
	(void)Verse::FPackageName::GetVersePackageNameFromUPackagePath(Package->GetFName(), &PackageType);
	if (PackageType != EVersePackageType::VNI)
	{
		TArray<FCoreRedirect> Redirects;

		const FString& Name = GetName();

		FString OldName{Name};
		OldName.ReplaceCharInline('-', '_', ESearchCase::CaseSensitive);

		int32 Index;
		FString OldShortName = Name.FindLastChar('-', Index) ? Name.RightChop(Index + 1) : Name;

		TStringBuilder<Verse::Names::DefaultNameLength> OldPackageName(InPlace, Package->GetName(), '/', OldName);
		TStringBuilder<Verse::Names::DefaultNameLength> PackageName(InPlace, Package->GetName());
		TStringBuilder<Verse::Names::DefaultNameLength> OldFullName(InPlace, OldPackageName, '.', OldShortName);
		TStringBuilder<Verse::Names::DefaultNameLength> FullName(InPlace, PackageName, '.', Name);
		Redirects.Emplace(ECoreRedirectFlags::Type_Class, OldFullName.ToString(), FullName.ToString());

		FCoreRedirects::AddRedirectList(Redirects, FullName.ToString());
	}
#endif

	// Properties which represent native C++ members need to be removed from the
	// destruct chain, as they will be destructed by the native C++ destructor.
	bool bPropertiesChanged = false;

	UEProperty_Private::FPropertyListBuilderDestructorLink DestructorLinkBuilder(&DestructorLink);
	for (FProperty* Prop = DestructorLinkBuilder.GetListStart(); Prop;)
	{
		FProperty* NextProp = DestructorLinkBuilder.GetNext(*Prop);

		const UVerseClass* SolOwnerClass = Cast<UVerseClass>(Prop->GetOwnerClass());
		if (SolOwnerClass && (SolOwnerClass->SolClassFlags & VCLASS_NativeBound) != EVerseClassFlags::VCLASS_None)
		{
			// property should be removed from linked list
			DestructorLinkBuilder.Remove(*Prop);
			bPropertiesChanged = true;
		}

		Prop = NextProp;
	}

	// Only do this for classes we're loading from disk/file -- in-memory generated ones
	// have these functions executed for them via Verse::FUObjectGenerator or FVerseVMAssembler
	if (HasAnyFlags(RF_WasLoaded))
	{
#if WITH_VERSE_BPVM
		// Make sure coroutine task classes have been loaded at this point
		if (!IsEventDrivenLoaderEnabled())
		{
			for (UVerseClass* TaskClass : TaskClasses)
			{
				if (TaskClass)
				{
					Ar.Preload(TaskClass);
				}
			}
		}
#endif

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
		Verse::FAllocationContext Context = Verse::FAllocationContextPromise{};
		Class->CreateShapeForExistingUStruct(Context);
#endif

		// For native classes, we need to bind them explicitly here -- we need to do it
		// after Super::Link() (so it can find named properties/functions), but before
		// CDO creation (since binding can affect property offsets and class size).
		if ((SolClassFlags & VCLASS_NativeBound) != EVerseClassFlags::VCLASS_None)
		{
			Verse::IEngineEnvironment* Environment = Verse::VerseVM::GetEngineEnvironment();
			ensure(Environment);
#if WITH_VERSE_BPVM
			Environment->TryBindVniType(this);
#endif
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
			Environment->TryBindVniType(&Class->GetPackage(), this);
#endif
		}

#if WITH_VERSE_BPVM
		// Connect native function thunks of loaded classes
		for (const FNativeFunctionLookup& NativeFunctionLookup : NativeFunctionLookupTable)
		{
			UFunction* Function = FindFunctionByName(NativeFunctionLookup.Name);
			if (ensureMsgf(Function, TEXT("The function: %s could not be found, even though it should have been available!"), *NativeFunctionLookup.Name.ToString()))
			{
				Function->SetNativeFunc(NativeFunctionLookup.Pointer);
				Function->FunctionFlags |= FUNC_Native;
			}
		}
#endif
	}

#if WITH_VERSE_BPVM
	// Manually build token stream for Solaris classes but only when linking cooked classes or
	// when linking a duplicated class during class reinstancing.
	// However, when classes are first created (from script source) this happens in
	// FAssembleClassOrStructTask as we want to make sure all dependencies are properly set up first
	if (HasAnyFlags(RF_WasLoaded) || HasAnyClassFlags(CLASS_NewerVersionExists))
	{
		AssembleReferenceTokenStream(bPropertiesChanged || bRelinkExistingProperties);
	}
#endif

	// If a class is compiled w/ support for dynamic references, but dynamic subobject instancing is disabled
	// for Verse types at runtime, fall back to forcing explicit instancing flags on all reference properties.
	// This makes it possible to patch dynamic instancing off at link time to avoid re-cooking engine content.
	if (HasAnyFlags(RF_WasLoaded))
	{
		const bool bHasDynamicInstancedReferenceSupport = SupportsDynamicInstancedReference();
		const bool bIsDynamicSubobjectInstancingEnabled = Verse::CVarUseDynamicSubobjectInstancing.GetValueOnAnyThread();
		if (bHasDynamicInstancedReferenceSupport
			&& !bIsDynamicSubobjectInstancingEnabled)
		{
			DisableDynamicInstancedReferenceSupport();
		}
#if WITH_EDITOR
		else
		{
			// In this case, dynamic subobject instancing is enabled, but the (cooked) class may have been packaged
			// with it disabled. We enable support in this case since it is an inheritable class trait (e.g. prefabs).
			// Note: This is restricted to the editor context, because we only need it to support testing/iteration,
			// where we might be running the editor against engine data (e.g. VNI types) cooked w/ the CVar turned off.
			if (!bHasDynamicInstancedReferenceSupport
				&& bIsDynamicSubobjectInstancingEnabled)
			{
				EnableDynamicInstancedReferenceSupport();
			}
		}
#endif
	}
}

void UVerseClass::PreloadChildren(FArchive& Ar)
{
#if WITH_VERSE_BPVM
	// Preloading functions for UVerseClass may end up with circular dependencies regardless of EDL being enabled or not
	// Since UVerseClass is not a UBlueprintGeneratedClass it does not use the deferred dependency loading path in FLinkerLoad
	// so we don't want to deal with circular dependencies here. They will be resolved by the linker eventually though.
	for (UField* Field = Children; Field; Field = Field->Next)
	{
		if (!Cast<UFunction>(Field))
		{
			Ar.Preload(Field);
		}
	}
#endif
}

FProperty* UVerseClass::CustomFindProperty(const FName InName) const
{
	OnPropertyRemoved.Broadcast(this, InName);
	return nullptr;
}

FString UVerseClass::GetAuthoredNameForField(const FField* Field) const
{
	if (Field)
	{
#if WITH_EDITORONLY_DATA
		static const FName NAME_DisplayName(TEXT("DisplayName"));
		if (const FString* NativeDisplayName = Field->FindMetaData(NAME_DisplayName))
		{
			return *NativeDisplayName;
		}
#else
		if (CVar_UseAuthoredNameNonEditor)
		{
			return Verse::Names::UEPropToVerseName(Field->GetName());
		}
#endif
	}

	return Super::GetAuthoredNameForField(Field);
}

bool UVerseClass::SupportsDynamicInstancedReference() const
{
	// This trait is set if the class is using explicit instanced reference semantics on its
	// generated object properties. Note that this differs from 'CLASS_HasInstancedReference'
	// which is used by engine code to signal the class may reference an instanced subobject.
	//
	// If this type was generated using explicit instanced reference semantics, disallow dynamic
	// subobject instancing at runtime to ensure backwards compatibility with legacy script code.
	return !HasInstancedSemantics();
}

bool UVerseClass::IsAsset() const
{
#if WITH_EDITOR
	// Don't include placeholder types that were created for missing type imports on load.
	// These allow exports to be serialized to avoid data loss, but should not be an asset.
	if (UE::FPropertyBagRepository::IsPropertyBagPlaceholderType(this))
	{
		return false;
	}
#endif

	return true;
}

void UVerseClass::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	Super::GetPreloadDependencies(OutDeps);

	// UClass::Serialize() will instantiate this class's CDO, but that means we need the
	// Super's CDO serialized before this class serializes
	OutDeps.Add(GetSuperClass()->GetDefaultObject());

	// For natively-bound classes, we need their coroutine objects serialized first,
	// because we bind on Link() (called during Serialize()) and native binding
	// for a class will binds its coroutine task objects at the same time.
	if ((SolClassFlags & VCLASS_NativeBound) != EVerseClassFlags::VCLASS_None)
	{
		for (UVerseClass* TaskClass : TaskClasses)
		{
			OutDeps.Add(TaskClass);
		}
	}
}

void UVerseClass::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

#if WITH_EDITOR
	// NativeParentClass
	{
		FString NativeParentClassName;
		if (UClass* ParentClass = GetSuperClass())
		{
			// Walk up until we find a native class
			UClass* NativeParentClass = ParentClass;
			while (!NativeParentClass->HasAnyClassFlags(CLASS_Native | CLASS_Intrinsic))
			{
				NativeParentClass = NativeParentClass->GetSuperClass();
			}
			NativeParentClassName = FObjectPropertyBase::GetExportPath(NativeParentClass);
		}
		else
		{
			NativeParentClassName = TEXT("None");
		}

		Context.AddTag(FAssetRegistryTag(NativeParentClassTagName, MoveTemp(NativeParentClassName), FAssetRegistryTag::TT_Alphabetical));
	}
	// PackageVersePath
	if (!MangledPackageVersePath.IsNone())
	{
		Context.AddTag(FAssetRegistryTag(PackageVersePathTagName, Verse::Names::Private::UnmangleCasedName(MangledPackageVersePath), FAssetRegistryTag::TT_Alphabetical));
	}
	// PackageRelativeVersePath
	{
		Context.AddTag(FAssetRegistryTag(PackageRelativeVersePathTagName, PackageRelativeVersePath, FAssetRegistryTag::TT_Alphabetical));
	}
#endif
}

static bool NeedsPostLoad(UObject* InObj)
{
	return InObj->HasAnyFlags(RF_NeedPostLoad);
}

static bool NeedsInit(UObject* InObj)
{
	if (NeedsPostLoad(InObj))
	{
		return false;
	}
	if (InObj->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		if (NeedsPostLoad(InObj->GetClass()))
		{
			return false;
		}
	}
	return true;
}

void UVerseClass::PostInitInstance(UObject* InObj, FObjectInstancingGraph* InstanceGraph)
{
	Super::PostInitInstance(InObj, InstanceGraph);

	if (NeedsInit(InObj))
	{
		// #jira SOL-6303: What should we do with a failing transaction?
		AutoRTFM::Transact([this, InObj, InstanceGraph] {
			CallInitInstanceFunctions(InObj, InstanceGraph);
		});

		AddSessionVars(InObj);
	}

	AddPersistentVars(InObj);
}

void UVerseClass::PostLoadInstance(UObject* InObj)
{
	Super::PostLoadInstance(InObj);

	if (bNeedsSubobjectInstancingForLoadedInstances && RefLink && !InObj->HasAnyFlags(RF_ClassDefaultObject))
	{
		InstanceNewSubobjects(InObj);
	}

	// For VerseVM: The loaded object should already contain everything it needs
	// and additionally calling the constructor should not be necessary
#if WITH_VERSE_BPVM
	// #jira SOL-6303: What should we do with a failing transaction?
	AutoRTFM::Transact([this, InObj] {
		CallInitInstanceFunctions(InObj, nullptr);
	});
#endif

	AddSessionVars(InObj);
}

namespace VerseClassPrivate
{
enum ETraverseSubobjectsFlag : uint32
{
	None = 0,
	NoNameGeneration = (1 << 0)
};

void TraverseSubobjectsInternal(UObject* InObject, void* ContainerPtr, FProperty* RefProperty, const FString& Prefix, const TFunctionRef<void(UObject* /*Subobject*/, const FString& /*CanonicalSubobjectName*/)> Operation, ETraverseSubobjectsFlag Flags);
} // namespace VerseClassPrivate

#if WITH_EDITORONLY_DATA
bool UVerseClass::CanCreateInstanceDataObject() const
{
	return true;
}

void UVerseClass::SerializeDefaultObject(UObject* Object, FStructuredArchive::FSlot Slot)
{
	Super::SerializeDefaultObject(Object, Slot);

	TrackDefaultInitializedProperties(Object);
}

static void TrackDefaultInitializedPropertiesInSubobject(UObject* Subobject, const UObject* CDO)
{
	// Keep track of visited property-owner pairs to avoid referencing cycles.
	TSet<TTuple<const FProperty*, void*>> VisitedPropOwners;

	Subobject->GetClass()->Visit(Subobject, [&VisitedPropOwners, &CDO](const FPropertyVisitorContext& Context) -> EPropertyVisitorControlFlow {
		const FPropertyVisitorPath& PropertyPath = Context.Path;
		const FPropertyVisitorData& Data = Context.Data;
		const FProperty* Property = PropertyPath.Top().Property;
		void* Owner = Data.ParentStructData;
		TTuple<const FProperty*, void*> PropOwner(Property, Owner);

		if (!Property || VisitedPropOwners.Contains(PropOwner))
		{
			return EPropertyVisitorControlFlow::StepOver;
		}

		bool bIsInCDO = true;

		const UStruct* OwnerType = PropertyPath.Top().ParentStructType;

		if (OwnerType && OwnerType->IsChildOf<UObject>())
		{
			const UObject* OwnerObject = (const UObject*)Owner;

			if (OwnerObject)
			{
				bIsInCDO = OwnerObject->IsInOuter(CDO);
			}
		}

		// It is possible for the property and owner types to differ during re-instancing
		// when a new CDO is created. Skip tracking in this case.
		if (bIsInCDO && OwnerType && Property->HasAnyPropertyFlags(CPF_RequiredParm) && OwnerType->IsChildOf(Property->GetOwnerStruct()))
		{
			UE::FInitializedPropertyValueState(OwnerType, Owner).Set(Property);
		}

		VisitedPropOwners.Add(PropOwner);

		if (bIsInCDO)
		{
			return EPropertyVisitorControlFlow::StepInto;
		}
		else
		{
			return EPropertyVisitorControlFlow::StepOver;
		}
	});
}

void UVerseClass::TrackDefaultInitializedProperties(void* DefaultData) const
{
	check(DefaultData);

	if (HasAnyClassFlags(CLASS_NewerVersionExists))
	{
		return;
	}

	UObject* CDO = IsChildOf<UObject>() ? (UObject*)DefaultData : nullptr;

	if (!CDO || !CDO->IsTemplate())
	{
		return;
	}

	// PropertiesWrittenByInitCDO will not contain the properties initialized in the super-class so
	// we need to traverse the class hierarchy upwards until we no longer have a Verse class.
	const UClass* SuperClass = GetSuperClass();
	if (SuperClass)
	{
		if (const UVerseClass* VerseSuperClass = Cast<UVerseClass>(SuperClass))
		{
			VerseSuperClass->TrackDefaultInitializedProperties(CDO);
		}
	}

	for (const TFieldPath<FProperty>& FieldPath : PropertiesWrittenByInitCDO)
	{
		FProperty* Property = FieldPath.Get();

		if (Property->HasAnyPropertyFlags(CPF_RequiredParm))
		{
			UE::FInitializedPropertyValueState(CDO).Set(Property);
		}

		// Recursively mark every sub-object in the property as initialized.
		VerseClassPrivate::TraverseSubobjectsInternal(
			CDO, CDO, Property, /*Prefix*/ FString(), [CDO](UObject* Subobject, const FString& CanonicalSubobjectName) {
				TrackDefaultInitializedPropertiesInSubobject(Subobject, CDO);
			},
			VerseClassPrivate::ETraverseSubobjectsFlag::NoNameGeneration);
	}
}

#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
FTopLevelAssetPath UVerseClass::GetReinstancedClassPathName_Impl() const
{
#if WITH_VERSE_COMPILER
	return FTopLevelAssetPath(PreviousPathName);
#else
	return nullptr;
#endif
}
#endif

const TCHAR* UVerseClass::GetPrefixCPP() const
{
	return TEXT("");
}

void UVerseClass::AddPersistentVars(UObject* InObj)
{
	// UHT generated types will need to be constructed prior to the engine environment.  So only call if we have these vars
	if (!PersistentVars.IsEmpty())
	{
		Verse::IEngineEnvironment* Environment = Verse::VerseVM::GetEngineEnvironment();
		ensure(Environment);
		Environment->AddPersistentVars(InObj, PersistentVars);
	}
}

void UVerseClass::AddSessionVars(UObject* InObj)
{
	// UHT generated types will need to be constructed prior to the engine environment.  So only call if we have these vars
	if (!SessionVars.IsEmpty())
	{
		Verse::IEngineEnvironment* Environment = Verse::VerseVM::GetEngineEnvironment();
		ensure(Environment);
		Environment->AddSessionVars(InObj, SessionVars);
	}
}

void UVerseClass::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Super::PreSave(ObjectSaveContext);

#if WITH_EDITOR
	// Hack: if cooking for clients, clear the InitInstanceFunction to make sure clients don't try to run it.
	if (ObjectSaveContext.IsCooking()
		&& ensure(ObjectSaveContext.GetTargetPlatform())
		&& !ObjectSaveContext.GetTargetPlatform()->IsServerOnly())
	{
		InitInstanceFunction = nullptr;
	}

	// Note: We do this in PreSave rather than PreSaveRoot since Verse stores multiple generated types in the same package, and PreSaveRoot is only called for the main "asset" within each package
	if (ObjectSaveContext.IsCooking() && (ObjectSaveContext.GetSaveFlags() & SAVE_Optional))
	{
		if (!CachedCookedMetaDataPtr)
		{
			CachedCookedMetaDataPtr = CookedMetaDataUtil::NewCookedMetaData<UClassCookedMetaData>(this, "CookedClassMetaData");
		}

		CachedCookedMetaDataPtr->CacheMetaData(this);

		if (!CachedCookedMetaDataPtr->HasMetaData())
		{
			CookedMetaDataUtil::PurgeCookedMetaData<UClassCookedMetaData>(CachedCookedMetaDataPtr);
		}
	}
	else if (CachedCookedMetaDataPtr)
	{
		CookedMetaDataUtil::PurgeCookedMetaData<UClassCookedMetaData>(CachedCookedMetaDataPtr);
	}
#endif
}

void UVerseClass::CallInitInstanceFunctions(UObject* InObj, FObjectInstancingGraph* InstanceGraph)
{
#if WITH_EDITOR
	InObj->SetFlags(RF_Transactional);
#endif

	if (InObj->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeingRegenerated))
	{
		// The construction of the CDO should not invoke class blocks.
		// RF_BeingRegenerated being set means that this object is constructed via the interpreter loop which will take care of initialization itself.
		return;
	}
	if (InstanceGraph && InObj == InstanceGraph->GetDestinationRoot())
	{
		// The root's class blocks will be invoked by the archetype instantiation.
		return;
	}

	if (GIsClient && !GIsEditor && !WITH_VERSE_COMPILER)
	{
		// SOL-4610: Don't run the InitInstance function on clients.
		return;
	}

#if WITH_VERSE_BPVM
	if (InitInstanceFunction)
	{
		// Make sure the function has been loaded and PostLoaded
		checkf(!InitInstanceFunction->HasAnyFlags(RF_NeedLoad), TEXT("Trying to call \"%s\" on \"%s\" but the function has not yet been loaded."), *InitInstanceFunction->GetPathName(), *InObj->GetFullName());
		InitInstanceFunction->ConditionalPostLoad();

		// DANGER ZONE: We're allowing VM code to potentially run during post load so fingers crossed it has no side effects
		TGuardValue<bool> GuardIsRoutingPostLoad(FUObjectThreadContext::Get().IsRoutingPostLoad, false);
		InObj->ProcessEvent(InitInstanceFunction, nullptr);
	}

	CallPropertyInitInstanceFunctions(InObj, InstanceGraph);
#else
	// DANGER ZONE: We're allowing VM code to potentially run during post load so fingers crossed it has no side effects
	Verse::FRunningContext Context = Verse::FRunningContextPromise{};
	Verse::FOpResult OpResult{Verse::FOpResult::Error};
	if (Class)
	{
		AutoRTFM::Open([&] {
			Context.EnterVM([&] {
				Verse::VNativeConstructorWrapper& WrappedObject = Verse::VNativeConstructorWrapper::New(Context, *InObj);
				OpResult = Class->GetConstructor().InvokeWithSelf(Context, Verse::VValue(WrappedObject), {/*SkipSupers = */ {}, /*SkipBlocks = */ {}});
			});
		});
		ensure(OpResult.IsReturn());
	}
	else
	{
		// Only instances of UClasses are allowed to not have a VClass
		ensure(InObj->IsA<UClass>());
	}
#endif
}

void UVerseClass::CallPropertyInitInstanceFunctions(UObject* InObj, FObjectInstancingGraph* InstanceGraph)
{
	checkf(!GIsClient || GIsEditor || WITH_VERSE_COMPILER, TEXT("SOL-4610: UEFN clients are not supposed to run Verse code."));

	for (FProperty* Property = (FProperty*)ChildProperties; Property; Property = (FProperty*)Property->Next)
	{
		if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			UVerseStruct* SolarisStruct = Cast<UVerseStruct>(StructProperty->Struct);
			if (SolarisStruct && SolarisStruct->InitFunction && SolarisStruct->ModuleClass && (!InstanceGraph || !InstanceGraph->IsPropertyInSubobjectExclusionList(Property)))
			{
				SolarisStruct->ModuleClass->GetDefaultObject()->ProcessEvent(SolarisStruct->InitFunction, StructProperty->ContainerPtrToValuePtr<void>(InObj));
			}
		}
	}
}

void UVerseClass::InstanceNewSubobjects(TNotNull<UObject*> InObj)
{
	bool bHasInstancedProperties = false;
	for (FProperty* Property = RefLink; Property != nullptr && !bHasInstancedProperties; Property = Property->NextRef)
	{
		bHasInstancedProperties = Property->ContainsInstancedObjectProperty();
	}

	if (bHasInstancedProperties)
	{
		FObjectInstancingGraph InstancingGraph(EObjectInstancingGraphOptions::InstanceTemplatesOnly);
		UObject* Archetype = GetDefaultObject();

		InstancingGraph.AddNewObject(InObj, Archetype);
		// We call the base class InstanceSubobjectTemplates which tries to instance subobjects on all instanced properties
		// because it should only instance subobject templates and keep already instanced subobjects without changes
		InstanceSubobjectTemplates(InObj, Archetype, nullptr, InObj, &InstancingGraph);
	}
}

namespace VerseClassPrivate
{

void GenerateSubobjectName(FString& OutName, const FString& InPrefix, const FProperty* InProperty, int32 Index)
{
	if (InPrefix.Len())
	{
		OutName = InPrefix;
		OutName += TEXT("_");
	}
	OutName += InProperty->GetName();
	if (Index > 0)
	{
		OutName += FString::Printf(TEXT("_%d"), Index);
	}
}

void RenameSubobject(UObject* Subobject, const FString& InName)
{
	const ERenameFlags RenameFlags = REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional;
	UObject* ExistingSubobject = StaticFindObjectFast(UObject::StaticClass(), Subobject->GetOuter(), *InName, false);
	if (ExistingSubobject && ExistingSubobject != Subobject)
	{
		// ExistingSubobject is an object with the same name and outer as the subobject currently assigned to the property we're traversing
		// The engine does not allow renaming on top of existing objects so we need to rename the old object first
		ExistingSubobject->Rename(*MakeUniqueObjectName(ExistingSubobject->GetOuter(), ExistingSubobject->GetClass()).ToString(), nullptr, RenameFlags);
	}
	Subobject->Rename(*InName, nullptr, RenameFlags);
}

void TraverseSubobjectsInternal(UObject* InObject, void* ContainerPtr, UStruct* Struct, const FString& Prefix, const TFunctionRef<void(UObject* /*Subobject*/, const FString& /*CanonicalSubobjectName*/)> Operation, ETraverseSubobjectsFlag Flags = ETraverseSubobjectsFlag::None);

void TraverseSubobjectsInternal(UObject* InObject, void* ContainerPtr, FProperty* RefProperty, const FString& Prefix, const TFunctionRef<void(UObject* /*Subobject*/, const FString& /*CanonicalSubobjectName*/)> Operation, ETraverseSubobjectsFlag Flags)
{
	{
		UStruct* OwnerStruct = RefProperty ? RefProperty->GetOwner<UStruct>() : nullptr;

		// If the direct owner of RefProperty is not a UStruct then we're traversing an inner property of a property that has already passed this test (FArray/FMap/FSetProperty)
		if (OwnerStruct && !OwnerStruct->IsA<UVerseClass>() && !OwnerStruct->IsA<UVerseStruct>())
		{
			// Skip non-verse properties
			return;
		}
	}

	bool bShouldGenerateSubobjectName = !(Flags & ETraverseSubobjectsFlag::NoNameGeneration);

	if (FObjectProperty* ObjProp = CastField<FObjectProperty>(RefProperty))
	{
		// Traverse all subobjects referenced by this property (potentially in a C-style array)
		for (int32 ObjectIndex = 0; ObjectIndex < ObjProp->ArrayDim; ++ObjectIndex)
		{
			void* Address = ObjProp->ContainerPtrToValuePtr<void>(ContainerPtr, ObjectIndex);
			UObject* Subobject = ObjProp->GetObjectPropertyValue(Address);
			if (Subobject && Subobject->GetOuter() == InObject)
			{
				FString CanonicalSubobjectName;

				if (bShouldGenerateSubobjectName)
				{
					GenerateSubobjectName(CanonicalSubobjectName, Prefix, ObjProp, ObjectIndex);
				}

				Operation(Subobject, CanonicalSubobjectName);
			}
		}
	}
	else if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(RefProperty))
	{
		// Traverse all subobjects referenced by this array property (potentially in a C-style array)
		for (int32 Index = 0; Index < ArrayProp->ArrayDim; ++Index)
		{
			FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(ContainerPtr, Index));

			// When traversing from an Optional property we could be dealing with an 'unset' (or invalid)
			// array here. For this reason use the unchecked variant.
			int32 ArrayNum = ArrayHelper.NumUnchecked();

			for (int32 ElementIndex = 0; ElementIndex < ArrayNum; ++ElementIndex)
			{
				FString NewPrefix;

				if (bShouldGenerateSubobjectName)
				{
					GenerateSubobjectName(NewPrefix, Prefix, ArrayProp, ElementIndex);
				}

				void* ElementAddress = ArrayHelper.GetRawPtr(ElementIndex);
				TraverseSubobjectsInternal(InObject, ElementAddress, ArrayProp->Inner, NewPrefix, Operation, Flags);
			}
		}
	}
	else if (FSetProperty* SetProp = CastField<FSetProperty>(RefProperty))
	{
		for (int32 Index = 0; Index < SetProp->ArrayDim; ++Index)
		{
			FScriptSetHelper SetHelper(SetProp, SetProp->ContainerPtrToValuePtr<void>(ContainerPtr, Index));

			// See comment for Array properties.
			int32 SetNum = SetHelper.NumUnchecked();

			for (int32 ElementIndex = 0, Count = SetNum; Count; ++ElementIndex)
			{
				if (SetHelper.IsValidIndex(ElementIndex))
				{
					FString NewPrefix;

					if (bShouldGenerateSubobjectName)
					{
						GenerateSubobjectName(NewPrefix, Prefix, SetProp, ElementIndex);
					}

					void* ElementAddress = SetHelper.GetElementPtr(ElementIndex);
					TraverseSubobjectsInternal(InObject, ElementAddress, SetProp->ElementProp, NewPrefix, Operation, Flags);
					--Count;
				}
			}
		}
	}
	else if (FMapProperty* MapProp = CastField<FMapProperty>(RefProperty))
	{
		for (int32 Index = 0; Index < MapProp->ArrayDim; ++Index)
		{
			FScriptMapHelper MapHelper(MapProp, MapProp->ContainerPtrToValuePtr<void>(ContainerPtr, Index));

			// See comment for Array properties.
			int32 MapNum = MapHelper.NumUnchecked();

			for (int32 ElementIndex = 0, Count = MapNum; Count; ++ElementIndex)
			{
				if (MapHelper.IsValidIndex(ElementIndex))
				{
					FString NewPrefix;

					if (bShouldGenerateSubobjectName)
					{
						GenerateSubobjectName(NewPrefix, Prefix, MapProp, ElementIndex);
					}

					uint8* ValuePairPtr = MapHelper.GetPairPtr(ElementIndex);

					TraverseSubobjectsInternal(InObject, ValuePairPtr, MapProp->KeyProp, NewPrefix + TEXT("_Key"), Operation, Flags);
					TraverseSubobjectsInternal(InObject, ValuePairPtr, MapProp->ValueProp, NewPrefix + TEXT("_Value"), Operation, Flags);

					--Count;
				}
			}
		}
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(RefProperty))
	{
		for (int32 Index = 0; Index < StructProp->ArrayDim; ++Index)
		{
			FString NewPrefix;

			if (bShouldGenerateSubobjectName)
			{
				GenerateSubobjectName(NewPrefix, Prefix, StructProp, Index);
			}

			void* StructAddress = StructProp->ContainerPtrToValuePtr<void>(ContainerPtr, Index);
			TraverseSubobjectsInternal(InObject, StructAddress, StructProp->Struct, NewPrefix, Operation, Flags);
		}
	}
	else if (FOptionalProperty* OptionProp = CastField<FOptionalProperty>(RefProperty))
	{
		FProperty* ValueProp = OptionProp->GetValueProperty();
		checkf(ValueProp->GetOffset_ForInternal() == 0, TEXT("Expected offset of value property of option property \"%s\" to be 0, got %d"), *OptionProp->GetFullName(), ValueProp->GetOffset_ForInternal());
		FString NewPrefix(Prefix);
		for (int32 Index = 0; Index < OptionProp->ArrayDim; ++Index)
		{
			// If for some reason the offset of ValueProp is not 0 then we may need to adjust how we calculate the ValueAddress
			void* ValueAddress = OptionProp->ContainerPtrToValuePtr<void>(ContainerPtr, Index);
			// Update the prefix only if this is an actual C-style array
			if (OptionProp->ArrayDim > 1)
			{
				if (bShouldGenerateSubobjectName)
				{
					GenerateSubobjectName(NewPrefix, Prefix, OptionProp, Index);
				}
			}
			TraverseSubobjectsInternal(InObject, ValueAddress, ValueProp, NewPrefix, Operation, Flags);
		}
	}
}

void TraverseSubobjectsInternal(UObject* InObject, void* ContainerPtr, UStruct* Struct, const FString& Prefix, const TFunctionRef<void(UObject* /*Subobject*/, const FString& /*CanonicalSubobjectName*/)> Operation, ETraverseSubobjectsFlag Flags)
{
	for (FProperty* RefProperty = Struct->RefLink; RefProperty; RefProperty = RefProperty->NextRef)
	{
		TraverseSubobjectsInternal(InObject, ContainerPtr, RefProperty, Prefix, Operation, Flags);
	}
}

} // namespace VerseClassPrivate

void UVerseClass::RenameDefaultSubobjects(UObject* InObject)
{
	VerseClassPrivate::TraverseSubobjectsInternal(InObject, InObject, InObject->GetClass(), /*Prefix*/ FString(), [](UObject* Subobject, const FString& CanonicalSubobjectName) {
		VerseClassPrivate::RenameSubobject(Subobject, CanonicalSubobjectName);
	});
}

bool UVerseClass::ValidateSubobjectArchetypes(UObject* InObject, UObject* InArchetype)
{
	bool bIsValid = true;

	check(InObject);

	if (InArchetype)
	{
		VerseClassPrivate::TraverseSubobjectsInternal(InObject, InObject, InObject->GetClass(), /*Prefix*/ FString(), [InArchetype, &bIsValid](UObject* Subobject, const FString& CanonicalSubobjectName) {
			if (!CanonicalSubobjectName.Equals(Subobject->GetName()))
			{
				UObject* SubArchetypeInOwnerArchetype = static_cast<UObject*>(FindObjectWithOuter(InArchetype, Subobject->GetClass(), Subobject->GetFName()));

				if (!SubArchetypeInOwnerArchetype)
				{
					const TCHAR* CanonicalSubobjectNameCStr = CanonicalSubobjectName.GetCharArray().GetData();

					UObject* ExpectedSubArchetype = static_cast<UObject*>(FindObjectWithOuter(InArchetype, Subobject->GetClass(), CanonicalSubobjectNameCStr));

					if (ExpectedSubArchetype)
					{
						UObject* SubArchetype = Subobject->GetArchetype();
						FString SubArchetypePath = SubArchetype ? SubArchetype->GetPathName() : FString();
						FString ExpectedSubArchetypePath = ExpectedSubArchetype->GetPathName();

						UE_LOG(LogSolGeneratedClass, Display, TEXT("Incorrectly named Verse sub-object: '%s', expected name: '%s' (path: '%s', archetype path: '%s', expected archetype path: '%s')"),
							*Subobject->GetName(), CanonicalSubobjectNameCStr, *Subobject->GetPathName(), *SubArchetypePath, *ExpectedSubArchetypePath);

						bIsValid = false;
					}
				}
			}
		});
	}

	return bIsValid;
}

int32 UVerseClass::GetVerseFunctionParameterCount(UFunction* Func)
{
	int32 ParameterCount = 0;
	if (FStructProperty* TupleProperty = CastField<FStructProperty>(Func->ChildProperties))
	{
		if (UStruct* TupleStruct = TupleProperty->Struct)
		{
			for (TFieldIterator<FProperty> It(TupleProperty->Struct); It; ++It)
			{
				if (It->GetFName() != UVerseClass::StructPaddingDummyName)
				{
					ParameterCount++;
				}
			}
		}
	}
	else
	{
		for (TFieldIterator<FProperty> It(Func); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			if (!It->HasAnyPropertyFlags(CPF_OutParm))
			{
				ParameterCount++;
			}
		}
	}
	return ParameterCount;
}

void UVerseClass::ForEachVerseFunction(UObject* Object, TFunctionRef<bool(FVerseFunctionDescriptor)> Operation, EFieldIterationFlags IterationFlags)
{
#if WITH_VERSE_BPVM
	checkf(Object, TEXT("Object instance must be provided when iterating Verse functions"));
	for (UVerseClass* Class = Cast<UVerseClass>(Object->GetClass());
		 Class != nullptr;
		 Class = Cast<UVerseClass>(Class->GetSuperClass()))
	{
		for (const TPair<FName, FName>& NamePair : Class->DisplayNameToUENameFunctionMap)
		{
			if (UFunction* VMFunc = Class->FindFunctionByName(NamePair.Value))
			{
				FVerseFunctionDescriptor Descriptor(Object, VMFunc, NamePair.Key, NamePair.Value);
				if (!Operation(Descriptor))
				{
					break;
				}
			}
		}

		if (!EnumHasAnyFlags(IterationFlags, EFieldIterationFlags::IncludeSuper))
		{
			break;
		}
	}
#endif // WITH_VERSE_BPVM
}

#if WITH_VERSE_BPVM
FVerseFunctionDescriptor UVerseClass::FindVerseFunctionByDisplayName(UObject* Object, const FString& DisplayName, EFieldIterationFlags SearchFlags)
{
	FName DisplayFName(DisplayName);
	checkf(Object, TEXT("Object instance must be provided when searching for Verse functions"));
	for (UVerseClass* Class = Cast<UVerseClass>(Object->GetClass());
		 Class != nullptr;
		 Class = Cast<UVerseClass>(Class->GetSuperClass()))
	{
		if (FName* UEName = Class->DisplayNameToUENameFunctionMap.Find(DisplayFName))
		{
			return FVerseFunctionDescriptor(Object, nullptr, DisplayFName, *UEName);
		}

		if (!EnumHasAnyFlags(SearchFlags, EFieldIterationFlags::IncludeSuper))
		{
			break;
		}
	}
	return FVerseFunctionDescriptor();
}
#endif // WITH_VERSE_BPVM

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
void UVerseClass::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);
	UVerseClass* This = static_cast<UVerseClass*>(InThis);
	Collector.AddReferencedVerseValue(This->Shape);
}
#endif

namespace VerseClassPrivate
{

#if WITH_EDITOR
// Property attributes used by the editor implementation. Set here to avoid requiring a recompile on cooked class types.
static const FName MD_EditInline(TEXT("EditInline"));
static const FName MD_SupportsDynamicInstance(TEXT("SupportsDynamicInstance"));
#endif // WITH_EDITOR

// Determines if the given property can be treated as an instanced reference.
bool CanTreatAsInstancedProperty(FProperty* RefProp)
{
	// The 'self' member of a task class must be handled as a special case, since it is implicitly bound at compile time.
	static const FName ContextSelfName("_Self");
	const bool bHasTaskClassNamePrefix = RefProp->GetOwnerClass()->GetName().StartsWith(Verse::FPackageName::TaskUClassPrefix);
	if (bHasTaskClassNamePrefix
		&& RefProp->HasAnyPropertyFlags(CPF_Parm)
		&& RefProp->GetFName() == ContextSelfName)
	{
		return false;
	}

	return true;
}

// Used to recursively apply instanced class property flags to an object property when dynamic subobject instancing is disabled.
void ApplyInstancedObjectPropertyFlags(FProperty* RefProp)
{
	if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(RefProp))
	{
		ApplyInstancedObjectPropertyFlags(ArrayProperty->Inner);
		if (ArrayProperty->Inner->ContainsInstancedObjectProperty())
		{
			ArrayProperty->SetPropertyFlags(CPF_ContainsInstancedReference);
		}
	}
	else if (FSetProperty* SetProperty = CastField<FSetProperty>(RefProp))
	{
		ApplyInstancedObjectPropertyFlags(SetProperty->ElementProp);
		if (SetProperty->ElementProp->ContainsInstancedObjectProperty())
		{
			SetProperty->SetPropertyFlags(CPF_ContainsInstancedReference);
		}
	}
	else if (FMapProperty* MapProperty = CastField<FMapProperty>(RefProp))
	{
		ApplyInstancedObjectPropertyFlags(MapProperty->KeyProp);
		ApplyInstancedObjectPropertyFlags(MapProperty->ValueProp);
		if (MapProperty->KeyProp->ContainsInstancedObjectProperty()
			|| MapProperty->ValueProp->ContainsInstancedObjectProperty())
		{
			MapProperty->SetPropertyFlags(CPF_ContainsInstancedReference);
		}
	}
	else if (FOptionalProperty* OptionalProperty = CastField<FOptionalProperty>(RefProp))
	{
		ApplyInstancedObjectPropertyFlags(OptionalProperty->GetValueProperty());
		if (OptionalProperty->GetValueProperty()->ContainsInstancedObjectProperty())
		{
			OptionalProperty->SetPropertyFlags(CPF_ContainsInstancedReference);
		}
	}
	else if (FStructProperty* StructProperty = CastField<FStructProperty>(RefProp))
	{
		// Note: When instanced reference semantics are used, the Verse compiler always applies this to struct properties,
		// regardless of whether or not the struct has any instanced reference fields. I am choosing to emulate that here.
		StructProperty->SetPropertyFlags(CPF_ContainsInstancedReference);
	}
	else if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(RefProp))
	{
		ObjectProperty->SetPropertyFlags(CPF_PersistentInstance | CPF_ExportObject | CPF_InstancedReference);
#if WITH_EDITOR
		// This is imposed by the @editable attribute when instanced reference semantics are enabled in the absence of
		// "editinline" meta. See ProcessEditableUeProperty() / "verse.EditInlineSubobjectProperties" for more context.
		if (!ObjectProperty->HasMetaData(MD_EditInline))
		{
			ObjectProperty->SetMetaData(MD_SupportsDynamicInstance, TEXT("true"));
		}
#endif // WITH_EDITOR
	}
#if WITH_VERSE_BPVM
	else if (FVerseDynamicProperty* DynamicProperty = CastField<FVerseDynamicProperty>(RefProp))
	{
		DynamicProperty->SetPropertyFlags(CPF_InstancedReference);
	}
#endif // WITH_VERSE_BPVM
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	else if (FVRestValueProperty* RestValueProperty = CastField<FVRestValueProperty>(RefProp))
	{
		RestValueProperty->SetPropertyFlags(CPF_InstancedReference);
	}
#endif
}

// Used to recursively clear instanced class property flags from an object property when dynamic subobject instancing is enabled.
void ClearInstancedObjectPropertyFlags(FProperty* RefProp)
{
	if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(RefProp))
	{
		if (ArrayProperty->Inner->ContainsInstancedObjectProperty())
		{
			ArrayProperty->ClearPropertyFlags(CPF_ContainsInstancedReference);
		}

		ClearInstancedObjectPropertyFlags(ArrayProperty->Inner);
	}
	else if (FSetProperty* SetProperty = CastField<FSetProperty>(RefProp))
	{
		if (SetProperty->ElementProp->ContainsInstancedObjectProperty())
		{
			SetProperty->ClearPropertyFlags(CPF_ContainsInstancedReference);
		}

		ClearInstancedObjectPropertyFlags(SetProperty->ElementProp);
	}
	else if (FMapProperty* MapProperty = CastField<FMapProperty>(RefProp))
	{
		if (MapProperty->KeyProp->ContainsInstancedObjectProperty()
			|| MapProperty->ValueProp->ContainsInstancedObjectProperty())
		{
			MapProperty->ClearPropertyFlags(CPF_ContainsInstancedReference);
		}

		ClearInstancedObjectPropertyFlags(MapProperty->KeyProp);
		ClearInstancedObjectPropertyFlags(MapProperty->ValueProp);
	}
	else if (FOptionalProperty* OptionalProperty = CastField<FOptionalProperty>(RefProp))
	{
		if (OptionalProperty->GetValueProperty()->ContainsInstancedObjectProperty())
		{
			OptionalProperty->ClearPropertyFlags(CPF_ContainsInstancedReference);
		}

		ClearInstancedObjectPropertyFlags(OptionalProperty->GetValueProperty());
	}
	else if (FStructProperty* StructProperty = CastField<FStructProperty>(RefProp))
	{
		// Note: When instanced reference semantics are used, the Verse compiler always applies this to struct properties,
		// regardless of whether or not the struct has any instanced reference fields. I am choosing to emulate that here.
		StructProperty->ClearPropertyFlags(CPF_ContainsInstancedReference);
	}
	else if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(RefProp))
	{
		ObjectProperty->ClearPropertyFlags(CPF_PersistentInstance | CPF_ExportObject | CPF_InstancedReference);
#if WITH_EDITOR
		// This is imposed by the @editable attribute when instanced reference semantics are enabled in the absence of
		// "editinline" meta. See ProcessEditableUeProperty() / "verse.EditInlineSubobjectProperties" for more context.
		if (!ObjectProperty->HasMetaData(MD_EditInline))
		{
			ObjectProperty->RemoveMetaData(MD_SupportsDynamicInstance);
		}
#endif // WITH_EDITOR
	}
#if WITH_VERSE_BPVM
	else if (FVerseDynamicProperty* DynamicProperty = CastField<FVerseDynamicProperty>(RefProp))
	{
		DynamicProperty->ClearPropertyFlags(CPF_InstancedReference);
	}
#endif // WITH_VERSE_BPVM
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	else if (FVRestValueProperty* RestValueProperty = CastField<FVRestValueProperty>(RefProp))
	{
		RestValueProperty->ClearPropertyFlags(CPF_InstancedReference);
	}
#endif
}

} // namespace VerseClassPrivate

void UVerseClass::EnableDynamicInstancedReferenceSupport()
{
	// Nothing to do if already enabled.
	if (SupportsDynamicInstancedReference())
	{
		return;
	}

	// Clear instanced property flags to simulate being compiled with instanced reference semantics disabled.
	for (FProperty* RefProp = RefLink; RefProp && RefProp->GetOwnerClass() == this; RefProp = RefProp->NextRef)
	{
		if (VerseClassPrivate::CanTreatAsInstancedProperty(RefProp))
		{
			VerseClassPrivate::ClearInstancedObjectPropertyFlags(RefProp);
		}
	}

	// Signal that this class no longer has explicitly-instanced properties and now supports dynamic instancing.
	SolClassFlags &= ~VCLASS_HasInstancedSemantics;
}

void UVerseClass::DisableDynamicInstancedReferenceSupport()
{
	// Nothing to do if already disabled.
	if (!SupportsDynamicInstancedReference())
	{
		return;
	}

	// Apply instanced property flags to allow instancing to work without dynamic references (legacy mode).
	for (FProperty* RefProp = RefLink; RefProp && RefProp->GetOwnerClass() == this; RefProp = RefProp->NextRef)
	{
		if (VerseClassPrivate::CanTreatAsInstancedProperty(RefProp))
		{
			VerseClassPrivate::ApplyInstancedObjectPropertyFlags(RefProp);
		}
	}

	// Signal that this class now has explicitly-instanced properties and no longer supports dynamic instancing.
	SolClassFlags |= VCLASS_HasInstancedSemantics;
}

UVerseClass::FStaleClassInfo UVerseClass::ResetUHTNative()
{
	check(IsUHTNative());

	FStaleClassInfo StaleState;
	StaleState.SourceClass = this;
	Swap(StaleState.DisplayNameToUENameFunctionMap, DisplayNameToUENameFunctionMap);
	Swap(StaleState.FunctionMangledNames, FunctionMangledNames);
	Swap(StaleState.TaskClasses, TaskClasses);
	StripVerseGeneratedFunctions(&StaleState.Children);
	return StaleState;
}

void UVerseClass::StripVerseGeneratedFunctions(TArray<TKeyValuePair<FName, TObjectPtr<UField>>>* StrippedFields)
{
	UField* Current = Children;
	Children = nullptr;
	UField::FLinkedListBuilder KeepBuilder(ToRawPtr(MutableView(Children)));
	while (Current != nullptr)
	{
		UField* NextField = Current->Next;
		Current->Next = nullptr;
		if (UVerseFunction::IsVerseGeneratedFunction(Current))
		{
			if (UFunction* AsFunction = Cast<UFunction>(Current))
			{
				RemoveFunctionFromFunctionMap(AsFunction);
				FName OriginalName = AsFunction->GetFName();
				Verse::Names::MakeTypeDead(AsFunction, AsFunction->GetOuter());
				if (StrippedFields != nullptr)
				{
					StrippedFields->Emplace(OriginalName, AsFunction);
				}
			}
		}
		else
		{
			KeepBuilder.AppendNoTerminate(*Current);
		}
		Current = NextField;
	}
}

#if WITH_VERSE_BPVM
void UVerseClass::BindVerseFunction(const char* DecoratedFunctionName, FNativeFuncPtr NativeThunkPtr)
{
	FString UEName = Verse::Names::VerseFuncToUEName(FString(DecoratedFunctionName));
	FName UEFName = FName(UEName);

	// If this class has yet to be loaded, or was just loaded, deal with it later in UVerseClass::Link
	if (!HasAnyFlags(RF_NeedLoad | RF_WasLoaded))
	{
		// Not a loaded class, bind immediately
		UFunction* UeFunction = FindFunctionByName(UEFName);
		if (ensureAlwaysMsgf(UeFunction, TEXT("Missing generated function: `%s.%s`"), *GetName(), *UEName))
		{
			UeFunction->SetNativeFunc(NativeThunkPtr);
			UeFunction->FunctionFlags |= FUNC_Native;
		}
	}

	// Register this native call in the NativeFunctionLookupTable
	FNativeFunctionLookup* FuncMapping = NativeFunctionLookupTable.FindByPredicate([UEFName](const FNativeFunctionLookup& NativeFunctionLookup) {
		return UEFName == NativeFunctionLookup.Name;
	});
	if (FuncMapping == nullptr)
	{
		NativeFunctionLookupTable.Emplace(UEFName, NativeThunkPtr);
	}
	else
	{
		FuncMapping->Pointer = NativeThunkPtr;
	}
}
#endif

#if WITH_VERSE_BPVM
void UVerseClass::BindVerseCoroClass(const char* DecoratedFunctionName, FNativeFuncPtr NativeThunkPtr)
{
	FString UEName = Verse::Names::VerseFuncToUEName(FString(DecoratedFunctionName));

	const FString TaskClassName = Verse::FPackageName::GetTaskUClassName(*this, *UEName);
	UVerseClass* TaskClass = FindObject<UVerseClass>(GetOutermost(), *TaskClassName);
	if (ensureAlwaysMsgf(TaskClass, TEXT("Failed to find coroutine task class: `%s`"), *TaskClassName))
	{
		TaskClass->BindVerseFunction("Update", NativeThunkPtr);
	}
}
#endif

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
void UVerseClass::SetVerseCallableThunks(const FVerseCallableThunk* InThunks, uint32 NumThunks)
{
	VerseCallableThunks.Empty();
	VerseCallableThunks.Append(InThunks, NumThunks);
}
#endif

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
void UVerseClass::BindVerseCallableFunctions(Verse::VPackage* VersePackage, FUtf8StringView VerseScopePath)
{
	for (const FVerseCallableThunk& Thunk : VerseCallableThunks)
	{
		Verse::VNativeFunction::SetThunk(VersePackage, VerseScopePath, Thunk.NameUTF8, Thunk.Pointer);
	}
}
#endif
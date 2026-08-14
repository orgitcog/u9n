// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableObjectInstance.h"

#include "Algo/Find.h"
#include "Algo/MaxElement.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "BoneControllers/AnimNode_RigidBody.h"
#include "ClothConfig.h"
#include "ClothingAsset.h"
#include "MuCO/CustomizableObjectInstanceUsagePrivate.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/SkeletalMeshLODSettings.h"
#include "MaterialDomain.h"
#include "MutableStreamRequest.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Optional.h"
#include "Modules/ModuleManager.h"
#include "Tasks/Task.h"

#include "MuCO/CustomizableObjectSystemPrivate.h"
#include "MuCO/CustomizableObjectSkeletalMesh.h"
#include "MuCO/CustomizableObjectInstancePrivate.h"
#include "MuCO/CustomizableObjectExtension.h"
#include "MuCO/CustomizableObjectMipDataProvider.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectInstanceUsage.h"
#include "MuCO/CustomizableObjectInstanceAssetUserData.h"
#include "MuCO/ICustomizableObjectModule.h"
#include "MuCO/UnrealConversionUtils.h"
#include "MuCO/UnrealPortabilityHelpers.h"
#include "MuCO/LogBenchmarkUtil.h"

#include "PhysicsEngine/PhysicsAsset.h"
#include "Rendering/Texture2DResource.h"
#include "RenderingThread.h"
#include "SkeletalMergingLibrary.h"
#include "MuCO/ICustomizableObjectEditorModule.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Hash/CityHash.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCO/CustomizableObjectResourceData.h"
#include "MuCO/CustomizableObjectStreamedResourceData.h"
#include "MuCO/CustomizableObjectResourceDataTypes.h"
#include "MuCO/LoadUtils.h"
#include "Serialization/BulkData.h"
#include "HAL/PlatformFileManager.h"

#include "MuCO/Plugins/IMutableClothingModule.h"
#include "Rendering/SkeletalMeshModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomizableObjectInstance)

#if WITH_EDITOR
#include "Logging/MessageLog.h"
#include "MessageLogModule.h"
#include "UnrealEdMisc.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Application/ThrottleManager.h"
#endif

namespace
{
#ifndef REQUIRES_SINGLEUSE_FLAG_FOR_RUNTIME_TEXTURES
	#define REQUIRES_SINGLEUSE_FLAG_FOR_RUNTIME_TEXTURES !PLATFORM_DESKTOP
#endif

bool bDisableClothingPhysicsEditsPropagation = false;
static FAutoConsoleVariableRef CVarDisableClothingPhysicsEditsPropagation(
	TEXT("mutable.DisableClothingPhysicsEditsPropagation"),
	bDisableClothingPhysicsEditsPropagation,
	TEXT("If set to true, disables clothing physics edits propagation from the render mesh."),
	ECVF_Default);

bool bDisableNotifyComponentsOfTextureUpdates = false;
static FAutoConsoleVariableRef CVarDisableNotifyComponentsOfTextureUpdates(
	TEXT("mutable.DisableNotifyComponentsOfTextureUpdates"),
	bDisableNotifyComponentsOfTextureUpdates,
	TEXT("If set to true, disables Mutable notifying the streaming system that a component has had a change in at least one texture of its components."),
	ECVF_Default);

}

const FString MULTILAYER_PROJECTOR_PARAMETERS_INVALID = TEXT("Invalid Multilayer Projector Parameters.");

const FString NUM_LAYERS_PARAMETER_POSTFIX = FString("_NumLayers");
const FString OPACITY_PARAMETER_POSTFIX = FString("_Opacity");
const FString IMAGE_PARAMETER_POSTFIX = FString("_SelectedImages");
const FString POSE_PARAMETER_POSTFIX = FString("_SelectedPoses");


// Struct used by BuildMaterials() to identify common materials between LODs
struct FMutableMaterialPlaceholder
{
	enum class EPlaceHolderParamType { Vector, Scalar, Texture };

	struct FMutableMaterialPlaceHolderParam
	{
		FName ParamName;
		EPlaceHolderParamType Type;
		int32 LayerIndex; // Set to -1 for non-multilayer params
		float Scalar;
		FLinearColor Vector;
		FGeneratedTexture Texture;

		FMutableMaterialPlaceHolderParam(const FName& InParamName, const int32 InLayerIndex, const FLinearColor& InVector)
			: ParamName(InParamName), Type(EPlaceHolderParamType::Vector), LayerIndex(InLayerIndex), Vector(InVector) {}

		FMutableMaterialPlaceHolderParam(const FName& InParamName, const int32 InLayerIndex, const float InScalar)
			: ParamName(InParamName), Type(EPlaceHolderParamType::Scalar), LayerIndex(InLayerIndex), Scalar(InScalar) {}

		FMutableMaterialPlaceHolderParam(const FName& InParamName, const int32 InLayerIndex, const FGeneratedTexture& InTexture)
			: ParamName(InParamName), Type(EPlaceHolderParamType::Texture), LayerIndex(InLayerIndex), Texture(InTexture) {}

		bool operator<(const FMutableMaterialPlaceHolderParam& Other) const
		{
			return Type < Other.Type || ParamName.CompareIndexes(Other.ParamName);
		}

		bool operator==(const FMutableMaterialPlaceHolderParam& Other) const = default;
	};

	uint32 ParentMaterialID = 0;
	int32 MatIndex = -1;

private:
	mutable TArray<FMutableMaterialPlaceHolderParam> Params;

public:
	void AddParam(const FMutableMaterialPlaceHolderParam& NewParam) { Params.Add(NewParam); }
	
	const TArray<FMutableMaterialPlaceHolderParam>& GetParams() const { return Params; }

	bool operator==(const FMutableMaterialPlaceholder& Other) const;

	friend uint32 GetTypeHash(const FMutableMaterialPlaceholder& PlaceHolder);
};


bool FMutableMaterialPlaceholder::operator==(const FMutableMaterialPlaceholder& Other) const
{
	return ParentMaterialID == Other.ParentMaterialID &&
		   Params == Other.Params;
}


// Return a hash of the material and its parameters
uint32 GetTypeHash(const FMutableMaterialPlaceholder& PlaceHolder)
{
	uint32 Hash = GetTypeHash(PlaceHolder.ParentMaterialID);

	// Sort parameters before building the hash.
	PlaceHolder.Params.Sort();

	for (const FMutableMaterialPlaceholder::FMutableMaterialPlaceHolderParam& Param : PlaceHolder.Params)
	{
		uint32 ParamHash = GetTypeHash(Param.ParamName);
		ParamHash = HashCombineFast(ParamHash, (uint32)Param.LayerIndex);
		ParamHash = HashCombineFast(ParamHash, (uint32)Param.Type);

		switch (Param.Type)
		{
		case FMutableMaterialPlaceholder::EPlaceHolderParamType::Vector:
			ParamHash = HashCombineFast(ParamHash, GetTypeHash(Param.Vector));
			break;

		case FMutableMaterialPlaceholder::EPlaceHolderParamType::Scalar:
			ParamHash = HashCombineFast(ParamHash, GetTypeHash(Param.Scalar));
			break;

		case FMutableMaterialPlaceholder::EPlaceHolderParamType::Texture:
			ParamHash = HashCombineFast(ParamHash, Param.Texture.Texture->GetUniqueID());
			break;
		}

		Hash = HashCombineFast(Hash, ParamHash);
	}

	return Hash;
}


UTexture2D* UCustomizableInstancePrivate::CreateTexture(const FString& TextureName)
{
	UTexture2D* NewTexture = NewObject<UTexture2D>(
		GetTransientPackage(),
		GetData(TextureName),
		RF_Transient
		);
	UCustomizableObjectSystem::GetInstance()->GetPrivate()->LogBenchmarkUtil.AddTexture(*NewTexture);
	NewTexture->SetPlatformData( nullptr );

	return NewTexture;
}


void UCustomizableInstancePrivate::SetLastMeshId(FCustomizableObjectComponentIndex ObjectComponentIndex, int32 LODIndex, mu::FResourceID MeshId)
{
	FCustomizableInstanceComponentData* ComponentData = GetComponentData(ObjectComponentIndex);
	if (ComponentData && ComponentData->LastMeshIdPerLOD.IsValidIndex(LODIndex))
	{
		ComponentData->LastMeshIdPerLOD[LODIndex] = MeshId;
	}
	else
	{
		check(false);
	}
}


void UCustomizableInstancePrivate::InvalidateGeneratedData()
{
	SkeletalMeshStatus = ESkeletalMeshStatus::NotGenerated;
	SkeletalMeshes.Reset();

	CommittedDescriptor = {};
	CommittedDescriptorHash = {};

	// Init Component Data
	FCustomizableInstanceComponentData TemplateComponentData;
	TemplateComponentData.LastMeshIdPerLOD.Init(MAX_uint64, MAX_MESH_LOD_COUNT);
	ComponentsData.Init(TemplateComponentData, ComponentsData.Num());

	GeneratedMaterials.Empty();
}


void UCustomizableInstancePrivate::InitCustomizableObjectData(const UCustomizableObject* InCustomizableObject)
{
	InvalidateGeneratedData();

	if (!InCustomizableObject || !InCustomizableObject->IsCompiled())
	{
		return;
	}
	
	// Init Component Data
	FCustomizableInstanceComponentData TemplateComponentData;
	TemplateComponentData.LastMeshIdPerLOD.Init(MAX_uint64, MAX_MESH_LOD_COUNT);
	ComponentsData.Init(TemplateComponentData, InCustomizableObject->GetComponentCount());

	ExtensionInstanceData.Empty();
}


FCustomizableInstanceComponentData* UCustomizableInstancePrivate::GetComponentData(const FName& ComponentName)
{
	UCustomizableObject* Object = GetPublic()->GetCustomizableObject();
	if (!Object || !Object->IsCompiled())
	{
		return nullptr;
	}

	const int32 ObjectComponentIndex = Object->GetPrivate()->GetModelResourcesChecked().ComponentNamesPerObjectComponent.IndexOfByKey(ComponentName);
	if (ObjectComponentIndex == INDEX_NONE)
	{
		return nullptr;
	}

	if (!ComponentsData.IsValidIndex(ObjectComponentIndex))
	{
		return nullptr;	
	}

	return &ComponentsData[ObjectComponentIndex];
}


FCustomizableInstanceComponentData* UCustomizableInstancePrivate::GetComponentData(FCustomizableObjectComponentIndex ObjectComponentIndex)
{
	return ComponentsData.IsValidIndex(ObjectComponentIndex.GetValue()) ? &ComponentsData[ObjectComponentIndex.GetValue()] : nullptr;
}


const FCustomizableInstanceComponentData* UCustomizableInstancePrivate::GetComponentData(FCustomizableObjectComponentIndex ObjectComponentIndex) const
{
	return ComponentsData.IsValidIndex(ObjectComponentIndex.GetValue()) ? &ComponentsData[ObjectComponentIndex.GetValue()] : nullptr;
}


UCustomizableObjectInstance::UCustomizableObjectInstance()
{
	SetFlags(RF_Transactional);
}


void UCustomizableInstancePrivate::SetDescriptor(const FCustomizableObjectInstanceDescriptor& InDescriptor)
{
	UCustomizableObject* InCustomizableObject = InDescriptor.GetCustomizableObject();
	const bool bCustomizableObjectChanged = GetPublic()->Descriptor.GetCustomizableObject() != InCustomizableObject;

#if WITH_EDITOR
	// Bind a lambda to the PostCompileDelegate and unbind from the previous object if any.
	BindObjectDelegates(GetPublic()->GetCustomizableObject(), InCustomizableObject);
#endif

	GetPublic()->Descriptor = InDescriptor;

	if (bCustomizableObjectChanged)
	{
		InitCustomizableObjectData(InCustomizableObject);
	}
}


void UCustomizableInstancePrivate::PrepareForUpdate(const TSharedRef<FUpdateContextPrivate>& OperationData)
{
	// Clear the ComponentData from previous updates
	for (FCustomizableInstanceComponentData& ComponentData : ComponentsData)
	{
		ComponentData.AnimSlotToBP.Empty();
		ComponentData.AssetUserDataArray.Empty();
		ComponentData.Skeletons.Skeleton = nullptr;
		ComponentData.Skeletons.SkeletonIds.Empty();
		ComponentData.Skeletons.SkeletonsToMerge.Empty();
		ComponentData.PhysicsAssets.PhysicsAssetToLoad.Empty();
		ComponentData.PhysicsAssets.PhysicsAssetsToMerge.Empty();
		ComponentData.ClothingPhysicsAssetsToStream.Empty();
		ComponentData.StreamedResourceIndex.Empty();
		ComponentData.OverlayMaterial = nullptr;

#if WITH_EDITORONLY_DATA
		ComponentData.MeshPartPaths.Empty();
#endif
	}
}


#if WITH_EDITOR
void UCustomizableInstancePrivate::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	// Invalidate all generated data to avoid modifying resources shared between CO instances.
	InvalidateGeneratedData();

	// Empty after duplicating or ReleasingMutableResources may free textures used by the other CO instance.
	GeneratedTextures.Empty();
}


void UCustomizableInstancePrivate::OnPostCompile()
{
	GetDescriptor().ReloadParameters();
	InitCustomizableObjectData(GetPublic()->GetCustomizableObject());
}


void UCustomizableInstancePrivate::OnObjectStatusChanged(FCustomizableObjectStatus::EState Previous, FCustomizableObjectStatus::EState Next)
{
	if (Previous != Next && Next == FCustomizableObjectStatus::EState::ModelLoaded)
	{
		OnPostCompile();
	}
}


void UCustomizableInstancePrivate::BindObjectDelegates(UCustomizableObject*  CurrentCustomizableObject, UCustomizableObject* NewCustomizableObject)
{
	if (CurrentCustomizableObject == NewCustomizableObject)
	{
		return;
	}

	// Unbind callback from the previous CO
	if (CurrentCustomizableObject)
	{
		CurrentCustomizableObject->GetPrivate()->Status.GetOnStateChangedDelegate().RemoveAll(this);
	}

	// Bind callback to the new CO
	if (NewCustomizableObject)
	{
		NewCustomizableObject->GetPrivate()->Status.GetOnStateChangedDelegate().AddUObject(this, &UCustomizableInstancePrivate::OnObjectStatusChanged);
	}
}


void UCustomizableObjectInstance::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UCustomizableObjectInstance, TextureParameterDeclarations))
	{
		UpdateSkeletalMeshAsync(true, true);
	}
}


bool UCustomizableObjectInstance::CanEditChange(const FProperty* InProperty) const
{
	bool bIsMutable = Super::CanEditChange(InProperty);
	if (bIsMutable && InProperty != NULL)
	{
		if (InProperty->GetFName() == TEXT("CustomizationObject"))
		{
			bIsMutable = false;
		}

		if (InProperty->GetFName() == TEXT("ParameterName"))
		{
			bIsMutable = false;
		}
	}

	return bIsMutable;
}

void UCustomizableObjectInstance::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	Super::PostTransacted(TransactionEvent);

	GetPrivate()->OnInstanceTransactedDelegate.Broadcast(TransactionEvent);
}

#endif // WITH_EDITOR


bool UCustomizableObjectInstance::IsEditorOnly() const
{
	if (UCustomizableObject* CustomizableObject = GetCustomizableObject())
	{
		return CustomizableObject->IsEditorOnly();
	}
	return false;
}

void UCustomizableObjectInstance::PostInitProperties()
{
	UObject::PostInitProperties();

	if (!HasAllFlags(RF_ClassDefaultObject))
	{
		if (!PrivateData)
		{
			PrivateData = NewObject<UCustomizableInstancePrivate>(this, FName("Private"));
		}
		else if (PrivateData->GetOuter() != this)
		{
			PrivateData = Cast<UCustomizableInstancePrivate>(StaticDuplicateObject(PrivateData, this, FName("Private")));
		}
	}
}


void UCustomizableObjectInstance::BeginDestroy()
{
	// Release the Live Instance ID if there it hadn't been released before
	DestroyLiveUpdateInstance();

	if (PrivateData)
	{
#if WITH_EDITOR
		// Unbind Object delegates
		PrivateData->BindObjectDelegates(GetCustomizableObject(), nullptr);
#endif

		PrivateData.Get()->ReleaseMutableResources(true, *this);
	}
	
	Super::BeginDestroy();
}


void UCustomizableObjectInstance::DestroyLiveUpdateInstance()
{
	if (PrivateData && PrivateData->LiveUpdateModeInstanceID)
	{
		// If UCustomizableObjectSystemPrivate::SSystem is nullptr it means it has already been destroyed, no point in registering an instanceID release
		// since the Mutable system has already been destroyed. Just checking UCustomizableObjectSystem::GetInstance() will try to recreate the system when
		// everything is shutting down, so it's better to check UCustomizableObjectSystemPrivate::SSystem first here
		if (UCustomizableObjectSystemPrivate::SSystem && UCustomizableObjectSystem::GetInstance() && UCustomizableObjectSystem::GetInstance()->GetPrivate())
		{
			UCustomizableObjectSystem::GetInstance()->GetPrivate()->InitInstanceIDRelease(PrivateData->LiveUpdateModeInstanceID);
			PrivateData->LiveUpdateModeInstanceID = 0;
		}
	}
}


void UCustomizableInstancePrivate::ReleaseMutableResources(bool bCalledFromBeginDestroy, const UCustomizableObjectInstance& Instance)
{
	GeneratedMaterials.Empty();

	if (UCustomizableObjectSystem::IsCreated()) // Need to check this because the object might be destroyed after the CustomizableObjectSystem at shutdown
	{
		UCustomizableObjectSystemPrivate* CustomizableObjectSystem = UCustomizableObjectSystem::GetInstance()->GetPrivate();
		// Get the cache of resources of all live instances of this object
		FMutableResourceCache& Cache = CustomizableObjectSystem->GetObjectCache(Instance.GetCustomizableObject());

		for (FGeneratedTexture& Texture : GeneratedTextures)
		{
			if (CustomizableObjectSystem->RemoveTextureReference(Texture.Key))
			{
				// Do not release textures when called from BeginDestroy, it would produce a texture artifact in the
				// instance's remaining sk meshes and GC is being performed anyway so it will free the textures if needed
				if (!bCalledFromBeginDestroy && CustomizableObjectSystem->bReleaseTexturesImmediately)
				{
					ReleaseMutableTexture(Texture.Key, Cast<UTexture2D>(Texture.Texture), Cache);
				}
			}
		}

		// Remove all references to cached Texture Parameters. Only if we're destroying the COI.
		if (bCalledFromBeginDestroy)
		{
			CustomizableObjectSystem->UnCacheTextureParameters(CommittedDescriptor.GetTextureParameters());
		}
	}

	GeneratedTextures.Empty();
}


bool UCustomizableObjectInstance::IsReadyForFinishDestroy()
{
	//return ReleaseResourcesFence.IsFenceComplete();
	return true;
}


void UCustomizableObjectInstance::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);

	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::GroupProjectorIntToScalarIndex)
	{
		TArray<int32> IntParametersToMove;

		// Find the num layer parameters that were int enums
		for (int32 i = 0; i < IntParameters_DEPRECATED.Num(); ++i)
		{
			if (IntParameters_DEPRECATED[i].ParameterName.EndsWith(NUM_LAYERS_PARAMETER_POSTFIX, ESearchCase::CaseSensitive))
			{
				FString ParameterNamePrefix, Aux;
				const bool bSplit = IntParameters_DEPRECATED[i].ParameterName.Split(NUM_LAYERS_PARAMETER_POSTFIX, &ParameterNamePrefix, &Aux);
				check(bSplit);

				// Confirm this is actually a multilayer param by finding the corresponding pose param
				for (int32 j = 0; j < IntParameters_DEPRECATED.Num(); ++j)
				{
					if (i != j)
					{
						if (IntParameters_DEPRECATED[j].ParameterName.StartsWith(ParameterNamePrefix, ESearchCase::CaseSensitive) &&
							IntParameters_DEPRECATED[j].ParameterName.EndsWith(POSE_PARAMETER_POSTFIX, ESearchCase::CaseSensitive))
						{
							IntParametersToMove.Add(i);
							break;
						}
					}
				}
			}
		}

		// Convert them to float params
		for (int32 i = 0; i < IntParametersToMove.Num(); ++i)
		{
			FloatParameters_DEPRECATED.AddDefaulted();
			FloatParameters_DEPRECATED.Last().ParameterName = IntParameters_DEPRECATED[IntParametersToMove[i]].ParameterName;
			FloatParameters_DEPRECATED.Last().ParameterValue = FCString::Atoi(*IntParameters_DEPRECATED[IntParametersToMove[i]].ParameterValueName);
			FloatParameters_DEPRECATED.Last().Id = IntParameters_DEPRECATED[IntParametersToMove[i]].Id;
		}

		// Remove them from the int params in reverse order
		for (int32 i = IntParametersToMove.Num() - 1; i >= 0; --i)
		{
			IntParameters_DEPRECATED.RemoveAt(IntParametersToMove[i]);
		}
	}
	
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::CustomizableObjectInstanceDescriptor)
	{
		Descriptor.CustomizableObject = CustomizableObject_DEPRECATED;

		Descriptor.BoolParameters = BoolParameters_DEPRECATED;
		Descriptor.IntParameters = IntParameters_DEPRECATED;
		Descriptor.FloatParameters = FloatParameters_DEPRECATED;
		Descriptor.TextureParameters = TextureParameters_DEPRECATED;
		Descriptor.VectorParameters = VectorParameters_DEPRECATED;
		Descriptor.ProjectorParameters = ProjectorParameters_DEPRECATED;
	}
}


void UCustomizableObjectInstance::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	PrivateData->BindObjectDelegates(nullptr, GetCustomizableObject());
#endif

	// Skip the cost of ReloadParameters in the cook commandlet; it will be reloaded during PreSave. For cooked runtime
	// and editor UI, reload on load because it will not otherwise reload unless the CustomizableObject recompiles.
	Descriptor.ReloadParameters();
	PrivateData->InitCustomizableObjectData(GetCustomizableObject());
}


FString UCustomizableObjectInstance::GetDesc()
{
	FString ObjectName = "Missing Object";
	if (UCustomizableObject* CustomizableObject = GetCustomizableObject())
	{
		ObjectName = CustomizableObject->GetName();
	}

	return FString::Printf(TEXT("Instance of [%s]"), *ObjectName);
}


int32 UCustomizableObjectInstance::GetProjectorValueRange(const FString& ParamName) const
{
	return Descriptor.GetProjectorValueRange(ParamName);
}


int32 UCustomizableObjectInstance::GetIntValueRange(const FString& ParamName) const
{
	return Descriptor.GetIntValueRange(ParamName);
}


int32 UCustomizableObjectInstance::GetFloatValueRange(const FString& ParamName) const
{
	return Descriptor.GetFloatValueRange(ParamName);
}


int32 UCustomizableObjectInstance::GetTextureValueRange(const FString& ParamName) const
{
	return Descriptor.GetTextureValueRange(ParamName);
}


// Only safe to call if the Mutable texture ref count system returns 0 and absolutely sure nobody holds a reference to the texture
void UCustomizableInstancePrivate::ReleaseMutableTexture(const FMutableImageCacheKey& MutableTextureKey, UTexture2D* Texture, FMutableResourceCache& Cache)
{
	if (ensure(Texture) && Texture->IsValidLowLevel())
	{
		Texture->ConditionalBeginDestroy();

		for (FTexture2DMipMap& Mip : Texture->GetPlatformData()->Mips)
		{
			Mip.BulkData.RemoveBulkData();
		}
	}

	// Must remove texture from cache since it has been released
	Cache.Images.Remove(MutableTextureKey);
}


void UCustomizableObjectInstance::SetObject(UCustomizableObject* InObject)
{
#if WITH_EDITOR
	// Bind a lambda to the PostCompileDelegate and unbind from the previous object if any.
	PrivateData->BindObjectDelegates(GetCustomizableObject(), InObject);
#endif

	Descriptor.SetCustomizableObject(InObject);
	PrivateData->InitCustomizableObjectData(InObject);
}


UCustomizableObject* UCustomizableObjectInstance::GetCustomizableObject() const
{
	return Descriptor.CustomizableObject;
}


bool UCustomizableObjectInstance::GetBuildParameterRelevancy() const
{
	return Descriptor.GetBuildParameterRelevancy();
}


void UCustomizableObjectInstance::SetBuildParameterRelevancy(bool Value)
{
	Descriptor.SetBuildParameterRelevancy(Value);
}


int32 UCustomizableInstancePrivate::GetState() const
{
	return GetPublic()->Descriptor.GetState();
}


void UCustomizableInstancePrivate::SetState(const int32 InState)
{
	const int32 OldState = GetState();
	
	GetPublic()->Descriptor.SetState(InState);

	if (OldState != InState)
	{
		// State may change texture properties, so invalidate the texture reuse cache
		TextureReuseCache.Empty();
	}
}


FString UCustomizableObjectInstance::GetCurrentState() const
{
	return Descriptor.GetCurrentState();
}


void UCustomizableObjectInstance::SetCurrentState(const FString& StateName)
{
	Descriptor.SetCurrentState(StateName);
}


bool UCustomizableObjectInstance::IsParameterRelevant(int32 ParameterIndex) const
{
	// This should have been precalculated in the last update if the appropriate flag in the instance was set.
	return GetPrivate()->RelevantParameters.Contains(ParameterIndex);
}


bool UCustomizableObjectInstance::IsParameterRelevant(const FString& ParamName) const
{
	UCustomizableObject* CustomizableObject = GetCustomizableObject();

	if (!CustomizableObject)
	{
		return false;
	}

	// This should have been precalculated in the last update if the appropriate flag in the instance was set.
	int32 ParameterIndexInObject = CustomizableObject->GetPrivate()->FindParameter(ParamName);
	return GetPrivate()->RelevantParameters.Contains(ParameterIndexInObject);
}


bool UCustomizableObjectInstance::IsParameterDirty(const FString& ParamName, const int32 RangeIndex) const
{
	switch (Descriptor.CustomizableObject->GetParameterTypeByName(ParamName))
	{
	case EMutableParameterType::None:
		return false;

	case EMutableParameterType::Projector:
		{
			const FCustomizableObjectProjectorParameterValue* Result = Descriptor.GetProjectorParameters().FindByPredicate([&](const FCustomizableObjectProjectorParameterValue& Value)
				{
					return Value.ParameterName == ParamName;
				});
			
			const FCustomizableObjectProjectorParameterValue* ResultCommited = GetPrivate()->CommittedDescriptor.GetProjectorParameters().FindByPredicate([&](const FCustomizableObjectProjectorParameterValue& Value)
				{
					return Value.ParameterName == ParamName;
				});
			
			if (Result && ResultCommited)
			{
				if (RangeIndex == INDEX_NONE)
				{
					return Result->Value == ResultCommited->Value;					
				}
				else
				{
					if (Result->RangeValues.IsValidIndex(RangeIndex) && ResultCommited->RangeValues.IsValidIndex(RangeIndex))
					{
						return Result->RangeValues[RangeIndex] == ResultCommited->RangeValues[RangeIndex];
					}
					else
					{
						return Result->RangeValues.Num() != ResultCommited->RangeValues.Num();
					}
				}
			}
			else
			{
				return Result != ResultCommited;
			}
		}		
	case EMutableParameterType::Texture:
		{
			const FCustomizableObjectAssetParameterValue* Result = Descriptor.GetTextureParameters().FindByPredicate([&](const FCustomizableObjectAssetParameterValue& Value)
				{
					return Value.ParameterName == ParamName;
				});
			
			const FCustomizableObjectAssetParameterValue* ResultCommited = GetPrivate()->CommittedDescriptor.GetTextureParameters().FindByPredicate([&](const FCustomizableObjectAssetParameterValue& Value)
				{
					return Value.ParameterName == ParamName;
				});
			
			if (Result && ResultCommited)
			{
				if (RangeIndex == INDEX_NONE)
				{
					return Result->ParameterValue == ResultCommited->ParameterValue;					
				}
				else
				{
					if (Result->ParameterRangeValues.IsValidIndex(RangeIndex) && ResultCommited->ParameterRangeValues.IsValidIndex(RangeIndex))
					{
						return Result->ParameterRangeValues[RangeIndex] == ResultCommited->ParameterRangeValues[RangeIndex];
					}
					else
					{
						return Result->ParameterRangeValues.Num() != ResultCommited->ParameterRangeValues.Num();
					}
				}
			}
			else
			{
				return Result != ResultCommited;
			}
		}

	case EMutableParameterType::Bool:
		{
			const FCustomizableObjectBoolParameterValue* Result = Descriptor.GetBoolParameters().FindByPredicate([&](const FCustomizableObjectBoolParameterValue& Value)
				{
					return Value.ParameterName == ParamName;
				});
			
			const FCustomizableObjectBoolParameterValue* ResultCommited = GetPrivate()->CommittedDescriptor.GetBoolParameters().FindByPredicate([&](const FCustomizableObjectBoolParameterValue& Value)
				{
					return Value.ParameterName == ParamName;
				});
			
			if (Result && ResultCommited)
			{
				if (RangeIndex == INDEX_NONE)
				{
					return Result->ParameterValue == ResultCommited->ParameterValue;					
				}
				else
				{
					return false;
				}
			}
			else
			{
				return Result != ResultCommited;
			}
		}
	case EMutableParameterType::Int:
		{
			const FCustomizableObjectIntParameterValue* Result = Descriptor.GetIntParameters().FindByPredicate([&](const FCustomizableObjectIntParameterValue& Value)
				{
					return Value.ParameterName == ParamName;
				});
			
			const FCustomizableObjectIntParameterValue* ResultCommited = GetPrivate()->CommittedDescriptor.GetIntParameters().FindByPredicate([&](const FCustomizableObjectIntParameterValue& Value)
				{
					return Value.ParameterName == ParamName;
				});
			
			if (Result && ResultCommited)
			{
				if (RangeIndex == INDEX_NONE)
				{
					return Result->ParameterValueName == ResultCommited->ParameterValueName;					
				}
				else
				{
					if (Result->ParameterRangeValueNames.IsValidIndex(RangeIndex) && ResultCommited->ParameterRangeValueNames.IsValidIndex(RangeIndex))
					{
						return Result->ParameterRangeValueNames[RangeIndex] == ResultCommited->ParameterRangeValueNames[RangeIndex];
					}
					else
					{
						return Result->ParameterRangeValueNames.Num() != ResultCommited->ParameterRangeValueNames.Num();
					}
				}
			}
			else
			{
				return Result != ResultCommited;
			}
		}
		
	case EMutableParameterType::Float:
		{
			const FCustomizableObjectFloatParameterValue* Result = Descriptor.GetFloatParameters().FindByPredicate([&](const FCustomizableObjectFloatParameterValue& Value)
				{
					return Value.ParameterName == ParamName;
				});
			
			const FCustomizableObjectFloatParameterValue* ResultCommited = GetPrivate()->CommittedDescriptor.GetFloatParameters().FindByPredicate([&](const FCustomizableObjectFloatParameterValue& Value)
				{
					return Value.ParameterName == ParamName;
				});
			
			if (Result && ResultCommited)
			{
				if (RangeIndex == INDEX_NONE)
				{
					return Result->ParameterValue == ResultCommited->ParameterValue;					
				}
				else
				{
					if (Result->ParameterRangeValues.IsValidIndex(RangeIndex) && ResultCommited->ParameterRangeValues.IsValidIndex(RangeIndex))
					{
						return Result->ParameterRangeValues[RangeIndex] == ResultCommited->ParameterRangeValues[RangeIndex];
					}
					else
					{
						return Result->ParameterRangeValues.Num() != ResultCommited->ParameterRangeValues.Num();
					}
				}
			}
			else
			{
				return Result != ResultCommited;
			}
		}
		
	case EMutableParameterType::Color:
		{
			const FCustomizableObjectVectorParameterValue* Result = Descriptor.GetVectorParameters().FindByPredicate([&](const FCustomizableObjectVectorParameterValue& Value)
				{
					return Value.ParameterName == ParamName;
				});
			
			const FCustomizableObjectVectorParameterValue* ResultCommited = GetPrivate()->CommittedDescriptor.GetVectorParameters().FindByPredicate([&](const FCustomizableObjectVectorParameterValue& Value)
				{
					return Value.ParameterName == ParamName;
				});
			
			if (Result && ResultCommited)
			{
				if (RangeIndex == INDEX_NONE)
				{
					return Result->ParameterValue == ResultCommited->ParameterValue;					
				}
				else
				{
					return false;
				}
			}
			else
			{
				return Result != ResultCommited;
			}
		}
		
	default:
		unimplemented();
		return false;
	}
}


void UCustomizableInstancePrivate::PostEditChangePropertyWithoutEditor()
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::PostEditChangePropertyWithoutEditor);

	for (TTuple<FName, TObjectPtr<USkeletalMesh>>& Tuple : SkeletalMeshes)
	{
		USkeletalMesh* SkeletalMesh = Tuple.Get<1>();
		
		if (SkeletalMesh && SkeletalMesh->GetResourceForRendering() && !SkeletalMesh->GetResourceForRendering()->IsInitialized())
		{
			MUTABLE_CPUPROFILER_SCOPE(InitResources);

			// reinitialize resources
			SkeletalMesh->InitResources();
		}
	}
}


bool UCustomizableInstancePrivate::CanUpdateInstance() const
{
	UCustomizableObject* Object = GetPublic()->GetCustomizableObject();
	if (!Object)
	{
		return false;
	}

#if WITH_EDITOR
	if (Object->GetPrivate()->IsLocked())
	{
		return false;
	}

	if (!Object->IsCompiled())
	{
		return false;
	}

	return true;
	
#else
	return Object->IsCompiled();
#endif
}


void UCustomizableObjectInstance::UpdateSkeletalMeshAsync(bool bIgnoreCloseDist, bool bForceHighPriority)
{
	UCustomizableObjectSystemPrivate* SystemPrivate = UCustomizableObjectSystem::GetInstance()->GetPrivate();

	const TSharedRef<FUpdateContextPrivate> Context = MakeShared<FUpdateContextPrivate>(*this);
	Context->bIgnoreCloseDist = bIgnoreCloseDist;
	Context->bForceHighPriority = bForceHighPriority;
	
	SystemPrivate->EnqueueUpdateSkeletalMesh(Context);
}


void UCustomizableObjectInstance::UpdateSkeletalMeshAsyncResult(FInstanceUpdateDelegate Callback, bool bIgnoreCloseDist, bool bForceHighPriority)
{
	UCustomizableObjectSystemPrivate* SystemPrivate = UCustomizableObjectSystem::GetInstance()->GetPrivate();

	const TSharedRef<FUpdateContextPrivate> Context = MakeShared<FUpdateContextPrivate>(*this);
	Context->bIgnoreCloseDist = bIgnoreCloseDist;
	Context->bForceHighPriority = bForceHighPriority;
	Context->UpdateCallback = Callback;
	
	SystemPrivate->EnqueueUpdateSkeletalMesh(Context);
}


void UCustomizableObjectInstance::UpdateSkeletalMeshAsyncResult(FInstanceUpdateNativeDelegate Callback, bool bIgnoreCloseDist, bool bForceHighPriority)
{
	UCustomizableObjectSystemPrivate* SystemPrivate = UCustomizableObjectSystem::GetInstance()->GetPrivate();

	const TSharedRef<FUpdateContextPrivate> Context = MakeShared<FUpdateContextPrivate>(*this);
	Context->bIgnoreCloseDist = bIgnoreCloseDist;
	Context->bForceHighPriority = bForceHighPriority;
	Context->UpdateNativeCallback = Callback;
	
	SystemPrivate->EnqueueUpdateSkeletalMesh(Context);
}


void UCustomizableInstancePrivate::TickUpdateCloseCustomizableObjects(UCustomizableObjectInstance& Public, FMutableInstanceUpdateMap& InOutRequestedUpdates)
{
	UCustomizableObject* Object = Public.GetCustomizableObject();
	if (!Object)
	{
		return;
	}

#if WITH_EDITOR
	if (!Object->IsCompiled() &&
		Object->GetPrivate()->CompilationResult != ECompilationResultPrivate::Errors) // Avoid constantly retry failed compilations.
	{
		if (ICustomizableObjectEditorModule* EditorModule = ICustomizableObjectEditorModule::Get())
		{
			EditorModule->CompileCustomizableObject(*Object, nullptr, true, false);
		}
	}
#endif
		
	if (!CanUpdateInstance())
	{
		return;
	}

	const UCustomizableObjectSystemPrivate* SystemPrivate = UCustomizableObjectSystem::GetInstance()->GetPrivate();	

	const EUpdateRequired UpdateRequired = SystemPrivate->IsUpdateRequired(Public, true, true, false);
	if (UpdateRequired != EUpdateRequired::NoUpdate) // Since this is done in the tick, avoid starting an update that we know for sure that would not be performed. Once started it has some performance implications that we want to avoid.
	{
		if (UpdateRequired == EUpdateRequired::Discard)
		{
			UCustomizableObjectSystem::GetInstance()->GetPrivate()->InitDiscardResourcesSkeletalMesh(&Public);
			InOutRequestedUpdates.Remove(&Public);
		}
		else if (UpdateRequired == EUpdateRequired::Update)
		{
			const EQueuePriorityType Priority = SystemPrivate->GetUpdatePriority(Public, false);

			FMutableUpdateCandidate* UpdateCandidate = InOutRequestedUpdates.Find(&Public);

			if (UpdateCandidate)
			{
				ensure(HasCOInstanceFlags(PendingLODsUpdate | PendingLODsDowngrade));

				UpdateCandidate->Priority = Priority;
				UpdateCandidate->Issue();
			}
			else
			{
				FMutableUpdateCandidate Candidate(&Public);
				Candidate.Priority = Priority;
				Candidate.Issue();
				InOutRequestedUpdates.Add(&Public, Candidate);
			}
		}
		else
		{
			check(false);
		}
	}
	else
	{
		InOutRequestedUpdates.Remove(&Public);
	}

	ClearCOInstanceFlags(PendingLODsUpdate | PendingLODsDowngrade);
}


void UCustomizableInstancePrivate::UpdateInstanceIfNotGenerated(UCustomizableObjectInstance& Public, FMutableInstanceUpdateMap& InOutRequestedUpdates)
{
	if (SkeletalMeshStatus != ESkeletalMeshStatus::NotGenerated)
	{
		return;
	}

	if (!CanUpdateInstance())
	{
		return;
	}

	UCustomizableObjectSystemPrivate* SystemPrivate = UCustomizableObjectSystem::GetInstance()->GetPrivate();

	const TSharedRef<FUpdateContextPrivate> Context = MakeShared<FUpdateContextPrivate>(Public);
	Context->bOnlyUpdateIfNotGenerated = true;
	
	SystemPrivate->EnqueueUpdateSkeletalMesh(Context);

	EQueuePriorityType Priority = SystemPrivate->GetUpdatePriority(Public, false);
	FMutableUpdateCandidate* UpdateCandidate = InOutRequestedUpdates.Find(&Public);

	if (UpdateCandidate)
	{
		UpdateCandidate->Priority = Priority;
		UpdateCandidate->Issue();
	}
	else
	{
		FMutableUpdateCandidate Candidate(&Public);
		Candidate.Priority = Priority;
		Candidate.Issue();
		InOutRequestedUpdates.Add(&Public, Candidate);
	}
}


#if !UE_BUILD_SHIPPING
bool AreSkeletonsCompatible(const TArray<TObjectPtr<USkeleton>>& InSkeletons)
{
	MUTABLE_CPUPROFILER_SCOPE(AreSkeletonsCompatible);

	if (InSkeletons.IsEmpty())
	{
		return true;
	}

	bool bCompatible = true;

	struct FBoneToMergeInfo
	{
		FBoneToMergeInfo(const uint32 InBonePathHash, const uint32 InSkeletonIndex, const uint32 InParentBoneSkeletonIndex) :
		BonePathHash(InBonePathHash), SkeletonIndex(InSkeletonIndex), ParentBoneSkeletonIndex(InParentBoneSkeletonIndex)
		{}

		uint32 BonePathHash = 0;
		uint32 SkeletonIndex = 0;
		uint32 ParentBoneSkeletonIndex = 0;
	};

	// Accumulated hierarchy hash from parent-bone to root bone
	TMap<FName, FBoneToMergeInfo> BoneNamesToBoneInfo;
	BoneNamesToBoneInfo.Reserve(InSkeletons[0] ? InSkeletons[0]->GetReferenceSkeleton().GetNum() : 0);
	
	for (int32 SkeletonIndex = 0; SkeletonIndex < InSkeletons.Num(); ++SkeletonIndex)
	{
		const TObjectPtr<USkeleton> Skeleton = InSkeletons[SkeletonIndex];
		check(Skeleton);

		const FReferenceSkeleton& ReferenceSkeleton = Skeleton->GetReferenceSkeleton();
		const TArray<FMeshBoneInfo>& Bones = ReferenceSkeleton.GetRawRefBoneInfo();
		const TArray<FTransform>& BonePoses = ReferenceSkeleton.GetRawRefBonePose();

		const int32 NumBones = Bones.Num();
		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			const FMeshBoneInfo& Bone = Bones[BoneIndex];

			// Retrieve parent bone name and respective hash, root-bone is assumed to have a parent hash of 0
			const FName ParentName = Bone.ParentIndex != INDEX_NONE ? Bones[Bone.ParentIndex].Name : NAME_None;
			const uint32 ParentHash = Bone.ParentIndex != INDEX_NONE ? GetTypeHash(ParentName) : 0;

			// Look-up the path-hash from root to the parent bone
			const FBoneToMergeInfo* ParentBoneInfo = BoneNamesToBoneInfo.Find(ParentName);
			const uint32 ParentBonePathHash = ParentBoneInfo ? ParentBoneInfo->BonePathHash : 0;
			const uint32 ParentBoneSkeletonIndex = ParentBoneInfo ? ParentBoneInfo->SkeletonIndex : 0;

			// Append parent hash to path to give full path hash to current bone
			const uint32 BonePathHash = HashCombine(ParentBonePathHash, ParentHash);

			// Check if the bone exists in the hierarchy 
			const FBoneToMergeInfo* ExistingBoneInfo = BoneNamesToBoneInfo.Find(Bone.Name);

			// If the hash differs from the existing one it means skeletons are incompatible
			if (!ExistingBoneInfo)
			{
				// Add path hash to current bone
				BoneNamesToBoneInfo.Add(Bone.Name, FBoneToMergeInfo(BonePathHash, SkeletonIndex, ParentBoneSkeletonIndex));
			}
			else if (ExistingBoneInfo->BonePathHash != BonePathHash)
			{
				if (bCompatible)
				{
					// Print the skeletons to merge
					FString Msg = TEXT("Failed to merge skeletons. Skeletons to merge: ");
					for (int32 AuxSkeletonIndex = 0; AuxSkeletonIndex < InSkeletons.Num(); ++AuxSkeletonIndex)
					{
						if (InSkeletons[AuxSkeletonIndex] != nullptr)
						{
							Msg += FString::Printf(TEXT("\n\t- %s"), *InSkeletons[AuxSkeletonIndex].GetName());
						}
					}

					UE_LOG(LogMutable, Error, TEXT("%s"), *Msg);

#if WITH_EDITOR
					FNotificationInfo Info(FText::FromString(TEXT("Mutable: Failed to merge skeletons. Invalid parent chain detected. Please check the output log for more information.")));
					Info.bFireAndForget = true;
					Info.FadeOutDuration = 1.0f;
					Info.ExpireDuration = 10.0f;
					FSlateNotificationManager::Get().AddNotification(Info);
#endif

					bCompatible = false;
				}
				
				// Print the first non compatible bone in the bone chain, since all child bones will be incompatible too.
				if (ExistingBoneInfo->ParentBoneSkeletonIndex != SkeletonIndex)
				{
					// Different skeletons can't be used if they are incompatible with the reference skeleton.
					UE_LOG(LogMutable, Error, TEXT("[%s] parent bone is different in skeletons [%s] and [%s]."),
						*Bone.Name.ToString(),
						*InSkeletons[SkeletonIndex]->GetName(),
						*InSkeletons[ExistingBoneInfo->ParentBoneSkeletonIndex]->GetName());
				}
			}
		}
	}

	return bCompatible;
}
#endif


USkeleton* UCustomizableInstancePrivate::MergeSkeletons(UCustomizableObject& CustomizableObject, const FMutableRefSkeletalMeshData& RefSkeletalMeshData, FCustomizableObjectComponentIndex ObjectComponentIndex, bool& bOutCreatedNewSkeleton)
{
	MUTABLE_CPUPROFILER_SCOPE(BuildSkeletonData_MergeSkeletons);
	bOutCreatedNewSkeleton = false;

	FCustomizableInstanceComponentData* ComponentData = GetComponentData(ObjectComponentIndex);
	check(ComponentData);

	FReferencedSkeletons& ReferencedSkeletons = ComponentData->Skeletons;
	
	// Merged skeleton found in the cache
	if (ReferencedSkeletons.Skeleton)
	{
		USkeleton* MergedSkeleton = ReferencedSkeletons.Skeleton;
		ReferencedSkeletons.Skeleton = nullptr;
		return MergedSkeleton;
	}

	// No need to merge skeletons
	if(ReferencedSkeletons.SkeletonsToMerge.Num() == 1)
	{
		const TObjectPtr<USkeleton> RefSkeleton = ReferencedSkeletons.SkeletonsToMerge[0];
		ReferencedSkeletons.SkeletonIds.Empty();
		ReferencedSkeletons.SkeletonsToMerge.Empty();
		return RefSkeleton;
	}

#if !UE_BUILD_SHIPPING
	// Test Skeleton compatibility before attempting the merge to avoid a crash.
	if (!AreSkeletonsCompatible(ReferencedSkeletons.SkeletonsToMerge))
	{
		return nullptr;
	}
#endif

	FSkeletonMergeParams Params;
	Params.SkeletonsToMerge = ReferencedSkeletons.SkeletonsToMerge;

	USkeleton* FinalSkeleton = USkeletalMergingLibrary::MergeSkeletons(Params);
	if (!FinalSkeleton)
	{
		FString Msg = FString::Printf(TEXT("MergeSkeletons failed for Customizable Object [%s], Instance [%s]. Skeletons involved: "),
			*CustomizableObject.GetName(),
			*GetOuter()->GetName());
		
		const int32 SkeletonCount = Params.SkeletonsToMerge.Num();
		for (int32 SkeletonIndex = 0; SkeletonIndex < SkeletonCount; ++SkeletonIndex)
		{
			Msg += FString::Printf(TEXT(" [%s]"), *Params.SkeletonsToMerge[SkeletonIndex]->GetName());
		}
		
		UE_LOG(LogMutable, Error, TEXT("%s"), *Msg);
	}
	else
	{
#if WITH_EDITOR
		uint32 CombinedSkeletonHash = INDEX_NONE;
#endif
		
		// Make the final skeleton compatible with all the merged skeletons and their compatible skeletons.
		for (USkeleton* Skeleton : Params.SkeletonsToMerge)
		{
			if (Skeleton)
			{
				FinalSkeleton->AddCompatibleSkeleton(Skeleton);

				const TArray<TSoftObjectPtr<USkeleton>>& CompatibleSkeletons = Skeleton->GetCompatibleSkeletons();
				for (const TSoftObjectPtr<USkeleton>& CompatibleSkeleton : CompatibleSkeletons)
				{
					FinalSkeleton->AddCompatibleSkeletonSoft(CompatibleSkeleton);
				}

#if WITH_EDITOR
				const uint32 SkeletonHash = GetTypeHash(Skeleton->GetName());
				CombinedSkeletonHash = HashCombine(CombinedSkeletonHash, SkeletonHash);
#endif
			}
		}

		// Add the hash based on the sources for the merged skeleton to make its name unique
#if WITH_EDITOR
		FinalSkeleton->Rename(*FString::Printf(TEXT("%s_%lu"), *FinalSkeleton->GetName(), CombinedSkeletonHash));
#endif
		
		// Add Skeleton to the cache
		CustomizableObject.GetPrivate()->SkeletonCache.Add(ReferencedSkeletons.SkeletonIds, FinalSkeleton);
		ReferencedSkeletons.SkeletonIds.Empty();

		bOutCreatedNewSkeleton = true;
	}
	
	return FinalSkeleton;
}

namespace
{
	FORCEINLINE TObjectPtr<UPhysicsConstraintTemplate> ClonePhysicsConstraintTemplate(
			const TObjectPtr<UPhysicsConstraintTemplate>& From, 
			const TObjectPtr<UObject>& Outer,
			FName Name = NAME_None)
	{
		// We don't use DuplicateObject here beacuse it is too slow.
		TObjectPtr<UPhysicsConstraintTemplate> Result = NewObject<UPhysicsConstraintTemplate>(Outer, Name);
		
		Result->DefaultInstance = From->DefaultInstance;
		Result->ProfileHandles = From->ProfileHandles;
#if WITH_EDITOR
		Result->SetDefaultProfile(From->DefaultInstance);
#endif

		return Result;
	}

	FKAggregateGeom MakeAggGeomFromMutablePhysics(int32 BodyIndex, const mu::FPhysicsBody* MutablePhysicsBody)
	{
		FKAggregateGeom BodyAggGeom;

		auto GetCollisionEnabledFormFlags = [](uint32 Flags) -> ECollisionEnabled::Type
		{
			return ECollisionEnabled::Type(Flags & 0xFF);
		};

		auto GetContributeToMassFromFlags = [](uint32 Flags) -> bool
		{
			return static_cast<bool>((Flags >> 8) & 1);
		};

		const int32 NumSpheres = MutablePhysicsBody->GetSphereCount(BodyIndex);
		TArray<FKSphereElem>& AggSpheres = BodyAggGeom.SphereElems;
		AggSpheres.Empty(NumSpheres);
		for (int32 I = 0; I < NumSpheres; ++I)
		{
			uint32 Flags = MutablePhysicsBody->GetSphereFlags(BodyIndex, I);
			FString Name = MutablePhysicsBody->GetSphereName(BodyIndex, I);

			FVector3f Position;
			float Radius;

			MutablePhysicsBody->GetSphere(BodyIndex, I, Position, Radius);
			FKSphereElem& NewElem = AggSpheres.AddDefaulted_GetRef();
			
			NewElem.Center = FVector(Position);
			NewElem.Radius = Radius;
			NewElem.SetContributeToMass(GetContributeToMassFromFlags(Flags));
			NewElem.SetCollisionEnabled(GetCollisionEnabledFormFlags(Flags));
			NewElem.SetName(FName(*Name));
		}
		
		const int32 NumBoxes = MutablePhysicsBody->GetBoxCount(BodyIndex);
		TArray<FKBoxElem>& AggBoxes = BodyAggGeom.BoxElems;
		AggBoxes.Empty(NumBoxes);
		for (int32 I = 0; I < NumBoxes; ++I)
		{
			uint32 Flags = MutablePhysicsBody->GetBoxFlags(BodyIndex, I);
			FString Name = MutablePhysicsBody->GetBoxName(BodyIndex, I);

			FVector3f Position;
			FQuat4f Orientation
				;
			FVector3f Size;
			MutablePhysicsBody->GetBox(BodyIndex, I, Position, Orientation, Size);

			FKBoxElem& NewElem = AggBoxes.AddDefaulted_GetRef();
			
			NewElem.Center = FVector(Position);
			NewElem.Rotation = FRotator(Orientation.Rotator());
			NewElem.X = Size.X;
			NewElem.Y = Size.Y;
			NewElem.Z = Size.Z;
			NewElem.SetContributeToMass(GetContributeToMassFromFlags(Flags));
			NewElem.SetCollisionEnabled(GetCollisionEnabledFormFlags(Flags));
			NewElem.SetName(FName(*Name));
		}

		//const int32 NumConvexes = MutablePhysicsBody->GetConvexCount( BodyIndex );
		//TArray<FKConvexElem>& AggConvexes = BodyAggGeom.ConvexElems;
		//AggConvexes.Empty();
		//for (int32 I = 0; I < NumConvexes; ++I)
		//{
		//	uint32 Flags = MutablePhysicsBody->GetConvexFlags( BodyIndex, I );
		//	FString Name = MutablePhysicsBody->GetConvexName( BodyIndex, I );

		//	const FVector3f* Vertices;
		//	const int32* Indices;
		//	int32 NumVertices;
		//	int32 NumIndices;
		//	FTransform3f Transform;

		//	MutablePhysicsBody->GetConvex( BodyIndex, I, Vertices, NumVertices, Indices, NumIndices, Transform );
		//	
		//	TArrayView<const FVector3f> VerticesView( Vertices, NumVertices );
		//	TArrayView<const int32> IndicesView( Indices, NumIndices );
		//}

		TArray<FKSphylElem>& AggSphyls = BodyAggGeom.SphylElems;
		const int32 NumSphyls = MutablePhysicsBody->GetSphylCount(BodyIndex);
		AggSphyls.Empty(NumSphyls);

		for (int32 I = 0; I < NumSphyls; ++I)
		{
			uint32 Flags = MutablePhysicsBody->GetSphylFlags(BodyIndex, I);
			FString Name = MutablePhysicsBody->GetSphylName(BodyIndex, I);

			FVector3f Position;
			FQuat4f Orientation;
			float Radius;
			float Length;

			MutablePhysicsBody->GetSphyl(BodyIndex, I, Position, Orientation, Radius, Length);

			FKSphylElem& NewElem = AggSphyls.AddDefaulted_GetRef();
			
			NewElem.Center = FVector(Position);
			NewElem.Rotation = FRotator(Orientation.Rotator());
			NewElem.Radius = Radius;
			NewElem.Length = Length;
			
			NewElem.SetContributeToMass(GetContributeToMassFromFlags(Flags));
			NewElem.SetCollisionEnabled(GetCollisionEnabledFormFlags(Flags));
			NewElem.SetName(FName(*Name));
		}	

		TArray<FKTaperedCapsuleElem>& AggTaperedCapsules = BodyAggGeom.TaperedCapsuleElems;
		const int32 NumTaperedCapsules = MutablePhysicsBody->GetTaperedCapsuleCount(BodyIndex);
		AggTaperedCapsules.Empty(NumTaperedCapsules);

		for (int32 I = 0; I < NumTaperedCapsules; ++I)
		{
			uint32 Flags = MutablePhysicsBody->GetTaperedCapsuleFlags(BodyIndex, I);
			FString Name = MutablePhysicsBody->GetTaperedCapsuleName(BodyIndex, I);

			FVector3f Position;
			FQuat4f Orientation;
			float Radius0;
			float Radius1;
			float Length;

			MutablePhysicsBody->GetTaperedCapsule(BodyIndex, I, Position, Orientation, Radius0, Radius1, Length);

			FKTaperedCapsuleElem& NewElem = AggTaperedCapsules.AddDefaulted_GetRef();
			
			NewElem.Center = FVector(Position);
			NewElem.Rotation = FRotator(Orientation.Rotator());
			NewElem.Radius0 = Radius0;
			NewElem.Radius1 = Radius1;
			NewElem.Length = Length;
			
			NewElem.SetContributeToMass(GetContributeToMassFromFlags(Flags));
			NewElem.SetCollisionEnabled(GetCollisionEnabledFormFlags(Flags));
			NewElem.SetName(FName(*Name));
		}	

		return BodyAggGeom;
	};

	TObjectPtr<UPhysicsAsset> MakePhysicsAssetFromTemplateAndMutableBody(
		const TSharedRef<FUpdateContextPrivate>& OperationData,
		TObjectPtr<UPhysicsAsset> TemplateAsset,
		const mu::FPhysicsBody* MutablePhysics,
		FCustomizableObjectInstanceComponentIndex InstanceComponentIndex)
	{
		check(TemplateAsset);
		TObjectPtr<UPhysicsAsset> Result = NewObject<UPhysicsAsset>();

		if (!Result)
		{
			return nullptr;
		}

		Result->SolverSettings = TemplateAsset->SolverSettings;
		Result->SolverType = TemplateAsset->SolverType;

		Result->bNotForDedicatedServer = TemplateAsset->bNotForDedicatedServer;

		const TMap<mu::FBoneName, TPair<FName, uint16>>& BoneInfoMap = OperationData->InstanceUpdateData.SkeletonsPerInstanceComponent[InstanceComponentIndex.GetValue()].BoneInfoMap;
		TMap<FName, int32> BonesInUse;

		const int32 MutablePhysicsBodyCount = MutablePhysics->GetBodyCount();
		BonesInUse.Reserve(MutablePhysicsBodyCount);
		for ( int32 I = 0; I < MutablePhysicsBodyCount; ++I )
		{
			if (const TPair<FName, uint16>* BoneInfo = BoneInfoMap.Find(MutablePhysics->GetBodyBoneId(I)))
			{
				BonesInUse.Add(BoneInfo->Key, I);
			}
		}

		const int32 PhysicsAssetBodySetupNum = TemplateAsset->SkeletalBodySetups.Num();
		bool bTemplateBodyNotUsedFound = false;

		TArray<uint8> UsageMap;
		UsageMap.Init(1, PhysicsAssetBodySetupNum);

		for (int32 BodySetupIndex = 0; BodySetupIndex < PhysicsAssetBodySetupNum; ++BodySetupIndex)
		{
			const TObjectPtr<USkeletalBodySetup>& BodySetup = TemplateAsset->SkeletalBodySetups[BodySetupIndex];

			int32* MutableBodyIndex = BonesInUse.Find(BodySetup->BoneName);
			if (!MutableBodyIndex)
			{
				bTemplateBodyNotUsedFound = true;
				UsageMap[BodySetupIndex] = 0;
				continue;
			}

			TObjectPtr<USkeletalBodySetup> NewBodySetup = NewObject<USkeletalBodySetup>(Result);
			NewBodySetup->BodySetupGuid = FGuid::NewGuid();

			// Copy Body properties 	
			NewBodySetup->BoneName = BodySetup->BoneName;
			NewBodySetup->PhysicsType = BodySetup->PhysicsType;
			NewBodySetup->bConsiderForBounds = BodySetup->bConsiderForBounds;
			NewBodySetup->bMeshCollideAll = BodySetup->bMeshCollideAll;
			NewBodySetup->bDoubleSidedGeometry = BodySetup->bDoubleSidedGeometry;
			NewBodySetup->bGenerateNonMirroredCollision = BodySetup->bGenerateNonMirroredCollision;
			NewBodySetup->bSharedCookedData = BodySetup->bSharedCookedData;
			NewBodySetup->bGenerateMirroredCollision = BodySetup->bGenerateMirroredCollision;
			NewBodySetup->PhysMaterial = BodySetup->PhysMaterial;
			NewBodySetup->CollisionReponse = BodySetup->CollisionReponse;
			NewBodySetup->CollisionTraceFlag = BodySetup->CollisionTraceFlag;
			NewBodySetup->DefaultInstance = BodySetup->DefaultInstance;
			NewBodySetup->WalkableSlopeOverride = BodySetup->WalkableSlopeOverride;
			NewBodySetup->BuildScale3D = BodySetup->BuildScale3D;
			NewBodySetup->bSkipScaleFromAnimation = BodySetup->bSkipScaleFromAnimation;

			// PhysicalAnimationProfiles can't be added with the current UPhysicsAsset API outside the editor.
			// Don't pouplate them for now.	
			//NewBodySetup->PhysicalAnimationData = BodySetup->PhysicalAnimationData;

			NewBodySetup->AggGeom = MakeAggGeomFromMutablePhysics(*MutableBodyIndex, MutablePhysics);

			Result->SkeletalBodySetups.Add(NewBodySetup);
		}

		if (!bTemplateBodyNotUsedFound)
		{
			Result->CollisionDisableTable = TemplateAsset->CollisionDisableTable;

			const int32 NumConstraints = TemplateAsset->ConstraintSetup.Num();
			Result->ConstraintSetup.SetNum(NumConstraints);	
	
			for (int32 ConstraintIndex = 0; ConstraintIndex < NumConstraints; ++ConstraintIndex)
			{
				const TObjectPtr<UPhysicsConstraintTemplate>& TemplateConstraint = TemplateAsset->ConstraintSetup[ConstraintIndex];

				if (!TemplateConstraint)
				{
					continue;
				}

				Result->ConstraintSetup[ConstraintIndex] = ClonePhysicsConstraintTemplate(TemplateConstraint, Result);
			}
		}
		else
		{
			// Recreate the collision disable entry
			Result->CollisionDisableTable.Reserve(TemplateAsset->CollisionDisableTable.Num());
			for (const TPair<FRigidBodyIndexPair, bool>& CollisionDisableEntry : TemplateAsset->CollisionDisableTable)
			{
				const bool bIndex0Used = UsageMap[CollisionDisableEntry.Key.Indices[0]] > 0;
				const bool bIndex1Used = UsageMap[CollisionDisableEntry.Key.Indices[1]] > 0;

				if (bIndex0Used && bIndex1Used)
				{
					Result->CollisionDisableTable.Add(CollisionDisableEntry);
				}
			}

			// Only add constraints that are part of the bones used for the mutable physics volumes description.
			Result->ConstraintSetup.Reserve(TemplateAsset->ConstraintSetup.Num());
			for (const TObjectPtr<UPhysicsConstraintTemplate>& Constraint : TemplateAsset->ConstraintSetup)
			{
				if (!Constraint)
				{
					continue;
				}

				const FName BoneA = Constraint->DefaultInstance.ConstraintBone1;
				const FName BoneB = Constraint->DefaultInstance.ConstraintBone2;

				if (BonesInUse.Find(BoneA) && BonesInUse.Find(BoneB))
				{
					Result->ConstraintSetup.AddDefaulted_GetRef() = ClonePhysicsConstraintTemplate(Constraint, Result);
				}	
			}
		}

		Result->UpdateBodySetupIndexMap();
		Result->UpdateBoundsBodiesArray();

#if WITH_EDITORONLY_DATA
		Result->ConstraintProfiles = TemplateAsset->ConstraintProfiles;
#endif

		return Result;
	}
}

UPhysicsAsset* UCustomizableInstancePrivate::GetOrBuildMainPhysicsAsset(
	const TSharedRef<FUpdateContextPrivate>& OperationData,
	TObjectPtr<UPhysicsAsset> TemplateAsset,
	const mu::FPhysicsBody* MutablePhysics,
	bool bDisableCollisionsBetweenDifferentAssets,
	FCustomizableObjectInstanceComponentIndex InstanceComponentIndex)
{

	MUTABLE_CPUPROFILER_SCOPE(MergePhysicsAssets);

	check(MutablePhysics);

	UPhysicsAsset* Result = nullptr;

	const FInstanceUpdateData::FComponent* Component = OperationData->GetComponentUpdateData(InstanceComponentIndex);
	if (!Component)
	{
		return nullptr;
	}

	FCustomizableObjectComponentIndex ObjectComponentIndex = Component->Id;
	FCustomizableInstanceComponentData* ComponentData = GetComponentData(ObjectComponentIndex);
	check(ComponentData);

	TArray<TObjectPtr<UPhysicsAsset>>& PhysicsAssets = ComponentData->PhysicsAssets.PhysicsAssetsToMerge;

	TArray<TObjectPtr<UPhysicsAsset>> ValidAssets;

	const int32 NumPhysicsAssets = ComponentData->PhysicsAssets.PhysicsAssetsToMerge.Num();
	for (int32 I = 0; I < NumPhysicsAssets; ++I)
	{
		const TObjectPtr<UPhysicsAsset>& PhysicsAsset = ComponentData->PhysicsAssets.PhysicsAssetsToMerge[I];

		if (PhysicsAsset)
		{
			ValidAssets.AddUnique(PhysicsAsset);
		}
	}

	if (!ValidAssets.Num())
	{
		return Result;
	}

	// Just get the referenced asset if no recontrution or merge is needed.
	if (ValidAssets.Num() == 1 && !MutablePhysics->bBodiesModified)
	{
		return ValidAssets[0];
	}

	TemplateAsset = TemplateAsset ? TemplateAsset : ValidAssets[0];
	check(TemplateAsset);

	Result = NewObject<UPhysicsAsset>();

	if (!Result)
	{
		return nullptr;
	}

	Result->SolverSettings = TemplateAsset->SolverSettings;
	Result->SolverType = TemplateAsset->SolverType;

	Result->bNotForDedicatedServer = TemplateAsset->bNotForDedicatedServer;

	const TMap<mu::FBoneName, TPair<FName, uint16>>& BoneInfoMap = OperationData->InstanceUpdateData.SkeletonsPerInstanceComponent[InstanceComponentIndex.GetValue()].BoneInfoMap;
	TMap<FName, int32> BonesInUse;

	const int32 MutablePhysicsBodyCount = MutablePhysics->GetBodyCount();
	BonesInUse.Reserve(MutablePhysicsBodyCount);
	for ( int32 I = 0; I < MutablePhysicsBodyCount; ++I )
	{
		if (const TPair<FName, uint16>* BoneInfo = BoneInfoMap.Find(MutablePhysics->GetBodyBoneId(I)))
		{
			BonesInUse.Add(BoneInfo->Key, I);
		}
	}

	// Each array is a set of elements that can collide  
	TArray<TArray<int32, TInlineAllocator<8>>> CollisionSets;

	// {SetIndex, ElementInSetIndex, BodyIndex}
	using CollisionSetEntryType = TTuple<int32, int32, int32>;	
	// Map from BodyName/BoneName to set and index in set.
	TMap<FName, CollisionSetEntryType> BodySetupSetMap;
	
	// {SetIndex0, SetIndex1, BodyIndex}
	using MultiSetCollisionEnableType = TTuple<int32, int32, int32>;

	// Only for elements that belong to two or more differnet sets. 
	// Contains in which set the elements belong.
	using MultiSetArrayType = TArray<int32, TInlineAllocator<4>>;
	TMap<int32, MultiSetArrayType> MultiCollisionSets;
	TArray<TArray<int32>> SetsIndexMap;

	CollisionSets.SetNum(ValidAssets.Num());
	SetsIndexMap.SetNum(CollisionSets.Num());

	TMap<FRigidBodyIndexPair, bool> CollisionDisableTable;

	// New body index
	int32 CurrentBodyIndex = 0;
	for (int32 CollisionSetIndex = 0;  CollisionSetIndex < ValidAssets.Num(); ++CollisionSetIndex)
	{
		const int32 PhysicsAssetBodySetupNum = ValidAssets[CollisionSetIndex]->SkeletalBodySetups.Num();
		SetsIndexMap[CollisionSetIndex].Init(-1, PhysicsAssetBodySetupNum);

		for (int32 BodySetupIndex = 0; BodySetupIndex < PhysicsAssetBodySetupNum; ++BodySetupIndex) 
		{
			const TObjectPtr<USkeletalBodySetup>& BodySetup = ValidAssets[CollisionSetIndex]->SkeletalBodySetups[BodySetupIndex];
			
			int32* MutableBodyIndex = BonesInUse.Find(BodySetup->BoneName);
			if (!MutableBodyIndex)
			{
				continue;
			}

			CollisionSetEntryType* Found = BodySetupSetMap.Find(BodySetup->BoneName);

			if (!Found)
			{
				TObjectPtr<USkeletalBodySetup> NewBodySetup = NewObject<USkeletalBodySetup>(Result);
				NewBodySetup->BodySetupGuid = FGuid::NewGuid();
			
				// Copy Body properties 	
				NewBodySetup->BoneName = BodySetup->BoneName;
				NewBodySetup->PhysicsType = BodySetup->PhysicsType;
				NewBodySetup->bConsiderForBounds = BodySetup->bConsiderForBounds;
				NewBodySetup->bMeshCollideAll = BodySetup->bMeshCollideAll;
				NewBodySetup->bDoubleSidedGeometry = BodySetup->bDoubleSidedGeometry;
				NewBodySetup->bGenerateNonMirroredCollision = BodySetup->bGenerateNonMirroredCollision;
				NewBodySetup->bSharedCookedData = BodySetup->bSharedCookedData;
				NewBodySetup->bGenerateMirroredCollision = BodySetup->bGenerateMirroredCollision;
				NewBodySetup->PhysMaterial = BodySetup->PhysMaterial;
				NewBodySetup->CollisionReponse = BodySetup->CollisionReponse;
				NewBodySetup->CollisionTraceFlag = BodySetup->CollisionTraceFlag;
				NewBodySetup->DefaultInstance = BodySetup->DefaultInstance;
				NewBodySetup->WalkableSlopeOverride = BodySetup->WalkableSlopeOverride;
				NewBodySetup->BuildScale3D = BodySetup->BuildScale3D;	
				NewBodySetup->bSkipScaleFromAnimation = BodySetup->bSkipScaleFromAnimation;
				
				// PhysicalAnimationProfiles can't be added with the current UPhysicsAsset API outside the editor.
				// Don't poulate them for now.	
				//NewBodySetup->PhysicalAnimationData = BodySetup->PhysicalAnimationData;

				NewBodySetup->AggGeom = MakeAggGeomFromMutablePhysics(*MutableBodyIndex, MutablePhysics);


				Result->SkeletalBodySetups.Add(NewBodySetup);
				
				int32 IndexInSet = CollisionSets[CollisionSetIndex].Add(CurrentBodyIndex);
				BodySetupSetMap.Add(BodySetup->BoneName, {CollisionSetIndex, IndexInSet, CurrentBodyIndex});
				SetsIndexMap[CollisionSetIndex][IndexInSet] = CurrentBodyIndex;

				++CurrentBodyIndex;
			}
			else
			{
				int32 FoundCollisionSetIndex = Found->Get<0>();
				int32 FoundCollisionSetElemIndex = Found->Get<1>();
				int32 FoundBodyIndex = Found->Get<2>();
				
				// No need to add the body again. Volumes that come form mutable are already merged.
				// here we only need to merge properies.
				// TODO: check if there is other properties worth merging. In case of conflict select the more restrivtive one? 
				Result->SkeletalBodySetups[FoundBodyIndex]->bConsiderForBounds |= BodySetup->bConsiderForBounds;

				// Mark as removed so no indices are invalidated.
				CollisionSets[FoundCollisionSetIndex][FoundCollisionSetElemIndex] = INDEX_NONE;
				// Add Elem to the set but mark it as removed so we have an index for remapping.
				int32 IndexInSet = CollisionSets[CollisionSetIndex].Add(INDEX_NONE);	
				SetsIndexMap[CollisionSetIndex][IndexInSet] = FoundBodyIndex;
				
				MultiSetArrayType& Sets = MultiCollisionSets.FindOrAdd(FoundBodyIndex);

				// The first time there is a collision (MultSet is empty), add the colliding element set
				// as well as the current set.
				if (!Sets.Num())
				{
					Sets.Add(FoundCollisionSetIndex);
				}
				
				Sets.Add(CollisionSetIndex);
			}
		}
	
		// Remap collision indices removing invalid ones.
		CollisionDisableTable.Reserve(CollisionDisableTable.Num() + ValidAssets[CollisionSetIndex]->CollisionDisableTable.Num());
		for (const TPair<FRigidBodyIndexPair, bool>& DisabledCollision : ValidAssets[CollisionSetIndex]->CollisionDisableTable)
		{
			int32 MappedIdx0 = SetsIndexMap[CollisionSetIndex][DisabledCollision.Key.Indices[0]];
			int32 MappedIdx1 = SetsIndexMap[CollisionSetIndex][DisabledCollision.Key.Indices[1]];

			// This will generate correct disables for the case when two shapes from different sets
			// are meged to the same setup. Will introduce repeated pairs, but this is not a problem.

			// Currenly if two bodies / bones have disabled collision in one of the merged assets, the collision
			// will remain disabled even if other merges allow it.   
			if ( MappedIdx0 != INDEX_NONE && MappedIdx1 != INDEX_NONE )
			{
				CollisionDisableTable.Add({MappedIdx0, MappedIdx1}, DisabledCollision.Value);
			}
		}

		// Only add constraints that are part of the bones used for the mutable physics volumes description.
		Result->ConstraintSetup.Reserve(Result->ConstraintSetup.Num() + ValidAssets[CollisionSetIndex]->ConstraintSetup.Num());
		for (const TObjectPtr<UPhysicsConstraintTemplate>& Constraint : ValidAssets[CollisionSetIndex]->ConstraintSetup)
		{
			if (!Constraint)
			{
				continue;
			}

			FName BoneA = Constraint->DefaultInstance.ConstraintBone1;
			FName BoneB = Constraint->DefaultInstance.ConstraintBone2;

			if (BonesInUse.Find(BoneA) && BonesInUse.Find(BoneB))
			{
				Result->ConstraintSetup.AddDefaulted_GetRef() = ClonePhysicsConstraintTemplate(Constraint, Result); 
			}
		}

#if WITH_EDITORONLY_DATA
		Result->ConstraintProfiles.Append(ValidAssets[CollisionSetIndex]->ConstraintProfiles);
#endif
	}

	if (bDisableCollisionsBetweenDifferentAssets)
	{
		// Compute collision disable table size upperbound to reduce number of alloactions.
		int32 CollisionDisableTableSize = 0;
		for (int32 S0 = 1; S0 < CollisionSets.Num(); ++S0)
		{
			for (int32 S1 = 0; S1 < S0; ++S1)
			{	
				CollisionDisableTableSize += CollisionSets[S1].Num() * CollisionSets[S0].Num();
			}
		}

		// We already may have elements in the table, but at the moment of 
		// addition we don't know yet the final number of elements.
		// Now a good number of elements will be added and because we know the final number of elements
		// an upperbound to the number of interactions can be computed and reserved. 
		CollisionDisableTable.Reserve(CollisionDisableTableSize);

		// Generate disable collision entry for every element in Set S0 for every element in Set S1 
		// that are not in multiple sets.
		for (int32 S0 = 1; S0 < CollisionSets.Num(); ++S0)
		{
			for (int32 S1 = 0; S1 < S0; ++S1)
			{	
				for (int32 Set0Elem : CollisionSets[S0])
				{
					// Element present in more than one set, will be treated later.
					if (Set0Elem == INDEX_NONE)
					{
						continue;
					}

					for (int32 Set1Elem : CollisionSets[S1])
					{
						// Element present in more than one set, will be treated later.
						if (Set1Elem == INDEX_NONE)
						{
							continue;
						}
						CollisionDisableTable.Add(FRigidBodyIndexPair{Set0Elem, Set1Elem}, false);
					}
				}
			}
		}

		// Process elements that belong to multiple sets that have been merged to the same element.
		for ( const TPair<int32, MultiSetArrayType>& Sets : MultiCollisionSets )
		{
			for (int32 S = 0; S < CollisionSets.Num(); ++S)
			{
				if (!Sets.Value.Contains(S))
				{	
					for (int32 SetElem : CollisionSets[S])
					{
						if (SetElem != INDEX_NONE)
						{
							CollisionDisableTable.Add(FRigidBodyIndexPair{Sets.Key, SetElem}, false);
						}
					}
				}
			}
		}

		CollisionDisableTable.Shrink();
	}

	Result->CollisionDisableTable = MoveTemp(CollisionDisableTable);
	Result->UpdateBodySetupIndexMap();
	Result->UpdateBoundsBodiesArray();

	ComponentData->PhysicsAssets.PhysicsAssetsToMerge.Empty();

	return Result;
}


static float MutableMeshesMinUVChannelDensity = 100.f;
FAutoConsoleVariableRef CVarMutableMeshesMinUVChannelDensity(
	TEXT("Mutable.MinUVChannelDensity"),
	MutableMeshesMinUVChannelDensity,
	TEXT("Min UV density to set on generated meshes. This value will influence the requested texture mip to stream in. Higher values will result in higher quality mips being streamed in earlier."));


void SetMeshUVChannelDensity(FMeshUVChannelInfo& UVChannelInfo, float Density = 0.f)
{
	Density = Density > 0.f ? Density : 150.f;
	Density = FMath::Max(MutableMeshesMinUVChannelDensity, Density);

	UVChannelInfo.bInitialized = true;
	UVChannelInfo.bOverrideDensities = false;

	for (int32 i = 0; i < TEXSTREAM_MAX_NUM_UVCHANNELS; ++i)
	{
		UVChannelInfo.LocalUVDensities[i] = Density;
	}
}


bool UCustomizableInstancePrivate::DoComponentsNeedUpdate(UCustomizableObjectInstance* Public, const TSharedRef<FUpdateContextPrivate>& OperationData, bool& bHasInvalidMesh)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::DoComponentsNeedUpdate);

	UCustomizableObject* CustomizableObject = Public->GetCustomizableObject();
	
	if (!CustomizableObject)
	{
		return false;
	}

	const UModelResources& ModelResources = *CustomizableObject->GetPrivate()->GetModelResources();

	const int32 NumInstanceComponents = OperationData->InstanceUpdateData.Components.Num();

	// To be indexed with instance component index
	TArray<bool> ComponentWithMesh;
	ComponentWithMesh.Init(false, NumInstanceComponents);

	TArray<mu::FResourceID> MeshIDs;
	MeshIDs.Init(MAX_uint64, NumInstanceComponents * MAX_MESH_LOD_COUNT);

	// Gather the Mesh Ids of all components, and validate the integrity of the meshes to generate. 
	for (int32 InstanceComponentIndex = 0; InstanceComponentIndex < NumInstanceComponents; ++InstanceComponentIndex)
	{
		const FInstanceUpdateData::FComponent& Component = OperationData->InstanceUpdateData.Components[InstanceComponentIndex];
		const FName ComponentName = OperationData->ComponentNames[Component.Id.GetValue()];

		for (int32 LODIndex = OperationData->GetFirstRequestedLOD()[ComponentName]; LODIndex < Component.LODCount; ++LODIndex)
		{
			const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[Component.FirstLOD + LODIndex];

			if (!LOD.Mesh)
			{
				continue;
			}

			if (LOD.SurfaceCount == 0 && !LOD.Mesh->IsReference())
			{
				continue;
			}

			// Unreal does not support empty sections.
			if (!LOD.Mesh->IsReference() && LOD.Mesh->GetVertexCount() == 0) // else
			{
				UE_LOG(LogMutable, Error, TEXT("Failed to generate SkeletalMesh for CO Instance [%s]. CO [%s] has invalid geometry for LOD [%d] Component [%d]."),
					*Public->GetName(), *CustomizableObject->GetName(),
					LODIndex, NumInstanceComponents);
				bHasInvalidMesh = true;
				continue;
			}

			ComponentWithMesh[InstanceComponentIndex] = true;
			MeshIDs[InstanceComponentIndex * MAX_MESH_LOD_COUNT + LODIndex] = LOD.MeshID;
		}
	}

	// Find which components need an update
	OperationData->MeshChangedPerInstanceComponent.Init(false, NumInstanceComponents);

	for (int32 InstanceComponentIndex = 0; InstanceComponentIndex < NumInstanceComponents; ++InstanceComponentIndex)
	{
		const FInstanceUpdateData::FComponent& Component = OperationData->InstanceUpdateData.Components[InstanceComponentIndex];
		const FCustomizableObjectComponentIndex ObjectComponentIndex = Component.Id;
		if (!ObjectComponentIndex.IsValid())
		{
			continue;
		}

		const FName ComponentName = ModelResources.ComponentNamesPerObjectComponent[ObjectComponentIndex.GetValue()];

		if (OperationData->bUseMeshCache)
		{
			USkeletalMesh* CachedMesh = CustomizableObject->GetPrivate()->MeshCache.Get(*OperationData->GetMeshDescriptors(ObjectComponentIndex));
			if (CachedMesh)
			{
				TObjectPtr<USkeletalMesh>* SkeletalMesh = SkeletalMeshes.Find(ComponentName);
				const bool bMeshNeedsUpdate = !SkeletalMesh || (*SkeletalMesh != CachedMesh);
				OperationData->MeshChangedPerInstanceComponent[InstanceComponentIndex] = bMeshNeedsUpdate;
				ComponentWithMesh[InstanceComponentIndex] = true;
				continue;
			}
		}

		// Components with mesh must have valid geometry at CurrentMaxLOD

		TObjectPtr<USkeletalMesh>* SkeletalMeshPtr = SkeletalMeshes.Find(ComponentName);
		const bool bHadSkeletalMesh = SkeletalMeshPtr && *SkeletalMeshPtr;

		if (Component.LODCount == 0)
		{
			// We don't have a mesh in the component, so it has changed if we had one before.
			OperationData->MeshChangedPerInstanceComponent[InstanceComponentIndex] = bHadSkeletalMesh;
			continue;
		}

		const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[Component.FirstLOD];
		bool bIsReferenced = LOD.Mesh && LOD.Mesh->IsReference();
		if (!bIsReferenced)
		{
			if (ComponentWithMesh[InstanceComponentIndex] && MeshIDs[InstanceComponentIndex * MAX_MESH_LOD_COUNT + OperationData->NumLODsAvailable[ComponentName] - 1] == MAX_uint64)
			{
				UE_LOG(LogMutable, Error, TEXT("Failed to generate SkeletalMesh for CO Instance [%s]. CO [%s] is missing geometry for LOD [%d] Object Component [%d]."),
					*Public->GetName(), *CustomizableObject->GetName(),
					OperationData->NumLODsAvailable[ComponentName] - 1, ObjectComponentIndex.GetValue());
				bHasInvalidMesh = true;
				continue;
			}
		}

		// If the component wasn't there and now is there, we need to update it.
		OperationData->MeshChangedPerInstanceComponent[InstanceComponentIndex] = !bHadSkeletalMesh && LOD.Mesh && (LOD.Mesh->GetFaceCount()>0);

		const FCustomizableInstanceComponentData* ComponentData = GetComponentData(ObjectComponentIndex);
		if (!ComponentData) // Could be nullptr if the component has not been generated.
		{
			continue;
		}

		// Update if MeshIDs are different
		const int32 ComponentOffset = InstanceComponentIndex * MAX_MESH_LOD_COUNT;
		for (int32 MeshIndex = 0; !OperationData->MeshChangedPerInstanceComponent[InstanceComponentIndex] && MeshIndex < MAX_MESH_LOD_COUNT; ++MeshIndex)
		{
			OperationData->MeshChangedPerInstanceComponent[InstanceComponentIndex] = MeshIDs[ComponentOffset + MeshIndex] != ComponentData->LastMeshIdPerLOD[MeshIndex];
		}

		// If the component SkeletalMesh was build with scalability settings different than the current, we need to update.
		// The mismatch can happen when the scalability setting is changed and an update is requested with a MinLOD lower 
		// than the new FirstLODAvailable. This is an artifact of our LOD management system setting MinLOD.
		if (!OperationData->bStreamMeshLODs &&
			!OperationData->MeshChangedPerInstanceComponent[InstanceComponentIndex] &&
			bHadSkeletalMesh) 
		{
			const int32 FirstRequestedLOD = OperationData->GetFirstRequestedLOD()[ComponentName];
			for (int32 LODIndex = OperationData->FirstLODAvailable[ComponentName]; LODIndex < FirstRequestedLOD; ++LODIndex)
			{
				TIndirectArray<FSkeletalMeshLODRenderData>& LODRenderData = (*SkeletalMeshPtr)->GetResourceForRendering()->LODRenderData;
				if (LODRenderData[LODIndex].RenderSections.Num() != LODRenderData[FirstRequestedLOD].RenderSections.Num())
				{
					OperationData->MeshChangedPerInstanceComponent[InstanceComponentIndex] = true;
					break;
				}
			}
		}

	}

	bool bChanged =
		OperationData->MeshChangedPerInstanceComponent.Num() != SkeletalMeshes.Num()
		||
		OperationData->MeshChangedPerInstanceComponent.Find(true) != INDEX_NONE;

	// It also changed if we removed a component that we did have before
	if (!bChanged)
	{
		for (const TPair<FName, TObjectPtr<USkeletalMesh>>& OldMesh : SkeletalMeshes)
		{
			bool bFound = false;
			for (int32 InstanceComponentIndex = 0; InstanceComponentIndex < NumInstanceComponents; ++InstanceComponentIndex)
			{
				const FInstanceUpdateData::FComponent& Component = OperationData->InstanceUpdateData.Components[InstanceComponentIndex];
				const FCustomizableObjectComponentIndex ObjectComponentIndex = Component.Id;
				const FName ComponentName = ModelResources.ComponentNamesPerObjectComponent[ObjectComponentIndex.GetValue()];

				if (ComponentName == OldMesh.Key)
				{
					bFound = true;
					break;
				}
			}

			if (!bFound)
			{
				bChanged = true;
				break;
			}
		}
	}

	return !bHasInvalidMesh && bChanged;
}


bool UCustomizableInstancePrivate::UpdateSkeletalMesh_PostBeginUpdate0(UCustomizableObjectInstance* Public, const TSharedRef<FUpdateContextPrivate>& OperationData)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::UpdateSkeletalMesh_PostBeginUpdate0)

	bool bHasInvalidMesh = false;

	bool bUpdateMeshes = DoComponentsNeedUpdate(Public, OperationData, bHasInvalidMesh);

	UCustomizableObject* CustomizableObject = Public->GetCustomizableObject();

	if (!CustomizableObject)
	{
		UE_LOG(LogMutable, Warning, TEXT("Failed to generate SkeletalMesh for CO Instance %s. It does not have a CO."), *Public->GetName());
		
		InvalidateGeneratedData();
		OperationData->UpdateResult = EUpdateResult::Error;

		return false;
	}

	// We can not handle empty meshes, clear any generated mesh and return
	if (bHasInvalidMesh)
	{
		UE_LOG(LogMutable, Warning, TEXT("Failed to generate SkeletalMesh for CO Instance %s. CO [%s]"), *Public->GetName(), *GetNameSafe(CustomizableObject));

		InvalidateGeneratedData();
		OperationData->UpdateResult = EUpdateResult::Error;

		return false;
	}
	
	TextureReuseCache.Empty(); // Sections may have changed, so invalidate the texture reuse cache because it's indexed by section

	TMap<FName, TObjectPtr<USkeletalMesh>> OldSkeletalMeshes = SkeletalMeshes;

	const UModelResources& ModelResources = CustomizableObject->GetPrivate()->GetModelResourcesChecked();

	// Collate the Extension Data on the instance into groups based on the extension that produced
	// it, so that we only need to call extension functions such as OnSkeletalMeshCreated once for
	// each extension.
	TMap<const UCustomizableObjectExtension*, TArray<FInputPinDataContainer>> ExtensionToExtensionData;
	{
		const TArrayView<const UCustomizableObjectExtension* const> AllExtensions = ICustomizableObjectModule::Get().GetRegisteredExtensions();

		// Pre-populate ExtensionToExtensionData with empty entries for all extensions.
		//
		// This ensures that extension functions such as OnSkeletalMeshCreated are called for each
		// extension, even if they didn't produce any extension data.
		Public->GetPrivate()->ExtensionInstanceData.Empty(AllExtensions.Num());
		for (const UCustomizableObjectExtension* Extension : AllExtensions)
		{
			ExtensionToExtensionData.Add(Extension);
		}

		const TArrayView<const FRegisteredObjectNodeInputPin> ExtensionPins = ICustomizableObjectModule::Get().GetAdditionalObjectNodePins();

		for (FInstanceUpdateData::FNamedExtensionData& ExtensionOutput : OperationData->InstanceUpdateData.ExtendedInputPins)
		{
			const FRegisteredObjectNodeInputPin* FoundPin =
				Algo::FindBy(ExtensionPins, ExtensionOutput.Name, &FRegisteredObjectNodeInputPin::GlobalPinName);

			if (!FoundPin)
			{
				// Failed to find the corresponding pin for this output
				// 
				// This may indicate that a plugin has been removed or renamed since the CO was compiled
				UE_LOG(LogMutable, Error, TEXT("Failed to find Object node input pin with name %s"), *ExtensionOutput.Name.ToString());
				continue;
			}

			const UCustomizableObjectExtension* Extension = FoundPin->Extension.Get();
			if (!Extension)
			{
				// Extension is not loaded or not found
				UE_LOG(LogMutable, Error, TEXT("Extension for Object node input pin %s is no longer valid"), *ExtensionOutput.Name.ToString());
				continue;
			}

			if (ExtensionOutput.Data->Origin == mu::FExtensionData::EOrigin::Invalid)
			{
				// Null data was produced
				//
				// This can happen if a node produces an FExtensionData but doesn't initialize it
				UE_LOG(LogMutable, Error, TEXT("Invalid data sent to Object node input pin %s"), *ExtensionOutput.Name.ToString());
				continue;
			}

			// All registered extensions were added to the map above, so if the extension is still
			// registered it should be found.
			TArray<FInputPinDataContainer>* ContainerArray = ExtensionToExtensionData.Find(Extension);
			if (!ContainerArray)
			{
				UE_LOG(LogMutable, Error, TEXT("Object node input pin %s received data for unregistered extension %s"),
					*ExtensionOutput.Name.ToString(), *Extension->GetPathName());
				continue;
			}

			const FCustomizableObjectResourceData* ReferencedExtensionData = nullptr;
			switch (ExtensionOutput.Data->Origin)
			{
				case mu::FExtensionData::EOrigin::ConstantAlwaysLoaded:
				{
					check(ModelResources.AlwaysLoadedExtensionData.IsValidIndex(ExtensionOutput.Data->Index));
					ReferencedExtensionData = &ModelResources.AlwaysLoadedExtensionData[ExtensionOutput.Data->Index];
				}
				break;

				case mu::FExtensionData::EOrigin::ConstantStreamed:
				{
#if WITH_EDITOR
					check(ModelResources.StreamedExtensionDataEditor.IsValidIndex(ExtensionOutput.Data->Index));
					ReferencedExtensionData = &ModelResources.StreamedExtensionDataEditor[ExtensionOutput.Data->Index];
#else
					check(ModelResources.StreamedExtensionData.IsValidIndex(ExtensionOutput.Data->Index));
					const FCustomizableObjectStreamedResourceData& StreamedData = ModelResources.StreamedExtensionData[ExtensionOutput.Data->Index];
					if (!StreamedData.IsLoaded())
					{
						// The data should have been loaded as part of executing the CO program.
						//
						// This could indicate a bug in the streaming logic.
						UE_LOG(LogMutable, Error, TEXT("Customizable Object produced a streamed extension data that is not loaded: %s"),
							*StreamedData.GetPath().ToString());

						continue;
					}

					ReferencedExtensionData = &StreamedData.GetLoadedData();
#endif
				}
				break;

				default:
					unimplemented();
			}

			check(ReferencedExtensionData);

			ContainerArray->Emplace(FoundPin->InputPin, ReferencedExtensionData->Data);
		}
	}

	// Give each extension the chance to generate Extension Instance Data
	for (const TPair<const UCustomizableObjectExtension*, TArray<FInputPinDataContainer>>& Pair : ExtensionToExtensionData)
	{
		FInstancedStruct NewExtensionInstanceData = Pair.Key->GenerateExtensionInstanceData(Pair.Value);
		if (NewExtensionInstanceData.IsValid())
		{
			FExtensionInstanceData& NewData = Public->GetPrivate()->ExtensionInstanceData.AddDefaulted_GetRef();
			NewData.Extension = Pair.Key;
			NewData.Data = MoveTemp(NewExtensionInstanceData);
		}
	}

	// None of the current meshes requires a mesh update. Continue to BuildMaterials
	if (!bUpdateMeshes)
	{
		return true;
	}

	SkeletalMeshes.Reset();
	
	const int32 NumInstanceComponents = OperationData->InstanceUpdateData.Components.Num();
	for (int32 InstanceComponentIndexValue = 0; InstanceComponentIndexValue < NumInstanceComponents; ++InstanceComponentIndexValue)
	{
		FCustomizableObjectInstanceComponentIndex InstanceComponentIndex(InstanceComponentIndexValue);
		const FInstanceUpdateData::FComponent* Component = OperationData->GetComponentUpdateData( InstanceComponentIndex );
		if (!Component)
		{
			continue;
		}
		const FCustomizableObjectComponentIndex ObjectComponentIndex = Component->Id;
		if (!ModelResources.ComponentNamesPerObjectComponent.IsValidIndex(ObjectComponentIndex.GetValue()))
		{
			continue;
		}
		const FName ComponentName = ModelResources.ComponentNamesPerObjectComponent[ObjectComponentIndex.GetValue()];

		FCustomizableInstanceComponentData* ComponentData = GetComponentData(ComponentName);
		if (!ComponentData)
		{
			ensure(false);
			
			InvalidateGeneratedData();
			return false;
		}

		// If the component doesn't need an update copy the previously generated mesh.
		if (!OperationData->MeshChangedPerInstanceComponent[InstanceComponentIndex.GetValue()])
		{
			if (TObjectPtr<USkeletalMesh>* Result = OldSkeletalMeshes.Find(ComponentName))
			{
				SkeletalMeshes.Add(ComponentName, *Result);
			}

			continue;
		}
		
		if (OperationData->bUseMeshCache)
		{
			TArray<mu::FResourceID>* MeshDescriptors = OperationData->GetMeshDescriptors(ObjectComponentIndex);
			if (!MeshDescriptors)
			{
				continue;
			}

			if (USkeletalMesh* CachedMesh = CustomizableObject->GetPrivate()->MeshCache.Get(*MeshDescriptors))
			{
				check(MeshDescriptors->Num() == MAX_MESH_LOD_COUNT);
				ComponentData->LastMeshIdPerLOD = *MeshDescriptors;
				SkeletalMeshes.Add(ComponentName, CachedMesh);
				continue;
			}
		}

		// Reset last mesh IDs.
		ComponentData->LastMeshIdPerLOD.Init(MAX_uint64, MAX_MESH_LOD_COUNT);

		// We need the first valid mesh. get it from the component, considering that some LOSs may have been skipped.
		TSharedPtr<const mu::FMesh> ComponentMesh;
		uint32 MeshHash = INDEX_NONE;
		for (int32 FirstValidLODIndex = Component->FirstLOD; FirstValidLODIndex < OperationData->InstanceUpdateData.LODs.Num() && !ComponentMesh; ++FirstValidLODIndex)
		{
			ComponentMesh = OperationData->InstanceUpdateData.LODs[FirstValidLODIndex].Mesh;
			MeshHash = HashCombine(MeshHash,GetTypeHash(OperationData->InstanceUpdateData.LODs[FirstValidLODIndex].MeshID));
		}
		
		if (!ComponentMesh)
		{
			continue;
		}

		if (ComponentMesh->GetSurfaceCount() == 0 && !ComponentMesh->IsReference())
		{
			continue;
		}

		// If it is a referenced resource, only the first LOD is relevant.
		if (ComponentMesh->IsReference())
        {
        	int32 ReferenceID = ComponentMesh->GetReferencedMesh();
        	TSoftObjectPtr<UStreamableRenderAsset> Ref = ModelResources.PassThroughMeshes[ReferenceID];

        	if (!Ref.IsValid())
        	{
        		// This shouldn't happen here synchronosuly. It should have been requested as an async load.
        		UE_LOG(LogMutable, Error, TEXT("Referenced mesh [%s] was not pre-loaded. It will be sync-loaded probably causing a hitch. CO [%s]"), *Ref.ToString(), *GetNameSafe(CustomizableObject));
        	}

			UStreamableRenderAsset* Asset = MutablePrivate::LoadObject(Ref);
			USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Asset);
			if (SkeletalMesh)
			{
				SkeletalMeshes.Add(ComponentName, SkeletalMesh);
			}
			else
			{
				// Pass-through static meshes not implemented yet.
				UE_LOG(LogMutable, Error, TEXT("Referenced static meshes [%s] are not supported yet. CO [%s]"), *Ref.ToString(), *GetNameSafe(CustomizableObject));
			}
        	continue;
        }

		if (!ModelResources.ReferenceSkeletalMeshesData.IsValidIndex(ObjectComponentIndex.GetValue()))
		{
			InvalidateGeneratedData();
			return false;
		}

		// Create and initialize the SkeletalMesh for this component
		MUTABLE_CPUPROFILER_SCOPE(ConstructMesh);

		USkeletalMesh* SkeletalMesh = nullptr;
		
		FString SkeletalMeshName = FString::Printf (TEXT("SK_%s_%s_%lu"), *Public->GetCustomizableObject()->GetName(), *ComponentName.ToString(), MeshHash);
		SkeletalMeshName = MakeUniqueObjectName(GetTransientPackage(), USkeletalMesh::StaticClass(), *SkeletalMeshName, EUniqueObjectNameOptions::GloballyUnique).ToString();
		if (OperationData->bStreamMeshLODs)
		{
			SkeletalMesh = NewObject<UCustomizableObjectSkeletalMesh>(GetTransientPackage(), FName(*SkeletalMeshName), RF_Transient);
		}
		else
		{
			SkeletalMesh = NewObject<USkeletalMesh>(GetTransientPackage(), FName(*SkeletalMeshName), RF_Transient);
		}
		
		check(SkeletalMesh);
		SkeletalMeshes.Add(ComponentName, SkeletalMesh);
		
		const FMutableRefSkeletalMeshData& RefSkeletalMeshData = ModelResources.ReferenceSkeletalMeshesData[ObjectComponentIndex.GetValue()];

		// Set up the default information any mesh from this component will have (LODArrayInfos, RenderData, Mesh settings, etc). 
		InitSkeletalMeshData(OperationData, SkeletalMesh, RefSkeletalMeshData, *CustomizableObject, ObjectComponentIndex);
		
		// Construct a new skeleton, fix up ActiveBones and Bonemap arrays and recompute the RefInvMatrices
		const bool bBuildSkeletonDataSuccess = BuildSkeletonData(OperationData, *SkeletalMesh, RefSkeletalMeshData, *CustomizableObject, FCustomizableObjectInstanceComponentIndex(InstanceComponentIndex) );
		if (!bBuildSkeletonDataSuccess)
		{
			InvalidateGeneratedData();
			return false;
		}

		// Build PhysicsAsset merging physics assets coming from SubMeshes of the newly generated Mesh
		if (TSharedPtr<const mu::FPhysicsBody> MutablePhysics = ComponentMesh->GetPhysicsBody())
		{
			constexpr bool bDisallowCollisionBetweenAssets = true;
			UPhysicsAsset* PhysicsAssetResult = GetOrBuildMainPhysicsAsset(
				OperationData, RefSkeletalMeshData.PhysicsAsset, MutablePhysics.Get(), bDisallowCollisionBetweenAssets, FCustomizableObjectInstanceComponentIndex(InstanceComponentIndex) );

			SkeletalMesh->SetPhysicsAsset(PhysicsAssetResult);

#if WITH_EDITORONLY_DATA
			if (PhysicsAssetResult && PhysicsAssetResult->GetPackage() == GetTransientPackage())
			{
				constexpr bool bMarkAsDirty = false;
				PhysicsAssetResult->SetPreviewMesh(SkeletalMesh, bMarkAsDirty);
			}
#endif
		}

		const int32 NumAdditionalPhysicsNum = ComponentMesh->AdditionalPhysicsBodies.Num();
		for (int32 I = 0; I < NumAdditionalPhysicsNum; ++I)
		{
			TSharedPtr<const mu::FPhysicsBody> AdditionalPhysiscBody = ComponentMesh->AdditionalPhysicsBodies[I];
			
			check(AdditionalPhysiscBody);
			if (!AdditionalPhysiscBody->bBodiesModified)
			{
				continue;
			}

			const int32 PhysicsBodyExternalId = ComponentMesh->AdditionalPhysicsBodies[I]->CustomId;
			
			const FAnimBpOverridePhysicsAssetsInfo& Info = ModelResources.AnimBpOverridePhysiscAssetsInfo[PhysicsBodyExternalId];

			// Make sure the AnimInstance class is loaded. It is expected to be already loaded at this point though. 
			UClass* AnimInstanceClassLoaded = MutablePrivate::LoadClass(Info.AnimInstanceClass);
			TSubclassOf<UAnimInstance> AnimInstanceClass = TSubclassOf<UAnimInstance>(AnimInstanceClassLoaded);
			if (!ensureAlways(AnimInstanceClass))
			{
				continue;
			}

			FAnimBpGeneratedPhysicsAssets& PhysicsAssetsUsedByAnimBp = AnimBpPhysicsAssets.FindOrAdd(AnimInstanceClass);

			TObjectPtr<UPhysicsAsset> PhysicsAssetTemplate = TObjectPtr<UPhysicsAsset>(Info.SourceAsset.Get());

			check(PhysicsAssetTemplate);

			FAnimInstanceOverridePhysicsAsset& Entry =
				PhysicsAssetsUsedByAnimBp.AnimInstancePropertyIndexAndPhysicsAssets.Emplace_GetRef();

			Entry.PropertyIndex = Info.PropertyIndex;
			Entry.PhysicsAsset = MakePhysicsAssetFromTemplateAndMutableBody(
				OperationData,PhysicsAssetTemplate, AdditionalPhysiscBody.Get(), InstanceComponentIndex);
		}

		// Add sockets from the SkeletalMesh of reference and from the MutableMesh
		BuildMeshSockets(OperationData, SkeletalMesh, ModelResources, RefSkeletalMeshData, ComponentMesh);
		
		for (const TPair<const UCustomizableObjectExtension*, TArray<FInputPinDataContainer>>& Pair : ExtensionToExtensionData)
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			Pair.Key->OnSkeletalMeshCreated(Pair.Value, ObjectComponentIndex.GetValue(), SkeletalMesh);
			PRAGMA_ENABLE_DEPRECATION_WARNINGS

			Pair.Key->OnSkeletalMeshCreated(Pair.Value, ComponentName, SkeletalMesh);
		}
		
		// Mesh to copy data from if possible.
		TObjectPtr<USkeletalMesh>* OldSkeletalMeshPtr = OldSkeletalMeshes.Find(ComponentName);
		USkeletalMesh* OldSkeletalMesh = OldSkeletalMeshPtr ? *OldSkeletalMeshPtr : nullptr;

		BuildOrCopyElementData(OperationData, SkeletalMesh, *CustomizableObject, InstanceComponentIndex);
		bool const bCopyRenderDataSuccess = BuildOrCopyRenderData(OperationData, SkeletalMesh, OldSkeletalMesh, Public, InstanceComponentIndex);
		if (!bCopyRenderDataSuccess)
		{
			InvalidateGeneratedData();
			return false;
		}

		BuildOrCopyMorphTargetsData(OperationData, SkeletalMesh, OldSkeletalMesh, *CustomizableObject, InstanceComponentIndex);
		BuildOrCopyClothingData(OperationData, SkeletalMesh, ModelResources, InstanceComponentIndex, ClothingPhysicsAssets);

		FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
		ensure(RenderData && RenderData->LODRenderData.Num() > 0);
		ensure(SkeletalMesh->GetLODNum() > 0);

		if(RenderData)
		{
			for (FSkeletalMeshLODRenderData& LODResource : RenderData->LODRenderData)
			{
				UnrealConversionUtils::UpdateSkeletalMeshLODRenderDataBuffersSize(LODResource);
			}
		}

		if (OperationData->bUseMeshCache)
		{
			const TArray<mu::FResourceID>* MeshId = OperationData->GetMeshDescriptors(ObjectComponentIndex);
			if (MeshId)
			{
				CustomizableObject->GetPrivate()->MeshCache.Add(*MeshId, SkeletalMesh);
			}
		}

		if (UCustomizableObjectSkeletalMesh* StreamableMesh = Cast<UCustomizableObjectSkeletalMesh>(SkeletalMesh))
		{
			StreamableMesh->InitMutableStreamingData(OperationData, ComponentName, Component->FirstLOD, Component->LODCount);
		}
	}

	return true;
}


UCustomizableObjectInstance* UCustomizableObjectInstance::Clone()
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableObjectInstance::Clone);

	// Default Outer is the transient package.
	UCustomizableObjectInstance* NewInstance = NewObject<UCustomizableObjectInstance>();
	check(NewInstance->PrivateData);
	NewInstance->CopyParametersFromInstance(this);

	return NewInstance;
}


UCustomizableObjectInstance* UCustomizableObjectInstance::CloneStatic(UObject* Outer)
{
	UCustomizableObjectInstance* NewInstance = NewObject<UCustomizableObjectInstance>(Outer, GetClass());
	NewInstance->CopyParametersFromInstance(this);
	NewInstance->GetPrivate()->bShowOnlyRuntimeParameters = false;

	return NewInstance;
}


void UCustomizableObjectInstance::CopyParametersFromInstance(UCustomizableObjectInstance* Instance)
{
	GetPrivate()->SetDescriptor(Instance->GetPrivate()->GetDescriptor());
}


int32 UCustomizableObjectInstance::AddValueToIntRange(const FString& ParamName)
{
	return Descriptor.AddValueToIntRange(ParamName);
}


int32 UCustomizableObjectInstance::AddValueToFloatRange(const FString& ParamName)
{
	return Descriptor.AddValueToFloatRange(ParamName);
}


int32 UCustomizableObjectInstance::AddValueToProjectorRange(const FString& ParamName)
{
	return Descriptor.AddValueToProjectorRange(ParamName);
}


int32 UCustomizableObjectInstance::RemoveValueFromIntRange(const FString& ParamName, int32 RangeIndex)
{
	return Descriptor.RemoveValueFromIntRange(ParamName, RangeIndex);

}


int32 UCustomizableObjectInstance::RemoveValueFromFloatRange(const FString& ParamName, const int32 RangeIndex)
{
	return Descriptor.RemoveValueFromFloatRange(ParamName, RangeIndex);
}


int32 UCustomizableObjectInstance::RemoveValueFromProjectorRange(const FString& ParamName, const int32 RangeIndex)
{
	return Descriptor.RemoveValueFromProjectorRange(ParamName, RangeIndex);
}


int32 UCustomizableObjectInstance::MultilayerProjectorNumLayers(const FName& ProjectorParamName) const
{
	return Descriptor.NumProjectorLayers(ProjectorParamName);
}


void UCustomizableObjectInstance::MultilayerProjectorCreateLayer(const FName& ProjectorParamName, int32 Index)
{
	Descriptor.CreateLayer(ProjectorParamName, Index);
}


void UCustomizableObjectInstance::MultilayerProjectorRemoveLayerAt(const FName& ProjectorParamName, int32 Index)
{
	Descriptor.RemoveLayerAt(ProjectorParamName, Index);
}


FMultilayerProjectorLayer UCustomizableObjectInstance::MultilayerProjectorGetLayer(const FName& ProjectorParamName, int32 Index) const
{
	return Descriptor.GetLayer(ProjectorParamName, Index);
}


void UCustomizableObjectInstance::MultilayerProjectorUpdateLayer(const FName& ProjectorParamName, int32 Index, const FMultilayerProjectorLayer& Layer)
{
	Descriptor.UpdateLayer(ProjectorParamName, Index, Layer);
}


void UCustomizableObjectInstance::SaveDescriptor(FArchive &Ar, bool bUseCompactDescriptor)
{
	Descriptor.SaveDescriptor(Ar, bUseCompactDescriptor);
}


void UCustomizableObjectInstance::LoadDescriptor(FArchive &Ar)
{
	Descriptor.LoadDescriptor(Ar);
}


const FString& UCustomizableObjectInstance::GetIntParameterSelectedOption(const FString& ParamName, const int32 RangeIndex) const
{
	return Descriptor.GetIntParameterSelectedOption(ParamName, RangeIndex);
}


void UCustomizableObjectInstance::SetIntParameterSelectedOption(int32 IntParamIndex, const FString& SelectedOption, const int32 RangeIndex)
{
	Descriptor.SetIntParameterSelectedOption(IntParamIndex, SelectedOption, RangeIndex);
}


void UCustomizableObjectInstance::SetIntParameterSelectedOption(const FString& ParamName, const FString& SelectedOptionName, const int32 RangeIndex)
{
	Descriptor.SetIntParameterSelectedOption(ParamName, SelectedOptionName, RangeIndex);
}


float UCustomizableObjectInstance::GetFloatParameterSelectedOption(const FString& FloatParamName, const int32 RangeIndex) const
{
	return Descriptor.GetFloatParameterSelectedOption(FloatParamName, RangeIndex);
}


void UCustomizableObjectInstance::SetFloatParameterSelectedOption(const FString& FloatParamName, const float FloatValue, const int32 RangeIndex)
{
	return Descriptor.SetFloatParameterSelectedOption(FloatParamName, FloatValue, RangeIndex);
}


FName UCustomizableObjectInstance::GetTextureParameterSelectedOption(const FString& TextureParamName, const int32 RangeIndex) const
{
	return Descriptor.GetTextureParameterSelectedOption(TextureParamName, RangeIndex);
}


void UCustomizableObjectInstance::SetTextureParameterSelectedOption(const FString& TextureParamName, const FString& TextureValue, const int32 RangeIndex)
{
	Descriptor.SetTextureParameterSelectedOption(TextureParamName, TextureValue, RangeIndex);
}


FLinearColor UCustomizableObjectInstance::GetColorParameterSelectedOption(const FString& ColorParamName) const
{
	return Descriptor.GetColorParameterSelectedOption(ColorParamName);
}


void UCustomizableObjectInstance::SetColorParameterSelectedOption(const FString & ColorParamName, const FLinearColor& ColorValue)
{
	Descriptor.SetColorParameterSelectedOption(ColorParamName, ColorValue);
}


bool UCustomizableObjectInstance::GetBoolParameterSelectedOption(const FString& BoolParamName) const
{
	return Descriptor.GetBoolParameterSelectedOption(BoolParamName);
}


void UCustomizableObjectInstance::SetBoolParameterSelectedOption(const FString& BoolParamName, const bool BoolValue)
{
	Descriptor.SetBoolParameterSelectedOption(BoolParamName, BoolValue);
}


void UCustomizableObjectInstance::SetVectorParameterSelectedOption(const FString& VectorParamName, const FLinearColor& VectorValue)
{
	Descriptor.SetVectorParameterSelectedOption(VectorParamName, VectorValue);
}

FTransform UCustomizableObjectInstance::GetTransformParameterSelectedOption(const FString& TransformParamName) const
{
	return Descriptor.GetTransformParameterSelectedOption(TransformParamName);
}

void UCustomizableObjectInstance::SetTransformParameterSelectedOption(const FString& TransformParamName, const FTransform& TransformValue)
{
	Descriptor.SetTransformParameterSelectedOption(TransformParamName, TransformValue);
}


void UCustomizableObjectInstance::SetProjectorValue(const FString& ProjectorParamName,
                                                    const FVector& Pos, const FVector& Direction, const FVector& Up, const FVector& Scale,
                                                    const float Angle,
                                                    const int32 RangeIndex)
{
	Descriptor.SetProjectorValue(ProjectorParamName, Pos, Direction, Up, Scale, Angle, RangeIndex);
}


void UCustomizableObjectInstance::SetProjectorPosition(const FString& ProjectorParamName, const FVector& Pos, const int32 RangeIndex)
{
	Descriptor.SetProjectorPosition(ProjectorParamName, Pos, RangeIndex);
}


void UCustomizableObjectInstance::SetProjectorDirection(const FString& ProjectorParamName, const FVector& Direction, int32 RangeIndex)
{
	Descriptor.SetProjectorDirection(ProjectorParamName, Direction, RangeIndex);
}


void UCustomizableObjectInstance::SetProjectorUp(const FString& ProjectorParamName, const FVector& Up, int32 RangeIndex)
{
	Descriptor.SetProjectorUp(ProjectorParamName, Up, RangeIndex);	
}


void UCustomizableObjectInstance::SetProjectorScale(const FString& ProjectorParamName, const FVector& Scale, int32 RangeIndex)
{
	Descriptor.SetProjectorScale(ProjectorParamName, Scale, RangeIndex);	
}


void UCustomizableObjectInstance::SetProjectorAngle(const FString& ProjectorParamName, float Angle, int32 RangeIndex)
{
	Descriptor.SetProjectorAngle(ProjectorParamName, Angle, RangeIndex);
}


void UCustomizableObjectInstance::GetProjectorValue(const FString& ProjectorParamName,
                                                    FVector& OutPos, FVector& OutDir, FVector& OutUp, FVector& OutScale,
                                                    float& OutAngle, ECustomizableObjectProjectorType& OutType,
                                                    const int32 RangeIndex) const
{
	Descriptor.GetProjectorValue(ProjectorParamName, OutPos, OutDir, OutUp, OutScale, OutAngle, OutType, RangeIndex);
}


void UCustomizableObjectInstance::GetProjectorValueF(const FString& ProjectorParamName,
	FVector3f& OutPos, FVector3f& OutDir, FVector3f& OutUp, FVector3f& OutScale,
	float& OutAngle, ECustomizableObjectProjectorType& OutType,
	int32 RangeIndex) const
{
	Descriptor.GetProjectorValueF(ProjectorParamName, OutPos, OutDir, OutUp, OutScale, OutAngle, OutType, RangeIndex);
}


FVector UCustomizableObjectInstance::GetProjectorPosition(const FString& ParamName, const int32 RangeIndex) const
{
	return Descriptor.GetProjectorPosition(ParamName, RangeIndex);
}


FVector UCustomizableObjectInstance::GetProjectorDirection(const FString& ParamName, const int32 RangeIndex) const
{
	return Descriptor.GetProjectorDirection(ParamName, RangeIndex);
}


FVector UCustomizableObjectInstance::GetProjectorUp(const FString& ParamName, const int32 RangeIndex) const
{
	return Descriptor.GetProjectorUp(ParamName, RangeIndex);
}


FVector UCustomizableObjectInstance::GetProjectorScale(const FString& ParamName, const int32 RangeIndex) const
{
	return Descriptor.GetProjectorScale(ParamName, RangeIndex);
}


float UCustomizableObjectInstance::GetProjectorAngle(const FString& ParamName, int32 RangeIndex) const
{
	return Descriptor.GetProjectorAngle(ParamName, RangeIndex);
}


ECustomizableObjectProjectorType UCustomizableObjectInstance::GetProjectorParameterType(const FString& ParamName, const int32 RangeIndex) const
{
	return Descriptor.GetProjectorParameterType(ParamName, RangeIndex);
}


FCustomizableObjectProjector UCustomizableObjectInstance::GetProjector(const FString& ParamName, const int32 RangeIndex) const
{
	return Descriptor.GetProjector(ParamName, RangeIndex);
}


bool UCustomizableObjectInstance::ContainsIntParameter(const FString& ParameterName) const
{
	return Descriptor.FindTypedParameterIndex(ParameterName, EMutableParameterType::Int) != INDEX_NONE;
}


bool UCustomizableObjectInstance::ContainsFloatParameter(const FString& ParameterName) const
{
	return Descriptor.FindTypedParameterIndex(ParameterName, EMutableParameterType::Float) != INDEX_NONE;
}


bool UCustomizableObjectInstance::ContainsBoolParameter(const FString& ParameterName) const
{
	return Descriptor.FindTypedParameterIndex(ParameterName, EMutableParameterType::Bool) != INDEX_NONE;
}


bool UCustomizableObjectInstance::ContainsVectorParameter(const FString& ParameterName) const
{
	return Descriptor.FindTypedParameterIndex(ParameterName, EMutableParameterType::Color) != INDEX_NONE;
}


bool UCustomizableObjectInstance::ContainsProjectorParameter(const FString& ParameterName) const
{
	return Descriptor.FindTypedParameterIndex(ParameterName, EMutableParameterType::Projector) != INDEX_NONE;
}


bool UCustomizableObjectInstance::ContainsTransformParameter(const FString& ParameterName) const
{
	return Descriptor.FindTypedParameterIndex(ParameterName, EMutableParameterType::Transform) != INDEX_NONE;
}


int32 UCustomizableObjectInstance::FindIntParameterNameIndex(const FString& ParamName) const
{
	return Descriptor.FindTypedParameterIndex(ParamName, EMutableParameterType::Int);
}


int32 UCustomizableObjectInstance::FindFloatParameterNameIndex(const FString& ParamName) const
{
	return Descriptor.FindTypedParameterIndex(ParamName, EMutableParameterType::Float);
}


int32 UCustomizableObjectInstance::FindBoolParameterNameIndex(const FString& ParamName) const
{
	return Descriptor.FindTypedParameterIndex(ParamName, EMutableParameterType::Bool);
}


int32 UCustomizableObjectInstance::FindVectorParameterNameIndex(const FString& ParamName) const
{
	return Descriptor.FindTypedParameterIndex(ParamName, EMutableParameterType::Color);
}


int32 UCustomizableObjectInstance::FindProjectorParameterNameIndex(const FString& ParamName) const
{
	return Descriptor.FindTypedParameterIndex(ParamName, EMutableParameterType::Projector);
}


int32 UCustomizableInstancePrivate::FindIntParameterNameIndex(const FString& ParamName) const
{
	return GetPublic()->Descriptor.FindTypedParameterIndex(ParamName, EMutableParameterType::Int);
}


int32 UCustomizableInstancePrivate::FindFloatParameterNameIndex(const FString& ParamName) const
{
	return GetPublic()->Descriptor.FindTypedParameterIndex(ParamName, EMutableParameterType::Float);
}


int32 UCustomizableInstancePrivate::FindBoolParameterNameIndex(const FString& ParamName) const
{
	return GetPublic()->Descriptor.FindTypedParameterIndex(ParamName, EMutableParameterType::Bool);
}


int32 UCustomizableInstancePrivate::FindVectorParameterNameIndex(const FString& ParamName) const
{
	return GetPublic()->Descriptor.FindTypedParameterIndex(ParamName, EMutableParameterType::Color);
}


int32 UCustomizableInstancePrivate::FindProjectorParameterNameIndex(const FString& ParamName) const
{
	return GetPublic()->Descriptor.FindTypedParameterIndex(ParamName, EMutableParameterType::Projector);
}


#if WITH_EDITOR

void UCustomizableInstancePrivate::UpdateSkeletalMeshAsyncResult(FInstanceUpdateNativeDelegate Callback, bool bIgnoreCloseDist, bool bForceHighPriority, TSharedPtr<FMutableSystemSettingsOverrides> MutableSystemSettingsOverride)
{
	UCustomizableObjectSystemPrivate* SystemPrivate = UCustomizableObjectSystem::GetInstance()->GetPrivate();

	const TSharedRef<FUpdateContextPrivate> Context = MakeShared<FUpdateContextPrivate>(*GetPublic());
	Context->bIgnoreCloseDist = bIgnoreCloseDist;
	Context->bForceHighPriority = bForceHighPriority;
	Context->UpdateNativeCallback = Callback;
	Context->UpdateSettingsOverride = MutableSystemSettingsOverride;
	
	SystemPrivate->EnqueueUpdateSkeletalMesh(Context);
}

#endif

void UCustomizableObjectInstance::SetRandomValues()
{
	Descriptor.SetRandomValues();
}

void UCustomizableObjectInstance::SetRandomValuesFromStream(const FRandomStream& InStream)
{
	Descriptor.SetRandomValuesFromStream(InStream);
}

void UCustomizableObjectInstance::SetDefaultValue(const FString& ParamName)
{
	UCustomizableObject* CustomizableObject = GetCustomizableObject();
	if (!CustomizableObject)
	{
		return;
	}

	Descriptor.SetDefaultValue(CustomizableObject->GetPrivate()->FindParameter(ParamName));
}

void UCustomizableObjectInstance::SetDefaultValues()
{
	Descriptor.SetDefaultValues();
}


bool UCustomizableInstancePrivate::LoadParametersFromProfile(int32 ProfileIndex)
{
	UCustomizableObject* CustomizableObject = GetPublic()->GetCustomizableObject();
	if (!CustomizableObject)
	{
		return false;
	}

#if WITH_EDITOR
	if (ProfileIndex < 0 || ProfileIndex >= CustomizableObject->GetPrivate()->GetInstancePropertiesProfiles().Num() )
	{
		return false;
	}
	
	// This could be done only when the instance changes.
	MigrateProfileParametersToCurrentInstance(ProfileIndex);

	const FProfileParameterDat& Profile = CustomizableObject->GetPrivate()->GetInstancePropertiesProfiles()[ProfileIndex];

	GetPublic()->Descriptor.BoolParameters = Profile.BoolParameters;
	GetPublic()->Descriptor.IntParameters = Profile.IntParameters;
	GetPublic()->Descriptor.FloatParameters = Profile.FloatParameters;
	GetPublic()->Descriptor.TextureParameters = Profile.TextureParameters;
	GetPublic()->Descriptor.ProjectorParameters = Profile.ProjectorParameters;
	GetPublic()->Descriptor.VectorParameters = Profile.VectorParameters;
	GetPublic()->Descriptor.TransformParameters = Profile.TransformParameters;
#endif
	return true;

}

bool UCustomizableInstancePrivate::SaveParametersToProfile(int32 ProfileIndex)
{
	UCustomizableObject* CustomizableObject = GetPublic()->GetCustomizableObject();
	if (!CustomizableObject)
	{
		return false;
	}

#if WITH_EDITOR
	bSelectedProfileDirty = ProfileIndex != SelectedProfileIndex;

	if (ProfileIndex < 0 || ProfileIndex >= CustomizableObject->GetPrivate()->GetInstancePropertiesProfiles().Num())
	{
		return false;
	}

	FProfileParameterDat& Profile = CustomizableObject->GetPrivate()->GetInstancePropertiesProfiles()[ProfileIndex];

	Profile.BoolParameters = GetPublic()->Descriptor.BoolParameters;
	Profile.IntParameters = GetPublic()->Descriptor.IntParameters;
	Profile.FloatParameters = GetPublic()->Descriptor.FloatParameters;
	Profile.TextureParameters = GetPublic()->Descriptor.TextureParameters;
	Profile.ProjectorParameters = GetPublic()->Descriptor.ProjectorParameters;
	Profile.VectorParameters = GetPublic()->Descriptor.VectorParameters;
	Profile.TransformParameters = GetPublic()->Descriptor.TransformParameters;
#endif
	return true;
}

bool UCustomizableInstancePrivate::MigrateProfileParametersToCurrentInstance(int32 ProfileIndex)
{
	UCustomizableObject* CustomizableObject = GetPublic()->GetCustomizableObject();
	if (!CustomizableObject)
	{
		return false;
	}

#if WITH_EDITOR
	if (ProfileIndex < 0 || ProfileIndex >= CustomizableObject->GetPrivate()->GetInstancePropertiesProfiles().Num())
	{
		return false;
	}

	FProfileParameterDat& Profile = CustomizableObject->GetPrivate()->GetInstancePropertiesProfiles()[ProfileIndex];
	FProfileParameterDat TempProfile;

	TempProfile.ProfileName = Profile.ProfileName;
	TempProfile.BoolParameters = GetPublic()->Descriptor.BoolParameters;
	TempProfile.FloatParameters = GetPublic()->Descriptor.FloatParameters;
	TempProfile.IntParameters = GetPublic()->Descriptor.IntParameters;
	TempProfile.ProjectorParameters = GetPublic()->Descriptor.ProjectorParameters;
	TempProfile.TextureParameters = GetPublic()->Descriptor.TextureParameters;
	TempProfile.VectorParameters = GetPublic()->Descriptor.VectorParameters;
	TempProfile.TransformParameters = GetPublic()->Descriptor.TransformParameters;

	// Populate TempProfile with the parameters found in the profile.
	// Any profile parameter missing will be discarded.
	for (FCustomizableObjectBoolParameterValue& Parameter : TempProfile.BoolParameters)
	{
		using ParamValType = FCustomizableObjectBoolParameterValue;
		ParamValType* Found = Profile.BoolParameters.FindByPredicate(
			[&Parameter](const ParamValType& P) { return P.ParameterName == Parameter.ParameterName; });

		if (Found)
		{
			Parameter.ParameterValue = Found->ParameterValue;
		}
	}

	for (FCustomizableObjectIntParameterValue& Parameter : TempProfile.IntParameters)
	{
		using ParamValType = FCustomizableObjectIntParameterValue;
		ParamValType* Found = Profile.IntParameters.FindByPredicate(
			[&Parameter](const ParamValType& P) { return P.ParameterName == Parameter.ParameterName; });

		if (Found)
		{
			Parameter.ParameterValueName = Found->ParameterValueName;
		}
	}

	for (FCustomizableObjectFloatParameterValue& Parameter : TempProfile.FloatParameters)
	{
		using ParamValType = FCustomizableObjectFloatParameterValue;
		ParamValType* Found = Profile.FloatParameters.FindByPredicate(
			[&Parameter](const ParamValType& P) { return P.ParameterName == Parameter.ParameterName; });

		if (Found)
		{
			Parameter.ParameterValue = Found->ParameterValue;
			Parameter.ParameterRangeValues = Found->ParameterRangeValues;
		}
	}

	for (FCustomizableObjectAssetParameterValue& Parameter : TempProfile.TextureParameters)
	{
		using ParamValType = FCustomizableObjectAssetParameterValue;
		ParamValType* Found = Profile.TextureParameters.FindByPredicate(
			[&Parameter](const ParamValType& P) { return P.ParameterName == Parameter.ParameterName; });

		if (Found)
		{
			Parameter.ParameterValue = Found->ParameterValue;
		}
	}

	for (FCustomizableObjectAssetParameterValue& Parameter : TempProfile.MeshParameters)
	{
		using ParamValType = FCustomizableObjectAssetParameterValue;
		ParamValType* Found = Profile.MeshParameters.FindByPredicate(
			[&Parameter](const ParamValType& P) { return P.ParameterName == Parameter.ParameterName; });

		if (Found)
		{
			Parameter.ParameterValue = Found->ParameterValue;
		}
	}

	for (FCustomizableObjectVectorParameterValue& Parameter : TempProfile.VectorParameters)
	{
		using ParamValType = FCustomizableObjectVectorParameterValue;
		ParamValType* Found = Profile.VectorParameters.FindByPredicate(
			[&Parameter](const ParamValType& P) { return P.ParameterName == Parameter.ParameterName; });

		if (Found)
		{
			Parameter.ParameterValue = Found->ParameterValue;
		}
	}

	for (FCustomizableObjectProjectorParameterValue& Parameter : TempProfile.ProjectorParameters)
	{
		using ParamValType = FCustomizableObjectProjectorParameterValue;
		ParamValType* Found = Profile.ProjectorParameters.FindByPredicate(
			[&Parameter](const ParamValType& P) { return P.ParameterName == Parameter.ParameterName; });

		if (Found)
		{
			Parameter.RangeValues = Found->RangeValues;
			Parameter.Value = Found->Value;
		}
	}

	Profile = TempProfile;

	//CustomizableObject->Modify();
#endif

	return true;
}


UCustomizableObjectInstance* UCustomizableInstancePrivate::GetPublic() const
{
	UCustomizableObjectInstance* Public = StaticCast<UCustomizableObjectInstance*>(GetOuter());
	check(Public);

	return Public;
}


void UCustomizableInstancePrivate::SetSelectedParameterProfileDirty()
{
	UCustomizableObject* CustomizableObject = GetPublic()->GetCustomizableObject();
	if (!CustomizableObject)
	{
		return;
	}

#if WITH_EDITOR
	bSelectedProfileDirty = SelectedProfileIndex != INDEX_NONE;
	
	if (bSelectedProfileDirty)
	{
		CustomizableObject->Modify();
	}
#endif
}

bool UCustomizableInstancePrivate::IsSelectedParameterProfileDirty() const
{
	
#if WITH_EDITOR
	return bSelectedProfileDirty && SelectedProfileIndex != INDEX_NONE;
#else
	return false;
#endif
}


void UCustomizableInstancePrivate::DiscardResources()
{
	check(IsInGameThread());

	UCustomizableObjectInstance* Instance = Cast<UCustomizableObjectInstance>(GetOuter());
	if (!Instance)
	{
		return;
	}

	if (SkeletalMeshStatus == ESkeletalMeshStatus::Success)
	{
		if (CVarEnableReleaseMeshResources.GetValueOnGameThread())
		{
			for (const TTuple<FName, TObjectPtr<USkeletalMesh>>& Tuple : SkeletalMeshes)
			{
				USkeletalMesh* SkeletalMesh = Tuple.Get<1>();
			
				if (SkeletalMesh->IsValidLowLevel() && !SkeletalMesh->HasPendingInitOrStreaming())
				{
					SkeletalMesh->ReleaseResources();
				}
			}
		}
		
		SkeletalMeshes.Empty();

		ReleaseMutableResources(false, *Instance);
	}
	
	InvalidateGeneratedData();
	
}


void UCustomizableInstancePrivate::SetReferenceSkeletalMesh() const
{
	UCustomizableObjectInstance* Instance = Cast<UCustomizableObjectInstance>(GetOuter());
	if (!Instance)
	{
		return;
	}

	UCustomizableObject* CustomizableObject = Instance->GetCustomizableObject();
	if (!CustomizableObject)
	{
		return;
	}
	
	const UModelResources* ModelResources = CustomizableObject->GetPrivate()->GetModelResources();
	if (!ModelResources)
	{
		return;
	}
	
	for (TObjectIterator<UCustomizableObjectInstanceUsage> It; It; ++It)
	{
		UCustomizableObjectInstanceUsage* CustomizableObjectInstanceUsage = *It;
		if (!IsValid(CustomizableObjectInstanceUsage) || CustomizableObjectInstanceUsage->GetCustomizableObjectInstance() != Instance)
		{
			continue;
		}

#if WITH_EDITOR
		if (CustomizableObjectInstanceUsage->GetPrivate()->IsNetMode(NM_DedicatedServer))
		{
			continue;
		}
#endif

		const FName& ComponentName = CustomizableObjectInstanceUsage->GetComponentName();
		const int32 ObjectComponentIndex = ModelResources->ComponentNamesPerObjectComponent.IndexOfByKey(ComponentName);
		if (!ModelResources->ReferenceSkeletalMeshesData.IsValidIndex(ObjectComponentIndex))
		{
			continue;
		}
		
		if (USkeletalMeshComponent* Parent = CustomizableObjectInstanceUsage->GetAttachParent())
		{
			Parent->EmptyOverrideMaterials();

			TSoftObjectPtr<USkeletalMesh> SoftObjectPtr = ModelResources->ReferenceSkeletalMeshesData[ObjectComponentIndex].SoftSkeletalMesh;
			USkeletalMesh* SkeletalMesh = MutablePrivate::LoadObject(SoftObjectPtr);
			Parent->SetSkeletalMesh(SkeletalMesh);
		}
	}
}


namespace
{
	inline float unpack_uint8(uint8 i)
	{
		float res = i;
		res -= 127.5f;
		res /= 127.5f;
		return res;
	}
}

bool MutableTextureUsesOfflineProcessedData()
{
#if PLATFORM_DESKTOP || PLATFORM_ANDROID || PLATFORM_IOS
	return true;
#else
	return false;
#endif
}

void SetTexturePropertiesFromMutableImageProps(UTexture2D* Texture, const FMutableModelImageProperties& Props, bool bNeverStream)
{
#if !PLATFORM_DESKTOP
	if (UCustomizableObjectSystem::GetInstance()->GetPrivate()->EnableMutableProgressiveMipStreaming <= 0)
	{
		Texture->NeverStream = true;
	}
	else
	{
		Texture->NeverStream = bNeverStream;
	}
#else
	Texture->NeverStream = bNeverStream;
#endif
	Texture->bNotOfflineProcessed = !MutableTextureUsesOfflineProcessedData();

	Texture->SRGB = Props.SRGB;
	Texture->Filter = Props.Filter;
	Texture->LODBias = Props.LODBias;

	if (Props.MipGenSettings == TextureMipGenSettings::TMGS_NoMipmaps)
	{
		Texture->NeverStream = true;
	}

#if WITH_EDITORONLY_DATA
	Texture->MipGenSettings = Props.MipGenSettings;

	Texture->bFlipGreenChannel = Props.FlipGreenChannel;
#endif

	Texture->LODGroup = Props.LODGroup;
	Texture->AddressX = Props.AddressX;
	Texture->AddressY = Props.AddressY;
}


UCustomizableInstancePrivate* UCustomizableObjectInstance::GetPrivate() const
{ 
	check(PrivateData); // Currently this is initialized in the constructor so we expect it always to exist.
	return PrivateData; 
}


FMutableUpdateCandidate::FMutableUpdateCandidate(UCustomizableObjectInstance* InCustomizableObjectInstance): CustomizableObjectInstance(InCustomizableObjectInstance)
{
	const FCustomizableObjectInstanceDescriptor& Descriptor = InCustomizableObjectInstance->GetPrivate()->GetDescriptor();
	MinLOD = Descriptor.MinLOD;
	QualitySettingMinLODs = Descriptor.QualitySettingMinLODs;
	FirstRequestedLOD = Descriptor.GetFirstRequestedLOD();
}


bool FMutableUpdateCandidate::HasBeenIssued() const
{
	return bHasBeenIssued;
}


void FMutableUpdateCandidate::Issue()
{
	bHasBeenIssued = true;
}


void FMutableUpdateCandidate::ApplyLODUpdateParamsToInstance(FUpdateContextPrivate& Context)
{
	CustomizableObjectInstance->Descriptor.MinLOD = MinLOD;
	CustomizableObjectInstance->Descriptor.QualitySettingMinLODs = QualitySettingMinLODs;
	CustomizableObjectInstance->Descriptor.FirstRequestedLOD = FirstRequestedLOD;

	Context.SetMinLOD(MinLOD);
	Context.SetQualitySettingMinLODs(QualitySettingMinLODs);
	Context.SetFirstRequestedLOD(FirstRequestedLOD);
}


// The memory allocated in the function and pointed by the returned pointer is owned by the caller and must be freed. 
// If assigned to a UTexture2D, it will be freed by that UTexture2D
FTexturePlatformData* MutableCreateImagePlatformData(TSharedPtr<const mu::FImage> MutableImage, int32 OnlyLOD, uint16 FullSizeX, uint16 FullSizeY)
{
	int32 SizeX = FMath::Max(MutableImage->GetSize()[0], FullSizeX);
	int32 SizeY = FMath::Max(MutableImage->GetSize()[1], FullSizeY);

	if (SizeX <= 0 || SizeY <= 0)
	{
		UE_LOG(LogMutable, Warning, TEXT("Invalid parameters specified for UCustomizableInstancePrivate::MutableCreateImagePlatformData()"));
		return nullptr;
	}

	int32 FirstLOD = 0;
	for (int32 l = 0; l < OnlyLOD; ++l)
	{
		if (SizeX <= 4 || SizeY <= 4)
		{
			break;
		}
		SizeX = FMath::Max(SizeX / 2, 1);
		SizeY = FMath::Max(SizeY / 2, 1);
		++FirstLOD;
	}

	int32 MaxSize = FMath::Max(SizeX, SizeY);
	int32 FullLODCount = 1;
	int32 MipsToSkip = 0;
	
	if (OnlyLOD < 0)
	{
		FullLODCount = FMath::CeilLogTwo(MaxSize) + 1;
		MipsToSkip = FullLODCount - MutableImage->GetLODCount();
		check(MipsToSkip >= 0);
	}

	// Reduce final texture size if we surpass the max size we can generate.
	UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();
	UCustomizableObjectSystemPrivate* SystemPrivate = System ? System->GetPrivate() : nullptr;

	int32 MaxTextureSizeToGenerate = SystemPrivate ? SystemPrivate->MaxTextureSizeToGenerate : 0;

	if (MaxTextureSizeToGenerate > 0)
	{
		// Skip mips only if texture streaming is disabled 
		const bool bIsStreamingEnabled = MipsToSkip > 0;

		// Skip mips if the texture surpasses a certain size
		if (MaxSize > MaxTextureSizeToGenerate && !bIsStreamingEnabled && OnlyLOD < 0)
		{
			// Skip mips until MaxSize is equal or less than MaxTextureSizeToGenerate or there aren't more mips to skip
			while (MaxSize > MaxTextureSizeToGenerate && FirstLOD < (FullLODCount - 1))
			{
				MaxSize = MaxSize >> 1;
				FirstLOD++;
			}

			// Update SizeX and SizeY
			SizeX = SizeX >> FirstLOD;
			SizeY = SizeY >> FirstLOD;
		}
	}

	if (MutableImage->GetLODCount() == 1)
	{
		MipsToSkip = 0;
		FullLODCount = 1;
		FirstLOD = 0;
	}

	int32 EndLOD = OnlyLOD < 0 ? FullLODCount : FirstLOD + 1;
	
	mu::EImageFormat MutableFormat = MutableImage->GetFormat();

	int32 MaxPossibleSize = 0;
		
	if (MaxTextureSizeToGenerate > 0)
	{
		MaxPossibleSize = int32(FMath::Pow(2.f, float(FullLODCount - FirstLOD - 1)));
	}
	else
	{
		MaxPossibleSize = int32(FMath::Pow(2.f, float(FullLODCount - 1)));
	}

	// This could happen with non-power-of-two images.
	//check(SizeX == MaxPossibleSize || SizeY == MaxPossibleSize || FullLODCount == 1);
	if (!(SizeX == MaxPossibleSize || SizeY == MaxPossibleSize || FullLODCount == 1))
	{
		UE_LOG(LogMutable, Warning, TEXT("Building instance: unsupported texture size %d x %d."), SizeX, SizeY);
		//return nullptr;
	}

	mu::FImageOperator ImOp = mu::FImageOperator::GetDefault(mu::FImageOperator::FImagePixelFormatFunc());

	EPixelFormat PlatformFormat = PF_Unknown;
	switch (MutableFormat)
	{
	case mu::EImageFormat::RGB_UByte:
		// performance penalty. can happen in states that remove compression.
		PlatformFormat = PF_R8G8B8A8;	
		UE_LOG(LogMutable, Display, TEXT("Building instance: a texture was generated in a format not supported by the hardware (RGB), this results in an additional conversion, so a performance penalty."));
		break; 

	case mu::EImageFormat::BGRA_UByte:			
		// performance penalty. can happen with texture parameter images.
		PlatformFormat = PF_R8G8B8A8;	
		UE_LOG(LogMutable, Display, TEXT("Building instance: a texture was generated in a format not supported by the hardware (BGRA), this results in an additional conversion, so a performance penalty."));
		break;

	// Good cases:
	case mu::EImageFormat::RGBA_UByte:		PlatformFormat = PF_R8G8B8A8;	break;
	case mu::EImageFormat::BC1:				PlatformFormat = PF_DXT1;		break;
	case mu::EImageFormat::BC2:				PlatformFormat = PF_DXT3;		break;
	case mu::EImageFormat::BC3:				PlatformFormat = PF_DXT5;		break;
	case mu::EImageFormat::BC4:				PlatformFormat = PF_BC4;		break;
	case mu::EImageFormat::BC5:				PlatformFormat = PF_BC5;		break;
	case mu::EImageFormat::L_UByte:			PlatformFormat = PF_G8;			break;
	case mu::EImageFormat::ASTC_4x4_RGB_LDR:	PlatformFormat = PF_ASTC_4x4;	break;
	case mu::EImageFormat::ASTC_4x4_RGBA_LDR:PlatformFormat = PF_ASTC_4x4;	break;
	case mu::EImageFormat::ASTC_4x4_RG_LDR:	PlatformFormat = PF_ASTC_4x4;	break;
	default:
		// Cannot prepare texture if it's not in the right format, this can happen if mutable is in debug mode or in case of bugs
		UE_LOG(LogMutable, Warning, TEXT("Building instance: a texture was generated in an unsupported format, it will be converted to Unreal with a performance penalty."));

		switch (mu::GetImageFormatData(MutableFormat).Channels)
		{
		case 1:
			PlatformFormat = PF_R8;
			MutableImage = ImOp.ImagePixelFormat(0, MutableImage.Get(), mu::EImageFormat::L_UByte);
			break;
		case 2:
		case 3:
		case 4:
			PlatformFormat = PF_R8G8B8A8;
			MutableImage = ImOp.ImagePixelFormat(0, MutableImage.Get(), mu::EImageFormat::RGBA_UByte);
			break;
		default: 
			// Absolutely worst case
			return nullptr;
		}		
	}

	FTexturePlatformData* PlatformData = new FTexturePlatformData();
	PlatformData->SizeX = SizeX;
	PlatformData->SizeY = SizeY;
	PlatformData->PixelFormat = PlatformFormat;

	// Allocate mipmaps.

	if (!FMath::IsPowerOfTwo(SizeX) || !FMath::IsPowerOfTwo(SizeY))
	{
		EndLOD = FirstLOD + 1;
		MipsToSkip = 0;
		FullLODCount = 1;
	}

	for (int32 MipLevelUE = FirstLOD; MipLevelUE < EndLOD; ++MipLevelUE)
	{
		int32 MipLevelMutable = MipLevelUE - MipsToSkip;		

		// Unlike Mutable, UE expects MIPs sizes to be at least the size of the compression block.
		// For example, a 8x8 PF_DXT1 texture will have the following MIPs:
		// Mutable    Unreal Engine
		// 8x8        8x8
		// 4x4        4x4
		// 2x2        4x4
		// 1x1        4x4
		//
		// Notice that even though Mutable reports MIP smaller than the block size, the actual data contains at least a block.
		FTexture2DMipMap* Mip = new FTexture2DMipMap( FMath::Max(SizeX, GPixelFormats[PlatformFormat].BlockSizeX)
													, FMath::Max(SizeY, GPixelFormats[PlatformFormat].BlockSizeY));

		PlatformData->Mips.Add(Mip);
		if(MipLevelUE >= MipsToSkip || OnlyLOD>=0)
		{
			check(MipLevelMutable >= 0);
			check(MipLevelMutable < MutableImage->GetLODCount());

			Mip->BulkData.Lock(LOCK_READ_WRITE);
			Mip->BulkData.ClearBulkDataFlags(BULKDATA_SingleUse);

			const uint8* MutableData = MutableImage->GetLODData(MipLevelMutable);
			const uint32 SourceDataSize = MutableImage->GetLODDataSize(MipLevelMutable);

			uint32 DestDataSize = (MutableFormat == mu::EImageFormat::RGB_UByte)
					? (SourceDataSize/3) * 4
					: SourceDataSize;
			void* pData = Mip->BulkData.Realloc(DestDataSize);

			// Special inefficient cases
			if (MutableFormat== mu::EImageFormat::BGRA_UByte)
			{
				check(SourceDataSize==DestDataSize);

				MUTABLE_CPUPROFILER_SCOPE(Innefficent_BGRA_Format_Conversion);

				uint8_t* pDest = reinterpret_cast<uint8_t*>(pData);
				for (size_t p = 0; p < SourceDataSize / 4; ++p)
				{
					pDest[p * 4 + 0] = MutableData[p * 4 + 2];
					pDest[p * 4 + 1] = MutableData[p * 4 + 1];
					pDest[p * 4 + 2] = MutableData[p * 4 + 0];
					pDest[p * 4 + 3] = MutableData[p * 4 + 3];
				}
			}

			else if (MutableFormat == mu::EImageFormat::RGB_UByte)
			{
				MUTABLE_CPUPROFILER_SCOPE(Innefficent_RGB_Format_Conversion);

				uint8_t* pDest = reinterpret_cast<uint8_t*>(pData);
				for (size_t p = 0; p < SourceDataSize / 3; ++p)
				{
					pDest[p * 4 + 0] = MutableData[p * 3 + 0];
					pDest[p * 4 + 1] = MutableData[p * 3 + 1];
					pDest[p * 4 + 2] = MutableData[p * 3 + 2];
					pDest[p * 4 + 3] = 255;
				}
			}

			// Normal case
			else
			{
				check(SourceDataSize == DestDataSize);
				FMemory::Memcpy(pData, MutableData, SourceDataSize);
			}

			Mip->BulkData.Unlock();
		}
		else
		{
			Mip->BulkData.SetBulkDataFlags(BULKDATA_PayloadInSeperateFile);
			Mip->BulkData.ClearBulkDataFlags(BULKDATA_PayloadAtEndOfFile);
		}

		SizeX /= 2;
		SizeY /= 2;

		SizeX = SizeX > 0 ? SizeX : 1;
		SizeY = SizeY > 0 ? SizeY : 1;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Some consistency checks for dev builds
	int32 BulkDataCount = 0;

	for (int32 i = 0; i < PlatformData->Mips.Num(); ++i)
	{
		if (i > 0)
		{
			check(PlatformData->Mips[i].SizeX == PlatformData->Mips[i - 1].SizeX / 2 || PlatformData->Mips[i].SizeX == GPixelFormats[PlatformFormat].BlockSizeX);
			check(PlatformData->Mips[i].SizeY == PlatformData->Mips[i - 1].SizeY / 2 || PlatformData->Mips[i].SizeY == GPixelFormats[PlatformFormat].BlockSizeY);
		}

		if (PlatformData->Mips[i].BulkData.GetBulkDataSize() > 0)
		{
			BulkDataCount++;
		}
	}

	if (MaxTextureSizeToGenerate > 0)
	{
		check(FullLODCount == 1 || OnlyLOD >= 0 || (BulkDataCount == (MutableImage->GetLODCount() - FirstLOD)));
	}
	else
	{
		check(FullLODCount == 1 || OnlyLOD >= 0 || (BulkDataCount == MutableImage->GetLODCount()));
	}
#endif

	return PlatformData;
}


void ConvertImage(UTexture2D* Texture, TSharedPtr<const mu::FImage> MutableImage, const FMutableModelImageProperties& Props, int OnlyLOD, int32 ExtractChannel)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::ConvertImage);

	SetTexturePropertiesFromMutableImageProps(Texture, Props, false);

	mu::EImageFormat MutableFormat = MutableImage->GetFormat();

	// Extract a single channel, if requested.
	if (ExtractChannel >= 0)
	{
		mu::FImageOperator ImOp = mu::FImageOperator::GetDefault(mu::FImageOperator::FImagePixelFormatFunc());

		MutableImage = ImOp.ImagePixelFormat( 4, MutableImage.Get(), mu::EImageFormat::RGBA_UByte );

		uint8_t Channel = uint8_t( FMath::Clamp(ExtractChannel,0,3) );
		MutableImage = ImOp.ImageSwizzle( mu::EImageFormat::L_UByte, &MutableImage, &Channel );
		MutableFormat = mu::EImageFormat::L_UByte;
	}

	// Hack: This format is unsupported in UE, but it shouldn't happen in production.
	if (MutableFormat == mu::EImageFormat::RGB_UByte)
	{
		UE_LOG(LogMutable, Warning, TEXT("Building instance: a texture was generated in RGB format, which is slow to convert to Unreal."));

		// Expand the image.
		TSharedPtr<mu::FImage> Converted = MakeShared<mu::FImage>(MutableImage->GetSizeX(), MutableImage->GetSizeY(), MutableImage->GetLODCount(), mu::EImageFormat::RGBA_UByte, mu::EInitializationType::NotInitialized);

		for (int32 LODIndex = 0; LODIndex < Converted->GetLODCount(); ++LODIndex)
		{
			int32 PixelCount = MutableImage->GetLODDataSize(LODIndex)/3;
			const uint8* pSource = MutableImage->GetMipData(LODIndex);
			uint8* pTarget = Converted->GetMipData(LODIndex);
			for (int32 p = 0; p < PixelCount; ++p)
			{
				pTarget[4 * p + 0] = pSource[3 * p + 0];
				pTarget[4 * p + 1] = pSource[3 * p + 1];
				pTarget[4 * p + 2] = pSource[3 * p + 2];
				pTarget[4 * p + 3] = 255;
			}
		}

		MutableImage = Converted;
		MutableFormat = mu::EImageFormat::RGBA_UByte;
	}
	else if (MutableFormat == mu::EImageFormat::BGRA_UByte)
	{
		UE_LOG(LogMutable, Warning, TEXT("Building instance: a texture was generated in BGRA format, which is slow to convert to Unreal."));

		MUTABLE_CPUPROFILER_SCOPE(Swizzle);
		// Swizzle the image.
		// \TODO: Raise a warning?
		TSharedPtr<mu::FImage> Converted = MakeShared<mu::FImage>(MutableImage->GetSizeX(), MutableImage->GetSizeY(), 1, mu::EImageFormat::RGBA_UByte, mu::EInitializationType::NotInitialized);
		int32 PixelCount = MutableImage->GetSizeX() * MutableImage->GetSizeY();

		const uint8* pSource = MutableImage->GetLODData(0);
		uint8* pTarget = Converted->GetLODData(0);
		for (int32 p = 0; p < PixelCount; ++p)
		{
			pTarget[4 * p + 0] = pSource[4 * p + 2];
			pTarget[4 * p + 1] = pSource[4 * p + 1];
			pTarget[4 * p + 2] = pSource[4 * p + 0];
			pTarget[4 * p + 3] = pSource[4 * p + 3];
		}

		MutableImage = Converted;
		MutableFormat = mu::EImageFormat::RGBA_UByte;
	}

	if (OnlyLOD >= 0)
	{
		OnlyLOD = FMath::Min( OnlyLOD, MutableImage->GetLODCount()-1 );
	}

	Texture->SetPlatformData(MutableCreateImagePlatformData(MutableImage,OnlyLOD,0,0) );
}

static int32 EnableRayTracingFix = 0;
FAutoConsoleVariableRef CVarMutableEnableRayTracingFix(
	TEXT("mutable.EnableRayTracingFix"),
	EnableRayTracingFix,
	TEXT("If 0, Disabled. Generated meshes will have ray tracing enabled.")
	TEXT("If 1, Enable fix for meshes with mesh LOD streaming. Meshes will have ray tracing disabled.")
	TEXT("If 2, Enable fix for all generated meshes. Meshes will have ray tracing disabled.")
	);

void UCustomizableInstancePrivate::InitSkeletalMeshData(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh* SkeletalMesh, const FMutableRefSkeletalMeshData& RefSkeletalMeshData, const UCustomizableObject& CustomizableObject, FCustomizableObjectComponentIndex ObjectComponentIndex)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::InitSkeletalMesh);

	check(SkeletalMesh);

	const FName ComponentName = OperationData->ComponentNames[ObjectComponentIndex.GetValue()];

	SkeletalMesh->NeverStream = !OperationData->bStreamMeshLODs;

	SkeletalMesh->SetImportedBounds(RefSkeletalMeshData.Bounds);
	SkeletalMesh->SetPostProcessAnimBlueprint(RefSkeletalMeshData.PostProcessAnimInst.Get());
	SkeletalMesh->SetShadowPhysicsAsset(RefSkeletalMeshData.ShadowPhysicsAsset.Get());
	
	const bool bEnableRayTracingFix = EnableRayTracingFix == 2 || (EnableRayTracingFix == 1 && OperationData->bStreamMeshLODs);
	if (bEnableRayTracingFix)
	{
		SkeletalMesh->SetSupportRayTracing(false);
	}
	
	SkeletalMesh->SetHasVertexColors(false);

	// Set the default Physics Assets
	SkeletalMesh->SetPhysicsAsset(RefSkeletalMeshData.PhysicsAsset.Get());
	SkeletalMesh->SetEnablePerPolyCollision(RefSkeletalMeshData.Settings.bEnablePerPolyCollision);

	// Asset User Data
	{
		const FCustomizableInstanceComponentData* ComponentData = GetComponentData(ObjectComponentIndex);
		check(ComponentData);
		for (TObjectPtr<UAssetUserData> AssetUserData : ComponentData->AssetUserDataArray)
		{
			SkeletalMesh->AddAssetUserData(AssetUserData);
		}

		//Custom Asset User Data
		if (OperationData->Instance->GetAnimationGameplayTags().Num() ||
			ComponentData->AnimSlotToBP.Num())
		{
			UCustomizableObjectInstanceUserData* InstanceData = NewObject<UCustomizableObjectInstanceUserData>(SkeletalMesh, NAME_None, RF_Public | RF_Transactional);
			InstanceData->AnimationGameplayTag = OperationData->Instance->GetAnimationGameplayTags();
			
			for (const TTuple<FName, TSoftClassPtr<UAnimInstance>>& AnimSlot : ComponentData->AnimSlotToBP)
			{
				FCustomizableObjectAnimationSlot AnimationSlot;
				AnimationSlot.Name = AnimSlot.Key;
				AnimationSlot.AnimInstance = AnimSlot.Value;
				
				InstanceData->AnimationSlots.Add(AnimationSlot);
			}
			
			SkeletalMesh->AddAssetUserData(InstanceData);
		}
	}

	// Allocate resources for rendering and add LOD Info
	{
		MUTABLE_CPUPROFILER_SCOPE(InitSkeletalMesh_AddLODData);
		SkeletalMesh->AllocateResourceForRendering();
		
		FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
		int32 NumLODsAvailablePerComponent = OperationData->NumLODsAvailable[ComponentName];
		RenderData->NumInlinedLODs = NumLODsAvailablePerComponent - OperationData->FirstResidentLOD[ComponentName];
		RenderData->NumNonOptionalLODs = NumLODsAvailablePerComponent - OperationData->FirstLODAvailable[ComponentName];
		RenderData->CurrentFirstLODIdx = OperationData->FirstResidentLOD[ComponentName];
		RenderData->PendingFirstLODIdx = RenderData->CurrentFirstLODIdx;
		RenderData->LODBiasModifier = OperationData->FirstLODAvailable[ComponentName];		
		
		if (bEnableRayTracingFix)
		{
			RenderData->bSupportRayTracing = false;
		}

		for (int32 LODIndex = 0; LODIndex < NumLODsAvailablePerComponent; ++LODIndex)
		{
			RenderData->LODRenderData.Add(new FSkeletalMeshLODRenderData());
			
			FSkeletalMeshLODRenderData& LODRenderData = RenderData->LODRenderData[LODIndex];
			LODRenderData.bIsLODOptional = LODIndex < OperationData->FirstLODAvailable[ComponentName];
			LODRenderData.bStreamedDataInlined = LODIndex >= OperationData->FirstResidentLOD[ComponentName];

			const FMutableRefLODData& LODData = RefSkeletalMeshData.LODData[LODIndex];
			FSkeletalMeshLODInfo& LODInfo = SkeletalMesh->AddLODInfo();
			LODInfo.ScreenSize = LODData.LODInfo.ScreenSize;
			LODInfo.LODHysteresis = LODData.LODInfo.LODHysteresis;
			LODInfo.bSupportUniformlyDistributedSampling = LODData.LODInfo.bSupportUniformlyDistributedSampling;
			LODInfo.bAllowCPUAccess = LODData.LODInfo.bAllowCPUAccess;

			if (bEnableRayTracingFix)
			{
				LODInfo.SkinCacheUsage = ESkinCacheUsage::Disabled;
			}

			// Disable LOD simplification when baking instances
			LODInfo.ReductionSettings.NumOfTrianglesPercentage = 1.f;
			LODInfo.ReductionSettings.NumOfVertPercentage = 1.f;
			LODInfo.ReductionSettings.MaxNumOfTriangles = TNumericLimits<uint32>::Max();
			LODInfo.ReductionSettings.MaxNumOfVerts = TNumericLimits<uint32>::Max();
			LODInfo.ReductionSettings.bRecalcNormals = 0;
			LODInfo.ReductionSettings.WeldingThreshold = TNumericLimits<float>::Min();
			LODInfo.ReductionSettings.bMergeCoincidentVertBones = 0;
			LODInfo.ReductionSettings.bImproveTrianglesForCloth = 0;

#if WITH_EDITORONLY_DATA
			LODInfo.ReductionSettings.MaxNumOfTrianglesPercentage = TNumericLimits<uint32>::Max();
			LODInfo.ReductionSettings.MaxNumOfVertsPercentage = TNumericLimits<uint32>::Max();

			LODInfo.BuildSettings.bRecomputeNormals = false;
			LODInfo.BuildSettings.bRecomputeTangents = false;
			LODInfo.BuildSettings.bUseMikkTSpace = false;
			LODInfo.BuildSettings.bComputeWeightedNormals = false;
			LODInfo.BuildSettings.bRemoveDegenerates = false;
			LODInfo.BuildSettings.bUseHighPrecisionTangentBasis = false;
			LODInfo.BuildSettings.bUseHighPrecisionSkinWeights = false;
			LODInfo.BuildSettings.bUseFullPrecisionUVs = true;
			LODInfo.BuildSettings.bUseBackwardsCompatibleF16TruncUVs = false;
			LODInfo.BuildSettings.ThresholdPosition = TNumericLimits<float>::Min();
			LODInfo.BuildSettings.ThresholdTangentNormal = TNumericLimits<float>::Min();
			LODInfo.BuildSettings.ThresholdUV = TNumericLimits<float>::Min();
			LODInfo.BuildSettings.MorphThresholdPosition = TNumericLimits<float>::Min();
			LODInfo.BuildSettings.BoneInfluenceLimit = 0;
#endif
			LODInfo.LODMaterialMap.SetNumZeroed(1);
		}
	}

	if (RefSkeletalMeshData.SkeletalMeshLODSettings)
	{
#if WITH_EDITORONLY_DATA
		SkeletalMesh->SetLODSettings(RefSkeletalMeshData.SkeletalMeshLODSettings);
#else
		// This is the part from the above SkeletalMesh->SetLODSettings that's available in-game
		RefSkeletalMeshData.SkeletalMeshLODSettings->SetLODSettingsToMesh(SkeletalMesh);
#endif
	}

	// Set Min LOD (Override the Reference Skeletal Mesh LOD Settings)
	const UModelResources& ModelResources = CustomizableObject.GetPrivate()->GetModelResourcesChecked();
	SkeletalMesh->SetMinLod(FMath::Max(ModelResources.MinLODPerComponent.FindChecked(ComponentName).GetDefault(), static_cast<int32>(OperationData->FirstLODAvailable[ComponentName])));
	SkeletalMesh->SetQualityLevelMinLod(ModelResources.MinQualityLevelLODPerComponent.FindChecked(ComponentName));

	// Set up unreal's default material, will be replaced when building materials
	{
		MUTABLE_CPUPROFILER_SCOPE(InitSkeletalMesh_AddDefaultMaterial);
		UMaterialInterface* UnrealMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		SkeletalMesh->GetMaterials().SetNum(1);
		SkeletalMesh->GetMaterials()[0] = UnrealMaterial;

		// Default density
		SetMeshUVChannelDensity(SkeletalMesh->GetMaterials()[0].UVChannelData);
	}

}


bool UCustomizableInstancePrivate::BuildSkeletonData(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh& SkeletalMesh, const FMutableRefSkeletalMeshData& RefSkeletalMeshData, UCustomizableObject& CustomizableObject, FCustomizableObjectInstanceComponentIndex InstanceComponentIndex)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::BuildSkeletonData);

	FCustomizableObjectComponentIndex ObjectComponentIndex = OperationData->GetObjectComponentIndex(InstanceComponentIndex);

	bool bCreatedNewSkeleton = false;
	const TObjectPtr<USkeleton> Skeleton = MergeSkeletons(CustomizableObject, RefSkeletalMeshData, ObjectComponentIndex, bCreatedNewSkeleton);
	if (!Skeleton)
	{
		return false;
	}

	SkeletalMesh.SetSkeleton(Skeleton);

	SkeletalMesh.SetRefSkeleton(Skeleton->GetReferenceSkeleton());
	FReferenceSkeleton& ReferenceSkeleton = SkeletalMesh.GetRefSkeleton();

	const TArray<FMeshBoneInfo>& RawRefBoneInfo = ReferenceSkeleton.GetRawRefBoneInfo();
	const int32 RawRefBoneCount = ReferenceSkeleton.GetRawBoneNum();

	const TArray<FInstanceUpdateData::FBone>& BonePose = OperationData->InstanceUpdateData.SkeletonsPerInstanceComponent[InstanceComponentIndex.GetValue()].BonePose;
	TMap<mu::FBoneName, TPair<FName,uint16>>& BoneInfoMap = OperationData->InstanceUpdateData.SkeletonsPerInstanceComponent[InstanceComponentIndex.GetValue()].BoneInfoMap;

	{
		MUTABLE_CPUPROFILER_SCOPE(BuildSkeletonData_BuildBoneInfoMap);

		BoneInfoMap.Reserve(RawRefBoneCount);

		const UModelResources& ModelResources = CustomizableObject.GetPrivate()->GetModelResourcesChecked();
		for (int32 Index = 0; Index < RawRefBoneCount; ++Index)
		{
			const FName BoneName = RawRefBoneInfo[Index].Name;
			const FString BoneNameString = BoneName.ToString().ToLower();
			if(const uint32* Hash = ModelResources.BoneNamesMap.Find(BoneNameString))
			{
				const mu::FBoneName Bone(*Hash);
				TPair<FName, uint16>& BoneInfo = BoneInfoMap.Add(Bone);
				BoneInfo.Key = BoneName;
				BoneInfo.Value = Index;
			}
		}
	}

	{
		MUTABLE_CPUPROFILER_SCOPE(BuildSkeletonData_EnsureBonesExist);

		// Ensure all required bones are present in the skeleton
		for (const FInstanceUpdateData::FBone& Bone : BonePose)
		{
			if (!BoneInfoMap.Find(Bone.Name))
			{
				UE_LOG(LogMutable, Warning, TEXT("The skeleton of skeletal mesh [%s] is missing a bone with ID [%d], which the mesh requires."),
					*SkeletalMesh.GetName(), Bone.Name.Id);
				return false;
			}
		}
	}

	{
		MUTABLE_CPUPROFILER_SCOPE(BuildSkeletonData_ApplyPose);
		
		TArray<FMatrix44f>& RefBasesInvMatrix = SkeletalMesh.GetRefBasesInvMatrix();
		RefBasesInvMatrix.Empty(RawRefBoneCount);

		// Calculate the InvRefMatrices to ensure all transforms are there for the second step 
		SkeletalMesh.CalculateInvRefMatrices();

		// First step is to update the RefBasesInvMatrix for the bones.
		for (const FInstanceUpdateData::FBone& Bone : BonePose)
		{
			const int32 BoneIndex = BoneInfoMap[Bone.Name].Value;
			RefBasesInvMatrix[BoneIndex] = Bone.MatrixWithScale;
		}

		// The second step is to update the pose transforms in the ref skeleton from the BasesInvMatrix
		FReferenceSkeletonModifier SkeletonModifier(ReferenceSkeleton, Skeleton);
		for (int32 RefSkelBoneIndex = 0; RefSkelBoneIndex < RawRefBoneCount; ++RefSkelBoneIndex)
		{
			int32 ParentBoneIndex = ReferenceSkeleton.GetParentIndex(RefSkelBoneIndex);
			if (ParentBoneIndex >= 0)
			{
				const FTransform3f BonePoseTransform(
						RefBasesInvMatrix[RefSkelBoneIndex].Inverse() * RefBasesInvMatrix[ParentBoneIndex]);

				SkeletonModifier.UpdateRefPoseTransform(RefSkelBoneIndex, (FTransform)BonePoseTransform);
			}
		}

		// Force a CalculateInvRefMatrices
		RefBasesInvMatrix.Empty(RawRefBoneCount);
	}

	{
		MUTABLE_CPUPROFILER_SCOPE(BuildSkeletonData_CalcInvRefMatrices);
		SkeletalMesh.CalculateInvRefMatrices();
	}

	USkeleton* GeneratedSkeleton = SkeletalMesh.GetSkeleton();

	if(GeneratedSkeleton && bCreatedNewSkeleton)
	{
		// If the skeleton is new, it means it has just been merged and the retargeting modes need merging too as the
		// MergeSkeletons function doesn't do it. Only do it for newly generated ones, not for cached or non-transient ones.
		GeneratedSkeleton->RecreateBoneTree(&SkeletalMesh);

		FCustomizableInstanceComponentData* ComponentData = GetComponentData(ObjectComponentIndex);
		check(ComponentData);

		TArray<TObjectPtr<USkeleton>>& SkeletonsToMerge = ComponentData->Skeletons.SkeletonsToMerge;
		check(SkeletonsToMerge.Num() > 1);

		TMap<FName, EBoneTranslationRetargetingMode::Type> BoneNamesToRetargetingMode;

		const int32 NumberOfSkeletons = SkeletonsToMerge.Num();

		for (int32 SkeletonIndex = 0; SkeletonIndex < NumberOfSkeletons; ++SkeletonIndex)
		{
			const USkeleton* ToMergeSkeleton = SkeletonsToMerge[SkeletonIndex];
			const FReferenceSkeleton& ToMergeReferenceSkeleton = ToMergeSkeleton->GetReferenceSkeleton();
			const TArray<FMeshBoneInfo>& Bones = ToMergeReferenceSkeleton.GetRawRefBoneInfo();

			const int32 NumBones = Bones.Num();
			for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
			{
				const FMeshBoneInfo& Bone = Bones[BoneIndex];

				EBoneTranslationRetargetingMode::Type RetargetingMode = ToMergeSkeleton->GetBoneTranslationRetargetingMode(BoneIndex, false);
				BoneNamesToRetargetingMode.Add(Bone.Name, RetargetingMode);
			}
		}

		for (const auto& Pair : BoneNamesToRetargetingMode)
		{
			const FName& BoneName = Pair.Key;
			const EBoneTranslationRetargetingMode::Type& RetargetingMode = Pair.Value;

			const int32 BoneIndex = GeneratedSkeleton->GetReferenceSkeleton().FindRawBoneIndex(BoneName);

			if (BoneIndex >= 0)
			{
				GeneratedSkeleton->SetBoneTranslationRetargetingMode(BoneIndex, RetargetingMode);
			}
		}
	}

	return true;
}


void UCustomizableInstancePrivate::BuildMeshSockets(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh* SkeletalMesh, const UModelResources& ModelResources, const FMutableRefSkeletalMeshData& RefSkeletalMeshData, TSharedPtr<const mu::FMesh> MutableMesh)
{
	// Build mesh sockets.
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::BuildMeshSockets);
	check(SkeletalMesh);

	const uint32 SocketCount = RefSkeletalMeshData.Sockets.Num();

	TArray<TObjectPtr<USkeletalMeshSocket>>& Sockets = SkeletalMesh->GetMeshOnlySocketList();
	Sockets.Empty(SocketCount);
	TMap<FName, TTuple<int32, int32>> SocketMap; // Maps Socket name to Sockets Array index and priority
	
	// Add sockets used by the SkeletalMesh of reference.
	{
		MUTABLE_CPUPROFILER_SCOPE(BuildMeshSockets_RefMeshSockets);
	
		for (uint32 SocketIndex = 0; SocketIndex < SocketCount; ++SocketIndex)
		{
			const FMutableRefSocket& RefSocket = RefSkeletalMeshData.Sockets[SocketIndex];

			USkeletalMeshSocket* Socket = NewObject<USkeletalMeshSocket>(SkeletalMesh, RefSocket.SocketName);

			Socket->SocketName = RefSocket.SocketName;
			Socket->BoneName = RefSocket.BoneName;

			Socket->RelativeLocation = RefSocket.RelativeLocation;
			Socket->RelativeRotation = RefSocket.RelativeRotation;
			Socket->RelativeScale = RefSocket.RelativeScale;

			Socket->bForceAlwaysAnimated = RefSocket.bForceAlwaysAnimated;
			const int32 LastIndex = Sockets.Add(Socket);

			SocketMap.Add(Socket->SocketName, TTuple<int32, int32>(LastIndex, RefSocket.Priority));
		}
	}

	// Add or update sockets modified by Mutable.
	if (MutableMesh)
	{
		MUTABLE_CPUPROFILER_SCOPE(BuildMeshSockets_MutableSockets);

		for (int32 TagIndex = 0; TagIndex < MutableMesh->GetTagCount(); ++TagIndex)
		{
			FString Tag = MutableMesh->GetTag(TagIndex);

			if (Tag.RemoveFromStart("__Socket:"))
			{
				check(Tag.IsNumeric());
				const int32 MutableSocketIndex = FCString::Atoi(*Tag);

				if (ModelResources.SocketArray.IsValidIndex(MutableSocketIndex))
				{
					const FMutableRefSocket& MutableSocket = ModelResources.SocketArray[MutableSocketIndex];
					int32 IndexToWriteSocket = -1;

					if (TTuple<int32, int32>* FoundSocket = SocketMap.Find(MutableSocket.SocketName))
					{
						if (FoundSocket->Value < MutableSocket.Priority)
						{
							// Overwrite the existing socket because the new mesh part one is higher priority
							IndexToWriteSocket = FoundSocket->Key;
							FoundSocket->Value = MutableSocket.Priority;
						}
					}
					else
					{
						// New Socket
						USkeletalMeshSocket* Socket = NewObject<USkeletalMeshSocket>(SkeletalMesh, MutableSocket.SocketName);
						IndexToWriteSocket = Sockets.Add(Socket);
						SocketMap.Add(MutableSocket.SocketName, TTuple<int32, int32>(IndexToWriteSocket, MutableSocket.Priority));
					}

					if (IndexToWriteSocket >= 0)
					{
						check(Sockets.IsValidIndex(IndexToWriteSocket));

						USkeletalMeshSocket* SocketToWrite = Sockets[IndexToWriteSocket];

						SocketToWrite->SocketName = MutableSocket.SocketName;
						SocketToWrite->BoneName = MutableSocket.BoneName;

						SocketToWrite->RelativeLocation = MutableSocket.RelativeLocation;
						SocketToWrite->RelativeRotation = MutableSocket.RelativeRotation;
						SocketToWrite->RelativeScale = MutableSocket.RelativeScale;

						SocketToWrite->bForceAlwaysAnimated = MutableSocket.bForceAlwaysAnimated;
					}
				}
			}
		}
	}

#if !WITH_EDITOR
	SkeletalMesh->RebuildSocketMap();
#endif // !WITH_EDITOR
}


void UCustomizableInstancePrivate::BuildOrCopyElementData(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh* SkeletalMesh, UCustomizableObject& CustomizableObject, 
	FCustomizableObjectInstanceComponentIndex InstanceComponentIndex)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::BuildOrCopyElementData);

	const FInstanceUpdateData::FComponent* Component = OperationData->GetComponentUpdateData(InstanceComponentIndex);
	if (!Component)
	{
		return;
	}

	const FName ComponentName = OperationData->ComponentNames[Component->Id.GetValue()];

	for (int32 LODIndex = OperationData->FirstLODAvailable[ComponentName]; LODIndex < Component->LODCount; ++LODIndex)
	{
		const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[Component->FirstLOD+LODIndex];

		if (!LOD.SurfaceCount)
		{
			continue;
		}

		for (int32 SurfaceIndex = 0; SurfaceIndex < LOD.SurfaceCount; ++SurfaceIndex)
		{
			new(SkeletalMesh->GetResourceForRendering()->LODRenderData[LODIndex].RenderSections) FSkelMeshRenderSection();
		}
	}
}

void UCustomizableInstancePrivate::BuildOrCopyMorphTargetsData(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh* SkeletalMesh, const USkeletalMesh* LastUpdateSkeletalMesh, UCustomizableObject& CustomizableObject, FCustomizableObjectInstanceComponentIndex InstanceComponentIndex)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::BuildOrCopyMorphTargetsData);

	// This is a bit redundant as ComponentMorphTargets should not be generated.
	if (!CVarEnableRealTimeMorphTargets.GetValueOnAnyThread())
	{
		return;
	}

	if (!SkeletalMesh)
	{
		return;
	}
	
	FCustomizableObjectComponentIndex ObjectComponentIndex = OperationData->GetObjectComponentIndex(InstanceComponentIndex);
	const FName ComponentName = OperationData->ComponentNames[ObjectComponentIndex.GetValue()];

	FSkeletalMeshMorphTargets* ComponentMorphTargets = OperationData->InstanceUpdateData.RealTimeMorphTargets.Find(ComponentName);
	if (!ComponentMorphTargets)
	{
		return;
	}

	const int32 NumMorphTargets = ComponentMorphTargets->RealTimeMorphTargetNames.Num();
	
	TArray<TObjectPtr<UMorphTarget>>& MorphTargets = SkeletalMesh->GetMorphTargets();
	MorphTargets.Empty(NumMorphTargets);
	
	for (int32 MorphTargetIndex = 0; MorphTargetIndex < NumMorphTargets; ++MorphTargetIndex)
	{
		TArray<FMorphTargetLODModel>& MorphTargetData = ComponentMorphTargets->RealTimeMorphsLODData[MorphTargetIndex];

		if (MorphTargetData.IsEmpty())
		{
			continue;
		}
		
		const FName& MorphTargetName = ComponentMorphTargets->RealTimeMorphTargetNames[MorphTargetIndex];

		UMorphTarget* NewMorphTarget = NewObject<UMorphTarget>(SkeletalMesh, MorphTargetName);
		NewMorphTarget->BaseSkelMesh = SkeletalMesh;

		TArray<FMorphTargetLODModel>& MorphLODModels = NewMorphTarget->GetMorphLODModels();

		if (OperationData->bStreamMeshLODs)
		{
			MorphLODModels.SetNum(ComponentMorphTargets->RealTimeMorphsLODData[MorphTargetIndex].Num());
			
			// Streamed LODs
			const int32 FirstLODAvailable = OperationData->FirstLODAvailable[ComponentName];
			for (int32 LODIndex = FirstLODAvailable; LODIndex < OperationData->FirstResidentLOD[ComponentName]; ++LODIndex)
			{
				// Copy data required for streaming
				MorphLODModels[LODIndex].NumVertices = 1; // Trick the engine
				MorphLODModels[LODIndex].SectionIndices = MoveTemp(MorphTargetData[LODIndex].SectionIndices);
			}
			
			// Residents LODs
			for (int32 LODIndex = OperationData->GetFirstRequestedLOD()[ComponentName]; LODIndex < OperationData->NumLODsAvailable[ComponentName]; ++LODIndex)
			{
				MorphLODModels[LODIndex] = ComponentMorphTargets->RealTimeMorphsLODData[MorphTargetIndex][LODIndex];
			}
		}
		else
		{
			MorphLODModels = MoveTemp(ComponentMorphTargets->RealTimeMorphsLODData[MorphTargetIndex]);
		}

		MorphTargets.Add(NewMorphTarget);
	}

	// Mutable hacky LOD Streaming
	if (!OperationData->bStreamMeshLODs)
	{
		// Copy MorphTargets from the FirstGeneratedLOD to the LODs below
		const int32 FirstRequestedLOD = OperationData->GetFirstRequestedLOD()[ComponentName];
		for (int32 LODIndex = OperationData->FirstLODAvailable[ComponentName]; LODIndex < FirstRequestedLOD; ++LODIndex)
		{
			MUTABLE_CPUPROFILER_SCOPE(CopyMorphTargetsData);

			for (int32 MorphTargetIndex = 0; MorphTargetIndex < MorphTargets.Num(); ++MorphTargetIndex)
			{
				MorphTargets[MorphTargetIndex]->GetMorphLODModels()[LODIndex] = MorphTargets[MorphTargetIndex]->GetMorphLODModels()[FirstRequestedLOD];
			}
		}
	}

	const bool bInKeepEmptyMorphTargets = OperationData->bStreamMeshLODs;
	SkeletalMesh->InitMorphTargets(bInKeepEmptyMorphTargets); // True to avoid removing streamed Morph Targets.
}

namespace 
{
	// Only used to be able to create new clothing assets and assign a new guid to them without the factory.
	class UCustomizableObjectClothingAsset : public UClothingAssetCommon
	{
	public:
		void AssignNewGuid()
		{
			AssetGuid = FGuid::NewGuid();
		}
	};

}

void UCustomizableInstancePrivate::BuildOrCopyClothingData(
		const TSharedRef<FUpdateContextPrivate>& OperationData, 
		USkeletalMesh* SkeletalMesh, 
		const UModelResources& ModelResources,
		FCustomizableObjectInstanceComponentIndex InstanceComponentIndex,
		const TArray<TObjectPtr<UPhysicsAsset>>& ClothingPhysicsAssets)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::BuildOrCopyClothingData);

	struct FSectionClothMetadata
	{
		int32 SectionIndex;
		int32 LODIndex;
		int32 ClothAssetIndex;
		int32 ClothAssetLodIndex;
		uint32 NumVertices; // Upper bound
	};

	struct FPerClothAssetData
	{
		int32 MinLOD = 0;
		TArray<TArray<int32, TInlineAllocator<8>>, TInlineAllocator<8>> AttachedSections; // Indices in SectionsWithCloth for render sections attached to this ClothAsset.
		FName Name;
		UPhysicsAsset* PhysicsAsset = nullptr;
		UClothingAssetCommon* ClothingAsset = nullptr;
	};
	
	const TArray<FCustomizableObjectClothingAssetData>& ClothingAssetsData = ModelResources.ClothingAssetsData;
	const TArray<FCustomizableObjectClothConfigData>& ClothSharedConfigsData = ModelResources.ClothSharedConfigsData;

	if (!(ClothingAssetsData.Num() && OperationData->InstanceUpdateData.ClothingMeshData.Num()))
	{
		return;
	}

	const FInstanceUpdateData::FComponent* Component = OperationData->GetComponentUpdateData(InstanceComponentIndex);
	if (!Component)
	{
		return;
	}

	const FName& ComponentName = OperationData->ComponentNames[Component->Id.GetValue()];

	TArray<FSectionClothMetadata> SectionClothMetadata; // Sections must be sorted ascending
	SectionClothMetadata.Reserve(32);
	
	TBitArray<> LODsWithClothing;
	LODsWithClothing.Init(false, Component->LODCount);

	// Keep in mind that clothing does not do the Hacky Mutable Streaming Copy. This is because LOD data can not be shared between LODs.
	// This means that LOD loops are a bit different form usual. With the hacky Mutable streaming, we must generate the requested and the hacky copied ones.
	
	// Metadata
	{
		int32 NumClothingDataNotFound = 0;
			
		for (int32 LODIndex = OperationData->FirstLODAvailable[ComponentName]; LODIndex < Component->LODCount; ++LODIndex)
		{
			const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[Component->FirstLOD+LODIndex];

			if (TSharedPtr<const mu::FMesh> MutableMesh = LOD.Mesh)
			{
				for (int32 SectionIndex = 0; SectionIndex < MutableMesh->GetSurfaceCount(); ++SectionIndex)
				{
					if (!MutableMesh->Surfaces[SectionIndex].SubMeshes.IsValidIndex(0))
					{
						continue;
					}
					
					const mu::FSurfaceSubMesh& SubMesh = MutableMesh->Surfaces[SectionIndex].SubMeshes[0];
					const uint32 ClothResourceId = ModelResources.MeshMetadata[SubMesh.ExternalId].ClothingMetadataId;

					if (ClothResourceId == 0)
					{
						continue;
					}

					if (MutableMesh->Surfaces[SectionIndex].SubMeshes.Num() > 1)
					{
						UE_LOG(LogMutable, Error, TEXT("Section %d has more than one submesh! Skipping section."), SectionIndex);
						continue;
					}
					
					FClothingMeshData* SectionClothingData = OperationData->InstanceUpdateData.ClothingMeshData.Find(ClothResourceId);

					if (!SectionClothingData)
					{
						++NumClothingDataNotFound;
						continue;
					}

					check(SectionClothingData->ClothingAssetIndex != INDEX_NONE);
					check(SectionClothingData->ClothingAssetLOD != INDEX_NONE);

					const int32 ClothAssetIndex = SectionClothingData->ClothingAssetIndex;
					const int32 ClothAssetLodIndex = SectionClothingData->ClothingAssetLOD;

					check(SectionClothingData->ClothingAssetIndex == ClothAssetIndex);

					// Defensive check, this indicates the clothing data might be stale and needs to be recompiled.
					// Should never happen.
					if (!ensure(ClothAssetIndex >= 0 && ClothAssetIndex < ClothingAssetsData.Num() && 
								ClothingAssetsData[ClothAssetIndex].LodData.Num()))
					{
						continue;
					}

					const uint32 NumVertices = SubMesh.VertexEnd - SubMesh.VertexBegin;
					SectionClothMetadata.Add(FSectionClothMetadata { SectionIndex, LODIndex, ClothAssetIndex, ClothAssetLodIndex, NumVertices });

					LODsWithClothing[LODIndex] = true;
				}
			}
		}
		
		if (NumClothingDataNotFound > 0)
		{
			UE_LOG(LogMutable, Error, TEXT("Some clothing data could not be loaded properly, clothing assets may not behave as expected."));
		}
	}

	// No clothing, early out.
	if (!SectionClothMetadata.Num())
	{
		return;
	}
	
	TMap<int32, FPerClothAssetData> PerClothAssetData;
	PerClothAssetData.Reserve(32);

	// Per Cloth Asset data
	{
		// Gather attached sections clothing asset LOD.
		for (int32 MetadataIndex = 0; MetadataIndex < SectionClothMetadata.Num(); ++MetadataIndex)
		{
			const FSectionClothMetadata& SectionClothing = SectionClothMetadata[MetadataIndex];
			FPerClothAssetData& AssetData = PerClothAssetData.FindOrAdd(SectionClothing.ClothAssetIndex); 

			AssetData.MinLOD = FMath::Min(AssetData.MinLOD, SectionClothing.ClothAssetLodIndex);

			int32 MaxLOD = FMath::Max<int32>(AssetData.AttachedSections.Num() - 1, SectionClothing.ClothAssetLodIndex);

			AssetData.AttachedSections.SetNum(MaxLOD + 1);
			AssetData.AttachedSections[SectionClothing.ClothAssetLodIndex].Add(MetadataIndex);
		}

		for (TPair<int32, FPerClothAssetData>& Data : PerClothAssetData)
		{
			int32 ClothAssetIndex = Data.Key;
			FPerClothAssetData& ClothAssetData = Data.Value; 
	
			ClothAssetData.Name = ClothingAssetsData[ClothAssetIndex].Name;
			ClothAssetData.PhysicsAsset = ClothingPhysicsAssets[ClothAssetIndex];
		}
	}
	
	TArray<UnrealConversionUtils::FSectionClothData> SectionsClothData; // Sorted by LOD, Section
	SectionsClothData.Reserve(32);
	
	// Data
	{
		MUTABLE_CPUPROFILER_SCOPE(DiscoverSectionsWithCloth);

		int32 NumClothingDataNotFound = 0;

		for (int32 LODIndex = OperationData->FirstResidentLOD[ComponentName]; LODIndex < Component->LODCount; ++LODIndex)
		{
			const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[Component->FirstLOD+LODIndex];

			if (TSharedPtr<const mu::FMesh> MutableMesh = LOD.Mesh)
			{
				GetSectionClothData(*MutableMesh, LODIndex, OperationData->InstanceUpdateData.ClothingMeshData, SectionsClothData, NumClothingDataNotFound);
			}
		}

		if (NumClothingDataNotFound > 0)
		{
			UE_LOG(LogMutable, Error, TEXT("Some clothing data could not be loaded properly, clothing assets may not behave as expected."));
		}
		
		CopyMeshToMeshClothData(SectionsClothData);
	}

	// Create Clothing Assets
	{ 
		MUTABLE_CPUPROFILER_SCOPE(CreateClothingAssets)

		auto CreateNewClothConfigFromData = [](UObject* Outer, const FCustomizableObjectClothConfigData& ConfigData) -> UClothConfigCommon* 
		{
			UClass* ClothConfigClass = FindObject<UClass>(nullptr, *ConfigData.ClassPath);
			if (ClothConfigClass)
			{
				UClothConfigCommon* ClothConfig = NewObject<UClothConfigCommon>(Outer, ClothConfigClass);
				if (ClothConfig)
				{
					FMemoryReaderView MemoryReader(ConfigData.ConfigBytes);
					ClothConfig->Serialize(MemoryReader);

					return ClothConfig;
				}
			}

			return nullptr;
		};

		TArray<TTuple<FName, UClothConfigCommon*>> SharedConfigs;
		SharedConfigs.Reserve(ClothSharedConfigsData.Num());
 
		for (const FCustomizableObjectClothConfigData& ConfigData : ClothSharedConfigsData)
		{
			UClothConfigCommon* ClothConfig = CreateNewClothConfigFromData(SkeletalMesh, ConfigData);
			if (ClothConfig)
			{
				SharedConfigs.Emplace(ConfigData.ConfigName, ClothConfig);
			}
		}
	
		bool bAllNamesUnique = true;
		TArray<FName, TInlineAllocator<8>> UniqueAssetNames;

		for (TPair<int32, FPerClothAssetData>& AssetData : PerClothAssetData)
		{
			const int32 PrevNumUniqueElems = UniqueAssetNames.Num();
			const int32 ElemIndex = UniqueAssetNames.AddUnique(AssetData.Value.Name);

			if (ElemIndex < PrevNumUniqueElems)
			{
				bAllNamesUnique = false;
				break;
			}
		}

		for (TPair<int32, FPerClothAssetData>& AssetData : PerClothAssetData)
		{
			int32 AssetIndex = AssetData.Key;
			FPerClothAssetData& ClothAssetData = AssetData.Value;  

			FName ClothingAssetObjectName = bAllNamesUnique 
					? ClothAssetData.Name
					: FName(FString::Printf(TEXT("%s_%d"), *ClothAssetData.Name.ToString(), AssetIndex));

			UCustomizableObjectClothingAsset* NewClothingAsset = NewObject<UCustomizableObjectClothingAsset>(SkeletalMesh, ClothingAssetObjectName);
			NewClothingAsset->AssignNewGuid();
		
			const int32 NumClothLODs = ClothingAssetsData[AssetIndex].LodData.Num() - ClothAssetData.MinLOD;

			NewClothingAsset->LodData.SetNum(NumClothLODs);
			for (int32 LODIndex = 0; LODIndex < NumClothLODs; ++LODIndex)
			{
				NewClothingAsset->LodData[LODIndex] = ClothingAssetsData[AssetIndex].LodData[LODIndex + ClothAssetData.MinLOD];
			}

			// Reconstruct clothing asset lod map.
			NewClothingAsset->LodMap.Init(INDEX_NONE, Component->LODCount);
			for (int32 LODIndex = 0; LODIndex < NumClothLODs; ++LODIndex)
			{
				for (int32 SectionWithClothIndex : ClothAssetData.AttachedSections[LODIndex])
				{
					NewClothingAsset->LodMap[SectionClothMetadata[SectionWithClothIndex].LODIndex] = LODIndex; 
				}
			}

			NewClothingAsset->UsedBoneIndices = ClothingAssetsData[AssetIndex].UsedBoneIndices;
			NewClothingAsset->UsedBoneNames = ClothingAssetsData[AssetIndex].UsedBoneNames;
			NewClothingAsset->ReferenceBoneIndex = ClothingAssetsData[AssetIndex].ReferenceBoneIndex;
			NewClothingAsset->RefreshBoneMapping(SkeletalMesh);
			NewClothingAsset->CalculateReferenceBoneIndex();	
			NewClothingAsset->PhysicsAsset = ClothAssetData.PhysicsAsset;

			for (const FCustomizableObjectClothConfigData& ConfigData : ClothingAssetsData[AssetIndex].ConfigsData)
			{
				UClothConfigCommon* ClothConfig = CreateNewClothConfigFromData(NewClothingAsset, ConfigData);
				if (ClothConfig)
				{
					NewClothingAsset->ClothConfigs.Add(ConfigData.ConfigName, ClothConfig);
				}
			}

			for (const TTuple<FName, UClothConfigCommon*>& SharedConfig : SharedConfigs)
			{
				NewClothingAsset->ClothConfigs.Add(SharedConfig);
			}

			ClothAssetData.ClothingAsset = NewClothingAsset;
			SkeletalMesh->GetMeshClothingAssets().AddUnique(NewClothingAsset);
		}
	}

	const bool bAllowClothingPhysicsEdits = !bDisableClothingPhysicsEditsPropagation &&
		ModelResources.bAllowClothingPhysicsEditsPropagation &&
		!OperationData->bStreamMeshLODs;
	
	if (bAllowClothingPhysicsEdits)
	{
		if (IMutableClothingModule* MutableClothingModule = FModuleManager::GetModulePtr<IMutableClothingModule>(MUTABLE_CLOTHING_MODULE_NAME))
		{
			for (TPair<int32, FPerClothAssetData>& Data : PerClothAssetData)
			{
				FPerClothAssetData& ClothAssetData = Data.Value; 
				UClothingAssetCommon* ClothingAsset = ClothAssetData.ClothingAsset;
			
				if (!ClothingAsset)
				{
					continue;
				}

				bool bNeedsLodTransitionUpdate = false;
				for (int32 LODIndex = 0; LODIndex < ClothingAsset->LodData.Num(); ++LODIndex)
				{
					TArray<TArrayView<FMeshToMeshVertData>, TInlineAllocator<8>> MeshToMeshDataViews;

					for (int32 AttachedSectionIndex : ClothAssetData.AttachedSections[LODIndex])
					{
						MeshToMeshDataViews.Add(MakeArrayView(SectionsClothData[AttachedSectionIndex].MappingData));
					}

					bool bModified = MutableClothingModule->UpdateClothSimulationLOD(
							LODIndex, *ClothingAsset, MakeConstArrayView(MeshToMeshDataViews));
				
					bNeedsLodTransitionUpdate = bNeedsLodTransitionUpdate || bModified;
				}

				if (bNeedsLodTransitionUpdate)
				{
					// This needs to happen after all LODs have been processed.
					for (int32 LODIndex = 0; LODIndex < ClothingAsset->LodData.Num(); ++LODIndex)
					{
						MutableClothingModule->FixLODTransitionMappings(LODIndex, *ClothingAsset);
					}
				}
			}	
		}
		else
		{
			UE_LOG(LogMutable, Warning, TEXT("MutableClothing plugin could not be found. Make sure the plugin is enabled if you want to use advanced clothing features."));
		}
	}

	TArray<TArray<FMeshToMeshVertData>> ResidentLODMappingData;
	ResidentLODMappingData.SetNum(Component->LODCount);

	TArray<TArray<FClothBufferIndexMapping>> ResidentLODClothIndexMapping;
	ResidentLODClothIndexMapping.SetNum(Component->LODCount);

	// Zero all LODs (even those which do not use cloth).
	for (int32 LODIndex = OperationData->FirstLODAvailable[ComponentName]; LODIndex < Component->LODCount; ++LODIndex)
	{
		const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[Component->FirstLOD + LODIndex];
		
		TSharedPtr<const mu::FMesh> MutableMesh = LOD.Mesh;
		if (!MutableMesh)
		{
			continue;
		}
		
		ResidentLODClothIndexMapping[LODIndex].SetNumZeroed(MutableMesh->GetSurfaceCount());
	}

	// Create the mapping of cloth LODs.
	for (const UnrealConversionUtils::FSectionClothData& Data : SectionsClothData)
	{
		CreateClothMapping(Data, ResidentLODMappingData[Data.LODIndex], ResidentLODClothIndexMapping[Data.LODIndex]);
	}

	FSkeletalMeshRenderData* RenderResource = SkeletalMesh->GetResourceForRendering();
	{
		MUTABLE_CPUPROFILER_SCOPE(InitClothRenderData)

		// Streamed
		for (int32 LODIndex = OperationData->FirstLODAvailable[ComponentName]; LODIndex < OperationData->FirstResidentLOD[ComponentName]; ++LODIndex)
		{
			FSkeletalMeshLODRenderData& LODModel = RenderResource->LODRenderData[LODIndex];

			if (LODsWithClothing[LODIndex])
			{
				TArray<FClothBufferIndexMapping> ClothIndexMapping;
				ClothIndexMapping.SetNumZeroed(LODModel.RenderSections.Num());

				int32 Stride = sizeof(FMeshToMeshVertData);
			
				int32 NumVertices = 0; // Upper bound
				for (const FSectionClothMetadata& Metadata : SectionClothMetadata)
				{
					if (Metadata.LODIndex == LODIndex)
					{
						// Based on FSkeletalMeshLODModel::GetClothMappingData().

						FSkelMeshRenderSection& RenderSection = LODModel.RenderSections[Metadata.SectionIndex];
						
						check(Metadata.NumVertices == RenderSection.NumVertices); // Both values are upper bounds since we can not know the exact number of vertices without executing the code. 
						
						ClothIndexMapping[Metadata.SectionIndex].BaseVertexIndex = RenderSection.BaseVertexIndex;
						ClothIndexMapping[Metadata.SectionIndex].MappingOffset = NumVertices;
						ClothIndexMapping[Metadata.SectionIndex].LODBiasStride = Metadata.NumVertices;
						
						NumVertices += Metadata.NumVertices;
					}
				}
				
				LODModel.ClothVertexBuffer.SetMetadata(ClothIndexMapping, Stride, NumVertices);
			}
		}


		// Resident
		for (int32 LODIndex = OperationData->FirstResidentLOD[ComponentName]; LODIndex < OperationData->NumLODsAvailable[ComponentName]; ++LODIndex)
		{
			FSkeletalMeshLODRenderData& LODModel = RenderResource->LODRenderData[LODIndex];
	
			if (LODsWithClothing[LODIndex])
			{
				LODModel.ClothVertexBuffer.Init(ResidentLODMappingData[LODIndex], ResidentLODClothIndexMapping[LODIndex]);
			}
		}
	}
	
	for (const FSectionClothMetadata& Metadata : SectionClothMetadata)
	{
		FSkeletalMeshLODRenderData& LODModel = RenderResource->LODRenderData[Metadata.LODIndex];
		FSkelMeshRenderSection& SectionData = LODModel.RenderSections[Metadata.SectionIndex];

		// Ideally we would copy the data of all LODs, but we do not have this information in the initial generation. In any case,
		// ClothMappingDataLODs is only used for CPU Skinning, and some engine checks (they only check the array size).
		// The size must be a multiple of SectionData.NumVertices. Currently Mutable only supports one influence per vertex (NumVertices * 1).
		SectionData.ClothMappingDataLODs.AddDefaulted(1);
		SectionData.ClothMappingDataLODs[0].SetNum(SectionData.NumVertices); // = MoveTemp(SectionWithCloth.MappingData);

		FPerClothAssetData& AssetData = PerClothAssetData.FindChecked(Metadata.ClothAssetIndex);

		SectionData.CorrespondClothAssetIndex = SkeletalMesh->GetClothingAssetIndex(AssetData.ClothingAsset);
		SectionData.ClothingData.AssetGuid = AssetData.ClothingAsset->GetAssetGuid();
		SectionData.ClothingData.AssetLodIndex = AssetData.ClothingAsset->LodMap[Metadata.LODIndex];
	}
	
	SkeletalMesh->SetHasActiveClothingAssets(!SectionClothMetadata.IsEmpty());
}


bool UCustomizableInstancePrivate::BuildOrCopyRenderData(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh* SkeletalMesh, const USkeletalMesh* LastUpdateSkeletalMesh, 
	UCustomizableObjectInstance* Public, FCustomizableObjectInstanceComponentIndex InstanceComponentIndex)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::BuildOrCopyRenderData);

	FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
	check(RenderData);

	UCustomizableObject* CustomizableObject = Public->GetCustomizableObject();

	// It must be not null as it's checked in the calling function
	check(CustomizableObject);

	const FInstanceUpdateData::FComponent* Component = OperationData->GetComponentUpdateData(InstanceComponentIndex);
	if (!Component)
	{
		return false;
	}

	const UModelResources& ModelResources = CustomizableObject->GetPrivate()->GetModelResourcesChecked();
	const FName ComponentName = ModelResources.ComponentNamesPerObjectComponent[Component->Id.GetValue()];

	for (int32 LODIndex = OperationData->GetFirstRequestedLOD()[ComponentName]; LODIndex < Component->LODCount; ++LODIndex)
	{
		const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[Component->FirstLOD+LODIndex];

		if (!LOD.Mesh || LOD.SurfaceCount == 0)
		{
			continue;	
		}

		SetLastMeshId(Component->Id, LODIndex, LOD.MeshID);
	}
	
	const int32 FirstLOD = OperationData->bStreamMeshLODs ?
		OperationData->FirstLODAvailable[ComponentName] :
		OperationData->GetFirstRequestedLOD()[ComponentName];
	
	for (int32 LODIndex = FirstLOD; LODIndex < Component->LODCount; ++LODIndex)
	{
		MUTABLE_CPUPROFILER_SCOPE(BuildRenderData);
		
		const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[Component->FirstLOD+LODIndex];

		// There could be components without a mesh in LODs
		if (!LOD.Mesh || LOD.SurfaceCount == 0)
		{
			UE_LOG(LogMutable, Warning, TEXT("Building instance: generated mesh [%s] has LOD [%d] of object component index [%d] with no mesh.")
				, *SkeletalMesh->GetName()
				, LODIndex
				, Component->Id.GetValue());

			// End with failure
			return false;
		}

		TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("BuildRenderData: Component index %d, LOD %d"), Component->Id.GetValue(), LODIndex));
		
		FSkeletalMeshLODRenderData& LODResource = RenderData->LODRenderData[LODIndex];

		const TMap<mu::FBoneName, TPair<FName, uint16>>& BoneInfoMap = OperationData->InstanceUpdateData.SkeletonsPerInstanceComponent[InstanceComponentIndex.GetValue()].BoneInfoMap;
		
		// Set active and required bones
		{
			const TArray<mu::FBoneName>& ActiveBones = OperationData->InstanceUpdateData.ActiveBones;
			LODResource.ActiveBoneIndices.Reserve(LOD.ActiveBoneCount);

			for (uint32 Index = 0; Index < LOD.ActiveBoneCount; ++Index)
			{
				const uint16 ActiveBoneIndex = BoneInfoMap[ActiveBones[LOD.FirstActiveBone + Index]].Value;
				LODResource.ActiveBoneIndices.Add(ActiveBoneIndex);
			}

			LODResource.RequiredBones = LODResource.ActiveBoneIndices;
			LODResource.RequiredBones.Sort();
		}

		// Find referenced surface metadata.
		const int32 MeshNumSurfaces = LOD.Mesh->Surfaces.Num();
		TArray<const FMutableSurfaceMetadata*> MeshSurfacesMetadata;
		MeshSurfacesMetadata.Init(nullptr, MeshNumSurfaces);

		for (int32 MeshSectionIndex = 0; MeshSectionIndex < MeshNumSurfaces; ++MeshSectionIndex)
		{
			uint32 MeshSurfaceId = LOD.Mesh->GetSurfaceId(MeshSectionIndex);
			int32 InstanceSurfaceIndex = OperationData->MutableInstance->FindSurfaceById(InstanceComponentIndex.GetValue(), LODIndex, MeshSurfaceId);
			
			if (InstanceSurfaceIndex < 0)
			{
				continue;
			}
			
			uint32 SurfaceMetadataId = OperationData->MutableInstance->GetSurfaceCustomId(InstanceComponentIndex.GetValue(), LODIndex, InstanceSurfaceIndex);
			
			uint32 UsedSurfaceMetadataId = 0;
			if (SurfaceMetadataId != 0)
			{
				UsedSurfaceMetadataId = SurfaceMetadataId;
			}
			else
			{
				// In case the surface does not have metadata, check if any submesh has surface metadata.
				for (const mu::FSurfaceSubMesh& SubMesh : LOD.Mesh->Surfaces[MeshSectionIndex].SubMeshes)	
				{
					const FMutableMeshMetadata* FoundMeshMetadata = ModelResources.MeshMetadata.Find(SubMesh.ExternalId);

					if (!FoundMeshMetadata)
					{
						continue;
					}

					UsedSurfaceMetadataId = FoundMeshMetadata->SurfaceMetadataId; 

					if (UsedSurfaceMetadataId != 0)
					{
						break;
					}
				}
			}	
			
			MeshSurfacesMetadata[MeshSectionIndex] = ModelResources.SurfaceMetadata.Find(UsedSurfaceMetadataId);
		}

		// Set RenderSections
		UnrealConversionUtils::SetupRenderSections(
			LODResource,
			LOD.Mesh.Get(),
			OperationData->InstanceUpdateData.BoneMaps,
			BoneInfoMap,
			LOD.FirstBoneMap,
			MeshSurfacesMetadata);

		// Set SkinWeightProfiles
		LODResource.SkinWeightProfilesData.Init(&LODResource.SkinWeightVertexBuffer);
		
		// Active SkinWeightProfiles ID and Name
		TArray<TPair<uint32, FName>> ActiveSkinWeightProfiles;
		
		const mu::FMeshBufferSet& MutableMeshVertexBuffers = LOD.Mesh->GetVertexBuffers();
		const int32 NumBuffers = MutableMeshVertexBuffers.GetBufferCount();

		for (int32 BufferIndex = 0; BufferIndex < NumBuffers; ++BufferIndex)
		{
			if (MutableMeshVertexBuffers.Buffers[BufferIndex].Channels.IsEmpty())
			{
				continue;
			}

			mu::EMeshBufferSemantic Semantic;
			int32 SemanticIndex;
			MutableMeshVertexBuffers.GetChannel(BufferIndex, 0, &Semantic, &SemanticIndex, nullptr, nullptr, nullptr);

			if (Semantic != mu::EMeshBufferSemantic::AltSkinWeight)
			{
				continue;
			}

			const FMutableSkinWeightProfileInfo* ProfileInfo = ModelResources.SkinWeightProfilesInfo.FindByPredicate(
				[&SemanticIndex](const FMutableSkinWeightProfileInfo& P) { return P.NameId == SemanticIndex; });

			if (ensure(ProfileInfo))
			{
				const FSkinWeightProfileInfo* ExistingProfile = SkeletalMesh->GetSkinWeightProfiles().FindByPredicate(
					[&ProfileInfo](const FSkinWeightProfileInfo& P) { return P.Name == ProfileInfo->Name; });

				if (!ExistingProfile)
				{
					SkeletalMesh->AddSkinWeightProfile({ ProfileInfo->Name, ProfileInfo->DefaultProfile, ProfileInfo->DefaultProfileFromLODIndex });
				}

				ActiveSkinWeightProfiles.Add({ ProfileInfo->NameId, ProfileInfo->Name });

				LODResource.SkinWeightProfilesData.AddOverrideData(ProfileInfo->Name);

			}
		}

		if (LODResource.bStreamedDataInlined) // Non-streamable LOD
		{
			// Copy Vertices
			UnrealConversionUtils::CopyMutableVertexBuffers(
				LODResource,
				LOD.Mesh.Get(),
				SkeletalMesh->GetLODInfo(LODIndex)->bAllowCPUAccess);

			// SurfaceIDs. Required to copy index buffers with padding
			TArray<uint32> SurfaceIDs;
			SurfaceIDs.SetNum(LOD.SurfaceCount);

			for (int32 SurfaceIndex = 0; SurfaceIndex < LOD.SurfaceCount; ++SurfaceIndex)
			{
				SurfaceIDs[SurfaceIndex] = LOD.Mesh->GetSurfaceId(SurfaceIndex);
			}

			// Copy indices.
			bool bMarkRenderStateDirty = false;
			if (!UnrealConversionUtils::CopyMutableIndexBuffers(LODResource, LOD.Mesh.Get(), SurfaceIDs, bMarkRenderStateDirty))
			{
				// End with failure
				return false;
			}

			// Copy SkinWeightProfiles
			UnrealConversionUtils::CopyMutableSkinWeightProfilesBuffers(
				LODResource,
				*SkeletalMesh,
				LODIndex,
				LOD.Mesh.Get(),
				ActiveSkinWeightProfiles);
		}
		else // Streamable LOD. 
		{
			// Init VertexBuffers for streaming
			UnrealConversionUtils::InitVertexBuffersWithDummyData(
				LODResource,
				LOD.Mesh.Get(),
				SkeletalMesh->GetLODInfo(LODIndex)->bAllowCPUAccess);

			// Init IndexBuffers for streaming
			UnrealConversionUtils::InitIndexBuffersWithDummyData(LODResource, LOD.Mesh.Get());
		}

		if (LODResource.StaticVertexBuffers.ColorVertexBuffer.GetNumVertices())
		{
			SkeletalMesh->SetHasVertexColors(true);
		}

		if (LODResource.DoesVertexBufferUse16BitBoneIndex() && !UCustomizableObjectSystem::GetInstance()->IsSupport16BitBoneIndexEnabled())
		{
			OperationData->UpdateResult = EUpdateResult::Error16BitBoneIndex;

			const FString Msg = FString::Printf(TEXT("Customizable Object [%s] requires of Skinning - 'Support 16 Bit Bone Index' to be enabled. Please, update the Project Settings."),
				*CustomizableObject->GetName());
			UE_LOG(LogMutable, Error, TEXT("%s"), *Msg);

#if WITH_EDITOR
			FNotificationInfo Info(FText::FromString(Msg));
			Info.bFireAndForget = true;
			Info.FadeOutDuration = 1.0f;
			Info.ExpireDuration = 10.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
#endif
		}
	}
	

	// Mutable hacky LOD Streaming
	if (!OperationData->bStreamMeshLODs)
	{
		// Copy LODRenderData from the FirstRequestedLOD to the LODs below
		const int32 FirstRequestedLOD = OperationData->GetFirstRequestedLOD()[ComponentName];
		for (int32 LODIndex = OperationData->FirstLODAvailable[ComponentName]; LODIndex < FirstRequestedLOD; ++LODIndex)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("CopyRenderData: From LOD %d to LOD %d"), FirstRequestedLOD, LODIndex));

			// Render Data will be reused from the previously generated component
			FSkeletalMeshLODRenderData& SourceLODResource = RenderData->LODRenderData[FirstRequestedLOD];
			FSkeletalMeshLODRenderData& LODResource = RenderData->LODRenderData[LODIndex];

			UnrealConversionUtils::CopySkeletalMeshLODRenderData(
				LODResource,
				SourceLODResource,
				*SkeletalMesh,
				LODIndex,
				SkeletalMesh->GetLODInfo(LODIndex)->bAllowCPUAccess
			);
		}
	}

	return true;
}


UE::Tasks::FTask UCustomizableInstancePrivate::LoadAdditionalAssetsAndData(const TSharedRef<FUpdateContextPrivate>& OperationData)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::LoadAdditionalAssetsAndDataAsync);

	UCustomizableObject* CustomizableObject = GetPublic()->GetCustomizableObject();

	const UModelResources& ModelResources = CustomizableObject->GetPrivate()->GetModelResourcesChecked();
	const TSharedPtr<FModelStreamableBulkData>& ModelStreamableBulkData = CustomizableObject->GetPrivate()->GetModelStreamableBulkData();
	
	FMutableStreamRequest StreamRequest(ModelStreamableBulkData);

	TArray<FSoftObjectPath> AssetsToStream;

	TArray<FInstanceUpdateData::FComponent>& Components = OperationData->InstanceUpdateData.Components;

	ObjectToInstanceIndexMap.Empty();
	ReferencedMaterials.Empty();

	const int32 NumClothingAssets = ModelResources.ClothingAssetsData.Num();
	ClothingPhysicsAssets.Reset(NumClothingAssets);
	ClothingPhysicsAssets.SetNum(NumClothingAssets);

	GatheredAnimBPs.Empty();
	AnimBPGameplayTags.Reset();
	AnimBpPhysicsAssets.Reset();
	
	for (const FInstanceUpdateData::FSurface& Surface : OperationData->InstanceUpdateData.Surfaces)
	{
		const int32 MaterialIndex = Surface.MaterialIndex;
		if (MaterialIndex<0 || ObjectToInstanceIndexMap.Contains(MaterialIndex))
		{
			continue;
		}

		TSoftObjectPtr<UMaterialInterface> AssetPtr = ModelResources.Materials.IsValidIndex(MaterialIndex) ? ModelResources.Materials[MaterialIndex] : nullptr;
		UMaterialInterface* LoadedMaterial = AssetPtr.Get();

		const int32 ReferencedMaterialsIndex = ReferencedMaterials.Add(LoadedMaterial);
		ObjectToInstanceIndexMap.Add(MaterialIndex, ReferencedMaterialsIndex);

		if (!LoadedMaterial && !AssetPtr.IsNull())
		{
			AssetsToStream.Add(AssetPtr.ToSoftObjectPath());
		}
	}

	for (const FInstanceUpdateData::FComponent& Component : OperationData->InstanceUpdateData.Components)
	{
		if (Component.OverlayMaterial == INDEX_NONE || ObjectToInstanceIndexMap.Contains(Component.OverlayMaterial))
		{
			continue;
		}

		TSoftObjectPtr<UMaterialInterface> AssetPtr = ModelResources.Materials.IsValidIndex(Component.OverlayMaterial) ? ModelResources.Materials[Component.OverlayMaterial] : nullptr;
		UMaterialInterface* LoadedMaterial = AssetPtr.Get();

		const int32 ReferencedMaterialsIndex = ReferencedMaterials.Add(LoadedMaterial);
		ObjectToInstanceIndexMap.Add(Component.OverlayMaterial, ReferencedMaterialsIndex);

		if (!LoadedMaterial && !AssetPtr.IsNull())
		{
			AssetsToStream.Add(AssetPtr.ToSoftObjectPath());
		}
	}


	// Load Skeletons required by the SubMeshes of the newly generated Mesh, will be merged later
	for (int32 InstanceComponentIndex = 0; InstanceComponentIndex < OperationData->NumInstanceComponents; ++InstanceComponentIndex)
	{
		FCustomizableObjectComponentIndex ObjectComponentIndex = OperationData->GetObjectComponentIndex(FCustomizableObjectInstanceComponentIndex(InstanceComponentIndex));
		if (!ObjectComponentIndex.IsValid())
		{
			continue;
		}

		const FInstanceUpdateData::FSkeletonData& SkeletonData = OperationData->InstanceUpdateData.SkeletonsPerInstanceComponent[InstanceComponentIndex];
		
		FCustomizableInstanceComponentData* ComponentData = GetComponentData(ObjectComponentIndex);
		if (!ComponentData)
		{
			continue;
		}

		// Reuse merged Skeleton if cached
		ComponentData->Skeletons.Skeleton = CustomizableObject->GetPrivate()->SkeletonCache.Get(SkeletonData.SkeletonIds);
		if (ComponentData->Skeletons.Skeleton)
		{
			ComponentData->Skeletons.SkeletonIds.Empty();
			ComponentData->Skeletons.SkeletonsToMerge.Empty();
			continue;
		}

		// Add Skeletons to merge
		for (const uint32 SkeletonId : SkeletonData.SkeletonIds)
		{
			TSoftObjectPtr<USkeleton> AssetPtr = ModelResources.Skeletons.IsValidIndex(SkeletonId) ? ModelResources.Skeletons[SkeletonId] : nullptr;
			if (AssetPtr.IsNull())
			{
				continue;
			}

			// Add referenced skeletons to the assets to stream
			ComponentData->Skeletons.SkeletonIds.Add(SkeletonId);

			if (USkeleton* Skeleton = AssetPtr.Get())
			{
				ComponentData->Skeletons.SkeletonsToMerge.Add(Skeleton);
			}
			else
			{
				AssetsToStream.Add(AssetPtr.ToSoftObjectPath());
			}
		}
	}
	
	bool bHasInvalidMesh = false;
	const bool bUpdateMeshes = DoComponentsNeedUpdate(GetPublic(), OperationData, bHasInvalidMesh);

	// Load assets coming from SubMeshes of the newly generated Mesh
	if (OperationData->InstanceUpdateData.LODs.Num())
	{
		for (int32 InstanceComponentIndex = 0; InstanceComponentIndex < OperationData->InstanceUpdateData.Components.Num(); ++InstanceComponentIndex)
		{
			const FInstanceUpdateData::FComponent& Component = Components[InstanceComponentIndex];
			FCustomizableInstanceComponentData* ComponentData = GetComponentData(Component.Id);

			TSharedPtr<const mu::FMesh> FirstComponentMesh = OperationData->InstanceUpdateData.LODs.IsValidIndex(Component.FirstLOD) ?
				OperationData->InstanceUpdateData.LODs[Component.FirstLOD].Mesh :
				nullptr;

			if (FirstComponentMesh && FirstComponentMesh->IsReference())
			{
				// Pass-through components don't have a Reference Mesh so don't access it
				continue;
			}
			
			const FCustomizableObjectComponentIndex ObjectComponentIndex = Component.Id;
			const FMutableRefSkeletalMeshData& RefSkeletalMeshData = ModelResources.ReferenceSkeletalMeshesData[ObjectComponentIndex.GetValue()];

			for (int32 AssetUserDataIndex : RefSkeletalMeshData.AssetUserDataIndices)
			{
#if !WITH_EDITOR
				OperationData->StreamedResourceIndex.AddUnique(AssetUserDataIndex); // Used to hold/release streamed resources in non-editor builds.
#endif
				ComponentData->StreamedResourceIndex.AddUnique(AssetUserDataIndex);
			}

			const FName ComponentName = OperationData->ComponentNames[Component.Id.GetValue()];

			if (bUpdateMeshes)
			{
				// Morphs
				{
					// Data
					for (int32 LODIndex = OperationData->GetFirstRequestedLOD()[ComponentName]; LODIndex < Component.LODCount; ++LODIndex)
					{
						const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[Component.FirstLOD + LODIndex];

						TSharedPtr<const mu::FMesh> MutableMesh = LOD.Mesh;
						if (!MutableMesh)
						{
							continue;
						}

						LoadMorphTargetsData(StreamRequest, MutableMesh.ToSharedRef(), OperationData->InstanceUpdateData.RealTimeMorphTargetMeshData);
					}

					// Metadata
					const int32 FirstLOD = OperationData->bStreamMeshLODs ?
						OperationData->FirstResidentLOD[ComponentName] :
						OperationData->GetFirstRequestedLOD()[ComponentName];
				
					for (int32 LODIndex = FirstLOD; LODIndex < Component.LODCount; ++LODIndex)
					{
						const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[Component.FirstLOD + LODIndex];

						TSharedPtr<const mu::FMesh> MutableMesh = LOD.Mesh;
						if (!MutableMesh)
						{
							continue;
						}

						LoadMorphTargetsMetadata(StreamRequest, MutableMesh.ToSharedRef(), OperationData->InstanceUpdateData.RealTimeMorphTargetMeshData);
					}
				}

				// Cloth
				{
					// Data
					// From FirstResidentLOD instead of FirstRequestedLOD since clothing we generate all LODs, even the hacky streaming copied ones.
					for (int32 LODIndex = OperationData->FirstResidentLOD[ComponentName]; LODIndex < Component.LODCount; ++LODIndex)
					{
						const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[Component.FirstLOD + LODIndex];

						TSharedPtr<const mu::FMesh> MutableMesh = LOD.Mesh;
						if (!MutableMesh)
						{
							continue;
						}

						if (bUpdateMeshes)
						{
							LoadClothing(StreamRequest, MutableMesh.ToSharedRef(), OperationData->InstanceUpdateData.ClothingMeshData);
						}
					}

					// Metadata
					for (int32 LODIndex = OperationData->FirstLODAvailable[ComponentName]; LODIndex < Component.LODCount; ++LODIndex)
					{
						const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[Component.FirstLOD + LODIndex];

						TSharedPtr<const mu::FMesh> MutableMesh = LOD.Mesh;
						if (!MutableMesh)
						{
							continue;
						}
					
						const TArray<uint64>& StreamedResources = MutableMesh->GetStreamedResources();

						for (uint64 ResourceId : StreamedResources)
						{
							FCustomizableObjectStreameableResourceId TypedResourceId = BitCast<FCustomizableObjectStreameableResourceId>(ResourceId);

							if (TypedResourceId.Type == (uint8)FCustomizableObjectStreameableResourceId::EType::Clothing)
							{
								check(TypedResourceId.Id != 0 && TypedResourceId.Id <= TNumericLimits<uint32>::Max());

								const TMap<uint32, FClothingStreamable>& ClothingStreamables = ModelStreamableBulkData->ClothingStreamables;
								if (const FClothingStreamable* ClothingStreamable = ClothingStreamables.Find(TypedResourceId.Id))
								{
									FClothingMeshData& ReadDestData = OperationData->InstanceUpdateData.ClothingMeshData.FindOrAdd(TypedResourceId.Id);
									ReadDestData.ClothingAssetIndex = ClothingStreamable->ClothingAssetIndex;
									ReadDestData.ClothingAssetLOD = ClothingStreamable->ClothingAssetLOD;
 								
									// TODO: Add async loading of ClothingAsset Data. This could be loaded as an streamead resource similar to and the asset user data.
									int32 ClothingAssetIndex = ClothingStreamable->ClothingAssetIndex; 
									int32 PhysicsAssetIndex = ClothingStreamable->PhysicsAssetIndex;					
									const TSoftObjectPtr<UPhysicsAsset>& PhysicsAsset = ModelResources.PhysicsAssets.IsValidIndex(PhysicsAssetIndex) 
											? ModelResources.PhysicsAssets[PhysicsAssetIndex] 
											: nullptr;

									// The entry should always be in the map
									if (!PhysicsAsset.IsNull())
									{
										if (PhysicsAsset.Get())
										{
											if (ClothingPhysicsAssets.IsValidIndex(ClothingAssetIndex))
											{
												ClothingPhysicsAssets[ClothingAssetIndex] = PhysicsAsset.Get();
											}
										}
										else
										{
											ComponentData->ClothingPhysicsAssetsToStream.Emplace(ClothingAssetIndex, PhysicsAssetIndex);
											AssetsToStream.AddUnique(PhysicsAsset.ToSoftObjectPath());
										}
									}
								}
								else
								{
									UE_LOG(LogMutable, Error, TEXT("Invalid streamed clothing data block [%d] found."), TypedResourceId.Id);
								}
							}
						}
					}
				}
			}
			
			for (int32 LODIndex = OperationData->FirstLODAvailable[ComponentName]; LODIndex < Component.LODCount; ++LODIndex)
			{
				const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[Component.FirstLOD + LODIndex];

				TSharedPtr<const mu::FMesh> MutableMesh = LOD.Mesh;
				if (!MutableMesh)
				{
					continue;
				}
				
				const TArray<uint64>& StreamedResources = MutableMesh->GetStreamedResources();

				for (uint64 ResourceId : StreamedResources)
				{
					FCustomizableObjectStreameableResourceId TypedResourceId = BitCast<FCustomizableObjectStreameableResourceId>(ResourceId);

					if (TypedResourceId.Type == (uint8)FCustomizableObjectStreameableResourceId::EType::AssetUserData)
					{
						const uint32 ResourceIndex = TypedResourceId.Id;
#if !WITH_EDITOR
						OperationData->StreamedResourceIndex.AddUnique(ResourceIndex); // Used to hold/release streamed resources in non-editor builds.
#endif
						ComponentData->StreamedResourceIndex.AddUnique(ResourceIndex);
					}
				}
				
				for (int32 TagIndex = 0; TagIndex < MutableMesh->GetTagCount(); ++TagIndex)
				{
					FString Tag = MutableMesh->GetTag(TagIndex);
					if (Tag.RemoveFromStart("__PA:"))
					{
						const int32 AssetIndex = FCString::Atoi(*Tag);
						const TSoftObjectPtr<UPhysicsAsset>& PhysicsAsset = ModelResources.PhysicsAssets.IsValidIndex(AssetIndex) ? ModelResources.PhysicsAssets[AssetIndex] : nullptr;

						if (!PhysicsAsset.IsNull())
						{
							if (PhysicsAsset.Get())
							{
								ComponentData->PhysicsAssets.PhysicsAssetsToMerge.Add(PhysicsAsset.Get());
							}
							else
							{
								ComponentData->PhysicsAssets.PhysicsAssetToLoad.Add(AssetIndex);
								AssetsToStream.AddUnique(PhysicsAsset.ToSoftObjectPath());
							}
						}
					}
					
					if (Tag.RemoveFromStart("__AnimBP:"))
					{
						FString SlotIndexString, AnimBpIndexString;

						if (Tag.Split(TEXT("_Slot_"), &SlotIndexString, &AnimBpIndexString))
						{
							if (SlotIndexString.IsEmpty() || AnimBpIndexString.IsEmpty())
							{
								continue;
							}

							const int32 AnimBpIndex = FCString::Atoi(*AnimBpIndexString);
							if (!ModelResources.AnimBPs.IsValidIndex(AnimBpIndex))
							{
								continue;
							}

							FName SlotIndex = *SlotIndexString;

							const TSoftClassPtr<UAnimInstance>& AnimBPAsset = ModelResources.AnimBPs[AnimBpIndex];

							if (!AnimBPAsset.IsNull())
							{
								const TSoftClassPtr<UAnimInstance>* FoundAnimBpSlot = ComponentData->AnimSlotToBP.Find(SlotIndex);
								bool bIsSameAnimBp = FoundAnimBpSlot && AnimBPAsset == *FoundAnimBpSlot;
								if (!FoundAnimBpSlot)
								{
									ComponentData->AnimSlotToBP.Add(SlotIndex, AnimBPAsset);

									if (AnimBPAsset.Get())
									{
										GatheredAnimBPs.Add(AnimBPAsset.Get());
									}
									else
									{
										AssetsToStream.AddUnique(AnimBPAsset.ToSoftObjectPath());
									}
								}
								else if (!bIsSameAnimBp)
								{
									// Two submeshes should not have the same animation slot index
									OperationData->UpdateResult = EUpdateResult::Warning;

									FString WarningMessage = FString::Printf(TEXT("Two submeshes have the same anim slot index [%s] in a Mutable Instance."), *SlotIndex.ToString());
									UE_LOG(LogMutable, Warning, TEXT("%s"), *WarningMessage);
#if WITH_EDITOR
									FMessageLog MessageLog("Mutable");
									MessageLog.Notify(FText::FromString(WarningMessage), EMessageSeverity::Warning, true);
#endif
								}
							}
						}
					}
					else if (Tag.RemoveFromStart("__AnimBPTag:"))
					{
						AnimBPGameplayTags.AddTag(FGameplayTag::RequestGameplayTag(*Tag));
					}
#if WITH_EDITORONLY_DATA
					else if (Tag.RemoveFromStart("__MeshPath:"))
					{
						ComponentData->MeshPartPaths.Add(Tag);
					}
#endif
				}

				const int32 AdditionalPhysicsNum = MutableMesh->AdditionalPhysicsBodies.Num();
				for (int32 I = 0; I < AdditionalPhysicsNum; ++I)
				{
					const int32 ExternalId = MutableMesh->AdditionalPhysicsBodies[I]->CustomId;
					
					ComponentData->PhysicsAssets.AdditionalPhysicsAssetsToLoad.Add(ExternalId);
					AssetsToStream.Add(ModelResources.AnimBpOverridePhysiscAssetsInfo[ExternalId].SourceAsset.ToSoftObjectPath());
				}
			}


			for (int32 ResourceIndex : ComponentData->StreamedResourceIndex)
			{
#if WITH_EDITOR
				if (!ModelResources.StreamedResourceDataEditor.IsValidIndex(ResourceIndex))
				{
					UE_LOG(LogMutable, Error, TEXT("Invalid streamed resource index. Max Index [%d]. Resource Index [%d]."), ModelResources.StreamedResourceDataEditor.Num(), ResourceIndex);
					continue;
				}

				if (const FCustomizableObjectAssetUserData* AUDResource = ModelResources.StreamedResourceDataEditor[ResourceIndex].Data.GetPtr<FCustomizableObjectAssetUserData>())
				{
					AssetsToStream.AddUnique(AUDResource->AssetUserDataEditor.ToSoftObjectPath());
				}
#else
				if (!ModelResources.StreamedResourceData.IsValidIndex(ResourceIndex))
				{
					UE_LOG(LogMutable, Error, TEXT("Invalid streamed resource index. Max Index [%d]. Resource Index [%d]."), ModelResources.StreamedResourceData.Num(), ResourceIndex);
					continue;
				}

				const FCustomizableObjectStreamedResourceData& StreamedResource = ModelResources.StreamedResourceData[ResourceIndex];
				if (!StreamedResource.IsLoaded())
				{
					AssetsToStream.AddUnique(StreamedResource.GetPath().ToSoftObjectPath());
				}
#endif
			}
		}
	}

	for (TSoftObjectPtr<const UTexture>& TextureRef : PassThroughTexturesToLoad)
	{
		AssetsToStream.Add(TextureRef.ToSoftObjectPath());
	}

	for (TSoftObjectPtr<const UStreamableRenderAsset>& MeshRef : PassThroughMeshesToLoad)
	{
		AssetsToStream.Add(MeshRef.ToSoftObjectPath());
	}

	// Copy FExtensionData Object node input from the Instance to the InstanceUpdateData
	for (int32 ExtensionDataIndex = 0; ExtensionDataIndex < OperationData->MutableInstance->GetExtensionDataCount(); ExtensionDataIndex++)
	{
		TSharedPtr<const mu::FExtensionData> ExtensionData;
		FName Name;
		OperationData->MutableInstance->GetExtensionData(ExtensionDataIndex, ExtensionData, Name);

		check(ExtensionData);

		FInstanceUpdateData::FNamedExtensionData& NewEntry = OperationData->InstanceUpdateData.ExtendedInputPins.AddDefaulted_GetRef();
		NewEntry.Data = ExtensionData;
		NewEntry.Name = Name;
		check(NewEntry.Name != NAME_None);

#if WITH_EDITOR
		if (!ModelResources.StreamedExtensionDataEditor.IsValidIndex(ExtensionData->Index))
		{
			// The compiled data appears to be out of sync with the CO's properties

			UE_LOG(LogMutable, Error, TEXT("Couldn't find streamed Extension Data with index %d in %s. Compiled data may be stale."),
				ExtensionData->Index, *CustomizableObject->GetFullName());
		}
#else
		if (!ModelResources.StreamedExtensionData.IsValidIndex(ExtensionData->Index))
		{
			// The compiled data appears to be out of sync with the CO's properties

			UE_LOG(LogMutable, Error, TEXT("Couldn't find streamed Extension Data with index %d in %s. Compiled data may be stale."),
				ExtensionData->Index, *CustomizableObject->GetFullName());

			continue;
		}

		const FCustomizableObjectStreamedResourceData& StreamedData = ModelResources.StreamedExtensionData[ExtensionData->Index];
		if (StreamedData.IsLoaded())
		{
			continue;
		}

		// Note that this just checks if the path is non-null, NOT if the object is loaded
		check(!StreamedData.GetPath().IsNull());

		OperationData->ExtensionStreamedResourceIndex.Add(ExtensionData->Index);
		AssetsToStream.Add(StreamedData.GetPath().ToSoftObjectPath());
#endif
	}

	TArray<UE::Tasks::FTask, TInlineAllocator<2>> Prerequisites;
	
	if (AssetsToStream.Num() > 0)
	{
#if WITH_EDITOR
		// TODO: Remove with UE-217665 when the underlying bug in the ColorPicker is solved
		// Disable the Slate throttling, otherwise the AsyncLoad may not complete until the editor window is clicked on due to a bug in
		// some widgets such as the ColorPicker's throttling handling
		FSlateThrottleManager::Get().DisableThrottle(true);
#endif
		
		UE::Tasks::FTaskEvent Event(TEXT("AssetsStreamed"));
		Prerequisites.Add(Event);

		UCustomizableObjectSystemPrivate* PrivateSystem = UCustomizableObjectSystem::GetInstance()->GetPrivate();
		
		PrivateSystem->StreamableManager->RequestAsyncLoad(
			AssetsToStream,
			FStreamableDelegate::CreateUObject(this, &UCustomizableInstancePrivate::AdditionalAssetsAsyncLoaded, OperationData, Event),
			CVarMutableHighPriorityLoading.GetValueOnAnyThread() ? FStreamableManager::AsyncLoadHighPriority : FStreamableManager::DefaultAsyncLoadPriority);
	}
	
	// Stream files
	UE::Tasks::FTask StreamingTask = StreamRequest.Stream();
	Prerequisites.Add(StreamingTask);

	return UE::Tasks::Launch(TEXT("CaptureOperationData"), [OperationData]() {}, // Keep a reference to make sure allocated memory is always alive.
	Prerequisites,
	UE::Tasks::ETaskPriority::Inherit);
}

FCustomizableObjectInstanceDescriptor& UCustomizableInstancePrivate::GetDescriptor() const
{
	return GetPublic()->Descriptor;
}


TArray<UMaterialInterface*> UCustomizableObjectInstance::GetSkeletalMeshComponentOverrideMaterials(const FName& ComponentName) const
{
	FCustomizableInstanceComponentData* ComponentData = PrivateData->GetComponentData(ComponentName);
	if (!ComponentData)
	{
		return {};
	}
	
	TArray<UMaterialInterface*> Result;

	for (const TObjectPtr<UMaterialInterface>& OverrideMaterial : ComponentData->OverrideMaterials)
	{
		Result.Add(OverrideMaterial);
	}
	
	return Result;
}


void UCustomizableInstancePrivate::AdditionalAssetsAsyncLoaded(const TSharedRef<FUpdateContextPrivate> OperationData, UE::Tasks::FTaskEvent Event)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::AdditionalAssetsAsyncLoaded);

	check(IsInGameThread())

	Event.Trigger();

	UCustomizableObjectPrivate* CustomizableObjectPrivate = GetPublic()->GetCustomizableObject()->GetPrivate();

	UModelResources& ModelResources = *CustomizableObjectPrivate->GetModelResources();

	for (int32 ResourceIndex : OperationData->StreamedResourceIndex)
	{
		ModelResources.StreamedResourceData[ResourceIndex].Hold();
	}
	
	for (int32 ResourceIndex : OperationData->ExtensionStreamedResourceIndex)
	{
		ModelResources.StreamedExtensionData[ResourceIndex].Hold();
	}
	
	// Loaded Materials
	check(ObjectToInstanceIndexMap.Num() == ReferencedMaterials.Num());

	for (TPair<uint32, uint32> Pair : ObjectToInstanceIndexMap)
	{
		const TSoftObjectPtr<UMaterialInterface>& AssetPtr = ModelResources.Materials.IsValidIndex(Pair.Key) ? ModelResources.Materials[Pair.Key] : nullptr;
		ReferencedMaterials[Pair.Value] = AssetPtr.Get();

#if WITH_EDITOR
		if (!ReferencedMaterials[Pair.Value])
		{
			if (!AssetPtr.IsNull())
			{
				FString ErrorMsg = FString::Printf(TEXT("Mutable couldn't load the material [%s] and won't be rendered. If it has been deleted or renamed, please recompile all the mutable objects that use it."), *AssetPtr.GetAssetName());
				UE_LOG(LogMutable, Error, TEXT("%s"), *ErrorMsg);

				FMessageLog MessageLog("Mutable");
				MessageLog.Notify(FText::FromString(ErrorMsg), EMessageSeverity::Error, true);
			}
			else
			{
				ensure(false); // Couldn't load the material, and we don't know which material
			}
		}
#endif
	}

	for (FCustomizableInstanceComponentData& ComponentData : ComponentsData)
	{
		for (int32 ResourceIndex : ComponentData.StreamedResourceIndex)
		{
#if WITH_EDITOR
			if (ModelResources.StreamedResourceDataEditor.IsValidIndex(ResourceIndex))
			{
				if (const FCustomizableObjectAssetUserData* AUDResource = ModelResources.StreamedResourceDataEditor[ResourceIndex].Data.GetPtr<FCustomizableObjectAssetUserData>())
				{
					ComponentData.AssetUserDataArray.Add(MutablePrivate::LoadObject(AUDResource->AssetUserDataEditor)); // Already loaded
				}
			}
#else
			if (ModelResources.StreamedResourceData.IsValidIndex(ResourceIndex) && ModelResources.StreamedResourceData[ResourceIndex].IsLoaded())
			{
				const FCustomizableObjectResourceData& ResourceData = ModelResources.StreamedResourceData[ResourceIndex].GetLoadedData();

				if (const FCustomizableObjectAssetUserData* AUDResource = ResourceData.Data.GetPtr<FCustomizableObjectAssetUserData>())
				{
					ComponentData.AssetUserDataArray.Add(AUDResource->AssetUserData);
				}
			}
#endif
		}

		// Loaded Skeletons
		FReferencedSkeletons& Skeletons = ComponentData.Skeletons;
		for (int32 SkeletonIndex : Skeletons.SkeletonIds)
		{
			const TSoftObjectPtr<USkeleton>& AssetPtr = ModelResources.Skeletons.IsValidIndex(SkeletonIndex) ? ModelResources.Skeletons[SkeletonIndex] : nullptr;
			Skeletons.SkeletonsToMerge.AddUnique(AssetPtr.Get());
		}

		// Loaded PhysicsAssets
		FReferencedPhysicsAssets& PhysicsAssets = ComponentData.PhysicsAssets;
		for(const int32 PhysicsAssetIndex : PhysicsAssets.PhysicsAssetToLoad)
		{
			check(ModelResources.PhysicsAssets.IsValidIndex(PhysicsAssetIndex));
			const TSoftObjectPtr<UPhysicsAsset>& PhysicsAsset = ModelResources.PhysicsAssets[PhysicsAssetIndex];
			PhysicsAssets.PhysicsAssetsToMerge.Add(PhysicsAsset.Get());

#if WITH_EDITOR
			if (!PhysicsAsset.Get())
			{
				if (!PhysicsAsset.IsNull())
				{
					FString ErrorMsg = FString::Printf(TEXT("Mutable couldn't load the PhysicsAsset [%s] and won't be merged. If it has been deleted or renamed, please recompile all the mutable objects that use it."), *PhysicsAsset.GetAssetName());
					UE_LOG(LogMutable, Error, TEXT("%s"), *ErrorMsg);

					FMessageLog MessageLog("Mutable");
					MessageLog.Notify(FText::FromString(ErrorMsg), EMessageSeverity::Error, true);
				}
				else
				{
					ensure(false); // Couldn't load the PhysicsAsset, and we don't know which PhysicsAsset
				}
			}
#endif
		}
		PhysicsAssets.PhysicsAssetToLoad.Empty();
		
		// Loaded Clothing PhysicsAssets 
		for ( TPair<int32, int32>& AssetToStream : ComponentData.ClothingPhysicsAssetsToStream )
		{
			const int32 AssetIndex = AssetToStream.Key;

			if (ClothingPhysicsAssets.IsValidIndex(AssetIndex) && ModelResources.PhysicsAssets.IsValidIndex(AssetToStream.Value))
			{
				const TSoftObjectPtr<UPhysicsAsset>& PhysicsAssetPtr = ModelResources.PhysicsAssets[AssetToStream.Value];
				ClothingPhysicsAssets[AssetIndex] = PhysicsAssetPtr.Get();
			}
		}
		ComponentData.ClothingPhysicsAssetsToStream.Empty();

		// Loaded anim BPs
		for (TPair<FName, TSoftClassPtr<UAnimInstance>>& SlotAnimBP : ComponentData.AnimSlotToBP)
		{
			if (TSubclassOf<UAnimInstance> AnimBP = SlotAnimBP.Value.Get())
			{
				if (!GatheredAnimBPs.Contains(AnimBP))
				{
					GatheredAnimBPs.Add(AnimBP);
				}
			}
#if WITH_EDITOR
			else
			{
				FString ErrorMsg = FString::Printf(TEXT("Mutable couldn't load the AnimBlueprint [%s]. If it has been deleted or renamed, please recompile all the mutable objects that use it."), *SlotAnimBP.Value.GetAssetName());
				UE_LOG(LogMutable, Error, TEXT("%s"), *ErrorMsg);

				FMessageLog MessageLog("Mutable");
				MessageLog.Notify(FText::FromString(ErrorMsg), EMessageSeverity::Error, true);
			}
#endif
		}

		const int32 AdditionalPhysicsNum = ComponentData.PhysicsAssets.AdditionalPhysicsAssetsToLoad.Num();
		ComponentData.PhysicsAssets.AdditionalPhysicsAssets.Reserve(AdditionalPhysicsNum);
		for (int32 I = 0; I < AdditionalPhysicsNum; ++I)
		{
			// Make the loaded assets references strong.
			const int32 AnimBpPhysicsOverrideIndex = ComponentData.PhysicsAssets.AdditionalPhysicsAssetsToLoad[I];
			ComponentData.PhysicsAssets.AdditionalPhysicsAssets.Add( 
				ModelResources.AnimBpOverridePhysiscAssetsInfo[AnimBpPhysicsOverrideIndex].SourceAsset.Get());
		}
		ComponentData.PhysicsAssets.AdditionalPhysicsAssetsToLoad.Empty();
	}

	LoadedPassThroughTexturesPendingSetMaterial.Empty(PassThroughTexturesToLoad.Num());

	for (TSoftObjectPtr<const UTexture>& TextureRef : PassThroughTexturesToLoad)
	{
		ensure(TextureRef.IsValid());
		LoadedPassThroughTexturesPendingSetMaterial.Add(TextureRef.Get());
	}

	PassThroughTexturesToLoad.Empty();

	LoadedPassThroughMeshesPendingSetMaterial.Empty(PassThroughMeshesToLoad.Num());

	for (TSoftObjectPtr<const UStreamableRenderAsset>& MeshRef : PassThroughMeshesToLoad)
	{
		ensure(MeshRef.IsValid());
		LoadedPassThroughMeshesPendingSetMaterial.Add(MeshRef.Get());
	}

	PassThroughMeshesToLoad.Empty();
	
#if WITH_EDITOR
	// TODO: Remove with UE-217665 when the underlying bug in the ColorPicker is solved
	// Reenable the throttling which disabled when launching the Async Load
	FSlateThrottleManager::Get().DisableThrottle(false);
#endif
}


void UpdateTextureRegionsMutable(UTexture2D* Texture, int32 MipIndex, uint32 NumMips, const FUpdateTextureRegion2D& Region, uint32 SrcPitch, 
								 const FByteBulkData* BulkData, TSharedRef<FTexturePlatformData, ESPMode::ThreadSafe>& PlatformData)
{
	if (Texture->GetResource())
	{
		struct FUpdateTextureRegionsData
		{
			FTexture2DResource* Texture2DResource;
			int32 MipIndex;
			FUpdateTextureRegion2D Region;
			uint32 SrcPitch;
			uint32 NumMips;
			
			// The Platform Data mips will be automatically deleted when all FUpdateTextureRegionsData that reference it are deleted
			// in the render thread after being used to update the texture
			TSharedRef<FTexturePlatformData, ESPMode::ThreadSafe> PlatformData;

			FUpdateTextureRegionsData(TSharedRef<FTexturePlatformData, ESPMode::ThreadSafe>& InPlatformData) : PlatformData(InPlatformData) {}
		};

		FUpdateTextureRegionsData* RegionData = new FUpdateTextureRegionsData(PlatformData);

		RegionData->Texture2DResource = (FTexture2DResource*)Texture->GetResource();
		RegionData->MipIndex = MipIndex;
		RegionData->Region = Region;
		RegionData->SrcPitch = SrcPitch;
		RegionData->NumMips = NumMips;

		ENQUEUE_RENDER_COMMAND(UpdateTextureRegionsMutable)(
			[RegionData, BulkData](FRHICommandList& CmdList)
			{
				check(int32(RegionData->NumMips) >= RegionData->Texture2DResource->GetCurrentMipCount());
				int32 MipDifference = RegionData->NumMips - RegionData->Texture2DResource->GetCurrentMipCount();
				check(MipDifference >= 0);
				int32 CurrentFirstMip = RegionData->Texture2DResource->GetCurrentFirstMip();
				uint8* SrcData = (uint8*)BulkData->LockReadOnly();

				//uint32 Size = RegionData->SrcPitch / (sizeof(uint8) * 4);
				//UE_LOG(LogMutable, Warning, TEXT("UpdateTextureRegionsMutable MipIndex = %d, FirstMip = %d, size = %d"),
				//	RegionData->MipIndex, CurrentFirstMip, Size);

				//checkf(Size <= RegionData->Texture2DResource->GetSizeX(),
				//	TEXT("UpdateTextureRegionsMutable incorrect size. %d, %d. NumMips=%d"), 
				//	Size, RegionData->Texture2DResource->GetSizeX(), RegionData->Texture2DResource->GetCurrentMipCount());

				if (RegionData->MipIndex >= CurrentFirstMip + MipDifference)
				{
					RHIUpdateTexture2D(
						RegionData->Texture2DResource->GetTexture2DRHI(),
						RegionData->MipIndex - CurrentFirstMip - MipDifference,
						RegionData->Region,
						RegionData->SrcPitch,
						SrcData);
				}

				BulkData->Unlock();
				delete RegionData; // This will implicitly delete the Platform Data if this is the last RegionData referencing it
			});
	}
}


void UCustomizableInstancePrivate::ReuseTexture(UTexture2D* Texture, TSharedRef<FTexturePlatformData, ESPMode::ThreadSafe>& PlatformData)
{
	uint32 NumMips = PlatformData->Mips.Num();

	for (uint32 i = 0; i < NumMips; i++)
	{
		FTexture2DMipMap& Mip = PlatformData->Mips[i];

		if (Mip.BulkData.GetElementCount() > 0)
		{
			FUpdateTextureRegion2D Region;

			Region.DestX = 0;
			Region.DestY = 0;
			Region.SrcX = 0;
			Region.SrcY = 0;
			Region.Width = Mip.SizeX;
			Region.Height = Mip.SizeY;

			check(int32(Region.Width) <= Texture->GetSizeX());
			check(int32(Region.Height) <= Texture->GetSizeY());

			UpdateTextureRegionsMutable(Texture, i, NumMips, Region, 
					                    Mip.SizeX * sizeof(uint8) * 4, &Mip.BulkData, PlatformData);
		}
	}
}


void UCustomizableInstancePrivate::BuildMaterials(const TSharedRef<FUpdateContextPrivate>& OperationData, UCustomizableObjectInstance* Public)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::BuildMaterials)

	UCustomizableObject* CustomizableObject = Public->GetCustomizableObject();

	const UModelResources& ModelResources = *CustomizableObject->GetPrivate()->GetModelResources();

	TArray<FGeneratedTexture> NewGeneratedTextures;

	// Temp copy to allow reuse of MaterialInstances
	TArray<FGeneratedMaterial> OldGeneratedMaterials;
	Exchange(OldGeneratedMaterials, GeneratedMaterials);
	
	GeneratedMaterials.Reset();

	// Prepare the data to store in order to regenerate resources for this instance (usually texture mips).
	TSharedPtr<FMutableUpdateContext> UpdateContext = MakeShared<FMutableUpdateContext>(
		CustomizableObject->GetPathName(),
		Public->GetPathName(),
		UCustomizableObjectSystem::GetInstance()->GetPrivate()->MutableSystem,
		OperationData->Model,
		CustomizableObject->GetPrivate()->GetModelStreamableBulkData(),
		OperationData->Parameters,
	    OperationData->GetCapturedDescriptor().GetState());

	// Cache the descriptor as a string if we want to later report it using our benchmark utility. 
	if (FLogBenchmarkUtil::IsBenchmarkingReportingEnabled())
	{
		UpdateContext->CapturedDescriptor = OperationData->GetCapturedDescriptor().ToString();
		if (GWorld)
		{
			UpdateContext->bLevelBegunPlay = GWorld->GetBegunPlay();
		}
	}
	
	const bool bReuseTextures = OperationData->bReuseInstanceTextures;

	TArray<bool> RecreateRenderStateOnInstanceComponent;
	RecreateRenderStateOnInstanceComponent.Init(false, OperationData->NumInstanceComponents);

	TArray<bool> NotifyUpdateOnInstanceComponent;
	NotifyUpdateOnInstanceComponent.Init(false, OperationData->NumInstanceComponents);

	for (int32 InstanceComponentIndex = 0; InstanceComponentIndex < OperationData->NumInstanceComponents; ++InstanceComponentIndex)
	{
		const FInstanceUpdateData::FComponent& Component = OperationData->InstanceUpdateData.Components[InstanceComponentIndex];

		const FCustomizableObjectComponentIndex ObjectComponentIndex = Component.Id;

		if (!ModelResources.ComponentNamesPerObjectComponent.IsValidIndex(ObjectComponentIndex.GetValue()))
		{
			continue;
		}
		const FName& ComponentName = ModelResources.ComponentNamesPerObjectComponent[ObjectComponentIndex.GetValue()];

		TObjectPtr<USkeletalMesh>* Result = SkeletalMeshes.Find(ComponentName);
		TObjectPtr<USkeletalMesh> SkeletalMesh = Result ? *Result : nullptr;
		if (!SkeletalMesh)
		{
			continue;
		}

		const bool bReuseMaterials = !OperationData->MeshChangedPerInstanceComponent[InstanceComponentIndex];

		// If the mesh is not transient, it means it's pass-through so it should use material overrides and not be modified in any way
		const bool bIsTransientMesh = static_cast<bool>(SkeletalMesh->HasAllFlags(EObjectFlags::RF_Transient));

		// It is not safe to replace the materials of a SkeletalMesh whose resources are initialized. Use overrides instead.
		const bool bUseOverrideMaterialsOnly = !bIsTransientMesh || (OperationData->bUseMeshCache && SkeletalMesh->GetResourceForRendering()->IsInitialized());

		UMaterialInterface* OverlayMaterial = nullptr;

		FCustomizableInstanceComponentData* ComponentData = GetComponentData(ObjectComponentIndex);
		if (ComponentData)
		{
			ComponentData->OverrideMaterials.Reset();
			ComponentData->OverlayMaterial = nullptr;

			if (const uint32* ReferencedMaterialIndex = ObjectToInstanceIndexMap.Find(Component.OverlayMaterial))
			{
				if (ReferencedMaterials.IsValidIndex(*ReferencedMaterialIndex))
				{
					ComponentData->OverlayMaterial = ReferencedMaterials[*ReferencedMaterialIndex];
					OverlayMaterial = ComponentData->OverlayMaterial;
				}
			}
		}

		if (!bUseOverrideMaterialsOnly)
		{
			RecreateRenderStateOnInstanceComponent[InstanceComponentIndex] |= SkeletalMesh->GetOverlayMaterial() != OverlayMaterial;
			SkeletalMesh->SetOverlayMaterial(OverlayMaterial);
		}

		TArray<FSkeletalMaterial> Materials;

		// Maps serializations of FMutableMaterialPlaceholder to Created Dynamic Material instances, used to reuse materials across LODs
		TSet<FMutableMaterialPlaceholder> ReuseMaterialCache;

		// SurfaceId per MaterialSlotIndex
		TArray<int32> SurfaceIdToMaterialIndex;

		MUTABLE_CPUPROFILER_SCOPE(BuildMaterials_LODLoop);

		const int32 FirstLOD = OperationData->bStreamMeshLODs ?
			OperationData->FirstLODAvailable[ComponentName] :
			OperationData->GetFirstRequestedLOD()[ComponentName];

		for (int32 LODIndex = FirstLOD; LODIndex < Component.LODCount; LODIndex++)
		{
			const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[Component.FirstLOD+LODIndex];
			
			if (!bUseOverrideMaterialsOnly && LODIndex < SkeletalMesh->GetLODNum())
			{
				SkeletalMesh->GetLODInfo(LODIndex)->LODMaterialMap.Reset();
			}

			// Pass-through components will not have a reference mesh.
			const FMutableRefSkeletalMeshData* RefSkeletalMeshData = nullptr;
			if (ModelResources.ReferenceSkeletalMeshesData.IsValidIndex(ObjectComponentIndex.GetValue()))
			{
				RefSkeletalMeshData = &ModelResources.ReferenceSkeletalMeshesData[ObjectComponentIndex.GetValue()];
			}

			for (int32 SurfaceIndex = 0; SurfaceIndex < LOD.SurfaceCount; ++SurfaceIndex)
			{
				const FInstanceUpdateData::FSurface& Surface = OperationData->InstanceUpdateData.Surfaces[LOD.FirstSurface + SurfaceIndex];

				// Is this a material in a passthrough mesh that we don't modify?
				if (Surface.MaterialIndex<0)
				{
					Materials.Emplace();
#if WITH_EDITOR
					// Without this, a change of a referenced material and recompilation doesn't show up in the preview.
					RecreateRenderStateOnInstanceComponent[InstanceComponentIndex] = true;
#endif
					continue;
				}

				// Reuse MaterialSlot from the previous LOD.
				if (const int32 MaterialIndex = SurfaceIdToMaterialIndex.Find(Surface.SurfaceId); MaterialIndex != INDEX_NONE)
				{
					if (!bUseOverrideMaterialsOnly)
					{
						const int32 LODMaterialIndex = SkeletalMesh->GetLODInfo(LODIndex)->LODMaterialMap.Add(MaterialIndex);
						SkeletalMesh->GetResourceForRendering()->LODRenderData[LODIndex].RenderSections[SurfaceIndex].MaterialIndex = LODMaterialIndex;
					}

					continue;
				}

				const uint32 ReferencedMaterialIndex = ObjectToInstanceIndexMap[Surface.MaterialIndex];
				UMaterialInterface* MaterialTemplate = ReferencedMaterials[ReferencedMaterialIndex];
				if (!MaterialTemplate)
				{
					// Missing MaterialTemplate. Use DefaultMaterial instead. 
					MaterialTemplate = UMaterial::GetDefaultMaterial(MD_Surface);
					check(MaterialTemplate);
					UE_LOG(LogMutable, Error, TEXT("Build Materials: Missing referenced template to use as parent material on CustomizableObject [%s]."), *CustomizableObject->GetName());
				}

				// This section will require a new slot
				SurfaceIdToMaterialIndex.Add(Surface.SurfaceId);

				// Add and set up the material data for this slot
				const int32 MaterialSlotIndex = Materials.Num();
				FSkeletalMaterial& MaterialSlot = Materials.AddDefaulted_GetRef();
				MaterialSlot.MaterialInterface = MaterialTemplate;

				uint32 UsedSurfaceMetadataId = Surface.SurfaceMetadataId;
				
				// If the surface metadata is invalid, check if any of the mesh fragments has metadata. 
				// For now use the fisrt found, an aggregate may be needed. 
				if (Surface.SurfaceMetadataId == 0 && LOD.Mesh)
				{
					int32 MeshSurfaceIndex = LOD.Mesh->Surfaces.IndexOfByPredicate([SurfaceId = Surface.SurfaceId](const mu::FMeshSurface& Surface)
					{
						return SurfaceId == Surface.Id;
					});

					if (MeshSurfaceIndex != INDEX_NONE)
					{
						for (const mu::FSurfaceSubMesh& SubMesh : LOD.Mesh->Surfaces[SurfaceIndex].SubMeshes)	
						{
							const FMutableMeshMetadata* FoundMeshMetadata = ModelResources.MeshMetadata.Find(SubMesh.ExternalId);

							if (!FoundMeshMetadata)
							{
								continue;
							}

							UsedSurfaceMetadataId = FoundMeshMetadata->SurfaceMetadataId; 

							if (UsedSurfaceMetadataId != 0)
							{
								break;
							}
						}
					}
				}

				const FMutableSurfaceMetadata* FoundSurfaceMetadata = ModelResources.SurfaceMetadata.Find(UsedSurfaceMetadataId);
				
				if (FoundSurfaceMetadata)
				{
					MaterialSlot.MaterialSlotName = FoundSurfaceMetadata->MaterialSlotName;
				}
				if (RefSkeletalMeshData)
				{
					SetMeshUVChannelDensity(MaterialSlot.UVChannelData, RefSkeletalMeshData->Settings.DefaultUVChannelDensity);
				}

				if (!bUseOverrideMaterialsOnly)
				{
					if (SkeletalMesh->GetResourceForRendering()->LODRenderData.IsValidIndex(LODIndex) &&
						SkeletalMesh->GetResourceForRendering()->LODRenderData[LODIndex].RenderSections.IsValidIndex(SurfaceIndex))
					{
						const int32 LODMaterialIndex = SkeletalMesh->GetLODInfo(LODIndex)->LODMaterialMap.Add(MaterialSlotIndex);
						SkeletalMesh->GetResourceForRendering()->LODRenderData[LODIndex].RenderSections[SurfaceIndex].MaterialIndex = LODMaterialIndex;
					}
					else
					{
						ensure(false);
					}
				}

				FMutableMaterialPlaceholder MutableMaterialPlaceholder;
				MutableMaterialPlaceholder.ParentMaterialID = MaterialTemplate->GetUniqueID();
				MutableMaterialPlaceholder.MatIndex = MaterialSlotIndex;

				{
					MUTABLE_CPUPROFILER_SCOPE(ParamLoop);

					for (int32 VectorIndex = 0; VectorIndex < Surface.VectorCount; ++VectorIndex)
					{
						const FInstanceUpdateData::FVector& Vector = OperationData->InstanceUpdateData.Vectors[Surface.FirstVector + VectorIndex];

						// Decoding Material Layer from Mutable parameter name
						FString EncodingString = "-MutableLayerParam:";

						FString VectorName = Vector.Name.ToString();
						int32 EncodingPosition = VectorName.Find(EncodingString);
						int32 LayerIndex = -1;

						if (EncodingPosition == INDEX_NONE)
						{
							MutableMaterialPlaceholder.AddParam(
								FMutableMaterialPlaceholder::FMutableMaterialPlaceHolderParam(FName(Vector.Name), -1, Vector.Vector));
						}
						else
						{
							//Getting layer index
							int32 LayerPosition = VectorName.Len() - (EncodingPosition + EncodingString.Len());
							FString IndexString = VectorName.RightChop(VectorName.Len() - LayerPosition);
							LayerIndex = FCString::Atof(*IndexString);

							//Getting parameter name
							FString Sufix = EncodingString + FString::FromInt(LayerIndex);
							VectorName.RemoveFromEnd(Sufix);

							MutableMaterialPlaceholder.AddParam(
								FMutableMaterialPlaceholder::FMutableMaterialPlaceHolderParam(FName(VectorName), LayerIndex, Vector.Vector));
						}
					}

					for (int32 ScalarIndex = 0; ScalarIndex < Surface.ScalarCount; ++ScalarIndex)
					{
						const FInstanceUpdateData::FScalar& Scalar = OperationData->InstanceUpdateData.Scalars[Surface.FirstScalar + ScalarIndex];

						// Decoding Material Layer from Mutable parameter name
						FString EncodingString = "-MutableLayerParam:";

						FString ScalarName = Scalar.Name.ToString();
						int32 EncodingPosition = ScalarName.Find(EncodingString);
						int32 LayerIndex = -1;

						if (EncodingPosition == INDEX_NONE)
						{
							MutableMaterialPlaceholder.AddParam(
								FMutableMaterialPlaceholder::FMutableMaterialPlaceHolderParam(FName(Scalar.Name), -1, Scalar.Scalar));
						}
						else
						{
							//Getting layer index
							int32 LayerPosition = ScalarName.Len() - (EncodingPosition + EncodingString.Len());
							FString IndexString = ScalarName.RightChop(ScalarName.Len() - LayerPosition);
							LayerIndex = FCString::Atof(*IndexString);

							//Getting parameter name
							FString Sufix = EncodingString + FString::FromInt(LayerIndex);
							ScalarName.RemoveFromEnd(Sufix);

							MutableMaterialPlaceholder.AddParam(
								FMutableMaterialPlaceholder::FMutableMaterialPlaceHolderParam(FName(ScalarName), LayerIndex, Scalar.Scalar));
						}
					}
				}

				{
					MUTABLE_CPUPROFILER_SCOPE(BuildMaterials_ImageLoop);

					// Get the cache of resources of all live instances of this object
					FMutableResourceCache& Cache = UCustomizableObjectSystem::GetInstance()->GetPrivate()->GetObjectCache(CustomizableObject);

					FString CurrentState = Public->GetCurrentState();
					bool bNeverStream = OperationData->bNeverStream;

					check((bNeverStream && OperationData->MipsToSkip == 0) ||
						(!bNeverStream && OperationData->MipsToSkip >= 0));

					for (int32 ImageIndex = 0; ImageIndex < Surface.ImageCount; ++ImageIndex)
					{
						const FInstanceUpdateData::FImage& Image = OperationData->InstanceUpdateData.Images[Surface.FirstImage + ImageIndex];
						FString KeyName = Image.Name.ToString();
						TSharedPtr<const mu::FImage> MutableImage = Image.Image;

						UTexture2D* MutableTexture = nullptr; // Texture generated by mutable
						UTexture* PassThroughTexture = nullptr; // Texture not generated by mutable

						// \TODO: Change this key to a struct.
						FString TextureReuseCacheRef = bReuseTextures ? FString::Printf(TEXT("%d-%d-%d-%d"), Image.BaseLOD, ObjectComponentIndex.GetValue(), Surface.SurfaceId, ImageIndex) : FString();

						// If the mutable image is null, it must be in the cache
						FMutableImageCacheKey ImageCacheKey = { Image.ImageID, OperationData->MipsToSkip };
						if (!MutableImage)
						{
							TWeakObjectPtr<UTexture2D>* CachedPointerPtr = Cache.Images.Find(ImageCacheKey);
							if (CachedPointerPtr)
							{
								ensure(!CachedPointerPtr->IsStale());
								MutableTexture = CachedPointerPtr->Get();
							}

							check(MutableTexture);
						}

						// Check if the image is a reference to an engine texture
						if (MutableImage && Image.bIsPassThrough)
						{
							check(MutableImage->IsReference());

							uint32 ReferenceID = MutableImage->GetReferencedTexture();
							if (ModelResources.PassThroughTextures.IsValidIndex(ReferenceID))
							{
								TSoftObjectPtr<UTexture> Ref = ModelResources.PassThroughTextures[ReferenceID];

								// The texture should have been loaded by now by LoadAdditionalAssetsAsync()
								PassThroughTexture = Ref.Get();

								if (!PassThroughTexture)
								{
									// The texture should be loaded, something went wrong, possibly a bug in LoadAdditionalAssetsAsync()
									UE_LOG(LogMutable, Error,
										TEXT("Pass-through texture with name %s hasn't been loaded yet in BuildMaterials(). Forcing sync load."),
										*Ref.ToSoftObjectPath().ToString());
									ensure(false);
									PassThroughTexture = MutablePrivate::LoadObject(Ref);
								}
							}

							if (!PassThroughTexture)
							{
								// Internal error.
								UE_LOG(LogMutable, Error, TEXT("Missing referenced image [%d]."), ReferenceID);
								continue;
							}
						}

						// Find the additional information for this image
						int32 ImageKey = FCString::Atoi(*KeyName);
						if (ImageKey >= 0 && ImageKey < ModelResources.ImageProperties.Num())
						{
							const FMutableModelImageProperties& Props = ModelResources.ImageProperties[ImageKey];

							if (!MutableTexture && !PassThroughTexture && MutableImage)
							{
								TWeakObjectPtr<UTexture2D>* ReusedTexture = bReuseTextures ? TextureReuseCache.Find(TextureReuseCacheRef) : nullptr;
								
								// This shared ptr will hold the reused texture platform data (mips) until the reused texture is updated 
								// and delete it automatically
								TSharedPtr<FTexturePlatformData, ESPMode::ThreadSafe> ReusedTexturePlatformData;

								// Ensure the name of the texture is unique (prevents black textures in editor previews)
								FString MutableTextureName = FString::Printf(TEXT("T_%s_%llu"), *Props.TextureParameterName, Image.ImageID);
								MutableTextureName.ReplaceInline(TEXT(" "), TEXT("_"));
								MutableTextureName = MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), *MutableTextureName, EUniqueObjectNameOptions::GloballyUnique).ToString();
									
								if (ReusedTexture && (*ReusedTexture).IsValid() && !(*ReusedTexture)->HasAnyFlags(RF_BeginDestroyed))
								{
									// Only uncompressed textures can be reused. This also fixes an issue in the editor where textures supposedly 
									// uncompressed by their state, are still compressed because the CO has not been compiled at maximum settings
									// and the uncompressed setting cannot be applied to them.
									EPixelFormat PixelFormat = (*ReusedTexture)->GetPixelFormat();

									if (PixelFormat == EPixelFormat::PF_R8G8B8A8)
									{
										MutableTexture = (*ReusedTexture).Get();
										check(MutableTexture != nullptr);
									}
									else
									{
										ReusedTexture = nullptr;
										MutableTexture = CreateTexture(MutableTextureName);
#if WITH_EDITOR
										UE_LOG(LogMutable, Warning,
											TEXT("Tried to reuse an uncompressed texture with name %s. Make sure the selected Mutable state disables texture compression/streaming, that one of the state's runtime parameters affects the texture and that the CO is compiled with max. optimization settings."),
											*MutableTexture->GetName());
#endif
									}
								}
								else
								{
									ReusedTexture = nullptr;
									MutableTexture = CreateTexture(MutableTextureName);
								}

								if (MutableTexture)
								{
									if (OperationData->ImageToPlatformDataMap.Contains(Image.ImageID))
									{
										SetTexturePropertiesFromMutableImageProps(MutableTexture, Props, bNeverStream);

										FTexturePlatformData* PlatformData = OperationData->ImageToPlatformDataMap[Image.ImageID];

										if (ReusedTexture)
										{
											check(PlatformData->Mips.Num() == MutableTexture->GetPlatformData()->Mips.Num());
											check(PlatformData->Mips[0].SizeX == MutableTexture->GetPlatformData()->Mips[0].SizeX);
											check(PlatformData->Mips[0].SizeY == MutableTexture->GetPlatformData()->Mips[0].SizeY);

											// Now the ReusedTexturePlatformData shared ptr owns the platform data
											ReusedTexturePlatformData = TSharedPtr<FTexturePlatformData, ESPMode::ThreadSafe>(PlatformData);
										}
										else
										{
											// Now the MutableTexture owns the platform data
											MutableTexture->SetPlatformData(PlatformData);
										}

										OperationData->ImageToPlatformDataMap.Remove(Image.ImageID);
									}
									else
									{
										UE_LOG(LogMutable, Error, TEXT("Required image [%s] with ID [%lld] was not generated in the mutable thread, and it is not cached. LOD [%d]. Object Component [%d]"),
											*Props.TextureParameterName,
											Image.ImageID,
											LODIndex, ObjectComponentIndex.GetValue());
										continue;
									}

									if (bNeverStream)
									{
										// To prevent LogTexture Error "Loading non-streamed mips from an external bulk file."
										for (int32 i = 0; i < MutableTexture->GetPlatformData()->Mips.Num(); ++i)
										{
											MutableTexture->GetPlatformData()->Mips[i].BulkData.ClearBulkDataFlags(BULKDATA_PayloadInSeperateFile);
										}
									}

									{
										MUTABLE_CPUPROFILER_SCOPE(UpdateResource);
#if REQUIRES_SINGLEUSE_FLAG_FOR_RUNTIME_TEXTURES
										for (int32 i = 0; i < MutableTexture->GetPlatformData()->Mips.Num(); ++i)
										{
											uint32 DataFlags = MutableTexture->GetPlatformData()->Mips[i].BulkData.GetBulkDataFlags();
											MutableTexture->GetPlatformData()->Mips[i].BulkData.SetBulkDataFlags(DataFlags | BULKDATA_SingleUse);
										}
#endif

										if (ReusedTexture)
										{
											// Must remove texture from cache since it will be reused with a different ImageID
											for (TPair<FMutableImageCacheKey, TWeakObjectPtr<UTexture2D>>& CachedTexture : Cache.Images)
											{
												if (CachedTexture.Value == MutableTexture)
												{
													Cache.Images.Remove(CachedTexture.Key);
													break;
												}
											}

											check(ReusedTexturePlatformData.IsValid());

											if (ReusedTexturePlatformData.IsValid())
											{
												TSharedRef<FTexturePlatformData, ESPMode::ThreadSafe> PlatformDataRef = ReusedTexturePlatformData.ToSharedRef();
												ReuseTexture(MutableTexture, PlatformDataRef);
											}
										}
										else if (MutableTexture)
										{	
											//if (!bNeverStream) // No need to check bNeverStream. In that case, the texture won't use 
											// the MutableMipDataProviderFactory anyway and it's needed for detecting Mutable textures elsewhere
											{
												UMutableTextureMipDataProviderFactory* MutableMipDataProviderFactory = Cast<UMutableTextureMipDataProviderFactory>(MutableTexture->GetAssetUserDataOfClass(UMutableTextureMipDataProviderFactory::StaticClass()));
												if (!MutableMipDataProviderFactory)
												{
													MutableMipDataProviderFactory = NewObject<UMutableTextureMipDataProviderFactory>();

													if (MutableMipDataProviderFactory)
													{
														MutableMipDataProviderFactory->CustomizableObjectInstance = Public;
														check(LODIndex < 256 && InstanceComponentIndex < 256 && ImageIndex < 256);
														MutableMipDataProviderFactory->ImageRef.ImageID = Image.ImageID;
														MutableMipDataProviderFactory->ImageRef.SurfaceId = Surface.SurfaceId;
														MutableMipDataProviderFactory->ImageRef.LOD = uint8(Image.BaseLOD);
														MutableMipDataProviderFactory->ImageRef.Component = uint8(InstanceComponentIndex);
														MutableMipDataProviderFactory->ImageRef.Image = uint8(ImageIndex);
														MutableMipDataProviderFactory->ImageRef.BaseMip = uint8(Image.BaseMip);
														MutableMipDataProviderFactory->ImageRef.ConstantImagesNeededToGenerate = Image.ConstantImagesNeededToGenerate;
														MutableMipDataProviderFactory->UpdateContext = UpdateContext;
														MutableTexture->AddAssetUserData(MutableMipDataProviderFactory);
													}
												}
											}

											MutableTexture->UpdateResource();
										}
									}

									Cache.Images.Add(ImageCacheKey, MutableTexture);						
								}
								else
								{
									UE_LOG(LogMutable, Error, TEXT("Texture creation failed."));
								}
							}

							FGeneratedTexture TextureData;
							TextureData.Key = ImageCacheKey;
							TextureData.Name = Props.TextureParameterName;
							TextureData.Texture = MutableTexture ? MutableTexture : PassThroughTexture;
							
							// Only add textures generated by mutable to the cache
							if (MutableTexture)
							{
								NewGeneratedTextures.Add(TextureData);
							}

							// Decoding Material Layer from Mutable parameter name
							FString ImageName = Image.Name.ToString();
							FString EncodingString = "-MutableLayerParam:";

							int32 EncodingPosition = ImageName.Find(EncodingString);
							int32 LayerIndex = -1;

							if (EncodingPosition == INDEX_NONE)
							{
								MutableMaterialPlaceholder.AddParam(
									FMutableMaterialPlaceholder::FMutableMaterialPlaceHolderParam(*Props.TextureParameterName, -1, TextureData));
							}
							else
							{
								//Getting layer index
								int32 LayerPosition = ImageName.Len() - (EncodingPosition + EncodingString.Len());
								FString IndexString = ImageName.RightChop(ImageName.Len() - LayerPosition);
								LayerIndex = FCString::Atof(*IndexString);

								MutableMaterialPlaceholder.AddParam(
									FMutableMaterialPlaceholder::FMutableMaterialPlaceHolderParam(*Props.TextureParameterName, LayerIndex, TextureData));
							}
						}
						else
						{
							// This means the compiled model (maybe coming from derived data) has images that the asset doesn't know about.
							UE_LOG(LogMutable, Error, TEXT("CustomizableObject derived data out of sync with asset for [%s]. Try recompiling it."), *CustomizableObject->GetName());
						}

						if (bReuseTextures)
						{
							if (MutableTexture)
							{
								TextureReuseCache.Add(TextureReuseCacheRef, MutableTexture);
							}
							else
							{
								TextureReuseCache.Remove(TextureReuseCacheRef);
							}
						}
					}
				}

				// Find or create the material for this slot
				UMaterialInterface* MaterialInterface = MaterialSlot.MaterialInterface;

				if (FMutableMaterialPlaceholder* FoundMaterialPlaceholder = ReuseMaterialCache.Find(MutableMaterialPlaceholder))
				{
					MaterialInterface = Materials[FoundMaterialPlaceholder->MatIndex].MaterialInterface;
				}
				else // Material not cached, create a new one
				{
					MUTABLE_CPUPROFILER_SCOPE(BuildMaterials_CreateMaterial);

					ReuseMaterialCache.Add(MutableMaterialPlaceholder);

					FGeneratedMaterial& Material = GeneratedMaterials.AddDefaulted_GetRef();
					Material.SurfaceId = Surface.SurfaceId;
					Material.MaterialIndex = Surface.MaterialIndex;
					Material.MaterialInterface = MaterialInterface;

#if WITH_EDITORONLY_DATA
					Material.ComponentName = ComponentName;
#endif

					UMaterialInstanceDynamic* MaterialInstance = nullptr;

					if (const int32 OldMaterialIndex = OldGeneratedMaterials.Find(Material); bReuseMaterials && OldMaterialIndex != INDEX_NONE)
					{
						const FGeneratedMaterial& OldMaterial = OldGeneratedMaterials[OldMaterialIndex];
						MaterialInstance = Cast<UMaterialInstanceDynamic>(OldMaterial.MaterialInterface);
						Material.MaterialInterface = OldMaterial.MaterialInterface;
					}
					
					if (!MaterialInstance && MutableMaterialPlaceholder.GetParams().Num() != 0)
					{
#if WITH_EDITOR
						// Remove the MI_ or M_ prefixes from the material string so we use it as the name of the MID
						FString MIDName = MaterialTemplate->GetName();
						{
							const FString MaterialPrefix = TEXT("M_");						// Material
							const FString MaterialInstancePrefix = TEXT("MI_");				// Material Instance
							const FString MaterialInstanceConstantPrefix = TEXT("MIC_");	// Material Instance Constant
							
							if (MIDName.Find(MaterialInstancePrefix, ESearchCase::CaseSensitive) == 0)
							{
								MIDName = MIDName.RightChop(MaterialInstancePrefix.Len());
							}
							else if (MIDName.Find(MaterialPrefix, ESearchCase::CaseSensitive) == 0)
							{
								MIDName = MIDName.RightChop(MaterialPrefix.Len());
							}
							else if (MIDName.Find(MaterialInstanceConstantPrefix, ESearchCase::CaseSensitive) == 0)
							{
								MIDName = MIDName.RightChop(MaterialInstanceConstantPrefix.Len());
							}

							const uint32 MaterialPlaceHolderHash = GetTypeHash(MutableMaterialPlaceholder);
							MIDName = FString::Printf(TEXT("%s%s_%u"),TEXT("MID_"),*MIDName, MaterialPlaceHolderHash);
							MIDName = MakeUniqueObjectName(GetTransientPackage(), UMaterialInstanceDynamic::StaticClass(), *MIDName).ToString();
						}
						
						MaterialInstance = UMaterialInstanceDynamic::Create(MaterialTemplate, GetTransientPackage(), FName(MIDName));
#else
						MaterialInstance = UMaterialInstanceDynamic::Create(MaterialTemplate, GetTransientPackage());
#endif
						
						Material.MaterialInterface = MaterialInstance;
					}

					if (MaterialInstance)
					{
						for (const FMutableMaterialPlaceholder::FMutableMaterialPlaceHolderParam& Param : MutableMaterialPlaceholder.GetParams())
						{
							switch (Param.Type)
							{
							case FMutableMaterialPlaceholder::EPlaceHolderParamType::Vector:
								if (Param.LayerIndex < 0)
								{
									FLinearColor Color = Param.Vector;

									// HACK: We encode an invalid value (Nan) for table option "None.
									// Decoding "None" color parameters that use the material color
									if (FMath::IsNaN(Color.R))
									{
										FMaterialParameterInfo ParameterInfo(Param.ParamName);
										MaterialTemplate->GetVectorParameterValue(ParameterInfo, Color);
									}

									MaterialInstance->SetVectorParameterValue(Param.ParamName, Color);
								}
								else
								{
									FMaterialParameterInfo ParameterInfo = FMaterialParameterInfo(Param.ParamName, EMaterialParameterAssociation::LayerParameter, Param.LayerIndex);
									MaterialInstance->SetVectorParameterValueByInfo(ParameterInfo, Param.Vector);
								}

								break;

							case FMutableMaterialPlaceholder::EPlaceHolderParamType::Scalar:
								if (Param.LayerIndex < 0)
								{
									MaterialInstance->SetScalarParameterValue(FName(Param.ParamName), Param.Scalar);
								}
								else
								{
									FMaterialParameterInfo ParameterInfo = FMaterialParameterInfo(Param.ParamName, EMaterialParameterAssociation::LayerParameter, Param.LayerIndex);
									MaterialInstance->SetScalarParameterValueByInfo(ParameterInfo, Param.Scalar);
								}

								break;

							case FMutableMaterialPlaceholder::EPlaceHolderParamType::Texture:
								if (Param.LayerIndex < 0)
								{
									MaterialInstance->SetTextureParameterValue(Param.ParamName, Param.Texture.Texture);
								}
								else
								{
									FMaterialParameterInfo ParameterInfo = FMaterialParameterInfo(Param.ParamName, EMaterialParameterAssociation::LayerParameter, Param.LayerIndex);
									MaterialInstance->SetTextureParameterValueByInfo(ParameterInfo, Param.Texture.Texture);
								}

								if (!bDisableNotifyComponentsOfTextureUpdates)
								{
									NotifyUpdateOnInstanceComponent[InstanceComponentIndex] = true;
								}

								Material.Textures.Add(Param.Texture);

								break;
							}
						}
					}

					MaterialInterface = Material.MaterialInterface;
				}

				// Assign the material to the slot, and add it to the  OverrideMaterials
				MaterialSlot.MaterialInterface = MaterialInterface;
				if (ComponentData)
				{
					ComponentData->OverrideMaterials.Add(MaterialInterface);
				}
			}
		}

		if (!bUseOverrideMaterialsOnly)
		{
			// Mutable hacky LOD Streaming
			if (!OperationData->bStreamMeshLODs)
			{
				// Copy data from the FirstLODAvailable into the LODs below.
				for (int32 LODIndex = OperationData->FirstLODAvailable[ComponentName]; LODIndex < FirstLOD; ++LODIndex)
				{
					SkeletalMesh->GetLODInfo(LODIndex)->LODMaterialMap = SkeletalMesh->GetLODInfo(FirstLOD)->LODMaterialMap;

					TIndirectArray<FSkeletalMeshLODRenderData>& LODRenderData = SkeletalMesh->GetResourceForRendering()->LODRenderData;

					const int32 NumRenderSections = LODRenderData[LODIndex].RenderSections.Num();
					check(NumRenderSections == LODRenderData[FirstLOD].RenderSections.Num());

					if (NumRenderSections == LODRenderData[FirstLOD].RenderSections.Num())
					{
						for (int32 RenderSectionIndex = 0; RenderSectionIndex < NumRenderSections; ++RenderSectionIndex)
						{
							const int32 MaterialIndex = LODRenderData[FirstLOD].RenderSections[RenderSectionIndex].MaterialIndex;
							LODRenderData[LODIndex].RenderSections[RenderSectionIndex].MaterialIndex = MaterialIndex;
						}
					}
				}
			}

			// Force recreate render state after replacing the materials to avoid a crash in the render pipeline if the old materials are GCed while in use.
			RecreateRenderStateOnInstanceComponent[InstanceComponentIndex] |= SkeletalMesh->GetResourceForRendering()->IsInitialized() && SkeletalMesh->GetMaterials() != Materials;

			SkeletalMesh->SetMaterials(Materials);

#if WITH_EDITOR
			if (GEditor && RecreateRenderStateOnInstanceComponent[InstanceComponentIndex])
			{
				// Close all open editors for this mesh to invalidate viewports.
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(SkeletalMesh);
			}
#endif
		}

		// Ensure the number of materials is the same on both sides when using overrides. 
		//check(SkeletalMesh->GetMaterials().Num() == Materials.Num());
	}

	// Force recreate render state if the mesh is reused and the materials have changed.
	// TODO: MTBL-1697 Remove after merging ConvertResources and Callbacks.
	if (RecreateRenderStateOnInstanceComponent.Find(true) != INDEX_NONE || NotifyUpdateOnInstanceComponent.Find(true) != INDEX_NONE)
	{
		MUTABLE_CPUPROFILER_SCOPE(BuildMaterials_RecreateRenderState);

		for (TObjectIterator<UCustomizableObjectInstanceUsage> It; It; ++It)
		{
			UCustomizableObjectInstanceUsage* CustomizableObjectInstanceUsage = *It;

			if (!IsValid(CustomizableObjectInstanceUsage) || CustomizableObjectInstanceUsage->GetCustomizableObjectInstance() != Public)
			{
				continue;
			}

#if WITH_EDITOR
			if (CustomizableObjectInstanceUsage->GetPrivate()->IsNetMode(NM_DedicatedServer))
			{
				continue;
			}
#endif

			const FName& ComponentName = CustomizableObjectInstanceUsage->GetComponentName();
			const int32 ObjectComponentIndex = ModelResources.ComponentNamesPerObjectComponent.IndexOfByKey(ComponentName);
			
			int32 InstanceComponentIndex = -1;
			for (int32 CurrentInstanceIndex=0; CurrentInstanceIndex<OperationData->InstanceUpdateData.Components.Num(); ++CurrentInstanceIndex)
			{
				if (OperationData->InstanceUpdateData.Components[CurrentInstanceIndex].Id.GetValue() == ObjectComponentIndex)
				{
					InstanceComponentIndex = CurrentInstanceIndex;
					break;
				}
			}

			bool bDoRecreateRenderStateOnComponent = RecreateRenderStateOnInstanceComponent.IsValidIndex(InstanceComponentIndex) && RecreateRenderStateOnInstanceComponent[InstanceComponentIndex];
			bool bDoNotifyUpdateOnComponent = NotifyUpdateOnInstanceComponent.IsValidIndex(InstanceComponentIndex) && NotifyUpdateOnInstanceComponent[InstanceComponentIndex];

			if (!bDoRecreateRenderStateOnComponent && !bDoNotifyUpdateOnComponent)
			{
				continue;
			}

			USkeletalMeshComponent* AttachedParent = CustomizableObjectInstanceUsage->GetAttachParent();
			TObjectPtr<USkeletalMesh>* SkeletalMesh = SkeletalMeshes.Find(ComponentName);
			if (!AttachedParent || (SkeletalMesh && AttachedParent->GetSkeletalMeshAsset() != *SkeletalMesh))
			{
				continue;
			}

			if (bDoRecreateRenderStateOnComponent)
			{
				AttachedParent->RecreateRenderState_Concurrent();
			}
			else if (bDoNotifyUpdateOnComponent)
			{
				IStreamingManager::Get().NotifyPrimitiveUpdated(AttachedParent);
			}
		}
	}

	{
		MUTABLE_CPUPROFILER_SCOPE(BuildMaterials_Exchange);

		UCustomizableObjectSystemPrivate* CustomizableObjectSystem = UCustomizableObjectSystem::GetInstance()->GetPrivate();
		TexturesToRelease.Empty();

		for (const FGeneratedTexture& Texture : NewGeneratedTextures)
		{
			CustomizableObjectSystem->AddTextureReference(Texture.Key);
		}

		for (const FGeneratedTexture& Texture : GeneratedTextures)
		{
			if (CustomizableObjectSystem->RemoveTextureReference(Texture.Key))
			{
				if (CustomizableObjectSystem->bReleaseTexturesImmediately)
				{
					TexturesToRelease.Add(Texture); // Texture count is zero, so prepare to release it
				}
			}
		}

		Exchange(GeneratedTextures, NewGeneratedTextures);

		// All pass-through textures and meshes have been set, no need to keep referencing them from the instance
		LoadedPassThroughTexturesPendingSetMaterial.Empty();
		LoadedPassThroughMeshesPendingSetMaterial.Empty();
	}
}


void UCustomizableObjectInstance::SetReplacePhysicsAssets(bool bReplaceEnabled)
{
	bReplaceEnabled ? GetPrivate()->SetCOInstanceFlags(ReplacePhysicsAssets) : GetPrivate()->ClearCOInstanceFlags(ReplacePhysicsAssets);
}


void UCustomizableObjectInstance::SetReuseInstanceTextures(bool bTextureReuseEnabled)
{
	bTextureReuseEnabled ? GetPrivate()->SetCOInstanceFlags(ReuseTextures) : GetPrivate()->ClearCOInstanceFlags(ReuseTextures);
}


void UCustomizableObjectInstance::SetForceGenerateResidentMips(bool bForceGenerateResidentMips)
{
	bForceGenerateResidentMips ? GetPrivate()->SetCOInstanceFlags(ForceGenerateMipTail) : GetPrivate()->ClearCOInstanceFlags(ForceGenerateMipTail);
}


void UCustomizableObjectInstance::SetIsBeingUsedByComponentInPlay(bool bIsUsedByComponentInPlay)
{
	bIsUsedByComponentInPlay ? GetPrivate()->SetCOInstanceFlags(UsedByComponentInPlay) : GetPrivate()->ClearCOInstanceFlags(UsedByComponentInPlay);
}


bool UCustomizableObjectInstance::GetIsBeingUsedByComponentInPlay() const
{
	return GetPrivate()->HasCOInstanceFlags(UsedByComponentInPlay);
}


void UCustomizableObjectInstance::SetIsDiscardedBecauseOfTooManyInstances(bool bIsDiscarded)
{
	bIsDiscarded ? GetPrivate()->SetCOInstanceFlags(DiscardedByNumInstancesLimit) : GetPrivate()->ClearCOInstanceFlags(DiscardedByNumInstancesLimit);
}


bool UCustomizableObjectInstance::GetIsDiscardedBecauseOfTooManyInstances() const
{
	return GetPrivate()->HasCOInstanceFlags(DiscardedByNumInstancesLimit);
}


void UCustomizableObjectInstance::SetIsPlayerOrNearIt(bool bIsPlayerorNearIt)
{
	bIsPlayerorNearIt ? GetPrivate()->SetCOInstanceFlags(UsedByPlayerOrNearIt) : GetPrivate()->ClearCOInstanceFlags(UsedByPlayerOrNearIt);
}


float UCustomizableObjectInstance::GetMinSquareDistToPlayer() const
{
	return GetPrivate()->MinSquareDistFromComponentToPlayer;
}

void UCustomizableObjectInstance::SetMinSquareDistToPlayer(float NewValue)
{
	GetPrivate()->MinSquareDistFromComponentToPlayer = NewValue;
}


int32 UCustomizableObjectInstance::GetNumComponents() const
{
	return GetCustomizableObject() ? GetCustomizableObject()->GetComponentCount() : 0;
}


void UCustomizableObjectInstance::SetRequestedLODs(const TMap<FName, uint8>& InMinLODs, const TMap<FName, uint8>& InFirstRequestedLOD, FMutableInstanceUpdateMap& InOutRequestedUpdates)
{
	check(PrivateData);
	
	if (!GetPrivate()->CanUpdateInstance())
	{
		return;
	}

	if (GetPrivate()->SkeletalMeshStatus == ESkeletalMeshStatus::Error)
	{
		return;
	}

	UCustomizableObject* CustomizableObject = GetCustomizableObject();

	if (!CustomizableObject)
	{
		return;
	}

	if (IsStreamingEnabled(*CustomizableObject))
	{
		return;
	}

	if (CVarPreserveUserLODsOnFirstGeneration.GetValueOnGameThread() &&
		CustomizableObject->bPreserveUserLODsOnFirstGeneration &&
		GetPrivate()->SkeletalMeshStatus != ESkeletalMeshStatus::Success)
	{
		return;
	}
	
	FMutableUpdateCandidate MutableUpdateCandidate(this);

	// Clamp Min LOD
	const UModelResources* ModelResources = CustomizableObject->GetPrivate()->GetModelResources();
	if (!ModelResources)
	{
		return;
	}

	bool bMinLODChanged = false;
	
	// Save the new LODs
	MutableUpdateCandidate.MinLOD = InMinLODs;
	MutableUpdateCandidate.FirstRequestedLOD = Descriptor.GetFirstRequestedLOD();
	
	const TMap<FName, uint8>& FirstRequestedLOD = GetPrivate()->CommittedDescriptorHash.FirstRequestedLOD;
	
	for (const FName& ComponentName : ModelResources->ComponentNamesPerObjectComponent)
	{	
		uint8& InMinLOD = MutableUpdateCandidate.MinLOD.FindOrAdd(ComponentName);
		if (const uint8* Result = InMinLODs.Find(ComponentName))
		{
			InMinLOD = *Result;
		}

		const uint8 MinLODIdx = CustomizableObject->GetPrivate()->GetMinLODIndex(ComponentName);
		MutableUpdateCandidate.QualitySettingMinLODs.Add(ComponentName, MinLODIdx);

		int32 MaxLODIdx = 0;
		if (const uint8* Found = ModelResources->NumLODsAvailable.Find(ComponentName))
		{
			MaxLODIdx = *Found - 1;
		}

		InMinLOD = FMath::Clamp(InMinLOD, MinLODIdx, MaxLODIdx);

		uint8 DescriptorMinLOD = 0;
		if (const uint8* Result = Descriptor.MinLOD.Find(ComponentName))
		{
			DescriptorMinLOD = *Result;
		}

		bMinLODChanged |= DescriptorMinLOD != InMinLOD;

		if (UCustomizableObjectSystem::GetInstance()->IsOnlyGenerateRequestedLODsEnabled())
		{
			uint8 CurrentMinLOD = 0;
			if (const uint8* Result = GetPrivate()->CommittedDescriptor.MinLOD.Find(ComponentName))
			{
				CurrentMinLOD = *Result;
			}

			PrivateData->SetCOInstanceFlags(InMinLOD > CurrentMinLOD ? PendingLODsDowngrade : ECONone);

			uint8 FirstNonStreamedLODIndex = 0;
			if (const uint8* Found = ModelResources->NumLODsToStream.Find(ComponentName))
			{
				FirstNonStreamedLODIndex = *Found;
			}

			MutableUpdateCandidate.FirstRequestedLOD.Add(ComponentName, FirstNonStreamedLODIndex);

			uint8 PredictedLOD = FirstNonStreamedLODIndex;
			if (const uint8* Result = InFirstRequestedLOD.Find(ComponentName))
			{
				PredictedLOD = FMath::Min(*Result, PredictedLOD);
			}

			if (const uint8* Result = FirstRequestedLOD.Find(ComponentName))
			{
				PredictedLOD = FMath::Min(*Result, PredictedLOD);
			}

			PredictedLOD = FMath::Clamp(PredictedLOD, MinLODIdx, MaxLODIdx);

			// Save new RequestedLODs
			MutableUpdateCandidate.FirstRequestedLOD[ComponentName] = PredictedLOD;
		}
	}
	
	if (bMinLODChanged || FirstRequestedLOD != MutableUpdateCandidate.FirstRequestedLOD)
	{
		// TODO: Remove this flag as it will become redundant with the new InOutRequestedUpdates system
		PrivateData->SetCOInstanceFlags(PendingLODsUpdate);

		InOutRequestedUpdates.Add(this, MutableUpdateCandidate);
	}
}


#if WITH_EDITOR
void UCustomizableObjectInstance::Bake(const FBakingConfiguration& InBakingConfiguration)
{
	if (ICustomizableObjectEditorModule* Module = ICustomizableObjectEditorModule::Get())
	{
		Module->BakeCustomizableObjectInstance(this, InBakingConfiguration);
	}
	else
	{
		// Notify of the error
		UE_LOG(LogMutable, Error, TEXT("The module \" ICustomizableObjectEditorModule \" could not be loaded and therefore the baking operation could not be started."));
		if (InBakingConfiguration.OnBakeOperationCompletedCallback.IsBound())
		{
			FCustomizableObjectInstanceBakeOutput Output;
			Output.bWasBakeSuccessful = false;
			Output.SavedPackages.Empty();
			InBakingConfiguration.OnBakeOperationCompletedCallback.Execute(Output);
		}
	}
}
#endif


USkeletalMesh* UCustomizableObjectInstance::GetSkeletalMesh(int32 ObjectComponentIndex) const
{
	return GetComponentMeshSkeletalMesh(FName(FString::FromInt(ObjectComponentIndex)));
}


USkeletalMesh* UCustomizableObjectInstance::GetComponentMeshSkeletalMesh(const FName& ComponentName) const
{
	TObjectPtr<USkeletalMesh>* Result = GetPrivate()->SkeletalMeshes.Find(ComponentName);
	return Result ? *Result : nullptr;
}


bool UCustomizableObjectInstance::HasAnySkeletalMesh() const
{
	return !GetPrivate()->SkeletalMeshes.IsEmpty();
}


bool UCustomizableObjectInstance::HasAnyParameters() const
{
	return Descriptor.HasAnyParameters();	
}


TArray<FName> UCustomizableObjectInstance::GetComponentNames() const
{
	TArray<FName> GeneratedComponents;

	// For now, the instances don't really hold a direct array of generated components FNames. 
	// They can be identified with the ones having a valid SkeletalMesh in the SkeletalMeshes array, but this will 
	// not longer work when we have components that don't have a SkeletalMesh, like grooms, or panel clothing. (TODO)
	for ( const TPair<FName, TObjectPtr<USkeletalMesh>>& Entry : GetPrivate()->SkeletalMeshes )
	{
		if (Entry.Value)
		{
			GeneratedComponents.Add(Entry.Key);
		}
	}

	return GeneratedComponents;
}


TSubclassOf<UAnimInstance> UCustomizableObjectInstance::GetAnimBP(FName ComponentName, const FName& SlotName) const
{
	FCustomizableInstanceComponentData* ComponentData =	GetPrivate()->GetComponentData(ComponentName);
	
	if (!ComponentData)
	{
		FString ErrorMsg = FString::Printf(TEXT("Tried to access an invalid component index [%s] in a Mutable Instance."), *ComponentName.ToString());
		UE_LOG(LogMutable, Error, TEXT("%s"), *ErrorMsg);
#if WITH_EDITOR
		FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
		MessageLogModule.RegisterLogListing(FName("Mutable"), FText::FromString(FString("Mutable")));
		FMessageLog MessageLog("Mutable");

		MessageLog.Notify(FText::FromString(ErrorMsg), EMessageSeverity::Error, true);
#endif

		return nullptr;
	}

	TSoftClassPtr<UAnimInstance>* Result = ComponentData->AnimSlotToBP.Find(SlotName);

	return Result ? Result->Get() : nullptr;
}

const FGameplayTagContainer& UCustomizableObjectInstance::GetAnimationGameplayTags() const
{
	return GetPrivate()->AnimBPGameplayTags;
}

namespace MutablePrivate
{
	template<typename DELEGATE>
	void InternalForEachAnimInstance(UCustomizableInstancePrivate* Private, FName ComponentName, DELEGATE Delegate)
	{
		// allow us to log out both bad states with one pass
		bool bAnyErrors = false;

		if (!Delegate.IsBound())
		{
			FString ErrorMsg = FString::Printf(TEXT("Attempting to iterate over AnimInstances with an unbound delegate for component [%s]."), *ComponentName.ToString());
			UE_LOG(LogMutable, Warning, TEXT("%s"), *ErrorMsg);
#if WITH_EDITOR
			FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
			MessageLogModule.RegisterLogListing(FName("Mutable"), FText::FromString(FString("Mutable")));
			FMessageLog MessageLog("Mutable");

			MessageLog.Notify(FText::FromString(ErrorMsg), EMessageSeverity::Warning, true);
#endif
			bAnyErrors = true;
		}

		const FCustomizableInstanceComponentData* ComponentData = Private->GetComponentData(ComponentName);

		if (!ComponentData)
		{
			FString ErrorMsg = FString::Printf(TEXT("Tried to access an invalid component [%s] in a Mutable Instance."), *ComponentName.ToString());
			UE_LOG(LogMutable, Error, TEXT("%s"), *ErrorMsg);
#if WITH_EDITOR
			FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
			MessageLogModule.RegisterLogListing(FName("Mutable"), FText::FromString(FString("Mutable")));
			FMessageLog MessageLog("Mutable");

			MessageLog.Notify(FText::FromString(ErrorMsg), EMessageSeverity::Error, true);
#endif

			bAnyErrors = true;
		}

		if (bAnyErrors)
		{
			return;
		}

		for (const TPair<FName, TSoftClassPtr<UAnimInstance>>& MapElem : ComponentData->AnimSlotToBP)
		{
			const FName& Index = MapElem.Key;
			const TSoftClassPtr<UAnimInstance>& AnimBP = MapElem.Value;

			// if this _can_ resolve to a real AnimBP
			if (!AnimBP.IsNull())
			{
				// force a load right now - we don't know whether we would have loaded already - this could be called in editor
				const TSubclassOf<UAnimInstance> LiveAnimBP = MutablePrivate::LoadClass(AnimBP);
				if (LiveAnimBP)
				{
					Delegate.Execute(Index, LiveAnimBP);
				}
			}
		}
	}

}


void UCustomizableObjectInstance::ForEachComponentAnimInstance(FName ComponentName, FEachComponentAnimInstanceClassDelegate Delegate) const
{
	MutablePrivate::InternalForEachAnimInstance<>(GetPrivate(), ComponentName, Delegate);
}


void UCustomizableObjectInstance::ForEachComponentAnimInstance(FName ComponentName, FEachComponentAnimInstanceClassNativeDelegate Delegate) const
{
	MutablePrivate::InternalForEachAnimInstance<>(GetPrivate(), ComponentName, Delegate);
}

// Deprecated
void UCustomizableObjectInstance::ForEachAnimInstance(int32 ObjectComponentIndex, FEachComponentAnimInstanceClassDelegate Delegate) const
{
	UCustomizableObject* CO = GetCustomizableObject();
	if (CO)
	{
		FName ComponentName = CO->GetPrivate()->GetComponentName(FCustomizableObjectComponentIndex(ObjectComponentIndex));
		MutablePrivate::InternalForEachAnimInstance<>(GetPrivate(), ComponentName, Delegate);
	}
}


// Deprecated
void UCustomizableObjectInstance::ForEachAnimInstance(int32 ObjectComponentIndex, FEachComponentAnimInstanceClassNativeDelegate Delegate) const
{
	UCustomizableObject* CO = GetCustomizableObject();
	if (CO)
	{
		FName ComponentName = CO->GetPrivate()->GetComponentName(FCustomizableObjectComponentIndex(ObjectComponentIndex));
		MutablePrivate::InternalForEachAnimInstance<>(GetPrivate(), ComponentName, Delegate);
	}
}


bool UCustomizableObjectInstance::AnimInstanceNeedsFixup(TSubclassOf<UAnimInstance> AnimInstanceClass) const
{
	return PrivateData->AnimBpPhysicsAssets.Contains(AnimInstanceClass);
}


void UCustomizableObjectInstance::AnimInstanceFixup(UAnimInstance* InAnimInstance) const
{
	if (!InAnimInstance)
	{
		return;
	}

	TSubclassOf<UAnimInstance> AnimInstanceClass = InAnimInstance->GetClass();

	const TArray<FAnimInstanceOverridePhysicsAsset>* AnimInstanceOverridePhysicsAssets = 
			PrivateData->GetGeneratedPhysicsAssetsForAnimInstance(AnimInstanceClass);
	
	if (!AnimInstanceOverridePhysicsAssets)
	{
		return;
	}

	// Swap RigidBody anim nodes override physics assets with mutable generated ones.
	if (UAnimBlueprintGeneratedClass* AnimClass = Cast<UAnimBlueprintGeneratedClass>(AnimInstanceClass))
	{
		bool bPropertyMismatchFound = false;
		const int32 AnimNodePropertiesNum = AnimClass->AnimNodeProperties.Num();

		for (const FAnimInstanceOverridePhysicsAsset& PropIndexAndAsset : *AnimInstanceOverridePhysicsAssets)
		{
			check(PropIndexAndAsset.PropertyIndex >= 0);
			if (PropIndexAndAsset.PropertyIndex >= AnimNodePropertiesNum)
			{
				bPropertyMismatchFound = true;
				continue;
			}

			const int32 AnimNodePropIndex = PropIndexAndAsset.PropertyIndex;

			FStructProperty* StructProperty = AnimClass->AnimNodeProperties[AnimNodePropIndex];

			if (!ensure(StructProperty))
			{
				bPropertyMismatchFound = true;
				continue;
			}

			const bool bIsRigidBodyNode = StructProperty->Struct->IsChildOf(FAnimNode_RigidBody::StaticStruct());

			if (!bIsRigidBodyNode)
			{
				bPropertyMismatchFound = true;
				continue;
			}

			FAnimNode_RigidBody* RbanNode = StructProperty->ContainerPtrToValuePtr<FAnimNode_RigidBody>(InAnimInstance);

			if (!ensure(RbanNode))
			{
				bPropertyMismatchFound = true;
				continue;
			}

			RbanNode->OverridePhysicsAsset = PropIndexAndAsset.PhysicsAsset;
		}
#if WITH_EDITOR
		if (bPropertyMismatchFound)
		{
			UE_LOG(LogMutable, Warning, TEXT("AnimBp %s is not in sync with the data stored in the CO %s. A CO recompilation may be needed."),
				*AnimInstanceClass.Get()->GetName(), 
				*GetCustomizableObject()->GetName());
		}
#endif
	}
}

const TArray<FAnimInstanceOverridePhysicsAsset>* UCustomizableInstancePrivate::GetGeneratedPhysicsAssetsForAnimInstance(TSubclassOf<UAnimInstance> AnimInstanceClass) const
{
	const FAnimBpGeneratedPhysicsAssets* Found = AnimBpPhysicsAssets.Find(AnimInstanceClass);

	if (!Found)
	{
		return nullptr;
	}

	return &Found->AnimInstancePropertyIndexAndPhysicsAssets;
}


FInstancedStruct UCustomizableObjectInstance::GetExtensionInstanceData(const UCustomizableObjectExtension* Extension) const
{
	const FExtensionInstanceData* FoundData = Algo::FindBy(PrivateData->ExtensionInstanceData, Extension, &FExtensionInstanceData::Extension);
	if (FoundData)
	{
		return FoundData->Data;
	}

	// Data not found. Return an empty instance.
	return FInstancedStruct();
}


TSet<UAssetUserData*> UCustomizableObjectInstance::GetMergedAssetUserData(int32 ComponentIndex) const
{
	UCustomizableInstancePrivate* PrivateInstanceData = GetPrivate();

	if (PrivateInstanceData && PrivateInstanceData->ComponentsData.IsValidIndex(ComponentIndex))
	{
		TSet<UAssetUserData*> Set;
		
		// Have to convert to UAssetUserData* because BP functions don't support TObjectPtr
		for (const TObjectPtr<UAssetUserData>& Elem : PrivateInstanceData->ComponentsData[ComponentIndex].AssetUserDataArray)
		{
			Set.Add(Elem);
		}

		return Set;
	}
	else
	{
		return TSet<UAssetUserData*>();
	}
}


bool UCustomizableObjectInstance::CanUpdateInstance() const
{
	return GetPrivate()->CanUpdateInstance();
}


#if WITH_EDITORONLY_DATA

void CalculateBonesToRemove(const FSkeletalMeshLODRenderData& LODResource, const FReferenceSkeleton& RefSkeleton, TArray<FBoneReference>& OutBonesToRemove)
{
	const int32 NumBones = RefSkeleton.GetNum();
	OutBonesToRemove.Empty(NumBones);

	TArray<bool> RemovedBones;
	RemovedBones.Init(true, NumBones);

	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		if (LODResource.RequiredBones.Find((uint16)BoneIndex) != INDEX_NONE)
		{
			RemovedBones[BoneIndex] = false;
			continue;
		}

		const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
		if (RemovedBones.IsValidIndex(ParentIndex) && !RemovedBones[ParentIndex])
		{
			OutBonesToRemove.Add(RefSkeleton.GetBoneName(BoneIndex));
		}
	}
}

void UCustomizableInstancePrivate::RegenerateImportedModels()
{
	MUTABLE_CPUPROFILER_SCOPE(RegenerateImportedModels);

	using UVsVectorType = typename TDecay<decltype(DeclVal<FSoftSkinVertex>().UVs[0])>::Type;
	struct FMeshDataConvertJob
	{
		int32 NumIndices = 0;
		int32 IndicesOffset = 0;
		const FRawStaticIndexBuffer16or32Interface* IndexBuffer = nullptr;
		uint32* DestIndexBuffer = nullptr;

		int32 NumVertices = 0;
		int32 VerticesOffset = 0;
		const FStaticMeshVertexBuffers*	StaticVertexBuffers = nullptr;
		const FSkinWeightVertexBuffer* SkinWeightVertexBuffer = nullptr;
		FSoftSkinVertex* DestVertexBuffer = nullptr;
	};
	
	constexpr int32 MaxJobCost = 1 << 18;
	constexpr int32 MaxVerticesPerJob = FMath::Max<int32>(1, MaxJobCost / sizeof(FSoftSkinVertex));
	constexpr int32 MaxIndicesPerJob =  FMath::Max<int32>(1, MaxJobCost / sizeof(int32));

	TArray<FMeshDataConvertJob, TInlineAllocator<64>> Jobs;   
	TArray<int32, TInlineAllocator<64>> JobRanges;
	JobRanges.Add(0);
	
	for (const TTuple<FName, TObjectPtr<USkeletalMesh>>& Tuple : SkeletalMeshes)
	{
		USkeletalMesh* SkeletalMesh = Tuple.Get<1>();
		
		if (!SkeletalMesh)
		{
			continue;
		}

		const bool bIsTransientMesh = static_cast<bool>(SkeletalMesh->HasAllFlags(EObjectFlags::RF_Transient));

		if (!bIsTransientMesh)
		{
			// This must be a pass-through referenced mesh so don't do anything to it
			continue;
		}

		FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
		if (!RenderData || RenderData->IsInitialized())
		{
			continue;
		}

		for (UClothingAssetBase* ClothingAssetBase : SkeletalMesh->GetMeshClothingAssets())
		{
			if (!ClothingAssetBase)
			{
				continue;
			}

			UClothingAssetCommon* ClothAsset = Cast<UClothingAssetCommon>(ClothingAssetBase);

			if (!ClothAsset)
			{
				continue;
			}

			if (!ClothAsset->LodData.Num())
			{
				continue;
			}

			for (FClothLODDataCommon& ClothLodData : ClothAsset->LodData)
			{
				ClothLodData.PointWeightMaps.Empty(16);
				for (TPair<uint32, FPointWeightMap>& WeightMap : ClothLodData.PhysicalMeshData.WeightMaps)
				{
					if (WeightMap.Value.Num())
					{
						FPointWeightMap& PointWeightMap = ClothLodData.PointWeightMaps.AddDefaulted_GetRef();
						PointWeightMap.Initialize(WeightMap.Value, WeightMap.Key);
					}
				}
			}
		}

		FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel();
		ImportedModel->bGuidIsHash = false;
		ImportedModel->SkeletalMeshModelGUID = FGuid::NewGuid();

		ImportedModel->LODModels.Empty();
	
		int32 OriginalIndex = 0;
		for (int32 LODIndex = 0; LODIndex < RenderData->LODRenderData.Num(); ++LODIndex)
		{
			ImportedModel->LODModels.Add(new FSkeletalMeshLODModel());
			FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[LODIndex];

			FSkeletalMeshLODRenderData& LODRenderData = RenderData->LODRenderData[LODIndex];

			LODModel.ActiveBoneIndices = LODRenderData.ActiveBoneIndices;
			LODModel.NumTexCoords = LODRenderData.GetNumTexCoords();
			LODModel.RequiredBones = LODRenderData.RequiredBones;
			LODModel.NumVertices = LODRenderData.GetNumVertices();

			// Indices
			if (LODRenderData.MultiSizeIndexContainer.IsIndexBufferValid())
			{
				const FRawStaticIndexBuffer16or32Interface* IndexBuffer =
						LODRenderData.MultiSizeIndexContainer.GetIndexBuffer();

				const int32 NumIndices = IndexBuffer->Num();
				LODModel.IndexBuffer.SetNum(NumIndices);

				uint32* BaseDestIndexBuffer = LODModel.IndexBuffer.GetData();
				
				const int32 NumIndicesJobs = FMath::DivideAndRoundUp(NumIndices, MaxIndicesPerJob);

				int32 CurrentJobIndex = Jobs.Num();
				Jobs.SetNum(Jobs.Num() + NumIndicesJobs);
		
				for (int32 I = 0; I < NumIndicesJobs; ++I)
				{
					FMeshDataConvertJob Job;
					Job.NumIndices = FMath::Min(MaxIndicesPerJob, NumIndices - I*MaxIndicesPerJob); 
					Job.IndexBuffer = IndexBuffer;
					Job.IndicesOffset = I*MaxIndicesPerJob; 
					Job.DestIndexBuffer = BaseDestIndexBuffer + I*MaxIndicesPerJob;

					Jobs[I + CurrentJobIndex] = Job;  
				}
			}

			LODModel.Sections.SetNum(LODRenderData.RenderSections.Num());

			for (int32 SectionIndex = 0; SectionIndex < LODRenderData.RenderSections.Num(); ++SectionIndex)
			{
				check(!LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetUseHighPrecisionTangentBasis());
				
				const FSkelMeshRenderSection& RenderSection = LODRenderData.RenderSections[SectionIndex];
				FSkelMeshSection& ImportedSection = ImportedModel->LODModels[LODIndex].Sections[SectionIndex];

				ImportedSection.CorrespondClothAssetIndex = RenderSection.CorrespondClothAssetIndex;
				ImportedSection.ClothingData = RenderSection.ClothingData;

				if (RenderSection.ClothMappingDataLODs.Num())
				{
					TArray<FMeshToMeshVertData>& ImportedClothMappingData = ImportedSection.ClothMappingDataLODs.AddDefaulted_GetRef();
					
					const int32 NumClothVerts = LODRenderData.ClothVertexBuffer.GetNumVertices();
					ImportedClothMappingData.SetNumUninitialized(NumClothVerts);
				
					for (int32 ClothVertDataIndex = 0; ClothVertDataIndex < NumClothVerts; ++ClothVertDataIndex)
					{
						ImportedClothMappingData[ClothVertDataIndex] = LODRenderData.ClothVertexBuffer.MappingData(ClothVertDataIndex);
					}
				}

				// Vertices
				ImportedSection.NumVertices = RenderSection.NumVertices;
				ImportedSection.SoftVertices.Empty(RenderSection.NumVertices);
				ImportedSection.SoftVertices.AddUninitialized(RenderSection.NumVertices);
				ImportedSection.bUse16BitBoneIndex = LODRenderData.DoesVertexBufferUse16BitBoneIndex();

				const int32 SectionNumVertices = RenderSection.NumVertices;
				const int32 SectionBaseVertexIndex = RenderSection.BaseVertexIndex;
				const FStaticMeshVertexBuffers*	StaticVertexBuffers = &LODRenderData.StaticVertexBuffers;
				const FSkinWeightVertexBuffer* SkinWeightVertexBuffer = &LODRenderData.SkinWeightVertexBuffer;

				FSoftSkinVertex* BaseSectionSoftVertex = ImportedSection.SoftVertices.GetData();

				int32 NumSectionJobs = FMath::DivideAndRoundUp<int32>(RenderSection.NumVertices, MaxVerticesPerJob);

				int32 FirstSectionJobIndex = Jobs.Num();
				Jobs.SetNum(Jobs.Num() + NumSectionJobs);

				for (int32 I = 0; I < NumSectionJobs; ++I)
				{
					FMeshDataConvertJob Job;
					Job.NumVertices = FMath::Min(MaxVerticesPerJob, SectionNumVertices - I*MaxVerticesPerJob); 
					Job.StaticVertexBuffers = StaticVertexBuffers;
					Job.SkinWeightVertexBuffer = SkinWeightVertexBuffer;
					Job.VerticesOffset = SectionBaseVertexIndex + I*MaxVerticesPerJob; 
					Job.DestVertexBuffer = BaseSectionSoftVertex + I*MaxVerticesPerJob;

					Jobs[I + FirstSectionJobIndex] = Job;  
				}

				// Triangles
				ImportedSection.NumTriangles = RenderSection.NumTriangles;
				ImportedSection.BaseIndex = RenderSection.BaseIndex;
				ImportedSection.BaseVertexIndex = RenderSection.BaseVertexIndex;
				ImportedSection.BoneMap = RenderSection.BoneMap;

				// Add bones to remove
				CalculateBonesToRemove(LODRenderData, SkeletalMesh->GetRefSkeleton(), SkeletalMesh->GetLODInfo(LODIndex)->BonesToRemove);

				const TArray<int32>& LODMaterialMap = SkeletalMesh->GetLODInfo(LODIndex)->LODMaterialMap;

				if (LODMaterialMap.IsValidIndex(RenderSection.MaterialIndex))
				{
					ImportedSection.MaterialIndex = LODMaterialMap[RenderSection.MaterialIndex];
				}
				else
				{
					// The material should have been in the LODMaterialMap
					ensureMsgf(false, TEXT("Unexpected material index in UCustomizableInstancePrivate::RegenerateImportedModel"));

					// Fallback index, may shift materials around sections
					if (SkeletalMesh->GetMaterials().IsValidIndex(RenderSection.MaterialIndex))
					{
						ImportedSection.MaterialIndex = RenderSection.MaterialIndex;
					}
					else
					{
						ImportedSection.MaterialIndex = 0;
					}
				}

				ImportedSection.MaxBoneInfluences = RenderSection.MaxBoneInfluences;
				ImportedSection.OriginalDataSectionIndex = OriginalIndex++;

				FSkelMeshSourceSectionUserData& SectionUserData = LODModel.UserSectionsData.FindOrAdd(ImportedSection.OriginalDataSectionIndex);
				SectionUserData.bCastShadow = RenderSection.bCastShadow;
				SectionUserData.bDisabled = RenderSection.bDisabled;

				SectionUserData.CorrespondClothAssetIndex = RenderSection.CorrespondClothAssetIndex;
				SectionUserData.ClothingData.AssetGuid = RenderSection.ClothingData.AssetGuid;
				SectionUserData.ClothingData.AssetLodIndex = RenderSection.ClothingData.AssetLodIndex;
				
				LODModel.SyncronizeUserSectionsDataArray();

				// DDC keys
				const USkeletalMeshLODSettings* LODSettings = SkeletalMesh->GetLODSettings();
				const bool bValidLODSettings = LODSettings && LODSettings->GetNumberOfSettings() > LODIndex;
				const FSkeletalMeshLODGroupSettings* SkeletalMeshLODGroupSettings = bValidLODSettings ? &LODSettings->GetSettingsForLODLevel(LODIndex) : nullptr;

				FSkeletalMeshLODInfo* LODInfo = SkeletalMesh->GetLODInfo(LODIndex);
				LODInfo->BuildGUID = LODInfo->ComputeDeriveDataCacheKey(SkeletalMeshLODGroupSettings);

				LODModel.BuildStringID = LODModel.GetLODModelDeriveDataKey();
			}
		}

		// Try to bundle Jobs so all cost roughly the same. Large Jobs are already split so they cost about MaxJobCost.
		// It uses a gready approach and assumes in general Jobs are sorted by cost.
		const int32 NumJobs = Jobs.Num();
		for (int32 JobIndex = 0; JobIndex < NumJobs;)
		{
			int32 RangeJobCost = 0;
			for (; JobIndex < NumJobs;)
			{
				int32 CurrentJobCost = Jobs[JobIndex].NumVertices*sizeof(FSoftSkinVertex) + Jobs[JobIndex].NumIndices*sizeof(int32);

				RangeJobCost += CurrentJobCost;
				if (RangeJobCost >= MaxJobCost)
				{
					// Go to the next Job if the current job alone cost is larger than MaxJobCost
					// and no other job has been processed for the range.
					JobIndex += static_cast<int32>(CurrentJobCost == RangeJobCost);
					break;
				}

				++JobIndex;
			}

			JobRanges.Add(JobIndex);
		}
	}

	{
		MUTABLE_CPUPROFILER_SCOPE(DoImportedModelMeshDataConversion)

		ParallelFor(JobRanges.Num() - 1, [&JobRanges, &Jobs](int32 JobId)
		{
			int32 JobRangeBegin = JobRanges[JobId];
			int32 JobRangeEnd = JobRanges[JobId + 1];
			for (int32 J = JobRangeBegin; J < JobRangeEnd; ++J)
			{
				FMeshDataConvertJob Job = Jobs[J];
		
				if (Job.NumIndices > 0)
				{
					MUTABLE_CPUPROFILER_SCOPE(DoImportedModelMeshDataConversion_Indices)

					for (int32 Index = 0; Index < Job.NumIndices; ++Index)
					{
						Job.DestIndexBuffer[Index] = Job.IndexBuffer->Get(Job.IndicesOffset + Index);
					}
				}

				if (Job.NumVertices > 0)
				{
					MUTABLE_CPUPROFILER_SCOPE(DoImportedModelMeshDataConversion_Vertices)

					check(Job.StaticVertexBuffers);
					check(Job.SkinWeightVertexBuffer);
					check(Job.DestVertexBuffer);

					const FPositionVertex* PositionBuffer = 
							static_cast<const FPositionVertex*>(Job.StaticVertexBuffers->PositionVertexBuffer.GetVertexData()) + Job.VerticesOffset;

					const FPackedNormal* TangentBuffer = 
							static_cast<const FPackedNormal*>(Job.StaticVertexBuffers->StaticMeshVertexBuffer.GetTangentData()) + Job.VerticesOffset*2;

					const int32 NumTexCoords = Job.StaticVertexBuffers->StaticMeshVertexBuffer.GetNumTexCoords();
					const int32 UVSize = Job.StaticVertexBuffers->StaticMeshVertexBuffer.GetUseFullPrecisionUVs() ? 2 * sizeof(float) : 2 * sizeof(FFloat16);
					const uint8* TexCoordBuffer = 
							static_cast<const uint8*>(Job.StaticVertexBuffers->StaticMeshVertexBuffer.GetTexCoordData()) + Job.VerticesOffset*NumTexCoords*UVSize;
					
					const FColor* ColorBuffer = 
							static_cast<const FColor*>(Job.StaticVertexBuffers->ColorVertexBuffer.GetVertexData());
			
					const bool bHasColor = !!ColorBuffer;
					ColorBuffer += Job.VerticesOffset;

					const FSkinWeightVertexBuffer* SkinWeightBuffer = Job.SkinWeightVertexBuffer;

					const int32 MaxBoneInfluences = Job.SkinWeightVertexBuffer->GetMaxBoneInfluences();

					for (int32 JobVertexIndex = 0; JobVertexIndex < Job.NumVertices; ++JobVertexIndex)
					{
						FSoftSkinVertex* Vertex = Job.DestVertexBuffer + JobVertexIndex;
						FMemory::Memzero(Vertex, sizeof(FSoftSkinVertex));

						Vertex->Position = PositionBuffer[JobVertexIndex].Position;

						const FPackedNormal* Tangent = TangentBuffer + JobVertexIndex*2;
					
						Vertex->TangentX = Tangent[0].ToFVector3f();
						Vertex->TangentZ = Tangent[1].ToFVector3f();
						float TangentSign = Tangent[1].Vector.W < 0 ? -1.0f : 1.0f;
						Vertex->TangentY = FVector3f::CrossProduct(Vertex->TangentZ, Vertex->TangentX) * TangentSign;

						const void* TexCoord = TexCoordBuffer + JobVertexIndex*NumTexCoords*UVSize;

						// Switch based jumptable.
						if (UVSize == 4)
						{
							const FFloat16* TypedSource = reinterpret_cast<const FFloat16*>(TexCoord);
							switch (NumTexCoords)
							{
							case 4: Vertex->UVs[3] = { TypedSource[6], TypedSource[7] }; // Fall through
							case 3: Vertex->UVs[2] = { TypedSource[4], TypedSource[5] }; // Fall through
							case 2: Vertex->UVs[1] = { TypedSource[2], TypedSource[3] }; // Fall through
							case 1: Vertex->UVs[0] = { TypedSource[0], TypedSource[1] }; // Fall through
							default: break;
							}
						}
						else
						{
							const FVector2f* TypedSource = reinterpret_cast<const FVector2f*>(TexCoord);
							switch (NumTexCoords)
							{
							case 4: Vertex->UVs[3] = TypedSource[3]; // Fall through
							case 3: Vertex->UVs[2] = TypedSource[2]; // Fall through
							case 2: Vertex->UVs[1] = TypedSource[1]; // Fall through
							case 1: Vertex->UVs[0] = TypedSource[0]; // Fall through
							default: break;
							}
						}

						Vertex->Color = bHasColor ? ColorBuffer[JobVertexIndex] : FColor::White;

						const int32 SourceVertexIndex = (JobVertexIndex + Job.VerticesOffset);

						for (int32 InfluenceIndex = 0; InfluenceIndex < MaxBoneInfluences; ++InfluenceIndex)
						{
							Vertex->InfluenceBones[InfluenceIndex] = SkinWeightBuffer->GetBoneIndex(SourceVertexIndex, InfluenceIndex);
							Vertex->InfluenceWeights[InfluenceIndex] = SkinWeightBuffer->GetBoneWeight(SourceVertexIndex, InfluenceIndex);
						}
					}
				}
			}
		});
	}
}

#endif

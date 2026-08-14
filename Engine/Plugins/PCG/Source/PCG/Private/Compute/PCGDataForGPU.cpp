// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/PCGDataForGPU.h"

#include "PCGContext.h"
#include "PCGData.h"
#include "PCGEdge.h"
#include "PCGNode.h"
#include "PCGParamData.h"
#include "PCGPoint.h"
#include "PCGSettings.h"
#include "Compute/PCGComputeCommon.h"
#include "Compute/PCGComputeGraph.h"
#include "Compute/PCGDataBinding.h"
#include "Compute/Data/PCGProxyForGPUData.h"
#include "Data/PCGPointData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Helpers/PCGAsync.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Utils/PCGLogErrors.h"

#include "RenderGraphResources.h"
#include "Async/ParallelFor.h"

#define LOCTEXT_NAMESPACE "PCGDataForGPU"

using namespace PCGComputeConstants;

#if WITH_EDITOR
static TAutoConsoleVariable<bool> CVarWarnOnGPUReadbacks(
	TEXT("pcg.Graph.GPU.WarnOnGPUReadbacks"),
	false,
	TEXT("Emits warnings on nodes which trigger a readback of data from GPU to CPU."));
#endif

namespace PCGDataForGPUConstants
{
	const static FPCGKernelAttributeDesc PointPropertyDescs[NUM_POINT_PROPERTIES] =
	{
		FPCGKernelAttributeDesc(POINT_POSITION_ATTRIBUTE_ID,   EPCGKernelAttributeType::Float3, TEXT("$Position")),
		FPCGKernelAttributeDesc(POINT_ROTATION_ATTRIBUTE_ID,   EPCGKernelAttributeType::Quat,   TEXT("$Rotation")),
		FPCGKernelAttributeDesc(POINT_SCALE_ATTRIBUTE_ID,      EPCGKernelAttributeType::Float3, TEXT("$Scale")),
		FPCGKernelAttributeDesc(POINT_BOUNDS_MIN_ATTRIBUTE_ID, EPCGKernelAttributeType::Float3, TEXT("$BoundsMin")),
		FPCGKernelAttributeDesc(POINT_BOUNDS_MAX_ATTRIBUTE_ID, EPCGKernelAttributeType::Float3, TEXT("$BoundsMax")),
		FPCGKernelAttributeDesc(POINT_COLOR_ATTRIBUTE_ID,      EPCGKernelAttributeType::Float4, TEXT("$Color")),
		FPCGKernelAttributeDesc(POINT_DENSITY_ATTRIBUTE_ID,    EPCGKernelAttributeType::Float,  TEXT("$Density")),
		FPCGKernelAttributeDesc(POINT_SEED_ATTRIBUTE_ID,       EPCGKernelAttributeType::Int,    TEXT("$Seed")),
		FPCGKernelAttributeDesc(POINT_STEEPNESS_ATTRIBUTE_ID,  EPCGKernelAttributeType::Float,  TEXT("$Steepness"))
	};
}

namespace PCGDataForGPUHelpers
{
	EPCGKernelAttributeType GetAttributeTypeFromMetadataType(EPCGMetadataTypes MetadataType)
	{
		switch (MetadataType)
		{
		case EPCGMetadataTypes::Boolean:
			return EPCGKernelAttributeType::Bool;
		case EPCGMetadataTypes::Float:
		case EPCGMetadataTypes::Double:
			return EPCGKernelAttributeType::Float;
		case EPCGMetadataTypes::Integer32:
		case EPCGMetadataTypes::Integer64:
			return EPCGKernelAttributeType::Int;
		case EPCGMetadataTypes::Vector2:
			return EPCGKernelAttributeType::Float2;
		case EPCGMetadataTypes::Vector:
			return EPCGKernelAttributeType::Float3;
		case EPCGMetadataTypes::Rotator:
			return EPCGKernelAttributeType::Rotator;
		case EPCGMetadataTypes::Vector4:
			return EPCGKernelAttributeType::Float4;
		case EPCGMetadataTypes::Quaternion:
			return EPCGKernelAttributeType::Quat;
		case EPCGMetadataTypes::Transform:
			return EPCGKernelAttributeType::Transform;
		case EPCGMetadataTypes::SoftObjectPath: // TODO: This collapses all StringKey types into String attributes, meaning we'll lose the original CPU type when doing readback.
		case EPCGMetadataTypes::SoftClassPath:
		case EPCGMetadataTypes::String:
			return EPCGKernelAttributeType::StringKey;
		case EPCGMetadataTypes::Name:
			return EPCGKernelAttributeType::Name;
		default:
			return EPCGKernelAttributeType::Invalid;
		}
	}

	int GetAttributeTypeStrideBytes(EPCGKernelAttributeType Type)
	{
		switch (Type)
		{
		case EPCGKernelAttributeType::Bool:
		case EPCGKernelAttributeType::Int:
		case EPCGKernelAttributeType::Float:
		case EPCGKernelAttributeType::StringKey:
		case EPCGKernelAttributeType::Name:
			return 4;
		case EPCGKernelAttributeType::Float2:
			return 8;
		case EPCGKernelAttributeType::Float3:
		case EPCGKernelAttributeType::Rotator:
			return 12;
		case EPCGKernelAttributeType::Float4:
		case EPCGKernelAttributeType::Quat:
			return 16;
		case EPCGKernelAttributeType::Transform:
			return 64;
		default:
			checkNoEntry();
			return 0;
		}
	}

	bool PackAttributeHelper(const FPCGMetadataAttributeBase* InAttributeBase, const FPCGKernelAttributeDesc& InAttributeDesc, PCGMetadataEntryKey InEntryKey, const TArray<FString>& InStringTable, TArray<uint32>& OutPackedDataCollection, uint32& InOutAddressUints)
	{
		check(InAttributeBase);

		const PCGMetadataValueKey ValueKey = InAttributeBase->GetValueKey(InEntryKey);
		const int16 TypeId = InAttributeBase->GetTypeId();
		const int StrideBytes = PCGDataForGPUHelpers::GetAttributeTypeStrideBytes(InAttributeDesc.AttributeKey.Type);

		switch (TypeId)
		{
		case PCG::Private::MetadataTypes<bool>::Id:
		{
			const FPCGMetadataAttribute<bool>* Attribute = static_cast<const FPCGMetadataAttribute<bool>*>(InAttributeBase);
			const bool Value = Attribute->GetValue(ValueKey);
			check(StrideBytes == 4);
			OutPackedDataCollection[InOutAddressUints++] = Value;
			break;
		}
		case PCG::Private::MetadataTypes<float>::Id:
		{
			const FPCGMetadataAttribute<float>* Attribute = static_cast<const FPCGMetadataAttribute<float>*>(InAttributeBase);
			const float Value = Attribute->GetValue(ValueKey);
			check(StrideBytes == 4);
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(Value);
			break;
		}
		case PCG::Private::MetadataTypes<double>::Id:
		{
			const FPCGMetadataAttribute<double>* Attribute = static_cast<const FPCGMetadataAttribute<double>*>(InAttributeBase);
			const double Value = Attribute->GetValue(ValueKey);
			check(StrideBytes == 4);
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Value));
			break;
		}
		case PCG::Private::MetadataTypes<int32>::Id:
		{
			const FPCGMetadataAttribute<int32>* Attribute = static_cast<const FPCGMetadataAttribute<int32>*>(InAttributeBase);
			const int32 Value = Attribute->GetValue(ValueKey);
			check(StrideBytes == 4);
			OutPackedDataCollection[InOutAddressUints++] = Value;
			break;
		}
		case PCG::Private::MetadataTypes<int64>::Id:
		{
			const FPCGMetadataAttribute<int64>* Attribute = static_cast<const FPCGMetadataAttribute<int64>*>(InAttributeBase);
			const int64 Value = Attribute->GetValue(ValueKey);
			check(StrideBytes == 4);
			OutPackedDataCollection[InOutAddressUints++] = Value;
			break;
		}
		case PCG::Private::MetadataTypes<FVector2D>::Id:
		{
			const FPCGMetadataAttribute<FVector2D>* Attribute = static_cast<const FPCGMetadataAttribute<FVector2D>*>(InAttributeBase);
			const FVector2D Value = Attribute->GetValue(ValueKey);
			check(StrideBytes == 8);
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Value.X));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Value.Y));
			break;
		}
		case PCG::Private::MetadataTypes<FRotator>::Id:
		{
			const FPCGMetadataAttribute<FRotator>* Attribute = static_cast<const FPCGMetadataAttribute<FRotator>*>(InAttributeBase);
			const FRotator Value = Attribute->GetValue(ValueKey);
			check(StrideBytes == 12);
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Value.Pitch));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Value.Yaw));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Value.Roll));
			break;
		}
		case PCG::Private::MetadataTypes<FVector>::Id:
		{
			const FPCGMetadataAttribute<FVector>* Attribute = static_cast<const FPCGMetadataAttribute<FVector>*>(InAttributeBase);
			const FVector Value = Attribute->GetValue(ValueKey);
			check(StrideBytes == 12);
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Value.X));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Value.Y));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Value.Z));
			break;
		}
		case PCG::Private::MetadataTypes<FVector4>::Id:
		{
			const FPCGMetadataAttribute<FVector4>* Attribute = static_cast<const FPCGMetadataAttribute<FVector4>*>(InAttributeBase);
			const FVector4 Value = Attribute->GetValue(ValueKey);
			check(StrideBytes == 16);
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Value.X));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Value.Y));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Value.Z));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Value.W));
			break;
		}
		case PCG::Private::MetadataTypes<FQuat>::Id:
		{
			const FPCGMetadataAttribute<FQuat>* Attribute = static_cast<const FPCGMetadataAttribute<FQuat>*>(InAttributeBase);
			const FQuat Value = Attribute->GetValue(ValueKey);
			check(StrideBytes == 16);
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Value.X));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Value.Y));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Value.Z));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Value.W));
			break;
		}
		case PCG::Private::MetadataTypes<FTransform>::Id:
		{
			const FPCGMetadataAttribute<FTransform>* Attribute = static_cast<const FPCGMetadataAttribute<FTransform>*>(InAttributeBase);
			const FTransform Transform = Attribute->GetValue(ValueKey);

			const bool bIsRotationNormalized = Transform.IsRotationNormalized();
			if (!bIsRotationNormalized)
			{
				UE_LOG(LogPCG, Error, TEXT("Tried to pack attribute '%s' of type Transform for GPU data collection, but the transform's rotation is not normalized (%f, %f, %f, %f). Using identity instead."),
					*InAttributeBase->Name.ToString(), Transform.GetRotation().X, Transform.GetRotation().Y, Transform.GetRotation().Z, Transform.GetRotation().W);
			}

			// Note: ToMatrixWithScale() crashes if the transform is not normalized.
			const FMatrix Matrix = bIsRotationNormalized ? Transform.ToMatrixWithScale() : FMatrix::Identity;

			check(StrideBytes == 64);
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Matrix.M[0][0]));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Matrix.M[0][1]));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Matrix.M[0][2]));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Matrix.M[0][3]));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Matrix.M[1][0]));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Matrix.M[1][1]));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Matrix.M[1][2]));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Matrix.M[1][3]));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Matrix.M[2][0]));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Matrix.M[2][1]));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Matrix.M[2][2]));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Matrix.M[2][3]));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Matrix.M[3][0]));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Matrix.M[3][1]));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Matrix.M[3][2]));
			OutPackedDataCollection[InOutAddressUints++] = FMath::AsUInt(static_cast<float>(Matrix.M[3][3]));

			break;
		}
		case PCG::Private::MetadataTypes<FString>::Id:
		{
			// String stored as an integer for reading/writing in kernel, and accompanying string table in data description.
			const FPCGMetadataAttribute<FString>* Attribute = static_cast<const FPCGMetadataAttribute<FString>*>(InAttributeBase);
			const int32 Value = InStringTable.IndexOfByKey(Attribute->GetValue(ValueKey));
			check(StrideBytes == 4);
			OutPackedDataCollection[InOutAddressUints++] = Value;
			break;
		}
		case PCG::Private::MetadataTypes<FSoftObjectPath>::Id:
		{
			// SOP path string stored as an integer for reading/writing in kernel, and accompanying string table in data description.
			const FPCGMetadataAttribute<FSoftObjectPath>* Attribute = static_cast<const FPCGMetadataAttribute<FSoftObjectPath>*>(InAttributeBase);
			const int32 Value = InStringTable.IndexOfByKey(Attribute->GetValue(ValueKey).ToString());
			check(StrideBytes == 4);
			OutPackedDataCollection[InOutAddressUints++] = Value;
			break;
		}
		case PCG::Private::MetadataTypes<FSoftClassPath>::Id:
		{
			// SCP path string stored as an integer for reading/writing in kernel, and accompanying string table in data description.
			const FPCGMetadataAttribute<FSoftClassPath>* Attribute = static_cast<const FPCGMetadataAttribute<FSoftClassPath>*>(InAttributeBase);
			const int32 Value = InStringTable.IndexOfByKey(Attribute->GetValue(ValueKey).ToString());
			check(StrideBytes == 4);
			OutPackedDataCollection[InOutAddressUints++] = Value;
			break;
		}
		case PCG::Private::MetadataTypes<FName>::Id:
        {
        	// FNames are currently stored in string table so use same logic as string.
        	const FPCGMetadataAttribute<FName>* Attribute = static_cast<const FPCGMetadataAttribute<FName>*>(InAttributeBase);
			const int32 Value = InStringTable.IndexOfByKey(Attribute->GetValue(ValueKey).ToString());
			check(StrideBytes == 4);
			OutPackedDataCollection[InOutAddressUints++] = Value;
        	break;
        }
		default:
			return false;
		}

		return true;
	}

	FPCGMetadataAttributeBase* CreateAttributeFromAttributeDesc(UPCGMetadata* Metadata, const FPCGKernelAttributeDesc& AttributeDesc)
	{
		check(Metadata);
		FPCGMetadataDomain* MetadataDomain = Metadata->GetMetadataDomain(AttributeDesc.AttributeKey.Identifier.MetadataDomain);
		if (!MetadataDomain)
		{
			return nullptr;
		}
		
		switch (AttributeDesc.AttributeKey.Type)
		{
		case EPCGKernelAttributeType::Bool:
		{
			return MetadataDomain->FindOrCreateAttribute<bool>(AttributeDesc.AttributeKey.Identifier.Name);
		}
		case EPCGKernelAttributeType::Int:
		{
			return MetadataDomain->FindOrCreateAttribute<int>(AttributeDesc.AttributeKey.Identifier.Name);
		}
		case EPCGKernelAttributeType::Float:
		{
			return MetadataDomain->FindOrCreateAttribute<float>(AttributeDesc.AttributeKey.Identifier.Name);
		}
		case EPCGKernelAttributeType::Float2:
		{
			return MetadataDomain->FindOrCreateAttribute<FVector2D>(AttributeDesc.AttributeKey.Identifier.Name);
		}
		case EPCGKernelAttributeType::Float3:
		{
			return MetadataDomain->FindOrCreateAttribute<FVector>(AttributeDesc.AttributeKey.Identifier.Name);
		}
		case EPCGKernelAttributeType::Float4:
		{
			return MetadataDomain->FindOrCreateAttribute<FVector4>(AttributeDesc.AttributeKey.Identifier.Name);
		}
		case EPCGKernelAttributeType::Rotator:
		{
			return MetadataDomain->FindOrCreateAttribute<FRotator>(AttributeDesc.AttributeKey.Identifier.Name);
		}
		case EPCGKernelAttributeType::Quat:
		{
			return MetadataDomain->FindOrCreateAttribute<FQuat>(AttributeDesc.AttributeKey.Identifier.Name);
		}
		case EPCGKernelAttributeType::Transform:
		{
			return MetadataDomain->FindOrCreateAttribute<FTransform>(AttributeDesc.AttributeKey.Identifier.Name);
		}
		case EPCGKernelAttributeType::StringKey:
		{
			return MetadataDomain->FindOrCreateAttribute<FString>(AttributeDesc.AttributeKey.Identifier.Name);
		}
		case EPCGKernelAttributeType::Name:
		{
			return MetadataDomain->FindOrCreateAttribute<FName>(AttributeDesc.AttributeKey.Identifier.Name);
		}
		default:
			return nullptr;
		}
	}

	bool UnpackAttributeHelper(FPCGContext* InContext, const void* InPackedData, const FPCGKernelAttributeDesc& InAttributeDesc, const TArray<FString>& InStringTable, uint32 InAddressUints, uint32 InNumElements, UPCGData* OutData)
	{
		check(InPackedData);
		check(OutData);

		const float* DataAsFloat = static_cast<const float*>(InPackedData);
		const int32* DataAsInt = static_cast<const int32*>(InPackedData);

		FPCGAttributePropertyOutputSelector Selector = FPCGAttributePropertySelector::CreateAttributeSelector<FPCGAttributePropertyOutputSelector>(InAttributeDesc.AttributeKey.Identifier.Name);
		OutData->SetDomainFromDomainID(InAttributeDesc.AttributeKey.Identifier.MetadataDomain, Selector);

		switch (InAttributeDesc.AttributeKey.Type)
		{
		case EPCGKernelAttributeType::Bool:
		{
			TArray<bool> Values;
			Values.SetNumUninitialized(InNumElements);

			for (uint32 ElementIndex = 0; ElementIndex < InNumElements; ++ElementIndex)
			{
				const uint32 PackedElementIndex = InAddressUints + ElementIndex;
				Values[ElementIndex] = static_cast<bool>(DataAsFloat[PackedElementIndex]);
			}

			ensure(PCGAttributeAccessorHelpers::WriteAllValues<bool>(OutData, Selector, Values, /*SourceSelector=*/nullptr, InContext));
			break;
		}
		case EPCGKernelAttributeType::Int:
		{
			TArray<int> Values;
			Values.SetNumUninitialized(InNumElements);

			for (uint32 ElementIndex = 0; ElementIndex < InNumElements; ++ElementIndex)
			{
				const uint32 PackedElementIndex = InAddressUints + ElementIndex;
				Values[ElementIndex] = DataAsInt[PackedElementIndex];
			}

			ensure(PCGAttributeAccessorHelpers::WriteAllValues<int>(OutData, Selector, Values, /*SourceSelector=*/nullptr, InContext));
			break;
		}
		case EPCGKernelAttributeType::Float:
		{
			TArray<float> Values;
			Values.SetNumUninitialized(InNumElements);

			for (uint32 ElementIndex = 0; ElementIndex < InNumElements; ++ElementIndex)
			{
				const uint32 PackedElementIndex = InAddressUints + ElementIndex;
				Values[ElementIndex] = DataAsFloat[PackedElementIndex];
			}

			ensure(PCGAttributeAccessorHelpers::WriteAllValues<float>(OutData, Selector, Values, /*SourceSelector=*/nullptr, InContext));
			break;
		}
		case EPCGKernelAttributeType::Float2:
		{
			TArray<FVector2D> Values;
			Values.SetNumUninitialized(InNumElements);

			for (uint32 ElementIndex = 0; ElementIndex < InNumElements; ++ElementIndex)
			{
				const uint32 PackedElementIndex = InAddressUints + ElementIndex * 2;
				Values[ElementIndex].X = DataAsFloat[PackedElementIndex + 0];
				Values[ElementIndex].Y = DataAsFloat[PackedElementIndex + 1];
			}

			ensure(PCGAttributeAccessorHelpers::WriteAllValues<FVector2D>(OutData, Selector, Values, /*SourceSelector=*/nullptr, InContext));
			break;
		}
		case EPCGKernelAttributeType::Float3:
		{
			TArray<FVector> Values;
			Values.SetNumUninitialized(InNumElements);

			for (uint32 ElementIndex = 0; ElementIndex < InNumElements; ++ElementIndex)
			{
				const uint32 PackedElementIndex = InAddressUints + ElementIndex * 3;
				Values[ElementIndex].X = DataAsFloat[PackedElementIndex + 0];
				Values[ElementIndex].Y = DataAsFloat[PackedElementIndex + 1];
				Values[ElementIndex].Z = DataAsFloat[PackedElementIndex + 2];
			}

			ensure(PCGAttributeAccessorHelpers::WriteAllValues<FVector>(OutData, Selector, Values, /*SourceSelector=*/nullptr, InContext));
			break;
		}
		case EPCGKernelAttributeType::Float4:
		{
			TArray<FVector4> Values;
			Values.SetNumUninitialized(InNumElements);

			for (uint32 ElementIndex = 0; ElementIndex < InNumElements; ++ElementIndex)
			{
				const uint32 PackedElementIndex = InAddressUints + ElementIndex * 4;
				Values[ElementIndex].X = DataAsFloat[PackedElementIndex + 0];
				Values[ElementIndex].Y = DataAsFloat[PackedElementIndex + 1];
				Values[ElementIndex].Z = DataAsFloat[PackedElementIndex + 2];
				Values[ElementIndex].W = DataAsFloat[PackedElementIndex + 3];
			}

			ensure(PCGAttributeAccessorHelpers::WriteAllValues<FVector4>(OutData, Selector, Values, /*SourceSelector=*/nullptr, InContext));
			break;
		}
		case EPCGKernelAttributeType::Rotator:
		{
			TArray<FRotator> Values;
			Values.SetNumUninitialized(InNumElements);

			for (uint32 ElementIndex = 0; ElementIndex < InNumElements; ++ElementIndex)
			{
				const uint32 PackedElementIndex = InAddressUints + ElementIndex * 3;
				Values[ElementIndex].Pitch = DataAsFloat[PackedElementIndex + 0];
				Values[ElementIndex].Yaw = DataAsFloat[PackedElementIndex + 1];
				Values[ElementIndex].Roll = DataAsFloat[PackedElementIndex + 2];
			}

			ensure(PCGAttributeAccessorHelpers::WriteAllValues<FRotator>(OutData, Selector, Values, /*SourceSelector=*/nullptr, InContext));
			break;
		}
		case EPCGKernelAttributeType::Quat:
		{
			TArray<FQuat> Values;
			Values.SetNumUninitialized(InNumElements);

			for (uint32 ElementIndex = 0; ElementIndex < InNumElements; ++ElementIndex)
			{
				const uint32 PackedElementIndex = InAddressUints + ElementIndex * 4;
				Values[ElementIndex].X = DataAsFloat[PackedElementIndex + 0];
				Values[ElementIndex].Y = DataAsFloat[PackedElementIndex + 1];
				Values[ElementIndex].Z = DataAsFloat[PackedElementIndex + 2];
				Values[ElementIndex].W = DataAsFloat[PackedElementIndex + 3];
			}

			ensure(PCGAttributeAccessorHelpers::WriteAllValues<FQuat>(OutData, Selector, Values, /*SourceSelector=*/nullptr, InContext));
			break;
		}
		case EPCGKernelAttributeType::Transform:
		{
			TArray<FTransform> Values;
			Values.SetNumUninitialized(InNumElements);

			FMatrix Matrix;

			for (uint32 ElementIndex = 0; ElementIndex < InNumElements; ++ElementIndex)
			{
				const uint32 PackedElementIndex = InAddressUints + ElementIndex * 16;

				Matrix.M[0][0] = DataAsFloat[PackedElementIndex + 0];
				Matrix.M[0][1] = DataAsFloat[PackedElementIndex + 1];
				Matrix.M[0][2] = DataAsFloat[PackedElementIndex + 2];
				Matrix.M[0][3] = DataAsFloat[PackedElementIndex + 3];
				Matrix.M[1][0] = DataAsFloat[PackedElementIndex + 4];
				Matrix.M[1][1] = DataAsFloat[PackedElementIndex + 5];
				Matrix.M[1][2] = DataAsFloat[PackedElementIndex + 6];
				Matrix.M[1][3] = DataAsFloat[PackedElementIndex + 7];
				Matrix.M[2][0] = DataAsFloat[PackedElementIndex + 8];
				Matrix.M[2][1] = DataAsFloat[PackedElementIndex + 9];
				Matrix.M[2][2] = DataAsFloat[PackedElementIndex + 10];
				Matrix.M[2][3] = DataAsFloat[PackedElementIndex + 11];
				Matrix.M[3][0] = DataAsFloat[PackedElementIndex + 12];
				Matrix.M[3][1] = DataAsFloat[PackedElementIndex + 13];
				Matrix.M[3][2] = DataAsFloat[PackedElementIndex + 14];
				Matrix.M[3][3] = DataAsFloat[PackedElementIndex + 15];

				new (&Values[ElementIndex]) FTransform(Matrix);
			}

			ensure(PCGAttributeAccessorHelpers::WriteAllValues<FTransform>(OutData, Selector, Values, /*SourceSelector=*/nullptr, InContext));
			break;
		}
		case EPCGKernelAttributeType::StringKey:
		{
			check(!InStringTable.IsEmpty());

			TArray<FString> Values;
			Values.Reserve(InNumElements);

			for (uint32 ElementIndex = 0; ElementIndex < InNumElements; ++ElementIndex)
			{
				const uint32 PackedElementIndex = InAddressUints + ElementIndex;
				const int32 StringKey = InStringTable.IsValidIndex(DataAsInt[PackedElementIndex]) ? DataAsInt[PackedElementIndex] : 0;
				Values.Add(InStringTable[StringKey]);
			}

			ensure(PCGAttributeAccessorHelpers::WriteAllValues<FString>(OutData, Selector, Values, /*SourceSelector=*/nullptr, InContext));
			break;
		}
		case EPCGKernelAttributeType::Name:
		{
			check(!InStringTable.IsEmpty());

			TArray<FName> Values;
			Values.SetNumUninitialized(InNumElements);

			for (uint32 ElementIndex = 0; ElementIndex < InNumElements; ++ElementIndex)
			{
				const uint32 PackedElementIndex = InAddressUints + ElementIndex;

				// FNames currently stored in string table.
				const int32 StringKey = InStringTable.IsValidIndex(DataAsInt[PackedElementIndex]) ? DataAsInt[PackedElementIndex] : 0;
				Values[ElementIndex] = *InStringTable[StringKey];
			}

			ensure(PCGAttributeAccessorHelpers::WriteAllValues<FName>(OutData, Selector, Values, /*SourceSelector=*/nullptr, InContext));
			break;
		}
		default:
			return false;
		}
		return true;
	}

	void ComputeCustomFloatPacking(
		FPCGContext* InContext,
		const UPCGSettings* InSettings,
		TArray<FName>& InAttributeNames,
		const UPCGDataBinding* InBinding,
		const FPCGDataCollectionDesc* InDataCollectionDescription,
		uint32& OutCustomFloatCount,
		TArray<FUint32Vector4>& OutAttributeIdOffsetStrides)
	{
		check(InBinding);
		check(InBinding->Graph);
		check(InDataCollectionDescription);

		uint32 OffsetFloats = 0;

		for (FName AttributeName : InAttributeNames)
		{
			// We need to do a lookup here as the user does not provide the full attribute description, only the attribute name. So we see what we can
			// find in the input data similar to the CPU SM Spawner.
			FPCGKernelAttributeDesc AttributeDesc;
			bool bConflictingTypesInData = false;
			InDataCollectionDescription->GetAttributeDesc(AttributeName, AttributeDesc, bConflictingTypesInData);

			if (bConflictingTypesInData)
			{
				// TODO: in future we could partition the execution - run it once per attribute type in input data.
				PCG_KERNEL_VALIDATION_ERR(InContext, InSettings,
					FText::Format(LOCTEXT("AttributePackingTypeConflict", "Attribute '{0}' encountered with multiple different types in input data, custom float packing failed."), FText::FromName(AttributeName)));

				OutAttributeIdOffsetStrides.Empty();
				OutCustomFloatCount = 0;

				return;
			}

			const uint32 StrideFloats = GetAttributeTypeStrideBytes(AttributeDesc.AttributeKey.Type) / sizeof(float);

			OutAttributeIdOffsetStrides.Emplace(static_cast<uint32>(AttributeDesc.AttributeId), OffsetFloats, StrideFloats, /*Unused*/0);

			OffsetFloats += StrideFloats;
		}

		OutCustomFloatCount = OffsetFloats;
	}
}

FPCGKernelAttributeKey::FPCGKernelAttributeKey(const FPCGAttributePropertySelector& InSelector, EPCGKernelAttributeType InType)
	: Type(InType)
{
	SetSelector(InSelector);
}

void FPCGKernelAttributeKey::SetSelector(const FPCGAttributePropertySelector& InSelector)
{
	Name.ImportFromOtherSelector(InSelector);
	UpdateIdentifierFromSelector();
}

bool FPCGKernelAttributeKey::UpdateIdentifierFromSelector()
{
	// @todo_pcg: When we support more domains, we'll need to have a way to convert it to a proper identifier.
	// For now, only support default metadata domain, and mark the identifier invalid if it is anything else.
	if (Name.GetSelection() != EPCGAttributePropertySelection::Attribute)
	{
		UE_LOG(LogPCG, Error, TEXT("While updating FPCGKernelAttributeKey with %s, the selector was not targeting an attribute. Discarded."), *Name.ToString());
		Identifier.MetadataDomain = PCGMetadataDomainID::Invalid;
		return true;
	}

	const FName DomainName = Name.GetDomainName();
	if (DomainName != NAME_None)// && DomainName != PCGDataConstants::DataDomainName)
	{
		//UE_LOG(LogPCG, Error, TEXT("While updating FPCGKernelAttributeKey with %s, the selector was targeting a domain that is not the @Data domain which is unsupported at the moment. Discarded."), *Name.ToString());
		UE_LOG(LogPCG, Error, TEXT("While updating FPCGKernelAttributeKey with %s, the selector was targeting a domain which is unsupported at the moment. Discarded."), *Name.ToString());
		Identifier.MetadataDomain = PCGMetadataDomainID::Invalid;
		return true;
	}

	FPCGAttributeIdentifier NewIdentifier{Name.GetAttributeName(), DomainName == PCGDataConstants::DataDomainName ? PCGMetadataDomainID::Data : PCGMetadataDomainID::Default};
	bool bHasChanged = NewIdentifier != Identifier;
	if (bHasChanged)
	{
		Identifier = std::move(NewIdentifier);
	}
	
	return bHasChanged;
}

bool FPCGKernelAttributeKey::IsValid() const
{
	return Identifier.MetadataDomain.IsValid();
}

bool FPCGKernelAttributeKey::operator==(const FPCGKernelAttributeKey& Other) const
{
	return Type == Other.Type && Identifier == Other.Identifier;
}

uint32 GetTypeHash(const FPCGKernelAttributeKey& In)
{
	return HashCombine(GetTypeHash(In.Type), GetTypeHash(In.Identifier));
}

int32 FPCGKernelAttributeTable::GetAttributeId(const FPCGKernelAttributeKey& InAttribute) const
{
	const int TableIndex = AttributeTable.IndexOfByKey(InAttribute);
	if (TableIndex >= 0)
	{
		return PCGComputeHelpers::GetAttributeIdFromMetadataAttributeIndex(TableIndex);
	}
	else
	{
		// Attribute with the given name and type is not present. If this is unexpected, check this attribute table for attributes
		// in case the supplied name or type is wrong, and ensure any attribute created by a kernel is declared using GetKernelAttributeKeys().
		return INDEX_NONE;
	}
}

int32 FPCGKernelAttributeTable::GetAttributeId(FPCGAttributeIdentifier InIdentifier, EPCGKernelAttributeType InType) const
{
	return GetAttributeId(FPCGKernelAttributeKey(InIdentifier, InType));
}

int32 FPCGKernelAttributeTable::AddAttribute(const FPCGKernelAttributeKey& Key)
{
	int32 Index = AttributeTable.Find(Key);

	if (Index == INDEX_NONE && AttributeTable.Num() < PCGComputeConstants::MAX_NUM_CUSTOM_ATTRS)
	{
		Index = AttributeTable.Num();
		AttributeTable.Add(Key);
	}

	return Index;
}

int32 FPCGKernelAttributeTable::AddAttribute(FPCGAttributeIdentifier InIdentifier, EPCGKernelAttributeType InType)
{
	return AddAttribute(FPCGKernelAttributeKey(InIdentifier, InType));
}

#if PCG_KERNEL_LOGGING_ENABLED
void FPCGKernelAttributeTable::DebugLog() const
{
	const UEnum* PCGKernelAttributeTypeEnum = StaticEnum<EPCGKernelAttributeType>();

	for (const FPCGKernelAttributeKey& Attribute : AttributeTable)
	{
		UE_LOG(LogPCG, Display, TEXT("\tName: %s\t\tID: %d\t\tType: %s"),
			*Attribute.Identifier.ToString(),
			GetAttributeId(Attribute),
			*PCGKernelAttributeTypeEnum->GetDisplayNameTextByValue(static_cast<int64>(Attribute.Type)).ToString());
	}
}
#endif

void FPCGKernelAttributeDesc::AddUniqueStringKeys(const TArray<int32>& InOtherStringKeys)
{
	for (int32 OtherStringKey : InOtherStringKeys)
	{
		UniqueStringKeys.AddUnique(OtherStringKey);
	}
}

void FPCGKernelAttributeDesc::SetStringKeys(const TArrayView<int32>& InStringKeys)
{
	UniqueStringKeys = InStringKeys;
}

bool FPCGKernelAttributeDesc::operator==(const FPCGKernelAttributeDesc& Other) const
{
	return AttributeId == Other.AttributeId && AttributeKey == Other.AttributeKey;
}

FPCGDataDesc::FPCGDataDesc(EPCGDataType InType)
	: FPCGDataDesc(InType, 0, FIntPoint::ZeroValue)
{
}

FPCGDataDesc::FPCGDataDesc(EPCGDataType InType, int InElementCount)
	: FPCGDataDesc(InType, InElementCount, FIntPoint::ZeroValue)
{
}

FPCGDataDesc::FPCGDataDesc(EPCGDataType InType, int InElementCount, FIntPoint InElementCount2D)
	: Type(InType)
	, ElementCount(InElementCount)
	, ElementCount2D(InElementCount2D)
{
	InitializeAttributeDescs(/*InData=*/nullptr, /*InBinding*/nullptr);
}

FPCGDataDesc::FPCGDataDesc(const FPCGTaggedData& InTaggedData, const UPCGDataBinding* InBinding)
{
	check(InTaggedData.Data);

	Type = InTaggedData.Data->GetDataType();
	ElementCount = PCGComputeHelpers::GetElementCount(InTaggedData.Data);
	ElementCount2D = PCGComputeHelpers::GetElementCount2D(InTaggedData.Data);

	TagStringKeys.Reserve(InTaggedData.Tags.Num());

	for (const FString& Tag : InTaggedData.Tags)
	{
		const int Index = InBinding->GetStringTable().IndexOfByKey(Tag);
		if (Index != INDEX_NONE)
		{
			TagStringKeys.Add(Index);
		}
	}

	InitializeAttributeDescs(InTaggedData.Data, InBinding);
}

uint64 FPCGDataDesc::ComputePackedSize() const
{
	check(PCGComputeHelpers::IsTypeAllowedInDataCollection(Type));

	uint64 DataSizeBytes = DATA_HEADER_SIZE_BYTES;

	for (const FPCGKernelAttributeDesc& AttributeDesc : AttributeDescs)
	{
		const uint64 NumValues = static_cast<uint64>(GetElementCountForAttribute(AttributeDesc));
		DataSizeBytes += static_cast<uint64>(PCGDataForGPUHelpers::GetAttributeTypeStrideBytes(AttributeDesc.AttributeKey.Type)) * NumValues;
	}

	return DataSizeBytes;
}

bool FPCGDataDesc::HasElementsMetadataDomainAttributes() const
{
	return AttributeDescs.ContainsByPredicate([](const FPCGKernelAttributeDesc& AttributeDesc)
	{
		return AttributeDesc.AttributeId >= NUM_RESERVED_ATTRS && AttributeDesc.AttributeKey.Identifier.MetadataDomain != PCGMetadataDomainID::Data;
	});
}

bool FPCGDataDesc::ContainsAttribute(FPCGAttributeIdentifier InAttributeIdentifier) const
{
	return AttributeDescs.ContainsByPredicate([InAttributeIdentifier](const FPCGKernelAttributeDesc& AttributeDesc)
	{
		return AttributeDesc.AttributeKey.Identifier == InAttributeIdentifier;
	});
}

bool FPCGDataDesc::ContainsAttribute(FPCGAttributeIdentifier InAttributeIdentifier, EPCGKernelAttributeType InAttributeType) const
{
	return AttributeDescs.ContainsByPredicate([InAttributeKey = FPCGKernelAttributeKey(InAttributeIdentifier, InAttributeType)](const FPCGKernelAttributeDesc& AttributeDesc)
	{
		return AttributeDesc.AttributeKey == InAttributeKey;
	});
}

void FPCGDataDesc::AddAttribute(FPCGKernelAttributeKey InAttribute, const UPCGDataBinding* InBinding, const TArray<int32>* InOptionalUniqueStringKeys)
{
	const int32 AttributeId = InBinding->GetAttributeId(InAttribute);
	if (AttributeId == INDEX_NONE)
	{
		return;
	}

	// Remove existing attributes if they collide with the new attribute (that is, if they have the same Identifier). The new attribute will stomp them,
	// which is consistent with the general behavior on CPU. The node is authoritative on its attributes types.
	// @todo_pcg: We should probably encapsulate the AttributeDescs, and they can only be added through AddAttribute() to ensure consistent behavior (w.r.t. stomping existing attributes).
	for (int AttributeIndex = AttributeDescs.Num() - 1; AttributeIndex >= 0; --AttributeIndex)
	{
		FPCGKernelAttributeDesc& ExistingAttributeDesc = AttributeDescs[AttributeIndex];

		if (ExistingAttributeDesc.AttributeKey.Identifier == InAttribute.Identifier)
		{
			AttributeDescs.RemoveAtSwap(AttributeIndex);
		}
	}

	AttributeDescs.Emplace(AttributeId, InAttribute.Type, InAttribute.Identifier, InOptionalUniqueStringKeys);
}

int32 FPCGDataDesc::GetElementCountForAttribute(const FPCGKernelAttributeDesc& AttributeDesc) const
{
	// Data domain only have a single element.
	return AttributeDesc.AttributeKey.Identifier.MetadataDomain == PCGMetadataDomainID::Data ? 1 : ElementCount;
}

void FPCGDataDesc::InitializeAttributeDescs(const UPCGData* InData, const UPCGDataBinding* InBinding)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDataDesc::InitializeAttributeDescs);

	if (Type == EPCGDataType::Point)
	{
		AttributeDescs.Append(PCGDataForGPUConstants::PointPropertyDescs, NUM_POINT_PROPERTIES);
	}
	else { /* TODO: More types! */ }

	const UPCGMetadata* Metadata = (InData && PCGComputeHelpers::ShouldImportAttributesFromData(InData)) ? InData->ConstMetadata() : nullptr;

	if (InBinding && Metadata)
	{
		const FPCGMetadataDomainID MetadataDefaultDomainID = Metadata->GetConstDefaultMetadataDomain()->GetDomainID();
		
		TArray<FPCGAttributeIdentifier> AttributeIdentifiers;
		TArray<EPCGMetadataTypes> AttributeTypes;
		Metadata->GetAllAttributes(AttributeIdentifiers, AttributeTypes);

		// Cache the keys to a given domain, so we don't recreate them
		TMap<FPCGMetadataDomainID, TUniquePtr<const IPCGAttributeAccessorKeys>> AllKeys;

		for (int CustomAttributeIndex = 0; CustomAttributeIndex < AttributeIdentifiers.Num(); ++CustomAttributeIndex)
		{
			// @todo_pcg: Attributes on other domains than the default are ignored at the moment, until we have a better way of representing
			// different domains in the GPU header.
			// It means those are lost.
			FPCGAttributeIdentifier AttributeIdentifier = AttributeIdentifiers[CustomAttributeIndex];
			if (AttributeIdentifier.MetadataDomain != PCGMetadataDomainID::Default && AttributeIdentifier.MetadataDomain != MetadataDefaultDomainID)
			{
				continue;
			}

			// If the domain is the default domain, force it to the default identifier.
			if (AttributeIdentifier.MetadataDomain == MetadataDefaultDomainID)
			{
				AttributeIdentifier.MetadataDomain = PCGMetadataDomainID::Default;
			}
			
			const EPCGKernelAttributeType AttributeType = PCGDataForGPUHelpers::GetAttributeTypeFromMetadataType(AttributeTypes[CustomAttributeIndex]);

			if (AttributeType == EPCGKernelAttributeType::Invalid)
			{
				const UEnum* EnumClass = StaticEnum<EPCGMetadataTypes>();
				check(EnumClass);

				FPCGAttributePropertySelector Selector = FPCGAttributePropertySelector::CreateAttributeSelector(AttributeIdentifier.Name);
				InData->SetDomainFromDomainID(AttributeIdentifier.MetadataDomain, Selector);

				UE_LOG(LogPCG, Warning, TEXT("Skipping attribute '%s'. '%s' type attributes are not supported on GPU."),
					*Selector.ToString(),
					*EnumClass->GetNameStringByValue(static_cast<int64>(AttributeTypes[CustomAttributeIndex])));

				continue;
			}

			// Ignore excess attributes.
			if (CustomAttributeIndex >= MAX_NUM_CUSTOM_ATTRS)
			{
				// TODO: Would be nice to include the pin label for debug purposes
				UE_LOG(LogPCG, Warning, TEXT("Attempted to exceed max number of custom attributes (%d). Additional attributes will be ignored."), MAX_NUM_CUSTOM_ATTRS);
				break;
			}

			TArray<int32> UniqueStringKeys;

			if (AttributeType == EPCGKernelAttributeType::StringKey || AttributeType == EPCGKernelAttributeType::Name)
			{
				const FPCGMetadataDomain* MetadataDomain = Metadata->GetConstMetadataDomain(AttributeIdentifier.MetadataDomain);
				const FPCGMetadataAttributeBase* AttributeBase = MetadataDomain->GetConstAttribute(AttributeIdentifier.Name);
				check(MetadataDomain && AttributeBase);

				const TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(AttributeBase, MetadataDomain);
				TUniquePtr<const IPCGAttributeAccessorKeys>& Keys = AllKeys.FindOrAdd(AttributeIdentifier.MetadataDomain);
				if (!Keys.IsValid())
				{
					FPCGAttributePropertySelector Selector = FPCGAttributePropertySelector::CreateAttributeSelector(AttributeIdentifier.Name);
					InData->SetDomainFromDomainID(AttributeIdentifier.MetadataDomain, Selector);
					Keys = PCGAttributeAccessorHelpers::CreateConstKeys(InData, Selector);
				}

				check(Accessor && Keys);

				PCGMetadataElementCommon::ApplyOnAccessor<FString>(*Keys, *Accessor, [&UniqueStringKeys, &StringTable = InBinding->GetStringTable()](const FString& InValue, int32)
				{
					const int StringTableIndex = StringTable.IndexOfByKey(InValue);
					if (StringTableIndex != INDEX_NONE)
					{
						UniqueStringKeys.AddUnique(StringTableIndex);
					}
				});
			}

			const int32 AttributeId = InBinding->GetAttributeId(AttributeIdentifier, AttributeType);
			ensureMsgf(AttributeId != INDEX_NONE, TEXT("Attribute '%s' type %d was missing from attribute table."), *AttributeIdentifier.Name.ToString(), (int32)AttributeType);

			if (AttributeId != INDEX_NONE)
			{
				AttributeDescs.Emplace(AttributeId, AttributeType, AttributeIdentifier, &UniqueStringKeys);
			}
		}
	}
}

FPCGDataCollectionDesc FPCGDataCollectionDesc::BuildFromDataCollection(
	const FPCGDataCollection& InDataCollection,
	const UPCGDataBinding* InBinding)
{
	FPCGDataCollectionDesc CollectionDesc;

	for (const FPCGTaggedData& Data : InDataCollection.TaggedData)
	{
		if (!Data.Data || !PCGComputeHelpers::IsTypeAllowedAsOutput(Data.Data->GetDataType()))
		{
			continue;
		}

		CollectionDesc.DataDescs.Emplace(Data, InBinding);
	}

	return CollectionDesc;
}

uint32 FPCGDataCollectionDesc::ComputePackedHeaderSizeBytes() const
{
	return PCGComputeConstants::DATA_COLLECTION_HEADER_SIZE_BYTES + PCGComputeConstants::DATA_HEADER_SIZE_BYTES * DataDescs.Num();
}

uint64 FPCGDataCollectionDesc::ComputePackedSizeBytes() const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDataCollectionDesc::ComputePackedSize);

	uint64 TotalCollectionSizeBytes = ComputePackedHeaderSizeBytes();

	for (const FPCGDataDesc& DataDesc : DataDescs)
	{
		TotalCollectionSizeBytes += DataDesc.ComputePackedSize();
	}

	return TotalCollectionSizeBytes;
}

void FPCGDataCollectionDesc::WriteHeader(TArray<uint32>& OutPackedDataCollectionHeader) const
{
	const uint32 HeaderSizeBytes = ComputePackedHeaderSizeBytes();
	const uint32 HeaderSizeUints = HeaderSizeBytes >> 2;

	if (OutPackedDataCollectionHeader.Num() < static_cast<int32>(HeaderSizeUints))
	{
		OutPackedDataCollectionHeader.SetNumUninitialized(HeaderSizeUints);
	}

	// Zero-initialize header portion. We detect absent attributes using 0s.
	for (uint32 Index = 0; Index < HeaderSizeUints; ++Index)
	{
		OutPackedDataCollectionHeader[Index] = 0;
	}

	uint32 WriteAddressUints = 0;

	// Start at end of header
	uint32 DataStartAddressBytes = HeaderSizeBytes;

	// Num data
	OutPackedDataCollectionHeader[WriteAddressUints++] = DataDescs.Num();

	for (int32 DataIndex = 0; DataIndex < DataDescs.Num(); ++DataIndex)
	{
		const FPCGDataDesc& DataDesc = DataDescs[DataIndex];

		// Data i: type ID
		if (DataDesc.Type == EPCGDataType::Param)
		{
			OutPackedDataCollectionHeader[WriteAddressUints++] = PARAM_DATA_TYPE_ID;
		}
		else
		{
			ensure(DataDesc.Type == EPCGDataType::Point);
			OutPackedDataCollectionHeader[WriteAddressUints++] = POINT_DATA_TYPE_ID;
		}

		// Data i: attribute count (including intrinsic point properties)
		OutPackedDataCollectionHeader[WriteAddressUints++] = DataDesc.AttributeDescs.Num();

		// Data i: element count
		OutPackedDataCollectionHeader[WriteAddressUints++] = DataDesc.ElementCount;

		const uint32 DataAttributesHeaderStartAddressBytes = WriteAddressUints << 2;

		for (int32 AttrIndex = 0; AttrIndex < DataDesc.AttributeDescs.Num(); ++AttrIndex)
		{
			const FPCGKernelAttributeDesc& AttributeDesc = DataDesc.AttributeDescs[AttrIndex];
			const int AttributeStride = PCGDataForGPUHelpers::GetAttributeTypeStrideBytes(AttributeDesc.AttributeKey.Type);
			const int AttributeElementsCount = DataDesc.GetElementCountForAttribute(AttributeDesc);

			// Scatter from attributes that are present into header which has slots for all possible attributes.
			WriteAddressUints = (AttributeDesc.AttributeId * ATTRIBUTE_HEADER_SIZE_BYTES + DataAttributesHeaderStartAddressBytes) >> 2;

			// Data i element j: packed ID and stride
			const uint32 AttributeId = AttributeDesc.AttributeId;
			const uint32 PackedIdAndStride = (AttributeId << 8) + AttributeStride;
			OutPackedDataCollectionHeader[WriteAddressUints++] = PackedIdAndStride;
			OutPackedDataCollectionHeader[WriteAddressUints++] = DataStartAddressBytes;

			// Move the DataStartAddress to the beginning of the next attribute by skipping all the bytes of the current attribute.
			DataStartAddressBytes += AttributeElementsCount * AttributeStride;
		}

		// After scattering in attribute headers, fast forward to end of section.
		WriteAddressUints = (PCGComputeConstants::MAX_NUM_ATTRS * PCGComputeConstants::ATTRIBUTE_HEADER_SIZE_BYTES + DataAttributesHeaderStartAddressBytes) >> 2;
	}

	check(WriteAddressUints * 4 == HeaderSizeBytes);
}

static uint32 GetElementDataStartAddressUints(const uint32* InPackedDataCollection, uint32 InDataIndex, uint32 InAttributeId)
{
	uint32 ReadAddressBytes = PCGComputeConstants::DATA_COLLECTION_HEADER_SIZE_BYTES + InDataIndex * PCGComputeConstants::DATA_HEADER_SIZE_BYTES;
	ReadAddressBytes += /*TypeId*/4 + /*Attribute Count*/4 + /*Element Count*/4;

	ReadAddressBytes += InAttributeId * PCGComputeConstants::ATTRIBUTE_HEADER_SIZE_BYTES;
	ReadAddressBytes += /*PackedIdAndStride*/4;

	return InPackedDataCollection[ReadAddressBytes >> 2] >> 2;
}

void FPCGDataCollectionDesc::PackDataCollection(const FPCGDataCollection& InDataCollection, FName InPin, const UPCGDataBinding* InDataBinding, TArray<uint32>& OutPackedDataCollection) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDataCollectionDesc::PackDataCollection);

	const TArray<FPCGTaggedData> InputData = InDataCollection.GetInputsByPin(InPin);

	const uint32 PackedDataCollectionSizeBytes = ComputePackedSizeBytes();
	check(PackedDataCollectionSizeBytes >= 0);

	// Uninitialized is fine, all data is initialized explicitly.
	OutPackedDataCollection.SetNumUninitialized(PackedDataCollectionSizeBytes >> 2);

	// Data addresses are written to the header and will be used during packing below.
	WriteHeader(OutPackedDataCollection);

	for (int32 DataIndex = 0; DataIndex < InputData.Num(); ++DataIndex)
	{
		const FPCGDataDesc& DataDesc = DataDescs[DataIndex];

		if (Cast<UPCGProxyForGPUData>(InputData[DataIndex].Data))
		{
			UE_LOG(LogPCG, Error, TEXT("Attempted to pack a data that is not resident on the CPU. Uploaded data will be uninitialized."));
			continue;
		}

		// No work to do if there are no elements to process.
		if (DataDesc.ElementCount <= 0)
		{
			continue;
		}

		const UPCGMetadata* Metadata = InputData[DataIndex].Data ? InputData[DataIndex].Data->ConstMetadata() : nullptr;
		if (!ensure(Metadata))
		{
			continue;
		}

		if (const UPCGPointData* PointData = Cast<UPCGPointData>(InputData[DataIndex].Data))
		{
			const TArray<FPCGPoint>& Points = PointData->GetPoints();
			if (Points.IsEmpty())
			{
				continue;
			}

			const uint32 NumElements = Points.Num();

			for (const FPCGKernelAttributeDesc& AttributeDesc : DataDesc.AttributeDescs)
			{
				const uint32 AttributeId = AttributeDesc.AttributeId;

				const FPCGMetadataDomain* MetadataDomain = Metadata->GetConstMetadataDomain(AttributeDesc.AttributeKey.Identifier.MetadataDomain);
				const FPCGMetadataAttributeBase* AttributeBase = (AttributeId >= NUM_RESERVED_ATTRS && MetadataDomain) ? MetadataDomain->GetConstAttribute(AttributeDesc.AttributeKey.Identifier.Name) : nullptr;

				uint32 AddressUints = GetElementDataStartAddressUints(OutPackedDataCollection.GetData(), DataIndex, AttributeId);

				if (AttributeId < NUM_RESERVED_ATTRS)
				{
					// Point property.
					switch (AttributeId)
					{
					case POINT_POSITION_ATTRIBUTE_ID:
					{
						for (uint32 ElementIndex = 0; ElementIndex < NumElements; ++ElementIndex)
						{
							const FVector Position = Points[ElementIndex].Transform.GetLocation();
							OutPackedDataCollection[AddressUints++] = FMath::AsUInt(static_cast<float>(Position.X));
							OutPackedDataCollection[AddressUints++] = FMath::AsUInt(static_cast<float>(Position.Y));
							OutPackedDataCollection[AddressUints++] = FMath::AsUInt(static_cast<float>(Position.Z));
						}
						break;
					}
					case POINT_ROTATION_ATTRIBUTE_ID:
					{
						for (uint32 ElementIndex = 0; ElementIndex < NumElements; ++ElementIndex)
						{
							const FQuat Rotation = Points[ElementIndex].Transform.GetRotation();
							OutPackedDataCollection[AddressUints++] = FMath::AsUInt(static_cast<float>(Rotation.X));
							OutPackedDataCollection[AddressUints++] = FMath::AsUInt(static_cast<float>(Rotation.Y));
							OutPackedDataCollection[AddressUints++] = FMath::AsUInt(static_cast<float>(Rotation.Z));
							OutPackedDataCollection[AddressUints++] = FMath::AsUInt(static_cast<float>(Rotation.W));
						}
						break;
					}
					case POINT_SCALE_ATTRIBUTE_ID:
					{
						for (uint32 ElementIndex = 0; ElementIndex < NumElements; ++ElementIndex)
						{
							const FVector Scale = Points[ElementIndex].Transform.GetScale3D();
							OutPackedDataCollection[AddressUints++] = FMath::AsUInt(static_cast<float>(Scale.X));
							OutPackedDataCollection[AddressUints++] = FMath::AsUInt(static_cast<float>(Scale.Y));
							OutPackedDataCollection[AddressUints++] = FMath::AsUInt(static_cast<float>(Scale.Z));
						}
						break;
					}
					case POINT_BOUNDS_MIN_ATTRIBUTE_ID:
					{
						for (uint32 ElementIndex = 0; ElementIndex < NumElements; ++ElementIndex)
						{
							const FVector& BoundsMin = Points[ElementIndex].BoundsMin;
							OutPackedDataCollection[AddressUints++] = FMath::AsUInt(static_cast<float>(BoundsMin.X));
							OutPackedDataCollection[AddressUints++] = FMath::AsUInt(static_cast<float>(BoundsMin.Y));
							OutPackedDataCollection[AddressUints++] = FMath::AsUInt(static_cast<float>(BoundsMin.Z));
						}
						break;
					}
					case POINT_BOUNDS_MAX_ATTRIBUTE_ID:
					{
						for (uint32 ElementIndex = 0; ElementIndex < NumElements; ++ElementIndex)
						{
							const FVector& BoundsMax = Points[ElementIndex].BoundsMax;
							OutPackedDataCollection[AddressUints++] = FMath::AsUInt(static_cast<float>(BoundsMax.X));
							OutPackedDataCollection[AddressUints++] = FMath::AsUInt(static_cast<float>(BoundsMax.Y));
							OutPackedDataCollection[AddressUints++] = FMath::AsUInt(static_cast<float>(BoundsMax.Z));
						}
						break;
					}
					case POINT_COLOR_ATTRIBUTE_ID:
					{
						for (uint32 ElementIndex = 0; ElementIndex < NumElements; ++ElementIndex)
						{
							const FVector4& Color = Points[ElementIndex].Color;
							OutPackedDataCollection[AddressUints++] = FMath::AsUInt(static_cast<float>(Color.X));
							OutPackedDataCollection[AddressUints++] = FMath::AsUInt(static_cast<float>(Color.Y));
							OutPackedDataCollection[AddressUints++] = FMath::AsUInt(static_cast<float>(Color.Z));
							OutPackedDataCollection[AddressUints++] = FMath::AsUInt(static_cast<float>(Color.W));
						}
						break;
					}
					case POINT_DENSITY_ATTRIBUTE_ID:
					{
						for (uint32 ElementIndex = 0; ElementIndex < NumElements; ++ElementIndex)
						{
							const float Density = Points[ElementIndex].Density;
							OutPackedDataCollection[AddressUints++] = FMath::AsUInt(Density);
						}
						break;
					}
					case POINT_SEED_ATTRIBUTE_ID:
					{
						for (uint32 ElementIndex = 0; ElementIndex < NumElements; ++ElementIndex)
						{
							const int Seed = Points[ElementIndex].Seed;
							OutPackedDataCollection[AddressUints++] = Seed;
						}
						break;
					}
					case POINT_STEEPNESS_ATTRIBUTE_ID:
					{
						for (uint32 ElementIndex = 0; ElementIndex < NumElements; ++ElementIndex)
						{
							const float Steepness = Points[ElementIndex].Steepness;
							OutPackedDataCollection[AddressUints++] = FMath::AsUInt(Steepness);
						}
						break;
					}
					default:
						checkNoEntry();
						break;
					}
				}
				else
				{
					// Pack attribute. Validate first element only for perf.
					if (AttributeBase->GetMetadataDomain()->GetDomainID() == PCGMetadataDomainID::Data)
					{
						ensure(PCGDataForGPUHelpers::PackAttributeHelper(AttributeBase, AttributeDesc, /*InEntryKey*/0, InDataBinding->GetStringTable(), OutPackedDataCollection, AddressUints));
					}
					else
					{
						ensure(PCGDataForGPUHelpers::PackAttributeHelper(AttributeBase, AttributeDesc, Points[0].MetadataEntry, InDataBinding->GetStringTable(), OutPackedDataCollection, AddressUints));
						for (uint32 ElementIndex = 1; ElementIndex < NumElements; ++ElementIndex)
						{
							PCGDataForGPUHelpers::PackAttributeHelper(AttributeBase, AttributeDesc, Points[ElementIndex].MetadataEntry, InDataBinding->GetStringTable(), OutPackedDataCollection, AddressUints);
						}
					}
				}
			}
		}
		else if (const UPCGParamData* ParamData = Cast<UPCGParamData>(InputData[DataIndex].Data))
		{
			for (const FPCGKernelAttributeDesc& AttributeDesc : DataDesc.AttributeDescs)
			{
				const FPCGMetadataDomain* MetadataDomain = Metadata->GetConstMetadataDomain(AttributeDesc.AttributeKey.Identifier.MetadataDomain);
				const FPCGMetadataAttributeBase* AttributeBase = MetadataDomain ? MetadataDomain->GetConstAttribute(AttributeDesc.AttributeKey.Identifier.Name) : nullptr;
				if (!AttributeBase)
				{
					continue;
				}

				uint32 AddressUints = GetElementDataStartAddressUints(OutPackedDataCollection.GetData(), DataIndex, AttributeDesc.AttributeId);

				// Pack attribute. Validate first element only for perf.
				ensure(PCGDataForGPUHelpers::PackAttributeHelper(AttributeBase, AttributeDesc, /*InEntryKey*/0, InDataBinding->GetStringTable(), OutPackedDataCollection, AddressUints));
				for (int32 ElementIndex = 1; ElementIndex < DataDesc.GetElementCountForAttribute(AttributeDesc); ++ElementIndex)
				{
					PCGDataForGPUHelpers::PackAttributeHelper(AttributeBase, AttributeDesc, /*InEntryKey*/ElementIndex, InDataBinding->GetStringTable(), OutPackedDataCollection, AddressUints);
				}
			}
		}
		else { /* TODO: Support additional data types. */ }
	}
}

EPCGUnpackDataCollectionResult FPCGDataCollectionDesc::UnpackDataCollection(FPCGContext* InContext, const TArray<uint8>& InPackedData, FName InPin, const TArray<FString>& InStringTable, FPCGDataCollection& OutDataCollection) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDataCollectionDesc::UnpackDataCollection);

	if (InPackedData.IsEmpty())
	{
		ensureMsgf(false, TEXT("Tried to unpack a GPU data collection, but the readback buffer was empty."));
		return EPCGUnpackDataCollectionResult::NoData;
	}

	const void* PackedData = InPackedData.GetData();
	const float* DataAsFloat = static_cast<const float*>(PackedData);
	const uint32* DataAsUint = static_cast<const uint32*>(PackedData);
	const int32* DataAsInt = static_cast<const int32*>(PackedData);

	const uint32 PackedExecutionFlagAndNumData = DataAsUint[0];

	// Most significant bit of NumData is reserved to flag whether or not the kernel executed.
	ensureMsgf(PackedExecutionFlagAndNumData & PCGComputeConstants::KernelExecutedFlag, TEXT("Tried to unpack a GPU data collection, but the compute shader did not execute."));
	const uint32 NumData = PackedExecutionFlagAndNumData & ~PCGComputeConstants::KernelExecutedFlag;

	if (NumData != DataDescs.Num())
	{
		return EPCGUnpackDataCollectionResult::DataMismatch;
	}

	TArray<FPCGTaggedData>& OutData = OutDataCollection.TaggedData;
	TStaticArray<FPCGMetadataAttributeBase*, MAX_NUM_ATTRS> MetadataAttributes;

	for (uint32 DataIndex = 0; DataIndex < NumData; ++DataIndex)
	{
		const uint32 DataHeaderAddress = (DATA_COLLECTION_HEADER_SIZE_BYTES + DATA_HEADER_SIZE_BYTES * DataIndex) / sizeof(uint32);

		const uint32 TypeId =        DataAsUint[DataHeaderAddress + 0];
		const uint32 NumAttributes = DataAsUint[DataHeaderAddress + 1];
		
		uint32 NumElements = DataAsUint[DataHeaderAddress + 2];
		// Use tighter/more refined element count if we have one:
		if (DataDescs[DataIndex].ElementCount >= 0)
		{
			NumElements = FMath::Min(NumElements, static_cast<uint32>(DataDescs[DataIndex].ElementCount));
		}

		const FPCGDataDesc& DataDesc = DataDescs[DataIndex];
		const TArray<FPCGKernelAttributeDesc>& AttributeDescs = DataDesc.AttributeDescs;
		check(NumAttributes == AttributeDescs.Num());
		check(AttributeDescs.Num() <= MAX_NUM_ATTRS);

		if (TypeId == POINT_DATA_TYPE_ID)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UnpackPointDataItem);

			UPCGBasePointData* OutPointData = FPCGContext::NewPointData_AnyThread(InContext);
			UPCGMetadata* Metadata = OutPointData->MutableMetadata();
			OutPointData->SetNumPoints(NumElements, /*bInitializeValues=*/false);

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(InitializeOutput);

				// We only need to add the entry keys if there are actually attributes to unpack.
				if (DataDesc.HasElementsMetadataDomainAttributes())
				{
					TPCGValueRange<int64> MetadataEntryRange = OutPointData->GetMetadataEntryValueRange(/*bAllocate=*/true);
					TArray<PCGMetadataEntryKey*> ParentEntryKeys;
					ParentEntryKeys.Reserve(NumElements);

					for (uint32 ElementIndex = 0; ElementIndex < NumElements; ++ElementIndex)
					{
						PCGMetadataEntryKey& EntryKey = MetadataEntryRange[ElementIndex];
						EntryKey = PCGInvalidEntryKey;
						ParentEntryKeys.Add(&EntryKey);
					}

					Metadata->AddEntriesInPlace(ParentEntryKeys);
				}
				else if(OutPointData->IsA<UPCGPointData>())
				{
					// Special case so that we can use ParallelFor (UPCGPointArrayData doesn't need this)
					TPCGValueRange<int64> MetadataEntryRange = OutPointData->GetMetadataEntryValueRange(/*bAllocate=*/true);
					ParallelFor(NumElements, [&MetadataEntryRange](int32 ElementIndex)
					{
						MetadataEntryRange[ElementIndex] = -1;
					});
				}
				else
				{
					OutPointData->SetMetadataEntry(-1);
				}
			}

			FPCGTaggedData& OutTaggedData = OutData.Emplace_GetRef();
			OutTaggedData.Data = OutPointData;
			OutTaggedData.Pin = InPin;

			for (const int32 TagStringKey : DataDesc.TagStringKeys)
			{
				if (ensure(InStringTable.IsValidIndex(TagStringKey)))
				{
					OutTaggedData.Tags.Add(InStringTable[TagStringKey]);
				}
			}

			for (const FPCGKernelAttributeDesc& AttributeDesc : AttributeDescs)
			{
				if (AttributeDesc.AttributeId >= NUM_RESERVED_ATTRS)
				{
					MetadataAttributes[AttributeDesc.AttributeId] = PCGDataForGPUHelpers::CreateAttributeFromAttributeDesc(Metadata, AttributeDesc);
				}
			}

			// No work to do if there are no elements to process.
			if (NumElements == 0)
			{
				continue;
			}

			// Loop over attributes.
			for (const FPCGKernelAttributeDesc& AttributeDesc : AttributeDescs)
			{
				const uint32 AttributeId = AttributeDesc.AttributeId;
				const uint32 AttributeStrideUints = PCGDataForGPUHelpers::GetAttributeTypeStrideBytes(AttributeDesc.AttributeKey.Type) >> 2;
				const uint32 AddressUints = GetElementDataStartAddressUints(DataAsUint, DataIndex, AttributeId);

				if (AttributeId < NUM_RESERVED_ATTRS)
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(UnpackPointProperty);

					// We tried hoisting this decision to a lambda but it didn't appear to help.
					switch (AttributeId)
					{
					case POINT_POSITION_ATTRIBUTE_ID:
					{
						TPCGValueRange<FTransform> TransformRange = OutPointData->GetTransformValueRange(/*bAlocate=*/true);
						ParallelFor(NumElements, [DataAsFloat, AddressUints, &TransformRange](int32 ElementIndex)
						{
							const FVector Location = FVector(
								DataAsFloat[AddressUints + ElementIndex * 3 + 0],
								DataAsFloat[AddressUints + ElementIndex * 3 + 1],
								DataAsFloat[AddressUints + ElementIndex * 3 + 2]);

							TransformRange[ElementIndex].SetLocation(Location);
						});
						break;
					}
					case POINT_ROTATION_ATTRIBUTE_ID:
					{
						TPCGValueRange<FTransform> TransformRange = OutPointData->GetTransformValueRange(/*bAlocate=*/true);
						ParallelFor(NumElements, [DataAsFloat, AddressUints, &TransformRange](int32 ElementIndex)
						{
							const FQuat Rotation = FQuat(
								DataAsFloat[AddressUints + ElementIndex * 4 + 0],
								DataAsFloat[AddressUints + ElementIndex * 4 + 1],
								DataAsFloat[AddressUints + ElementIndex * 4 + 2],
								DataAsFloat[AddressUints + ElementIndex * 4 + 3]);

							// Normalize here with default tolerance (zero quat will return identity).
							TransformRange[ElementIndex].SetRotation(Rotation.GetNormalized());
						});
						break;
					}
					case POINT_SCALE_ATTRIBUTE_ID:
					{
						TPCGValueRange<FTransform> TransformRange = OutPointData->GetTransformValueRange(/*bAlocate=*/true);
						ParallelFor(NumElements, [DataAsFloat, AddressUints, &TransformRange](int32 ElementIndex)
						{
							const FVector Scale = FVector
							(
								DataAsFloat[AddressUints + ElementIndex * 3 + 0],
								DataAsFloat[AddressUints + ElementIndex * 3 + 1],
								DataAsFloat[AddressUints + ElementIndex * 3 + 2]);

							TransformRange[ElementIndex].SetScale3D(Scale);
						});
						break;
					}
					case POINT_BOUNDS_MIN_ATTRIBUTE_ID:
					{
						TPCGValueRange<FVector> BoundsMinRange = OutPointData->GetBoundsMinValueRange(/*bAllocate=*/true);
						ParallelFor(NumElements, [DataAsFloat, AddressUints, &BoundsMinRange](int32 ElementIndex)
						{
							const FVector BoundsMin = FVector(
								DataAsFloat[AddressUints + ElementIndex * 3 + 0],
								DataAsFloat[AddressUints + ElementIndex * 3 + 1],
								DataAsFloat[AddressUints + ElementIndex * 3 + 2]);

							BoundsMinRange[ElementIndex] = BoundsMin;
						});
						break;
					}
					case POINT_BOUNDS_MAX_ATTRIBUTE_ID:
					{
						TPCGValueRange<FVector> BoundsMaxRange = OutPointData->GetBoundsMaxValueRange(/*bAllocate=*/true);
						ParallelFor(NumElements, [DataAsFloat, AddressUints, &BoundsMaxRange](int32 ElementIndex)
						{
							const FVector BoundsMax = FVector(
								DataAsFloat[AddressUints + ElementIndex * 3 + 0],
								DataAsFloat[AddressUints + ElementIndex * 3 + 1],
								DataAsFloat[AddressUints + ElementIndex * 3 + 2]);

							BoundsMaxRange[ElementIndex] = BoundsMax;
						});
						break;
					}
					case POINT_COLOR_ATTRIBUTE_ID:
					{
						TPCGValueRange<FVector4> ColorRange = OutPointData->GetColorValueRange(/*bAllocate=*/true);
						ParallelFor(NumElements, [DataAsFloat, AddressUints, &ColorRange](int32 ElementIndex)
						{
							const FVector4 Color = FVector4(
								DataAsFloat[AddressUints + ElementIndex * 4 + 0],
								DataAsFloat[AddressUints + ElementIndex * 4 + 1],
								DataAsFloat[AddressUints + ElementIndex * 4 + 2],
								DataAsFloat[AddressUints + ElementIndex * 4 + 3]);

							ColorRange[ElementIndex] = Color;
						});
						break;
					}
					case POINT_DENSITY_ATTRIBUTE_ID:
					{
						TPCGValueRange<float> DensityRange = OutPointData->GetDensityValueRange(/*bAllocate=*/true);
						ParallelFor(NumElements, [DataAsFloat, AddressUints, &DensityRange](int32 ElementIndex)
						{
							DensityRange[ElementIndex] = DataAsFloat[AddressUints + ElementIndex];
						});
						break;
					}
					case POINT_SEED_ATTRIBUTE_ID:
					{
						TPCGValueRange<int32> SeedRange = OutPointData->GetSeedValueRange(/*bAllocate=*/true);
						ParallelFor(NumElements, [DataAsInt, AddressUints, &SeedRange](int32 ElementIndex)
						{
							SeedRange[ElementIndex] = DataAsInt[AddressUints + ElementIndex];
						});
						break;
					}
					case POINT_STEEPNESS_ATTRIBUTE_ID:
					{
						TPCGValueRange<float> SteepnessRange = OutPointData->GetSteepnessValueRange(/*bAllocate=*/true);
						ParallelFor(NumElements, [DataAsFloat, AddressUints, &SteepnessRange](int32 ElementIndex)
						{
							SteepnessRange[ElementIndex] = DataAsFloat[AddressUints + ElementIndex];
						});
						break;
					}
					default:
						checkNoEntry();
						break;
					}
				}
				else
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(UnpackAttribute);

					if (FPCGMetadataAttributeBase* AttributeBase = MetadataAttributes[AttributeDesc.AttributeId])
					{
						ensure(PCGDataForGPUHelpers::UnpackAttributeHelper(InContext, PackedData, AttributeDesc, InStringTable, AddressUints, DataDesc.GetElementCountForAttribute(AttributeDesc), OutPointData));
					}
				}
			}

			// TODO: It may be more efficient to create a mapping from input point index to final output point index and do everything in one pass.
			auto ProcessRangeFunc = [OutPointData](int32 StartReadIndex, int32 StartWriteIndex, int32 Count)
			{
				int32 NumWritten = 0;

				const FConstPCGPointValueRanges InRanges(OutPointData);
				FPCGPointValueRanges OutRanges(OutPointData, /*bAllocate=*/false);

				for (int32 ReadIndex = StartReadIndex; ReadIndex < StartReadIndex + Count; ++ReadIndex)
				{
					if (InRanges.DensityRange[ReadIndex] == PCGComputeConstants::INVALID_DENSITY)
					{
						continue;
					}

					const int32 WriteIndex = StartWriteIndex + NumWritten;
					OutRanges.SetFromValueRanges(WriteIndex, InRanges, ReadIndex);
					++NumWritten;
				}

				return NumWritten;
			};

			auto MoveDataRangeFunc = [OutPointData](int32 RangeStartIndex, int32 MoveToIndex, int32 NumElements)
			{
				OutPointData->MoveRange(RangeStartIndex, MoveToIndex, NumElements);
			};

			auto FinishedFunc = [OutPointData](int32 NumWritten)
			{
				OutPointData->SetNumPoints(NumWritten);
			};

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(DiscardInvalidPoints);
				FPCGAsyncState* AsyncState = InContext ? &InContext->AsyncState : nullptr;
				FPCGAsync::AsyncProcessingRangeEx(
					AsyncState, 
					OutPointData->GetNumPoints(),
					[]{},
					ProcessRangeFunc,
					MoveDataRangeFunc,
					FinishedFunc,
					/*bEnableTimeSlicing=*/false);
			}
		}
		else if (TypeId == PARAM_DATA_TYPE_ID)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UnpackParamDataItem);

			UPCGParamData* OutParamData = FPCGContext::NewObject_AnyThread<UPCGParamData>(InContext);
			UPCGMetadata* Metadata = OutParamData->MutableMetadata();

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(InitializeOutput);

				TArray<TTuple</*EntryKey=*/int64, /*ParentEntryKey=*/int64>> AllMetadataEntries;
				AllMetadataEntries.SetNumUninitialized(NumElements);

				ParallelFor(NumElements, [&](int32 ElementIndex)
				{
					AllMetadataEntries[ElementIndex] = MakeTuple(Metadata->AddEntryPlaceholder(), PCGInvalidEntryKey);
				});

				Metadata->AddDelayedEntries(AllMetadataEntries);
			}

			FPCGTaggedData& OutTaggedData = OutData.Emplace_GetRef();
			OutTaggedData.Data = OutParamData;
			OutTaggedData.Pin = InPin;

			for (const FPCGKernelAttributeDesc& AttributeDesc : AttributeDescs)
			{
				MetadataAttributes[AttributeDesc.AttributeId] = PCGDataForGPUHelpers::CreateAttributeFromAttributeDesc(Metadata, AttributeDesc);
			}

			// No work to do if there are no elements to process.
			if (NumElements == 0)
			{
				continue;
			}

			// Loop over attributes.
			for (const FPCGKernelAttributeDesc& AttributeDesc : AttributeDescs)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(UnpackAttribute);

				if (FPCGMetadataAttributeBase* AttributeBase = MetadataAttributes[AttributeDesc.AttributeId])
				{
					const uint32 AddressUints = GetElementDataStartAddressUints(DataAsUint, DataIndex, AttributeDesc.AttributeId);
					ensure(PCGDataForGPUHelpers::UnpackAttributeHelper(InContext, PackedData, AttributeDesc, InStringTable, AddressUints, NumElements, OutParamData));
				}
			}
		}
		else { /* TODO: Support additional data types. */ }
	}

	return EPCGUnpackDataCollectionResult::Success;
}

uint32 FPCGDataCollectionDesc::ComputeDataElementCount(EPCGDataType InDataType) const
{
	uint32 ElementCount = 0;

	for (const FPCGDataDesc& DataDesc : DataDescs)
	{
		if (!!(DataDesc.Type & InDataType))
		{
			if (!!(DataDesc.Type & EPCGDataType::BaseTexture))
			{
				ElementCount += DataDesc.ElementCount2D.X * DataDesc.ElementCount2D.Y;
			}
			else
			{
				ElementCount += DataDesc.ElementCount;
			}
		}
	}

	return ElementCount;
}

void FPCGDataCollectionDesc::Combine(const FPCGDataCollectionDesc& Other)
{
	DataDescs.Append(Other.DataDescs);
}

bool FPCGDataCollectionDesc::GetAttributeDesc(FPCGAttributeIdentifier InAttributeIdentifier, FPCGKernelAttributeDesc& OutAttributeDesc, bool& bOutConflictingTypesFound) const
{
	// Will be set to the type of the first attribute across all the data which has a matching name, and then is used to detect if subsequent
	// attributes are found with conflicting type.
	EPCGKernelAttributeType FoundType = EPCGKernelAttributeType::Invalid;

	for (const FPCGDataDesc& DataDesc : DataDescs)
	{
		for (const FPCGKernelAttributeDesc& AttributeDesc : DataDesc.AttributeDescs)
		{
			if (AttributeDesc.AttributeKey.Identifier == InAttributeIdentifier && ensure(AttributeDesc.AttributeKey.Type != EPCGKernelAttributeType::Invalid))
			{
				if (FoundType == EPCGKernelAttributeType::Invalid)
				{
					// Take the first matching attribute.
					OutAttributeDesc = AttributeDesc;
					FoundType = AttributeDesc.AttributeKey.Type;
				}
				else if (FoundType != AttributeDesc.AttributeKey.Type)
				{
					// Signal conflict found.
					bOutConflictingTypesFound = true;

					// Can stop iterating once we find a conflict. Signal that a matching attribute was found (despite the conflict).
					return true;
				}
			}
		}
	}

	// Signal no conflict found.
	bOutConflictingTypesFound = false;

	// Signal attribute found or not.
	return FoundType != EPCGKernelAttributeType::Invalid;
}

bool FPCGDataCollectionDesc::ContainsAttributeOnAnyData(FPCGAttributeIdentifier InAttributeIdentifier) const
{
	return DataDescs.ContainsByPredicate([InAttributeIdentifier](const FPCGDataDesc& DataDesc)
	{
		return DataDesc.ContainsAttribute(InAttributeIdentifier);
	});
}

void FPCGDataCollectionDesc::AddAttributeToAllData(FPCGKernelAttributeKey InAttribute, const UPCGDataBinding* InBinding, const TArray<int32>* InOptionalUniqueStringKeys)
{
	for (FPCGDataDesc& DataDesc : DataDescs)
	{
		DataDesc.AddAttribute(InAttribute, InBinding, InOptionalUniqueStringKeys);
	}
}

void FPCGDataCollectionDesc::GetUniqueStringKeyValues(int32 InAttributeId, TArray<int32>& OutUniqueStringKeys) const
{
	for (const FPCGDataDesc& DataDesc : DataDescs)
	{
		for (const FPCGKernelAttributeDesc& AttributeDesc : DataDesc.AttributeDescs)
		{
			if (AttributeDesc.AttributeId == InAttributeId)
			{
				for (int32 StringKey : AttributeDesc.GetUniqueStringKeys())
				{
					OutUniqueStringKeys.AddUnique(StringKey);
				}

				break;
			}
		}
	}
}

FPCGProxyForGPUDataCollection::FPCGProxyForGPUDataCollection(TRefCountPtr<FRDGPooledBuffer> InBuffer, uint32 InBufferSizeBytes, const FPCGDataCollectionDesc& InDescription, const TArray<FString>& InStringTable)
	: Buffer(InBuffer)
	, BufferSizeBytes(InBufferSizeBytes)
	, Description(InDescription)
	, StringTable(InStringTable)
{
}

bool FPCGProxyForGPUDataCollection::GetCPUData(FPCGContext* InContext, int32 InDataIndex, FPCGTaggedData& OutData)
{
	UE::TScopeLock Lock(ReadbackLock);

	if (bReadbackDataProcessed)
	{
		OutData = ReadbackData.IsValidIndex(InDataIndex) ? ReadbackData[InDataIndex] : FPCGTaggedData();
		return true;
	}

	if (!ReadbackRequest && !bReadbackDataArrived)
	{
#if WITH_EDITOR
		if (CVarWarnOnGPUReadbacks.GetValueOnAnyThread())
		{
			PCGLog::LogWarningOnGraph(LOCTEXT("PerformingReadback", "Performing readback"), InContext);
		}
#endif

		ReadbackRequest = MakeShared<FRHIGPUBufferReadback>(TEXT("FPCGProxyForGPUDataCollectionReadback"));

		ENQUEUE_RENDER_COMMAND(ReadbackDataCollection)([WeakThis=AsWeak(), ReadbackRequest=ReadbackRequest, Buffer=Buffer, BufferSizeBytes=BufferSizeBytes](FRHICommandListImmediate& RHICmdList)
		{
			FRDGPooledBuffer* RDGBuffer = Buffer.GetReference();

			ReadbackRequest->EnqueueCopy(RHICmdList, Buffer->GetRHI());

			auto ExecuteAsync = [](auto&& RunnerFunc) -> void
			{
				if (IsInActualRenderingThread())
				{
					AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]()
					{
						RunnerFunc(RunnerFunc);
					});
				}
				else
				{
					// In specific cases (Server, -onethread, etc) the RenderingThread is actually the same as the GameThread.
					// When this happens we want to avoid calling AsyncTask which could put us in a infinite task execution loop. 
					// The reason is that if we are running this callback through the task graph we might stay in an executing loop until it has no tasks to execute,
					// since we are pushing a new task as long as our data isn't ready and we are not advancing the GameThread as we are already on the GameThread this causes a infinite task execution.
					// Instead delay to GameThread with ExecuteOnGameThread
					ExecuteOnGameThread(UE_SOURCE_LOCATION, [RunnerFunc]()
					{
						RunnerFunc(RunnerFunc);
					});
				}
			};

			auto RunnerFunc = [WeakThis, ReadbackRequest, BufferSizeBytes, ExecuteAsync](auto&& RunnerFunc) -> void
			{
				if (TSharedPtr<FPCGProxyForGPUDataCollection> This = WeakThis.Pin())
				{
					if (ReadbackRequest->IsReady())
					{
						if (void* RawData = ReadbackRequest->Lock(BufferSizeBytes))
						{
							uint32* DataAsUint = static_cast<uint32*>(RawData);

							This->RawReadbackData.SetNumUninitialized(BufferSizeBytes);
							FMemory::Memcpy(This->RawReadbackData.GetData(), RawData, BufferSizeBytes);
						}

						ReadbackRequest->Unlock();

						This->bReadbackDataArrived = true;
					}
					else
					{
						ExecuteAsync(RunnerFunc);
					}
				}
			};

			ExecuteAsync(RunnerFunc);

		});
	}

	if (!bReadbackDataArrived)
	{
		OutData = FPCGTaggedData();
		return false;
	}

	ReadbackRequest.Reset();

	bReadbackDataProcessed = true;

	if (!RawReadbackData.IsEmpty())
	{
		FPCGDataCollection DataFromGPU;
		const EPCGUnpackDataCollectionResult Result = Description.UnpackDataCollection(InContext, RawReadbackData, /*OutputPinLabelAlias*/NAME_None, StringTable, DataFromGPU);

		RawReadbackData.Empty();

		if (Result != EPCGUnpackDataCollectionResult::Success)
		{
			OutData = FPCGTaggedData();
			return true;
		}

		ReadbackData.Reset();
		ReadbackDataRefs.Reset();

		for (const FPCGTaggedData& TaggedData : DataFromGPU.TaggedData)
		{
			ReadbackData.Add(TaggedData);
			ReadbackDataRefs.Add(TStrongObjectPtr<const UPCGData>(TaggedData.Data));
		}
	}

	OutData = ReadbackData.IsValidIndex(InDataIndex) ? ReadbackData[InDataIndex] : FPCGTaggedData();
	return true;
}

void FPCGProxyForGPUDataCollection::UpdateElementCountsFromReadback(const TArray<uint32>& InElementCounts)
{
	uint32 DataCounter = 0;

	if (!ensure(Description.DataDescs.Num() == InElementCounts.Num()))
	{
		return;
	}

	for (FPCGDataDesc& DataDesc : Description.DataDescs)
	{
		DataDesc.ElementCount = InElementCounts[DataCounter++];
	}
}

#undef LOCTEXT_NAMESPACE

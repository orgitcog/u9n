// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGPropertyAccessor.h"
#include "Data/PCGSplineStruct.h"

#include "Components/SplineComponent.h"

enum class EPCGInterpCurveAccessorTarget
{
	Value,
	ArriveTangent,
	LeaveTangent,
	InterpMode
};

enum class EPCGSplineAccessorTarget
{
	Transform,
	ClosedLoop
};

/**
* Templated accessor for any interp curve. It's important that the keys only have a single value, the struct that holds the spline curve, since
* the interp curve is basically an array.
* Keys supported: PCGSplineData, FPCGSplineStruct, FSplineCurves
*/
template <typename CurveType,
	EPCGInterpCurveAccessorTarget Target = EPCGInterpCurveAccessorTarget::Value,
	std::enable_if_t<std::is_base_of_v<FInterpCurve<typename CurveType::ElementType>, CurveType>, int> = 0>
class FPCGInterpCurveAccessor : public IPCGAttributeAccessorT<FPCGInterpCurveAccessor<CurveType, Target>>, IPCGPropertyChain
{
public:
	// The underlying type is either the Curve element type or the same as the enum property accessor if the target is the InterpMode, as it is an enum below.
	using Type = std::conditional_t<Target == EPCGInterpCurveAccessorTarget::InterpMode, FPCGEnumPropertyAccessor::Type, typename CurveType::ElementType>;
	using Super = IPCGAttributeAccessorT<FPCGInterpCurveAccessor<CurveType, Target>>;

	FPCGInterpCurveAccessor(const FStructProperty* InProperty, TArray<const FProperty*>&& ExtraProperties = {})
		: Super(/*bInReadOnly=*/ false)
		, IPCGPropertyChain(InProperty, std::move(ExtraProperties))
	{
		static_assert(PCG::Private::IsPCGType<Type>());		
		check(InProperty && InProperty->Struct->IsChildOf(TBaseStructure<CurveType>::Get()));
		TopPropertyStruct = GetTopPropertyStruct();
	}

	bool GetRangeImpl(TArrayView<Type> OutValues, int32 Index, const IPCGAttributeAccessorKeys& Keys) const
	{
		// Spline struct contains an array, so there is only a single address, to the array.
		const void* ContainerKeys = nullptr;
		TArrayView<const void*> ContainerKeysView(&ContainerKeys, 1);
		if (!Keys.GetKeys(Index, ContainerKeysView))
		{
			return false;
		}

		// Validation to not access keys that are not the expected type. Done after the GetKeys, as we also want to discard other type of incompatible
		// keys (like a Default Metadata entry key)
		if (!ensure(Keys.IsClassSupported(TopPropertyStruct)))
		{
			return false;
		}

		PCGPropertyAccessor::AddressOffset(GetPropertyChain(), ContainerKeysView);

		const CurveType& InterpCurve = *static_cast<const CurveType*>(ContainerKeys);
		const int32 NumPoints = InterpCurve.Points.Num();
		
		if (NumPoints == 0)
		{
			return false;
		}
		
		for (int32 i = 0; i < OutValues.Num(); ++i)
		{
			const int32 CurrIndex = (Index + i) % NumPoints;
			if constexpr (Target == EPCGInterpCurveAccessorTarget::InterpMode)
			{
				OutValues[i] = static_cast<Type>(ConvertInterpCurveModeToSplinePointType(InterpCurve.Points[CurrIndex].InterpMode));
			}
			else if constexpr (Target == EPCGInterpCurveAccessorTarget::ArriveTangent)
			{
				OutValues[i] = InterpCurve.Points[CurrIndex].ArriveTangent;
			}
			else if constexpr (Target == EPCGInterpCurveAccessorTarget::LeaveTangent)
			{
				OutValues[i] = InterpCurve.Points[CurrIndex].LeaveTangent;
			}
			else // if constexpr (Target == EPCGInterpCurveAccessorTarget::Value)
			{
				OutValues[i] = InterpCurve.Points[CurrIndex].OutVal;
			}
		}

		return true;
	}

	bool SetRangeImpl(TArrayView<const Type> InValues, int32 Index, IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags)
	{		
		// Spline struct contains an array, so there is only a single address, to the array.
		void* ContainerKeys = nullptr;
		TArrayView<void*> ContainerKeysView(&ContainerKeys, 1);
		if (!Keys.GetKeys(Index, ContainerKeysView))
		{
			return false;
		}

		// Validation to not access keys that are not the expected type. Done after the GetKeys, as we also want to discard other type of incompatible
		// keys (like a Default Metadata entry key)
		if (!ensure(Keys.IsClassSupported(TopPropertyStruct)))
		{
			return false;
		}

		PCGPropertyAccessor::AddressOffset(GetPropertyChain(), ContainerKeysView);

		CurveType& InterpCurve = *static_cast<CurveType*>(ContainerKeys);
		const int32 NumPoints = InterpCurve.Points.Num();
		if (NumPoints == 0)
		{
			return false;
		}
		
		for (int32 i = 0; i < InValues.Num(); ++i)
		{
			const int32 CurrIndex = (Index + i) % NumPoints;

			if constexpr (Target == EPCGInterpCurveAccessorTarget::InterpMode)
			{
				InterpCurve.Points[CurrIndex].InterpMode = ConvertSplinePointTypeToInterpCurveMode(static_cast<ESplinePointType::Type>(InValues[i]));
			}
			else if constexpr (Target == EPCGInterpCurveAccessorTarget::ArriveTangent)
			{
				InterpCurve.Points[CurrIndex].ArriveTangent = InValues[i];
			}
			else if constexpr (Target == EPCGInterpCurveAccessorTarget::LeaveTangent)
			{
				InterpCurve.Points[CurrIndex].LeaveTangent = InValues[i];
			}
			else // if constexpr (Target == EPCGInterpCurveAccessorTarget::Value)
			{
				InterpCurve.Points[CurrIndex].OutVal = InValues[i];
			}
		}

		return true;
	}

private:
	const UStruct* TopPropertyStruct = nullptr;
};

/**
* Templated accessor for location/rotation/scale in world coordinates. It's important that the keys only have a single value, the struct that holds the spline data/struct, since
* there is a single transform per spline.
* Keys supported: PCGSplineData, FPCGSplineStruct
*/
template <typename T, EPCGControlPointsAccessorTarget Target, bool bWorldCoordinates>
class FPCGControlPointsAccessor : public IPCGAttributeAccessorT<FPCGControlPointsAccessor<T, Target, bWorldCoordinates>>, IPCGPropertyChain
{
public:
	// The underlying type is a quat if we target the rotation, otherwise a vector
	using Type = T;
	using Super = IPCGAttributeAccessorT<FPCGControlPointsAccessor<Type, Target, bWorldCoordinates>>;
	
	FPCGControlPointsAccessor(const FStructProperty* InProperty, TArray<const FProperty*>&& ExtraProperties = {})
		: Super(/*bInReadOnly=*/ false)
		, IPCGPropertyChain(InProperty, std::move(ExtraProperties))
	{
		static_assert(PCG::Private::IsPCGType<Type>());		
		check(InProperty && InProperty->Struct->IsChildOf<FPCGSplineStruct>());
		TopPropertyStruct = GetTopPropertyStruct();
	}

	bool GetRangeImpl(TArrayView<Type> OutValues, int32 Index, const IPCGAttributeAccessorKeys& Keys) const
	{
		// We want to access the transform on the spline struct, there is just a single one.
		const void* ContainerKeys = nullptr;
		TArrayView<const void*> ContainerKeysView(&ContainerKeys, 1);
		if (!Keys.GetKeys(Index, ContainerKeysView))
		{
			return false;
		}

		// Validation to not access keys that are not the expected type. Done after the GetKeys, as we also want to discard other type of incompatible
		// keys (like a Default Metadata entry key)
		if (!ensure(Keys.IsClassSupported(TopPropertyStruct)))
		{
			return false;
		}

		PCGPropertyAccessor::AddressOffset(GetPropertyChain(), ContainerKeysView);

		const FPCGSplineStruct& SplineStruct = *static_cast<const FPCGSplineStruct*>(ContainerKeys);
		const FInterpCurveVector& Positions = SplineStruct.GetSplinePointsPosition();
		const FInterpCurveQuat& Rotations = SplineStruct.GetSplinePointsRotation();
		const FInterpCurveVector& Scale = SplineStruct.GetSplinePointsScale();
		const FTransform SplineTransform = SplineStruct.GetTransform();

		const int32 NumPoints = Positions.Points.Num();
		if (NumPoints == 0)
		{
			return false;
		}
		
		for (int32 i = 0; i < OutValues.Num(); ++i)
		{
			const int32 CurrIndex = (Index + i) % NumPoints;
			
			if constexpr (Target == EPCGControlPointsAccessorTarget::Location)
			{
				static_assert(std::is_same_v<Type, FVector>);
				OutValues[i] = Positions.Points[CurrIndex].OutVal;
				if constexpr (bWorldCoordinates)
				{
					OutValues[i] = SplineTransform.TransformPosition(OutValues[i]);
				}
			}
			else if constexpr (Target == EPCGControlPointsAccessorTarget::Rotation)
			{
				static_assert(std::is_same_v<Type, FQuat>);
				OutValues[i] = Rotations.Points[CurrIndex].OutVal;
				if constexpr (bWorldCoordinates)
				{
					OutValues[i] = SplineTransform.TransformRotation(OutValues[i]);
				}
			}
			else if constexpr (Target == EPCGControlPointsAccessorTarget::Scale)
			{
				
				static_assert(std::is_same_v<Type, FVector>);
				OutValues[i] = Scale.Points[CurrIndex].OutVal;
				if constexpr (bWorldCoordinates)
				{
					OutValues[i] = (FTransform(FQuat::Identity, FVector::ZeroVector, OutValues[i]) * SplineTransform).GetScale3D();
				}
			}
			else if constexpr (Target == EPCGControlPointsAccessorTarget::Transform)
			{
				static_assert(std::is_same_v<Type, FTransform>);
				OutValues[i] = FTransform(Rotations.Points[CurrIndex].OutVal, Positions.Points[CurrIndex].OutVal, Scale.Points[CurrIndex].OutVal);
				if constexpr (bWorldCoordinates)
				{
					OutValues[i] = OutValues[i] * SplineTransform;
				}
			}
			else
			{
				// Pitfall static assert
				static_assert(!std::is_same_v<Type, Type>);
			}
		}

		return true;
	}

	bool SetRangeImpl(TArrayView<const Type> InValues, int32 Index, IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags)
	{		
		// We want to access the transform on the spline struct, there is just a single one.
		void* ContainerKeys = nullptr;
		TArrayView<void*> ContainerKeysView(&ContainerKeys, 1);
		if (!Keys.GetKeys(Index, ContainerKeysView))
		{
			return false;
		}

		// Validation to not access keys that are not the expected type. Done after the GetKeys, as we also want to discard other type of incompatible
		// keys (like a Default Metadata entry key)
		if (!ensure(Keys.IsClassSupported(TopPropertyStruct)))
		{
			return false;
		}

		PCGPropertyAccessor::AddressOffset(GetPropertyChain(), ContainerKeysView);

		FPCGSplineStruct& SplineStruct = *static_cast<FPCGSplineStruct*>(ContainerKeys);
		const FTransform InverseSplineTransform = SplineStruct.GetTransform().Inverse();
		
		const int32 NumPoints = SplineStruct.SplineCurves.Position.Points.Num();
		if (NumPoints == 0)
		{
			return false;
		}
		
		for (int32 i = 0; i < InValues.Num(); ++i)
		{
			const int32 CurrIndex = (Index + i) % NumPoints;
			
			if constexpr (Target == EPCGControlPointsAccessorTarget::Location)
			{
				static_assert(std::is_same_v<Type, FVector>);
				FVector& OutPosition = SplineStruct.SplineCurves.Position.Points[CurrIndex].OutVal;
				OutPosition = InValues[i];
				if constexpr (bWorldCoordinates)
				{
					OutPosition = InverseSplineTransform.TransformPosition(OutPosition);
				}
			}
			else if constexpr (Target == EPCGControlPointsAccessorTarget::Rotation)
			{
				static_assert(std::is_same_v<Type, FQuat>);
				FQuat& OutRotation = SplineStruct.SplineCurves.Rotation.Points[CurrIndex].OutVal;
				OutRotation = InValues[i];
				if constexpr (bWorldCoordinates)
				{
					OutRotation = InverseSplineTransform.TransformRotation(OutRotation);
				}
			}
			else if constexpr (Target == EPCGControlPointsAccessorTarget::Scale)
			{
				static_assert(std::is_same_v<Type, FVector>);
				FVector& OutScale = SplineStruct.SplineCurves.Scale.Points[CurrIndex].OutVal;
				OutScale = InValues[i];
				
				if constexpr (bWorldCoordinates)
				{
					OutScale = (FTransform(FQuat::Identity, FVector::ZeroVector, OutScale) * InverseSplineTransform).GetScale3D();
				}
			}
			else if constexpr (Target == EPCGControlPointsAccessorTarget::Transform)
			{
				static_assert(std::is_same_v<Type, FTransform>);
				FTransform Transform = InValues[i];
				if constexpr (bWorldCoordinates)
				{
					Transform = Transform * InverseSplineTransform;
				}

				SplineStruct.SplineCurves.Position.Points[CurrIndex].OutVal = InValues[i].GetLocation();
				SplineStruct.SplineCurves.Rotation.Points[CurrIndex].OutVal = InValues[i].GetRotation();
				SplineStruct.SplineCurves.Scale.Points[CurrIndex].OutVal = InValues[i].GetScale3D();
			}
			else
			{
				// Pitfall static assert
				static_assert(!std::is_same_v<Type, Type>);
			}
		}

		return true;
	}

private:
	const UStruct* TopPropertyStruct = nullptr;
};

/**
* Templated accessor for global spline data. Note that closed loop value is read-only.
* Keys supported: PCGSplineData, FPCGSplineStruct
*/
template <typename T, EPCGSplineAccessorTarget Target>
class FPCGSplineAccessor : public IPCGAttributeAccessorT<FPCGSplineAccessor<T, Target>>, IPCGPropertyChain
{
public:
	// The underlying type is a quat if we target the rotation, otherwise a vector
	using Type = T;
	using Super = IPCGAttributeAccessorT<FPCGSplineAccessor<T, Target>>;
	
	FPCGSplineAccessor(const FStructProperty* InProperty, TArray<const FProperty*>&& ExtraProperties = {})
		: Super(/*bInReadOnly=*/ Target == EPCGSplineAccessorTarget::ClosedLoop)
		, IPCGPropertyChain(InProperty, std::move(ExtraProperties))
	{
		static_assert(PCG::Private::IsPCGType<Type>());		
		check(InProperty && InProperty->Struct->IsChildOf<FPCGSplineStruct>());
		TopPropertyStruct = GetTopPropertyStruct();
	}

	bool GetRangeImpl(TArrayView<Type> OutValues, int32 Index, const IPCGAttributeAccessorKeys& Keys) const
	{
		// We want to access the transform on the spline struct, there is just a single one.
		const void* ContainerKeys = nullptr;
		TArrayView<const void*> ContainerKeysView(&ContainerKeys, 1);
		if (!Keys.GetKeys(Index, ContainerKeysView))
		{
			return false;
		}

		// Validation to not access keys that are not the expected type. Done after the GetKeys, as we also want to discard other type of incompatible
		// keys (like a Default Metadata entry key)
		if (!ensure(Keys.IsClassSupported(TopPropertyStruct)))
		{
			return false;
		}

		PCGPropertyAccessor::AddressOffset(GetPropertyChain(), ContainerKeysView);

		const FPCGSplineStruct& SplineStruct = *static_cast<const FPCGSplineStruct*>(ContainerKeys);
		
		for (int32 i = 0; i < OutValues.Num(); ++i)
		{
			if constexpr (Target == EPCGSplineAccessorTarget::Transform)
			{
				static_assert(std::is_same_v<Type, FTransform>);
				OutValues[i] = SplineStruct.GetTransform();
			}
			else if constexpr (Target == EPCGSplineAccessorTarget::ClosedLoop)
			{
				static_assert(std::is_same_v<Type, bool>);
				OutValues[i] = SplineStruct.IsClosedLoop();
			}
			else
			{
				// Pitfall static assert
				static_assert(!std::is_same_v<Type, Type>);
			}
		}

		return true;
	}

	bool SetRangeImpl(TArrayView<const Type> InValues, int32 Index, IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags)
	{
		if (Target == EPCGSplineAccessorTarget::ClosedLoop)
		{
			// Not supported, as we need to update the spline when we change it, which is not threadsafe.
			return false;
		}
			
		// We want to access the transform on the spline struct, there is just a single one.
		void* ContainerKeys = nullptr;
		TArrayView<void*> ContainerKeysView(&ContainerKeys, 1);
		if (!Keys.GetKeys(Index, ContainerKeysView))
		{
			return false;
		}

		// Validation to not access keys that are not the expected type. Done after the GetKeys, as we also want to discard other type of incompatible
		// keys (like a Default Metadata entry key)
		if (!ensure(Keys.IsClassSupported(TopPropertyStruct)))
		{
			return false;
		}

		PCGPropertyAccessor::AddressOffset(GetPropertyChain(), ContainerKeysView);

		FPCGSplineStruct& SplineStruct = *static_cast<FPCGSplineStruct*>(ContainerKeys);
		
		for (int32 i = 0; i < InValues.Num(); ++i)
		{
			if constexpr (Target == EPCGSplineAccessorTarget::Transform)
			{
				static_assert(std::is_same_v<Type, FTransform>);
				SplineStruct.Transform = InValues[i];
			}
			else if constexpr (Target == EPCGSplineAccessorTarget::ClosedLoop)
			{
				// Not supported, as we need to update the spline when we change it, which is not threadsafe.
				// Need to stay here to avoid unreachable code.
			}
			else
			{
				// Pitfall static assert
				static_assert(!std::is_same_v<Type, Type>);
			}
		}

		return true;
	}

private:
	const UStruct* TopPropertyStruct = nullptr;
};
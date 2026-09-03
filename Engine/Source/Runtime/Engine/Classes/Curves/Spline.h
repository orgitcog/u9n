// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/InterpCurve.h"

#include "Spline.generated.h"

struct FSplinePoint;
struct FSplineCurves;
class FLegacySpline;

struct UE_INTERNAL FSpline;

/**
 * A general purpose, reflected spline.
 * The implementation can be configured at runtime.
 */
USTRUCT()
struct FSpline
{
	GENERATED_BODY()

	ENGINE_API FSpline();
	ENGINE_API FSpline(const FSpline& Other);
	ENGINE_API FSpline& operator=(const FSpline& Other);
	ENGINE_API ~FSpline();
	ENGINE_API static FSpline FromSplineCurves(const FSplineCurves& InSpline);
	
	/* Control Point Index Interface */
	
	ENGINE_API void AddPoint(const FSplinePoint& InPoint);
	ENGINE_API void InsertPoint(const FSplinePoint& InPoint, int32 Index);
	ENGINE_API FSplinePoint GetPoint(const int32 Index) const;
	ENGINE_API void RemovePoint(const int32 Index);
	
	ENGINE_API void SetLocation(int32 Index, const FVector& InLocation);
	ENGINE_API FVector GetLocation(const int32 Index) const;
	
	ENGINE_API void SetInTangent(const int32 Index, const FVector& InTangent);
	ENGINE_API FVector GetInTangent(const int32 Index) const;
	
	ENGINE_API void SetOutTangent(const int32 Index, const FVector& OutTangent);
	ENGINE_API FVector GetOutTangent(const int32 Index) const;

	ENGINE_API void SetRotation(int32 Index, const FQuat& InRotation);
	ENGINE_API FQuat GetRotation(const int32 Index) const;
	
	ENGINE_API void SetScale(int32 Index, const FVector& InScale);
	ENGINE_API FVector GetScale(const int32 Index) const;
	
	ENGINE_API void SetSplinePointType(int32 Index, EInterpCurveMode Type);
	ENGINE_API EInterpCurveMode GetSplinePointType(int32 Index) const;

	ENGINE_API float GetParameterAtIndex(int32 Index) const;
	ENGINE_API float GetParameterAtDistance(float Distance) const;
	ENGINE_API float GetDistanceAtParameter(float Parameter) const;
	
	/* Parameter Interface */

	ENGINE_API FVector Evaluate(float Param) const;
	ENGINE_API FVector EvaluateDerivative(float Param) const;
	ENGINE_API FQuat EvaluateRotation(float Param) const;
	ENGINE_API FVector EvaluateScale(float Param) const;

	/* Attribute Interface */
	
	bool SupportsAttributes() const { return false; }
	ENGINE_API bool HasAttributeChannel(FName AttributeName) const;
	ENGINE_API int32 NumAttributeValues(FName AttributeName) const;
	ENGINE_API float GetAttributeParameter(int32 Index, const FName& Name) const;
	ENGINE_API int32 SetAttributeParameter(int32 Index, float Parameter, const FName& Name);
	ENGINE_API void RemoveAttributeValue(int32 Index, FName AttributeName);
	ENGINE_API TArray<FName> GetFloatPropertyChannels() const;
	ENGINE_API TArray<FName> GetVectorPropertyChannels() const;
	
	template<typename T> ENGINE_API T GetAttributeValue(int32 Index, const FName& Name) const;
	template<typename T> ENGINE_API void SetAttributeValue(int32 Index, const T& Value, const FName& Name);
	template<typename T> ENGINE_API bool CreateAttributeChannel(FName AttributeName) const;
	template<typename T> ENGINE_API int32 AddAttributeValue(float Param, const T& Value, FName AttributeName) const;
	
	template<typename T> ENGINE_API T EvaluateAttribute(float Param, FName AttributeName) const;

	ENGINE_API float FindNearest(const FVector& InLocation) const;
	ENGINE_API float FindNearestOnSegment(const FVector& InLocation, int32 SegmentIndex) const;
	
	/* Misc Interface */
	
	ENGINE_API bool operator==(const FSpline& Other) const;
	bool operator!=(const FSpline& Other) const
	{
		return !(*this == Other);
	}
	
	friend ENGINE_API FArchive& operator<<(FArchive& Ar, FSpline& Spline)
	{
		Spline.Serialize(Ar);
		return Ar;
	}
	ENGINE_API bool Serialize(FArchive& Ar);
	ENGINE_API void SerializeLoad(FArchive& Ar);
	ENGINE_API void SerializeSave(FArchive& Ar) const;
	ENGINE_API bool ExportTextItem(FString& ValueStr, FSpline const& DefaultValue, class UObject* Parent, int32 PortFlags, class UObject* ExportRootScope) const;
	ENGINE_API bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText);

	uint32 GetVersion() const { return Version; }
	
	ENGINE_API const FInterpCurveVector& GetSplinePointsPosition() const;
	ENGINE_API const FInterpCurveQuat& GetSplinePointsRotation() const;
	ENGINE_API const FInterpCurveVector& GetSplinePointsScale() const;

	/** Returns the length of the specified spline segment up to the parametric value given */
	ENGINE_API float GetSegmentLength(const int32 Index, const float Param, const FVector& Scale3D = FVector(1.0f)) const;

	/** Returns total length along this spline */
	ENGINE_API float GetSplineLength() const;
	
	/** Returns the total number of control points on this spline. */
	ENGINE_API int32 GetNumControlPoints() const;
	
	/** Reset the spline to an empty spline. */
	ENGINE_API void Reset();
	
	/** Reset the rotation attribute channel to default values. */
	ENGINE_API void ResetRotation();
	
	/** Reset the scale attribute channel to default values. */
	ENGINE_API void ResetScale();
	
	/** Reset the spline to the default spline (2 points). */
	ENGINE_API void ResetToDefault();

	struct FUpdateSplineParams
	{
		bool bClosedLoop = false;
		bool bStationaryEndpoints = false;
		int32 ReparamStepsPerSegment = 10;
		bool bLoopPositionOverride = false;
		float LoopPosition = 0.0f;
		const FVector Scale3D = FVector(1.0f);
	};
	
	/** Update the spline's internal data according to the passed-in params. */
	ENGINE_API void UpdateSpline(const FUpdateSplineParams& InParams);

private:
	
	static inline const FInterpCurveVector PositionCurve;
	static inline const FInterpCurveQuat RotationCurve;
	static inline const FInterpCurveVector ScaleCurve;
	
	// Used for upgrade logic in spline component.
	// Not ideal, but allows us to automatically populate the proxy
	// at serialize time when we might otherwise not be able to.
	friend class USplineComponent;
#if WITH_EDITOR
	uint8 PreviousImplementation;
#endif
	uint8 CurrentImplementation;
	uint32 Version;
	TSharedPtr<FLegacySpline> Data;					// Valid when CurrentImplementation is 1.
	
	bool IsEnabled() const { return CurrentImplementation != 0; }
	bool IsLegacy() const { return CurrentImplementation == 1; }

#if WITH_EDITOR
	bool WasEnabled() const { return PreviousImplementation != 0; }
	bool WasLegacy() const { return PreviousImplementation == 1; }

	/** Called when the implementation is changed at editor time due to a console command. */
	void OnSplineImplementationChanged();
	FDelegateHandle OnSplineImplementationChangedHandle;
#endif
};

template<>
struct TStructOpsTypeTraits<FSpline> : public TStructOpsTypeTraitsBase2<FSpline>
{
	enum
	{
		WithSerializer				= true, // Enables the use of a custom Serialize method.
		WithIdenticalViaEquality	= true, // Enables the use of a custom equality operator.
		WithExportTextItem			= true, // Enables the use of a custom ExportTextItem method.
		WithImportTextItem			= true, // Enables the use of a custom ImportTextItem method.
	};
};

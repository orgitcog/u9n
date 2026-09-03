// Copyright Epic Games, Inc. All Rights Reserved.

#include "Curves/Spline.h"

#include "Components/SplineComponent.h"	// for FSplineCurves, ideally removed
#include "Misc/Base64.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/FortniteMainBranchObjectVersion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(Spline)

namespace UE::Spline
{

int32 GImplementation = 0;
#if WITH_EDITOR
static FSimpleMulticastDelegate& GetOnSplineImplementationChanged()
{
	static FSimpleMulticastDelegate OnSplineImplementationChanged;
	return OnSplineImplementationChanged;
};
static void SplineImplementationSink(IConsoleVariable*)
{
	static int32 GPreviousImplementation = -1;
	if (GPreviousImplementation != GImplementation)
	{
		GImplementation = FMath::Clamp(GImplementation, 0, 2);
		GetOnSplineImplementationChanged().Broadcast();
		GPreviousImplementation = GImplementation;
	}
}
FAutoConsoleVariableRef CVarSplineImplementation(
	TEXT("Spline.Implementation"),
	GImplementation,
	TEXT("0) Not Implemented - 1) Legacy Implementation - 2) New Implementation"),
	FConsoleVariableDelegate::CreateStatic(&SplineImplementationSink)
);
#endif
	
bool GApproximateTangents = false;
FAutoConsoleVariableRef CVarApproximateTangents(
	TEXT("Spline.ApproximateTangents"),
	GApproximateTangents,
	TEXT("True if we should approximate tangents using the central difference formula.")
);

bool GFallbackFindNearest = false;
FAutoConsoleVariableRef CVarFallbackFindNearest(
	TEXT("Spline.FallbackFindNearest"),
	GFallbackFindNearest,
	TEXT("True if we should implement FindNearest and FindNearestOnSegment using an intermediate spline representation. Only applies if Spline.Implementation == 2.")
);

}

/** FLegacySpline Definition */

class FLegacySpline
{
	FInterpCurveVector PositionCurve;
	FInterpCurveQuat RotationCurve;
	FInterpCurveVector ScaleCurve;
	FInterpCurveFloat ReparamTable;
	
public:

	ENGINE_API FLegacySpline();
	ENGINE_API FLegacySpline(const FSplineCurves& Other);
	
	/* Control Point Index Interface */

	void AddPoint(const FSplinePoint& InPoint);
	void InsertPoint(const FSplinePoint& InPoint, int32 Index);
	FSplinePoint GetPoint(const int32 Index) const;
	void RemovePoint(int32 Index);
	
	void SetLocation(int32 Index, const FVector& InLocation);
	FVector GetLocation(const int32 Index) const;
	
	void SetInTangent(const int32 Index, const FVector& InTangent);
	FVector GetInTangent(const int32 Index) const;
	
	void SetOutTangent(const int32 Index, const FVector& OutTangent);
	FVector GetOutTangent(const int32 Index) const;

	void SetRotation(int32 Index, const FQuat& InRotation);
	FQuat GetRotation(const int32 Index) const;
	
	void SetScale(int32 Index, const FVector& InScale);
	FVector GetScale(const int32 Index) const;
	
	void SetSplinePointType(int32 Index, EInterpCurveMode Type);
	EInterpCurveMode GetSplinePointType(int32 Index) const;

	float GetParameterAtIndex(int32 Index) const;
	float GetParameterAtDistance(float Distance) const;
	float GetDistanceAtParameter(float Parameter) const;
	
	/* Parameter Interface */

	FVector Evaluate(float Param) const;
	FVector EvaluateDerivative(float Param) const;
	FQuat EvaluateRotation(float Param) const;
	FVector EvaluateScale(float Param) const;
	float FindNearest(const FVector& InLocation) const;
	float FindNearestOnSegment(const FVector& InLocation, int32 SegmentIndex) const;
	
	/* Misc Interface */
	
	bool operator==(const FLegacySpline& Other) const
	{
		return PositionCurve == Other.PositionCurve
			&& RotationCurve == Other.RotationCurve
			&& ScaleCurve == Other.ScaleCurve;
	}

	bool operator!=(const FLegacySpline& Other) const
	{
		return !(*this == Other);
	}
	
	friend ENGINE_API FArchive& operator<<(FArchive& Ar, FLegacySpline& Spline)
	{
		Spline.Serialize(Ar);
		return Ar;
	}
	bool Serialize(FArchive& Ar);
	
	FInterpCurveVector& GetSplinePointsPosition() { return PositionCurve; }
	const FInterpCurveVector& GetSplinePointsPosition() const { return PositionCurve; }
	FInterpCurveQuat& GetSplinePointsRotation() { return RotationCurve; }
	const FInterpCurveQuat& GetSplinePointsRotation() const { return RotationCurve; }
	FInterpCurveVector& GetSplinePointsScale() { return ScaleCurve; }
	const FInterpCurveVector& GetSplinePointsScale() const { return ScaleCurve; }

	/** Returns the length of the specified spline segment up to the parametric value given */
	float GetSegmentLength(const int32 Index, const float Param, const FVector& Scale3D = FVector(1.0f)) const;

	/** Returns total length along this spline */
	float GetSplineLength() const;
	
	/** Returns the total number of control points on this spline. */
	int32 GetNumControlPoints() const;
	
	/** Reset the spline to an empty spline. */
	void Reset();
	
	/** Reset the rotation attribute channel to default values. */
	void ResetRotation();
	
	/** Reset the scale attribute channel to default values. */
	void ResetScale();
	
	/** Reset the spline to the default spline (2 points). */
	void ResetToDefault();
	
	/** Update the spline's internal data according to the passed-in params. */
	void UpdateSpline(const FSpline::FUpdateSplineParams& InParams);
};



/** FSpline Implementation */

FSpline::FSpline()
: CurrentImplementation(UE::Spline::GImplementation)
, Version(0xffffffff)
{
#if WITH_EDITOR
	PreviousImplementation = 0;
#endif
	
	switch (CurrentImplementation)
	{
	default: break;		// Intentionally doing nothing
	case 1: Data = MakeShared<FLegacySpline>(); break;
	}

#if WITH_EDITOR
	OnSplineImplementationChangedHandle = UE::Spline::GetOnSplineImplementationChanged().AddRaw(this, &FSpline::OnSplineImplementationChanged);
#endif
}

FSpline::FSpline(const FSpline& Other)
	: FSpline()
{
	*this = Other;
}

FSpline& FSpline::operator=(const FSpline& Other)
{
	CurrentImplementation = Other.CurrentImplementation;

	switch (CurrentImplementation)
	{
	default:
		break;
	case 1:
		*Data = Other.Data ? *Other.Data : FLegacySpline();
		break;
	}
	
	return *this;
}

FSpline::~FSpline()
{
#if WITH_EDITOR
	if (OnSplineImplementationChangedHandle.IsValid())
	{
		UE::Spline::GetOnSplineImplementationChanged().Remove(OnSplineImplementationChangedHandle);
		OnSplineImplementationChangedHandle.Reset();
	}
#endif
}

FSpline FSpline::FromSplineCurves(const FSplineCurves& InSpline)
{
	FSpline NewSpline;
	
#if WITH_EDITOR
	NewSpline.PreviousImplementation = UE::Spline::GImplementation;
#endif
	NewSpline.CurrentImplementation = UE::Spline::GImplementation;

	switch (NewSpline.CurrentImplementation)
	{
	default: break;		// Intentionally doing nothing
	case 1: NewSpline.Data = MakeShared<FLegacySpline>(InSpline); break;
	}
	
	return NewSpline;
}

void FSpline::AddPoint(const FSplinePoint& InPoint)
{
	switch (CurrentImplementation)
	{
	default: break;		// Intentionally doing nothing
	case 1: Data->AddPoint(InPoint); break;
	}
}

void FSpline::InsertPoint(const FSplinePoint& InPoint, int32 Index)
{
	switch (CurrentImplementation)
	{
	default: break;		// Intentionally doing nothing
	case 1: Data->InsertPoint(InPoint, Index); break;
	}
}

FSplinePoint FSpline::GetPoint(const int32 Index) const
{
	switch (CurrentImplementation)
	{
	default: return FSplinePoint();		// Intentionally doing nothing
	case 1: return Data->GetPoint(Index);
	}
}

void FSpline::RemovePoint(const int32 Index)
{
	switch (CurrentImplementation)
	{
	default: break;		// Intentionally doing nothing
	case 1: Data->RemovePoint(Index); break;
	}
}

void FSpline::SetLocation(int32 Index, const FVector& InLocation)
{
	switch (CurrentImplementation)
	{
	default: break;		// Intentionally doing nothing
	case 1: Data->SetLocation(Index, InLocation); break;
	}
}

FVector FSpline::GetLocation(const int32 Index) const
{
	switch (CurrentImplementation)
	{
	default: return FVector();
	case 1: return Data->GetLocation(Index);
	}
}

void FSpline::SetInTangent(const int32 Index, const FVector& InTangent)
{
	switch (CurrentImplementation)
	{
	default: break;
	case 1: Data->SetInTangent(Index, InTangent); break;
	}
}

FVector FSpline::GetInTangent(const int32 Index) const
{
	switch (CurrentImplementation)
	{
	default: return FVector();
	case 1: return Data->GetInTangent(Index);
	}
}

void FSpline::SetOutTangent(const int32 Index, const FVector& OutTangent)
{
	switch (CurrentImplementation)
	{
	default: break;
	case 1: Data->SetOutTangent(Index, OutTangent); break;
	}
}

FVector FSpline::GetOutTangent(const int32 Index) const
{
	switch (CurrentImplementation)
	{
	default: return FVector();
	case 1: return Data->GetOutTangent(Index);
	}
}

void FSpline::SetRotation(int32 Index, const FQuat& InRotation)
{
	switch (CurrentImplementation)
	{
	default: break;
	case 1: Data->SetRotation(Index, InRotation); break;
	}
}

FQuat FSpline::GetRotation(const int32 Index) const
{
	switch (CurrentImplementation)
	{
	default: return FQuat();
	case 1: return Data->GetRotation(Index);
	}
}

void FSpline::SetScale(int32 Index, const FVector& InScale)
{
	switch (CurrentImplementation)
	{
	default: break;
	case 1: Data->SetScale(Index, InScale); break;
	}
}

FVector FSpline::GetScale(const int32 Index) const
{
	switch (CurrentImplementation)
	{
	default: return FVector(1.0);
	case 1: return Data->GetScale(Index);
	}
}

void FSpline::SetSplinePointType(int32 Index, EInterpCurveMode Type)
{
	switch (CurrentImplementation)
	{
	default: break;
	case 1: Data->SetSplinePointType(Index, Type); break;
	}
}

EInterpCurveMode FSpline::GetSplinePointType(int32 Index) const
{
	switch (CurrentImplementation)
	{
	default: return CIM_Unknown;
	case 1: return Data->GetSplinePointType(Index);
	}
}

float FSpline::GetParameterAtIndex(int32 Index) const
{
	switch (CurrentImplementation)
	{
	default: return 0.f;
	case 1: return Data->GetParameterAtIndex(Index);
	}
}

float FSpline::GetParameterAtDistance(float Distance) const
{
	switch (CurrentImplementation)
	{
	default: return 0.f;
	case 1: return Data->GetParameterAtDistance(Distance);
	}
}

float FSpline::GetDistanceAtParameter(float Parameter) const
{
	switch (CurrentImplementation)
	{
	default: return 0.f;
	case 1: return Data->GetDistanceAtParameter(Parameter);
	}
}

FVector FSpline::Evaluate(float Param) const
{
	switch (CurrentImplementation)
	{
	default: return FVector();
	case 1: return Data->Evaluate(Param);
	}
}

FVector FSpline::EvaluateDerivative(float Param) const
{
	if (UE::Spline::GApproximateTangents)
	{
		// Approximate using central difference.
		
		// This computes the tangent direction using central difference and
		// assumes that tangent magnitude is linearly changing between control points.
		// While the assumption about magnitude is probably wrong, it works well.
		
		auto ClampValidParameterRange = [Min = 0.f, Max = GetNumControlPoints() - 1](float Param)
			{ return FMath::Clamp(Param, Min, Max); };

		constexpr float H = UE_KINDA_SMALL_NUMBER;
		const float ParamLow = ClampValidParameterRange(Param - H);
		const float ParamHigh = ClampValidParameterRange(Param + H);
		const FVector Tangent = ((Evaluate(ParamHigh) - Evaluate(ParamLow)) / (ParamHigh - ParamLow)).GetSafeNormal();

		const int32 Index1 = FMath::Clamp((int32)Param, 0, GetNumControlPoints() - 1);
		const int32 Index2 = FMath::Clamp((int32)(Param+1), 0, GetNumControlPoints() - 1);
		const float Mag1 = GetInTangent(Index1).Length();
		const float Mag2 = GetInTangent(Index2).Length();
		const float Mag = FMath::Lerp(Mag1, Mag2, FMath::Frac(Param));

		return Tangent * Mag;
	}
	else
	{
		switch (CurrentImplementation)
		{
		default: return FVector();
		case 1: return Data->EvaluateDerivative(Param);
		}
	}
}

FQuat FSpline::EvaluateRotation(float Param) const
{
	switch (CurrentImplementation)
	{
	default: return FQuat();
	case 1: return Data->EvaluateRotation(Param);
	}
}

FVector FSpline::EvaluateScale(float Param) const
{
	switch (CurrentImplementation)
	{
	default: return FVector();
	case 1: return Data->EvaluateScale(Param);
	}	
}

bool FSpline::HasAttributeChannel(FName AttributeName) const
{
	return false;
}

int32 FSpline::NumAttributeValues(FName AttributeName) const
{
	return 0;
}

float FSpline::GetAttributeParameter(int32 Index, const FName& Name) const
{
	return 0.f;
}

int32 FSpline::SetAttributeParameter(int32 Index, float Parameter, const FName& Name)
{
	return INDEX_NONE;
}

template<typename T>
T FSpline::GetAttributeValue(int32 Index, const FName& Name) const
{
	return T();
}
template ENGINE_API float FSpline::GetAttributeValue<float>(int32 Index, const FName& Name) const;
template ENGINE_API FVector FSpline::GetAttributeValue<FVector>(int32 Index, const FName& Name) const;

template<typename T>
void FSpline::SetAttributeValue(int32 Index, const T& Value, const FName& Name)
{
}
template ENGINE_API void FSpline::SetAttributeValue<float>(int32 Index, const float& Value, const FName& Name);
template ENGINE_API void FSpline::SetAttributeValue<FVector>(int32 Index, const FVector& Value, const FName& Name);

template<typename T>
bool FSpline::CreateAttributeChannel(FName AttributeName) const
{
	return false;
}
template ENGINE_API bool FSpline::CreateAttributeChannel<float>(FName AttributeName) const;
template ENGINE_API bool FSpline::CreateAttributeChannel<FVector>(FName AttributeName) const;

template<typename T>
int32 FSpline::AddAttributeValue(float Param, const T& Value, FName AttributeName) const
{
	return INDEX_NONE;
}
template ENGINE_API int32 FSpline::AddAttributeValue<float>(float Param, const float& Value, FName AttributeName) const;
template ENGINE_API int32 FSpline::AddAttributeValue<FVector>(float Param, const FVector& Value, FName AttributeName) const;

void FSpline::RemoveAttributeValue(int32 Index, FName AttributeName)
{
}

TArray<FName> FSpline::GetFloatPropertyChannels() const
{
	TArray<FName> ChannelNames;
	return ChannelNames;
}

TArray<FName> FSpline::GetVectorPropertyChannels() const
{
	TArray<FName> ChannelNames;
	return ChannelNames;
}

template<typename T>
T FSpline::EvaluateAttribute(float Param, FName AttributeName) const
{
	return T(0);
}

template ENGINE_API float FSpline::EvaluateAttribute<float>(float Param, FName AttributeName) const;
template ENGINE_API FVector FSpline::EvaluateAttribute<FVector>(float Param, FName AttributeName) const;
float FSpline::FindNearest(const FVector& InLocation) const
{
	switch (CurrentImplementation)
	{
	default: return 0.f;
	case 1: return Data->FindNearest(InLocation);
	}
}

float FSpline::FindNearestOnSegment(const FVector& InLocation, int32 SegmentIndex) const
{
	switch (CurrentImplementation)
	{
	default: return 0.f;
	case 1: return Data->FindNearestOnSegment(InLocation, SegmentIndex);
	}
}

bool FSpline::operator==(const FSpline& Other) const
{
	if (CurrentImplementation == Other.CurrentImplementation)
	{
		if (Data && Other.Data)
		{
			return *Data == *Other.Data;
		}
	}

	return false;
}

bool FSpline::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);
	
	// Data format:

	// Byte 1 - The data format, determine by CurrentImplementation at the time of last save
	// Remaining N Bytes - Spline data (or empty, if byte 0 is 0). The format is determined by byte 1.
	
	if (Ar.IsLoading())
	{
		SerializeLoad(Ar);
	}
	else
	{
		SerializeSave(Ar);
	}

	return true;
}

void FSpline::SerializeLoad(FArchive& Ar)
{
	int8 PreviousImpl;
	Ar << PreviousImpl;
#if WITH_EDITOR
	PreviousImplementation = PreviousImpl;
#endif

	auto WasEnabled = [PreviousImpl]()
	{ return PreviousImpl != 0; };

	auto WasLegacy = [PreviousImpl]()
	{ return PreviousImpl == 1; };
		
	if (WasEnabled())
	{
		if (WasLegacy())
		{
			if (IsLegacy())
			{
				Data = MakeShared<FLegacySpline>();
				Ar << *Data;
			}
			else if (!IsEnabled())
			{
				// Intentionally doing nothing with data
				FLegacySpline IntermediateData;
				Ar << IntermediateData;
			}
		}
	}
}

void FSpline::SerializeSave(FArchive& Ar) const
{
	uint8 CurrentImpl = CurrentImplementation;
	Ar << CurrentImpl;
		
	if (IsLegacy())
	{
		Ar << *Data;
	}
}

bool FSpline::ExportTextItem(FString& ValueStr, FSpline const& DefaultValue, class UObject* Parent, int32 PortFlags, class UObject* ExportRootScope) const
{
	// serialize our spline
    TArray<uint8> SplineWriteBuffer;
    FMemoryWriter MemWriter(SplineWriteBuffer);
    SerializeSave(MemWriter);

    FString Base64String = FBase64::Encode(SplineWriteBuffer);

    // Base64 encoding uses the '/' character, but T3D interprets '//' as some kind of
    // terminator (?). If it occurs then the string passed to ImportTextItem() will
    // come back as full of nullptrs. So we will swap in '-' here, and swap back to '/' in ImportTextItem()
	Base64String.ReplaceCharInline('/', '-');

	ValueStr = FString::Printf(TEXT("SplineData SplineDataLen=%d SplineData=%s\r\n"), Base64String.Len(), *Base64String);
	
	return true;
}

bool FSpline::ImportTextItem(const TCHAR*& SourceText, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
	if (FParse::Command(&SourceText, TEXT("SplineData")))	
	{
		static const TCHAR SplineDataLenToken[] = TEXT("SplineDataLen=");
		const TCHAR* FoundSplineDataLenStart = FCString::Strifind(SourceText, SplineDataLenToken);
		if (FoundSplineDataLenStart)
		{
			SourceText = FoundSplineDataLenStart + FCString::Strlen(SplineDataLenToken);
			int32 SplineDataLen = FCString::Atoi(SourceText);

			static const TCHAR SplineDataToken[] = TEXT("SplineData=");
			const TCHAR* FoundSplineDataStart = FCString::Strifind(SourceText, SplineDataToken);
			if (FoundSplineDataStart)
			{
				SourceText = FoundSplineDataStart + FCString::Strlen(SplineDataToken);
				FString SplineData = FString::ConstructFromPtrSize(SourceText, SplineDataLen);

				// fix-up the hack applied to the Base64-encoded string in ExportTextItem()
				SplineData.ReplaceCharInline('-', '/');

				TArray<uint8> SplineReadBuffer;
				bool bDecoded = FBase64::Decode(SplineData, SplineReadBuffer);
				if (bDecoded)
				{
					FMemoryReader MemReader(SplineReadBuffer);
					SerializeLoad(MemReader);
				}
			}
		}


	}
	
	return true;
}

const FInterpCurveVector& FSpline::GetSplinePointsPosition() const
{
	switch (CurrentImplementation)
	{
	default: return PositionCurve;
	case 1: return Data->GetSplinePointsPosition();
	}
}

const FInterpCurveQuat& FSpline::GetSplinePointsRotation() const
{
	switch (CurrentImplementation)
	{
	default: return RotationCurve;
	case 1: return Data->GetSplinePointsRotation();
	}
}

const FInterpCurveVector& FSpline::GetSplinePointsScale() const
{
	switch (CurrentImplementation)
	{
	default: return ScaleCurve;
	case 1: return Data->GetSplinePointsScale();
	}
}

float FSpline::GetSegmentLength(const int32 Index, const float Param, const FVector& Scale3D) const
{
	switch (CurrentImplementation)
	{
	default: return 0.f;
	case 1: return Data->GetSegmentLength(Index, Param, Scale3D);
	}
}

float FSpline::GetSplineLength() const
{
	switch (CurrentImplementation)
	{
	default: return 0.f;	// Intentionally doing nothing
	case 1: return Data->GetSplineLength();
	}
}

int32 FSpline::GetNumControlPoints() const
{
	switch (CurrentImplementation)
	{
	default: return 0;
	case 1: return Data->GetNumControlPoints();
	}
}

void FSpline::Reset()
{
	switch (CurrentImplementation)
	{
	default: break;
	case 1: Data->Reset(); break;
	}
}
	
void FSpline::ResetRotation()
{
	switch (CurrentImplementation)
	{
	default: break;
	case 1: Data->ResetRotation(); break;
	}
}
	
void FSpline::ResetScale()
{
	switch (CurrentImplementation)
	{
	default: break;
	case 1: Data->ResetScale(); break;
	}
}
	
void FSpline::ResetToDefault()
{
	switch (CurrentImplementation)
	{
	default: break;
	case 1: Data->ResetToDefault(); break;
	}
}

void FSpline::UpdateSpline(const FUpdateSplineParams& InParams)
{
	switch (CurrentImplementation)
    {
    default: break;
    case 1: Data->UpdateSpline(InParams); break;
    }
}

#if WITH_EDITOR
void FSpline::OnSplineImplementationChanged()
{
	using namespace UE::Spline;
	
	if (GImplementation == CurrentImplementation)			// 0->0, 1->1
	{
	}
	else if (GImplementation == 0)							// 1->0
	{
		Data.Reset();
	}
	else if (CurrentImplementation == 0)					// 0->1
	{
		if (GImplementation == 1)
		{
			Data = MakeShared<FLegacySpline>();
		}
	}

	CurrentImplementation = GImplementation;
}
#endif

/** FLegacySpline Implementation */

FLegacySpline::FLegacySpline()
{
	ResetToDefault();
}

FLegacySpline::FLegacySpline(const FSplineCurves& InSpline)
	: FLegacySpline()
{
	PositionCurve = InSpline.Position;
	RotationCurve = InSpline.Rotation;
	ScaleCurve = InSpline.Scale;
	ReparamTable = InSpline.ReparamTable;
}

void FLegacySpline::AddPoint(const FSplinePoint& InPoint)
{
	auto UpperBound = [this](float Value)
	{
		int32 Count = PositionCurve.Points.Num();
		int32 First = 0;

		while (Count > 0)
		{
			const int32 Middle = Count / 2;
			if (Value >= PositionCurve.Points[First + Middle].InVal)
			{
				First += Middle + 1;
				Count -= Middle + 1;
			}
			else
			{
				Count = Middle;
			}
		}

		return First;
	};
	
	int32 Index = UpperBound(InPoint.InputKey);
	
	PositionCurve.Points.Insert(FInterpCurvePoint<FVector>(
		InPoint.InputKey,
		InPoint.Position,
		InPoint.ArriveTangent,
		InPoint.LeaveTangent,
		ConvertSplinePointTypeToInterpCurveMode(InPoint.Type)
	), Index);

	RotationCurve.Points.Insert(FInterpCurvePoint<FQuat>(
		InPoint.InputKey,
		InPoint.Rotation.Quaternion(),
		FQuat::Identity,
		FQuat::Identity,
		CIM_CurveAuto
	), Index);

	ScaleCurve.Points.Insert(FInterpCurvePoint<FVector>(
		InPoint.InputKey,
		InPoint.Scale,
		FVector::ZeroVector,
		FVector::ZeroVector,
		CIM_CurveAuto
	), Index);
}

void FLegacySpline::InsertPoint(const FSplinePoint& InPoint, int32 Index)
{
	const float InKey = (Index == 0) ? 0.0f : GetParameterAtIndex(Index - 1) + 1.0f;
	
	PositionCurve.Points.Insert(FInterpCurvePoint<FVector>(
		InKey,
		InPoint.Position,
		InPoint.ArriveTangent,
		InPoint.LeaveTangent,
		ConvertSplinePointTypeToInterpCurveMode(InPoint.Type)
	), Index);

	RotationCurve.Points.Insert(FInterpCurvePoint<FQuat>(
		InKey,
		InPoint.Rotation.Quaternion(),
		FQuat::Identity,
		FQuat::Identity,
		CIM_CurveAuto
	), Index);

	ScaleCurve.Points.Insert(FInterpCurvePoint<FVector>(
		InKey,
		InPoint.Scale,
		FVector::ZeroVector,
		FVector::ZeroVector,
		CIM_CurveAuto
	), Index);

	// Increment all point input values after the inserted element
	for (int32 Idx = Index + 1; Idx < PositionCurve.Points.Num(); Idx++)
	{
		PositionCurve.Points[Idx].InVal += 1.f;
		RotationCurve.Points[Idx].InVal += 1.f;
		ScaleCurve.Points[Idx].InVal += 1.f;
	}
}

FSplinePoint FLegacySpline::GetPoint(const int32 Index) const
{
	FSplinePoint Point;

	if (!PositionCurve.Points.IsValidIndex(Index))
	{
		return Point;
	}
	
	Point.InputKey = PositionCurve.Points[Index].InVal;
	Point.Position = PositionCurve.Points[Index].OutVal;
	Point.ArriveTangent = PositionCurve.Points[Index].ArriveTangent;
	Point.LeaveTangent = PositionCurve.Points[Index].LeaveTangent;
	Point.Rotation = RotationCurve.Points[Index].OutVal.Rotator();
	Point.Scale = ScaleCurve.Points[Index].OutVal;
	Point.Type = ConvertInterpCurveModeToSplinePointType(PositionCurve.Points[Index].InterpMode);
	
	return Point;
}

void FLegacySpline::RemovePoint(int32 Index)
{
	if (!PositionCurve.Points.IsValidIndex(Index))
	{
		return;
	}
	
	PositionCurve.Points.RemoveAt(Index);
	RotationCurve.Points.RemoveAt(Index);
	ScaleCurve.Points.RemoveAt(Index);

	while (Index < GetNumControlPoints())
	{
		PositionCurve.Points[Index].InVal -= 1.0f;
		RotationCurve.Points[Index].InVal -= 1.0f;
		ScaleCurve.Points[Index].InVal -= 1.0f;
		Index++;
	}
}

void FLegacySpline::SetLocation(int32 Index, const FVector& InLocation)
{
	if (!PositionCurve.Points.IsValidIndex(Index))
	{
		return;
	}
	
	PositionCurve.Points[Index].OutVal = InLocation;
}

FVector FLegacySpline::GetLocation(const int32 Index) const
{
	return PositionCurve.Points.IsValidIndex(Index) ? PositionCurve.Points[Index].OutVal : FVector();
}

void FLegacySpline::SetInTangent(const int32 Index, const FVector& InTangent)
{
	if (!PositionCurve.Points.IsValidIndex(Index))
	{
		return;
	}
	
	PositionCurve.Points[Index].ArriveTangent = InTangent;
	PositionCurve.Points[Index].InterpMode = CIM_CurveUser;
}

FVector FLegacySpline::GetInTangent(const int32 Index) const
{
	return PositionCurve.Points.IsValidIndex(Index) ? PositionCurve.Points[Index].ArriveTangent : FVector();
}

void FLegacySpline::SetOutTangent(const int32 Index, const FVector& OutTangent)
{
	if (!PositionCurve.Points.IsValidIndex(Index))
	{
		return;
	}
	
	PositionCurve.Points[Index].LeaveTangent = OutTangent;
	PositionCurve.Points[Index].InterpMode = CIM_CurveUser;
}

FVector FLegacySpline::GetOutTangent(const int32 Index) const
{
	return PositionCurve.Points.IsValidIndex(Index) ? PositionCurve.Points[Index].LeaveTangent : FVector();
}

void FLegacySpline::SetRotation(int32 Index, const FQuat& InRotation)
{
	if (!RotationCurve.Points.IsValidIndex(Index))
	{
		return;
	}
	
	RotationCurve.Points[Index].OutVal = InRotation;
}

FQuat FLegacySpline::GetRotation(const int32 Index) const
{
	return RotationCurve.Points.IsValidIndex(Index) ? RotationCurve.Points[Index].OutVal : FQuat();
}

void FLegacySpline::SetScale(int32 Index, const FVector& InScale)
{
	if (!ScaleCurve.Points.IsValidIndex(Index))
	{
		return;
	}
	
	ScaleCurve.Points[Index].OutVal = InScale;
}

FVector FLegacySpline::GetScale(const int32 Index) const
{
	return ScaleCurve.Points.IsValidIndex(Index) ? ScaleCurve.Points[Index].OutVal : FVector();
}

void FLegacySpline::SetSplinePointType(int32 Index, EInterpCurveMode Type)
{
	if (!PositionCurve.Points.IsValidIndex(Index))
	{
		return;
	}
	
	PositionCurve.Points[Index].InterpMode = Type;
}

EInterpCurveMode FLegacySpline::GetSplinePointType(const int32 Index) const
{
	return PositionCurve.Points.IsValidIndex(Index) ? PositionCurve.Points[Index].InterpMode.GetValue() : CIM_Unknown;
}

float FLegacySpline::GetParameterAtIndex(int32 Index) const
{
	return PositionCurve.Points.IsValidIndex(Index) ? PositionCurve.Points[Index].InVal : 0.f;
}

float FLegacySpline::GetParameterAtDistance(float Distance) const
{
	return ReparamTable.Eval(Distance);
}

float FLegacySpline::GetDistanceAtParameter(float Parameter) const
{
	// this might be duplicating what an interp curve can already do...
	
	const float ParameterMax = PositionCurve.Points.Last().InVal;
	const float Key = (Parameter / ParameterMax) * (ReparamTable.Points.Num() - 1);
	const int32 LowerKey = FMath::FloorToInt32(Key);
	ensureAlways(LowerKey >= 0 && LowerKey < ReparamTable.Points.Num());
	const int32 UpperKey = FMath::CeilToInt32(Key);
	ensureAlways(UpperKey >= 0 && UpperKey < ReparamTable.Points.Num());
	const float Alpha = FMath::Frac(Key);
	const float Distance = FMath::Lerp(ReparamTable.Points[LowerKey].InVal, ReparamTable.Points[UpperKey].InVal, Alpha);

	return Distance;
}

FVector FLegacySpline::Evaluate(float Param) const
{
	return PositionCurve.Eval(Param);
}

FVector FLegacySpline::EvaluateDerivative(float Param) const
{
	return PositionCurve.EvalDerivative(Param);
}

FQuat FLegacySpline::EvaluateRotation(float Param) const
{
	return RotationCurve.Eval(Param);
}

FVector FLegacySpline::EvaluateScale(float Param) const
{
	return ScaleCurve.Eval(Param);
}

float FLegacySpline::FindNearest(const FVector& InLocation) const
{
	float Dummy;
	return PositionCurve.FindNearest(InLocation, Dummy);
}

float FLegacySpline::FindNearestOnSegment(const FVector& InLocation, int32 SegmentIndex) const
{
	if (!PositionCurve.Points.IsValidIndex(SegmentIndex))
	{
		return 0.f;
	}
	
	float Dummy;
	return PositionCurve.FindNearestOnSegment(InLocation, SegmentIndex, Dummy);
}

bool FLegacySpline::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);
	Ar << PositionCurve;
	Ar << RotationCurve;
	Ar << ScaleCurve;
	Ar << ReparamTable;

	return true;
}

float FLegacySpline::GetSegmentLength(const int32 Index, const float Param, const FVector& Scale3D) const
{
	const int32 NumPoints = PositionCurve.Points.Num();
	const int32 LastPoint = NumPoints - 1;

	check(Index >= 0 && ((PositionCurve.bIsLooped && Index < NumPoints) || (!PositionCurve.bIsLooped && Index < LastPoint)));
	check(Param >= 0.0f && Param <= 1.0f);

	// Evaluate the length of a Hermite spline segment.
	// This calculates the integral of |dP/dt| dt, where P(t) is the spline equation with components (x(t), y(t), z(t)).
	// This isn't solvable analytically, so we use a numerical method (Legendre-Gauss quadrature) which performs very well
	// with functions of this type, even with very few samples.  In this case, just 5 samples is sufficient to yield a
	// reasonable result.

	struct FLegendreGaussCoefficient
	{
		float Abscissa;
		float Weight;
	};

	static const FLegendreGaussCoefficient LegendreGaussCoefficients[] =
	{
		{ 0.0f, 0.5688889f },
		{ -0.5384693f, 0.47862867f },
		{ 0.5384693f, 0.47862867f },
		{ -0.90617985f, 0.23692688f },
		{ 0.90617985f, 0.23692688f }
	};

	const auto& StartPoint = PositionCurve.Points[Index];
	const auto& EndPoint = PositionCurve.Points[Index == LastPoint ? 0 : Index + 1];

	const auto& P0 = StartPoint.OutVal;
	const auto& T0 = StartPoint.LeaveTangent;
	const auto& P1 = EndPoint.OutVal;
	const auto& T1 = EndPoint.ArriveTangent;

	// Special cases for linear or constant segments
	if (StartPoint.InterpMode == CIM_Linear)
	{
		return ((P1 - P0) * Scale3D).Size() * Param;
	}
	else if (StartPoint.InterpMode == CIM_Constant)
	{
		// Special case: constant interpolation acts like distance = 0 for all p in [0, 1[ but for p == 1, the distance returned is the linear distance between start and end
		return Param == 1.f ? ((P1 - P0) * Scale3D).Size() : 0.0f;
	}

	// Cache the coefficients to be fed into the function to calculate the spline derivative at each sample point as they are constant.
	const FVector Coeff1 = ((P0 - P1) * 2.0f + T0 + T1) * 3.0f;
	const FVector Coeff2 = (P1 - P0) * 6.0f - T0 * 4.0f - T1 * 2.0f;
	const FVector Coeff3 = T0;

	const float HalfParam = Param * 0.5f;

	float Length = 0.0f;
	for (const auto& LegendreGaussCoefficient : LegendreGaussCoefficients)
	{
		// Calculate derivative at each Legendre-Gauss sample, and perform a weighted sum
		const float Alpha = HalfParam * (1.0f + LegendreGaussCoefficient.Abscissa);
		const FVector Derivative = ((Coeff1 * Alpha + Coeff2) * Alpha + Coeff3) * Scale3D;
		Length += Derivative.Size() * LegendreGaussCoefficient.Weight;
	}
	Length *= HalfParam;

	return Length;
}

float FLegacySpline::GetSplineLength() const
{
	const int32 NumPoints = ReparamTable.Points.Num();

	// This is given by the input of the last entry in the remap table
	if (NumPoints > 0)
	{
		return ReparamTable.Points.Last().InVal;
	}

	return 0.0f;
}

int32 FLegacySpline::GetNumControlPoints() const
{
	return PositionCurve.Points.Num();
}

void FLegacySpline::Reset()
{
	PositionCurve.Points.Reset();
	RotationCurve.Points.Reset();
	ScaleCurve.Points.Reset();
}

void FLegacySpline::ResetRotation()
{
	RotationCurve.Points.Reset(PositionCurve.Points.Num());

	for (int32 Count = RotationCurve.Points.Num(); Count < PositionCurve.Points.Num(); Count++)
	{
		RotationCurve.Points.Emplace(static_cast<float>(Count), FQuat::Identity, FQuat::Identity, FQuat::Identity, CIM_CurveAuto);
	}
}

void FLegacySpline::ResetScale()
{
	ScaleCurve.Points.Reset(PositionCurve.Points.Num());

	for (int32 Count = ScaleCurve.Points.Num(); Count < PositionCurve.Points.Num(); Count++)
	{
		ScaleCurve.Points.Emplace(static_cast<float>(Count), FVector(1.0f), FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);
	}
}

void FLegacySpline::ResetToDefault()
{
	PositionCurve.Points.Reset(10);
	RotationCurve.Points.Reset(10);
	ScaleCurve.Points.Reset(10);

	PositionCurve.Points.Emplace(0.0f, FVector(0, 0, 0), FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);
	RotationCurve.Points.Emplace(0.0f, FQuat::Identity, FQuat::Identity, FQuat::Identity, CIM_CurveAuto);
	ScaleCurve.Points.Emplace(0.0f, FVector(1.0f), FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);

	PositionCurve.Points.Emplace(1.0f, FVector(100, 0, 0), FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);
	RotationCurve.Points.Emplace(1.0f, FQuat::Identity, FQuat::Identity, FQuat::Identity, CIM_CurveAuto);
	ScaleCurve.Points.Emplace(1.0f, FVector(1.0f), FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);
}

void FLegacySpline::UpdateSpline(const FSpline::FUpdateSplineParams& InParams)
{
	const int32 NumPoints = PositionCurve.Points.Num();
	check(RotationCurve.Points.Num() == NumPoints && ScaleCurve.Points.Num() == NumPoints);

#if DO_CHECK
	// Ensure input keys are strictly ascending
	for (int32 Index = 1; Index < NumPoints; Index++)
	{
		ensureAlways(PositionCurve.Points[Index - 1].InVal < PositionCurve.Points[Index].InVal);
	}
#endif

	// Ensure splines' looping status matches with that of the spline component
	if (InParams.bClosedLoop)
	{
		const float LastKey = PositionCurve.Points.Num() > 0 ? PositionCurve.Points.Last().InVal : 0.0f;
		const float LoopKey = InParams.bLoopPositionOverride ? InParams.LoopPosition : LastKey + 1.0f;
		PositionCurve.SetLoopKey(LoopKey);
		RotationCurve.SetLoopKey(LoopKey);
		ScaleCurve.SetLoopKey(LoopKey);
	}
	else
	{
		PositionCurve.ClearLoopKey();
		RotationCurve.ClearLoopKey();
		ScaleCurve.ClearLoopKey();
	}

	// Automatically set the tangents on any CurveAuto keys
	PositionCurve.AutoSetTangents(0.0f, InParams.bStationaryEndpoints);
	RotationCurve.AutoSetTangents(0.0f, InParams.bStationaryEndpoints);
	ScaleCurve.AutoSetTangents(0.0f, InParams.bStationaryEndpoints);

	// Now initialize the spline reparam table
	const int32 NumSegments = PositionCurve.bIsLooped ? NumPoints : FMath::Max(0, NumPoints - 1);

	// Start by clearing it
	ReparamTable.Points.Reset(NumSegments * InParams.ReparamStepsPerSegment + 1);
	float AccumulatedLength = 0.0f;
	for (int32 SegmentIndex = 0; SegmentIndex < NumSegments; ++SegmentIndex)
	{
		for (int32 Step = 0; Step < InParams.ReparamStepsPerSegment; ++Step)
		{
			const float Param = static_cast<float>(Step) / InParams.ReparamStepsPerSegment;
			const float SegmentLength = (Step == 0) ? 0.0f : GetSegmentLength(SegmentIndex, Param, InParams.Scale3D);

			ReparamTable.Points.Emplace(SegmentLength + AccumulatedLength, SegmentIndex + Param, 0.0f, 0.0f, CIM_Linear);
		}
		AccumulatedLength += GetSegmentLength(SegmentIndex, 1.0f, InParams.Scale3D);
	}
	ReparamTable.Points.Emplace(AccumulatedLength, static_cast<float>(NumSegments), 0.0f, 0.0f, CIM_Linear);
}
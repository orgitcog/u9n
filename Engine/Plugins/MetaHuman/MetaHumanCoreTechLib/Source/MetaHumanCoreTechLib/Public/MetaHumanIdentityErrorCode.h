// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanIdentityErrorCode.generated.h"

UENUM()
enum class EIdentityErrorCode : uint8
{
	None = 0,
	MLRig,
	CreateRigFromDNA,
	LoadBrows,
	NoDNA,
	NoTemplate,
	CreateDebugFolder,
	CalculatePCAModel,
	Initialization,
	CameraParameters,
	ScanInput,
	DepthInput,
	TeethSource,
	FitRigid,
	FitPCA,
	FitTeethFailed,
	TeethDepthDelta,
	UpdateRigWithTeeth,
	InvalidDNA,
	ApplyDeltaDNA,
	RefineTeeth,
	ApplyScaleToDNA,
	NoPart,
	InCompatibleDNA,
	CaptureDataInvalid,
	SolveFailed,
	BrowsFailed,
	NoPose,
	FitEyesFailed,
	BadInputMeshTopology,
};

UENUM()
enum class EAutoRigIdentityValidationError : uint8
{
	None = 0,
	BodyNotSelected,
	BodyIndexInvalid,
	EmptyConformalMesh,
	MeshNotConformed,
	NoFacePart
};
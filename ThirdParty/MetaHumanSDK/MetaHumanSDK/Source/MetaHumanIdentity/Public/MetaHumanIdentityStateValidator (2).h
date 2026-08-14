// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/SecureHash.h"

#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"


enum class EIdentityPoseType : uint8;
enum class EIdentityInvalidationState : uint8;

enum class EIdentityEditType: uint8
{
	Add,
	Remove,
	ChangeProperty,
	Count
};

// Teeth fitting state has to be checked separately as it doesn't prevent you from further progressing
enum class EIdentityProgressState : uint8
{
	Solve, AR, PrepareForPerformance, Complete, Invalid
};

struct FIdentityHashes
{
	FSHAHash SolveStateHash;
	FSHAHash TeethStateHash;
};

class METAHUMANIDENTITY_API FMetaHumanIdentityStateValidator : public TSharedFromThis<FMetaHumanIdentityStateValidator>
{

public:
	FMetaHumanIdentityStateValidator();

	void PostAssetLoadHashInitialization(TWeakObjectPtr<class UMetaHumanIdentity> InIdentity);
	
	void UpdateIdentityProgress();

	void MeshConformedStateUpdate();
	void MeshAutoriggedUpdate();
	void MeshPreparedForPerformanceUpdate() const;
	void TeethFittedUpdate();

	//void PopulateIdentityValidationTooltip();

	FText GetInvalidationStateToolTip();

private:

	void CalculateIdentityHashes();
	void BindToContourDataChangeDelegates();
	void InvalidateIdentityWhenContoursChange();
	void InvalidateTeethWhenContoursChange();
	void UpdateIdentityInvalidationState();
	void UpdateCurrentProgressState();

	FSHAHash GetHashForString(const FString& InStringToHash) const;
	FSHAHash GetSolverStateHash() const;
	FSHAHash GetTeethStateHash() const;
	
	FText IdentityStateTooltip;
	FIdentityHashes IdentityHashes;
	EIdentityProgressState CurrentProgress;
	
	TWeakObjectPtr<class UMetaHumanIdentity> Identity;
	
	TPair<int32, FText> SolveText;
	TPair<int32, FText> MeshToMetahumanText;
	TPair<int32, FText> FitTeethText;
	TPair<int32, FText> PrepareForPerformanceText;

	bool bPrepareForPerformanceEnabled = false;

};

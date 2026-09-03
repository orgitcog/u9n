// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "MetaHumanFaceFittingSolver.generated.h"



/** MetaHuman Face Fitting Solver
*
*   Holds configuration info used by the solver.
*
*/
UCLASS()
class METAHUMANFACEFITTINGSOLVER_API UMetaHumanFaceFittingSolver : public UObject
{
	GENERATED_BODY()

public:

	// Delegate called when something changes in the face fitting solver data that others should know about
	DECLARE_MULTICAST_DELEGATE(FOnInternalsChanged)

#if WITH_EDITOR

	//~Begin UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangedEvent) override;
	virtual void PostTransacted(const FTransactionObjectEvent& InTransactionEvent) override;
	//~End UObject interface

#endif

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (InlineEditConditionToggle))
	bool bOverrideDeviceConfig = false;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditCondition = "bOverrideDeviceConfig"))
	TObjectPtr<class UMetaHumanConfig> DeviceConfig;

	UPROPERTY()
	TObjectPtr<class UMetaHumanFaceAnimationSolver> FaceAnimationSolver;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	TObjectPtr<class UMetaHumanConfig> PredictiveSolver;

	/** Load the solvers for face fitting */
	void LoadFaceFittingSolvers();

	/** Load Solver that will be trained as part of preparing identity for performance */
	void LoadPredictiveSolver();

	bool CanProcess() const;
	bool GetConfigDisplayName(class UCaptureData* InCaptureData, FString& OutName) const;

	FString GetFittingTemplateData(class UCaptureData* InCaptureData = nullptr) const;
	FString GetFittingConfigData(class UCaptureData* InCaptureData = nullptr) const;
	FString GetFittingConfigTeethData(class UCaptureData* InCaptureData = nullptr) const;
	FString GetFittingIdentityModelData(class UCaptureData* InCaptureData = nullptr) const;
	FString GetFittingControlsData(class UCaptureData* InCaptureData = nullptr) const;

	TArray<uint8> GetPredictiveGlobalTeethTrainingData() const;
	TArray<uint8> GetPredictiveTrainingData() const;

	FOnInternalsChanged& OnInternalsChanged();

private:

	class UMetaHumanConfig* GetEffectiveConfig(class UCaptureData* InCaptureData) const;

	FString JsonObjectAsString(TSharedPtr<class FJsonObject> InJsonObject) const;

	FOnInternalsChanged OnInternalsChangedDelegate;

	void NotifyInternalsChanged();
};

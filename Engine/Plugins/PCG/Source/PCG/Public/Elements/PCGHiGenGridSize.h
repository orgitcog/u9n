// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "PCGElement.h"
#include "PCGSettings.h"

#include "PCGHiGenGridSize.generated.h"

/**
 * Set the execution grid size for downstream nodes. Enables executing a single graph across a hierarchy of grids.
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGHiGenGridSizeSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	EPCGHiGenGrid GetGrid() const;
	uint32 GetGridSize() const;

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::HierarchicalGeneration; }
	virtual TArray<FPCGPreConfiguredSettingsInfo> GetPreconfiguredInfo() const override;
	virtual bool OnlyExposePreconfiguredSettings() const override { return true; }
#endif
	virtual FString GetAdditionalTitleInformation() const override;
	virtual void ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo) override;
	virtual bool HasDynamicPins() const override { return true; }
	virtual EPCGDataType GetCurrentPinTypes(const UPCGPin* InPin) const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName = "HiGen Grid Size"))
	EPCGHiGenGrid HiGenGridSize = EPCGHiGenGrid::Grid256;

	/** This property is hidden from the user and drives the behavior of the Grid Size node. */
	UPROPERTY()
	bool bShowInputPin = true;

protected:
#if WITH_EDITOR
	virtual EPCGChangeType GetChangeTypeForProperty(const FName& InPropertyName) const override;
	virtual TArray<FPCGPreconfiguredInfo> GetConversionInfo() const override;
#endif
	virtual bool HasExecutionDependencyPin() const override { return bShowInputPin; }
	virtual bool ConvertNode(const FPCGPreconfiguredInfo& ConversionInfo) override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface
};

class FPCGHiGenGridSizeElement : public IPCGElement
{
public:
	virtual void GetDependenciesCrc(const FPCGGetDependenciesCrcParams& InParams, FPCGCrc& OutCrc) const override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool SupportsBasePointDataInputs(FPCGContext* InContext) const override { return true; }
};

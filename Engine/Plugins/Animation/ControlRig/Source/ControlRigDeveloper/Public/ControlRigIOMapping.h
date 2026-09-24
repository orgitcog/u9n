// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphPin.h"
#include "Rigs/RigHierarchyDefines.h"
#include "RigVMCore/RigVMExternalVariable.h"
#include "Styling/SlateTypes.h"

struct FRigVMVariableMappingInfo;
struct FOptionalPinFromProperty;
class IDetailLayoutBuilder;
class UClass;
class USkeleton;
class UControlRig;

struct CONTROLRIGDEVELOPER_API FControlRigIOMapping : public TSharedFromThis<FControlRigIOMapping>
{
	DECLARE_DELEGATE_RetVal(USkeleton*, FOnGetTargetSkeleton);
	DECLARE_DELEGATE_RetVal(UClass*, FOnGetTargetClass);
	DECLARE_DELEGATE_TwoParams(FOnPinCheckStateChanged, ECheckBoxState /*NewState*/, FName /** Property Name */);
	DECLARE_DELEGATE_ThreeParams(FOnVariableMappingChanged, const FName& /*PathName*/, const FName& /*Curve*/, bool /*bInput*/);

	FControlRigIOMapping() = delete;
	FControlRigIOMapping(TMap<FName, FName>& InInputMapping, TMap<FName, FName>& InOutputMapping, TArray<FOptionalPinFromProperty>& InCustomPinProperties);
	virtual ~FControlRigIOMapping();

	struct CONTROLRIGDEVELOPER_API FControlsInfo
	{
		FName Name;
		FString DisplayName;
		FEdGraphPinType PinType;
		ERigControlType ControlType;
		FString DefaultValue;
	};

	// Helper enabling quering controls in editor from a Control Rig class
	struct CONTROLRIGDEVELOPER_API FRigControlsData
	{
		FRigControlsData() = default;

		const TArray<FControlsInfo>& GetControls(const UClass* ControlRigClass, USkeleton* TargetSkeleton) const;

	private:
		// Controls data
		mutable const UClass* ControlsInfoClass = nullptr;
		mutable TArray<FControlsInfo> ControlsInfo;
	};

	bool CreateVariableMappingWidget(IDetailLayoutBuilder& DetailBuilder);

	const TMap<FName, FRigVMExternalVariable>& GetInputVariables() const
	{
		return InputVariables;
	}
	const TMap<FName, FRigVMExternalVariable>& GetOutputVariables() const
	{
		return OutputVariables;
	}

	const TArray<FControlRigIOMapping::FControlsInfo>& GetControls() const;

	void RebuildExposedProperties();

	FOnGetTargetSkeleton& GetOnGetTargetSkeletonDelegate()
	{
		return OnGetTargetSkeletonDelegate;
	}

	FOnGetTargetClass& GetOnGetTargetClassDelegate()
	{
		return OnGetTargetClassDelegate;
	}

	FOnPinCheckStateChanged& GetOnPinCheckStateChangedDelegate()
	{
		return OnPinCheckStateChangedDelegate;
	}

	FOnVariableMappingChanged& GetOnVariableMappingChanged()
	{
		return OnVariableMappingChangedDelegate;
	}

	bool IsInputProperty(const FName& PropertyName) const;

	void SetIOMapping(bool bInput, const FName& SourceProperty, const FName& TargetCurve);
	FName GetIOMapping(bool bInput, const FName& SourceProperty) const;

	void SetIgnoreVariablesWithNoMemory(bool bIgnore) { bIgnoreVariablesWithNoMemory = bIgnore; }

private:
	FOnGetTargetSkeleton OnGetTargetSkeletonDelegate;
	FOnGetTargetClass OnGetTargetClassDelegate;
	FOnPinCheckStateChanged OnPinCheckStateChangedDelegate;
	FOnVariableMappingChanged OnVariableMappingChangedDelegate;

	TMap<FName, FName>& InputMapping;
	TMap<FName, FName>& OutputMapping;
	TArray<FOptionalPinFromProperty>& CustomPinProperties;

	// Controls data
	FRigControlsData RigControlsData;

	bool bIgnoreVariablesWithNoMemory = false;

	// property related things
	static void GetVariables(const UClass* TargetClass, bool bInput, bool bIgnoreVariablesWithNoMemory, TMap<FName, FRigVMExternalVariable>& OutParameters);

	TMap<FName, FRigVMExternalVariable> InputVariables;
	TMap<FName, FRigVMExternalVariable> OutputVariables;

	// pin option related
	bool IsPropertyExposeEnabled(FName PropertyName) const;
	ECheckBoxState IsPropertyExposed(FName PropertyName) const;
	void OnPropertyExposeCheckboxChanged(ECheckBoxState NewState, FName PropertyName);

#if WITH_EDITOR
	// SRigVMVariableMappingWidget related
	void OnVariableMappingChanged(const FName& PathName, const FName& Curve, bool bInput);
	FName GetVariableMapping(const FName& PathName, bool bInput);
	void GetAvailableMapping(const FName& PathName, TArray<FName>& OutArray, bool bInput);
	void CreateVariableMapping(const FString& FilteredText, TArray< TSharedPtr<FRigVMVariableMappingInfo> >& OutArray, bool bInput);
#endif

	bool IsAvailableToMapToCurve(const FName& PropertyName, bool bInput) const;
	const FControlsInfo* FindControlElement(const FName& InControlName) const;

	const UClass* GetTargetClass() const;
	USkeleton* GetTargetSkeleton() const;
};

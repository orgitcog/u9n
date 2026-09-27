// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocumentBuilder.h"
#include "MetasoundGeneratorHandle.h"
#include "MVVMViewModelBase.h"

#include "MetaSoundViewModel.generated.h"

TECHAUDIOTOOLSMETASOUND_API DECLARE_LOG_CATEGORY_EXTERN(LogTechAudioToolsMetaSound, Log, All);

class UMetaSoundBuilderBase;
class UMetaSoundInputViewModel;
class UMetaSoundOutputViewModel;

/**
 * The base class for MetaSound viewmodels. Used for binding metadata and member inputs/outputs of a MetaSound to widgets in UMG.
 * Can be initialized using a MetaSound Builder or a MetaSound asset. Creates member viewmodels for each input and output in the
 * MetaSound upon initialization.
 */
UCLASS(DisplayName = "MetaSound Viewmodel")
class TECHAUDIOTOOLSMETASOUND_API UMetaSoundViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

private:
	// True if this MetaSound Viewmodel has been initialized.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MetaSound Viewmodel", meta = (AllowPrivateAccess))
	bool bIsInitialized = false;

	// True if the initialized MetaSound is a preset.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MetaSound Viewmodel", meta = (AllowPrivateAccess))
	bool bIsPreset = false;

public:
	// Returns the object name of the initialized builder as text.
	UFUNCTION(BlueprintCallable, FieldNotify, Category = "MetaSound Viewmodel")
	FText GetBuilderNameAsText() const;

	// Contains MetaSound Input Viewmodels for each input of the initialized MetaSound.
	UFUNCTION(BlueprintCallable, FieldNotify, DisplayName = "Get Input Viewmodels", Category = "MetaSound Viewmodel")
	virtual TArray<UMetaSoundInputViewModel*> GetInputViewModels() const;

	// Contains MetaSound Output ViewModels for each output of the initialized MetaSound.
	UFUNCTION(BlueprintCallable, FieldNotify, DisplayName = "Get Output Viewmodels", Category = "MetaSound Viewmodel")
	virtual TArray<UMetaSoundOutputViewModel*> GetOutputViewModels() const;

	// Initializes the viewmodel using the given MetaSound asset.
	UFUNCTION(BlueprintCallable, DisplayName = "Initialize MetaSound", Category = "Audio|MetaSound Viewmodel")
	virtual void InitializeMetaSound(const TScriptInterface<IMetaSoundDocumentInterface> InMetaSound);

	// Initializes the viewmodel using the given builder.
	UFUNCTION(BlueprintCallable, DisplayName = "Initialize Builder", Category = "Audio|MetaSound Viewmodel")
	virtual void Initialize(UMetaSoundBuilderBase* InBuilder);

	// Resets this MetaSoundViewModel instance to an uninitialized state.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound Viewmodel")
	virtual void Reset();

	// Returns a reference to the initialized MetaSound's Builder.
	UFUNCTION(BlueprintCallable, Category = "Audio|MetaSound Viewmodel")
	UMetaSoundBuilderBase* GetBuilder() const { return Builder; }

	bool IsInitialized() const { return bIsInitialized; }
	void SetIsInitialized(const bool bInIsInitialized) { UE_MVVM_SET_PROPERTY_VALUE(bIsInitialized, bInIsInitialized); }

protected:
	virtual void InitializeProperties(const FMetasoundFrontendDocument& FrontendDocument);
	virtual void ResetProperties();

	// Called upon initialization. Creates viewmodel instances for all inputs and outputs of the initialized MetaSound.
	void CreateMemberViewModels();

	// Creates a single MetaSoundInputViewModel instance for the given input.
	virtual void CreateInputViewModel(const FMetasoundFrontendClassInput& InInput);
    virtual UMetaSoundInputViewModel* CreateInputViewModelInstance();

	// Creates a single MetaSoundOutputViewModel instance for the given output.
	void CreateOutputViewModel(const FMetasoundFrontendClassOutput& InOutput);
	virtual UMetaSoundOutputViewModel* CreateOutputViewModelInstance();

	UPROPERTY(Transient)
	TObjectPtr<UMetaSoundBuilderBase> Builder;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UMetaSoundInputViewModel>> InputViewModels;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UMetaSoundOutputViewModel>> OutputViewModels;
};

/**
 * Viewmodel class for MetaSound inputs. Allows widgets in UMG to bind to MetaSound literals. Useful for creating knobs, sliders, and other
 * widgets for setting MetaSound input parameters.
 */
UCLASS(DisplayName = "MetaSound Input Viewmodel")
class TECHAUDIOTOOLSMETASOUND_API UMetaSoundInputViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

protected:
	// True if this MetaSoundInputViewModel has been initialized.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MetaSound Input", meta = (AllowPrivateAccess))
	bool bIsInitialized = false;

	// Sets the name of the initialized MetaSound input.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, FieldNotify, Setter, Category = "MetaSound Input", meta = (AllowPrivateAccess))
	FName InputName;

	// Returns the data type of the initialized MetaSound input.
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Category = "MetaSound Input", meta = (AllowPrivateAccess))
	FName DataType;

	// True if the initialized MetaSound input is an array.
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter = "SetIsArray", Category = "MetaSound Input", meta = (AllowPrivateAccess))
	bool bIsArray;

	// The MetaSound Literal belonging to the initialized MetaSound input.
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Category = "MetaSound Input", meta = (AllowPrivateAccess))
	FMetasoundFrontendLiteral Literal;

	// Returns the Literal Type belonging to the initialized MetaSound input.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MetaSound Input", meta = (AllowPrivateAccess))
	EMetasoundFrontendLiteralType LiteralType;

	UPROPERTY(Transient)
	TObjectPtr<UMetaSoundBuilderBase> Builder;

public:
	virtual void InitializeInput(UMetaSoundBuilderBase* InBuilder, const FMetasoundFrontendClassInput& InInput);
	virtual void ResetInput();

	FName GetInputName() const { return InputName; }

	void SetIsInitialized(const bool bInIsInitialized);
	void SetInputName(const FName& InInputName);
	void SetDataType(const FName& InDataType);
	void SetIsArray(const bool bInIsArray);

	// Returns the value of this input's MetaSound Literal as a text value.
	UFUNCTION(BlueprintCallable, FieldNotify, Category = "Audio|MetaSound Input Viewmodel")
	FText GetLiteralValueAsText() const;

	void SetLiteral(const FMetasoundFrontendLiteral& InLiteral);
};

/**
 * Viewmodel class for MetaSound outputs. Allows widgets in UMG to bind to data from a MetaSound output. Useful for driving visual parameters
 * using MetaSound outputs.
 */
UCLASS(DisplayName = "MetaSound Output Viewmodel")
class TECHAUDIOTOOLSMETASOUND_API UMetaSoundOutputViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

protected:
	// True if this MetaSoundOutputViewModel has been initialized.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "MetaSound Output", meta = (AllowPrivateAccess))
	bool bIsInitialized = false;

	// Sets the name of the initialized MetaSound output.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, FieldNotify, Setter, Category = "MetaSound Output", meta = (AllowPrivateAccess))
	FName OutputName;

	// Returns the data type of the initialized MetaSound output.
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Category = "MetaSound Output", meta = (AllowPrivateAccess))
	FName DataType;

	// True if the initialized MetaSound output is an array.
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter = "SetIsArray", Category = "MetaSound Output", meta = (AllowPrivateAccess))
	bool bIsArray;

	// The MetaSound Output belonging to the initialized MetaSound output.
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, DisplayName = "MetaSound Output", Category = "MetaSound Output", meta = (AllowPrivateAccess))
	FMetaSoundOutput MetaSoundOutput;

	UPROPERTY(Transient)
	TObjectPtr<UMetaSoundBuilderBase> Builder;

public:
	virtual void InitializeOutput(UMetaSoundBuilderBase* InBuilder, const FMetasoundFrontendClassOutput& InOutput);
	virtual void ResetOutput();

	FName GetOutputName() const { return OutputName; }

	void SetIsInitialized(const bool bInIsInitialized);
	void SetOutputName(const FName& InOutputName);
	void SetDataType(const FName& InDataType);
	void SetIsArray(const bool bInIsArray);
	void SetMetaSoundOutput(const FMetaSoundOutput& InMetaSoundOutput);

	UFUNCTION()
	void OnOutputValueChanged(FName InOutputName, const FMetaSoundOutput& InOutput);
};

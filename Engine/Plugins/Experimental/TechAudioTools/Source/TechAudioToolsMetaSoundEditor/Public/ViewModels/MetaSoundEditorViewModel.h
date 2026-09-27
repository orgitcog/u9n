// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/MetaSoundViewModel.h"

#include "MetaSoundEditorViewModel.generated.h"

class UMetaSoundEditorBuilderListener;

TECHAUDIOTOOLSMETASOUNDEDITOR_API DECLARE_LOG_CATEGORY_EXTERN(LogTechAudioToolsMetaSoundEditor, Log, All);

/**
 * Editor viewmodel for MetaSounds. Creates MetaSoundEditorBuilderListener bindings upon initialization, allowing changes made to assets in the
 * MetaSound Editor to be reflected in UMG widgets.
 */
UCLASS(MinimalAPI, DisplayName = "MetaSound Editor Viewmodel")
class UMetaSoundEditorViewModel : public UMetaSoundViewModel
{
	GENERATED_BODY()

protected:
	// Sets the display name of the initialized MetaSound.
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter = "SetMetaSoundDisplayName", Category = "MetaSound Viewmodel|Metadata", meta = (AllowPrivateAccess))
	FText DisplayName;

	// Sets the description of the initialized MetaSound.
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter = "SetMetaSoundDescription", Category = "MetaSound Viewmodel|Metadata", meta = (AllowPrivateAccess))
	FText Description;

	// Sets the author of the initialized MetaSound.
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Category = "MetaSound Viewmodel|Metadata", meta = (AllowPrivateAccess))
	FString Author;

	// Sets the keywords of the initialized MetaSound.
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Category = "MetaSound Viewmodel|Metadata", meta = (AllowPrivateAccess))
	TArray<FText> Keywords;

	// Sets the category hierarchy of the initialized MetaSound.
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Category = "MetaSound Viewmodel|Metadata", meta = (AllowPrivateAccess))
	TArray<FText> CategoryHierarchy;

	// Sets the initialized MetaSound asset as deprecated.
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter = "SetIsDeprecated", Category = "MetaSound Viewmodel|Metadata", meta = (AllowPrivateAccess))
	bool bIsDeprecated = false;

public:
	virtual bool IsEditorOnly() const override;
	virtual UWorld* GetWorld() const override;

	virtual void InitializeMetaSound(const TScriptInterface<IMetaSoundDocumentInterface> InMetaSound) override;
	virtual void Initialize(UMetaSoundBuilderBase* InBuilder) override;
	virtual void Reset() override;

	virtual void InitializeProperties(const FMetasoundFrontendDocument& FrontendDocument) override;
	virtual void ResetProperties() override;

	void SetMetaSoundDisplayName(const FText& InDisplayName);
	void SetMetaSoundDescription(const FText& InDescription);
	void SetAuthor(const FString& InAuthor);
	void SetKeywords(const TArray<FText>& InKeywords);
	void SetCategoryHierarchy(const TArray<FText>& InCategoryHierarchy);
	void SetIsDeprecated(const bool bInIsDeprecated);

protected:
	UPROPERTY(Transient)
	TObjectPtr<UMetaSoundEditorBuilderListener> BuilderListener;

	virtual void CreateInputViewModel(const FMetasoundFrontendClassInput& InInput) override;
    virtual UMetaSoundInputViewModel* CreateInputViewModelInstance() override;
    virtual UMetaSoundOutputViewModel* CreateOutputViewModelInstance() override;

	void BindDelegates();
	void UnbindDelegates();

	// Called when a new MetaSound input has been added to the initialized MetaSound.
	UFUNCTION()
	void OnInputAdded(FName VertexName, FName DataType);

	// Called when a MetaSound input has been removed from the initialized MetaSound.
	UFUNCTION()
	void OnInputRemoved(FName VertexName, FName DataType);

	// Called when the name of an input on the initialized MetaSound has changed.
	UFUNCTION()
	void OnInputNameChanged(FName OldName, FName NewName);

	// Called when an input's data type has changed.
	UFUNCTION()
	void OnInputDataTypeChanged(FName VertexName, FName DataType);

	// Called when a new MetaSound output has been added to the initialized MetaSound.
	UFUNCTION()
	void OnOutputAdded(FName VertexName, FName DataType);

	// Called when a MetaSound output has been removed from the initialized MetaSound.
	UFUNCTION()
	void OnOutputRemoved(FName VertexName, FName DataType);

	// Called when the name of an output on the initialized MetaSound has changed.
	UFUNCTION()
	void OnOutputNameChanged(FName OldName, FName NewName);

	// Called when an output's data type has changed.
	UFUNCTION()
	void OnOutputDataTypeChanged(FName VertexName, FName DataType);
};

/**
 * Editor viewmodel class for MetaSound inputs. Extends the runtime MetaSoundInputViewModel with editor-only functionality. 
 */
UCLASS(MinimalAPI, DisplayName = "MetaSound Input Editor Viewmodel")
class UMetaSoundInputEditorViewModel : public UMetaSoundInputViewModel
{
	GENERATED_BODY()

protected:
	// Sets the display name of the initialized MetaSound input.
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Category = "MetaSound Input", meta = (AllowPrivateAccess))
	FText InputDisplayName;

	// Sets the description of the initialized MetaSound input.
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Category = "MetaSound Input", meta = (AllowPrivateAccess))
	FText InputDescription;

	// Sets the sort order index of the initialized MetaSound input.
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Category = "MetaSound Input", meta = (AllowPrivateAccess))
	int32 SortOrderIndex;

	// Sets whether the initialized MetaSound input should be located in the Advanced Display category.
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter = "SetIsAdvancedDisplay", Category = "MetaSound Input", meta = (AllowPrivateAccess))
	bool bIsAdvancedDisplay;

public:
	virtual bool IsEditorOnly() const override;
	virtual UWorld* GetWorld() const override;

	virtual void InitializeInput(UMetaSoundBuilderBase* InBuilder, const FMetasoundFrontendClassInput& InInput) override;
	virtual void ResetInput() override;

	FText GetInputDisplayName() const { return InputDisplayName; }
	FText GetInputDescription() const { return InputDescription; }
	int32 GetSortOrderIndex() const { return SortOrderIndex; }
	bool IsAdvancedDisplay() const { return bIsAdvancedDisplay; }

	void SetInputDisplayName(const FText& InDisplayName);
	void SetInputDescription(const FText& InDescription);
	void SetSortOrderIndex(const int32 InSortOrderIndex);
	void SetIsAdvancedDisplay(const bool bInIsAdvancedDisplay);

	// Called when the default value of an input has been changed on the initialized MetaSound.
	UFUNCTION()
	void OnInputDefaultChanged(FName VertexName, FMetasoundFrontendLiteral LiteralValue, FName PageName);
};

/**
 * Editor viewmodel class for MetaSound outputs. Extends the runtime MetaSoundOutputViewModel with editor-only functionality. 
 */
UCLASS(MinimalAPI, DisplayName = "MetaSound Output Editor Viewmodel")
class UMetaSoundOutputEditorViewModel : public UMetaSoundOutputViewModel
{
	GENERATED_BODY()

protected:
	// Sets the display name of the initialized MetaSound output.
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Category = "MetaSound Output", meta = (AllowPrivateAccess))
	FText OutputDisplayName;

	// Sets the description of the initialized MetaSound output.
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Category = "MetaSound Output", meta = (AllowPrivateAccess))
	FText OutputDescription;

	// Sets the sort order index of the initialized MetaSound output.
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter, Category = "MetaSound Output", meta = (AllowPrivateAccess))
	int32 SortOrderIndex;

	// Sets whether the initialized MetaSound output should be located in the Advanced Display category.
	UPROPERTY(BlueprintReadWrite, FieldNotify, Setter = "SetIsAdvancedDisplay", Category = "MetaSound Output", meta = (AllowPrivateAccess))
	bool bIsAdvancedDisplay;

public:
	virtual bool IsEditorOnly() const override;
	virtual UWorld* GetWorld() const override;

	virtual void InitializeOutput(UMetaSoundBuilderBase* InBuilder, const FMetasoundFrontendClassOutput& InOutput) override;
	virtual void ResetOutput() override;

	FText GetOutputDisplayName() const { return OutputDisplayName; }
	FText GetOutputDescription() const { return OutputDescription; }
	int32 GetSortOrderIndex() const { return SortOrderIndex; }
	bool IsAdvancedDisplay() const { return bIsAdvancedDisplay; }

	void SetOutputDisplayName(const FText& InDisplayName);
	void SetOutputDescription(const FText& InDescription);
	void SetSortOrderIndex(const int32 InSortOrderIndex);
	void SetIsAdvancedDisplay(const bool bInIsAdvancedDisplay);
};

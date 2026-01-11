// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/MetaSoundEditorViewModel.h"

#include "Editor/EditorEngine.h"
#include "Logging/StructuredLog.h"
#include "MetasoundDocumentBuilderRegistry.h"
#include "MetasoundEditorBuilderListener.h"
#include "MetasoundEditorSubsystem.h"
#include "MetasoundOutputSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaSoundEditorViewModel)

extern UNREALED_API UEditorEngine* GEditor;

DEFINE_LOG_CATEGORY(LogTechAudioToolsMetaSoundEditor);

bool UMetaSoundEditorViewModel::IsEditorOnly() const
{
	return true;
}

UWorld* UMetaSoundEditorViewModel::GetWorld() const
{
	if (HasAllFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	return GEditor ? GEditor->GetEditorWorldContext(false).World() : nullptr;
}

void UMetaSoundEditorViewModel::InitializeMetaSound(const TScriptInterface<IMetaSoundDocumentInterface> InMetaSound)
{
	if (!InMetaSound)
	{
		Reset();
		return;
	}

	Builder = &Metasound::Engine::FDocumentBuilderRegistry::GetChecked().FindOrBeginBuilding(*InMetaSound.GetObject());
	Initialize(Builder);
}

void UMetaSoundEditorViewModel::Initialize(UMetaSoundBuilderBase* InBuilder)
{
	Super::Initialize(InBuilder);
	BindDelegates();
}

void UMetaSoundEditorViewModel::Reset()
{
	Super::Reset();
	UnbindDelegates();
}

void UMetaSoundEditorViewModel::InitializeProperties(const FMetasoundFrontendDocument& FrontendDocument)
{
	Super::InitializeProperties(FrontendDocument);

	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, FrontendDocument.RootGraph.Metadata.GetDisplayName());
	UE_MVVM_SET_PROPERTY_VALUE(Description, FrontendDocument.RootGraph.Metadata.GetDescription());
	UE_MVVM_SET_PROPERTY_VALUE(Author, FrontendDocument.RootGraph.Metadata.GetAuthor());
	UE_MVVM_SET_PROPERTY_VALUE(Keywords, FrontendDocument.RootGraph.Metadata.GetKeywords());
	UE_MVVM_SET_PROPERTY_VALUE(CategoryHierarchy, FrontendDocument.RootGraph.Metadata.GetCategoryHierarchy());
	UE_MVVM_SET_PROPERTY_VALUE(bIsDeprecated, FrontendDocument.RootGraph.Metadata.GetIsDeprecated());
}

void UMetaSoundEditorViewModel::ResetProperties()
{
	Super::ResetProperties();

	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, FText());
	UE_MVVM_SET_PROPERTY_VALUE(Description, FText());
	UE_MVVM_SET_PROPERTY_VALUE(Author, FString());
	UE_MVVM_SET_PROPERTY_VALUE(Keywords, TArray<FText>());
	UE_MVVM_SET_PROPERTY_VALUE(CategoryHierarchy, TArray<FText>());
	UE_MVVM_SET_PROPERTY_VALUE(bIsDeprecated, false);
}

void UMetaSoundEditorViewModel::SetMetaSoundDisplayName(const FText& InDisplayName)
{
	if (Builder && UE_MVVM_SET_PROPERTY_VALUE(DisplayName, InDisplayName))
	{
		FMetaSoundFrontendDocumentBuilder& DocumentBuilder = Builder->GetBuilder();
		DocumentBuilder.SetDisplayName(InDisplayName);
	}
}

void UMetaSoundEditorViewModel::SetMetaSoundDescription(const FText& InDescription)
{
	if (Builder && UE_MVVM_SET_PROPERTY_VALUE(Description, InDescription))
	{
		FMetaSoundFrontendDocumentBuilder& DocumentBuilder = Builder->GetBuilder();
		DocumentBuilder.SetDescription(InDescription);
	}
}

void UMetaSoundEditorViewModel::SetAuthor(const FString& InAuthor)
{
	if (Builder && UE_MVVM_SET_PROPERTY_VALUE(Author, InAuthor))
	{
		FMetaSoundFrontendDocumentBuilder& DocumentBuilder = Builder->GetBuilder();
		DocumentBuilder.SetAuthor(InAuthor);
	}
}

void UMetaSoundEditorViewModel::SetKeywords(const TArray<FText>& InKeywords)
{
	if (Builder && UE_MVVM_SET_PROPERTY_VALUE(Keywords, InKeywords))
	{
		FMetaSoundFrontendDocumentBuilder& DocumentBuilder = Builder->GetBuilder();
		DocumentBuilder.SetKeywords(InKeywords);
	}
}

void UMetaSoundEditorViewModel::SetCategoryHierarchy(const TArray<FText>& InCategoryHierarchy)
{
	if (Builder && UE_MVVM_SET_PROPERTY_VALUE(CategoryHierarchy, InCategoryHierarchy))
	{
		FMetaSoundFrontendDocumentBuilder& DocumentBuilder = Builder->GetBuilder();
		DocumentBuilder.SetCategoryHierarchy(InCategoryHierarchy);
	}
}

void UMetaSoundEditorViewModel::SetIsDeprecated(const bool bInIsDeprecated)
{
	if (Builder && UE_MVVM_SET_PROPERTY_VALUE(bIsDeprecated, bInIsDeprecated))
	{
		FMetaSoundFrontendDocumentBuilder& DocumentBuilder = Builder->GetBuilder();
		DocumentBuilder.SetIsDeprecated(bInIsDeprecated);
	}
}

void UMetaSoundEditorViewModel::CreateInputViewModel(const FMetasoundFrontendClassInput& InInput)
{
	Super::CreateInputViewModel(InInput);

	UMetaSoundInputViewModel* InputViewModel = *InputViewModels.Find(InInput.Name);
	if (UMetaSoundInputEditorViewModel* InputEditorViewModel = Cast<UMetaSoundInputEditorViewModel>(InputViewModel))
	{
		if (BuilderListener && InputEditorViewModel)
		{
			BuilderListener->OnGraphInputDefaultChangedDelegate.AddDynamic(InputEditorViewModel, &UMetaSoundInputEditorViewModel::OnInputDefaultChanged);
		}
	}
}

UMetaSoundInputViewModel* UMetaSoundEditorViewModel::CreateInputViewModelInstance()
{
    return NewObject<UMetaSoundInputEditorViewModel>(this, UMetaSoundInputEditorViewModel::StaticClass());
}

UMetaSoundOutputViewModel* UMetaSoundEditorViewModel::CreateOutputViewModelInstance()
{
    return NewObject<UMetaSoundOutputEditorViewModel>(this, UMetaSoundOutputEditorViewModel::StaticClass());
}

void UMetaSoundEditorViewModel::BindDelegates()
{
	if (!Builder)
	{
		UE_LOGFMT(LogTechAudioToolsMetaSoundEditor, Log, "Could not bind MetaSoundViewModel delegates. Builder was null.");
		SetIsInitialized(false);
		return;
	}

	UMetaSoundEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMetaSoundEditorSubsystem>();
	if (!EditorSubsystem)
	{
		UE_LOGFMT(LogTechAudioToolsMetaSoundEditor, Log, "Could not bind MetaSoundViewModel delegates. Unable to locate MetaSound Editor Subsystem.");
		SetIsInitialized(false);
		return;
	}

	EMetaSoundBuilderResult Result;
	BuilderListener = EditorSubsystem->AddBuilderDelegateListener(Builder, Result);

	if (Result != EMetaSoundBuilderResult::Succeeded)
	{
		UE_LOGFMT(LogTechAudioToolsMetaSoundEditor, Log, "Could not bind MetaSoundViewModel delegates. Failed to create BuilderListener.");
		SetIsInitialized(false);
		return;
	}

	if (BuilderListener)
	{
		BuilderListener->OnGraphInputAddedDelegate.AddDynamic(this, &ThisClass::OnInputAdded);
		BuilderListener->OnRemovingGraphInputDelegate.AddDynamic(this, &ThisClass::OnInputRemoved);
		BuilderListener->OnGraphInputNameChangedDelegate.AddDynamic(this, &ThisClass::OnInputNameChanged);
		BuilderListener->OnGraphInputDataTypeChangedDelegate.AddDynamic(this, &ThisClass::OnInputDataTypeChanged);

		BuilderListener->OnGraphOutputAddedDelegate.AddDynamic(this, &ThisClass::OnOutputAdded);
		BuilderListener->OnRemovingGraphOutputDelegate.AddDynamic(this, &ThisClass::OnOutputRemoved);
		BuilderListener->OnGraphOutputNameChangedDelegate.AddDynamic(this, &ThisClass::OnOutputNameChanged);
		BuilderListener->OnGraphOutputDataTypeChangedDelegate.AddDynamic(this, &ThisClass::OnOutputDataTypeChanged);

		for (const TPair<FName, UMetaSoundInputViewModel*> InputViewModel : InputViewModels)
		{
			if (UMetaSoundInputEditorViewModel* InputEditorViewModel = Cast<UMetaSoundInputEditorViewModel>(InputViewModel.Value))
			{
				BuilderListener->OnGraphInputDefaultChangedDelegate.AddDynamic(InputEditorViewModel, &UMetaSoundInputEditorViewModel::OnInputDefaultChanged);
			}
		}
	}
}

void UMetaSoundEditorViewModel::UnbindDelegates()
{
	if (BuilderListener)
	{
		BuilderListener->RemoveAllDelegates();
		BuilderListener = nullptr;
	}
}

void UMetaSoundEditorViewModel::OnInputAdded(const FName VertexName, const FName DataType)
{
	const FMetaSoundFrontendDocumentBuilder& DocumentBuilder = Builder->GetConstBuilder();
	const FMetasoundFrontendDocument& FrontendDocument = DocumentBuilder.GetConstDocumentChecked();
	for (const FMetasoundFrontendClassInput& Input : FrontendDocument.RootGraph.GetDefaultInterface().Inputs)
	{
		if (Input.Name == VertexName)
		{
			CreateInputViewModel(Input);
			return;
		}
	}
}

void UMetaSoundEditorViewModel::OnInputRemoved(const FName VertexName, const FName DataType)
{
	InputViewModels.Remove(VertexName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetInputViewModels);
}

void UMetaSoundEditorViewModel::OnInputNameChanged(const FName OldName, const FName NewName)
{
	if (UMetaSoundInputViewModel* InputViewModel = *InputViewModels.Find(OldName))
	{
		InputViewModel->SetInputName(NewName);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetInputViewModels);
	}
}

void UMetaSoundEditorViewModel::OnInputDataTypeChanged(const FName VertexName, const FName DataType)
{
	if (UMetaSoundInputViewModel* InputViewModel = *InputViewModels.Find(VertexName))
	{
		InputViewModel->SetDataType(DataType);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetInputViewModels);
	}
}

void UMetaSoundEditorViewModel::OnOutputAdded(const FName VertexName, const FName DataType)
{
	if (Builder)
	{
		const FMetaSoundFrontendDocumentBuilder& DocumentBuilder = Builder->GetConstBuilder();
		const FMetasoundFrontendDocument& FrontendDocument = DocumentBuilder.GetConstDocumentChecked();
		for (const FMetasoundFrontendClassOutput& Output : FrontendDocument.RootGraph.GetDefaultInterface().Outputs)
		{
			if (Output.Name == VertexName)
			{
				CreateOutputViewModel(Output);
				return;
			}
		}
	}
}

void UMetaSoundEditorViewModel::OnOutputRemoved(const FName VertexName, const FName DataType)
{
	OutputViewModels.Remove(VertexName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetOutputViewModels);
}

void UMetaSoundEditorViewModel::OnOutputNameChanged(const FName OldName, const FName NewName)
{
	if (UMetaSoundOutputViewModel* OutputViewModel = *OutputViewModels.Find(OldName))
	{
		OutputViewModel->SetOutputName(NewName);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetOutputViewModels);
	}
}

void UMetaSoundEditorViewModel::OnOutputDataTypeChanged(const FName VertexName, const FName DataType)
{
	if (UMetaSoundOutputViewModel* OutputViewModel = *OutputViewModels.Find(VertexName))
	{
		OutputViewModel->SetDataType(DataType);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetOutputViewModels);
	}
}

bool UMetaSoundInputEditorViewModel::IsEditorOnly() const
{
	return true;
}

UWorld* UMetaSoundInputEditorViewModel::GetWorld() const
{
	if (HasAllFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	return GEditor ? GEditor->GetEditorWorldContext(false).World() : nullptr;
}

void UMetaSoundInputEditorViewModel::InitializeInput(UMetaSoundBuilderBase* InBuilder, const FMetasoundFrontendClassInput& InInput)
{
	Super::InitializeInput(InBuilder, InInput);

	UE_MVVM_SET_PROPERTY_VALUE(InputDisplayName, InInput.Metadata.GetDisplayName());
	UE_MVVM_SET_PROPERTY_VALUE(InputDescription, InInput.Metadata.GetDescription());
	UE_MVVM_SET_PROPERTY_VALUE(SortOrderIndex, InInput.Metadata.SortOrderIndex);
	UE_MVVM_SET_PROPERTY_VALUE(bIsAdvancedDisplay, InInput.Metadata.bIsAdvancedDisplay);
}

void UMetaSoundInputEditorViewModel::ResetInput()
{
	Super::ResetInput();

	UE_MVVM_SET_PROPERTY_VALUE(InputDisplayName, FText());
	UE_MVVM_SET_PROPERTY_VALUE(InputDescription, FText());
	UE_MVVM_SET_PROPERTY_VALUE(SortOrderIndex, 0);
	UE_MVVM_SET_PROPERTY_VALUE(bIsAdvancedDisplay, false);
}

void UMetaSoundInputEditorViewModel::SetInputDisplayName(const FText& InDisplayName)
{
	if (Builder)
	{
		FMetaSoundFrontendDocumentBuilder& DocumentBuilder = Builder->GetBuilder();
		if (DocumentBuilder.IsValid() && UE_MVVM_SET_PROPERTY_VALUE(InputDisplayName, InDisplayName))
		{
			DocumentBuilder.SetGraphOutputDisplayName(InputName, InDisplayName);
		}
	}
}

void UMetaSoundInputEditorViewModel::SetInputDescription(const FText& InDescription)
{
	if (Builder)
	{
		FMetaSoundFrontendDocumentBuilder& DocumentBuilder = Builder->GetBuilder();
		if (DocumentBuilder.IsValid() && UE_MVVM_SET_PROPERTY_VALUE(InputDescription, InDescription))
		{
			DocumentBuilder.SetGraphInputDescription(InputName, InDescription);
		}
	}
}

void UMetaSoundInputEditorViewModel::SetSortOrderIndex(const int32 InSortOrderIndex)
{
	if (Builder)
	{
		FMetaSoundFrontendDocumentBuilder& DocumentBuilder = Builder->GetBuilder();
		if (DocumentBuilder.IsValid() && UE_MVVM_SET_PROPERTY_VALUE(SortOrderIndex, InSortOrderIndex))
		{
			DocumentBuilder.SetGraphInputSortOrderIndex(InputName, InSortOrderIndex);
		}
	}
}

void UMetaSoundInputEditorViewModel::SetIsAdvancedDisplay(const bool bInIsAdvancedDisplay)
{
	if (Builder)
	{
		FMetaSoundFrontendDocumentBuilder& DocumentBuilder = Builder->GetBuilder();
		if (DocumentBuilder.IsValid() && UE_MVVM_SET_PROPERTY_VALUE(bIsAdvancedDisplay, bInIsAdvancedDisplay))
		{
			DocumentBuilder.SetGraphInputAdvancedDisplay(InputName, bInIsAdvancedDisplay);
		}
	}
}

void UMetaSoundInputEditorViewModel::OnInputDefaultChanged(const FName VertexName, FMetasoundFrontendLiteral LiteralValue, FName PageName)
{
	if (InputName == VertexName)
	{
		SetLiteral(LiteralValue);
	}
}

bool UMetaSoundOutputEditorViewModel::IsEditorOnly() const
{
	return true;
}

UWorld* UMetaSoundOutputEditorViewModel::GetWorld() const
{
	if (HasAllFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	return GEditor ? GEditor->GetEditorWorldContext(false).World() : nullptr;
}

void UMetaSoundOutputEditorViewModel::InitializeOutput(UMetaSoundBuilderBase* InBuilder, const FMetasoundFrontendClassOutput& InOutput)
{
	Super::InitializeOutput(InBuilder, InOutput);

	UE_MVVM_SET_PROPERTY_VALUE(OutputDisplayName, InOutput.Metadata.GetDisplayName());
	UE_MVVM_SET_PROPERTY_VALUE(OutputDescription, InOutput.Metadata.GetDescription());
	UE_MVVM_SET_PROPERTY_VALUE(SortOrderIndex, InOutput.Metadata.SortOrderIndex);
	UE_MVVM_SET_PROPERTY_VALUE(bIsAdvancedDisplay, InOutput.Metadata.bIsAdvancedDisplay);
}

void UMetaSoundOutputEditorViewModel::ResetOutput()
{
	Super::ResetOutput();

	UE_MVVM_SET_PROPERTY_VALUE(OutputDisplayName, FText());
	UE_MVVM_SET_PROPERTY_VALUE(OutputDescription, FText());
	UE_MVVM_SET_PROPERTY_VALUE(SortOrderIndex, 0);
	UE_MVVM_SET_PROPERTY_VALUE(bIsAdvancedDisplay, false);
}

void UMetaSoundOutputEditorViewModel::SetOutputDisplayName(const FText& InDisplayName)
{
	if (Builder)
	{
		FMetaSoundFrontendDocumentBuilder& DocumentBuilder = Builder->GetBuilder();
		if (DocumentBuilder.IsValid() && UE_MVVM_SET_PROPERTY_VALUE(OutputDisplayName, InDisplayName))
		{
			DocumentBuilder.SetGraphOutputDisplayName(OutputName, InDisplayName);
		}
	}
}

void UMetaSoundOutputEditorViewModel::SetOutputDescription(const FText& InDescription)
{
	if (Builder)
	{
		FMetaSoundFrontendDocumentBuilder& DocumentBuilder = Builder->GetBuilder();
		if (DocumentBuilder.IsValid() && UE_MVVM_SET_PROPERTY_VALUE(OutputDescription, InDescription))
		{
			DocumentBuilder.SetGraphOutputDescription(OutputName, InDescription);
		}
	}
}

void UMetaSoundOutputEditorViewModel::SetSortOrderIndex(const int32 InSortOrderIndex)
{
	if (Builder)
	{
		FMetaSoundFrontendDocumentBuilder& DocumentBuilder = Builder->GetBuilder();
		if (DocumentBuilder.IsValid() && UE_MVVM_SET_PROPERTY_VALUE(SortOrderIndex, InSortOrderIndex))
		{
			DocumentBuilder.SetGraphOutputSortOrderIndex(OutputName, InSortOrderIndex);
		}
	}
}

void UMetaSoundOutputEditorViewModel::SetIsAdvancedDisplay(const bool bInIsAdvancedDisplay)
{
	if (Builder)
	{
		FMetaSoundFrontendDocumentBuilder& DocumentBuilder = Builder->GetBuilder();
		if (DocumentBuilder.IsValid() && UE_MVVM_SET_PROPERTY_VALUE(bIsAdvancedDisplay, bInIsAdvancedDisplay))
		{
			DocumentBuilder.SetGraphOutputAdvancedDisplay(OutputName, bInIsAdvancedDisplay);
		}
	}
}
